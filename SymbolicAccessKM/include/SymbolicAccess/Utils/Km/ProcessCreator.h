#pragma once
#include <SymbolicAccess/Utils/ProcessCreatorInterface.h>
#include <SymbolicAccess/Phnt/phnt.h>

namespace symbolic_access
{
	class ProcessCreator : public ProcessCreatorInterface
	{
	public:
		ProcessCreator(PEPROCESS CsrssProcess, size_t ZwCreateUserProcessAddress);

		std::pair<ScopedHandle, ScopedHandle> CreateUmProcess(std::wstring_view ImagePath, 
			std::wstring_view CommandLine, std::wstring_view CurrentDirectory, std::wstring_view DllPath) override;

	private:
		const size_t m_MaxCurDirSize = 0x208;
		PEPROCESS m_CsrssProcess;
		size_t m_ZwCreateUserProcess;
	};
}