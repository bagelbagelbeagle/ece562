
#include <algorithm>
#include <cassert>
#include <map>
#include <vector>
#include <fstream> // For writing to a file

#include "cache.h"

namespace
{
    std::map<CACHE*, std::vector<uint64_t>> last_used_cycles;  // For temporal locality
    std::map<CACHE*, std::vector<bool>> valid_status;          // Cache line valid status
    std::map<CACHE*, std::vector<bool>> dirty_status;          // Cache line dirty status
    std::map<CACHE*, std::vector<uint64_t>> eviction_cycles;   // Tracks when the cache line was last evicted
    std::ofstream csv_file("cache_access_data.csv");  // Open CSV file to log data

    // Write header to the CSV file
    void write_csv_header() {
        csv_file << "PC,Memory Address,Cache Set,Access Type,Hit/Miss,Cycle Count,Time Since Last Access,Valid Status,Dirty Status,Cache Occupancy,Last Eviction Cycle\n";
    }
}

// Initialize replacement state
void CACHE::initialize_replacement() {
    ::last_used_cycles[this] = std::vector<uint64_t>(NUM_SET * NUM_WAY, 0);
    ::valid_status[this] = std::vector<bool>(NUM_SET * NUM_WAY, false);
    ::dirty_status[this] = std::vector<bool>(NUM_SET * NUM_WAY, false);
    ::eviction_cycles[this] = std::vector<uint64_t>(NUM_SET * NUM_WAY, 0);
    write_csv_header();  // Write CSV header at initialization
}

// Find victim for replacement based on LRU policy
uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    auto begin = std::next(std::begin(::last_used_cycles[this]), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    // Find the least recently used cache line
    auto victim = std::min_element(begin, end);
    assert(begin <= victim);
    assert(victim < end);
    return static_cast<uint32_t>(std::distance(begin, victim)); // cast protected by prior asserts
}

// Update replacement state and log the data to CSV
void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    // Determine access type (read/write)
    bool is_write = (static_cast<access_type>(type) == access_type::WRITE);
    std::string access_type_str = is_write ? "WRITE" : "READ";

    // Time Since Last Access (Temporal Locality)
    uint64_t time_since_last_access = current_cycle - ::last_used_cycles[this].at(set * NUM_WAY + way);

    // Cache Line Status
    bool is_valid = ::valid_status[this].at(set * NUM_WAY + way);
    bool is_dirty = ::dirty_status[this].at(set * NUM_WAY + way);

    // Cache Occupancy (Number of valid lines in the set)
    uint32_t cache_occupancy = 0;
    for (size_t i = set * NUM_WAY; i < (set + 1) * NUM_WAY; ++i) {
        if (::valid_status[this].at(i)) {
            cache_occupancy++;
        }
    }

    // Eviction History
    uint64_t last_eviction_cycle = ::eviction_cycles[this].at(set * NUM_WAY + way);

    // Update the status of the cache line being accessed
    if (!hit || !is_write) {
        ::last_used_cycles[this].at(set * NUM_WAY + way) = current_cycle;
        ::valid_status[this].at(set * NUM_WAY + way) = true;
        ::dirty_status[this].at(set * NUM_WAY + way) = is_write;  // Mark as dirty on write
    }

    // Log the access details to CSV
    csv_file << ip << "," << full_addr << "," << set << "," << access_type_str << "," 
             << static_cast<int>(hit) << "," << current_cycle << "," << time_since_last_access << "," 
             << is_valid << "," << is_dirty << "," << cache_occupancy << "," << last_eviction_cycle << "\n";
}

// Collect final statistics (optional for this case)
void CACHE::replacement_final_stats() {}
