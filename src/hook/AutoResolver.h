// TerraMath-LeviLauncroidMod
// -----------------------------------------------------------------------------
// Auto-resolver: locates Bedrock's terrain-generation function inside the loaded
// libminecraftpe.so at runtime, so the user never pastes a byte signature.
//
// Strategy (best-effort, in order):
//   1. dlsym() of the demangled hint as an exported symbol (fast path).
//   2. In-memory .dynsym scan: walk the loaded module's dynamic symbol table,
//      demangle each STT_FUNC name (abi::__cxa_demangle) and match the hints.
// A discovered address is enough to install the hook — and a byte signature is
// auto-generated from the prologue purely for display/caching.
// -----------------------------------------------------------------------------
#pragma once

#if defined(__ANDROID__)

#include <cstdint>
#include <string>

#include "ResolverConfig.h"

namespace terramath {

struct ResolveResult {
    bool ok = false;
    uintptr_t address = 0;
    std::string method;       // how it was found (for the UI/log)
    std::string matchedName;  // demangled symbol name that matched
    std::string signature;    // auto-generated prologue pattern (informational)
};

// Try to locate the generation function using cfg.symbolHints.
ResolveResult autoResolveTerrainFunction(const ResolverConfig& cfg);

// Read `len` bytes at `address` and format them as a space-separated hex
// pattern (e.g. "1f 20 03 d5 ..."). Used to surface/cache what was resolved.
std::string generateSignature(uintptr_t address, std::size_t len = 16);

// Manual-signature fallback: scan libminecraftpe.so's executable segments for a
// byte pattern ("1F 20 03 D5 ?? ?? 94", spaces + `??` wildcards). Returns the
// first matching address, or 0. Self-contained (no external signature lib), so
// the mod links against only the prebuilt Gloss library.
uintptr_t resolveSignature(const std::string& pattern);

}  // namespace terramath

#endif
