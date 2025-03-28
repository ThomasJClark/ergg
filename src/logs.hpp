#pragma once

#include <array>
#include <string>

namespace gg
{
namespace logs
{

struct log_entry_st
{
    std::string message;
    int x;
    int y;
};

extern std::array<log_entry_st, 2048> logs_ring;
extern size_t logs_begin;
extern size_t logs_size;

/**
 * Append a new log message to the ring buffer, overwriting the oldest message if the buffer is
 * full
 */
template <class... A> inline void log(A &&...args)
{
    if (logs_size == logs_ring.size())
    {
        auto &oldest_entry = logs_ring[logs_begin++];
        logs_begin = logs_begin % logs_ring.size();
        new (&oldest_entry) log_entry_st(std::forward<A>(args)...);
    }
    else
    {
        new (&logs_ring[logs_size++]) log_entry_st(std::forward<A>(args)...);
    }
}

}
}
