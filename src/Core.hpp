#pragma once

namespace SCRIBE
{
	constexpr uint32_t INI_VERSION = 3;

	void VerifyConfiguration();
	void GenerateDynamicScrolls();
	void LoadFused();
	void PatchVanillaScrolls();
	void PerformCleanup();
	void PatchSoulGemFormList();
	void PerformIniMigrations();
	void LoadFormIDOffset();
	
	bool			BindPapyrusFunctions(RE::BSScript::IVirtualMachine*);
	RE::ScrollItem* FuseAndCreateFunc(RE::ScrollItem* scrollOne, RE::ScrollItem* scrollTwo);
	void			GenerateFusedConcSpell(RE::ScrollItem* scrollObj);
	RE::ScrollItem* FuseAndCreate(RE::StaticFunctionTag*, RE::ScrollItem*, RE::ScrollItem*);
	bool			CanFuse(RE::StaticFunctionTag*, RE::ScrollItem*, RE::ScrollItem*, bool);
	RE::ScrollItem* GetScrollForBook(RE::StaticFunctionTag*, RE::TESObjectBOOK*);
	RE::SpellItem*	GetSpellFromScroll(RE::StaticFunctionTag*, RE::ScrollItem*);
	void			OverrideSpell(RE::StaticFunctionTag*, RE::SpellItem*, RE::SpellItem*);
	RE::SpellItem*	GetZeroCostCopy(RE::StaticFunctionTag*, RE::SpellItem*);
	int				GetApproxFullGoldValue(RE::StaticFunctionTag*, RE::TESForm*);
	RE::SpellItem*	GetUpgradedSpell(RE::StaticFunctionTag*, RE::SpellItem* spell);
	RE::ScrollItem* GetScrollFromSpell(RE::StaticFunctionTag*, RE::SpellItem* spell);
}
