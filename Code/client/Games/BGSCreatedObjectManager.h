#pragma once

struct AlchemyItem;

// The game's owner of runtime-created potions, poisons and enchantments. Created potions and poisons are
// reference counted here: when an item's count reaches 0 the game unregisters it and destroys the form,
// and IncrementRef does nothing for an item that is no longer registered.
struct BGSCreatedObjectManager
{
    static BGSCreatedObjectManager* Get() noexcept;

    void IncrementRef(AlchemyItem* apItem) noexcept;
    void DecrementRef(AlchemyItem* apItem) noexcept;

    // Returns the item's reference count, or -1 if it is not registered (its form is gone or about to be).
    int32_t GetRefCount(const AlchemyItem* apItem) noexcept;
};
