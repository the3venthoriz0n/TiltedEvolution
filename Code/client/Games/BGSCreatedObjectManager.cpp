#include "BGSCreatedObjectManager.h"

#include <Forms/AlchemyItem.h>

namespace
{
// Layout, the same on 1.6.1170 and 1.7.104 (CommonLibSSE-NG RE::BGSCreatedObjectManager, and the offsets the
// game's own IncrementRef/DecrementRef use): two BSTHashMap<FormID, {MagicItem*, refCount}> for potions and
// poisons, viewed from the offset the game uses, and the BSSpinLock that guards them.
constexpr uintptr_t kPotionMap = 0x40;
constexpr uintptr_t kPoisonMap = 0x70;
constexpr uintptr_t kLock = 0xC8;

// Map header, relative to kPotionMap/kPoisonMap.
constexpr uintptr_t kCapacity = 0x04;
constexpr uintptr_t kEntries = 0x20;

struct MapEntry
{
    uint32_t formId;
    uint32_t pad04;
    void* pMagicItem;
    uint32_t refCount;
    uint32_t pad14;
    MapEntry* pNext; // nullptr marks an empty slot
};
static_assert(sizeof(MapEntry) == 0x20);
static_assert(offsetof(MapEntry, refCount) == 0x10);
static_assert(offsetof(MapEntry, pNext) == 0x18);

int32_t FindRefCount(uintptr_t aMap, uint32_t aFormId) noexcept
{
    const uint32_t capacity = *reinterpret_cast<const uint32_t*>(aMap + kCapacity);
    const auto* pEntries = *reinterpret_cast<MapEntry* const*>(aMap + kEntries);
    if (!pEntries)
        return -1;

    // A linear scan avoids depending on the game's hash function; the maps hold a few hundred items at most.
    for (uint32_t i = 0; i < capacity; ++i)
    {
        const MapEntry& entry = pEntries[i];
        if (entry.pNext && entry.formId == aFormId)
            return static_cast<int32_t>(entry.refCount);
    }

    return -1;
}
} // namespace

BGSCreatedObjectManager* BGSCreatedObjectManager::Get() noexcept
{
    POINTER_SKYRIMSE(BGSCreatedObjectManager*, pObjManager, 400320);
    return *pObjManager.Get();
}

void BGSCreatedObjectManager::IncrementRef(AlchemyItem* apItem) noexcept
{
    TP_THIS_FUNCTION(TIncrementRef, void, BGSCreatedObjectManager, AlchemyItem*);
    POINTER_SKYRIMSE(TIncrementRef, incrementRef, 36170);

    if (apItem)
        TiltedPhoques::ThisCall(incrementRef, this, apItem);
}

void BGSCreatedObjectManager::DecrementRef(AlchemyItem* apItem) noexcept
{
    TP_THIS_FUNCTION(TDecrementRef, void, BGSCreatedObjectManager, AlchemyItem*);
    POINTER_SKYRIMSE(TDecrementRef, decrementRef, 36171);

    if (apItem)
        TiltedPhoques::ThisCall(decrementRef, this, apItem);
}

int32_t BGSCreatedObjectManager::GetRefCount(const AlchemyItem* apItem) noexcept
{
    if (!apItem)
        return -1;

    const auto base = reinterpret_cast<uintptr_t>(this);
    auto& lock = *reinterpret_cast<BSRecursiveLock*>(base + kLock);
    BSScopedLock<BSRecursiveLock> _(lock);

    // Check both maps rather than trusting the poison flag of an item that may already be stale.
    const int32_t potionCount = FindRefCount(base + kPotionMap, apItem->formID);
    if (potionCount >= 0)
        return potionCount;

    return FindRefCount(base + kPoisonMap, apItem->formID);
}
