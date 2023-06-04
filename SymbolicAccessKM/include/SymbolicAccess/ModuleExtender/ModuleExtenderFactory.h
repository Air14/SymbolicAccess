#pragma once
#include "ModuleExtender.h"
#include <SymbolicAccess/Pdb/PdbGrabber.h>

namespace symbolic_access
{
	class ModuleExtenderFactory
	{
	public:
		ModuleExtenderFactory();
		ModuleExtenderFactory(const ModuleExtenderFactory&) = delete;
		ModuleExtenderFactory& operator=(const ModuleExtenderFactory&) = delete;
		ModuleExtenderFactory(ModuleExtenderFactory&&) = default;
		ModuleExtenderFactory& operator=(ModuleExtenderFactory&&) = default;

		std::optional<ModuleExtender> Create(std::wstring_view ModuleName);
	private:
		PdbGrabber m_PdbGrabber;
	};
}