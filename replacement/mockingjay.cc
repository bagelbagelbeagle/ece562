#include "cache.h"
#include "ooo_cpu.h"
#include <unordered_map>
#include <cmath> // For log2 function
#include <algorithm> // For min and max

#ifndef LLC_SETS
#define LLC_SETS 2048  // Number of sets in LLC
#endif

#ifndef LLC_WAYS
#define LLC_WAYS 16  // Number of ways in LLC
#endif

using namespace std;

constexpr int LOG2_LLC_SET = log2(LLC_SETS);
constexpr int LOG2_LLC_SIZE = LOG2_LLC_SET + log2(LLC_WAYS) + LOG2_BLOCK_SIZE;
constexpr int LOG2_SAMPLED_SETS = LOG2_LLC_SIZE - 16;

constexpr int HISTORY = 8;
constexpr int GRANULARITY = 8;

constexpr int INF_RD = LLC_WAYS * HISTORY - 1;
constexpr int INF_ETR = (LLC_WAYS * HISTORY / GRANULARITY) - 1;
constexpr int MAX_RD = INF_RD - 22;

constexpr int SAMPLED_CACHE_WAYS = 5;
constexpr int LOG2_SAMPLED_CACHE_SETS = 4;
constexpr int SAMPLED_CACHE_TAG_BITS = 31 - LOG2_LLC_SIZE;
constexpr int PC_SIGNATURE_BITS = LOG2_LLC_SIZE - 10;
constexpr int TIMESTAMP_BITS = 8;

constexpr double TEMP_DIFFERENCE = 1.0 / 16.0;
constexpr double FLEXMIN_PENALTY = 2.0 - log2(NUM_CPUS) / 4.0;

int etr[LLC_SETS][LLC_WAYS];
int etr_clock[LLC_SETS];
unordered_map<uint32_t, int> rdp;
int current_timestamp[LLC_SETS];

struct SampledCacheLine {
    bool valid;
    uint64_t tag;
    uint64_t signature;
    int timestamp;
};
unordered_map<uint32_t, SampledCacheLine*> sampled_cache;

bool is_sampled_set(int set) {
    int mask_length = LOG2_LLC_SET - LOG2_SAMPLED_SETS;
    int mask = (1 << mask_length) - 1;
    return (set & mask) == ((set >> (LOG2_LLC_SET - mask_length)) & mask);
}

uint64_t CRC_HASH(uint64_t _blockAddress) {
    static const unsigned long long crcPolynomial = 3988292384ULL;
    unsigned long long _returnVal = _blockAddress;
    for (unsigned int i = 0; i < 3; i++)
        _returnVal = ((_returnVal & 1) == 1) ? ((_returnVal >> 1) ^ crcPolynomial) : (_returnVal >> 1);
    return _returnVal;
}

uint64_t get_pc_signature(uint64_t pc, bool hit, bool prefetch, uint32_t core) {
    if (NUM_CPUS == 1) {
        pc = pc << 1;
        if (hit) {
            pc |= 1;
        }
        pc = (pc << 1) | (prefetch ? 1 : 0);
        pc = CRC_HASH(pc);
        pc = (pc << (64 - PC_SIGNATURE_BITS)) >> (64 - PC_SIGNATURE_BITS);
    } else {
        pc = (pc << 1) | (prefetch ? 1 : 0);
        pc = (pc << 2) | core;
        pc = CRC_HASH(pc);
        pc = (pc << (64 - PC_SIGNATURE_BITS)) >> (64 - PC_SIGNATURE_BITS);
    }
    return pc;
}

uint32_t get_sampled_cache_index(uint64_t full_addr) {
    full_addr >>= LOG2_BLOCK_SIZE;
    full_addr = (full_addr << (64 - (LOG2_SAMPLED_CACHE_SETS + LOG2_LLC_SET))) >> (64 - (LOG2_SAMPLED_CACHE_SETS + LOG2_LLC_SET));
    return full_addr;
}

uint64_t get_sampled_cache_tag(uint64_t x) {
    x >>= LOG2_LLC_SET + LOG2_BLOCK_SIZE + LOG2_SAMPLED_CACHE_SETS;
    x = (x << (64 - SAMPLED_CACHE_TAG_BITS)) >> (64 - SAMPLED_CACHE_TAG_BITS);
    return x;
}

