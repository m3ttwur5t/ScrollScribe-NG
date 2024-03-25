#pragma once

#include "Bimap.h"
#include "SimpleIni.h"

namespace SCRIBE
{

	namespace UTIL
	{
		RE::FormID lexical_cast_formid(const std::string& hex_string);
		bool IsConcentrationSpell(RE::SpellItem* theSpell);
		int GetSpellRank(RE::SpellItem* theSpell);
		const int GetSpellLevelApprox(RE::SpellItem* const& theSpell);
		RE::TESGlobal* GetFilterGlobalForSpell(RE::SpellItem* theSpell);
		void AddTierKeywords(RE::ScrollItem* scrollObj, RE::SpellItem* theSpell);
		void AddDisintegrateEffect(RE::ScrollItem* scrollObj);
		void AddRankKeywords(RE::ScrollItem* scrollObj, RE::SpellItem* theSpell);

		std::size_t GetEffectListHash(RE::BSTArray<RE::Effect*> effList);
		std::size_t GetNameHash(std::string name);

		std::string ExtractSpellName(const std::string& inputString);

		struct CobjGenerationArgs
		{
			RE::ScrollItem* theScroll;
			RE::SpellItem* theSpell;
			const int baseDust;
			const int reducedDust;
		};
		std::vector<RE::BGSConstructibleObject*> GetConstructibleObjectForScroll(CobjGenerationArgs args);
	}

	namespace CONFIG
	{
		class Plugin
		{
		private:
			const std::string iniPath = "Data/SKSE/Plugins/ScrollScribeNG.ini";
			CSimpleIniA Ini;

			Plugin()
			{
				Ini.SetUnicode();
				Ini.LoadFile(iniPath.c_str());
			}

		public:
			static Plugin& GetSingleton()
			{
				static Plugin instance;
				return instance;
			}

			bool HasKey(const std::string& section, const std::string& key)
			{
				return Ini.KeyExists(section.c_str(), key.c_str());
			}

			bool HasSection(const std::string& section)
			{
				return Ini.SectionExists(section.c_str());
			}

			const std::vector<std::pair<std::string, std::string>> GetAllKeyValuePairs(const std::string& section) const {
				std::vector<std::pair<std::string, std::string>> ret;
				CSimpleIniA::TNamesDepend keys;
				Ini.GetAllKeys(section.c_str(), keys);
				for (auto& key : keys) {
					auto val = Ini.GetValue(section.c_str(), key.pItem);
					ret.push_back({ key.pItem, val });
				}
				
				return ret;
			}

			void DeleteSection(const std::string& section)
			{
				Ini.Delete(section.c_str(), nullptr, true);
			}

			void DeleteKey(const std::string& section, const std::string& key) {
				Ini.Delete(section.c_str(), key.c_str());
			}

			std::string GetValue(const std::string& section, const std::string& key)
			{
				return Ini.GetValue(section.c_str(), key.c_str());
			}

			long GetLongValue(const std::string& section, const std::string& key)
			{
				return Ini.GetLongValue(section.c_str(), key.c_str());
			}

			bool GetBoolValue(const std::string& section, const std::string& key)
			{
				return Ini.GetBoolValue(section.c_str(), key.c_str());
			}

			void SetValue(const std::string& section, const std::string& key, const std::string& value, const std::string& comment = std::string())
			{
				Ini.SetValue(section.c_str(), key.c_str(), value.c_str(), comment.length() > 0 ? comment.c_str() : (const char*)0);
			}

			void SetBoolValue(const std::string& section, const std::string& key, const bool value, const std::string& comment = std::string())
			{
				Ini.SetBoolValue(section.c_str(), key.c_str(), value, comment.length() > 0 ? comment.c_str() : (const char*)0);
			}

			void SetLongValue(const std::string& section, const std::string& key, const long value, const std::string& comment = std::string())
			{
				Ini.SetLongValue(section.c_str(), key.c_str(), value, comment.length() > 0 ? comment.c_str() : (const char*)0);
			}

			void Save()
			{
				Ini.SaveFile(iniPath.c_str());
			}

			Plugin(Plugin const&) = delete;
			void operator=(Plugin const&) = delete;
		};
	}

	namespace CACHE
	{
		inline BiMap<RE::TESObjectBOOK*, RE::SpellItem*> BookSpellBiMap;
		inline BiMap<RE::SpellItem*, RE::ScrollItem*> SpellScrollBiMap;
		inline BiMap<RE::FormID, RE::FormID> FormIDRelocationBiMap;
		inline BiMap<std::pair<RE::ScrollItem*, RE::ScrollItem*>, RE::ScrollItem*> FusionComponentsToResultBiMap;
		inline std::map<RE::SpellItem*, std::vector<RE::BGSConstructibleObject*>> SpellScrollCobjMap;
		inline std::map<RE::ScrollItem*, RE::SpellItem*> FusionSpellMap;
		inline std::map<RE::BGSKeyword*, std::vector<RE::SpellItem*>> KeywordSpellListMap;
		inline std::map<RE::SpellItem*, RE::SpellItem*> ZeroCostMap;
		inline std::map<ULONG64, RE::SpellItem*> HashToSpellMap;

		void AddNameAndEffectHashedSpell(RE::SpellItem* theSpell);
		void AddKeywordSpellCache(RE::SpellItem* theSpell);
	}

