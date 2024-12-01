#include <algorithm>
#include <cassert>
#include <map>
#include <vector>
#include <iostream>
#include <cmath>

#include "cache.h"
#include "hawkeye_predictor.h"
#include "optgen.h"
#include "helper_function.h"

#define NUM_CORE 1
#define NUM_SET (NUM_CORE * 1024)
#define NUM_WAY 16

// 3-bit RRIP counter
#define MAXRRIP 7
uint32_t rrip[NUM_SET][NUM_WAY];

// Hawkeye predictors for demand and prefetch requests
Hawkeye_Predictor* predictor_demand;
Hawkeye_Predictor* predictor_prefetch;

OPTgen optgen_occup_vector[NUM_SET];

// Prefetching metadata
bool prefetching[NUM_SET][NUM_WAY];

// Sampler components for tracking cache history
#define SAMPLER_ENTRIES 2800
#define SAMPLER_HIST 8
#define SAMPLER_SETS (SAMPLER_ENTRIES / SAMPLER_HIST)
std::vector<std::map<uint64_t, HISTORY>> cache_history_sampler;
uint64_t sample_signature[NUM_SET][NUM_WAY];

// History timer
#define TIMER_SIZE 1024
uint64_t set_timer[NUM_SET];

// Helper macros for sampling
#define bitmask(l) (((l) == 64) ? (unsigned long long)(-1LL) : ((1LL << (l)) - 1LL))
#define bits(x, i, l) (((x) >> (i)) & bitmask(l))
#define SAMPLED_SET(set) (bits(set, 0, 6) == bits(set, (unsigned long long)(log2(NUM_SET) - 6), 6))

// Initialize replacement state
void CACHE::initialize_replacement() {
    std::cout << "Initialize Hawkeye replacement policy state" << std::endl;

    for (int i = 0; i < NUM_SET; i++) {
        for (int j = 0; j < NUM_WAY; j++) {
            rrip[i][j] = MAXRRIP;
            sample_signature[i][j] = 0;
            prefetching[i][j] = false;
        }
        set_timer[i] = 0;
        optgen_occup_vector[i].init(NUM_WAY - 2);
    }

    cache_history_sampler.resize(SAMPLER_SETS);
    for (int i = 0; i < SAMPLER_SETS; i++) {
        cache_history_sampler[i].clear();
    }

    predictor_prefetch = new Hawkeye_Predictor();
    predictor_demand = new Hawkeye_Predictor();

    std::cout << "Finished initializing Hawkeye replacement policy state" << std::endl;
}

// Find replacement victim
uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type) {
    for (uint32_t i = 0; i < NUM_WAY; i++) {
        if (rrip[set][i] == MAXRRIP) {
            return i;
        }
    }

    uint32_t max_rrpv = 0;
    int32_t victim = -1;
    for (uint32_t i = 0; i < NUM_WAY; i++) {
        if (rrip[set][i] >= max_rrpv) {
            max_rrpv = rrip[set][i];
            victim = i;
        }
    }

    if (SAMPLED_SET(set)) {
        if (prefetching[set][victim]) {
            predictor_prefetch->decrease(sample_signature[set][victim]);
        } else {
            predictor_demand->decrease(sample_signature[set][victim]);
        }
    }

    return victim;
}

// Helper function to update cache history
void update_cache_history(unsigned int sample_set, unsigned int currentVal) {
    for (auto& [key, history] : cache_history_sampler[sample_set]) {
        if (history.lru < currentVal) {
            history.lru++;
        }
    }
}

