#pragma once

// A small in-memory ring buffer of recent events (actor lifecycle, inventory rebuilds, potion sync, crash guard
// skips). Recording is lock-free and allocation-free, so it is safe from any thread, including AI job threads.
// The buffer is written to tp_client.log as a "FLIGHT RECORDER DUMP" block on a crash and when a guard skips.
namespace FlightRecorder
{
enum class Event : uint16_t
{
    ActorSpawned,          // actor, item = base form, a = 1 if the actor has a TESNPC base
    ActorRemoved,          // actor
    ActorDeleted,          // actor (temporary actor deleted by STR)
    RemoteActorReceived,   // actor became remotely managed
    LocalActorReceived,    // actor became locally owned
    OwnershipTransferred,  // actor (we handed it to another client)
    SetInventoryBegin,     // actor, a = inventory entries, b = potions/poisons pinned
    SetInventoryEnd,       // actor, b = potions/poisons released
    PotionCreated,         // item = new or reused created potion, a = effect count
    PotionCacheHit,        // item
    PotionCacheStale,      // item = cached form that is no longer registered
    PotionLastRefReleased, // actor, item (game destroys the potion)
    PotionUnregisteredHeld,// actor, item, a = count
    PotionRefCountOne,     // actor, item, a = count
    GuardSkipInvalidItem,  // actor, item = form id read from the item (may be garbage), a = 1 scoring / 2 add
    GuardSkipRemoteActor,  // actor, a = 1 scoring / 2 add
    Count
};

void Record(Event aEvent, uint32_t aActorId, uint32_t aItemId = 0, int32_t aA = 0, int32_t aB = 0, bool aRemote = false) noexcept;

// Writes the last aCount events (at most 300) to the log. Safe to call from the crash handler: no heap allocation
// in this code, and exceptions raised while formatting are swallowed.
void Dump(const char* apReason, uint32_t aCount = 300) noexcept;

// Dump after a guard skip, rate limited (first skip, then at most once a minute, 10 times per session; every 5 s
// when the log level is debug).
void DumpForGuardSkip() noexcept;
} // namespace FlightRecorder
