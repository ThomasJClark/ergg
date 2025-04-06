#pragma once

#include <functional>
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

namespace impl
{
log_entry_st &insert();
}

/**
 * Append a new log message to the ring buffer, overwriting the oldest message if the buffer is
 * full
 */
template <class... A> inline void log(A &&...args)
{
    new (&impl::insert()) log_entry_st(std::forward<A>(args)...);
}

size_t size();

void for_each(std::function<void(const log_entry_st &)> callback);

}
}
