#include <SymbolicAccess/Phnt/phnt.h>
#include <SymbolicAccess/Utils/ProcessCreatorFactory.h>
#include <SymbolicAccess/Utils/Km/ProcessCreator.h>
#include <SymbolicAccess/Utils/Log.h>

extern "C"
NTSYSCALLAPI NTSTATUS NTAPI ZwQuerySystemInformation(
	_In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
	_Out_writes_bytes_opt_(SystemInformationLength) PVOID SystemInformation,
	_In_ ULONG SystemInformationLength,
	_Out_opt_ PULONG ReturnLength
);

namespace symbolic_access
{
	namespace
	{
		template <typename T>
		PEPROCESS PidToProcess(T Pid)
		{
			PEPROCESS Process;
			PsLookupProcessByProcessId((HANDLE)Pid, &Process);
			return Process;
		}

		PEPROCESS GetProcessByName(std::wstring_view ProcessName)
		{
			ULONG processInfoSize{};
			ZwQuerySystemInformation(SystemProcessInformation, 0, 0, &processInfoSize);
			const auto buffer = internal::make_unique<char[]>(processInfoSize + 0x1000);
			if (!buffer)
				return {};

			if (!NT_SUCCESS(ZwQuerySystemInformation(SystemProcessInformation, buffer.get(), processInfoSize, &processInfoSize)))
				return {};

			const auto processInfo = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(buffer.get());
			for (auto entry = processInfo; entry->NextEntryOffset; entry = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(reinterpret_cast<size_t>(entry) + entry->NextEntryOffset))
			{
				if (entry->ImageName.Buffer && std::wstring_view{ entry->ImageName.Buffer }.find(ProcessName) != std::wstring_view::npos)
					return PidToProcess(entry->UniqueProcessId);
			}

			return {};
		}

	}

	internal::unique_ptr<ProcessCreatorInterface> ProcessCreatorFactory::Create()
	{
		UNICODE_STRING zwFunctionName{};
		RtlInitUnicodeString(&zwFunctionName, L"ZwCreateTransactionManager");

#ifdef _X86_
		const auto zwCreateUserProcess = reinterpret_cast<size_t>(MmGetSystemRoutineAddress(&zwFunctionName)) - 0x14;
#elif defined (_AMD64_) || defined(_M_ARM64 )
		const auto zwCreateUserProcess = reinterpret_cast<size_t>(MmGetSystemRoutineAddress(&zwFunctionName)) + 0x20;
#endif
		if (!zwCreateUserProcess)
		{
			PrintDbg("ZwCreateUserProcess address is null\n");
			return {};
		}

		const auto csrssProcess = GetProcessByName(L"csrss.exe");
		if (!csrssProcess)
		{
			PrintDbg("Failed to get csrss process\n");
			return {};
		}
		
		return internal::unique_ptr<ProcessCreatorInterface>(new(POOL_TYPE::NonPagedPool, 'syma')
			ProcessCreator(csrssProcess, zwCreateUserProcess));
	}
}