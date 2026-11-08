#include "AlchemyItem.h"

#include <Systems/ModSystem.h>
#include <Magic/EffectItem.h>
#include <World.h>
#include <Games/BGSCreatedObjectManager.h>

#include <mutex>

namespace
{
constexpr uint32_t kDynamicFormMask = 0xFF000000u;

// Content key -> form ID of the local item that matches it. Form IDs, not pointers: the save can be
// reloaded, after which a cached form may be gone or may be a different item.
std::mutex s_cacheMutex;
TiltedPhoques::Map<uint32_t, uint32_t> s_formByKey;

// Describes a local potion or poison by its effects. Returns false if it has nothing to describe.
bool ReadData(AlchemyItem* apItem, Inventory::AlchemyData& aData) noexcept
{
    auto& modSystem = World::Get().GetModSystem();

    aData = {};
    aData.Kind = apItem->IsPoison() ? Inventory::AlchemyData::kPoison : Inventory::AlchemyData::kPotion;

    for (EffectItem* pEffect : apItem->listOfEffects)
    {
        if (!pEffect || !pEffect->pEffectSetting)
            continue;

        Inventory::EffectItem effect{};
        effect.Magnitude = pEffect->data.fMagnitude;
        effect.Area = pEffect->data.iArea;
        effect.Duration = pEffect->data.iDuration;
        effect.RawCost = pEffect->fRawCost;
        modSystem.GetServerModId(pEffect->pEffectSetting->formID, effect.EffectId);
        aData.Effects.push_back(effect);
    }

    return aData.IsPresent();
}
} // namespace

AlchemyItem* AlchemyItem::Create(const Inventory::AlchemyData& aData) noexcept
{
    TP_THIS_FUNCTION(TAddPotion, void, void, AlchemyItem**, GameArray<EffectItem>*);
    POINTER_SKYRIMSE(TAddPotion, addPotion, 36167);
    TP_THIS_FUNCTION(TAddPoison, AlchemyItem*, void, AlchemyItem**, GameArray<EffectItem>*);
    POINTER_SKYRIMSE(TAddPoison, addPoison, 36168);

    ModSystem& modSystem = World::Get().GetModSystem();

    GameArray<EffectItem> effects{};
    effects.Resize(aData.Effects.size());
    for (size_t i = 0; i < aData.Effects.size(); i++)
    {
        const Inventory::EffectItem& effect = aData.Effects[i];

        EffectItem effectItem{};
        effectItem.data.fMagnitude = effect.Magnitude;
        effectItem.data.iArea = effect.Area;
        effectItem.data.iDuration = effect.Duration;
        effectItem.fRawCost = effect.RawCost;
        effectItem.pEffectSetting = Cast<EffectSetting>(TESForm::GetById(modSystem.GetGameId(effect.EffectId)));
        if (!effectItem.pEffectSetting)
        {
            // Without its effect the potion would be wrong, and the game would not like a null effect.
            spdlog::error("Alchemy effect setting not found: {:X}:{:X}", effect.EffectId.ModId, effect.EffectId.BaseId);
            return nullptr;
        }

        effects[i] = effectItem;
    }

    BGSCreatedObjectManager* objManager = BGSCreatedObjectManager::Get();

    // The game stores the new item in the out parameter and keeps one reference for it. That reference is
    // deliberately never released, so the item stays alive for the whole session.
    AlchemyItem* pCreated = nullptr;
    if (aData.Kind == Inventory::AlchemyData::kPoison)
    {
        AlchemyItem* pReturned = TiltedPhoques::ThisCall(addPoison, objManager, &pCreated, &effects);
        if (!pCreated)
            pCreated = pReturned;
    }
    else
    {
        TiltedPhoques::ThisCall(addPotion, objManager, &pCreated, &effects);
    }

    return pCreated;
}

void AlchemySync::SetEntryBase(Inventory::Entry& arEntry, TESForm* apForm) noexcept
{
    World::Get().GetModSystem().GetServerModId(apForm->formID, arEntry.BaseId);

    // Only runtime-created forms need the alchemy treatment.
    if ((apForm->formID & kDynamicFormMask) != kDynamicFormMask)
        return;

    AlchemyItem* pItem = Cast<AlchemyItem>(apForm);
    if (!pItem)
        return;

    Inventory::AlchemyData data;
    if (!ReadData(pItem, data))
        return;

    const uint32_t key = data.ComputeKey();

    // Remember our own potion, so that receiving an identical one later reuses it instead of creating a twin.
    {
        std::scoped_lock lock(s_cacheMutex);
        s_formByKey.try_emplace(key, apForm->formID);
    }

    arEntry.BaseId = GameId{Inventory::AlchemyData::kKeyModId, key};
    arEntry.AlchemyInfo = std::move(data);
}

AlchemyItem* AlchemySync::ResolveItem(const Inventory::AlchemyData& acData, bool aCreate) noexcept
{
    if (!acData.IsPresent())
        return nullptr;

    const uint32_t key = acData.ComputeKey();

    std::scoped_lock lock(s_cacheMutex);

    if (const auto it = s_formByKey.find(key); it != s_formByKey.end())
    {
        // Only trust the cached form if it still describes the same potion.
        AlchemyItem* pCached = Cast<AlchemyItem>(TESForm::GetById(it->second));
        Inventory::AlchemyData current;
        if (pCached && ReadData(pCached, current) && current.ComputeKey() == key)
            return pCached;

        s_formByKey.erase(it);
    }

    if (!aCreate)
        return nullptr;

    AlchemyItem* pItem = AlchemyItem::Create(acData);
    if (!pItem)
    {
        spdlog::error("Could not create alchemy item with {} effect(s)", acData.Effects.size());
        return nullptr;
    }

    s_formByKey[key] = pItem->formID;
    spdlog::info("Created {:X} for a {} with {} effect(s)", pItem->formID, acData.Kind == Inventory::AlchemyData::kPoison ? "poison" : "potion", acData.Effects.size());
    return pItem;
}
