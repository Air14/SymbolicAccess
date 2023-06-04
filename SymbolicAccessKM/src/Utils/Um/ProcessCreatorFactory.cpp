#include <SymbolicAccess/Utils/ProcessCreatorFactory.h>
#include <SymbolicAccess/Internal/memory.h>
#include <SymbolicAccess/Utils/Um/ProcessCreator.h>

namespace symbolic_access
{
	internal::unique_ptr<ProcessCreatorInterface> ProcessCreatorFactory::Create()
	{
		return internal::make_unique<ProcessCreator>();
	}
}