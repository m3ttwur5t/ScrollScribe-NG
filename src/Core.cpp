#include "Core.hpp"
#include "Util.h"
#include <regex>

namespace SCRIBE
{

	std::vector<std::string> splitString(const std::string& input, char delimiter)
	{
		std::vector<std::string> tokens;
		std::istringstream ss(input);
		std::string token;

		while (std::getline(ss, token, delimiter)) {
			tokens.push_back(token);
		}

		return tokens;
	}

	bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm)
	{
		vm->RegisterFunction("FuseAndCreate", "ScrollScribeExtender", FuseAndCreate);
		vm->RegisterFunction("CanFuse", "ScrollScribeExtender", CanFuse);
		vm->RegisterFunction("GetScrollForBook", "ScrollScribeExtender", GetScrollForBook);
		vm->RegisterFunction("GetSpellFromScroll", "ScrollScribeExtender", GetSpellFromScroll);
		vm->RegisterFunction("GetZeroCostCopy", "ScrollScribeExtender", GetZeroCostCopy);
		vm->RegisterFunction("GetApproxFullGoldValue", "ScrollScribeExtender", GetApproxFullGoldValue);
		vm->RegisterFunction("GetUpgradedSpell", "ScrollScribeExtender", GetUpgradedSpell);
		vm->RegisterFunction("GetScrollFromSpell", "ScrollScribeExtender", GetScrollFromSpell);

		return true;
	}

	int GetApproxFullGoldValue(RE::StaticFunctionTag*, RE::TESForm* form)
	{
		if (form == nullptr)
			return 0;
		if (auto weapon = form->As<RE::TESObjectWEAP>(); weapon != nullptr) {
			logger::info("{} is weapon", form->GetName());
			if (weapon->formEnchanting == nullptr)
				return weapon->GetGoldValue();
			logger::info("{} has enchantment: {}", form->GetName(), weapon->formEnchanting->GetFullName());
			auto enchItem = weapon->formEnchanting;
			return (weapon->GetGoldValue() + static_cast<int>(0.4 * enchItem->CalculateTotalGoldValue()));
		}
		if (auto armor = form->As<RE::TESObjectARMO>(); armor != nullptr) {
			logger::info("{} is armor", form->GetName());
			if (armor->formEnchanting == nullptr)
				return armor->GetGoldValue();
			logger::info("{} has enchantment: {}", form->GetName(), armor->formEnchanting->GetFullName());
			auto enchItem = armor->formEnchanting;
			return (armor->GetGoldValue() + static_cast<int>(0.85 * enchItem->CalculateTotalGoldValue())) / 2;
		}
		return form->GetGoldValue();
	}

	RE::SpellItem* GetZeroCostCopy(RE::StaticFunctionTag*, RE::SpellItem* spell)
	{
		if (spell == nullptr)
			return nullptr;
		static auto spellFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();
		if (!spellFactory) {
			logger::error("Failed to fetch IFormFactory: SPEL!");
			return nullptr;
		}

		if (!SCRIBE::CACHE::ZeroCostMap.contains(spell)) {
			auto theScroll = SCRIBE::CACHE::SpellScrollBiMap.containsKey(spell) ? SCRIBE::CACHE::SpellScrollBiMap.getValue(spell) : nullptr;

			auto zeroCostSpell = spellFactory->Create();
			zeroCostSpell->fullName = spell->GetFullName();
			zeroCostSpell->effects = spell->effects;
			zeroCostSpell->data.castDuration = spell->data.castDuration;
			zeroCostSpell->SetDelivery(spell->GetDelivery());
			zeroCostSpell->SetCastingType(spell->GetCastingType());
			zeroCostSpell->SetAutoCalc(false);
			zeroCostSpell->data.flags.set(RE::SpellItem::SpellFlag::kCostOverride);
			zeroCostSpell->data.costOverride = 0;

			for (uint32_t i = 0; theScroll && i < theScroll->GetNumKeywords(); i++)
				if (auto kwd = theScroll->GetKeywordAt(i); kwd.has_value())
					zeroCostSpell->AddKeyword(kwd.value());

			SCRIBE::CACHE::ZeroCostMap.insert_or_assign(spell, zeroCostSpell);
		}

		return SCRIBE::CACHE::ZeroCostMap[spell];
	}

	RE::SpellItem* GetSpellFromScroll(RE::StaticFunctionTag*, RE::ScrollItem* scroll)
	{
		if (SCRIBE::CACHE::SpellScrollBiMap.containsValue(scroll))
			return SCRIBE::CACHE::SpellScrollBiMap.getKey(scroll);
		return nullptr;
	}

	RE::ScrollItem* GetScrollForBook(RE::StaticFunctionTag*, RE::TESObjectBOOK* book)
	{
		if (!SCRIBE::CACHE::BookSpellBiMap.containsKey(book))
			return nullptr;
		auto bookSpell = SCRIBE::CACHE::BookSpellBiMap.getValue(book);
		if (!SCRIBE::CACHE::SpellScrollBiMap.containsKey(bookSpell))
			return nullptr;

		return SCRIBE::CACHE::SpellScrollBiMap.getValue(bookSpell);
	}

	bool CanFuse(RE::StaticFunctionTag*, RE::ScrollItem* scrollOne, RE::ScrollItem* scrollTwo, bool canDoubleFuse)
	{
		if (SCRIBE::CACHE::FusionComponentsToResultBiMap.containsValue(scrollOne) && SCRIBE::CACHE::FusionComponentsToResultBiMap.containsValue(scrollTwo)) {
			auto first = SCRIBE::CACHE::FusionComponentsToResultBiMap.getKey(scrollOne);
			auto second = SCRIBE::CACHE::FusionComponentsToResultBiMap.getKey(scrollTwo);

			if (first.first == second.first || first.first == second.second || first.second == second.first || first.second == second.second) {
				logger::info("Incest fusion. Denied.");
				return false;
			}
		}

		auto fusedKywd = SCRIBE::FORMS::GetSingleton().KywdFused;
		auto fusedDoubleKywd = SCRIBE::FORMS::GetSingleton().KywdDoubleFused;

		if (scrollOne == nullptr || scrollTwo == nullptr)
			return false;
		if (scrollOne->GetCastingType() != scrollTwo->GetCastingType())
			return false;
		if (scrollOne->GetDelivery() != scrollTwo->GetDelivery())
			return false;
		if (!canDoubleFuse && (scrollOne->HasKeyword(fusedKywd) || scrollTwo->HasKeyword(fusedKywd)))
			return false;
		if (canDoubleFuse &&
			(scrollOne->HasKeyword(fusedKywd) && !scrollTwo->HasKeyword(fusedKywd) || !scrollOne->HasKeyword(fusedKywd) && scrollTwo->HasKeyword(fusedKywd)))
			return false;
		if (scrollOne->HasKeyword(fusedDoubleKywd) || scrollTwo->HasKeyword(fusedDoubleKywd))
			return false;
		return true;
	}

	static RE::SpellItem* GetUpgradedSpellFunc(RE::SpellItem* spell, bool listCandidates = false)
	{
		static constexpr auto getSpellScrollValue = [](RE::SpellItem* spell) {
			return SCRIBE::CACHE::SpellScrollBiMap.containsKey(spell) ? SCRIBE::CACHE::SpellScrollBiMap.getValue(spell)->GetGoldValue() : 0;
		};

		static auto biasedRandomNumber = [&](const std::vector<RE::SpellItem*>& container) {
			static std::random_device rd;
			static std::mt19937 gen(rd());
			std::vector<int> weights;

			int w = 10000;
			for (int i = 1; i <= container.size(); i++) {
				//logger::info("{} = {}", container[i - 1]->GetName(), w);
				weights.push_back(w);
				w = static_cast<int>((w * 8.0) / 10.0);
				w = w < 1000 ? 1000 : w;
			}

			std::discrete_distribution<int> d(weights.begin(), weights.end());

			return container[d(gen)];
		};

		const static auto player = RE::PlayerCharacter::GetSingleton();
		std::vector<RE::SpellItem*> candidates;

		for (auto& eff : spell->effects) {
			for (size_t i = 0; i < eff->baseEffect->numKeywords && i < 2; i++) {
				auto& kywd = eff->baseEffect->keywords[i];
				for (auto& upSpell : SCRIBE::CACHE::KeywordSpellListMap[kywd]) {
					if ((SCRIBE::CACHE::SpellScrollBiMap.containsKey(upSpell) &&
							SCRIBE::CACHE::SpellScrollBiMap.containsKey(spell)) &&
						(getSpellScrollValue(upSpell) > getSpellScrollValue(spell)) &&
						(upSpell->effects.front()->baseEffect->data.resistVariable == spell->effects.front()->baseEffect->data.resistVariable || upSpell->effects.front()->baseEffect->data.archetype == spell->effects.front()->baseEffect->data.archetype) && upSpell->data.delivery == spell->data.delivery) {
						candidates.push_back(upSpell);
					}
				}
			}
		}

		std::sort(candidates.begin(), candidates.end());
		candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

		std::ranges::sort(
			candidates,
			[](RE::SpellItem* a, RE::SpellItem* b) {
				return getSpellScrollValue(a) < getSpellScrollValue(b);
			});

		if (candidates.size() > 0) {
			const auto& candidate = biasedRandomNumber(candidates);
			if (listCandidates) {
				logger::info("[UpSpell] {} has {} upgrade candidates.", spell->GetName(), candidates.size());
				for (auto& v : candidates)
					if (v == candidate)
						logger::info("\t |-> {}", v->GetName());
					else
						logger::info("\t {}", v->GetName());
			}

			return candidate;
		} else {
			logger::info("[UpSpell] {} has no upgrade candidates.", spell->GetName());
			return nullptr;
		}
	}
	RE::SpellItem* GetUpgradedSpell(RE::StaticFunctionTag*, RE::SpellItem* spell)
	{
		return GetUpgradedSpellFunc(spell, true);
	}

	RE::ScrollItem* GetScrollFromSpell(RE::StaticFunctionTag*, RE::SpellItem* spell)
	{
		return SCRIBE::CACHE::SpellScrollBiMap.containsKey(spell) ? SCRIBE::CACHE::SpellScrollBiMap.getValue(spell) : nullptr;
	}

	RE::ScrollItem* FuseAndCreateFunc(RE::ScrollItem* scrollOne, RE::ScrollItem* scrollTwo)
	{
		const auto fusedKYWD = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x82C, "Scribe.esp"sv);        // _scrKeywordScrollFused
		const auto doubleFusedKYWD = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x840, "Scribe.esp"sv);  // _scrKeywordScrollFusedTWICE
		const auto equipSlotEither = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13F44);

		if (scrollOne == nullptr || scrollTwo == nullptr)
			return nullptr;
		if (scrollOne->GetCastingType() != scrollTwo->GetCastingType())
			return nullptr;
		if (scrollOne->GetDelivery() != scrollTwo->GetDelivery())
			return nullptr;

		logger::info("Fusion: {} + {}", scrollOne->GetName(), scrollTwo->GetName());

		if (SCRIBE::CACHE::FusionComponentsToResultBiMap.containsKey({ scrollOne, scrollTwo }))
			return SCRIBE::CACHE::FusionComponentsToResultBiMap.getValue({ scrollOne, scrollTwo });
		if (SCRIBE::CACHE::FusionComponentsToResultBiMap.containsKey({ scrollTwo, scrollOne }))
			return SCRIBE::CACHE::FusionComponentsToResultBiMap.getValue({ scrollTwo, scrollOne });

		static auto scrollFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::ScrollItem>();

		if (!scrollFactory) {
			logger::error("Failed to fetch IFormFactory: SCRL!");
			return nullptr;
		}

		auto scrollObj = scrollFactory->Create();

		for (uint32_t i = 0; i < scrollOne->effects.size(); i++) {
			scrollObj->effects.emplace_back(scrollOne->effects[i]);
		}
		for (uint32_t i = 0; i < scrollTwo->effects.size(); i++) {
			scrollObj->effects.emplace_back(scrollTwo->effects[i]);
		}

		auto fusedScrollName =
			"Fused Scroll of "s +
			SCRIBE::UTIL::ExtractSpellName(scrollOne->GetFullName()) +
			" & " +
			SCRIBE::UTIL::ExtractSpellName(scrollTwo->GetFullName());

		for (uint32_t i = 0; i < scrollOne->GetNumKeywords(); i++)
			if (auto kwd = scrollOne->GetKeywordAt(i); kwd.has_value())
				scrollObj->AddKeyword(kwd.value());
		for (uint32_t i = 0; i < scrollTwo->GetNumKeywords(); i++)
			if (auto kwd = scrollTwo->GetKeywordAt(i); kwd.has_value())
				scrollObj->AddKeyword(kwd.value());

		if (scrollObj->HasKeyword(fusedKYWD))
			scrollObj->AddKeyword(doubleFusedKYWD);
		else
			scrollObj->AddKeyword(fusedKYWD);

		scrollObj->menuDispObject = RE::TESForm::LookupByID<RE::TESBoundObject>(0x76e8f);

		SCRIBE::CACHE::FusionComponentsToResultBiMap.insert({ scrollOne, scrollTwo }, scrollObj);

		auto spellOne = SCRIBE::CACHE::SpellScrollBiMap.containsValue(scrollOne) ? SCRIBE::CACHE::SpellScrollBiMap.getKey(scrollOne) : nullptr;
		auto spellTwo = SCRIBE::CACHE::SpellScrollBiMap.containsValue(scrollTwo) ? SCRIBE::CACHE::SpellScrollBiMap.getKey(scrollTwo) : nullptr;

		scrollObj->weight = scrollOne->weight + scrollTwo->weight;
		scrollObj->value = scrollOne->value + scrollTwo->value;
		scrollObj->SpellItem::data = scrollOne->SpellItem::data;
		scrollObj->model = "Clutter/Common/Scroll06.nif"s;
		scrollObj->SetEquipSlot(equipSlotEither);

		if (spellOne && spellOne->GetCastingType() == RE::MagicSystem::CastingType::kConcentration || spellTwo && spellTwo->GetCastingType() == RE::MagicSystem::CastingType::kConcentration) {
			fusedScrollName.append(" - Concentration");
			GenerateFusedConcSpell(scrollObj);
		}

		scrollObj->fullName = fusedScrollName;

		return scrollObj;
	}

	void GenerateFusedConcSpell(RE::ScrollItem* scrollObj)
	{
		if (SCRIBE::CACHE::SpellScrollBiMap.containsValue(scrollObj))
			return;

		static auto spellFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();
		if (!spellFactory) {
			logger::error("Failed to fetch IFormFactory: SPEL!");
			return;
		}
		static auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			logger::error("Failed to fetch TESDataHandler!");
			return;
		}

		auto fusedSpell = spellFactory->Create();
		fusedSpell->data = scrollObj->SpellItem::data;
		for (auto& eff : scrollObj->effects)
			fusedSpell->effects.push_back(eff);

		dataHandler->GetFormArray<RE::SpellItem>().emplace_back(fusedSpell);
		SCRIBE::CACHE::SpellScrollBiMap.insert(fusedSpell, scrollObj);
		logger::info("\tCreated Fusion SPEL: 0x{:08X}", fusedSpell->GetFormID());
	}

	RE::ScrollItem* FuseAndCreate(RE::StaticFunctionTag*, RE::ScrollItem* scrollOne, RE::ScrollItem* scrollTwo)
	{
		auto result = FuseAndCreateFunc(scrollOne, scrollTwo);
		if (result == nullptr)
			return nullptr;

		if (FORMS::GetSingleton().GetUseOffset())
			result->SetFormID(FORMS::GetSingleton().NextFormID(), false);

		static auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			logger::error("Failed to fetch TESDataHandler!");
			return nullptr;
		}

		auto& ini = SCRIBE::CONFIG::Plugin::GetSingleton();

		RE::FormID leftFormID = scrollOne->GetFormID();
		if (SCRIBE::CACHE::FormIDRelocationBiMap.containsKey(leftFormID))
			leftFormID = SCRIBE::CACHE::FormIDRelocationBiMap.getValue(leftFormID);

		std::string leftFormString = std::format("0x{:08X}", leftFormID);
		if (leftFormID < 0xFF000000) {
			leftFormString = std::format("{}~0x{:08X}", scrollOne->GetFile(0)->GetFilename(), scrollOne->GetLocalFormID());
		}

		RE::FormID rightFormID = scrollTwo->GetFormID();
		if (SCRIBE::CACHE::FormIDRelocationBiMap.containsKey(rightFormID))
			rightFormID = SCRIBE::CACHE::FormIDRelocationBiMap.getValue(rightFormID);
		std::string rightFormString = std::format("0x{:08X}", rightFormID);
		if (rightFormID < 0xFF000000) {
			rightFormString = std::format("{}~0x{:08X}", scrollTwo->GetFile(0)->GetFilename(), scrollTwo->GetLocalFormID());
		}

		ini.SetValue("FUSION",
			std::format("0x{:08X}", result->GetFormID()).c_str(),
			std::format("{}+{}", leftFormString, rightFormString).c_str(),
			std::format("# {}", result->GetName()).c_str());

		dataHandler->GetFormArray<RE::ScrollItem>().emplace_back(result);

		return result;
	}

	void LoadFormIDOffset()
	{
		logger::info("{:*^30}", "CHECK OFFSET");
		auto& ini = SCRIBE::CONFIG::Plugin::GetSingleton();
		RE::FormID off = 0x0;
		for (const auto& [key, value] : ini.GetAllKeyValuePairs("SCROLLS")) {
			auto cur = UTIL::lexical_cast_formid(value);
			if (cur > off)
				off = cur;
		}
		for (const auto& [key, value] : ini.GetAllKeyValuePairs("FUSION")) {
			auto cur = UTIL::lexical_cast_formid(key);
			if (cur > off)
				off = cur;
		}

		if (off > 0x0 && off < FORMS::FORMID_OFFSET_BASE) {
			logger::info("V2 Format detected. Offset will not be used.\n");
			FORMS::GetSingleton().SetUseOffset(false);
			return;
		}
		if (ini.GetLongValue("VERSION", "Version") < 0x3)
			ini.SetLongValue("VERSION", "Version", 0x3);

		off &= ~FORMS::FORMID_OFFSET_BASE;
		logger::info("Local Offset: 0x{:08X}", off);
		logger::info("Global Offset: 0x{:08X}\n", FORMS::FORMID_OFFSET_BASE);

		FORMS::GetSingleton().SetOffset(off);
		FORMS::GetSingleton().SetUseOffset(true);
	}

	struct FusionKeyValue
	{
		RE::FormID product;
		std::pair<std::string, RE::FormID> leftIngredient;
		std::pair<std::string, RE::FormID> rightIngredient;
	};
	void LoadFused()
	{
		auto& ini = SCRIBE::CONFIG::Plugin::GetSingleton();

		size_t purgedCount = 0;
		bool updateFile = !FORMS::GetSingleton().GetUseOffset();

		logger::info("{:*^30}", "RESTORING FUSIONS");

		static auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			logger::error("Failed to fetch TESDataHandler!");
			return;
		}

		constexpr auto splitComponents = [](const std::string& input) -> std::pair<std::string, std::string> {
			std::pair<std::string, std::string> components;

			std::size_t plusPos = input.find("+");

			if (plusPos != std::string::npos) {
				components.first = input.substr(0, plusPos);
				components.second = input.substr(plusPos + 1);
			}

			return components;
		};

		constexpr auto getPluginNameWithLocalID = [](const std::string& part) -> std::pair<std::string, RE::FormID> {
			std::size_t tildePos = part.find("~");

			if (tildePos == std::string::npos)
				return { std::string(), UTIL::lexical_cast_formid(part) };

			std::string pluginName = part.substr(0, tildePos);
			auto localFormID = UTIL::lexical_cast_formid(part.substr(tildePos + 1));
			return { pluginName, localFormID };
		};

		auto& keyValues = ini.GetAllKeyValuePairs("FUSION");

		std::vector<FusionKeyValue> lateLoadFusions;

		for (auto& kv : keyValues) {
			auto fusionResultFormID = UTIL::lexical_cast_formid(kv.first);

			auto fusionComponents = splitComponents(kv.second);
			logger::info("Restoring {} with components {} & {}", kv.first, fusionComponents.first, fusionComponents.second);

			auto leftFullPair = getPluginNameWithLocalID(fusionComponents.first);
			auto rightFullPair = getPluginNameWithLocalID(fusionComponents.second);

			if (leftFullPair.first.empty() && leftFullPair.second == 0x0 || rightFullPair.first.empty() && rightFullPair.second == 0x0) {
				logger::info("Invalid data for {}", kv.first);
				ini.DeleteKey("FUSION", kv.first);
				++purgedCount;
				continue;
			}

			RE::ScrollItem* leftScrollItem;
			RE::ScrollItem* rightScrollItem;

			if (!leftFullPair.first.empty())
				leftScrollItem = dataHandler->LookupForm<RE::ScrollItem>(leftFullPair.second, leftFullPair.first);
			else
				leftScrollItem = RE::TESForm::LookupByID<RE::ScrollItem>(leftFullPair.second);

			if (!rightFullPair.first.empty())
				rightScrollItem = dataHandler->LookupForm<RE::ScrollItem>(rightFullPair.second, rightFullPair.first);
			else
				rightScrollItem = RE::TESForm::LookupByID<RE::ScrollItem>(rightFullPair.second);

			if (!leftScrollItem && SCRIBE::CACHE::FormIDRelocationBiMap.containsKey(leftFullPair.second)) {
				auto rel = SCRIBE::CACHE::FormIDRelocationBiMap.getValue(leftFullPair.second);
				logger::info("\tFound relocation: 0x{:08X} => 0x{:08X}", leftFullPair.second, rel);
				leftScrollItem = RE::TESForm::LookupByID<RE::ScrollItem>(rel);
			}

			if (!rightScrollItem && SCRIBE::CACHE::FormIDRelocationBiMap.containsKey(rightFullPair.second)) {
				auto rel = SCRIBE::CACHE::FormIDRelocationBiMap.getValue(rightFullPair.second);
				logger::info("\tFound relocation: 0x{:08X} => 0x{:08X}", rightFullPair.second, rel);
				rightScrollItem = RE::TESForm::LookupByID<RE::ScrollItem>(rel);
			}

			if (leftScrollItem && rightScrollItem) {
				auto loadedScroll = FuseAndCreateFunc(leftScrollItem, rightScrollItem);

				if (loadedScroll == nullptr) {
					logger::info("\tFuseAndCreateFunc RETURNED NULL!");
					ini.DeleteKey("FUSION", kv.first);
					++purgedCount;
				} else {
					logger::info("\tForcing FormID to 0x{:08X}", fusionResultFormID);

					if (auto entry = RE::TESForm::LookupByID<RE::TESForm>(fusionResultFormID); entry != nullptr) {
						logger::info("\tWARNING FORMID ALREADY IN USE BY {} ({})! Choosing to swap: 0x{:08X} <=> 0x{:08X}", entry->GetName(), RE::FormTypeToString(entry->GetFormType()), loadedScroll->GetFormID(), entry->GetFormID());
						entry->SetFormID(loadedScroll->formID, updateFile);
						loadedScroll->SetFormID(fusionResultFormID, updateFile);
					} else {
						loadedScroll->SetFormID(fusionResultFormID, updateFile);
					}
				}
			} else {
				FusionKeyValue entry{
					fusionResultFormID,
					leftFullPair,
					rightFullPair
				};
				lateLoadFusions.push_back(entry);
				logger::info("Invalid entry or components not loaded yet? Deferring...");
			}
		}

		for (auto& kv : lateLoadFusions) {
			logger::info("RETRY 0x{:08X} with components 0x{:08X} & 0x{:08X}", kv.product, kv.leftIngredient.second, kv.rightIngredient.second);

			RE::ScrollItem* leftScrollItem;
			RE::ScrollItem* rightScrollItem;

			if (!kv.leftIngredient.first.empty())
				leftScrollItem = dataHandler->LookupForm<RE::ScrollItem>(kv.leftIngredient.second, kv.leftIngredient.first);
			else
				leftScrollItem = RE::TESForm::LookupByID<RE::ScrollItem>(kv.leftIngredient.second);

			if (!kv.rightIngredient.first.empty())
				rightScrollItem = dataHandler->LookupForm<RE::ScrollItem>(kv.rightIngredient.second, kv.rightIngredient.first);
			else
				rightScrollItem = RE::TESForm::LookupByID<RE::ScrollItem>(kv.rightIngredient.second);

			if (!leftScrollItem && SCRIBE::CACHE::FormIDRelocationBiMap.containsKey(kv.leftIngredient.second)) {
				auto rel = SCRIBE::CACHE::FormIDRelocationBiMap.getValue(kv.leftIngredient.second);
				logger::info("\tFound relocation: 0x{:08X} => 0x{:08X}", kv.leftIngredient.second, rel);
				leftScrollItem = RE::TESForm::LookupByID<RE::ScrollItem>(rel);
			}

			if (!rightScrollItem && SCRIBE::CACHE::FormIDRelocationBiMap.containsKey(kv.rightIngredient.second)) {
				auto rel = SCRIBE::CACHE::FormIDRelocationBiMap.getValue(kv.rightIngredient.second);
				logger::info("\tFound relocation: 0x{:08X} => 0x{:08X}", kv.rightIngredient.second, rel);
				rightScrollItem = RE::TESForm::LookupByID<RE::ScrollItem>(rel);
			}

			if (leftScrollItem && rightScrollItem) {
				auto loadedScroll = FuseAndCreateFunc(leftScrollItem, rightScrollItem);

				if (loadedScroll == nullptr) {
					logger::info("\tFuseAndCreateFunc RETURNED NULL!");
					ini.DeleteKey("FUSION", std::format("0x{:08X}", kv.product));
					++purgedCount;
				} else {
					logger::info("\tForcing FormID to 0x{:08X}", kv.product);

					if (auto entry = RE::TESForm::LookupByID<RE::TESForm>(kv.product); entry != nullptr) {
						logger::info("\tWARNING FORMID ALREADY IN USE BY {} ({})! Choosing to swap: 0x{:08X} <=> 0x{:08X}", entry->GetName(), RE::FormTypeToString(entry->GetFormType()), loadedScroll->GetFormID(), entry->GetFormID());
						entry->SetFormID(loadedScroll->formID, updateFile);
						loadedScroll->SetFormID(kv.product, updateFile);
					} else {
						loadedScroll->SetFormID(kv.product, updateFile);
					}
				}
			} else {
				logger::info("\tInvalid Entry! Purging...");
				ini.DeleteKey("FUSION", std::format("0x{:08X}", kv.product));
				++purgedCount;
			}
		}

		logger::info("Done.\n");
	}

	void PatchSoulGemFormList()
	{
		auto& ini = SCRIBE::CONFIG::Plugin::GetSingleton();

		bool patchGems = ini.GetBoolValue("SETTINGS", "PatchSoulgems");
		if (!patchGems)
			return;

		logger::info("{:*^30}", "PATCHING SOULGEM LISTS");

		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			logger::error("Failed to fetch TESDataHandler!");
			return;
		}

		const auto flistGemsEmpty = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSListForm>(0x80C, "Scribe.esp"sv);   // _scrSoulGemList
		const auto flistGemsFilled = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSListForm>(0x833, "Scribe.esp"sv);  // _scrFilledSoulGemList
		const auto reausableGemKYWD = RE::TESForm::LookupByID<RE::BGSKeyword>(0xED2F1);                                       // ReusableSoulGem

		flistGemsEmpty->ClearData();
		flistGemsFilled->ClearData();

		size_t gemsTotal = dataHandler->GetFormArray<RE::TESSoulGem>().size();
		size_t gemsPatched = 0;

		for (auto& gem : dataHandler->GetFormArray<RE::TESSoulGem>()) {
			if (!gem)
				continue;
			if (gem->HasKeyword(reausableGemKYWD)) {
				logger::info("Skipping Soul Gem: {} (0x{:08X})", gem->GetFullName(), gem->GetFormID());
				continue;
			}

			if (gem->GetContainedSoul() == RE::SOUL_LEVEL::kNone) {
				logger::info("Including {} (0x{:08X}) in _scrSoulGemList ...", gem->GetFullName(), gem->GetFormID());
				flistGemsEmpty->AddForm(gem);
				++gemsPatched;
			} else {
				logger::info("Including {} (0x{:08X}) in _scrFilledSoulGemList ...", gem->GetFullName(), gem->GetFormID());
				flistGemsFilled->AddForm(gem);
				++gemsPatched;
			}
		}
		logger::info("Successfully included {} out of {} Soul Gems in Scribe's formlists.\n", gemsPatched, gemsTotal);
	}

	void VerifyConfiguration()
	{
		auto& ini = SCRIBE::CONFIG::Plugin::GetSingleton();

		logger::info("{:*^30}", "VALIDATING CONFIG");

		if (!ini.HasKey("VERSION", "Version")) {
			ini.SetLongValue("VERSION", "Version", 0L);
		}

		if (!ini.HasKey("SETTINGS", "ModSpellChargingTime"))
			ini.SetBoolValue("SETTINGS", "ModSpellChargingTime", true, "# If true, will make concentration spells/scrolls charge up instantly. Might not affect master-tier spells.");

		if (!ini.HasKey("SETTINGS", "PatchVanillaScrolls")) {
			ini.SetBoolValue("SETTINGS", "PatchVanillaScrolls", true, "# If true, will attempt to integrate vanilla/modded scrolls into Scribe's systems.");
		}

		if (!ini.HasKey("SETTINGS", "ApplyScrollMismatchFix")) {
			ini.SetBoolValue("SETTINGS", "ApplyScrollMismatchFix", true, "# If true, will match scroll stats to be the same as their origin spell. This is necessary for certain modpacks that come with incorrectly setup scrolls.");
		}

		if (!ini.HasKey("SETTINGS", "PatchSoulgems")) {
			ini.SetBoolValue("SETTINGS", "PatchSoulgems", true, "# If true, will integrate all (non-reusable) soulgems into Scribe's systems.");
		}

		if (!ini.HasKey("SETTINGS", "Generate10xRecipes")) {
			ini.SetBoolValue("SETTINGS", "Generate10xRecipes", false, "# If true, will generate the recipes to craft 10 scrolls at a time. Leave false to declutter the crafting menu.");
		}

		logger::info("Done.\n");
	}

	void MigrateFormsToNewFormat()
	{
		auto& ini = SCRIBE::CONFIG::Plugin::GetSingleton();

		logger::info("\t{:*^30}", "MIGRATING OLD FORMIDS");

		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			logger::error("\tFailed to fetch TESDataHandler!");
			return;
		}

		auto kvPairs = ini.GetAllKeyValuePairs("SCROLLS");
		for (auto& kv : kvPairs) {
			if (std::size_t tildePos = kv.first.find("~"); tildePos != std::string::npos)
				continue;

			auto bookFormID = UTIL::lexical_cast_formid(kv.first);
			if (bookFormID == 0x0) {
				ini.DeleteKey("SCROLLS", kv.first);
				continue;
			}

			auto bookForm = RE::TESForm::LookupByID<RE::TESObjectBOOK>(bookFormID);
			if (bookForm == nullptr) {
				ini.DeleteKey("SCROLLS", kv.first);
				continue;
			}

			auto bookSourceFile = bookForm->GetFile(0)->GetFilename();
			auto newKey = std::format("{}~0x{:08X}", bookSourceFile, bookForm->GetLocalFormID());

			logger::info("\tChange key {} => {}", kv.first, newKey);
			ini.DeleteKey("SCROLLS", kv.first);
			ini.SetValue("SCROLLS",
				newKey,
				kv.second,
				std::format("# {}", bookForm->GetName()).c_str());
		}

		logger::info("\tDone.");
	}

	void PerformIniMigrations()
	{
		auto& ini = SCRIBE::CONFIG::Plugin::GetSingleton();

		auto loadedVersion = ini.GetLongValue("VERSION", "Version");
		if (loadedVersion == SCRIBE::INI_VERSION)
			return;

		logger::info("{:*^30}", "UPDATING INI");
		logger::info("v{:02X} => v{:02X}", loadedVersion, SCRIBE::INI_VERSION);

		if (loadedVersion < 1) {
			if (ini.HasSection("LOADORDER")) {
				ini.DeleteSection("LOADORDER");
			}
			MigrateFormsToNewFormat();
			++loadedVersion;
		}

		if (loadedVersion < 3) {
			if (FORMS::GetSingleton().GetUseOffset()) {
				++loadedVersion;
			} else {
				logger::info("Cannot auto-migrate Forms to V3.");
			}
		}

		ini.SetLongValue("VERSION", "Version", loadedVersion);

		logger::info("Done.\n");
	}

	void PerformCleanup()
	{
		logger::info("{:*^30}", "PERFORMING SANITIZATION");

		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			logger::error("Failed to fetch TESDataHandler!");
			return;
		}

		auto& ini = SCRIBE::CONFIG::Plugin::GetSingleton();
		auto keyValues = ini.GetAllKeyValuePairs("SCROLLS");

		size_t removedEntries = 0;

		std::map<std::string, bool> foundPlugins;

		for (auto& kv : keyValues) {
			auto& pluginSource = kv.first;

			std::size_t tildePos = pluginSource.find("~");

			if (tildePos != std::string::npos) {
				std::string pluginName = pluginSource.substr(0, tildePos);

				if (!foundPlugins.contains(pluginName))
					foundPlugins.insert_or_assign(pluginName, dataHandler->LookupModByName(pluginName) != nullptr);

				if (foundPlugins[pluginName] == true)
					continue;
				logger::info("Missing plugin. Removing {}", kv.first);
			} else {
				logger::info("Invalid format. Removing {}", kv.first);
			}
			ini.DeleteKey("SCROLLS", pluginSource);
			++removedEntries;
		}

		logger::info("Removed {} invalid entries.\n", removedEntries);
	}

	void FixScrollSpellMismatch(RE::ScrollItem* theScroll, RE::SpellItem* theSpell)
	{
		for (RE::BSTArrayBase::size_type i = 0; i < theScroll->effects.size(); i++) {
			auto& scrollEff = theScroll->effects[i];
			for (RE::BSTArrayBase::size_type j = 0; j < theSpell->effects.size(); j++) {
				auto& spellEff = theSpell->effects[j];
				if (spellEff->baseEffect->GetFormID() != scrollEff->baseEffect->GetFormID())
					continue;

				bool fixArea = scrollEff->effectItem.area < spellEff->effectItem.area;
				bool fixDuration = scrollEff->effectItem.duration < spellEff->effectItem.duration;
				bool fixMagnitude = scrollEff->effectItem.magnitude < spellEff->effectItem.magnitude;

				if (!fixArea && !fixDuration && !fixMagnitude)
					continue;

				std::string logString = std::format("v\tMismatch in {} (0x{:08X})", scrollEff->baseEffect->GetName(), scrollEff->baseEffect->GetFormID());

				if (fixArea) {
					logString.append(std::format(" | Area {} => {}", scrollEff->effectItem.area, spellEff->effectItem.area));
					scrollEff->effectItem.area = spellEff->effectItem.area;
				}
				if (fixDuration) {
					logString.append(std::format(" | Duration {} = > {}", scrollEff->effectItem.duration, spellEff->effectItem.duration));
					scrollEff->effectItem.duration = spellEff->effectItem.duration;
				}
				if (fixMagnitude) {
					logString.append(std::format(" | Magnitude {} = > {}", scrollEff->effectItem.magnitude, spellEff->effectItem.magnitude));
					scrollEff->effectItem.magnitude = spellEff->effectItem.magnitude;
				}
				logger::info("{}", logString);
			}
		}
	}

	void PatchVanillaScrolls()
	{
		auto& ini = SCRIBE::CONFIG::Plugin::GetSingleton();

		bool patchVanilla = ini.GetBoolValue("SETTINGS", "PatchVanillaScrolls");
		if (!patchVanilla)
			return;

		logger::info("{:*^30}", "PATCHING VANILLA SCROLLS");

		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			logger::error("Failed to fetch TESDataHandler!");
			return;
		}

		bool applyMismatchFix = ini.GetBoolValue("SETTINGS", "ApplyScrollMismatchFix");

		size_t formTotal = 0;
		size_t integratedCount = 0;

		std::vector<RE::TESForm*> missedItems;

		for (auto& replacerScroll : dataHandler->GetFormArray<RE::ScrollItem>()) {
			if (replacerScroll->effects.size() == 0 || replacerScroll->effects.front() == nullptr) {
				logger::info("Skipped null-effect scroll {} (0x{:08X})", replacerScroll->GetName(), replacerScroll->GetFormID());
				continue;
			}
			if (replacerScroll && !replacerScroll->HasKeyword(SCRIBE::FORMS::GetSingleton().KywdScrollCustom)                               // ignore Scribe's scrolls
				&& replacerScroll->HasKeyword(SCRIBE::FORMS::GetSingleton().KywdVendorItemScroll) && strlen(replacerScroll->GetName()) > 0  // filter bogus scrolls
				&& !std::string(replacerScroll->model.c_str()).contains("Actors\\DLC02")) {                                                 // filter Dragonborn spiders which are treated as scroll items

				std::string logString = std::format("Patched {} (0x{:08X})", replacerScroll->GetFullName(), replacerScroll->GetFormID());

				replacerScroll->AddKeyword(SCRIBE::FORMS::GetSingleton().KywdScrollCustom);

				RE::SpellItem* foundSpell = nullptr;
				auto nameHash = SCRIBE::UTIL::GetNameHash(SCRIBE::UTIL::ExtractSpellName(replacerScroll->GetName()));
				if (SCRIBE::CACHE::HashToSpellMap.contains(nameHash)) {
					foundSpell = SCRIBE::CACHE::HashToSpellMap[nameHash];
				} else {
					auto effHash = SCRIBE::UTIL::GetEffectListHash(replacerScroll->effects);
					if (SCRIBE::CACHE::HashToSpellMap.contains(effHash)) {
						foundSpell = SCRIBE::CACHE::HashToSpellMap[effHash];
					}
				}

				if (foundSpell && SCRIBE::CACHE::SpellScrollBiMap.containsKey(foundSpell)) {
					logString.append(std::format(" = SPEL {} (0x{:08X})", foundSpell->GetName(), foundSpell->formID));

					auto oldScroll = SCRIBE::CACHE::SpellScrollBiMap.getValue(foundSpell);

					SCRIBE::CACHE::SpellScrollBiMap.eraseKey(foundSpell);
					SCRIBE::CACHE::SpellScrollBiMap.insert(foundSpell, replacerScroll);

					for (auto& cobj : SCRIBE::CACHE::SpellScrollCobjMap[foundSpell])
						cobj->createdItem = replacerScroll;

					replacerScroll->weight = oldScroll->weight;
					replacerScroll->value = oldScroll->value;

					SCRIBE::CACHE::FormIDRelocationBiMap.insert(oldScroll->GetFormID(), replacerScroll->GetFormID());
					logString.append(std::format(" REL 0x{:08X} => 0x{:08X}", oldScroll->GetFormID(), replacerScroll->GetFormID()));

					SCRIBE::UTIL::AddDisintegrateEffect(replacerScroll);
					SCRIBE::UTIL::AddTierKeywords(replacerScroll, foundSpell);
					SCRIBE::UTIL::AddRankKeywords(replacerScroll, foundSpell);

					if (applyMismatchFix)
						FixScrollSpellMismatch(replacerScroll, foundSpell);

					++integratedCount;

				} else {
					missedItems.push_back(replacerScroll);
				}

				logger::info("{}", logString);
				++formTotal;
			}
		}
		for (auto& ele : missedItems)
			logger::info("Skipped {} (0x{:08X})", ele->GetName(), ele->formID);

		logger::info("Successfully patched {} scrolls. Integrated {} into Scribe's cache.\n", formTotal, integratedCount);
	}

	void GenerateDynamicScrolls()
	{
		logger::info("{:*^30}", "PROCESSING SPELL TOMES");

		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			logger::error("Failed to fetch TESDataHandler!");
			return;
		}

		const auto scrollFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::ScrollItem>();
		if (!scrollFactory) {
			logger::error("Failed to fetch IFormFactory: SCRL!");
			return;
		}

		std::vector<RE::BGSConstructibleObject*> generatedConstructibles;
		std::vector<RE::ScrollItem*> generatedScrolls;

		auto& ini = SCRIBE::CONFIG::Plugin::GetSingleton();
		auto modChargeTime = ini.GetBoolValue("SETTINGS", "ModSpellChargingTime");

		size_t processedEntries = 0;
		bool updateFile = !FORMS::GetSingleton().GetUseOffset();

		for (auto& book : dataHandler->GetFormArray<RE::TESObjectBOOK>()) {
			logger::info("{} (0x{:08X})", book->fullName.c_str(), book->formID);
			if (!book || !book->TeachesSpell() || book->GetSpell() == nullptr) {
				logger::info("Does not teach a valid spell");
				continue;
			}

			auto theSpell = book->GetSpell();
			if (auto castType = theSpell->GetCastingType(); (castType == RE::MagicSystem::CastingType::kConstantEffect) || theSpell->data.costOverride <= 5) {
				logger::info("Invalid Casting Type or cost <= 5");
				continue;
			}

			if (theSpell->effects.size() == 0 || theSpell->effects.front() == nullptr) {
				logger::info("Skipped null-effect spell {} (0x{:08X})", theSpell->GetName(), theSpell->GetFormID());
				continue;
			}
			
			logger::info("{} (0x{:08X}) = SPEL 0x{:08X}", book->fullName.c_str(), book->formID, theSpell->formID);

			auto scrollObj = scrollFactory->Create();

			SCRIBE::CACHE::AddKeywordSpellCache(theSpell);
			SCRIBE::CACHE::AddNameAndEffectHashedSpell(theSpell);

			auto spellRank = SCRIBE::UTIL::GetSpellRank(theSpell);
			auto isConcentration = SCRIBE::UTIL::IsConcentrationSpell(theSpell);

			auto fullScrollName = std::format("Scroll of {}", theSpell->GetFullName());

			scrollObj->AddKeyword(SCRIBE::FORMS::GetSingleton().KywdVendorItemScroll);
			scrollObj->AddKeyword(SCRIBE::FORMS::GetSingleton().KywdScrollCustom);
			if (isConcentration) {
				fullScrollName.append(" - Concentration");
				scrollObj->AddKeyword(SCRIBE::FORMS::GetSingleton().KywdKeywordScrollConcentration);
			}

			scrollObj->fullName = fullScrollName;

			scrollObj->weight = 0.1f;
			scrollObj->SpellItem::data = theSpell->data;
			scrollObj->model = "Clutter/Common/Scroll01.nif"s;
			scrollObj->menuDispObject = RE::TESForm::LookupByID<RE::TESBoundObject>(0x76e8f);
			scrollObj->SetEquipSlot(SCRIBE::FORMS::GetSingleton().EquipSlotEither);

			for (auto& eff : theSpell->effects)
				scrollObj->effects.emplace_back(eff);

			if (modChargeTime && isConcentration) {
				//for (auto& eff : scrollObj->effects)
				//	eff->baseEffect->data.spellmakingChargeTime = 0.0;
				scrollObj->SpellItem::data.chargeTime = 0.0f;
			}

			SCRIBE::CACHE::BookSpellBiMap.insert(book, theSpell);
			SCRIBE::CACHE::SpellScrollBiMap.insert(theSpell, scrollObj);

			SCRIBE::UTIL::AddDisintegrateEffect(scrollObj);
			SCRIBE::UTIL::AddTierKeywords(scrollObj, theSpell);
			SCRIBE::UTIL::AddRankKeywords(scrollObj, theSpell);

			int baseDustCost = std::max<int>(spellRank * 5, SCRIBE::UTIL::GetSpellLevelApprox(theSpell)) + static_cast<int>(std::max<float>(std::min<float>(theSpell->GetCostliestEffectItem()->cost, 500), static_cast<float>(theSpell->data.costOverride)));
			baseDustCost = max(baseDustCost / 4, 5);
			if (isConcentration)
				baseDustCost *= 2;
			int reducedDustCost = max((baseDustCost * 66) / 100, 5);

			scrollObj->value = baseDustCost;

			auto bookFormID = std::format("{}~0x{:08X}", book->GetFile(0)->GetFilename(), book->GetLocalFormID());
			if (ini.HasKey("SCROLLS", bookFormID)) {
				std::string logString = "Found ID in INI...";

				auto assignedScrollFormID = UTIL::lexical_cast_formid(ini.GetValue("SCROLLS", bookFormID));
				if (scrollObj->GetFormID() != assignedScrollFormID) {
					logString.append(std::format("Overwrite with 0x{:08X}...", assignedScrollFormID));
					if (auto existingEntry = RE::TESForm::LookupByID<RE::TESForm>(assignedScrollFormID); existingEntry != nullptr && existingEntry->GetFormID() != scrollObj->formID) {
						logString.append(std::format("ID IN USE BY {} ({})! Swapping...", existingEntry->GetName(), RE::FormTypeToString(existingEntry->GetFormType())));
						{
							existingEntry->SetFormID(scrollObj->formID, updateFile);
							scrollObj->SetFormID(assignedScrollFormID, updateFile);
						}
					} else {
						scrollObj->SetFormID(assignedScrollFormID, updateFile);
					}
				}

				logger::info("{}", logString);
			} else {
				if (FORMS::GetSingleton().GetUseOffset())
					scrollObj->SetFormID(FORMS::GetSingleton().NextFormID(), updateFile);
			}

			generatedScrolls.push_back(scrollObj);
			auto cobjList = SCRIBE::UTIL::GetConstructibleObjectForScroll({ scrollObj, theSpell, baseDustCost, reducedDustCost });
			for (auto& cobj : cobjList)
				generatedConstructibles.push_back(cobj);

			SCRIBE::CACHE::SpellScrollCobjMap.insert_or_assign(theSpell, cobjList);

			auto rightHandSide = std::format("0x{:08X}", scrollObj->GetFormID());

			ini.SetValue("SCROLLS",
				bookFormID,
				rightHandSide,
				std::format("# {}", book->GetName()));

			logger::info("Generated Scroll {} (0x{:08X})", scrollObj->GetName(), scrollObj->GetFormID());
			++processedEntries;
		}

		logger::info("Successfully processed {} Spell Tomes.\n\n", processedEntries);

		std::ranges::copy(generatedConstructibles, std::back_inserter(dataHandler->GetFormArray<RE::BGSConstructibleObject>()));
		generatedConstructibles.clear();

		std::ranges::copy(generatedScrolls, std::back_inserter(dataHandler->GetFormArray<RE::ScrollItem>()));
		generatedScrolls.clear();
	}
}

