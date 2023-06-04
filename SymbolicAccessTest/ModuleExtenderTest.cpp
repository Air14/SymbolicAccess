#pragma comment(lib, "ntdll.lib")

#include <gtest/gtest.h>
#include <SymbolicAccess/Phnt/phnt.h>
#include <SymbolicAccess/Pdb/Extractors/StructExtractor.h>
#include <SymbolicAccess/Pdb/Extractors/SymbolExtractor.h>
#include <SymbolicAccess/ModuleExtender/ModuleExtender.h>
#include <SymbolicAccess/ModuleExtender/ModuleExtenderFactory.h>

bool operator==(const UNICODE_STRING First, const UNICODE_STRING Second) 
{
    return std::memcmp(&First, &Second, sizeof(UNICODE_STRING)) == 0;
}

static symbolic_access::ModuleExtender CreateNtdllModuleExtender()
{
    symbolic_access::MsfReader msfReader{ symbolic_access::FileStream(LR"(TestData\)" PDB_ARCH_DIR LR"(\ntdll.pdb)")};
    msfReader.Initialize();

    symbolic_access::SymbolsExtractor symbolsExtractor(msfReader);
    symbolic_access::StructExtractor structsExtractor(msfReader);
    return symbolic_access::ModuleExtender(0, structsExtractor.Extract(), symbolsExtractor.Extract());
}

static symbolic_access::ModuleExtender g_ModuleExtender = CreateNtdllModuleExtender();
TEST(ModuleExtenderTest, ShouldSetAndGetSimpleValuesInsideStructure)
{
    PEB peb{};
    UCHAR beingDebugged = 1;
    ULONG osMajorVersion = 0xfe;

    ASSERT_TRUE(g_ModuleExtender.Set("_PEB", "BeingDebugged", &peb, beingDebugged));
    ASSERT_TRUE(g_ModuleExtender.Set("_PEB", "OSMajorVersion", &peb, osMajorVersion));

    ASSERT_EQ(*g_ModuleExtender.Get<decltype(beingDebugged)>("_PEB", "BeingDebugged", &peb), beingDebugged);
    ASSERT_EQ(*g_ModuleExtender.Get<decltype(osMajorVersion)>("_PEB", "OSMajorVersion", &peb), osMajorVersion);
}

TEST(ModuleExtenderTest, ShouldSetAndGetBitfieldsInsideStructure)
{
    PEB peb{};
    UCHAR imageUsesLargePages = 1;
    ULONG spareTracingBits = 0x10;

    ASSERT_TRUE(g_ModuleExtender.Set("_PEB", "ImageUsesLargePages", &peb, imageUsesLargePages));
    ASSERT_TRUE(g_ModuleExtender.Set("_PEB", "SpareTracingBits", &peb, spareTracingBits));

    ASSERT_EQ(*g_ModuleExtender.Get<decltype(imageUsesLargePages)>("_PEB", "ImageUsesLargePages", &peb), imageUsesLargePages);
    ASSERT_EQ(*g_ModuleExtender.Get<decltype(spareTracingBits)>("_PEB", "SpareTracingBits", &peb), spareTracingBits);
}

TEST(ModuleExtenderTest, ShouldSetAndGetStructuresInsideStructure)
{
    PEB peb{};
    UNICODE_STRING csdVersion{.Length = 0xee, .MaximumLength = 0xff, .Buffer = const_cast<PWCH>(L"Test"), };

    ASSERT_TRUE(g_ModuleExtender.Set("_PEB", "CSDVersion", &peb, csdVersion));

    ASSERT_EQ(*g_ModuleExtender.Get<decltype(csdVersion)>("_PEB", "CSDVersion", &peb), csdVersion);
}

TEST(ModuleExtenderTest, ShouldNotSetBitfieldInsideStructureIfValueExceedsItsSize)
{
    PEB peb{};
    UCHAR imageUsesLargePages = 2;

    ASSERT_TRUE(g_ModuleExtender.Set("_PEB", "ImageUsesLargePages", &peb, imageUsesLargePages));
    ASSERT_EQ(*g_ModuleExtender.Get<decltype(imageUsesLargePages)>("_PEB", "ImageUsesLargePages", &peb), 0);
}

