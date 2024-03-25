#include "Util.h"

namespace SCRIBE
{
	namespace UTIL
	{
		RE::FormID lexical_cast_formid(const std::string& hex_string)
		{
			if (hex_string.substr(0, 2) != "0x") {
				throw std::invalid_argument("Input string is not a valid hexadecimal format (should start with '0x').");
			}

			std::istringstream iss(hex_string.substr(2));
			uint32_t result;
			if (!(iss >> std::hex >> result)) {
				throw std::invalid_argument("Failed to convert hexadecimal string to integer.");
			}
			return static_cast<RE::FormID>(result);
		}

		bool IsConcentrationSpell(RE::SpellItem* theSpell)
		{
			return theSpell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration;
		}
		int GetSpellRank(RE::SpellItem* theSpell)
		{
			if (theSpell->data.castingPerk) {
				switch (theSpell->data.castingPerk->formID) {
				case 0xF2CA6:
				case 0xF2CA7:
				case 0xF2CA8:
				case 0xF2CA9:
				case 0xF2CAA:
					return 1;
				case 0xC44B7:
				case 0xC44BB:
				case 0xC44BF:
				case 0xC44C3:
				case 0xC44C7:
					return 2;
				case 0xC44B8:
				case 0xC44BC:
				case 0xC44C0:
				case 0xC44C4:
				case 0xC44C8:
					return 3;
				case 0xC44B9:
				case 0xC44BD:
				case 0xC44C1:
				case 0xC44C5:
				case 0xC44C9:
					return 4;
				case 0x000C44BA:
				case 0x000C44BE:
				case 0x000C44C2:
				case 0x000C44C6:
				case 0x000C44CA:
					return 5;
				}
			}
			return 0;
		}
		const int GetSpellLevelApprox(RE::SpellItem* const& theSpell)
		{
			if (theSpell->effects.size() != 0 && theSpell->effects.front() && theSpell->effects.front()->baseEffect)
				return min(GetSpellRank(theSpell) * 25 - 25, theSpell->effects.front()->baseEffect->GetMinimumSkillLevel());
			return 0;
		}
		RE::TESGlobal* GetFilterGlobalForSpell(RE::SpellItem* theSpell)
		{
			const auto& spellLevel = GetSpellLevelApprox(theSpell);
			auto filterGlob = FORMS::GetSingleton().GlobFilterStrange;
			if (spellLevel < 25) {
				filterGlob = FORMS::GetSingleton().GlobFilterNovice;
			} else if (spellLevel < 50) {
				filterGlob = FORMS::GetSingleton().GlobFilterApprentice;
			} else if (spellLevel < 75) {
				filterGlob = FORMS::GetSingleton().GlobFilterAdept;
			} else if (spellLevel < 100) {
				filterGlob = FORMS::GetSingleton().GlobFilterExpert;
			} else {
				filterGlob = FORMS::GetSingleton().GlobFilterMaster;
			}
			if (GetSpellRank(theSpell) == 0) {
				filterGlob = FORMS::GetSingleton().GlobFilterStrange;
			}
			return filterGlob;
		}
		void AddTierKeywords(RE::ScrollItem* scrollObj, RE::SpellItem* theSpell)
		{
			switch (theSpell->GetAssociatedSkill()) {
			case RE::ActorValue::kAlteration:
				scrollObj->AddKeyword(FORMS::GetSingleton().KywdScrollAlteration);
				break;
			case RE::ActorValue::kConjuration:
				scrollObj->AddKeyword(FORMS::GetSingleton().KywdScrollConjuration);
				break;
			case RE::ActorValue::kDestruction:
				scrollObj->AddKeyword(FORMS::GetSingleton().KywdScrollDestruction);
				break;
			case RE::ActorValue::kIllusion:
				scrollObj->AddKeyword(FORMS::GetSingleton().KywdScrollIllusion);
				break;
			case RE::ActorValue::kRestoration:
				scrollObj->AddKeyword(FORMS::GetSingleton().KywdScrollRestoration);
				break;
			}
		}
		void AddDisintegrateEffect(RE::ScrollItem* scrollObj)
		{
			bool isHostile = false;
			for (auto& eff : scrollObj->effects) {
				isHostile = isHostile || eff->IsHostile();
				if (eff == FORMS::GetSingleton().SpelDisintegrateEffectTemplate->effects.front())
					return;
			}

			auto castType = scrollObj->GetCastingType();

			if (SCRIBE::CACHE::SpellScrollBiMap.containsValue(scrollObj)) {
				auto assocSpell = SCRIBE::CACHE::SpellScrollBiMap.getKey(scrollObj);
				if (assocSpell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration)
					castType = assocSpell->GetCastingType();
			}

			if (isHostile && castType == RE::MagicSystem::CastingType::kFireAndForget) {
				bool isTargeted = false;
				bool isArea = false;
				bool isLocationArea = false;
				for (auto& eff : scrollObj->effects) {
					isTargeted = isTargeted ||
					             (eff->baseEffect->data.delivery == RE::MagicSystem::Delivery::kTargetActor ||
									 eff->baseEffect->data.delivery == RE::MagicSystem::Delivery::kAimed ||
									 eff->baseEffect->data.delivery == RE::MagicSystem::Delivery::kTouch);
					isArea = isArea || eff->baseEffect->data.delivery == RE::MagicSystem::Delivery::kSelf;
					isLocationArea = isLocationArea || eff->baseEffect->data.delivery == RE::MagicSystem::Delivery::kTargetLocation;
				}
				if (isTargeted)
					scrollObj->effects.emplace_back(FORMS::GetSingleton().SpelDisintegrateEffectTemplate->effects.front());
				//if (isArea)
				//	scrollObj->effects.emplace_back(effectDisintegrateArea->effects[0]);
				//if (isLocationArea)
				//	scrollObj->effects.emplace_back(effectDisintegrateLocationArea->effects[0]);
			}
		}
		void AddRankKeywords(RE::ScrollItem* scrollObj, RE::SpellItem* theSpell)
		{
			const auto& spellLevel = GetSpellLevelApprox(theSpell);
			if (spellLevel < 25) {
				scrollObj->AddKeyword(FORMS::GetSingleton().KywdScrollNovice);
			} else if (spellLevel < 50) {
				scrollObj->AddKeyword(FORMS::GetSingleton().KywdScrollApprentice);
			} else if (spellLevel < 75) {
				scrollObj->AddKeyword(FORMS::GetSingleton().KywdScrollAdept);
			} else if (spellLevel < 100) {
				scrollObj->AddKeyword(FORMS::GetSingleton().KywdScrollExpert);
			} else {
				scrollObj->AddKeyword(FORMS::GetSingleton().KywdScrollMaster);
			}
			if (GetSpellRank(theSpell) == 0) {
				scrollObj->AddKeyword(FORMS::GetSingleton().KywdScrollStrange);
			}
		}