	class FORMS
	{
	public:
		// Static member function to access the singleton instance
		static FORMS& GetSingleton()
		{
			static FORMS instance;  // Guaranteed to be initialized only once
			return instance;
		}

		static const RE::FormID FORMID_OFFSET_BASE = 0xFF070000;
		RE::FormID CurrentOffset;
		bool UseOffset;

		void SetOffset(RE::FormID offset) {
			CurrentOffset = offset;
		}
		void SetUseOffset(bool use) {
			UseOffset = use;
		}
		bool GetUseOffset() const
		{
			return UseOffset;
		}
		RE::FormID NextFormID() const
		{
			static RE::FormID current = 0;
			return FORMID_OFFSET_BASE + (++current) + CurrentOffset;
		}

		RE::TESObjectMISC* MiscPaperRoll = RE::TESForm::LookupByID<RE::TESObjectMISC>(0x33761);
		RE::TESObjectMISC* MiscArcaneDust = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectMISC>(0x804, "Scribe.esp"sv);

		RE::BGSKeyword* KywdVendorItemScroll = RE::TESForm::LookupByID<RE::BGSKeyword>(0xa0e57);                                                 // VendorItemScroll
		RE::BGSKeyword* KywdScrollEnchantingStation = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x80E, "Scribe.esp"sv);     // _scrScrollEnchantingStation
		RE::BGSKeyword* KywdScrollCustom = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x801, "Scribe.esp"sv);                // _scrKeywordScrollCustom
		RE::BGSKeyword* KywdKeywordScrollConcentration = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x843, "Scribe.esp"sv);  // _scrKeywordScrollConcentration

		RE::BGSKeyword* KywdScrollAlteration = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x84F, "Scribe.esp"sv);   // _scrSchoolAlteration
		RE::BGSKeyword* KywdScrollConjuration = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x850, "Scribe.esp"sv);  // _scrSchoolConjuration
		RE::BGSKeyword* KywdScrollDestruction = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x851, "Scribe.esp"sv);  // _scrSchoolDestruction
		RE::BGSKeyword* KywdScrollIllusion = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x852, "Scribe.esp"sv);     // _scrSchoolIllusion
		RE::BGSKeyword* KywdScrollRestoration = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x853, "Scribe.esp"sv);  // _scrSchoolRestoration
		RE::BGSKeyword* KywdScrollNovice = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x807, "Scribe.esp"sv);       // _scrNoviceSpell
		RE::BGSKeyword* KywdScrollApprentice = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x808, "Scribe.esp"sv);   // _scrApprenticeSpell
		RE::BGSKeyword* KywdScrollAdept = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x809, "Scribe.esp"sv);        // _scrAdeptSpell
		RE::BGSKeyword* KywdScrollExpert = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x80A, "Scribe.esp"sv);       // _scrExpertSpell
		RE::BGSKeyword* KywdScrollMaster = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x80B, "Scribe.esp"sv);       // _scrMasterSpell
		RE::BGSKeyword* KywdScrollStrange = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x81D, "Scribe.esp"sv);      // _scrStrangeSpell

		RE::BGSKeyword* KywdFused = RE::TESDataHandler::GetSingleton() -> LookupForm<RE::BGSKeyword>(0x82C, "Scribe.esp"sv);        // _scrKeywordScrollFused
		RE::BGSKeyword* KywdDoubleFused = RE::TESDataHandler::GetSingleton() -> LookupForm<RE::BGSKeyword>(0x840, "Scribe.esp"sv);  // _scrKeywordScrollFusedTWICE

		RE::SpellItem* SpelDisintegrateEffectTemplate = RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(0x849, "Scribe.esp"sv);  // _scrDustDisintegrateAbility

		RE::TESGlobal* GlobFilterNovice = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x818, "Scribe.esp"sv);      // _scrCraftingFilterNovice
		RE::TESGlobal* GlobFilterApprentice = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x819, "Scribe.esp"sv);  // _scrCraftingFilterApprentice
		RE::TESGlobal* GlobFilterAdept = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x81A, "Scribe.esp"sv);       // _scrCraftingFilterAdept
		RE::TESGlobal* GlobFilterExpert = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x81B, "Scribe.esp"sv);      // _scrCraftingFilterExpert
		RE::TESGlobal* GlobFilterMaster = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x81C, "Scribe.esp"sv);      // _scrCraftingFilterMaster
		RE::TESGlobal* GlobFilterStrange = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x81E, "Scribe.esp"sv);     // _scrCraftingFilterStrange
		RE::TESGlobal* GlobFilterKnown = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x835, "Scribe.esp"sv);       // _scrCraftingFilterKnown
		RE::TESGlobal* GlobScribeLevel = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x800, "Scribe.esp"sv);       // _scrInscriptionLevel

		RE::BGSPerk* PerkDustDiscount = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSPerk>(0x82D, "Scribe.esp"sv);  // Dust Discount
		RE::BGSEquipSlot* EquipSlotEither = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13F44);                              // EitherHand

	private:
		// Private constructor to prevent instantiation
		FORMS() { }

		// Delete copy constructor and assignment operator to prevent cloning
		FORMS(const FORMS&) = delete;
		FORMS& operator=(const FORMS&) = delete;
	};

}
