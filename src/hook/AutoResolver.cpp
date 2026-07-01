#include "AutoResolver.h"

#if defined(__ANDROID__)

#include <cxxabi.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../util/Log.h"

namespace terramath {
namespace {

constexpr const char* kModule = "libminecraftpe.so";

struct Module {
    bool found = false;
    uintptr_t base = 0;                 // load bias (dlpi_addr)
    const ElfW(Phdr)* phdr = nullptr;
    int phnum = 0;
};

int phdrCallback(struct dl_phdr_info* info, size_t, void* data) {
    auto* out = static_cast<Module*>(data);
    const char* name = info->dlpi_name ? info->dlpi_name : "";
    if (std::strstr(name, kModule) != nullptr) {
        out->found = true;
        out->base = static_cast<uintptr_t>(info->dlpi_addr);
        out->phdr = info->dlpi_phdr;
        out->phnum = info->dlpi_phnum;
        return 1;  // stop iteration
    }
    return 0;
}

Module findModule() {
    Module m;
    dl_iterate_phdr(&phdrCallback, &m);
    return m;
}

std::string demangle(const char* name) {
    int status = 0;
    char* d = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    std::string out = (status == 0 && d) ? d : name;
    std::free(d);
    return out;
}

std::vector<std::string> splitHints(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find(',', i);
        if (j == std::string::npos) j = s.size();
        std::string tok = s.substr(i, j - i);
        size_t a = tok.find_first_not_of(" \t");
        size_t b = tok.find_last_not_of(" \t");
        if (a != std::string::npos) out.push_back(tok.substr(a, b - a + 1));
        i = j + 1;
    }
    return out;
}

// Count entries in a DT_GNU_HASH symbol table.
uint32_t gnuHashCount(const uint32_t* table) {
    const uint32_t nbuckets = table[0];
    const uint32_t symoffset = table[1];
    const uint32_t bloom_size = table[2];
    const uintptr_t* bloom = reinterpret_cast<const uintptr_t*>(table + 4);
    const uint32_t* buckets = reinterpret_cast<const uint32_t*>(bloom + bloom_size);
    const uint32_t* chain = buckets + nbuckets;

    uint32_t last = 0;
    for (uint32_t i = 0; i < nbuckets; ++i)
        if (buckets[i] > last) last = buckets[i];
    if (last < symoffset) return symoffset;
    uint32_t idx = last;
    while (!(chain[idx - symoffset] & 1u)) ++idx;
    return idx + 1;
}

// Scan the loaded module's dynamic symbol table for a function whose demangled
// name contains any of the hints.
ResolveResult scanDynsym(const Module& m, const std::vector<std::string>& hints) {
    ResolveResult r;
    const ElfW(Dyn)* dyn = nullptr;
    for (int i = 0; i < m.phnum; ++i) {
        if (m.phdr[i].p_type == PT_DYNAMIC) {
            dyn = reinterpret_cast<const ElfW(Dyn)*>(m.base + m.phdr[i].p_vaddr);
            break;
        }
    }
    if (!dyn) return r;

    auto norm = [&](uintptr_t v) -> uintptr_t { return v < m.base ? m.base + v : v; };

    const ElfW(Sym)* symtab = nullptr;
    const char* strtab = nullptr;
    const uint32_t* sysv_hash = nullptr;
    const uint32_t* gnu_hash = nullptr;

    for (const ElfW(Dyn)* d = dyn; d->d_tag != DT_NULL; ++d) {
        switch (d->d_tag) {
            case DT_SYMTAB:
                symtab = reinterpret_cast<const ElfW(Sym)*>(norm(d->d_un.d_ptr));
                break;
            case DT_STRTAB:
                strtab = reinterpret_cast<const char*>(norm(d->d_un.d_ptr));
                break;
            case DT_HASH:
                sysv_hash = reinterpret_cast<const uint32_t*>(norm(d->d_un.d_ptr));
                break;
            case DT_GNU_HASH:
                gnu_hash = reinterpret_cast<const uint32_t*>(norm(d->d_un.d_ptr));
                break;
            default:
                break;
        }
    }
    if (!symtab || !strtab) return r;

    uint32_t count = 0;
    if (sysv_hash)
        count = sysv_hash[1];  // nchain == number of symbols
    else if (gnu_hash)
        count = gnuHashCount(gnu_hash);
    if (count == 0 || count > 5'000'000) return r;  // sanity bound

    for (uint32_t i = 0; i < count; ++i) {
        const ElfW(Sym)& s = symtab[i];
        if ((s.st_info & 0xf) != STT_FUNC) continue;  // ELF*_ST_TYPE
        if (s.st_value == 0 || s.st_name == 0) continue;
        const char* raw = strtab + s.st_name;
        std::string name = demangle(raw);
        for (const std::string& h : hints) {
            if (!h.empty() && name.find(h) != std::string::npos) {
                r.ok = true;
                r.address = m.base + s.st_value;
                r.method = "dynsym scan";
                r.matchedName = name;
                return r;
            }
        }
    }
    return r;
}

}  // namespace