void OnInit(SKSE::MessagingInterface::Message* const a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kPostLoad:
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		SCRIBE::LoadFormIDOffset();
		SCRIBE::VerifyConfiguration();
		SCRIBE::PerformIniMigrations();
		SCRIBE::PerformCleanup();
		SCRIBE::GenerateDynamicScrolls();
		SCRIBE::PatchVanillaScrolls();
		SCRIBE::PatchSoulGemFormList();
		SCRIBE::LoadFused();
		break;
	case SKSE::MessagingInterface::kSaveGame:
		SCRIBE::CONFIG::Plugin::GetSingleton().Save();
		break;
	default:
		break;
	}
}

class ScrollSpellCastEventHandler : public RE::BSTEventSink<RE::TESSpellCastEvent>
{
public:
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* a_event, RE::BSTEventSource<RE::TESSpellCastEvent>*)
	{
		auto castForm = RE::TESForm::LookupByID<RE::TESForm>(a_event->spell);
		if (auto scrollForm = castForm->As<RE::ScrollItem>(); scrollForm != nullptr) {
			SKSE::ModCallbackEvent myEvent{ scrollForm->SpellItem::data.castingType == RE::MagicSystem::CastingType::kConcentration ? "ConcScrollCast" : "FFScrollCast" };
			myEvent.sender = scrollForm;  //RE::TESForm::LookupByID<RE::TESForm>(a_event->spell);
			myEvent.strArg = std::format("{:x}", a_event->object->GetFormID());
			SKSE::GetModCallbackEventSource()->SendEvent(&myEvent);
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	static ScrollSpellCastEventHandler& GetSingleton()
	{
		static ScrollSpellCastEventHandler singleton;
		return singleton;
	}
};

bool Load()
{
	const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(OnInit);

	SKSE::GetPapyrusInterface()->Register(SCRIBE::BindPapyrusFunctions);

	auto& eventProcessor = ScrollSpellCastEventHandler::GetSingleton();
	RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESSpellCastEvent>(&eventProcessor);
	return true;
}
