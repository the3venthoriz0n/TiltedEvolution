#pragma once

#include "MagicItem.h"

#include <Structs/Inventory.h>

struct TESObjectREFR;

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

// Returns a local item that matches the data, for adding it to an inventory. The item is always one the
// created-object manager keeps alive for the session: a cached Create() result that is still registered, or a
// new Create() result (which reuses a live identical item if the game has one).
AlchemyItem* ResolveItem(const Inventory::AlchemyData& acData) noexcept;

// Returns the item in this reference's inventory that matches the data, for removing or dropping it.
AlchemyItem* FindInContainer(TESObjectREFR* apRef, const Inventory::AlchemyData& acData) noexcept;

// Takes a created-object reference on every runtime-created potion or poison in the reference's inventory, so
// none of them can be destroyed while the inventory is rebuilt. Release with UnpinItems.
Vector<AlchemyItem*> PinDynamicItems(TESObjectREFR* apRef) noexcept;
void UnpinItems(const Vector<AlchemyItem*>& acPinned) noexcept;
} // namespace AlchemySync
