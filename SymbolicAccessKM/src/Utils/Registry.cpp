#include <SymbolicAccess/Phnt/phnt.h>
#include <SymbolicAccess/Utils/Registry.h>
#include <SymbolicAccess/Utils/ScopedHandle.h>
#include <SymbolicAccess/Utils/Log.h>

#ifndef _KERNEL_MODE
#define ZwOpenKey(...) NtOpenKey(__VA_ARGS__)
#define ZwQueryValueKey(...) NtQueryValueKey(__VA_ARGS__)
#endif

namespace symbolic_access
{
	internal::vector<uint8_t> GetRegistryData(std::wstring_view RegistryPath, std::wstring_view RegistryKeyName)
	{
		UNICODE_STRING valueName{ static_cast<USHORT>(RegistryKeyName.size() * 2), 
			 static_cast<USHORT>(RegistryKeyName.size() * 2), const_cast<PWCH>(RegistryKeyName.data()) };

		UNICODE_STRING registryPath{ static_cast<USHORT>(RegistryPath.size() * 2),
			 static_cast<USHORT>(RegistryPath.size() * 2), const_cast<PWCH>(RegistryPath.data()) };

		OBJECT_ATTRIBUTES objectAttributes{};
		InitializeObjectAttributes(&objectAttributes, &registryPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

		HANDLE registryKey{};
		ScopedHandle scopedRegistryKey(registryKey);
		auto status = ZwOpenKey(&registryKey, KEY_QUERY_VALUE, &objectAttributes);
		if (!NT_SUCCESS(status))
		{
			PrintDbg("Failed to open registry %S\\%S\tstatus: 0x%X\n", RegistryPath.data(), RegistryKeyName.data(), status);
			return {};
		}

		ULONG keyInfoSize{};
		status = ZwQueryValueKey(registryKey, &valueName, KeyValueFullInformation, nullptr, 0, &keyInfoSize);
		if (!keyInfoSize)
		{
			PrintDbg("Failed to query registry %S\\%S size\tstatus: 0x%X\n", RegistryPath.data(), RegistryKeyName.data(), status);
			return {};
		}

		auto buffer = internal::vector<uint8_t>(keyInfoSize);
		const auto keyInfo = reinterpret_cast<PKEY_VALUE_FULL_INFORMATION>(buffer.data());
		if (!keyInfo)
		{
			PrintDbg("Failed to allocate memory for registry %S\\%S data\n", RegistryPath.data(), RegistryKeyName.data());
			return {};
		}

		status = ZwQueryValueKey(registryKey, &valueName, KeyValueFullInformation, keyInfo, keyInfoSize, &keyInfoSize);
		if (!NT_SUCCESS(status))
		{
			PrintDbg("Failed to query registry %S\\%S data\tstatus: 0x%X\n", RegistryPath.data(), RegistryKeyName.data(), status);
			return {};
		}

		return internal::vector<uint8_t>(buffer.begin() + keyInfo->DataOffset, buffer.begin() + keyInfo->DataOffset + keyInfo->DataLength);
	}
}