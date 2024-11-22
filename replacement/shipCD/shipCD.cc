//This code implements the SHiP-CD (Signature-based Hit Predictor with Cache Decay) cache replacement policy for a cache simulator. SHiP-CD enhances the traditional SHiP algorithm by integrating a decay mechanism that dynamically adjusts cache line priorities based on their usage patterns. It utilizes a Signature Hit Counter Table (SHCT) indexed by the instruction pointer (IP) to predict the likelihood of cache line reuse. The Re-Reference Prediction Value (RRPV) is assigned to each cache line to determine its eviction priority—lower RRPV means higher likelihood of reuse. The code initializes key data structures like the SHCT, RRPV values, and a sampler for selected cache sets. During cache accesses, it updates the SHCT and RRPV values: on a cache hit, the RRPV is reset to indicate recent use; on a miss, the RRPV is set based on SHCT predictions. The decay mechanism gradually decreases the SHCT counters for signatures not recently used, allowing the cache to adapt to changing access patterns by prioritizing frequently reused data and demoting stale cache lines, thereby improving overall cache performance.
#include <algorithm>
#include <array>
#include <cassert>
#include <map>
#include <utility>
#include <vector>
#include <random>

#include "cache.h"
#include "msl/bits.h"


namespace
{
constexpr int maxRRPV = 3;
constexpr std::size_t SHCT_SIZE = 16384;
constexpr unsigned SHCT_PRIME = 16381;
constexpr std::size_t SAMPLER_SET = (256 * NUM_CPUS);
constexpr unsigned SHCT_MAX = 7;

// Sampler structure
class SAMPLER_class
{
public:
    bool valid = false;
    uint8_t used = 0;
    uint64_t address = 0, cl_addr = 0, ip = 0;
    uint64_t last_used = 0;
};

// Sampler and prediction table maps
std::map<CACHE*, std::vector<std::size_t>> rand_sets;
std::map<CACHE*, std::vector<SAMPLER_class>> sampler;
std::map<CACHE*, std::vector<int>> rrpv_values;

// Prediction table with signature
std::map<std::pair<CACHE*, std::size_t>, std::array<unsigned, SHCT_SIZE>> SHCT;
} // namespace

// Initialize replacement state
void CACHE::initialize_replacement()
{
    // Set random seed and generator
    std::size_t rand_seed = 1103515245 + 12345;
    std::default_random_engine generator(rand_seed);
    std::uniform_int_distribution<int> distribution(0, NUM_SET - 1);

    // Initialize random sampler sets
    rand_sets[this].clear();
    for (std::size_t i = 0; i < SAMPLER_SET; ++i) {
        rand_sets[this].push_back(distribution(generator));
    }

    // Initialize RRPV values
    rrpv_values[this].resize(NUM_SET * NUM_WAY, maxRRPV);

    // Initialize SHCT entries
    for (std::size_t i = 0; i < SAMPLER_SET; ++i) {
        for (std::size_t j = 0; j < SHCT_SIZE; ++j) {
            SHCT[std::make_pair(this, i)].fill(0);
        }
    }

    // Initialize sampler
    sampler[this].resize(SAMPLER_SET * NUM_WAY);
}

// Find replacement victim
uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    // Look for the maxRRPV line
    auto begin = std::next(std::begin(rrpv_values[this]), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);
    auto victim = std::find(begin, end, maxRRPV);

    while (victim == end) {
        for (auto it = begin; it != end; ++it) {
            ++(*it);
        }
        victim = std::find(begin, end, maxRRPV);
    }

    assert(begin <= victim);
    return static_cast<uint32_t>(std::distance(begin, victim)); // Safe cast protected by assert
}

// Update replacement state on cache hits and fills
void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
    // Handle writeback access
    if (access_type{type} == access_type::WRITE) {
        if (!hit) {
            rrpv_values[this][set * NUM_WAY + way] = maxRRPV - 1;
        }
        return;
    }

    // Update sampler
    auto s_idx = std::find(std::begin(rand_sets[this]), std::end(rand_sets[this]), set);
    if (s_idx != std::end(rand_sets[this])) {
        auto s_set_begin = std::next(std::begin(sampler[this]), std::distance(std::begin(rand_sets[this]), s_idx) * NUM_WAY);
        auto s_set_end = std::next(s_set_begin, NUM_WAY);

        // Check hit
        auto match = std::find_if(s_set_begin, s_set_end,
                                  [addr = full_addr, shamt = 8 + champsim::lg2(NUM_WAY)](auto x) { return x.valid && (x.address >> shamt) == (addr >> shamt); });
        if (match != s_set_end) {
            auto SHCT_idx = match->ip % SHCT_PRIME;

            // SHIP-CD modification: Decay only if used recently
            if (match->used) {
                if (SHCT[std::make_pair(this, triggering_cpu)][SHCT_idx] > 0) {
                    SHCT[std::make_pair(this, triggering_cpu)][SHCT_idx]--;
                }
            }
            match->used = 1;
        } else {
            match = std::min_element(s_set_begin, s_set_end, [](auto x, auto y) { return x.last_used < y.last_used; });

            if (match->used) {
                auto SHCT_idx = match->ip % SHCT_PRIME;
                if (SHCT[std::make_pair(this, triggering_cpu)][SHCT_idx] < SHCT_MAX) {
                    SHCT[std::make_pair(this, triggering_cpu)][SHCT_idx]++;
                }
            }

            match->valid = 1;
            match->address = full_addr;
            match->ip = ip;
            match->used = 0;
        }

        // Update LRU state
        match->last_used = current_cycle;
    }

    if (hit) {
        rrpv_values[this][set * NUM_WAY + way] = 0;
    } else {
        // SHIP-CD prediction
        auto SHCT_idx = ip % SHCT_PRIME;

        rrpv_values[this][set * NUM_WAY + way] = maxRRPV - 1;
        if (SHCT[std::make_pair(this, triggering_cpu)][SHCT_idx] == SHCT_MAX) {
            rrpv_values[this][set * NUM_WAY + way] = maxRRPV;
        }
    }
}

// Print custom stats at the end of simulation
void CACHE::replacement_final_stats() {}

