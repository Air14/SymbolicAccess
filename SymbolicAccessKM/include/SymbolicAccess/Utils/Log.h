#pragma once
#include <string_view>

namespace symbolic_access
{
	void PrintToDebugger(std::string_view Format, ...);

#ifdef ENABLE_LOGGING
#define PrintDbg(Format, ...) PrintToDebugger("[SymbolicAccess][%s:%d]: " Format, __func__, __LINE__, __VA_ARGS__)
#else
#define PrintDbg(Format, ...) 
#endif
}
