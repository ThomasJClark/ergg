#include "logs.hpp"

#include <array>
#include <memory>

using namespace std;

static auto logs_ring_ptr = make_unique<array<gg::logs::log_entry_st, 50>>();
static size_t logs_begin = 0;
static size_t logs_size = 0;

gg::logs::log_entry_st &gg::logs::impl::insert()
{
    auto &logs_ring = *logs_ring_ptr;

    if (logs_size == logs_ring.size())
    {
        auto &oldest_entry = logs_ring[logs_begin];
        logs_begin = (logs_begin + 1) % logs_ring.size();
        oldest_entry.~log_entry_st();
        return oldest_entry;
    }
    return logs_ring[logs_size++];
}

size_t gg::logs::size()
{
    return logs_size;
}

void gg::logs::for_each(function<void(const gg::logs::log_entry_st &)> callback)
{
    auto &logs_ring = *logs_ring_ptr;

    auto limit = logs_begin + logs_size;
    for (size_t i = logs_begin; i < limit; i++)
    {
        callback(logs_ring[i % logs_ring.size()]);
    }
}