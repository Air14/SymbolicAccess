#pragma once
#include <map>
#include "allocator.h"

namespace symbolic_access::internal
{
    template <typename TKey,
        typename T,
        typename TLess = std::less<TKey>,
        typename TAlloc = Allocator<std::pair<const TKey, T>>>
        using map = std::map<TKey, T, TLess, TAlloc>;
}