#pragma once

// Defensive switches, read once from Data/SkyrimTogetherReborn/config/CrashGuards.ini. The file is created with
// the defaults on first use, and keys missing from an older file are added with their defaults.
namespace CrashGuards
{
struct Flags
{
    // Skip magic items whose form was destroyed while a combat inventory still points at them.
    bool CombatMagicItemGuard = true;
    // Do not create or score combat magic items for actors this client does not own (remote players, NPCs
    // owned by another client). Their combat is driven by the owner's client.
    bool SkipRemoteCombatItemScoring = true;
    // Keep a ring buffer of recent events and write it to tp_client.log on a crash or a guard skip.
    bool FlightRecorder = true;
    // Also log every flight recorder event as its own info line.
    bool DebugLogging = false;
};

const Flags& Get() noexcept;

// Logs the "CLIENT BUILD" block (version, commit, flags, game version, loaded SKSE plugins). Called at startup and
// on the first server connection (when SKSE plugins are loaded); logs at most twice.
void LogClientBuild(const char* apWhen) noexcept;
} // namespace CrashGuards
