#pragma once
#include <string>
#include "allocator.h"

namespace symbolic_access::internal
{
    template <typename T, typename TAllocator = Allocator<T>>
    using basic_string = std::basic_string<T, std::char_traits<T>, TAllocator>;

    using string = basic_string<char, Allocator<char>>;

    using wstring = basic_string<wchar_t, Allocator<wchar_t>>;
}