int search_sampled_cache(uint64_t blockAddress, uint32_t set) {
    SampledCacheLine* sampled_set = sampled_cache[set];
    for (int way = 0; way < SAMPLED_CACHE_WAYS; way++) {
        if (sampled_set[way].valid && (sampled_set[way].tag == blockAddress)) {
            return way;
        }
    }
    return -1;
}

void detrain(uint32_t set, int way) {
    SampledCacheLine temp = sampled_cache[set][way];
    if (!temp.valid) {
        return;
    }

    if (rdp.count(temp.signature)) {
        rdp[temp.signature] = min(rdp[temp.signature] + 1, INF_RD);
    } else {
        rdp[temp.signature] = INF_RD;
    }
    sampled_cache[set][way].valid = false;
}

int temporal_difference(int init, int sample) {
    if (sample > init) {
        int diff = sample - init;
        diff = diff * TEMP_DIFFERENCE;
        diff = min(1, diff);
        return min(init + diff, INF_RD);
    } else if (sample < init) {
        int diff = init - sample;
        diff = diff * TEMP_DIFFERENCE;
        diff = min(1, diff);
        return max(init - diff, 0);
    } else {
        return init;
    }
}

int time_elapsed(int global, int local) {
    if (global >= local) {
        return global - local;
    }
    global = global + (1 << TIMESTAMP_BITS);
    return global - local;
}

// Initialize cache replacement state
void CACHE::initialize_replacement() {
    for (int i = 0; i < LLC_SETS; i++) {
        etr_clock[i] = GRANULARITY;
        current_timestamp[i] = 0;
    }
    for (uint32_t set = 0; set < LLC_SETS; set++) {
        if (is_sampled_set(set)) {
            int modifier = 1 << LOG2_LLC_SET;
            int limit = 1 << LOG2_SAMPLED_CACHE_SETS;
            for (int i = 0; i < limit; i++) {
                sampled_cache[set + modifier * i] = new SampledCacheLine[SAMPLED_CACHE_WAYS]();
            }
        }
    }
}

// Find a cache block to evict
uint32_t CACHE::find_victim(uint32_t cpu_id, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t pc, uint64_t full_addr, uint32_t type) {
    for (int way = 0; way < LLC_WAYS; way++) {
        if (!current_set[way].valid) {
            return way;
        }
    }

    int max_etr = 0;
    int victim_way = 0;
    for (int way = 0; way < LLC_WAYS; way++) {
        if (abs(etr[set][way]) > max_etr || (abs(etr[set][way]) == max_etr && etr[set][way] < 0)) {
            max_etr = abs(etr[set][way]);
            victim_way = way;
        }
    }

    uint64_t pc_signature = get_pc_signature(pc, false, type == static_cast<uint32_t>(access_type::PREFETCH), cpu_id);
    if (type != 3 && rdp.count(pc_signature) && (rdp[pc_signature] > MAX_RD || rdp[pc_signature] / GRANULARITY > max_etr)) {
        return LLC_WAYS;
    }

    return victim_way;
}

// Update the replacement state
void CACHE::update_replacement_state(uint32_t cpu_id, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t pc, uint64_t victim_addr, uint32_t type, uint8_t hit) {
    if (type == 3 && !hit) {
        etr[set][way] = -INF_ETR;
        return;
    }

    pc = get_pc_signature(pc, hit, type == static_cast<uint32_t>(access_type::PREFETCH), cpu_id);

    if (is_sampled_set(set)) {
        uint32_t sampled_cache_index = get_sampled_cache_index(full_addr);
        uint64_t sampled_cache_tag = get_sampled_cache_tag(full_addr);
        int sampled_cache_way = search_sampled_cache(sampled_cache_tag, sampled_cache_index);

        if (sampled_cache_way > -1) {
            uint64_t last_signature = sampled_cache[sampled_cache_index][sampled_cache_way].signature;
            int sample = time_elapsed(current_timestamp[set], sampled_cache[sampled_cache_index][sampled_cache_way].timestamp);

            if (sample <= INF_RD) {
                if (type == static_cast<uint32_t>(access_type::PREFETCH)) {
                    sample *= FLEXMIN_PENALTY;
                }
                rdp[last_signature] = temporal_difference(rdp[last_signature], sample);
                sampled_cache[sampled_cache_index][sampled_cache_way].valid = false;
            }
        }
    }
    current_timestamp[set] = (current_timestamp[set] + 1) % (1 << TIMESTAMP_BITS);
}

// Final replacement stats
void CACHE::replacement_final_stats() {}
