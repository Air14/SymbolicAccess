#include <SymbolicAccess/Phnt/phnt.h>
#include <SymbolicAccess/Utils/Km/ProcessCreator.h>
#include <SymbolicAccess/Utils/Km/ScopedStackAttach.h>
#include <SymbolicAccess/Internal/memory.h>
#include <SymbolicAccess/Utils/Log.h>

#define RTL_MAX_DRIVE_LETTERS 32

typedef enum _PS_CREATE_STATE
{
	PsCreateInitialState,
	PsCreateFailOnFileOpen,
	PsCreateFailOnSectionCreate,
	PsCreateFailExeFormat,
	PsCreateFailMachineMismatch,
	PsCreateFailExeName, // Debugger specified
	PsCreateSuccess,
	PsCreateMaximumStates
} PS_CREATE_STATE;

typedef enum _PS_ATTRIBUTE_NUM
{
	PsAttributeParentProcess,                   // in HANDLE
	PsAttributeDebugPort,                       // in HANDLE
	PsAttributeToken,                           // in HANDLE
	PsAttributeClientId,                        // out PCLIENT_ID
	PsAttributeTebAddress,                      // out PTEB
	PsAttributeImageName,                       // in PWSTR
	PsAttributeImageInfo,                       // out PSECTION_IMAGE_INFORMATION
	PsAttributeMemoryReserve,                   // in PPS_MEMORY_RESERVE
	PsAttributePriorityClass,                   // in UCHAR
	PsAttributeErrorMode,                       // in ULONG
	PsAttributeStdHandleInfo,                   // in PPS_STD_HANDLE_INFO
	PsAttributeHandleList,                      // in PHANDLE
	PsAttributeGroupAffinity,                   // in PGROUP_AFFINITY
	PsAttributePreferredNode,                   // in PUSHORT
	PsAttributeIdealProcessor,                  // in PPROCESSOR_NUMBER
	PsAttributeUmsThread,                       // see MSDN UpdateProceThreadAttributeList (CreateProcessW) - in PUMS_CREATE_THREAD_ATTRIBUTES
	PsAttributeMitigationOptions,               // in UCHAR
	PsAttributeProtectionLevel,                 // in ULONG
	PsAttributeSecureProcess,                   // since THRESHOLD (Virtual Secure Mode, Device Guard)
	PsAttributeJobList,
	PsAttributeChildProcessPolicy,              // since THRESHOLD2
	PsAttributeAllApplicationPackagesPolicy,    // since REDSTONE
	PsAttributeWin32kFilter,
	PsAttributeSafeOpenPromptOriginClaim,
	PsAttributeBnoIsolation,
	PsAttributeDesktopAppPolicy,
	PsAttributeMa
}PS_ATTRIBUTE_NUM;

#define PS_ATTRIBUTE_NUMBER_MASK    0x0000ffff
#define PS_ATTRIBUTE_THREAD         0x00010000 // Attribute may be used with thread creation
#define PS_ATTRIBUTE_INPUT          0x00020000 // Attribute is input only
#define PS_ATTRIBUTE_ADDITIVE       0x00040000 // Attribute may be "accumulated", e.g. bitmasks, counters, etc.

#define PsAttributeValue(Number, Thread, Input, Additive) \
    (((Number) & PS_ATTRIBUTE_NUMBER_MASK) | \
    ((Thread) ? PS_ATTRIBUTE_THREAD : 0) | \
    ((Input) ? PS_ATTRIBUTE_INPUT : 0) | \
    ((Additive) ? PS_ATTRIBUTE_ADDITIVE : 0))

#define PS_ATTRIBUTE_PARENT_PROCESS \
    PsAttributeValue(PsAttributeParentProcess, FALSE, TRUE, TRUE) // 0x60000
#define PS_ATTRIBUTE_DEBUG_PORT \
    PsAttributeValue(PsAttributeDebugPort, FALSE, TRUE, TRUE) // 0x60001
#define PS_ATTRIBUTE_TOKEN \
    PsAttributeValue(PsAttributeToken, FALSE, TRUE, TRUE) // 0x60002
#define PS_ATTRIBUTE_CLIENT_ID \
    PsAttributeValue(PsAttributeClientId, TRUE, FALSE, FALSE) // 0x10003
#define PS_ATTRIBUTE_TEB_ADDRESS \
    PsAttributeValue(PsAttributeTebAddress, TRUE, FALSE, FALSE) // 0x10004
#define PS_ATTRIBUTE_IMAGE_NAME \
    PsAttributeValue(PsAttributeImageName, FALSE, TRUE, FALSE) // 0x20005