// Update replacement state
void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit) {
    full_addr = (full_addr >> 6) << 6;

    // Handle writebacks
    if (type == static_cast<uint32_t>(access_type::WRITE)) {
        return;
    }

    if (type == static_cast<uint32_t>(access_type::PREFETCH)) {
        prefetching[set][way] = !hit;
    } else {
        prefetching[set][way] = false;
    }

    if (SAMPLED_SET(set)) {
        uint64_t currentVal = set_timer[set] % OPTGEN_SIZE;
        uint64_t sample_tag = CRC(full_addr >> 12) % 256;
        uint32_t sample_set = (full_addr >> 6) % SAMPLER_SETS;

        if ((type != static_cast<uint32_t>(access_type::PREFETCH)) && cache_history_sampler[sample_set].find(sample_tag) != cache_history_sampler[sample_set].end()) {
            unsigned int current_time = set_timer[set];
            if (current_time < cache_history_sampler[sample_set][sample_tag].previousVal) {
                current_time += TIMER_SIZE;
            }
            uint64_t previousVal = cache_history_sampler[sample_set][sample_tag].previousVal % OPTGEN_SIZE;
            bool isWrap = (current_time - cache_history_sampler[sample_set][sample_tag].previousVal) > OPTGEN_SIZE;

            if (!isWrap && optgen_occup_vector[set].is_cache(currentVal, previousVal)) {
                if (cache_history_sampler[sample_set][sample_tag].prefetching) {
                    predictor_prefetch->increase(cache_history_sampler[sample_set][sample_tag].PCval);
                } else {
                    predictor_demand->increase(cache_history_sampler[sample_set][sample_tag].PCval);
                }
            } else {
                if (cache_history_sampler[sample_set][sample_tag].prefetching) {
                    predictor_prefetch->decrease(cache_history_sampler[sample_set][sample_tag].PCval);
                } else {
                    predictor_demand->decrease(cache_history_sampler[sample_set][sample_tag].PCval);
                }
            }

            optgen_occup_vector[set].set_access(currentVal);
            update_cache_history(sample_set, cache_history_sampler[sample_set][sample_tag].lru);
            cache_history_sampler[sample_set][sample_tag].prefetching = false;
        } else if (cache_history_sampler[sample_set].find(sample_tag) == cache_history_sampler[sample_set].end()) {
            if (cache_history_sampler[sample_set].size() == SAMPLER_HIST) {
                uint64_t addr_val = 0;
                for (auto it = cache_history_sampler[sample_set].begin(); it != cache_history_sampler[sample_set].end(); it++) {
                    if (it->second.lru == SAMPLER_HIST - 1) {
                        addr_val = it->first;
                        break;
                    }
                }
                cache_history_sampler[sample_set].erase(addr_val);
            }

            cache_history_sampler[sample_set][sample_tag].init();
            if (type == static_cast<uint32_t>(access_type::PREFETCH)) {
                cache_history_sampler[sample_set][sample_tag].set_prefetch();
                optgen_occup_vector[set].set_prefetch(currentVal);
            } else {
                optgen_occup_vector[set].set_access(currentVal);
            }

            update_cache_history(sample_set, SAMPLER_HIST - 1);
        }

        cache_history_sampler[sample_set][sample_tag].update(set_timer[set], ip);
        cache_history_sampler[sample_set][sample_tag].lru = 0;
        set_timer[set] = (set_timer[set] + 1) % TIMER_SIZE;
    }

    bool prediction = predictor_demand->get_prediction(ip);
    if (type == static_cast<uint32_t>(access_type::PREFETCH)) {
        prediction = predictor_prefetch->get_prediction(ip);
    }

    sample_signature[set][way] = ip;

    if (!prediction) {
        rrip[set][way] = MAXRRIP;
    } else {
        rrip[set][way] = 0;
        if (!hit) {
            bool isMaxVal = false;
            for (uint32_t i = 0; i < NUM_WAY; i++) {
                if (rrip[set][i] == MAXRRIP - 1) {
                    isMaxVal = true;
                }
            }

            for (uint32_t i = 0; i < NUM_WAY; i++) {
                if (!isMaxVal && rrip[set][i] < MAXRRIP - 1) {
                    rrip[set][i]++;
                }
            }
        }
        rrip[set][way] = 0;
    }
}

// Use this function to print out your own stats at the end of simulation
void CACHE::replacement_final_stats() {}
