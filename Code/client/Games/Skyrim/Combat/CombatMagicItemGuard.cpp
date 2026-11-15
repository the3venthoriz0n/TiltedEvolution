// Crash guards for combat magic items (spells, potions, scrolls, staves in an actor's combat inventory).
//
// The game crashed at SkyrimSE+0x832AE1 (CombatMagicItemData's constructor, ID 45319) and, with the first guard,
// with a stack overflow after a virtual call through a destroyed form. In both cases combat AI on a job thread read
// a magic item whose form had been destroyed. Two hooks, both on functions that already handle a "no item" result:
//
// - ID 45084, magic item scoring, float(CombatController*, MagicItem*, type, maxRange). A null MagicItem returns
//   the default score without building CombatMagicItemData.
// - ID 44898, creation of a combat magic item, CombatInventoryItem*(CombatInventory*, type, source, MagicItem*).
//   It returns nullptr when the item is not usable; every caller handles that.
//
// Both IDs exist with identical function sizes on 1.6.1170 and 1.7.104 (45084: 0x120 bytes, 44898: 0x220 bytes).
//
// SkipRemoteCombatItemScoring (default on): for actors another client owns (remote players, NPCs owned by another
// player) no magic combat items are created or scored. Their attacks, casts and potion use are replayed from the
// owner's client, and STR rebuilds their inventories from the network, which is exactly when their items get
// destroyed. CombatMagicItemGuard (default on): for local actors, a destroyed or invalid item is skipped and logged.

#include <Actor.h>
#include <Combat/CombatController.h>
#include <Forms/MagicItem.h>
#include <Games/ActorExtension.h>

#include <CrashGuards.h>
#include <FlightRecorder.h>

#include <spdlog/fmt/fmt.h>

#include <atomic>

namespace
{
// Form types a combat magic item can have.
constexpr uint8_t kEnchantment = 21;
constexpr uint8_t kSpell = 22;
constexpr uint8_t kScroll = 23;
constexpr uint8_t kIngredient = 30;
constexpr uint8_t kAlchemy = 46;

enum class Site : int32_t
{
    Scoring = 1,
    Creation = 2
};

struct ImageRange
{
    uintptr_t begin = 0;
    uintptr_t end = 0;

    bool Contains(uintptr_t aAddress, size_t aSize) const noexcept { return begin != 0 && aAddress >= begin && aAddress + aSize <= end; }
};

ImageRange s_gameImage{};

// The game image's address range, found from the address of a game function (STR maps the game into its own
// image, so this is the range of SkyrimTogether.exe including the game).
ImageRange FindImageRange(const void* apInsideImage) noexcept
{
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(apInsideImage, &info, sizeof(info)) || !info.AllocationBase)
        return {};

    const auto base = reinterpret_cast<uintptr_t>(info.AllocationBase);
    const auto* pDos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (pDos->e_magic != IMAGE_DOS_SIGNATURE)
        return {};

    const auto* pNt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + pDos->e_lfanew);
    if (pNt->Signature != IMAGE_NT_SIGNATURE)
        return {};

    return {base, base + pNt->OptionalHeader.SizeOfImage};
}

// True if [aAddress, aAddress + aSize) is committed, readable memory. The checks below must never fault: STR's
// vectored crash handler treats any access violation as a crash, even one a __try block would catch.
bool IsReadable(uintptr_t aAddress, size_t aSize) noexcept
{
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(reinterpret_cast<const void*>(aAddress), &info, sizeof(info)))
        return false;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
        return false;
    constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                                PAGE_EXECUTE_WRITECOPY;
    if ((info.Protect & kReadable) == 0)
        return false;
    return aAddress + aSize <= reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
}

