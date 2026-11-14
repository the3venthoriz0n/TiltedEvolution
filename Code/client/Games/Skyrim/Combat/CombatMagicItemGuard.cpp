// Turns the combat crash on a freed magic item (SkyrimSE+0x832AE1 on 1.7.104, CombatMagicItemData's constructor
// calling into a destroyed runtime-created potion) into a log line.
//
// The hook sits on the constructor's caller, the combat inventory's magic item scoring function (ID 45084, the same
// function on 1.6.1170 and 1.7.104). It is called as float(CombatController*, MagicItem*, type, maxRange) and already
// treats a null MagicItem as "no item": it returns its default score without building CombatMagicItemData. So an
// invalid item is passed on as nullptr and no fake object is needed.
//
// Enabled by CombatMagicItemGuard in Data/SkyrimTogetherReborn/config/CrashGuards.ini (default on, file created on
// first use like CellRespawnOverrides.ini).

#include <Actor.h>
#include <Combat/CombatController.h>
#include <Forms/MagicItem.h>

#include <base/simpleini/SimpleIni.h>
#include <spdlog/fmt/fmt.h>

#include <atomic>
#include <filesystem>

namespace
{
constexpr char kConfigPathName[] = "Data/SkyrimTogetherReborn/config";
constexpr char kSettingsFileName[] = "CrashGuards.ini";
constexpr char kSectionName[] = "CrashGuards";
constexpr char kGuardKey[] = "CombatMagicItemGuard";
constexpr char kGuardComment[] =
    "; Skip magic items (usually runtime-created potions) that were destroyed while an actor's combat inventory\n"
    "; still referenced them, and log them instead of crashing. true or false.";

// Form types a combat magic item can have.
constexpr uint8_t kEnchantment = 21;
constexpr uint8_t kSpell = 22;
constexpr uint8_t kScroll = 23;
constexpr uint8_t kIngredient = 30;
constexpr uint8_t kAlchemy = 46;

bool LoadGuardEnabled() noexcept
{
    try
    {
        const auto path = std::filesystem::current_path() / kConfigPathName / kSettingsFileName;

        std::error_code error{};
        if (!std::filesystem::exists(path, error) && !error)
        {
            std::filesystem::create_directories(path.parent_path(), error);
            CSimpleIni defaults(true);
            defaults.SetBoolValue(kSectionName, kGuardKey, true, kGuardComment);
            defaults.SaveFile(path.c_str(), true);
        }

        CSimpleIni ini(true);
        if (ini.LoadFile(path.c_str()) != SI_Error::SI_OK)
            return true;

        return ini.GetBoolValue(kSectionName, kGuardKey, true);
    }
    catch (...)
    {
        return true;
    }
}

bool IsGuardEnabled() noexcept
{
    static const bool s_enabled = []()
    {
        const bool enabled = LoadGuardEnabled();
        spdlog::info("{} is {}", kGuardKey, enabled ? "on" : "off");
        return enabled;
    }();
    return s_enabled;
}

struct ImageRange
{
    uintptr_t begin = 0;
    uintptr_t end = 0;
};

// The game image's address range, found from the address of a game function (the game may be mapped by STR's
// loader rather than being the process's main module).
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

ImageRange s_gameImage{};

// Reads the item's vtable and form type without trusting the memory. Kept free of C++ objects for __try.
bool ProbeMagicItem(const void* apItem, uintptr_t& aVtable, uint8_t& aFormType, uint32_t& aFormId) noexcept
{
    __try
    {
        const auto* pForm = static_cast<const TESForm*>(apItem);
        aVtable = *static_cast<const uintptr_t*>(apItem);
        aFormType = static_cast<uint8_t>(pForm->formType);
        aFormId = pForm->formID;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool IsLiveMagicItem(const MagicItem* apItem, uint32_t& aFormId) noexcept
{
    uintptr_t vtable = 0;
    uint8_t formType = 0;
    aFormId = 0;
    if (!ProbeMagicItem(apItem, vtable, formType, aFormId))
        return false;

    if (vtable == 0)
        return false;

    if (s_gameImage.begin != 0 && (vtable < s_gameImage.begin || vtable >= s_gameImage.end))
        return false;

    switch (formType)
    {
    case kEnchantment:
    case kSpell:
    case kScroll:
    case kIngredient:
    case kAlchemy: return true;
    default: return false;
    }
}

uint32_t GetAttackerFormId(const CombatController* apController) noexcept
{
    // Same rule the game uses: the cached attacker is valid while handleCount is non-zero.
    if (apController && apController->handleCount != 0 && apController->pCachedAttacker.object)
        return apController->pCachedAttacker.object->formID;
    return 0;
}

std::atomic<uint32_t> s_skipCount{0};
} // namespace

TP_THIS_FUNCTION(TScoreMagicItem, float, CombatController, MagicItem* apItem, int32_t aType, float aMaxRange);
static TScoreMagicItem* RealScoreMagicItem = nullptr;

float TP_MAKE_THISCALL(HookScoreMagicItem, CombatController, MagicItem* apItem, int32_t aType, float aMaxRange)
{
    uint32_t formId = 0;
    if (apItem && IsGuardEnabled() && !IsLiveMagicItem(apItem, formId))
    {
        // AI threads can hit the same stale item every update: log the first ones, then every 500th.
        const uint32_t count = s_skipCount.fetch_add(1, std::memory_order_relaxed);
        if (count < 20 || count % 500 == 0)
        {
            spdlog::warn("CombatMagicItemGuard: skipped destroyed magic item at {} (form id read {:X}, type {}) in the combat "
                         "inventory of actor {:X} (attacker handle {:X}); total skipped {}",
                         fmt::ptr(apItem), formId, aType, GetAttackerFormId(apThis), apThis ? apThis->attackerHandle : 0, count + 1);
        }

        apItem = nullptr;
    }

    return TiltedPhoques::ThisCall(RealScoreMagicItem, apThis, apItem, aType, aMaxRange);
}

static TiltedPhoques::Initializer s_combatMagicItemGuardHooks(
    []()
    {
        // ID 45084: 1.7.104 0x82EAF0, 1.6.1170 0x819C10 (0x120 bytes in both).
        POINTER_SKYRIMSE(TScoreMagicItem, s_scoreMagicItem, 45084);

        RealScoreMagicItem = s_scoreMagicItem.Get();
        s_gameImage = FindImageRange(reinterpret_cast<const void*>(RealScoreMagicItem));

        TP_HOOK(&RealScoreMagicItem, HookScoreMagicItem);
    });
