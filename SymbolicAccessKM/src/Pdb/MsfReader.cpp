#include <SymbolicAccess/Pdb/MsfReader.h>
#include <algorithm>

namespace symbolic_access
{
	MsfReader::MsfReader(FileStream FileStream) : m_FileStream(std::move(FileStream)), m_PageSize(0)
	{
	}

	bool MsfReader::Initialize()
	{
		MsfHeader msfHeader{};
		if (!m_FileStream.Read(&msfHeader, sizeof(msfHeader)) || msfHeader.Signature != m_Signature)
			return false;

		m_PageSize = msfHeader.PageSize;

		const auto numberOfRootPages = GetNumberOfPages(msfHeader.DirectorySize);
		if (!numberOfRootPages)
			return false;

		const auto numberOfRootIndexPages = GetNumberOfPages(numberOfRootPages * sizeof(uint32_t));
		if (!numberOfRootIndexPages)
			return false;

		internal::vector<uint32_t> rootPages(numberOfRootPages);
		internal::vector<uint32_t> rootIndexPages(numberOfRootIndexPages);

		if (!m_FileStream.Read(rootIndexPages.data(), numberOfRootIndexPages * sizeof(uint32_t)) || HasNullOffsets(rootIndexPages))
			return false;

		for (uint32_t i{}, k{}, len{}; i < numberOfRootIndexPages; ++i, k += len)
		{
			len = std::min(m_PageSize / 4, numberOfRootPages - k);

			m_FileStream.Seekg(rootIndexPages[i] * m_PageSize);
			if (!m_FileStream.Read(&rootPages[k], len * 4))
				return false;
		}

		if (HasNullOffsets(rootPages))
			return false;

		uint32_t numberOfStreams{};
		m_FileStream.Seekg(rootPages[0] * m_PageSize);
		if (!m_FileStream.Read(&numberOfStreams, sizeof(numberOfStreams)))
			return false;
		m_Streams.reserve(numberOfStreams);

		uint32_t currentRootPage{};
		for (uint32_t i{}; i < numberOfRootPages; ++i)
		{
			if(i)
				m_FileStream.Seekg(rootPages[i] * m_PageSize);

			for (uint32_t k = i == 0; numberOfStreams > 0 && k < m_PageSize / 4; ++k, --numberOfStreams)
			{
				uint32_t size{};
				if (!m_FileStream.Read(&size, sizeof(size)))
					return false;

				if (size == 0xFFFFFFFF)
					size = 0;

				m_Streams.emplace_back(ContentStream{ size });
			}

			if (!numberOfStreams)
			{
				currentRootPage = i;
				break;
			}
		}

		for (auto& stream : m_Streams)
		{
			const auto numberOfPages = GetNumberOfPages(stream.Size);
			if (!numberOfPages)
				continue;

			stream.PageIndices.resize(numberOfPages);

			for (auto remainingPages = numberOfPages; remainingPages > 0;)
			{
				const auto pageOffset = static_cast<uint32_t>(m_FileStream.Tellg()) % m_PageSize;
				const auto pageSize = std::min(remainingPages * 4, m_PageSize - pageOffset);

				if (!m_FileStream.Read(stream.PageIndices.data() + numberOfPages - remainingPages, pageSize))
					return false;

				remainingPages -= pageSize / 4;

				if (pageOffset + pageSize == m_PageSize)
					m_FileStream.Seekg(rootPages[++currentRootPage] * m_PageSize);
			}
		}

		return true;
	}

	internal::vector<char> MsfReader::GetStream(size_t Index)
	{
		const auto& stream = m_Streams[Index];
		internal::vector<char> streamData(stream.PageIndices.size() * m_PageSize);

		size_t offset{};
		for (const auto pageIndex : stream.PageIndices)
		{
			m_FileStream.Seekg(pageIndex * m_PageSize);
			m_FileStream.Read(streamData.data() + offset, m_PageSize);

			offset += m_PageSize;
		}

		streamData.resize(stream.Size);
		return streamData;
	}

	uint32_t MsfReader::GetNumberOfPages(uint32_t Size)
	{
		const auto numberOfPages = Size / m_PageSize;
		return Size % m_PageSize ? numberOfPages + 1 : numberOfPages;
	}

	bool MsfReader::HasNullOffsets(const internal::vector<uint32_t>& Offsets)
	{
		return std::any_of(Offsets.begin(), Offsets.end(), [](uint32_t Offset) { return Offset == 0; });
	}
}