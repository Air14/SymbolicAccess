#include <SymbolicAccess/Phnt/phnt.h>
#include <string_view>

namespace symbolic_access
{
	size_t GetModuleAddress(std::wstring_view ModuleName)
	{
		return reinterpret_cast<size_t>(GetModuleHandleW(ModuleName.data()));
	}
}