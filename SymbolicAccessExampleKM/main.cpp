#include <ntifs.h>
#include <SymbolicAccess/ModuleExtender/ModuleExtenderFactory.h>

extern "C"
{
	NTSTATUS DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING)
	{
		symbolic_access::ModuleExtenderFactory extenderFactory{};
		const auto& moduleExtender = extenderFactory.Create(L"ntoskrnl.exe");
		if (!moduleExtender.has_value())
			return STATUS_UNSUCCESSFUL;

		const auto activeProcessLock = moduleExtender->GetPointer<ULONG_PTR>("PspActiveProcessLock");
		const auto activeProcessHead = moduleExtender->GetPointer<LIST_ENTRY>("PsActiveProcessHead");
		if (!activeProcessLock || !activeProcessHead)
			return STATUS_UNSUCCESSFUL;

		moduleExtender->Call("ExAcquirePushLockExclusiveEx", activeProcessLock, 0);

		auto nextProcess = activeProcessHead->Flink;
		while (nextProcess != activeProcessHead)
		{
			const auto currentEprocess = reinterpret_cast<void*>(reinterpret_cast<size_t>(nextProcess) -
				 *moduleExtender->GetOffset("_EPROCESS", "ActiveProcessLinks"));

			if (std::string_view{ moduleExtender->GetPointer<char>("_EPROCESS", "ImageFileName", currentEprocess) }.find("explorer.exe") != std::string_view::npos)
			{
				const auto currentProcessActiveProcessLinks = moduleExtender->GetPointer<LIST_ENTRY>("_EPROCESS", "ActiveProcessLinks", currentEprocess);
				currentProcessActiveProcessLinks->Blink->Flink = currentProcessActiveProcessLinks->Flink;
			}

			nextProcess = nextProcess->Flink;
		}

		moduleExtender->Call("ExReleasePushLockExclusiveEx", activeProcessLock, 0);
		return STATUS_UNSUCCESSFUL;
	}
}