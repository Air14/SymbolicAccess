#include <SymbolicAccess/Phnt/phnt.h>
#include <SymbolicAccess/Utils/Um/ProcessCreator.h>
#include <SymbolicAccess/Utils/Log.h>

namespace symbolic_access
{
	std::pair<ScopedHandle, ScopedHandle> ProcessCreator::CreateUmProcess(std::wstring_view ImagePath,
		std::wstring_view CommandLine, std::wstring_view CurrentDirectory, std::wstring_view)
	{
		STARTUPINFOW startupInfo{};
		PROCESS_INFORMATION processInfo{};
		if (!CreateProcessW(ImagePath.data(), const_cast<LPWSTR>(CommandLine.data()),
			nullptr, nullptr, false, 0, 0, CurrentDirectory.data(), &startupInfo, &processInfo))
		{
			PrintDbg("Failed to create process %S\n", ImagePath.data());
			return {};
		}

		return std::make_pair(processInfo.hProcess, processInfo.hThread);
	}
}