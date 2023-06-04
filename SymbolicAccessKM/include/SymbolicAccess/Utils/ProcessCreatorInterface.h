#pragma once
#include <SymbolicAccess/Utils/ScopedHandle.h>
#include <utility>
#include <string_view>

namespace symbolic_access
{
	class ProcessCreatorInterface
	{
	public:
		virtual ~ProcessCreatorInterface() = default;

		virtual std::pair<ScopedHandle, ScopedHandle> CreateUmProcess(std::wstring_view ImagePath,
			std::wstring_view CommandLine, std::wstring_view CurrentDirectory, std::wstring_view DllPath) = 0;
	};
}