#include "DiaSymbols.h"

#include <array>
#include <format>

HRESULT NoRegCoCreate(std::wstring_view DllName, REFCLSID Rclsid, REFIID Riid, void** Ppv)
{
	const auto mod = LoadLibraryW(DllName.data());
	if (!mod)
		return REASON_UNKNOWN;

	const auto dllGetClassObject = reinterpret_cast<HRESULT(__stdcall*)(REFCLSID, REFIID, LPVOID*)>(GetProcAddress(mod, "DllGetClassObject"));
	if (!dllGetClassObject)
		return REASON_UNKNOWN;

	IClassFactory* classFactory;
	if (FAILED(dllGetClassObject(Rclsid, IID_IClassFactory, reinterpret_cast<void**>(&classFactory))))
		return REASON_UNKNOWN;

	const auto result = classFactory->CreateInstance(nullptr, Riid, Ppv);
	classFactory->AddRef();
	return result;
}

CComPtr<IDiaSymbol> DiaSymbols::CreateGlobal(std::wstring_view PdbPath)
{
	auto status = CoInitialize(nullptr);
	if (FAILED(status))
		return {};

	CComPtr<IDiaDataSource> dataSource{};
	status = NoRegCoCreate(LR"(msdia\)" PDB_ARCH_DIR LR"(\msdia140.dll)", __uuidof(DiaSource), __uuidof(IDiaDataSource), reinterpret_cast<void**>(&dataSource));
	if (FAILED(status))
		return {};

	status = dataSource->loadDataFromPdb(PdbPath.data());
	if (FAILED(status))
		return {};

	CComPtr<IDiaSession> session{};
	status = dataSource->openSession(&session);
	if (FAILED(status))
		return {};

	CComPtr<IDiaSymbol> globalSymbol{};
	status = session->get_globalScope(&globalSymbol);
	if (FAILED(status))
		return {};

	return globalSymbol;
}

symbolic_access::SymbolsMap DiaSymbols::GetSymbols(std::wstring_view PdbPath)
{
	symbolic_access::SymbolsMap result{};
	auto globalSymbol = CreateGlobal(PdbPath);
	if (!globalSymbol)
		return result;

	GetModuleSymbols(result, globalSymbol);
	GetGlobalSymbols(result, globalSymbol);

	return result;
}

symbolic_access::StructsMap DiaSymbols::GetStructs(std::wstring_view PdbPath)
{
	symbolic_access::StructsMap result{};
	auto globalSymbol = CreateGlobal(PdbPath);
	if (!globalSymbol)
		return result;

	GetUDTs(result, globalSymbol);

	return result;
}

void DiaSymbols::GetGlobalSymbols(symbolic_access::SymbolsMap& Symbols, CComPtr<IDiaSymbol>& Global)
{
	for (const auto symTag : { SymTagPublicSymbol, SymTagFunction, SymTagThunk, SymTagData })
	{
		CComPtr<IDiaEnumSymbols> enumSymbols;
		if (FAILED(Global->findChildren(symTag, NULL, nsNone, &enumSymbols)))
			continue;

		IDiaSymbol* tmpPointer{};
		ULONG celt{};
		while (SUCCEEDED(enumSymbols->Next(1, &tmpPointer, &celt)) && celt == 1)
		{
			CComPtr<IDiaSymbol> symbol{ tmpPointer };
			InsertSymbol(Symbols, symbol);
		}
	}
}

void DiaSymbols::GetModuleSymbols(symbolic_access::SymbolsMap& Symbols, CComPtr<IDiaSymbol>& Global)
{
	CComPtr<IDiaEnumSymbols> enumSymbols;
	if (FAILED(Global->findChildren(SymTagCompiland, NULL, nsNone, &enumSymbols)))
		return;

	ULONG celt{};
	IDiaSymbol* tmpPointer{};
	while (SUCCEEDED(enumSymbols->Next(1, reinterpret_cast<IDiaSymbol**>(&tmpPointer), &celt)) && celt == 1)
	{
		for (const auto symTag : { SymTagNull })
		{
			CComPtr<IDiaSymbol> compiland{ tmpPointer };
			CComPtr<IDiaEnumSymbols> pEnumChildren{};
			if (FAILED(compiland->findChildren(symTag, NULL, nsNone, &pEnumChildren)))
				continue;

			ULONG celtChildren{};
			while (SUCCEEDED(pEnumChildren->Next(1, &tmpPointer, &celtChildren)) && celtChildren == 1)
			{
				CComPtr<IDiaSymbol> symbolChildren{ tmpPointer };
				InsertSymbol(Symbols, symbolChildren);
			}
		}
	}
}

void DiaSymbols::InsertSymbol(symbolic_access::SymbolsMap& Symbols, CComPtr<IDiaSymbol>& DiaSymbol)
{
	DWORD symTag{};
	if (FAILED(DiaSymbol->get_symTag(&symTag)) || symTag == SymTagLabel || symTag == SymTagCoffGroup)
		return;

	BSTR wideName{};
	if (FAILED(DiaSymbol->get_name(&wideName)) || !wideName || SkipSymbol(wideName))
		return;

	ULONG offset{};
	if (FAILED(DiaSymbol->get_relativeVirtualAddress(&offset)) || offset == 0)
		return;

	DWORD addressSection{};
	DiaSymbol->get_addressSection(&addressSection);

	std::wstring_view wideNameView{ wideName };
	std::string name{ wideNameView.begin(), wideNameView.end() };

	// msdia may find some symbols in a different order than we find in symbolic access
	// so this will be skipped for now, since I have no idea how to get the same order as msdia
	if (name == "`CFileDiscovery::Search'::`1'::dtor$0")
		return;

	std::string nameCopy = name;
	auto symbolIter = Symbols.find(nameCopy);
	for (size_t i{}; symbolIter != Symbols.end() && symbolIter->second != offset; ++i, symbolIter = Symbols.find(nameCopy))
	{
		std::array<char, 21> buffer{};
		std::to_chars(buffer.data(), buffer.data() + buffer.size(), i);
		nameCopy = name;
		nameCopy += "_";
		nameCopy += buffer.data();
	}

	Symbols.insert(std::make_pair(nameCopy, offset));
}