#define PS_ATTRIBUTE_IMAGE_INFO \
    PsAttributeValue(PsAttributeImageInfo, FALSE, FALSE, FALSE) // 0x6
#define PS_ATTRIBUTE_MEMORY_RESERVE \
    PsAttributeValue(PsAttributeMemoryReserve, FALSE, TRUE, FALSE) // 0x20007
#define PS_ATTRIBUTE_PRIORITY_CLASS \
    PsAttributeValue(PsAttributePriorityClass, FALSE, TRUE, FALSE) // 0x20008
#define PS_ATTRIBUTE_ERROR_MODE \
    PsAttributeValue(PsAttributeErrorMode, FALSE, TRUE, FALSE) // 0x20009
#define PS_ATTRIBUTE_STD_HANDLE_INFO \
    PsAttributeValue(PsAttributeStdHandleInfo, FALSE, TRUE, FALSE) // 0x2000A
#define PS_ATTRIBUTE_HANDLE_LIST \
    PsAttributeValue(PsAttributeHandleList, FALSE, TRUE, FALSE) // 0x2000B
#define PS_ATTRIBUTE_GROUP_AFFINITY \
    PsAttributeValue(PsAttributeGroupAffinity, TRUE, TRUE, FALSE) // 0x2000C
#define PS_ATTRIBUTE_PREFERRED_NODE \
    PsAttributeValue(PsAttributePreferredNode, FALSE, TRUE, FALSE) // 0x2000D
#define PS_ATTRIBUTE_IDEAL_PROCESSOR \
    PsAttributeValue(PsAttributeIdealProcessor, TRUE, TRUE, FALSE) // 0x2000E
#define PS_ATTRIBUTE_MITIGATION_OPTIONS \
    PsAttributeValue(PsAttributeMitigationOptions, FALSE, TRUE, TRUE) // 0x60010
#define PS_ATTRIBUTE_PROTECTION_LEVEL \
    PsAttributeValue(PsAttributeProtectionLevel, FALSE, TRUE, FALSE) // 0x20011

typedef struct _CURDIR
{
	UNICODE_STRING DosPath;
	HANDLE Handle;
} CURDIR, * PCURDIR;

typedef struct _RTL_DRIVE_LETTER_CURDIR
{
	USHORT Flags;
	USHORT Length;
	ULONG TimeStamp;
	UNICODE_STRING DosPath;
} RTL_DRIVE_LETTER_CURDIR, * PRTL_DRIVE_LETTER_CURDIR;

struct RTL_USER_PROCESS_PARAMETERS
{
	ULONG MaximumLength;
	ULONG Length;
	ULONG Flags;
	ULONG DebugFlags;
	VOID* ConsoleHandle;
	ULONG ConsoleFlags;
	VOID* StandardInput;
	VOID* StandardOutput;
	VOID* StandardError;
	CURDIR CurrentDirectory;
	UNICODE_STRING DllPath;
	UNICODE_STRING ImagePathName;
	UNICODE_STRING CommandLine;
	VOID* Environment;
	ULONG StartingX;
	ULONG StartingY;
	ULONG CountX;
	ULONG CountY;
	ULONG CountCharsX;
	ULONG CountCharsY;
	ULONG FillAttribute;
	ULONG WindowFlags;
	ULONG ShowWindowFlags;
	UNICODE_STRING WindowTitle;
	UNICODE_STRING DesktopInfo;
	UNICODE_STRING ShellInfo;
	UNICODE_STRING RuntimeData;
	RTL_DRIVE_LETTER_CURDIR CurrentDirectores[32];
	SIZE_T EnvironmentSize;
	SIZE_T EnvironmentVersion;
	VOID* PackageDependencyData;
	ULONG ProcessGroupId;
	ULONG LoaderThreads;
	UNICODE_STRING RedirectionDllName;
	UNICODE_STRING HeapPartitionName;
	ULONGLONG* DefaultThreadpoolCpuSetMasks;
	ULONG DefaultThreadpoolCpuSetMaskCount;
	ULONG DefaultThreadpoolThreadMaximum;
	//ULONG HeapMemoryTypeMask;
};

