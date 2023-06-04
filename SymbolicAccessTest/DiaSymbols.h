#pragma once
#include <SymbolicAccess/ModuleExtender/ModuleData.h>
#include <string_view>
#include <dia2.h>
#include <atlbase.h>

class DiaSymbols
{
public:
	symbolic_access::SymbolsMap GetSymbols(std::wstring_view PdbPath);

	symbolic_access::StructsMap GetStructs(std::wstring_view PdbPath);

private:
	CComPtr<IDiaSymbol> CreateGlobal(std::wstring_view PdbPath);

	void GetGlobalSymbols(symbolic_access::SymbolsMap& Symbols, CComPtr<IDiaSymbol>& Global);

	void GetModuleSymbols(symbolic_access::SymbolsMap& Symbols, CComPtr<IDiaSymbol>& Global);

	void InsertSymbol(symbolic_access::SymbolsMap& Symbols, CComPtr<IDiaSymbol>& DiaSymbol);

	void GetUDTs(symbolic_access::StructsMap& Structs, CComPtr<IDiaSymbol>& Global);

	std::optional<std::pair<std::string, symbolic_access::StructMembers>> GetStructWithMembers(CComPtr<IDiaSymbol>& Symbol);

	symbolic_access::StructMembers GetStructMembers(CComPtr<IDiaEnumSymbols>& EnumSybols);

	std::optional<symbolic_access::BitfieldData> GetBitfieldData(CComPtr<IDiaSymbol>& Symbol);

	bool SkipStruct(std::wstring_view StructName);

	bool SkipSymbol(std::wstring_view SymbolName);
};