namespace {
// Parse a "1F 20 ?? D5" pattern into bytes + wildcard mask.
bool parsePattern(const std::string& pat, std::vector<uint8_t>& bytes,
                  std::vector<bool>& wild) {
    size_t i = 0;
    while (i < pat.size()) {
        while (i < pat.size() && std::isspace(static_cast<unsigned char>(pat[i]))) ++i;
        if (i >= pat.size()) break;
        if (pat[i] == '?') {
            bytes.push_back(0);
            wild.push_back(true);
            ++i;
            if (i < pat.size() && pat[i] == '?') ++i;
        } else {
            char* end = nullptr;
            long v = std::strtol(pat.c_str() + i, &end, 16);
            if (end == pat.c_str() + i || v < 0 || v > 0xff) return false;
            bytes.push_back(static_cast<uint8_t>(v));
            wild.push_back(false);
            i = static_cast<size_t>(end - pat.c_str());
        }
    }
    return !bytes.empty();
}
}  // namespace

uintptr_t resolveSignature(const std::string& pattern) {
    std::vector<uint8_t> bytes;
    std::vector<bool> wild;
    if (!parsePattern(pattern, bytes, wild)) return 0;

    Module m = findModule();
    if (!m.found) return 0;

    const size_t n = bytes.size();
    for (int i = 0; i < m.phnum; ++i) {
        const ElfW(Phdr)& ph = m.phdr[i];
        if (ph.p_type != PT_LOAD || !(ph.p_flags & PF_X)) continue;
        const uint8_t* start = reinterpret_cast<const uint8_t*>(m.base + ph.p_vaddr);
        size_t len = ph.p_memsz;
        if (len < n) continue;
        for (size_t off = 0; off + n <= len; ++off) {
            const uint8_t* p = start + off;
            bool hit = true;
            for (size_t k = 0; k < n; ++k) {
                if (!wild[k] && p[k] != bytes[k]) {
                    hit = false;
                    break;
                }
            }
            if (hit) return reinterpret_cast<uintptr_t>(p);
        }
    }
    return 0;
}

std::string generateSignature(uintptr_t address, std::size_t len) {
    if (address == 0) return "";
    const unsigned char* p = reinterpret_cast<const unsigned char*>(address);
    std::string out;
    char buf[4];
    for (std::size_t i = 0; i < len; ++i) {
        std::snprintf(buf, sizeof(buf), "%02x", p[i]);
        if (i) out += ' ';
        out += buf;
    }
    return out;
}

ResolveResult autoResolveTerrainFunction(const ResolverConfig& cfg) {
    ResolveResult r;
    if (!cfg.autoDetect) return r;

    Module m = findModule();
    if (!m.found) {
        TM_LOGW("Auto-resolver: %s not found in the process map.", kModule);
        return r;
    }

    std::vector<std::string> hints = splitHints(cfg.symbolHints);
    if (hints.empty()) return r;

    // Fast path: a hint provided as an exact exported symbol name.
    if (void* h = dlopen(kModule, RTLD_NOW | RTLD_NOLOAD)) {
        for (const std::string& hint : hints) {
            if (void* sym = dlsym(h, hint.c_str())) {
                r.ok = true;
                r.address = reinterpret_cast<uintptr_t>(sym);
                r.method = "exported symbol (dlsym)";
                r.matchedName = hint;
                break;
            }
        }
    }

    // Main path: demangled dynamic-symbol scan.
    if (!r.ok) r = scanDynsym(m, hints);

    if (r.ok) {
        r.signature = generateSignature(r.address, 16);
        TM_LOGI("Auto-resolver: matched '%s' at %p via %s.", r.matchedName.c_str(),
                reinterpret_cast<void*>(r.address), r.method.c_str());
    } else {
        TM_LOGW(
            "Auto-resolver: no symbol matched the hints. This Bedrock build is "
            "likely stripped of these symbols; adjust the hints in the menu's "
            "Advanced section or set a manual signature.");
    }
    return r;
}

}  // namespace terramath

#endif