typedef struct _PS_CREATE_INFO
{
	SIZE_T Size;
	PS_CREATE_STATE State;
	union
	{
		// PsCreateInitialState
		struct
		{
			union
			{
				ULONG InitFlags;
				struct
				{
					UCHAR WriteOutputOnExit : 1;
					UCHAR DetectManifest : 1;
					UCHAR IFEOSkipDebugger : 1;
					UCHAR IFEODoNotPropagateKeyState : 1;
					UCHAR SpareBits1 : 4;
					UCHAR SpareBits2 : 8;
					USHORT ProhibitedImageCharacteristics : 16;
				} s1;
			} u1;
			ACCESS_MASK AdditionalFileAccess;
		} InitState;

		// PsCreateFailOnSectionCreate
		struct
		{
			HANDLE FileHandle;
		} FailSection;

		// PsCreateFailExeFormat
		struct
		{
			USHORT DllCharacteristics;
		} ExeFormat;

		// PsCreateFailExeName
		struct
		{
			HANDLE IFEOKey;
		} ExeName;

		// PsCreateSuccess
		struct
		{
			union
			{
				ULONG OutputFlags;
				struct
				{
					UCHAR ProtectedProcess : 1;
					UCHAR AddressSpaceOverride : 1;
					UCHAR DevOverrideEnabled : 1; // From Image File Execution Options
					UCHAR ManifestDetected : 1;
					UCHAR ProtectedProcessLight : 1;
					UCHAR SpareBits1 : 3;
					UCHAR SpareBits2 : 8;
					USHORT SpareBits3 : 16;
				} s2;
			} u2;
			HANDLE FileHandle;
			HANDLE SectionHandle;
			ULONGLONG UserProcessParametersNative;
			ULONG UserProcessParametersWow64;
			ULONG CurrentParameterFlags;
			ULONGLONG PebAddressNative;
			ULONG PebAddressWow64;
			ULONGLONG ManifestAddress;
			ULONG ManifestSize;
		} SuccessState;
	};
} PS_CREATE_INFO, * PPS_CREATE_INFO;

typedef struct _PS_ATTRIBUTE
{
	ULONG_PTR Attribute;                // PROC_THREAD_ATTRIBUTE_XXX | PROC_THREAD_ATTRIBUTE_XXX modifiers, see ProcThreadAttributeValue macro and Windows Internals 6 (372)
	SIZE_T Size;                        // Size of Value or *ValuePtr
	union
	{
		ULONG_PTR Value;                // Reserve 8 bytes for data (such as a Handle or a data pointer)
		PVOID ValuePtr;                 // data pointer
	};
	PSIZE_T ReturnLength;               // Either 0 or specifies size of data returned to caller via "ValuePtr"
} PS_ATTRIBUTE, * PPS_ATTRIBUTE;

typedef struct _PS_ATTRIBUTE_LIST
{
	SIZE_T TotalLength;                 // sizeof(PS_ATTRIBUTE_LIST)
	PS_ATTRIBUTE Attributes[2];         // Depends on how many attribute entries should be supplied to NtCreateUserProcess
} PS_ATTRIBUTE_LIST, * PPS_ATTRIBUTE_LIST;

extern "C"
PPEB NTAPI PsGetProcessPeb(IN PEPROCESS Process);

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwCreateUserProcess(
	_Out_ PHANDLE ProcessHandle,
	_Out_ PHANDLE ThreadHandle,
	_In_ ACCESS_MASK ProcessDesiredAccess,
	_In_ ACCESS_MASK ThreadDesiredAccess,
	_In_opt_ POBJECT_ATTRIBUTES ProcessObjectAttributes,
	_In_opt_ POBJECT_ATTRIBUTES ThreadObjectAttributes,
	_In_ ULONG ProcessFlags,
	_In_ ULONG ThreadFlags,
	_In_ RTL_USER_PROCESS_PARAMETERS* ProcessParameters,
	_Inout_ PPS_CREATE_INFO CreateInfo,
	_In_ PPS_ATTRIBUTE_LIST AttributeList
);

namespace symbolic_access
{
	ProcessCreator::ProcessCreator(PEPROCESS CsrssProcess, size_t ZwCreateUserProcessAddress) :
		m_CsrssProcess(CsrssProcess), m_ZwCreateUserProcess(ZwCreateUserProcessAddress)
	{
	}
	
