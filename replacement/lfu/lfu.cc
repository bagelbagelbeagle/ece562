#include <algorithm>
#include <cassert>
#include <map>
#include <vector>

#include "cache.h"

namespace
{
// Store the access frequency for each block
std::map<CACHE*, std::vector<uint64_t>> access_frequencies;
}

void CACHE::initialize_replacement() 
{
  // Initialize access frequency counts to 0 for each block
  ::access_frequencies[this] = std::vector<uint64_t>(NUM_SET * NUM_WAY, 0);
}

uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  // Find the block with the least access frequency in the set
  auto begin = std::next(std::begin(::access_frequencies[this]), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  // Find the way with the least frequency
  auto victim = std::min_element(begin, end);
  assert(begin <= victim);
  assert(victim < end);
  
  return static_cast<uint32_t>(std::distance(begin, victim)); // cast protected by prior asserts
}

void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
  // Increment access frequency on each hit or miss
  ::access_frequencies[this].at(set * NUM_WAY + way)++;
}

void CACHE::replacement_final_stats() 
{
  // Optionally: Gather and print statistics about the access frequencies
}