		std::size_t GetEffectListHash(RE::BSTArray<RE::Effect*> effList)
		{
			std::size_t effHash = 0UL;
			for (auto& eff : effList) {
				std::size_t h1 = std::hash<RE::FormID>{}(eff->baseEffect->formID);
				effHash ^= (h1 << 1);
			}
			return effHash;
		}
		std::size_t GetNameHash(std::string name)
		{
			return std::hash<std::string>{}(name);
		}

		std::string ExtractSpellName(const std::string& inputString)
		{
			const std::regex regexPattern(R"(\bScroll\s+of\s+([^\(\)-]+)\b)");

			//const std::regex regexPattern("(?:Scroll of|Conjure|Summon) (?:the )?(\\w+(?: \\w+)*)");
			std::smatch match;
			if (std::regex_search(inputString, match, regexPattern)) {
				return match[1];
			} else {
				return "<No Spell Name Found>";
			}
		}

		std::vector<RE::BGSConstructibleObject*> GetConstructibleObjectForScroll(CobjGenerationArgs args)
		{
			static const auto cobjFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSConstructibleObject>();
			if (!cobjFactory) {
				logger::error("Failed to fetch IFormFactory: COBJ!");
				return {};
			}

			auto spellRank = GetSpellRank(args.theSpell);
			auto filterGlob = GetFilterGlobalForSpell(args.theSpell);

			auto constructibleObj = cobjFactory->Create();
			constructibleObj->benchKeyword = FORMS::GetSingleton().KywdScrollEnchantingStation;
			constructibleObj->requiredItems.AddObjectToContainer(FORMS::GetSingleton().MiscArcaneDust, args.baseDust, nullptr);
			constructibleObj->requiredItems.AddObjectToContainer(FORMS::GetSingleton().MiscPaperRoll, 1, nullptr);
			constructibleObj->createdItem = args.theScroll;

			auto nodeSpellLearnedFirst = new RE::TESConditionItem;
			auto nodeSpellLearnedSecond = new RE::TESConditionItem;
			auto nodeHasInscriptionLevel = new RE::TESConditionItem;
			auto nodeFilterOnlyKnown = new RE::TESConditionItem;
			auto nodeFilterSpellRank = new RE::TESConditionItem;
			auto nodeHasDustPerk = new RE::TESConditionItem;

			// A || B && A || C && D && E
			nodeSpellLearnedFirst->next = nodeHasInscriptionLevel;
			nodeSpellLearnedFirst->data.comparisonValue.f = 1.0f;
			nodeSpellLearnedFirst->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kHasSpell;
			nodeSpellLearnedFirst->data.functionData.params[0] = args.theSpell;
			nodeSpellLearnedFirst->data.flags.isOR = true;

			nodeHasInscriptionLevel->next = nodeSpellLearnedSecond;
			nodeHasInscriptionLevel->data.comparisonValue.f = max(0, spellRank - 1) * 20.0f;
			nodeHasInscriptionLevel->data.flags.opCode = RE::CONDITION_ITEM_DATA::OpCode::kGreaterThanOrEqualTo;
			nodeHasInscriptionLevel->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetGlobalValue;
			nodeHasInscriptionLevel->data.functionData.params[0] = FORMS::GetSingleton().GlobScribeLevel;
			nodeHasInscriptionLevel->data.flags.isOR = false;

			nodeSpellLearnedSecond->next = nodeFilterOnlyKnown;
			nodeSpellLearnedSecond->data.comparisonValue.f = 1.0f;
			nodeSpellLearnedSecond->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kHasSpell;
			nodeSpellLearnedSecond->data.functionData.params[0] = args.theSpell;
			nodeSpellLearnedSecond->data.flags.isOR = true;

			nodeFilterOnlyKnown->next = nodeFilterSpellRank;
			nodeFilterOnlyKnown->data.comparisonValue.f = 0.0f;
			nodeFilterOnlyKnown->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetGlobalValue;
			nodeFilterOnlyKnown->data.functionData.params[0] = FORMS::GetSingleton().GlobFilterKnown;
			nodeFilterOnlyKnown->data.flags.isOR = false;

			nodeFilterSpellRank->next = nodeHasDustPerk;
			nodeFilterSpellRank->data.comparisonValue.f = 1.0f;
			nodeFilterSpellRank->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetGlobalValue;
			nodeFilterSpellRank->data.functionData.params[0] = filterGlob;
			nodeFilterSpellRank->data.flags.isOR = false;

			nodeHasDustPerk->next = nullptr;
			nodeHasDustPerk->data.comparisonValue.f = 0.0f;
			nodeHasDustPerk->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kHasPerk;
			nodeHasDustPerk->data.functionData.params[0] = FORMS::GetSingleton().PerkDustDiscount;

			constructibleObj->conditions.head = nodeSpellLearnedFirst;

			auto constructibleObjDustPerk = cobjFactory->Create();

			constructibleObjDustPerk->benchKeyword = FORMS::GetSingleton().KywdScrollEnchantingStation;
			constructibleObjDustPerk->requiredItems.AddObjectToContainer(FORMS::GetSingleton().MiscArcaneDust, args.reducedDust, nullptr);
			constructibleObjDustPerk->requiredItems.AddObjectToContainer(FORMS::GetSingleton().MiscPaperRoll, 1, nullptr);
			constructibleObjDustPerk->createdItem = args.theScroll;

			auto DDnodeSpellLearnedFirst = new RE::TESConditionItem;
			auto DDnodeSpellLearnedSecond = new RE::TESConditionItem;
			auto DDnodeHasInscriptionLevel = new RE::TESConditionItem;
			auto DDnodeFilterOnlyKnown = new RE::TESConditionItem;
			auto DDnodeFilterSpellRank = new RE::TESConditionItem;
			auto DDnodeHasDustPerk = new RE::TESConditionItem;

			// A || B && A || C && D && E
			DDnodeSpellLearnedFirst->next = DDnodeHasInscriptionLevel;
			DDnodeSpellLearnedFirst->data.comparisonValue.f = 1.0f;
			DDnodeSpellLearnedFirst->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kHasSpell;
			DDnodeSpellLearnedFirst->data.functionData.params[0] = args.theSpell;
			DDnodeSpellLearnedFirst->data.flags.isOR = true;

			DDnodeHasInscriptionLevel->next = DDnodeSpellLearnedSecond;
			DDnodeHasInscriptionLevel->data.comparisonValue.f = max(0, spellRank - 1) * 20.0f;
			DDnodeHasInscriptionLevel->data.flags.opCode = RE::CONDITION_ITEM_DATA::OpCode::kGreaterThanOrEqualTo;
			DDnodeHasInscriptionLevel->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetGlobalValue;
			DDnodeHasInscriptionLevel->data.functionData.params[0] = FORMS::GetSingleton().GlobScribeLevel;
			DDnodeHasInscriptionLevel->data.flags.isOR = false;

			DDnodeSpellLearnedSecond->next = DDnodeFilterOnlyKnown;
			DDnodeSpellLearnedSecond->data.comparisonValue.f = 1.0f;
			DDnodeSpellLearnedSecond->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kHasSpell;
			DDnodeSpellLearnedSecond->data.functionData.params[0] = args.theSpell;
			DDnodeSpellLearnedSecond->data.flags.isOR = true;

			DDnodeFilterOnlyKnown->next = DDnodeFilterSpellRank;
			DDnodeFilterOnlyKnown->data.comparisonValue.f = 0.0f;
			DDnodeFilterOnlyKnown->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetGlobalValue;
			DDnodeFilterOnlyKnown->data.functionData.params[0] = FORMS::GetSingleton().GlobFilterKnown;
			DDnodeFilterOnlyKnown->data.flags.isOR = false;

			DDnodeFilterSpellRank->next = DDnodeHasDustPerk;
			DDnodeFilterSpellRank->data.comparisonValue.f = 1.0f;
			DDnodeFilterSpellRank->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetGlobalValue;
			DDnodeFilterSpellRank->data.functionData.params[0] = filterGlob;
			DDnodeFilterSpellRank->data.flags.isOR = false;

			DDnodeHasDustPerk->next = nullptr;
			DDnodeHasDustPerk->data.comparisonValue.f = 1.0f;
			DDnodeHasDustPerk->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kHasPerk;
			DDnodeHasDustPerk->data.functionData.params[0] = FORMS::GetSingleton().PerkDustDiscount;

			constructibleObjDustPerk->conditions.head = DDnodeSpellLearnedFirst;

			if (SCRIBE::CONFIG::Plugin::GetSingleton().GetBoolValue("SETTINGS", "Generate10xRecipes")) {
				auto constructibleObj10x = cobjFactory->Create();

				constructibleObj10x->benchKeyword = FORMS::GetSingleton().KywdScrollEnchantingStation;
				constructibleObj10x->requiredItems.AddObjectToContainer(FORMS::GetSingleton().MiscArcaneDust, args.baseDust * 10, nullptr);
				constructibleObj10x->requiredItems.AddObjectToContainer(FORMS::GetSingleton().MiscPaperRoll, 10, nullptr);
				constructibleObj10x->createdItem = args.theScroll;
				constructibleObj10x->data.numConstructed = 10;
				constructibleObj10x->conditions.head = nodeSpellLearnedFirst;

				auto constructibleObjDustPerk10x = cobjFactory->Create();

				constructibleObjDustPerk10x->benchKeyword = FORMS::GetSingleton().KywdScrollEnchantingStation;
				constructibleObjDustPerk10x->requiredItems.AddObjectToContainer(FORMS::GetSingleton().MiscArcaneDust, args.reducedDust * 10, nullptr);
				constructibleObjDustPerk10x->requiredItems.AddObjectToContainer(FORMS::GetSingleton().MiscPaperRoll, 10, nullptr);
				constructibleObjDustPerk10x->createdItem = args.theScroll;
				constructibleObjDustPerk10x->data.numConstructed = 10;
				constructibleObjDustPerk10x->conditions.head = DDnodeSpellLearnedFirst;

				return { constructibleObj, constructibleObj10x, constructibleObjDustPerk, constructibleObjDustPerk10x };
			}
			else
				return { constructibleObj, constructibleObjDustPerk };
		}
	}

	namespace CONFIG
	{

	}

	namespace CACHE
	{
		void AddNameAndEffectHashedSpell(RE::SpellItem* theSpell)
		{
			HashToSpellMap[UTIL::GetEffectListHash(theSpell->effects)] = theSpell;
			HashToSpellMap[UTIL::GetNameHash(theSpell->GetName())] = theSpell;
		}

		void AddKeywordSpellCache(RE::SpellItem* theSpell)
		{
			for (auto& eff : theSpell->effects) {
				for (size_t i = 0; i < eff->baseEffect->numKeywords; i++) {
					const auto& kywd = eff->baseEffect->keywords[i];
					KeywordSpellListMap[kywd].push_back(theSpell);
				}
			}
		}
	}
}