	std::pair<ScopedHandle, ScopedHandle> ProcessCreator::CreateUmProcess(std::wstring_view ImagePath, 
		std::wstring_view CommandLine, std::wstring_view CurrentDirectory, std::wstring_view DllPath)
	{
		if (CurrentDirectory.size() * sizeof(wchar_t) >= m_MaxCurDirSize)
		{
			PrintDbg("CurrentDirectory string is too big\n");
			return {};
		}

		ScopedStackAttach scopedStackAttach(m_CsrssProcess);
		const auto currentProcessParatmeters = reinterpret_cast<RTL_USER_PROCESS_PARAMETERS*>(*(reinterpret_cast<size_t*>(PsGetProcessPeb(IoGetCurrentProcess())) + 4));
		if (!currentProcessParatmeters)
		{
			PrintDbg("Process parameters in csrss is null\n");
			return {};
		}

		const auto finalParamtersSize = static_cast<size_t>(sizeof(RTL_USER_PROCESS_PARAMETERS) + m_MaxCurDirSize + 0x100 +
			(ImagePath.size() + CommandLine.size() + DllPath.size()) * sizeof(wchar_t) + currentProcessParatmeters->EnvironmentSize);

		const auto buffer = internal::make_unique<uint8_t[]>(finalParamtersSize);
		if (!buffer)
		{
			PrintDbg("Failed to allocate memory for process parameters\n");
			return {};
		}
		
		const auto processParameters = reinterpret_cast<RTL_USER_PROCESS_PARAMETERS*>(buffer.get());
		processParameters->MaximumLength = static_cast<ULONG>(finalParamtersSize - currentProcessParatmeters->EnvironmentSize);
		processParameters->Length = processParameters->MaximumLength;
		processParameters->Flags = 0x1;

		processParameters->CurrentDirectory.DosPath.Buffer = reinterpret_cast<wchar_t*>(reinterpret_cast<size_t>(processParameters) + sizeof(RTL_USER_PROCESS_PARAMETERS));
		processParameters->CurrentDirectory.DosPath.Length = processParameters->CurrentDirectory.DosPath.MaximumLength = static_cast<USHORT>(CurrentDirectory.size() * sizeof(wchar_t));
		memcpy(processParameters->CurrentDirectory.DosPath.Buffer, CurrentDirectory.data(), processParameters->CurrentDirectory.DosPath.Length);

		processParameters->ImagePathName.Buffer = reinterpret_cast<wchar_t*>(reinterpret_cast<size_t>(processParameters) + sizeof(RTL_USER_PROCESS_PARAMETERS) + m_MaxCurDirSize);
		processParameters->ImagePathName.Length = processParameters->ImagePathName.MaximumLength = static_cast<USHORT>(ImagePath.size() * sizeof(wchar_t));
		memcpy(processParameters->ImagePathName.Buffer, ImagePath.data(), processParameters->ImagePathName.Length);

		processParameters->CommandLine.Buffer = processParameters->ImagePathName.Buffer + ImagePath.size() + 1;
		processParameters->CommandLine.Length = processParameters->CommandLine.MaximumLength = static_cast<USHORT>(CommandLine.size() * sizeof(wchar_t));
		memcpy(processParameters->CommandLine.Buffer, CommandLine.data(), processParameters->CommandLine.Length);

		processParameters->DllPath.Buffer = processParameters->CommandLine.Buffer + CommandLine.size() + 1;
		processParameters->DllPath.Length = processParameters->DllPath.MaximumLength = static_cast<USHORT>(DllPath.size() * sizeof(wchar_t));
		memcpy(processParameters->DllPath.Buffer, DllPath.data(), processParameters->DllPath.Length);

		processParameters->Environment = processParameters->DllPath.Buffer + DllPath.size() + 1;
		processParameters->EnvironmentSize = currentProcessParatmeters->EnvironmentSize;
		memcpy(processParameters->Environment, currentProcessParatmeters->Environment, static_cast<size_t>(processParameters->EnvironmentSize));

		PS_CREATE_INFO createInfo{};
		createInfo.Size = sizeof(PS_CREATE_INFO);
		createInfo.State = PsCreateInitialState;

		PS_ATTRIBUTE_LIST attributeList{};
		attributeList.TotalLength = sizeof(PS_ATTRIBUTE_LIST) - sizeof(PS_ATTRIBUTE);

		attributeList.Attributes[0].Attribute = PS_ATTRIBUTE_IMAGE_NAME;
		attributeList.Attributes[0].Size = ImagePath.size() * 2;
		attributeList.Attributes[0].Value = reinterpret_cast<ULONG_PTR>(ImagePath.data());

		OBJECT_ATTRIBUTES kernelObjectAttribute{};
		kernelObjectAttribute.Length = sizeof(OBJECT_ATTRIBUTES);
		kernelObjectAttribute.Attributes = OBJ_KERNEL_HANDLE;

		HANDLE processHandle{};
		HANDLE threadHandle{};
		const auto status = reinterpret_cast<decltype(&ZwCreateUserProcess)>(m_ZwCreateUserProcess)(&processHandle, &threadHandle, 0x2000000, 0x2000000,
			&kernelObjectAttribute, &kernelObjectAttribute, NULL, NULL, processParameters, &createInfo, &attributeList);
		
		if (!NT_SUCCESS(status))
		{
			PrintDbg("Failed to create process %S\tstatus: \n", ImagePath.data());
			return {};
		}

		return std::make_pair(ScopedHandle(processHandle), ScopedHandle(threadHandle));
	}
}