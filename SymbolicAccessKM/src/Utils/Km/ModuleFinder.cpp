#include <SymbolicAccess/Phnt/phnt.h>
#include <SymbolicAccess/Utils/ModuleFinder.h>
#include <SymbolicAccess/Internal/memory.h>
#include <SymbolicAccess/Internal/string.h>

extern "C"
NTKERNELAPI NTSTATUS NTAPI ZwQuerySystemInformation
(
	IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
	OUT PVOID SystemInformation,
	IN ULONG SystemInformationLength,
	OUT PULONG ReturnLength OPTIONAL
);

namespace symbolic_access
{
	// Must be with extension e.g. ntoskrnl.exe, win32k.sys, ntdll.dll
	size_t GetModuleAddress(std::wstring_view ModuleName)
	{
		ULONG moduleInfoSize{};
		ZwQuerySystemInformation(SystemModuleInformation, 0, moduleInfoSize, &moduleInfoSize);

		const auto buffer = internal::make_unique<char[]>(moduleInfoSize + 1000);
		if (!buffer)
			return {};

		if (!NT_SUCCESS(ZwQuerySystemInformation(SystemModuleInformation, buffer.get(), moduleInfoSize, &moduleInfoSize)))
			return {};

		const auto mods = reinterpret_cast<RTL_PROCESS_MODULES*>(buffer.get());
		for (size_t i{}; i < mods->NumberOfModules; ++i)
		{
			std::string_view tmpPathName{ reinterpret_cast<char*>(&mods->Modules[i].FullPathName[0]) };
			internal::wstring pathName{ tmpPathName.begin(), tmpPathName.end() };
			if (pathName.find(ModuleName) != internal::wstring::npos)
				return reinterpret_cast<size_t>(mods->Modules[i].ImageBase);
		}

		return {};
	}
}