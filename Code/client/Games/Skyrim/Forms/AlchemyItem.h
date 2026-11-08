#pragma once

#include "MagicItem.h"

#include <Structs/Inventory.h>

struct AlchemyItem : MagicItem
{
    enum Flags : uint32_t
    {
        kFoodItem = 1u << 1,
        kMedicine = 1u << 16,
        kPoison = 1u << 17
    };

    bool IsPoison() const noexcept { return (uiFlags & kPoison) != 0; }

    // Creates a new potion or poison from its effects, as the alchemy menu does.
    static AlchemyItem* Create(const Inventory::AlchemyData& aData) noexcept;

    // Layout from CommonLibSSE-NG's RE::AlchemyItem (MagicItem is followed by TESModelTextureSwap, TESIcon,
    // BGSMessageIcon, TESWeightForm, BGSEquipType, BGSDestructibleObjectForm and BGSPickupPutdownSounds).
    uint8_t padComponents[0xA8]; // 090
    int32_t iCostOverride;       // 138 - ENIT
    uint32_t uiFlags;            // 13C
    uint8_t padTail[0x28];       // 140
};

static_assert(sizeof(AlchemyItem) == 0x168);

// Keeps runtime-created potions and poisons in sync between players.
//
// Their form IDs only exist in the owner's save, so inventory entries describe them by their effects
// (Inventory::AlchemyData) and the receiving game finds or creates an equivalent item.
namespace AlchemySync
{
// Sets the entry's base ID from a form. For a runtime-created potion or poison the entry also gets its
// AlchemyData, and BaseId becomes a hash of that data. Any other form is handled as before.
void SetEntryBase(Inventory::Entry& arEntry, TESForm* apForm) noexcept;

// Returns the local item that matches the data, creating it if aCreate is set and none exists yet.
AlchemyItem* ResolveItem(const Inventory::AlchemyData& acData, bool aCreate) noexcept;
} // namespace AlchemySync
