#include <algorithm>
#include <cassert>
#include <map>
#include <vector>
#include <fstream> // For writing to a file

#include "cache.h"

// Define global variables to store cache access data
std::map<CACHE*, std::vector<uint64_t>> last_used_cycles;
std::map<CACHE*, std::vector<uint64_t>> eviction_cycles;

namespace
{
    std::ofstream csv_file("cache_access_data.csv");  // Open CSV file to log data

    // Write header to the CSV file
    void write_csv_header() {
        csv_file << "Memory Address,Cache Set,Access Type,Cycle Count,Data Size,Hit/Miss\n";
    }
}

// Initialize replacement state
void CACHE::repl_replacementDlruStat_initialize_replacement() {
    last_used_cycles[this] = std::vector<uint64_t>(NUM_SET * NUM_WAY, 0);
    eviction_cycles[this] = std::vector<uint64_t>(NUM_SET * NUM_WAY, 0);
    write_csv_header();  // Write CSV header at initialization
}

// Find victim for replacement based on LRU policy
uint32_t CACHE::repl_replacementDlruStat_find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t full_addr, uint64_t /* pc */, uint32_t type)
{
    auto begin = std::next(std::begin(last_used_cycles[this]), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    // Find the least recently used cache line
    auto victim = std::min_element(begin, end);
    assert(begin <= victim);
    assert(victim < end);

    uint32_t victim_way = static_cast<uint32_t>(std::distance(begin, victim));
    
    // Log the eviction cycle for the selected victim
    eviction_cycles[this].at(set * NUM_WAY + victim_way) = current_cycle;

    return victim_way; // cast protected by prior asserts
}

// Update replacement state and log the data to CSV
void CACHE::repl_replacementDlruStat_update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t /* pc */, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    // Determine access type (read/write)
    bool is_write = (static_cast<access_type>(type) == access_type::WRITE);
    std::string access_type_str = is_write ? "WRITE" : "READ";

    // Use fixed data size or retrieve if available in the trace
    uint32_t data_size = 64;  // Example: assuming 64 bytes; replace if your trace has data size info

    // Log only the relevant trace data and hit/miss status, excluding the PC
    csv_file << full_addr << "," << set << "," << access_type_str << ","
             << current_cycle << "," << data_size << ","
             << static_cast<int>(hit) << "\n";

    // Update last used cycle for this way in the set
    last_used_cycles[this].at(set * NUM_WAY + way) = current_cycle;
}

// Collect final statistics (optional for this case)
void CACHE::repl_replacementDlruStat_replacement_final_stats() {
    // Optional: Include final statistics if needed
    // This function is a placeholder and can remain empty if no final stats are required.
}
