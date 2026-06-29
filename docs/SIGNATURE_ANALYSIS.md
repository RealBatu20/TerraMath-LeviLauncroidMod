# Terrain targeting — automatic, with a manual fallback

**You normally do nothing here.** At load time the mod auto-detects Minecraft's
terrain-generation function and hooks it (`src/hook/AutoResolver.cpp`):

1. It finds `libminecraftpe.so` in the process (`dl_iterate_phdr`).
2. It walks the loaded module's dynamic symbol table, **demangles** each function
   name (`abi::__cxa_demangle`), and matches the configured hints
   (`terrainSymbolHints`, default targets the overworld height function).
3. The matched address is hooked; a prologue byte pattern is auto-generated for
   display/logging only.

All of this is editable live in the menu's **Advanced** section (toggle
auto-detect, edit the symbol hints, choose height/density mode). No byte
signature is required in the normal case.

The rest of this document is the **fallback** for the uncommon case where a
particular Bedrock build is stripped of the relevant symbols, so auto-detect
can't find them. Then you locate the function in IDA and either tighten the hint
or paste a signature. Nothing here is guessed; you generate it against your own
binary.

> Default config ships `terrainAutoDetect: true` and an empty `terrainSignature`.
> If auto-detect succeeds (the common case) the manual signature is never used.

## What you need
- The `libminecraftpe.so` (arm64-v8a) for the Bedrock version you target — pull
  it from your device APK: `…/lib/arm64-v8a/libminecraftpe.so`.
- A disassembler: **IDA Pro** (with the IDA Pro MCP bridge if you drive it from
  an assistant) or Ghidra. The steps below are written for IDA; Ghidra is
  equivalent.

## Step 1 — Identify the generation function
Bedrock terrain for the overworld is produced by the noise-based chunk
generator. Good anchors to pivot from (use whichever your symbol/strings table
exposes):

1. **String search** for generator-related literals, then xref to the owning
   class methods. Useful needles seen across versions: `"overworld"`,
   `"NoiseChunkGenerator"`, `"ChunkSource"`, `"generateChunk"`,
   `"buildSurface"`, biome/noise setting names.
2. **RTTI / vtables.** Search the read-only segment for type-name strings such
   as `NoiseBasedChunkGenerator` / `ChunkGenerator` / `OverworldGenerator`,
   follow to the vtable, and enumerate virtual methods. The per-column height /
   per-voxel density method lives here.
3. **Cross-reference the heightmap writer.** Find where the chunk's heightmap or
   the column min/max solid Y is written and walk up to the function that
   computes it.

Pick the function that, given world X/Z (and possibly Y), returns or writes the
terrain height/density. Confirm by decompiling: it should read noise/biome state
and produce a height or density value.

## Step 2 — Confirm the prototype
Decompile the chosen function and match it to one of the two seams:
- **Column height:** `int f(self, int x, int z)` → use `FormulaEngine::surfaceY`
  (the default `TerrainColumnHeightFn` in `TerrainHook.cpp`).
- **Per-voxel density/solidity:** `… f(self, …, x, y, z)` → switch to the
  density path (`FormulaEngine::compute`, solid when `>= 0`). See HOOK_ANALYSIS.md.

Adjust `TerrainColumnHeightFn` / the detour parameter mapping in
`src/hook/TerrainHook.cpp` to match the real argument order if needed.

## Step 3 — Generate a stable byte signature
At the function's entry, build a pattern from the first ~16–32 bytes, wildcarding
relocated/immediate operands so it survives minor rebuilds:

- IDA: select the entry bytes → see the hex view; replace bytes that belong to
  relative branches, `ADRP/ADD` immediates, and literal-pool offsets with `??`.
- Verify uniqueness: the pattern must match **exactly one** site
  (`Search > sequence of bytes`, ensure a single hit).

Format the pattern as space-separated hex with `??` wildcards, e.g.:
```
1F 20 03 D5 ?? ?? ?? 94 E0 03 1F AA ?? ?? ?? ??
```
(Illustrative format only — yours will differ; do not copy this literally.)

The exact wildcard/format conventions accepted by
`pl::signature::pl_resolve_signature` are documented at
<https://liteldev.github.io/LeviLaunchroid/api/signature>. Match that syntax.

## Step 4 — Wire it in
Two ways, both with no recompile:

**In-game (fastest):** open the TerraMath menu → *Terrain hook (binary)* → paste
the pattern into **Signature**, set **Hook mode** (`height` or `density`, per
Step 2), tap **Install terrain hook**. The status line / logcat reports success.

**Config file:** edit `…/games/com.mojang/terramath/terramath.json`:
```json
"terrainSignature": "1F 20 03 D5 ?? ?? ?? 94 ...",
"terrainHookMode": "height"
```
Restart Minecraft. On success the log shows:
```
TerraMath: Terrain hook installed at 0x... (signature resolved).
```
(`adb logcat -s TerraMath`). If it fails to resolve, the pattern doesn't match
this build — widen wildcards or re-derive against the running version.

## Step 5 — Validate
- Generate a fresh world with an active formula (e.g. `sin(x)*cos(z)*10`).
- Confirm the surface follows the formula; check no crash on chunk load.
- Re-verify after each Bedrock update — **signatures are version-specific.**

## Why a signature, not an offset
A raw address is valid for exactly one build. A wildcarded byte pattern tolerates
minor recompiles and is the documented Levi Launchroid approach. Hardcoding an
address would break on the next update and risks crashing if mismatched.
