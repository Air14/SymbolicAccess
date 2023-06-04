#pragma once
#include <SymbolicAccess/Internal/vector.h>
#include <string_view>

namespace symbolic_access
{
	internal::vector<uint8_t> GetRegistryData(std::wstring_view RegistryPath, std::wstring_view RegistryKeyName);
}