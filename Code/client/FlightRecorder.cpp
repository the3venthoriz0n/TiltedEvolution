#include <TiltedOnlinePCH.h>

#include <FlightRecorder.h>
#include <CrashGuards.h>

#include <atomic>
#include <cstdio>

namespace
{
constexpr uint32_t kCapacity = 512; // power of two
constexpr uint32_t kMaxDump = 300;

struct Entry
{
    std::atomic<uint32_t> Sequence{0}; // index + 1 once complete, 0 while being written
    uint64_t FileTime;
    uint32_t ThreadId;
    uint16_t Kind;
    uint8_t Remote;
    uint8_t Pad;
    uint32_t ActorId;
    uint32_t ItemId;
    int32_t A;
    int32_t B;
};

Entry s_entries[kCapacity];
std::atomic<uint32_t> s_next{0};

const char* EventName(uint16_t aKind) noexcept
{
    using FlightRecorder::Event;
    switch (static_cast<Event>(aKind))
    {
    case Event::ActorSpawned: return "ActorSpawned";
    case Event::ActorRemoved: return "ActorRemoved";
    case Event::ActorDeleted: return "ActorDeleted";
    case Event::RemoteActorReceived: return "RemoteActorReceived";
    case Event::LocalActorReceived: return "LocalActorReceived";
    case Event::OwnershipTransferred: return "OwnershipTransferred";
    case Event::SetInventoryBegin: return "SetInventoryBegin";
    case Event::SetInventoryEnd: return "SetInventoryEnd";
    case Event::PotionCreated: return "PotionCreated";
    case Event::PotionCacheHit: return "PotionCacheHit";
    case Event::PotionCacheStale: return "PotionCacheStale";
    case Event::PotionLastRefReleased: return "PotionLastRefReleased";
    case Event::PotionUnregisteredHeld: return "PotionUnregisteredHeld";
    case Event::PotionRefCountOne: return "PotionRefCountOne";
    case Event::GuardSkipInvalidItem: return "GuardSkipInvalidItem";
    case Event::GuardSkipRemoteActor: return "GuardSkipRemoteActor";
    default: return "Unknown";
    }
}

// Formats one entry into a caller-provided buffer (no allocation).
void FormatEntry(char* apBuffer, size_t aSize, uint32_t aIndex, const Entry& acEntry) noexcept
{
    FILETIME utc{};
    utc.dwLowDateTime = static_cast<DWORD>(acEntry.FileTime);
    utc.dwHighDateTime = static_cast<DWORD>(acEntry.FileTime >> 32);
    FILETIME local{};
    SYSTEMTIME time{};
    FileTimeToLocalFileTime(&utc, &local);
    FileTimeToSystemTime(&local, &time);

    std::snprintf(apBuffer, aSize, "FR #%u %02u:%02u:%02u.%03u tid %u %-22s actor %08X item %08X a %d b %d%s", aIndex,
                  time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, acEntry.ThreadId, EventName(acEntry.Kind),
                  acEntry.ActorId, acEntry.ItemId, acEntry.A, acEntry.B, acEntry.Remote ? " remote" : "");
}

void DumpImpl(const char* apReason, uint32_t aCount) noexcept
{
    const uint32_t next = s_next.load(std::memory_order_acquire);
    uint32_t count = aCount > kMaxDump ? kMaxDump : aCount;
    if (count > next)
        count = next;

    spdlog::critical("===== FLIGHT RECORDER DUMP ({}) - last {} of {} events =====", apReason ? apReason : "", count, next);

    char line[256];
    for (uint32_t index = next - count; index != next; ++index)
    {
        const Entry& entry = s_entries[index & (kCapacity - 1)];
        if (entry.Sequence.load(std::memory_order_acquire) != index + 1)
            continue; // overwritten or still being written

        Entry copy;
        copy.FileTime = entry.FileTime;
        copy.ThreadId = entry.ThreadId;
        copy.Kind = entry.Kind;
        copy.Remote = entry.Remote;
        copy.ActorId = entry.ActorId;
        copy.ItemId = entry.ItemId;
        copy.A = entry.A;
        copy.B = entry.B;
        if (entry.Sequence.load(std::memory_order_acquire) != index + 1)
            continue;

        FormatEntry(line, sizeof(line), index, copy);
        spdlog::critical("{}", static_cast<const char*>(line));
    }

    spdlog::critical("===== END FLIGHT RECORDER DUMP =====");
    spdlog::default_logger()->flush();
}

// No C++ objects in this frame, so structured exception handling is allowed here.
void SafeDump(const char* apReason, uint32_t aCount) noexcept
{
    __try
    {
        DumpImpl(apReason, aCount);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}
} // namespace

void FlightRecorder::Record(Event aEvent, uint32_t aActorId, uint32_t aItemId, int32_t aA, int32_t aB, bool aRemote) noexcept
{
    const CrashGuards::Flags& flags = CrashGuards::Get();
    if (!flags.FlightRecorder && !flags.DebugLogging)
        return;

    uint64_t fileTime = 0;
    {
        FILETIME now{};
        GetSystemTimePreciseAsFileTime(&now);
        fileTime = (static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
    }

    const uint32_t index = s_next.fetch_add(1, std::memory_order_acq_rel);
    Entry& entry = s_entries[index & (kCapacity - 1)];
    entry.Sequence.store(0, std::memory_order_release);
    entry.FileTime = fileTime;
    entry.ThreadId = GetCurrentThreadId();
    entry.Kind = static_cast<uint16_t>(aEvent);
    entry.Remote = aRemote ? 1 : 0;
    entry.ActorId = aActorId;
    entry.ItemId = aItemId;
    entry.A = aA;
    entry.B = aB;
    entry.Sequence.store(index + 1, std::memory_order_release);

    if (flags.DebugLogging)
    {
        char line[256];
        FormatEntry(line, sizeof(line), index, entry);
        spdlog::info("{}", static_cast<const char*>(line));
    }
}

void FlightRecorder::Dump(const char* apReason, uint32_t aCount) noexcept
{
    if (!CrashGuards::Get().FlightRecorder)
        return;

    SafeDump(apReason, aCount);
}

void FlightRecorder::DumpForGuardSkip() noexcept
{
    static std::atomic<uint64_t> s_lastDump{0};
    static std::atomic<uint32_t> s_dumps{0};

    const bool debug = spdlog::get_level() <= spdlog::level::debug;
    const uint64_t now = GetTickCount64();
    const uint64_t last = s_lastDump.load(std::memory_order_relaxed);
    const uint64_t interval = debug ? 5000 : 60000;

    if (last != 0 && now - last < interval)
        return;
    if (!debug && s_dumps.load(std::memory_order_relaxed) >= 10)
        return;

    uint64_t expected = last;
    if (!s_lastDump.compare_exchange_strong(expected, now))
        return; // another thread is dumping

    s_dumps.fetch_add(1, std::memory_order_relaxed);
    Dump("guard skip");
}
