# Modifications to Skyrim Together Reborn (potion-sync client)

This is a **modified version** of Skyrim Together Reborn ("Tilted Online", codename TiltedEvolution), licensed
under the GNU General Public License, version 3 or (at your option) any later version. See `LICENSE`.

**The changes below were made by the3venthoriz0n (the fork owner), with AI assistance from Claude (Anthropic).
They were not made, reviewed or endorsed by the upstream authors (the Tilted Online / Skyrim Together Reborn
team). Do not report problems with this build to the upstream team; report them at
https://github.com/the3venthoriz0n/TiltedEvolution/issues.**

- Upstream: https://github.com/tiltedphoques/TiltedEvolution
- This fork: https://github.com/the3venthoriz0n/TiltedEvolution
- Branch described here: `potion-sync-crashfix` (client)
- Fork point: upstream commit `54b3a858` ("Merge pull request #930 from tiltedphoques/dev", 2026-10-22,
  upstream release v1.8.4). Every commit after it is by the fork owner.
- This notice was written on 2026-11-15 and covers commits up to `c80e248b` (2026-11-14). Later commits on the branch
  are listed by `git log 54b3a858..HEAD`.

Dates are commit dates (author date, yyyy-mm-dd). Compare with upstream yourself using
`git diff 54b3a858 <commit>`; the totals from the fork point are 28 files changed, 1740 insertions, 29 deletions.

## Changes

### Player-brewed potion and poison sync (2026-11-07 to 2026-11-14)

Potions and poisons brewed by a player have dynamic form IDs that exist only in that player's save, so stock STR
could not move them between players. These commits sync them. Design notes: `PLAYER_ALCHEMY_SYNC.md`.

- `ca8f2615` 2026-11-08 Sync player-brewed potions and poisons between players. Adds `Inventory::AlchemyData` to the
  shared encoding (**this changes the network wire format**, so clients of this build only work with a server built
  from the same line), `Code/client/Games/Skyrim/Forms/AlchemyItem.{h,cpp}`, changes to `Actor.cpp`,
  `PlayerCharacter.cpp`, `TESObjectREFR.cpp`, `Code/encoding/Structs/Inventory.{h,cpp}`, the test
  `Code/tests/alchemy.cpp`, and the CI workflow `windows-potion-build.yml`.
- `bdfe5573` 2026-11-08 Fix missing includes in the alchemy test and `Inventory.h`.
- `b02e068f` 2026-11-14 Stop caching unreferenced local potions; resolve through `Create()`; find removals in the
  container (adds `Code/client/Games/BGSCreatedObjectManager.{h,cpp}`).
- `f78747f4` 2026-11-14 Keep runtime-created potions alive while STR rebuilds an inventory.
- `15bf5ae3` 2026-11-14 Log potions destroyed when an inventory rebuild releases them.

### Server-side inventory transfers (2026-11-07)

- `0e6fed5f` 2026-11-07 Opt-in player-to-player inventory transfers (`Code/server/Services/InventoryService.cpp`,
  setting `Gameplay:bAllowPlayerInventoryTransfers`). This is server code; it ships in the client source tree
  because client and server share one repository.

### Crash fixes

- `358f2418` 2026-11-14 Skip destroyed magic items in combat scoring instead of crashing
  (`Code/client/Games/Skyrim/Combat/CombatMagicItemGuard.cpp`).
- `fce52a08` 2026-11-14 CombatMagicItemGuard: also reject destroyed forms.
- `61a3a133` 2026-11-14 Skip combat magic items for remotely owned actors; shared crash guard configuration. Adds
  `Code/client/CrashGuards.{h,cpp}` and `Code/client/FlightRecorder.{h,cpp}`. The guards are switched by the config file
  `Data/SkyrimTogetherReborn/config/CrashGuards.ini`, which the client creates with defaults on first use
  (keys `CombatMagicItemGuard`, `SkipRemoteCombatItemScoring`, `FlightRecorder`, `DebugLogging`).
- `474ddb27` 2026-11-14 Feed the flight recorder (a ring buffer of recent events written to `tp_client.log` on a crash or
  a guard skip) and log a "CLIENT BUILD" block (version, commit hash, flags, game version, loaded SKSE plugins).
  Touches `CrashHandler.cpp`, `Actor.cpp`, `TESObjectREFR.cpp`, `CharacterService.cpp`, `TransportService.cpp`,
  `TiltedOnlineApp.cpp`, and `xmake.lua` (adds a `BUILD_COMMIT_HASH` define to the generated `BranchInfo.h`).

### Version string

- `ca8f2615` (2026-11-08) adds a `version-override` input to `.github/workflows/windows.yml`, and
  `windows-potion-build.yml` uses it, so that this client reports the version string `v1.8.4-potions1` instead of
  the one derived from `git describe`. The server refuses clients whose version string differs; this build is meant to
  be used with a server reporting the same string. The string does not mean upstream released a "potions" version.
- `c80e248b` 2026-11-14 `release-potion-client.yml`, a tag-triggered workflow that builds the client and publishes the zip
  and a debug-symbols zip as a GitHub release (tags `potions-v*`).

### Build and release (2026-11-07 to 2026-11-15)

- `cb23800e` 2026-11-07 CI workflow to build the patched server image to GHCR; `cedb9bb9` 2026-11-07 Dockerfile
  `VERSION_OVERRIDE` build argument (these are also in the server branch).
- 2026-11-15 (this documentation change): `LICENSE`, `MODIFICATIONS.md` and `SOURCE.txt` are packaged into the client
  zip under `Docs/SkyrimTogetherReborn-potions/`; release notes link to the source commit; README credits added.

## Not changed

The remaining source is upstream's. Copyright notices in upstream files are untouched. New files written for this
fork carry the same licence (GPL-3.0-or-later).
