
#include <cstdint>  // For uint64_t and other fixed-width integer types
#include <iostream>  // For std::cout and std::endl
#include <map>       // For the Hawkeye predictor maps
#include <algorithm> // For LRU fallback
#include <vector>    // For OPTgen tracking during training
#include "cache.h"   // For CACHE and ChampSim-specific structures

#ifndef LLC_SETS
#define LLC_SETS 2048  // Number of sets in LLC
#endif

#ifndef LLC_WAYS
#define LLC_WAYS 16  // Number of ways in LLC
#endif

namespace {

// Predictor structure for Hawkeye (demand and prefetch predictors)
struct Hawkeye_Predictor {
    std::map<uint64_t, int> reuse_map;  // Tracks reuse behavior of program counters (PCs)
    const int max_reuse_count = 1024;   // Max reuse count for prediction
    int threshold = max_reuse_count / 2;  // Threshold for dead/live prediction

    // Get prediction (true = dead, false = live)
    bool predict(uint64_t PC) const {
        auto it = reuse_map.find(PC);
        if (it != reuse_map.end()) {
            return it->second < threshold;  // Predict dead if below threshold
        }
        return true;  // Default prediction is "dead"
    }

    // Increase reuse count for a PC
    void increase(uint64_t PC) {
        auto& count = reuse_map[PC];
        if (count < max_reuse_count) {
            ++count;
        }
    }

    // Decrease reuse count for a PC
    void decrease(uint64_t PC) {
        auto& count = reuse_map[PC];
        if (count > 0) {
            --count;
        }
    }

    // Train the predictor using OPTgen decisions during the offline phase
    void train(uint64_t PC, bool is_dead) {
        if (is_dead) {
            decrease(PC);  // Decrease the reuse count if block is marked as dead
        } else {
            increase(PC);  // Increase the reuse count if block is marked as live
        }
    }
};

// OPTgen component used during training (offline phase)
struct OPTgen {
    std::vector<uint64_t> future_use;  // Tracks the future use distance of cache blocks

    // Initialize the OPTgen tracker with cache size
    void init(uint64_t cache_size) {
        future_use.resize(cache_size, UINT64_MAX);  // Use max value to represent no future use
    }

    // Set future use distance for a block
    void set_future_use(uint64_t index, uint64_t distance) {
        if (index < future_use.size()) {
            future_use[index] = distance;
        }
    }

    // Get the block with the furthest future use (Belady-optimal eviction candidate)
    uint64_t get_optimal_victim() {
        return std::distance(future_use.begin(), std::max_element(future_use.begin(), future_use.end()));
    }
};

Hawkeye_Predictor demand_predictor;   // Demand predictor
Hawkeye_Predictor prefetch_predictor;  // Prefetch predictor

// LRU tracking for fallback
std::map<CACHE*, std::vector<uint64_t>> last_used_cycles;

OPTgen optgen_occup_vector[LLC_SETS];  // OPTgen data structure for Belady-optimal predictions during training

}

// Initialize the Hawkeye replacement policy (including LRU tracking and OPTgen for training)
void CACHE::initialize_replacement() {
    last_used_cycles[this] = std::vector<uint64_t>(NUM_SET * NUM_WAY);  // LRU initialization
    for (int i = 0; i < LLC_SETS; i++) {
        optgen_occup_vector[i].init(LLC_WAYS);  // Initialize OPTgen for each set during training
    }
}

// Offline training using OPTgen to guide the reuse predictor
void CACHE::train_replacement_policy(uint32_t set, uint64_t PC, bool is_dead) {
    // Train the Hawkeye predictor using the OPTgen decision (dead/live)
    demand_predictor.train(PC, is_dead);
}

// Find a victim cache block to evict using Hawkeye prediction logic and LRU fallback
uint32_t CACHE::find_victim(uint32_t cpu_id, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type) {
    bool is_dead = demand_predictor.predict(ip);  // Predict dead for demand
    
    // Correct PREFETCH comparison
    if (type == 2) {  // 2 corresponds to PREFETCH type in ChampSim
        is_dead = prefetch_predictor.predict(ip);  // Predict dead for prefetches
    }

    // If prediction says dead, evict the first valid block
    for (uint32_t i = 0; i < LLC_WAYS; ++i) {
        if (!current_set[i].valid) {  // If block is invalid, it's a natural victim
            return i;
        }
        if (is_dead) {
            return i;  // Evict based on dead prediction
        }
    }

    // Fallback to LRU eviction method
    auto begin = std::next(std::begin(last_used_cycles[this]), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);
    auto victim = std::min_element(begin, end);
    return static_cast<uint32_t>(std::distance(begin, victim));
}

// Update the replacement state when a block is accessed (hit or miss)
void CACHE::update_replacement_state(uint32_t cpu_id, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit) {
    // Update predictors based on the hit/miss
    if (hit) {
        demand_predictor.increase(ip);
    } else {
        demand_predictor.decrease(ip);
    }

    if (type == 2) {  // PREFETCH type in ChampSim
        if (hit) {
            prefetch_predictor.increase(ip);
        } else {
            prefetch_predictor.decrease(ip);
        }
    }

    // Update LRU state
    if (!hit || access_type{type} != access_type::WRITE)  // Skip for writeback hits
        last_used_cycles[this].at(set * NUM_WAY + way) = current_cycle;
}

// Print final statistics for the Hawkeye replacement policy
void CACHE::replacement_final_stats() {
    std::cout << "Final stats for Hawkeye replacement policy (OPTgen used during offline training)." << std::endl;
}