// A form whose destructor already ran keeps a base-class vtable (the last destructor in the chain,
// ~BaseFormComponent, leaves BaseFormComponent's 4-slot vtable). The game's RTTI cast tells such an object apart:
// it no longer derives from MagicItem. The vtable and its RTTI locator are validated first so the cast cannot
// fault; the __try only catches the C++ exception the cast may throw. No C++ objects in this frame.
bool IsMagicItemByRtti(const MagicItem* apItem) noexcept
{
    __try
    {
        return Cast<MagicItem>(reinterpret_cast<TESForm*>(const_cast<MagicItem*>(apItem))) != nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool IsLiveMagicItem(const MagicItem* apItem, uint32_t& aFormId) noexcept
{
    aFormId = 0;
    const auto address = reinterpret_cast<uintptr_t>(apItem);
    if (!IsReadable(address, sizeof(TESForm)))
        return false;

    const auto* pForm = reinterpret_cast<const TESForm*>(apItem);
    aFormId = pForm->formID;

    const uintptr_t vtable = *reinterpret_cast<const uintptr_t*>(address);
    if (vtable == 0 || !s_gameImage.Contains(vtable - sizeof(uintptr_t), 2 * sizeof(uintptr_t)))
        return false;

    // The RTTI locator in front of the vtable must be in the image too, or this is not a vtable at all.
    const uintptr_t locator = *reinterpret_cast<const uintptr_t*>(vtable - sizeof(uintptr_t));
    if (!s_gameImage.Contains(locator, 0x18) || *reinterpret_cast<const uint32_t*>(locator) != 1)
        return false;

    switch (static_cast<uint8_t>(pForm->formType))
    {
    case kEnchantment:
    case kSpell:
    case kScroll:
    case kIngredient:
    case kAlchemy: break;
    default: return false;
    }

    return IsMagicItemByRtti(apItem);
}

// The actor that owns the combat controller, by the same rule the game uses: the cached attacker while handleCount
// is non-zero, otherwise the attacker handle.
Actor* GetCombatActor(const CombatController* apController) noexcept
{
    if (!apController)
        return nullptr;

    if (apController->handleCount != 0 && apController->pCachedAttacker.object)
        return apController->pCachedAttacker.object;

    TESObjectREFR* pRef = TESObjectREFR::GetByHandle(apController->attackerHandle);
    return pRef ? Cast<Actor>(pRef) : nullptr;
}

bool IsRemoteActor(Actor* apActor) noexcept
{
    if (!apActor)
        return false;

    const ActorExtension* pExtension = apActor->GetExtension();
    return pExtension && pExtension->IsRemote();
}

std::atomic<uint32_t> s_lastRemoteSkipActor{0};
std::atomic<uint32_t> s_invalidSkipCount{0};

// Returns true if the item must not be used: the actor is remote (and the flag is on) or the item is invalid.
bool ShouldSkip(const CombatController* apController, MagicItem* apItem, Site aSite) noexcept
{
    const CrashGuards::Flags& flags = CrashGuards::Get();
    if (!flags.SkipRemoteCombatItemScoring && !flags.CombatMagicItemGuard)
        return false;

    Actor* pActor = GetCombatActor(apController);
    const uint32_t actorId = pActor ? pActor->formID : 0;

    if (flags.SkipRemoteCombatItemScoring && IsRemoteActor(pActor))
    {
        // Record each remote actor once in a row rather than on every AI update.
        if (s_lastRemoteSkipActor.exchange(actorId, std::memory_order_relaxed) != actorId)
            FlightRecorder::Record(FlightRecorder::Event::GuardSkipRemoteActor, actorId, 0, static_cast<int32_t>(aSite), 0, true);
        return true;
    }

    uint32_t formId = 0;
    if (apItem && flags.CombatMagicItemGuard && !IsLiveMagicItem(apItem, formId))
    {
        FlightRecorder::Record(FlightRecorder::Event::GuardSkipInvalidItem, actorId, formId, static_cast<int32_t>(aSite));

        // AI threads can hit the same stale item every update: log the first ones, then every 500th.
        const uint32_t count = s_invalidSkipCount.fetch_add(1, std::memory_order_relaxed);
        if (count < 20 || count % 500 == 0)
        {
            spdlog::warn("CombatMagicItemGuard: skipped destroyed magic item at {} (form id read {:X}) in the combat inventory of "
                         "actor {:X} ({}); total skipped {}",
                         fmt::ptr(apItem), formId, actorId, aSite == Site::Scoring ? "scoring" : "creation", count + 1);
            FlightRecorder::DumpForGuardSkip();
        }
        return true;
    }

    return false;
}
} // namespace

TP_THIS_FUNCTION(TScoreMagicItem, float, CombatController, MagicItem* apItem, int32_t aType, float aMaxRange);
static TScoreMagicItem* RealScoreMagicItem = nullptr;

float TP_MAKE_THISCALL(HookScoreMagicItem, CombatController, MagicItem* apItem, int32_t aType, float aMaxRange)
{
    // nullptr makes the game return its "no item" score without touching the item.
    if (apItem && ShouldSkip(apThis, apItem, Site::Scoring))
        apItem = nullptr;

    return TiltedPhoques::ThisCall(RealScoreMagicItem, apThis, apItem, aType, aMaxRange);
}

TP_THIS_FUNCTION(TCreateMagicItem, void*, void, uint32_t aType, void* apSource, MagicItem* apItem);
static TCreateMagicItem* RealCreateMagicItem = nullptr;

void* TP_MAKE_THISCALL(HookCreateMagicItem, void, uint32_t aType, void* apSource, MagicItem* apItem)
{
    // The combat inventory's first member is its CombatController (CommonLibSSE-NG RE::CombatInventory, and the game
    // code of this function reads it the same way).
    const auto* pController = apThis ? *static_cast<CombatController* const*>(apThis) : nullptr;

    // nullptr is the function's own "item not usable" result.
    if (apItem && ShouldSkip(pController, apItem, Site::Creation))
        return nullptr;

    return TiltedPhoques::ThisCall(RealCreateMagicItem, apThis, aType, apSource, apItem);
}

static TiltedPhoques::Initializer s_combatMagicItemGuardHooks(
    []()
    {
        // ID 45084: 1.7.104 0x82EAF0, 1.6.1170 0x819C10. ID 44898: 1.7.104 0x828180, 1.6.1170 0x8132A0.
        POINTER_SKYRIMSE(TScoreMagicItem, s_scoreMagicItem, 45084);
        POINTER_SKYRIMSE(TCreateMagicItem, s_createMagicItem, 44898);

        RealScoreMagicItem = s_scoreMagicItem.Get();
        RealCreateMagicItem = s_createMagicItem.Get();
        s_gameImage = FindImageRange(reinterpret_cast<const void*>(RealScoreMagicItem));

        TP_HOOK(&RealScoreMagicItem, HookScoreMagicItem);
        TP_HOOK(&RealCreateMagicItem, HookCreateMagicItem);
    });
