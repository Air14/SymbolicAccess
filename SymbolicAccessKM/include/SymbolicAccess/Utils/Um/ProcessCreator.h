#pragma once
#include <SymbolicAccess/Utils/ProcessCreatorInterface.h>

namespace symbolic_access
{
	class ProcessCreator : public ProcessCreatorInterface
	{
	public:
		// First two paramteres are mandatory
		std::pair<ScopedHandle, ScopedHandle> CreateUmProcess(std::wstring_view ImagePath,
			std::wstring_view CommandLine, std::wstring_view CurrentDirectory, std::wstring_view DllPath) override;
	};
}