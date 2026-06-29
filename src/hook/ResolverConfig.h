// TerraMath-LeviLauncroidMod
// -----------------------------------------------------------------------------
// Configuration for the on-device terrain-function auto-resolver. All fields are
// editable in the ImGui "Advanced" section, so the hook target can be retargeted
// for any Bedrock build without recompiling or pasting byte signatures.
// Pure data (no Android deps) so it is part of the host-built Config.
// -----------------------------------------------------------------------------
#pragma once

#include <string>

namespace terramath {

struct ResolverConfig {
    // Master switch: when true the mod locates the generation function itself
    // (exported symbol -> demangled .dynsym scan). When false, only a manual
    // signature override (if provided) is used.
    bool autoDetect = true;

    // Comma-separated substrings matched against demangled symbol names in
    // libminecraftpe.so. The first STT_FUNC whose demangled name contains any
    // of these is used. Defaults target the overworld terrain-height path.
    std::string symbolHints =
        "ChunkGenerator::getHeight,NoiseBasedChunkGenerator,OverworldGenerator,buildSurface";

    // Manual byte-signature override (optional, Advanced only). Empty unless you
    // deliberately pin a pattern. Auto-detect is tried first regardless.
    std::string manualSignature = "";

    // "height" -> column surface seam; "density" -> per-voxel seam.
    std::string hookMode = "height";
};

}  // namespace terramath
