# NBT / Persistence Report

## How TerraMath persists data (Java)
**FACT**, from source:
- `TerrainData extends SavedData` — the active formula and all terrain/noise
  parameters are written to **world NBT** (`save`/`load` with `CompoundTag`),
  keyed `terramath_terrain`, so each world remembers its generation settings.
- `ModConfig` — defaults persisted as **JSON** at `config/terramath.json`.

## What this port does
- The **JSON config** is ported directly (`src/config/Config.*`): same fields,
  same defaults, round-trip unit-tested. This is the primary persistence layer
  and is sufficient for client-side use (the formula is applied at generation
  time from config / the live menu).
- The **world-NBT `SavedData`** path is **not** reproduced. Rationale:
  - Bedrock stores world data in a **LevelDB** database (not a single region
    NBT tree), using **Little-Endian NBT** for tag payloads. Writing custom keys
    into a live world's LevelDB from inside the process is invasive and
    version-sensitive, and is not required for terrain generation to work — the
    generator reads the active settings from the engine/config.
  - This is a **disclosed difference**, not an oversight (see FEATURE_PARITY.md).

## If per-world persistence is added later (design notes)
Bedrock NBT facts to respect (so a future implementation is correct):
- **Little-Endian NBT** on disk (tag id, then LE-encoded lengths/values); UTF-8
  string payloads with an LE `uint16` length prefix.
- **Network NBT** (if ever syncing over the wire) uses **VarInt / ZigZag**
  (Protobuf-style) lengths — different from the disk format. Don't mix the two.
- World data lives in **LevelDB** under the world folder (`db/`); custom data
  would go in a dedicated key, not by editing chunk sub-keys.
- Validate any writes against the actual Bedrock version before shipping; a
  malformed tag can corrupt a world.

Because this mod only **reads** settings to drive generation (it never needs to
write into world chunk NBT), no NBT serialization is required for the current
feature set, and none is shipped — avoiding the corruption risk entirely.

## Summary
| Aspect | Java TerraMath | This port |
|---|---|---|
| Defaults config | JSON (`config/terramath.json`) | JSON (`terramath.json`) ✅ |
| Per-world settings | world-NBT `SavedData` | not reproduced (disclosed) 🟡 |
| Network NBT | n/a | n/a |
