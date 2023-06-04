#pragma once
#include <memory>
#ifdef _KERNEL_MODE
#include <SymbolicAccess/Utils/Km/jxy/allocator.h>
#endif

namespace symbolic_access::internal
{
	template<typename T>
	using Allocator =
#ifdef _KERNEL_MODE
		jxy::allocator<T>
#else
		std::allocator<T>
#endif
		;

	template<typename T>
	using DefaultDelete =
#ifdef _KERNEL_MODE
		jxy::default_delete<T>
#else
		std::default_delete<T>
#endif
		;
}