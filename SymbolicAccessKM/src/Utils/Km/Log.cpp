#include <SymbolicAccess/Phnt/phnt.h>
#include <SymbolicAccess/Utils/Log.h>
#include <cstdarg>
#include <ntstrsafe.h>

namespace symbolic_access
{
	void PrintToDebugger(std::string_view Format, ...)
	{
		char messageBuffer[1024]{};

		va_list args{};
		va_start(args, Format);
		RtlStringCchVPrintfA(messageBuffer, sizeof(messageBuffer), Format.data(), args);
		va_end(args);

		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "%s", messageBuffer);
	}
}