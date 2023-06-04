#pragma once
#include <SymbolicAccess/Phnt/phnt.h>

class ScopedStackAttach
{
public:
	ScopedStackAttach(PEPROCESS Process)
	{
		KeStackAttachProcess(Process, &m_State);
	}

	~ScopedStackAttach()
	{
		KeUnstackDetachProcess(&m_State);
	}

	ScopedStackAttach& operator=(const ScopedStackAttach&) = delete;
	ScopedStackAttach(const ScopedStackAttach&) = delete;
private:
	KAPC_STATE m_State;
};