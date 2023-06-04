#pragma once
#include <SymbolicAccess/Utils/ProcessCreatorInterface.h>
#include <SymbolicAccess/Internal/memory.h>

namespace symbolic_access
{
	class ProcessCreatorFactory
	{
	public:
		ProcessCreatorFactory() = delete;

		static internal::unique_ptr<ProcessCreatorInterface> Create();
	};
}