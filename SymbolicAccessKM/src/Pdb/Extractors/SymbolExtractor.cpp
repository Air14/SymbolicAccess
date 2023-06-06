#include <SymbolicAccess/Pdb/Extractors/SymbolExtractor.h>
#include <algorithm>

namespace symbolic_access
{
	SymbolsExtractor::SymbolsExtractor(MsfReader& MsfReader) : m_MsfReader(MsfReader)
	{
	}

	SymbolsMap SymbolsExtractor::Extract()
	{
		const auto debugInfoStream = m_MsfReader.GetStream(MsfReader::KnownStreams::DebugInfo);
		if (debugInfoStream.empty())
			return {};

		StreamReader reader(debugInfoStream);

		const auto dbiHeader = reader.Read<DbiHeader>();
		const auto& modules = GetModulesInfo(reader, dbiHeader.ModuleSize);

		reader.Skip(dbiHeader.SecConSize + dbiHeader.SecMapSize + dbiHeader.FileInfoSize + dbiHeader.SrcModuleSize + dbiHeader.EcInfoSize);

		const auto dbiDebugHeader = reader.Read<DbiDebugHeader>();
		m_OriginalSectionHeaders = ReadSectionHeaders(dbiDebugHeader.SectionHdrOriginal);
		m_SectionHeaders = ReadSectionHeaders(dbiDebugHeader.SectionHdr);
		if (m_SectionHeaders.empty())
			return {};
		
		ReadOmaps(dbiDebugHeader.OmapFromSource, dbiDebugHeader.OmapToSource);

		SymbolsMap symbols{};
		for (const auto& module : modules)
			if (!GetModuleSymbols(module, symbols))
				return {};

		if (!GetGlobalSymbols(dbiHeader.SymRecordStream, symbols))
			return {};

		return symbols;
	}

	internal::vector<SymbolsExtractor::IMAGE_SECTION_HEADER> SymbolsExtractor::ReadSectionHeaders(uint32_t HeaderStream)
	{
		internal::vector<SymbolsExtractor::IMAGE_SECTION_HEADER> sectionsHeaders{};
		if (HeaderStream == 0xffff)
			return sectionsHeaders;

		const auto& headerStream = m_MsfReader.GetStream(HeaderStream);
		if (headerStream.empty())
			return sectionsHeaders;

		StreamReader reader(headerStream);
		while (reader.Tellg() < headerStream.size())
			sectionsHeaders.emplace_back(reader.Read<IMAGE_SECTION_HEADER>());

		return sectionsHeaders;
	}

	internal::vector<DbiModuleInfo> SymbolsExtractor::GetModulesInfo(StreamReader& Reader, size_t ModulesSize)
	{
		internal::vector<DbiModuleInfo> modules{};
		for (const auto endOffsetOfModules = Reader.Tellg() + ModulesSize; Reader.Tellg() < endOffsetOfModules;)
		{
			const auto dbInfo = Reader.Read<DbiModuleInfo>();
			Reader.ReadString(); // read module name
			Reader.ReadString(); // read object name

			if (dbInfo.Stream != 0xffff)
				modules.emplace_back(dbInfo);

			Reader.Align(4);
		}
		return modules;
	}

	bool SymbolsExtractor::GetGlobalSymbols(uint16_t SymbolRecordStreamIndex, SymbolsMap& Symbols)
	{
		const auto& symbolRecordStream = m_MsfReader.GetStream(SymbolRecordStreamIndex);
		if (symbolRecordStream.empty())
			return false;

		StreamReader reader(symbolRecordStream);
		while (reader.Tellg() < symbolRecordStream.size())
			GetSymbol(reader, Symbols);

		return true;
	}

	bool SymbolsExtractor::GetModuleSymbols(const DbiModuleInfo& ModuleInfo, SymbolsMap& Symbols)
	{
		const auto& moduleStream = m_MsfReader.GetStream(ModuleInfo.Stream);
		if (moduleStream.empty())
			return false;

		StreamReader reader(moduleStream);
		if (reader.Read<CvSignature>() != CvSignature::C13)
			return false;

		const auto end = static_cast<uint32_t>(ModuleInfo.CbSyms);
		while (reader.Tellg() < end)
			GetSymbol(reader, Symbols);

		return true;
	}

