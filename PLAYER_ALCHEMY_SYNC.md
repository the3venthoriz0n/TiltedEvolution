# Potion and poison sync (potion-sync branch)

Player-brewed potions and poisons are created at runtime. Their form IDs are dynamic (`0xFFxxxxxx`) and
exist only in the owner's save, so STR could not move them between players: the receiving game found no
such form, logged `Object to add not found`, and the item was lost on the giving side.

## What changed

- `Inventory::AlchemyData` (shared encoding) describes a potion or poison by its kind and effects. It is
  serialized as part of every `Inventory::Entry`, so this changes the wire format.
- Entries for runtime-created potions use a **content hash of the effects** as their base ID
  (`ModId 0xFFFFFFFE`) instead of the local form ID, so the server merges identical potions and never
  confuses two players' unrelated dynamic IDs.
- The client (`Games/Skyrim/Forms/AlchemyItem.{h,cpp}`) reads a local potion's effects when it builds an
  entry (`AlchemySync::SetEntryBase`) and, when applying one, finds or creates the matching local item
  through the game's `BGSCreatedObjectManager::AddPotion` / `AddPoison`
  (`AlchemySync::ResolveItem`). Already-seen potions are cached by content key, as form IDs.
- The server only stores and relays entries, so it needs no logic change beyond the shared format.

## Compatibility

The wire format changed, so **server and clients must all run builds from this branch**. They report the
version string `v1.8.4-potions1` (set by the CI), and the server refuses clients with any other version.

## Known gaps

- Poisons applied to a weapon (`ExtraPoison`) are not covered, only potions and poisons as items.
- Item names, icons and value come from what the game generates for a created potion; this is unverified.
- The `AlchemyItem` layout (poison flag at `0x13C`) and the function IDs 36167 / 36168 follow
  CommonLibSSE-NG and must be confirmed on each supported Skyrim version.
- Created items stay in the save for good, one per distinct potion.
