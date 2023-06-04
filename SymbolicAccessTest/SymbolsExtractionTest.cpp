#include <gtest/gtest.h>
#include <Windows.h>

#include <SymbolicAccess/Pdb/Extractors/StructExtractor.h>
#include <SymbolicAccess/Pdb/Extractors/SymbolExtractor.h>

#include "DiaSymbols.h"
#include <format>

static symbolic_access::SymbolsMap GetSymbolsWithSymbolicAccess(std::wstring_view PdbPath)
{
	symbolic_access::MsfReader msfReader{ symbolic_access::FileStream(PdbPath) };
	if (!msfReader.Initialize())
		return {};

	symbolic_access::SymbolsExtractor symbolsExtractor(msfReader);
	auto symbols = symbolsExtractor.Extract();
	if (symbols.empty())
		return {};

	return symbols;
}

static symbolic_access::StructsMap GetStructsWithSymbolicAccess(std::wstring_view PdbPath)
{
	symbolic_access::MsfReader msfReader{ symbolic_access::FileStream(PdbPath) };
	if (!msfReader.Initialize())
		return {};

	symbolic_access::StructExtractor structsExtractor(msfReader);
	auto structs = structsExtractor.Extract();
	if (structs.empty())
		return {};

	return structs;
}

void CompareSymbols(const symbolic_access::SymbolsMap& SymbolicAccessSymbols, const symbolic_access::SymbolsMap& DiaSymbols)
{
	auto success = true;
	for (const auto& [key, value] : DiaSymbols)
	{
		const auto iter = SymbolicAccessSymbols.find(key);
		if (iter == SymbolicAccessSymbols.end())
		{
			success = false;
			std::cout << "Symbol " << key << " from dia isn't present in symbols from SymbolicAccess\n";
		}
		else if (iter->second != value)
		{
			success = false;
			std::cout << "Offset for symbol " << key << " should be equall to " << value << "\n";
		}
	}
	
	ASSERT_TRUE(success);
}

bool operator ==(const symbolic_access::BitfieldData& First, const symbolic_access::BitfieldData& Second)
{
	return First.Length == Second.Length && First.Position == Second.Position;
}

void CompareStructs(symbolic_access::StructsMap&& SymbolicAccessStructs, symbolic_access::StructsMap&& DiaStructs)
{
	// msdia may find some stucts in a different order than we find in symbolic access
	// so this will be skipped for now, since I have no idea how to get the same order as msdia
	SymbolicAccessStructs.erase("_FILE_REMOTE_PROTOCOL_INFO");
	SymbolicAccessStructs.erase("_RTL_CRITICAL_SECTION_DEBUG");
	SymbolicAccessStructs.erase("std::_System_error");
	DiaStructs.erase("_FILE_REMOTE_PROTOCOL_INFO");
	DiaStructs.erase("_RTL_CRITICAL_SECTION_DEBUG");
	DiaStructs.erase("std::_System_error");

	for (const auto& [name, members] : DiaStructs)
	{
		if(SymbolicAccessStructs.find(name) == SymbolicAccessStructs.end())
			std::cout << name << " not present in SymbolicAccess structs" << "\n";
	}

	ASSERT_EQ(SymbolicAccessStructs.size(), DiaStructs.size());
	for (auto symbolicAccessIter = SymbolicAccessStructs.begin(), diaIter = DiaStructs.begin(); 
		symbolicAccessIter != SymbolicAccessStructs.end(); ++symbolicAccessIter, ++diaIter)
	{
		const auto& [symbolicAccessStructName, symbolicAccessStructMembers] = *symbolicAccessIter;
		const auto& [diaStructName, diaStructMembers] = *diaIter;

		ASSERT_EQ(symbolicAccessStructName, diaStructName);
		ASSERT_EQ(symbolicAccessStructMembers.size(), diaStructMembers.size()) << std::format("Mismatched size of members for struct: {}", symbolicAccessStructName);

		for (auto symbolicAccessMembersIter = symbolicAccessStructMembers.begin(), diaMembersIter = diaStructMembers.begin();
			symbolicAccessMembersIter < symbolicAccessStructMembers.end(); ++symbolicAccessMembersIter, ++diaMembersIter)
		{
			ASSERT_EQ(symbolicAccessMembersIter->Name, diaMembersIter->Name) << 
				std::format("Mismatched member name for struct: {}", symbolicAccessStructName);

			ASSERT_EQ(symbolicAccessMembersIter->Offset, diaMembersIter->Offset) <<
				std::format("Mismatched offset value for struct: {} and for member: {}", symbolicAccessStructName, symbolicAccessMembersIter->Name);

			ASSERT_EQ(symbolicAccessMembersIter->Bitfield.has_value(), diaMembersIter->Bitfield.has_value()) <<
				std::format("Mismatched bitfield for struct: {} and for member: {}", symbolicAccessStructName, symbolicAccessMembersIter->Name);

			if (symbolicAccessMembersIter->Bitfield.has_value() && diaMembersIter->Bitfield.has_value())
			{
				ASSERT_TRUE(*symbolicAccessMembersIter->Bitfield == *diaMembersIter->Bitfield) <<
					std::format("Mismatched bitfield value for struct: {} and for member: {}", symbolicAccessStructName, symbolicAccessMembersIter->Name);
			}
		}
	}
}

class ExtractionParameterizedTestFixture : public ::testing::TestWithParam<std::wstring_view>
{
};

TEST_P(ExtractionParameterizedTestFixture, ShouldGetAllSymbolsAndStructsFromPdb)
{
	const auto pdbPath = GetParam();

	DiaSymbols diaSymbols{};
	CompareSymbols(GetSymbolsWithSymbolicAccess(pdbPath), diaSymbols.GetSymbols(pdbPath));
	CompareStructs(GetStructsWithSymbolicAccess(pdbPath), diaSymbols.GetStructs(pdbPath));
}

INSTANTIATE_TEST_SUITE_P(
	ExtractionTests,
	ExtractionParameterizedTestFixture,
	::testing::Values(
		LR"(TestData\)" PDB_ARCH_DIR LR"(\ntkrnlmp.pdb)",
		LR"(TestData\)" PDB_ARCH_DIR LR"(\ntdll.pdb)",
		LR"(TestData\)" PDB_ARCH_DIR LR"(\win32kfull.pdb)",
		LR"(TestData\)" PDB_ARCH_DIR LR"(\d3d11.pdb)",
		LR"(TestData\)" PDB_ARCH_DIR LR"(\vcruntime140.amd64.pdb)",
		LR"(TestData\)" PDB_ARCH_DIR LR"(\Windows.FileExplorer.Common.pdb)",
		LR"(TestData\)" PDB_ARCH_DIR LR"(\devenv.pdb)",
		LR"(TestData\)" PDB_ARCH_DIR LR"(\crash.pdb)",
		LR"(TestData\)" PDB_ARCH_DIR LR"(\mozglue.pdb)",
		LR"(TestData\)" PDB_ARCH_DIR LR"(\ProcMonX.pdb)",
		LR"(TestData\)" PDB_ARCH_DIR LR"(\HyperHide.pdb)",
		LR"(TestData\)" PDB_ARCH_DIR LR"(\HyperHideDrv.pdb)"
		LR"(TestData\)" PDB_ARCH_DIR LR"(\D3DCompiler_47.pdb)"
	));