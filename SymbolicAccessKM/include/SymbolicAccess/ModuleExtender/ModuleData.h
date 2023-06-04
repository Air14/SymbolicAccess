#pragma once
#include <utility>
#include <optional>
#include <SymbolicAccess/Internal/map.h>
#include <SymbolicAccess/Internal/string.h>
#include <SymbolicAccess/Internal/vector.h>

namespace symbolic_access
{
	struct BitfieldData
	{
		uint8_t Position;
		uint8_t Length;
	};

	struct Member
	{
		internal::string Name;
		size_t Offset;
		std::optional<BitfieldData> Bitfield;
	};

	using StructMembers = internal::vector<Member>;
	using StructsMap = internal::map<internal::string, StructMembers, std::less<>>;
	using SymbolsMap = internal::map<internal::string, size_t, std::less<>>;
}