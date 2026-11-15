#include <TiltedOnlinePCH.h>

#include <CrashGuards.h>

#include <BranchInfo.h>
#include <VersionDb.h>

#include <base/simpleini/SimpleIni.h>

#include <TlHelp32.h>

#include <atomic>
#include <cwctype>
#include <filesystem>
#include <string>

#ifndef BUILD_COMMIT_HASH
#define BUILD_COMMIT_HASH "unknown"
#endif

namespace
{
constexpr char kConfigPathName[] = "Data/SkyrimTogetherReborn/config";
constexpr char kSettingsFileName[] = "CrashGuards.ini";
constexpr char kSectionName[] = "CrashGuards";

struct FlagSpec
{
    const char* Key;
    bool CrashGuards::Flags::*Member;
    const char* Comment;
};

const FlagSpec kFlagSpecs[] = {
    {"CombatMagicItemGuard", &CrashGuards::Flags::CombatMagicItemGuard,
     "; Skip magic items (usually runtime-created potions) that were destroyed while an actor's combat inventory\n"
     "; still referenced them, and log them instead of crashing. true or false."},
    {"SkipRemoteCombatItemScoring", &CrashGuards::Flags::SkipRemoteCombatItemScoring,
     "; Do not create or score combat magic items (spells, potions, scrolls, staves) for actors another client owns\n"
     "; (remote players, NPCs owned by another player). Their combat comes from the owner's client. true or false."},
    {"FlightRecorder", &CrashGuards::Flags::FlightRecorder,
     "; Keep the last 512 actor/inventory/potion events and write them to tp_client.log on a crash or guard skip."},
    {"DebugLogging", &CrashGuards::Flags::DebugLogging,
     "; Also log every flight recorder event as an info line (verbose)."},
};

CrashGuards::Flags LoadFlags() noexcept
{
    CrashGuards::Flags flags{};

    try
    {
        const auto path = std::filesystem::current_path() / kConfigPathName / kSettingsFileName;

        std::error_code error{};
        std::filesystem::create_directories(path.parent_path(), error);

        CSimpleIni ini(true);
        const bool exists = std::filesystem::exists(path, error);
        if (exists && ini.LoadFile(path.c_str()) != SI_Error::SI_OK)
        {
            spdlog::warn("Failed to read {}, using the default crash guard settings", path.string());
            return flags;
        }

        // Add any key the file does not have yet, with its default, so the file always lists every switch.
        bool changed = false;
        for (const FlagSpec& spec : kFlagSpecs)
        {
            if (!ini.GetValue(kSectionName, spec.Key))
            {
                ini.SetBoolValue(kSectionName, spec.Key, flags.*spec.Member, spec.Comment);
                changed = true;
            }
            flags.*spec.Member = ini.GetBoolValue(kSectionName, spec.Key, flags.*spec.Member);
        }

        if (changed)
            ini.SaveFile(path.c_str(), true);
    }
    catch (...)
    {
    }

    return flags;
}

// Names of loaded DLLs from Data\SKSE\Plugins, comma separated.
std::string ListSksePlugins() noexcept
{
    std::string result;
    try
    {
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
        if (snapshot == INVALID_HANDLE_VALUE)
            return "(module list unavailable)";

        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        for (BOOL ok = Module32FirstW(snapshot, &entry); ok; ok = Module32NextW(snapshot, &entry))
        {
            std::wstring path = entry.szExePath;
            for (auto& c : path)
                c = static_cast<wchar_t>(std::towlower(c));

            if (path.find(L"\\skse\\plugins\\") == std::wstring::npos)
                continue;

            if (!result.empty())
                result += ", ";
            result += std::filesystem::path(entry.szModule).string();
        }
        CloseHandle(snapshot);
    }
    catch (...)
    {
    }

    return result.empty() ? std::string("(none loaded)") : result;
}
} // namespace

const CrashGuards::Flags& CrashGuards::Get() noexcept
{
    static const Flags s_flags = LoadFlags();
    return s_flags;
}

void CrashGuards::LogClientBuild(const char* apWhen) noexcept
{
    static std::atomic<uint32_t> s_logged{0};
    if (s_logged.fetch_add(1) >= 2)
        return;

    try
    {
        const Flags& flags = Get();
#ifdef TP_SKYRIM_1_6_1170
        constexpr char kTarget[] = "1.6.1170";
#else
        constexpr char kTarget[] = "1.7.104";
#endif
        spdlog::info("===== CLIENT BUILD ({}) =====", apWhen ? apWhen : "");
        spdlog::info("Version {} (branch {}, commit {}), built for Skyrim {}, running {}", BUILD_COMMIT, BUILD_BRANCH,
                     BUILD_COMMIT_HASH, kTarget, VersionDb::Get().GetLoadedVersionString());
        spdlog::info("CrashGuards.ini: CombatMagicItemGuard={} SkipRemoteCombatItemScoring={} FlightRecorder={} DebugLogging={}",
                     flags.CombatMagicItemGuard, flags.SkipRemoteCombatItemScoring, flags.FlightRecorder, flags.DebugLogging);
        spdlog::info("SKSE plugins: {}", ListSksePlugins());
        spdlog::info("===== END CLIENT BUILD =====");
    }
    catch (...)
    {
    }
}