void DiaSymbols::GetUDTs(symbolic_access::StructsMap& Structs, CComPtr<IDiaSymbol>& Global)
{
	CComPtr<IDiaEnumSymbols> enumSymbols;
	if (FAILED(Global->findChildren(SymTagUDT, NULL, nsNone, &enumSymbols)))
		return;

	IDiaSymbol* tmpPointer{};
	ULONG celt{};
	while (SUCCEEDED(enumSymbols->Next(1, &tmpPointer, &celt)) && celt == 1)
	{
		CComPtr<IDiaSymbol> symbol{ tmpPointer };
		auto structWithMembers = GetStructWithMembers(symbol);
		if (!structWithMembers.has_value())
			continue;

		if (const auto iter = Structs.find(structWithMembers->first); iter != Structs.end() &&
			structWithMembers->second.size() >= iter->second.size())
		{
			iter->second = std::move(structWithMembers->second);
		}
		else
		{
			Structs.insert(std::move(*structWithMembers));
		}

	}
}

std::optional<std::pair<std::string, symbolic_access::StructMembers>> DiaSymbols::GetStructWithMembers(CComPtr<IDiaSymbol>& Symbol)
{
	BSTR wideName{};
	if (FAILED(Symbol->get_name(&wideName)) || !wideName || SkipStruct(wideName))
		return {};

	CComPtr<IDiaEnumSymbols> enumSymbols;
	if (FAILED(Symbol->findChildren(SymTagNull, NULL, nsNone, &enumSymbols)))
		return {};

	auto members = GetStructMembers(enumSymbols);
	if (members.empty())
		return {};

	std::wstring_view wideNameView = wideName;
	std::string name{ wideNameView.begin(), wideNameView.end() };

	return std::make_pair(std::move(name), std::move(members));
}

symbolic_access::StructMembers DiaSymbols::GetStructMembers(CComPtr<IDiaEnumSymbols>& EnumSybols)
{
	symbolic_access::StructMembers members{};

	ULONG celt{};
	IDiaSymbol* tmpPointer{};
	while (SUCCEEDED(EnumSybols->Next(1, &tmpPointer, &celt)) && celt == 1)
	{
		CComPtr<IDiaSymbol> symbol{ tmpPointer };
		DWORD symTag{};
		if (FAILED(symbol->get_symTag(&symTag)) || symTag == SymTagFunction || symTag == SymTagTypedef ||
			symTag == SymTagBaseClass || symTag == SymTagEnum || symTag == SymTagUDT)
		{
			continue;
		}

		DWORD dataKind{};
		if (FAILED(symbol->get_dataKind(&dataKind)) || dataKind == DataIsStaticMember)
			continue;

		BSTR wideName{};
		if (FAILED(symbol->get_name(&wideName)) || !wideName)
			continue;

		LONG offset{};
		if (FAILED(symbol->get_offset(&offset)))
			continue;

		std::wstring_view wideNameView = wideName;
		std::string name{ wideNameView.begin(), wideNameView.end() };
		members.emplace_back(symbolic_access::Member{ std::move(name), static_cast<size_t>(offset), GetBitfieldData(symbol) });
	}

	return members;
}

std::optional<symbolic_access::BitfieldData> DiaSymbols::GetBitfieldData(CComPtr<IDiaSymbol>& Symbol)
{
	ULONG locationType{};
	if (FAILED(Symbol->get_locationType(&locationType)))
		return {};

	std::optional<symbolic_access::BitfieldData> bitfieldData{};
	if (LocIsBitField != locationType)
		return {};

	ULONG bitPosition{};
	if (FAILED(Symbol->get_bitPosition(&bitPosition)))
		return {};

	ULONGLONG len{};
	if (FAILED(Symbol->get_length(&len)))
		return {};

	return symbolic_access::BitfieldData{ static_cast<uint8_t>(bitPosition), static_cast<uint8_t>(len) };
}

bool DiaSymbols::SkipStruct(std::wstring_view StructName)
{
	return StructName.find(L"<anonymous-tag>") != std::wstring_view::npos ||
		StructName.find(L"<unnamed-tag>") != std::wstring_view::npos ||
		StructName.find(L"__unnamed") != std::wstring_view::npos ||
		StructName.find(L"<unnamed-type-u>") != std::wstring_view::npos;
}

bool DiaSymbols::SkipSymbol(std::wstring_view SymbolName)
{
	return SymbolName.empty() ||
		SymbolName.find(L"??_C@") == 0 ||
		SymbolName.find(L"__imp_") == 0 ||
		SymbolName.find(L"NULL_THUNK_DATA") != std::string::npos ||
		SymbolName.find(L"NULL_IMPORT_DESCRIPTOR") != std::string::npos;
}