TEST(ModuleExtenderTest, ShouldNotSetGetValuesInsideStructureIfStructOrItsMemberNameDoesNotMatchCase)
{
    PEB peb{};
    UCHAR beingDebugged = 1;

    ASSERT_FALSE(g_ModuleExtender.Set("_PeB", "BeingDebugged", &peb, beingDebugged));
    ASSERT_FALSE(g_ModuleExtender.Set("_PEB", "beingDebugged", &peb, beingDebugged));

    ASSERT_EQ(*g_ModuleExtender.Get<decltype(beingDebugged)>("_PEB", "BeingDebugged", &peb), 0);
}

TEST(ModuleExtenderTest, ShouldGetOffsetToMember)
{
    ASSERT_EQ(g_ModuleExtender.GetOffset("_PEB", "PostProcessInitRoutine"), offsetof(PEB, PostProcessInitRoutine));
}

TEST(ModuleExtenderTest, ShouldGetAndDereferenceAddressOfSymbol)
{
    symbolic_access::SymbolsMap symbolsMap{};

    std::string_view symbolName{ "Test" };
    const auto data = 0xc0febabe;
    const auto symbolOffset = reinterpret_cast<size_t>(&data);
    symbolsMap[symbolName.data()] = symbolOffset;

    symbolic_access::ModuleExtender moduleExtender(0, {}, std::move(symbolsMap));

    ASSERT_EQ(reinterpret_cast<decltype(symbolOffset)>(moduleExtender.GetPointer<void>(symbolName)), symbolOffset);
    ASSERT_EQ(moduleExtender.Get<decltype(data)>(symbolName), data);
}

size_t TestFunction(size_t& Arg1, size_t& Arg2, size_t& Arg3, size_t& Arg4, size_t& Arg5, size_t& Arg6, size_t& Arg7)
{
    Arg1 = 11;
    Arg2 = 12;
    Arg3 = 13;
    Arg4 = 14;
    Arg5 = 15;
    Arg6 = 16;
    Arg7 = 17;

    return Arg1 + Arg2 + Arg3 + Arg4 + Arg5 + Arg6 + Arg7;
}

TEST(ModuleExtenderTest, ShouldCallFunction)
{
    size_t arg1{}, arg2{}, arg3{}, arg4{}, arg5{}, arg6{}, arg7{};
    size_t expecetedArg1{ 11 }, expecetedArg2{ 12 }, expecetedArg3{ 13 }, expecetedArg4{ 14 }, 
        expecetedArg5{ 15 }, expecetedArg6{ 16 }, expecetedArg7{ 17 };

    symbolic_access::SymbolsMap symbolsMap{};
    const auto offset = reinterpret_cast<size_t>(&TestFunction);
    std::string_view functionName{ "TestFunction" };
    symbolsMap[functionName.data()] = offset;

    symbolic_access::ModuleExtender moduleExtender(0, {}, std::move(symbolsMap));

    ASSERT_EQ(*moduleExtender.Call<size_t>(functionName, &arg1, &arg2, &arg3, &arg4, &arg5, &arg6, &arg7),
        expecetedArg1 + expecetedArg2 + expecetedArg3 + expecetedArg4 + expecetedArg5 + expecetedArg6 + expecetedArg7);
    ASSERT_EQ(arg1, expecetedArg1);
    ASSERT_EQ(arg2, expecetedArg2);
    ASSERT_EQ(arg3, expecetedArg3);
    ASSERT_EQ(arg4, expecetedArg4);
    ASSERT_EQ(arg5, expecetedArg5);
    ASSERT_EQ(arg6, expecetedArg6);
    ASSERT_EQ(arg7, expecetedArg7);
}

TEST(ModuleExtenderTest, ShouldNotGetSymbolAddressIfItsNameIsInWrongCase)
{
    symbolic_access::SymbolsMap symbolsMap{};
    symbolsMap["Test"] = 0xc0febabe;

    symbolic_access::ModuleExtender moduleExtender(0, {}, std::move(symbolsMap));

    ASSERT_EQ(reinterpret_cast<size_t>(moduleExtender.GetPointer<void>("test")), 0);
}
