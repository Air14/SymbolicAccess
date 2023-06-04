#pragma once
#include <vector>
#include "allocator.h"

namespace symbolic_access::internal
{
    template <typename T, typename TAllocator = Allocator<T>>
    using vector = std::vector<T, TAllocator>;
}