	void SymbolsExtractor::GetSymbol(StreamReader& Reader, SymbolsMap& Symbols)
	{
		const auto symbolSize = Reader.Read<uint16_t>();
		const auto symbolType = Reader.Read<SymbolDefs>();
		const auto offsetBeg = Reader.Tellg() - sizeof(uint16_t);

		switch (symbolType)
		{
		case SymbolDefs::S_LPROC32:
		case SymbolDefs::S_LPROC32_ST:
		case SymbolDefs::S_LPROC32_ID:
		case SymbolDefs::S_GPROC32:
		case SymbolDefs::S_GPROC32_ST:
		case SymbolDefs::S_GPROC32_ID:
		case SymbolDefs::S_LPROC32_DPC:
		case SymbolDefs::S_LPROC32_DPC_ID:
			InsertSymbol<ProcSym32>(Reader, Symbols);
			break;
		case SymbolDefs::S_THUNK32:
		case SymbolDefs::S_THUNK32_ST:
			InsertSymbol<ThunkSym32>(Reader, Symbols);
			break;
		case SymbolDefs::S_PUB32:
		case SymbolDefs::S_PUB32_ST:
			InsertSymbol<PubSym32>(Reader, Symbols);
			break;
		case SymbolDefs::S_LDATA32:
		case SymbolDefs::S_LDATA32_ST:
		case SymbolDefs::S_GDATA32:
		case SymbolDefs::S_GDATA32_ST:
		case SymbolDefs::S_LMANDATA:
		case SymbolDefs::S_LMANDATA_ST:
		case SymbolDefs::S_GMANDATA:
		case SymbolDefs::S_GMANDATA_ST:
			InsertSymbol<DataSym32>(Reader, Symbols);
			break;
		}

		Reader.Seek(offsetBeg + symbolSize);
	}

	bool SymbolsExtractor::SkipSymbol(std::string_view SymbolName)
	{
		return SymbolName.empty() || 
			SymbolName.find("??_C@") == 0 || 
			SymbolName.find("__imp_") == 0 || 
			SymbolName.find("NULL_THUNK_DATA") != std::string_view::npos || 
			SymbolName.find("NULL_IMPORT_DESCRIPTOR") != std::string_view::npos;
	}

	void SymbolsExtractor::ReadOmaps(uint16_t OmapFromSourceStreamIndex, uint16_t OmapToSourceStreamIndex)
	{
		if (!OmapToSourceStreamIndex || OmapToSourceStreamIndex == 0xffff ||
			!OmapFromSourceStreamIndex || OmapFromSourceStreamIndex == 0xffff)
		{
			return;
		}

		// Both OMAPs cannot be combined into a single vector, as this can cause bad translation of offsets
		// FromSource
		{
			const auto& omapFromSourceStream = m_MsfReader.GetStream(OmapFromSourceStreamIndex);
			if (omapFromSourceStream.empty())
				return;

			StreamReader reader(omapFromSourceStream);
			for (size_t i{}; i < omapFromSourceStream.size() / sizeof(OMAP); ++i)
				m_FirstOmaps.push_back(reader.Read<OMAP>());
		}

		// ToSource
		{
			const auto& omapToSourceStream = m_MsfReader.GetStream(OmapToSourceStreamIndex);
			if (omapToSourceStream.empty())
				return;

			StreamReader reader(omapToSourceStream);
			for (size_t i{}; i < omapToSourceStream.size() / sizeof(OMAP); ++i)
			{
				auto omap = reader.Read<OMAP>();
				std::swap(omap.SourceAddress, omap.TargetAddress);
				m_SecondOmaps.push_back(omap);
			}

			std::sort(m_SecondOmaps.begin(), m_SecondOmaps.end());
		}
	}

	std::optional<size_t> SymbolsExtractor::GetSymbolsOffset(size_t Offset, uint16_t Section)
	{
		if (!Section || Section > m_SectionHeaders.size())
			return {};

		const auto& section = m_OriginalSectionHeaders.size() ?
			m_OriginalSectionHeaders[Section - 1] :
			m_SectionHeaders[Section - 1];

		if (Offset >= section.VirtualAddress + section.Misc.VirtualSize)
			return {};

		const auto offset = Offset + section.VirtualAddress;
		if (m_FirstOmaps.size() && m_SecondOmaps.size())
		{
			for (const auto& omaps : { m_FirstOmaps, m_SecondOmaps })
			{
				if (const auto translatedOffset = TranslateOffset(offset, omaps); translatedOffset.has_value())
					return translatedOffset;
			}

			return std::optional<size_t>{};
		}

		return offset;
	}

	std::optional<size_t> SymbolsExtractor::TranslateOffset(size_t Offset, const internal::vector<OMAP>& Omaps)
	{
		if (auto omapIter = std::find_if(Omaps.begin(), Omaps.end(), [&](OMAP Omap) {return Omap.SourceAddress > Offset; });
			omapIter != Omaps.end())
		{
			if (omapIter != Omaps.begin())
				--omapIter;

			return omapIter->TargetAddress ?
				Offset - omapIter->SourceAddress + omapIter->TargetAddress :
				std::optional<size_t>{};
		}

		return std::optional<size_t>{};
	}
}