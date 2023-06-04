#include <SymbolicAccess/Phnt/phnt.h>
#include <SymbolicAccess/Utils/Log.h>

namespace symbolic_access
{
	void PrintToDebugger(std::string_view Format, ...)
	{
		char messageBuffer[1024]{};

		va_list args{};
		va_start(args, Format);
		vsnprintf(messageBuffer, sizeof(messageBuffer), Format.data(), args);
		va_end(args);

		OutputDebugStringA(messageBuffer);
	}
}