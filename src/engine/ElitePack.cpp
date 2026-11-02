#include "ElitePack.hpp"

#include <shellapi.h>

#include "common/Logger.hpp"
#include "common/SaveCodec.hpp"
#include "hooks/HookManager.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <climits>
#include <intrin.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <mutex>
#include <vector>

// Engine addresses (RVAs in the supported haloreach.dll, SHA-256 738dd2d2...). Found by reading how
// Reach Renaissance's host loads its Elite pack (docs: claude/elite-pack-notes.md in the project).
//   0x0A38A3  call to the match map loader 0x0141C0 (request: name, extra cache count, extra cache names)
//   0x031395  call to the extra cache loader 0x031078 inside the cache set loader (first extra list)
//   0x0AA634  map header slot (index 0xFFFF, name, mode 2), the main map gets it at 0x0A37DC
//   0x030698  per-cache load step (rebases the cache's own 32-bit fixups, then registers its tags)
//   0x030859  "mov edx,edx" there drops the top half of a 64-bit load delta; patched to a 2-byte nop
//   0x688F3C  packs a 64-bit tag address into a 32-bit packed pointer (table at 0x4E39F20)

namespace noble {
namespace ElitePack {
namespace {

constexpr uintptr_t kLoadCallSite = 0x0A38A3;
constexpr uintptr_t kMapLoad = 0x0141C0;
constexpr uintptr_t kExtraCallSite = 0x031395;
constexpr uintptr_t kExtraLoad = 0x031078;
constexpr uintptr_t kHeaderSlot = 0x0AA634;
constexpr uintptr_t kCacheStep = 0x030698;
constexpr uintptr_t kDeltaTruncation = 0x030859;
constexpr uintptr_t kEncodePointer = 0x688F3C;

struct LoadRequest {
    const char* mapName;        // "haloreach\maps\20_sword_slayer"
    int32_t extraCount;         // extra caches loaded with the map
    int32_t pad;
    const char* const* extraNames;
};

using MapLoadFn = uint8_t(__fastcall*)(void*, void*, void*, LoadRequest*);
using ExtraLoadFn = uint8_t(__fastcall*)(void*, const char*);
using HeaderSlotFn = uint8_t(__fastcall*)(uint32_t, const char*, uint32_t);
using CacheStepFn = uint8_t(__fastcall*)(void*, void*, uint32_t, void*, void*, void*);
using EncodeFn = uint32_t(__fastcall*)(uint64_t);

struct Reloc {
    std::vector<uint32_t> offsets;  // offsets of packed pointers inside the loaded pack
    uint64_t virtualBase = 0;       // the pack's own base address (cache +0x2F0 must match)
    uint64_t packedBase = 0;        // base the packed pointers were written against
    uint32_t sectionSize = 0;       // loaded size (cache +0x24 must match)
};

uintptr_t s_base = 0;
std::filesystem::path s_packPath;
std::string s_packStem;
std::string s_packEngineName;       // "haloreach\maps\<stem>", built from the match map's name
Reloc s_reloc;
bool s_relocPending = false;
std::atomic<bool> s_loading{false};
std::atomic<bool> s_loaded{false};
std::atomic<uint64_t> s_adaptedCalls{0};
bool s_ready = false;               // every hook is in place

MapLoadFn s_mapLoad = nullptr;
ExtraLoadFn s_extraLoad = nullptr;
CacheStepFn s_cacheStepOriginal = nullptr;
const char* s_extraList[1] = {nullptr};

template <typename T> T At(uintptr_t rva) { return reinterpret_cast<T>(s_base + rva); }

bool ReadReloc(const std::filesystem::path& file, Reloc& out) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (d.size() < 0x20 || std::memcmp(d.data(), "RRRL", 4) != 0) {
        NOBLE_WARN("ElitePack: {} is not a relocation sidecar", file.string());
        return false;
    }
    uint32_t version = 0, count = 0;
    std::memcpy(&version, &d[4], 4);
    std::memcpy(&count, &d[8], 4);
    std::memcpy(&out.virtualBase, &d[12], 8);
    std::memcpy(&out.packedBase, &d[20], 8);
    std::memcpy(&out.sectionSize, &d[28], 4);
    if (version != 1 || d.size() != 0x20 + static_cast<size_t>(count) * 4) {
        NOBLE_WARN("ElitePack: {} has version {} / {} records for {} bytes", file.string(), version, count, d.size());
        return false;
    }
    out.offsets.resize(count);
    std::memcpy(out.offsets.data(), &d[0x20], static_cast<size_t>(count) * 4);
    for (uint32_t off : out.offsets) {
        if (static_cast<uint64_t>(off) + 4 > out.sectionSize) {
            NOBLE_WARN("ElitePack: relocation record outside the pack's section");
            return false;
        }
    }
    return true;
}

// Writes bytes into the engine's code after checking what is there.
bool PatchCode(uintptr_t rva, const uint8_t* expect, const uint8_t* replace, size_t n) {
    uint8_t* p = At<uint8_t*>(rva);
    if (std::memcmp(p, replace, n) == 0) return true;
    if (std::memcmp(p, expect, n) != 0) return false;
    DWORD old = 0;
    if (!VirtualProtect(p, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    std::memcpy(p, replace, n);
    VirtualProtect(p, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, n);
    return true;
}

// A jump stub within +-2 GB of the engine, so a rel32 call there can reach Noble's code.
void* NearStub(void* target) {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const uintptr_t gran = si.dwAllocationGranularity;
    for (uintptr_t delta = 0x10000000; delta < 0x7F000000; delta += gran * 16) {
        for (int sign : {-1, 1}) {
            const uintptr_t want = ((s_base + sign * static_cast<intptr_t>(delta)) / gran) * gran;
            void* mem = VirtualAlloc(reinterpret_cast<void*>(want), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (!mem) continue;
            uint8_t* s = static_cast<uint8_t*>(mem);
            s[0] = 0xFF; s[1] = 0x25; std::memset(s + 2, 0, 4);       // jmp [rip+0]
            const uint64_t abs = reinterpret_cast<uint64_t>(target);
            std::memcpy(s + 6, &abs, 8);
            FlushInstructionCache(GetCurrentProcess(), s, 14);
            return mem;
        }
    }
    return nullptr;
}

// Points the call at `site` (E8 rel32 to `expect`) at `detour`. Returns the original target.
void* RetargetCall(uintptr_t site, uintptr_t expect, void* detour) {
    uint8_t* p = At<uint8_t*>(site);
    if (p[0] != 0xE8) return nullptr;
    int32_t rel = 0;
    std::memcpy(&rel, p + 1, 4);
    const uintptr_t target = s_base + site + 5 + rel;
    if (target != s_base + expect) return nullptr;
    void* stub = NearStub(detour);
    if (!stub) return nullptr;
    const int64_t newRel = reinterpret_cast<intptr_t>(stub) - static_cast<intptr_t>(s_base + site + 5);
    if (newRel < INT32_MIN || newRel > INT32_MAX) return nullptr;
    const int32_t r32 = static_cast<int32_t>(newRel);
    DWORD old = 0;
    if (!VirtualProtect(p + 1, 4, PAGE_EXECUTE_READWRITE, &old)) return nullptr;
    std::memcpy(p + 1, &r32, 4);
    VirtualProtect(p + 1, 4, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    return reinterpret_cast<void*>(target);
}

uint8_t __fastcall MapLoadHook(void* a1, void* a2, void* a3, LoadRequest* req) {
    if (!s_ready || !req || !req->mapName || req->extraCount != 0) {
        if (req) NOBLE_INFO("ElitePack: not added (map {}, extra caches already {})",
                            req->mapName ? req->mapName : "?", req->extraCount);
        return s_mapLoad(a1, a2, a3, req);
    }
    std::string name = req->mapName;
    const size_t slash = name.find_last_of("\\/");
    s_packEngineName = (slash == std::string::npos ? std::string() : name.substr(0, slash + 1)) + s_packStem;
    s_extraList[0] = s_packEngineName.c_str();
    req->extraCount = 1;
    req->extraNames = s_extraList;
    s_relocPending = true;
    NOBLE_INFO("ElitePack: loading {} with {}", s_packEngineName, name);
    s_loading = true;
    const uint8_t ok = s_mapLoad(a1, a2, a3, req);
    s_loading = false;
    s_relocPending = false;
    NOBLE_INFO("ElitePack: map load returned {} (pack {}, {} adapted resource calls so far)", ok,
               s_loaded ? "loaded" : "not loaded", s_adaptedCalls.load());
    return ok;
}

uint8_t __fastcall ExtraLoadHook(void* a1, const char* name) {
    const bool ours = s_loading && name && s_packEngineName == name;
    if (ours) {
        const uint8_t slot = At<HeaderSlotFn>(kHeaderSlot)(0xFFFF, name, 2);
        NOBLE_INFO("ElitePack: header slot for {}: {}", name, slot ? "ready" : "failed");
    }
    const uint8_t ok = s_extraLoad(a1, name);
    if (ours) NOBLE_INFO("ElitePack: extra cache {} load returned {}", name, ok);
    return ok;
}

// Rebases the pack's packed tag pointers before the engine processes the loaded cache.
void Relocate(uint8_t* cache) {
    uint64_t a = 0, b = 0, hdr = 0;
    uint32_t size = 0;
    std::memcpy(&a, cache + 0x000, 8);
    std::memcpy(&size, cache + 0x024, 4);
    std::memcpy(&b, cache + 0x2F0, 8);
    std::memcpy(&hdr, cache + 0x2F8, 8);
    if (!a || b != s_reloc.virtualBase || size != s_reloc.sectionSize) return;

    const auto encode = At<EncodeFn>(kEncodePointer);
    const uint64_t delta = a - b, end = b + size;
    size_t moved = 0, outside = 0;
    for (uint32_t off : s_reloc.offsets) {
        uint32_t packed = 0;
        std::memcpy(&packed, reinterpret_cast<void*>(a + off), 4);
        if (!packed) continue;
        const uint64_t addr = (static_cast<uint64_t>(packed) << 2) + s_reloc.packedBase;
        if (addr < b || addr >= end) { ++outside; continue; }
        const uint32_t rebased = encode(addr + delta);
        std::memcpy(reinterpret_cast<void*>(a + off), &rebased, 4);
        ++moved;
    }
    int tables = 0;
    for (uint64_t k : {0x08ull, 0x18ull, 0x28ull, 0x38ull}) {
        uint64_t* slot = reinterpret_cast<uint64_t*>(hdr + delta + k);
        if (*slot >= b && *slot < end) { *slot += delta; ++tables; }
    }
    s_relocPending = false;
    s_loaded = true;
    NOBLE_INFO("ElitePack: relocated {} packed pointers ({} outside the pack left alone) and {} table pointers from 0x{:X} to 0x{:X}",
               moved, outside, tables, b, a);
}

bool TryRelocate(uint8_t* cache) {
    __try {
        Relocate(cache);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ---- Stage A2: players use the pack's Elite ----------------------------------------------------
// The engine's combined tag table (4th argument of the cache step): 8-byte instances {i16 group
// index, u16 salt, u32 packed address} (count at +0x3FFF8, at most 0x8000), group magics at
// +0x3FFFC + group * 16. A datum is (salt << 16) | index. Packed address -> pointer:
// table[p >> 28] + (p << 2), table = 16 u64 at hr+0x4E39F20.
constexpr uintptr_t kPointerBases = 0x4E39F20;
constexpr uintptr_t kZoneRebuild = 0x08EAF0;   // zone-set rebuild; active-tag bits at arg +0x30B70

uint8_t* TagAt(uint32_t packed) {
    const uint64_t* bases = At<const uint64_t*>(kPointerBases);
    return reinterpret_cast<uint8_t*>(bases[packed >> 28] + (static_cast<uint64_t>(packed) << 2));
}

struct TagTable {
    uint8_t* t;
    int32_t Count() const {
        int32_t n = 0;
        std::memcpy(&n, t + 0x3FFF8, 4);
        return std::clamp(n, 0, 0x8000);
    }
    bool Valid(int32_t i, uint32_t magic) const {
        int16_t group = 0;
        uint16_t salt = 0;
        std::memcpy(&group, t + i * 8, 2);
        std::memcpy(&salt, t + i * 8 + 2, 2);
        if (group < 0 || salt == 0xFFFF) return false;
        uint32_t m = 0;
        std::memcpy(&m, t + 0x3FFFC + group * 16, 4);
        return m == magic;
    }
    uint32_t Address(int32_t i) const { uint32_t a = 0; std::memcpy(&a, t + i * 8 + 4, 4); return a; }
    uint32_t Magic(int32_t i) const {
        int16_t group = 0;
        std::memcpy(&group, t + i * 8, 2);
        if (group < 0) return 0;
        uint32_t m = 0;
        std::memcpy(&m, t + 0x3FFFC + group * 16, 4);
        return m;
    }
    void SetAddress(int32_t i, uint32_t a) { std::memcpy(t + i * 8 + 4, &a, 4); }
    uint32_t Datum(int32_t i) const { uint16_t salt = 0; std::memcpy(&salt, t + i * 8 + 2, 2); return (uint32_t(salt) << 16) | uint32_t(i); }
    int32_t First(uint32_t magic) const { for (int32_t i = 0; i < Count(); ++i) if (Valid(i, magic)) return i; return -1; }
    bool Is(uint32_t datum, uint32_t magic) const {
        if (datum == 0xFFFFFFFFu) return false;
        const int32_t i = datum & 0xFFFF;
        return i < Count() && Valid(i, magic) && Datum(i) == datum;
    }
};
constexpr uint32_t kBipd = 0x62697064, kMatg = 0x6D617467, kPmcg = 0x706D6367;

uint32_t s_activeDatums[3] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
std::atomic<bool> s_haveActive{false};
using RebuildFn = void(__fastcall*)(uint8_t*);
RebuildFn s_rebuildOriginal = nullptr;

// Keeps the pack Elite's biped, model and render model active after every zone-set switch (RR "rebuild").
void __fastcall RebuildHook(uint8_t* state) {
    s_rebuildOriginal(state);
    if (!state || !s_haveActive) return;
    for (uint32_t d : s_activeDatums) {
        if (d == 0xFFFFFFFFu) continue;
        uint32_t* word = reinterpret_cast<uint32_t*>(state + 0x30B70 + ((d >> 3) & 0x1FFC));
        *word |= 1u << (d & 31);
    }
}

bool s_keepPackList = false;
bool s_keepPackHealth = false;
int32_t s_pmcgIndex = -1;
uint32_t s_pmcgAddress = 0;
std::vector<uint32_t> s_addressesBefore;   // every tag's address before the pack's step
bool s_keepPackGlobals = false;
bool s_keepDeathEffect = false;

void BeforePackStep(uint8_t* tags) {
    TagTable tt{tags};
    s_pmcgIndex = tt.First(kPmcg);
    s_pmcgAddress = s_pmcgIndex >= 0 ? tt.Address(s_pmcgIndex) : 0;
    s_addressesBefore.resize(static_cast<size_t>(tt.Count()));
    for (int32_t i = 0; i < tt.Count(); ++i) s_addressesBefore[static_cast<size_t>(i)] = tt.Address(i);
}

std::string fmt_hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof(b), " 0x%08X", v);
    return b;
}

std::string GroupText(uint32_t magic) {
    char g[5] = {static_cast<char>(magic >> 24), static_cast<char>(magic >> 16), static_cast<char>(magic >> 8),
                 static_cast<char>(magic), 0};
    for (char& ch : g) if (ch && (ch < 0x20 || ch > 0x7E)) ch = '?';
    return g;
}

// Besides the armour list, the pack carries its own copy of some of the map's one-of-a-kind tags (globals
// such as effect_globals) and loading it points those at the pack's copies. The pack's effect globals made
// Elites play Heart Attack and Grunt Birthday Party. Put back the map's own copy of every one-of-a-kind tag
// the pack replaced (tags of a group the pack has only one of). "-elite-pack-keep-globals" leaves them (test).
void RestoreMapGlobals(TagTable& tt, uint64_t packStart, uint64_t packEnd) {
    std::vector<std::pair<int32_t, uint32_t>> replaced;
    std::vector<uint32_t> packGroups;   // group magic of every tag stored inside the pack
    const int32_t n = std::min<int32_t>(tt.Count(), static_cast<int32_t>(s_addressesBefore.size()));
    for (int32_t i = 0; i < tt.Count(); ++i) {
        const uint32_t now = tt.Address(i);
        if (!now) continue;
        const uint64_t at = reinterpret_cast<uint64_t>(TagAt(now));
        if (at < packStart || at >= packEnd) continue;
        packGroups.push_back(tt.Magic(i));
        if (i < n && s_addressesBefore[static_cast<size_t>(i)] && s_addressesBefore[static_cast<size_t>(i)] != now)
            replaced.push_back({i, s_addressesBefore[static_cast<size_t>(i)]});
    }
    std::string restoredText, keptText;
    int restored = 0, kept = 0;
    for (const auto& [i, before] : replaced) {
        const uint32_t magic = tt.Magic(i);
        if (magic == kPmcg) continue;   // handled above (merged)
        const bool single = std::count(packGroups.begin(), packGroups.end(), magic) == 1;
        std::string& text = (single && !s_keepPackGlobals) ? restoredText : keptText;
        text += (text.empty() ? "" : ", ") + GroupText(magic) + fmt_hex(tt.Datum(i));
        if (single && !s_keepPackGlobals) { tt.SetAddress(i, before); ++restored; } else ++kept;
    }
    NOBLE_INFO("ElitePack: the pack replaced {} of the map's tags; map's copy put back for {} [{}]; pack's kept for {} [{}]{}",
               replaced.size(), restored, restoredText, kept, keptText, s_keepPackGlobals ? " (-elite-pack-keep-globals)" : "");
}

// Armour list (pmcg): 11 lists from +0x20, 12 bytes apart ({i32 count, u32 packed pointer, u32});
// the Elite armour sets are the list at +0x74. A selection is 16 bytes: +0 name (string id), +4 its
// changes {i32 count, u32 pointer, u32}; a change is 12 bytes: region, permutation (string ids), flags.
constexpr size_t kEliteSets = 0x74;

uint32_t EncodePointer(void* p) {
    const uint32_t packed = At<EncodeFn>(kEncodePointer)(reinterpret_cast<uint64_t>(p));
    return TagAt(packed) == p ? packed : 0;
}

void MergeEliteSets(uint8_t* mapList, uint8_t* packList) {
    auto block = [](uint8_t* at, int32_t& count, uint8_t*& items) {
        uint32_t ptr = 0;
        std::memcpy(&count, at, 4);
        std::memcpy(&ptr, at + 4, 4);
        items = (count > 0 && count < 0x400 && ptr) ? TagAt(ptr) : nullptr;
        if (!items) count = 0;
    };
    int32_t mapCount = 0, packCount = 0;
    uint8_t* mapSets = nullptr;
    uint8_t* packSets = nullptr;
    block(mapList + kEliteSets, mapCount, mapSets);
    block(packList + kEliteSets, packCount, packSets);
    int merged = 0, added = 0;
    for (int32_t p = 0; p < packCount; ++p) {
        uint8_t* ps = packSets + p * 16;
        uint32_t name = 0;
        std::memcpy(&name, ps, 4);
        uint8_t* ms = nullptr;
        for (int32_t m = 0; m < mapCount && !ms; ++m) {
            uint32_t mn = 0;
            std::memcpy(&mn, mapSets + m * 16, 4);
            if (mn == name) ms = mapSets + m * 16;
        }
        if (!ms) continue;
        int32_t pc = 0, mc = 0;
        uint8_t* pch = nullptr;
        uint8_t* mch = nullptr;
        block(ps + 4, pc, pch);
        block(ms + 4, mc, mch);
        if (p < 2) {   // what the sets change, for checking (region:permutation:flags string ids)
            auto list = [](uint8_t* ch, int32_t n) {
                std::string out;
                for (int32_t i = 0; i < n; ++i) {
                    uint32_t v[3];
                    std::memcpy(v, ch + i * 12, 12);
                    char buf[48];
                    std::snprintf(buf, sizeof(buf), "%s%X:%X:%X", i ? " " : "", v[0], v[1], v[2]);
                    out += buf;
                }
                return out;
            };
            NOBLE_INFO("ElitePack: Elite set {:X}: pack [{}] map [{}]", name, list(pch, pc), list(mch, mc));
        }
        // The map's changes for regions the pack's set doesn't change (the first-person arms' region).
        std::vector<uint8_t> extra;
        for (int32_t i = 0; i < mc; ++i) {
            uint32_t region = 0;
            std::memcpy(&region, mch + i * 12, 4);
            bool have = false;
            for (int32_t j = 0; j < pc && !have; ++j) {
                uint32_t r = 0;
                std::memcpy(&r, pch + j * 12, 4);
                have = r == region;
            }
            if (!have) extra.insert(extra.end(), mch + i * 12, mch + i * 12 + 12);
        }
        if (extra.empty()) continue;
        const size_t bytes = static_cast<size_t>(pc) * 12 + extra.size();
        auto* mem = static_cast<uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (!mem) continue;
        if (pc) std::memcpy(mem, pch, static_cast<size_t>(pc) * 12);
        std::memcpy(mem + static_cast<size_t>(pc) * 12, extra.data(), extra.size());
        const uint32_t packed = EncodePointer(mem);
        if (!packed) {
            VirtualFree(mem, 0, MEM_RELEASE);
            NOBLE_WARN("ElitePack: couldn't address a merged Elite set; first-person arms stay as the pack has them");
            return;
        }
        const int32_t total = pc + static_cast<int32_t>(extra.size() / 12);
        std::memcpy(ps + 4, &total, 4);
        std::memcpy(ps + 8, &packed, 4);
        ++merged;
        added += static_cast<int>(extra.size() / 12);
    }
    // Ensure packList + 0x8C (Elite effects) is populated
    {
        int32_t pec = 0;
        uint8_t* pes = nullptr;
        block(packList + 0x8C, pec, pes);
        if (pec <= 0 || !pes) {
            std::memcpy(packList + 0x8C, mapList + 0x80, 12);
        }
    }
    NOBLE_INFO("ElitePack: armour list: the pack's ({} Elite sets) with {} of the map's changes added to {} sets (map has {})",
               packCount, added, merged, mapCount);
}

// RR's merge: the region ids the pack's Elite sets change are the "Elite" regions. For the 7 Spartan piece
// lists (+0x20 helmet .. +0x68 knees), every map selection gets the changes to Elite regions from the pack's
// selection of the same name, so a piece index picks Spartan and Elite pieces alike.
void MergeElitePieces(uint8_t* mapList, uint8_t* packList) {
    auto block = [](uint8_t* at, int32_t& count, uint8_t*& items) {
        uint32_t ptr = 0;
        std::memcpy(&count, at, 4);
        std::memcpy(&ptr, at + 4, 4);
        items = (count > 0 && count < 0x400 && ptr) ? TagAt(ptr) : nullptr;
        if (!items) count = 0;
    };
    std::vector<uint32_t> eliteRegions;
    {
        int32_t n = 0;
        uint8_t* sets = nullptr;
        block(packList + kEliteSets, n, sets);
        for (int32_t i = 0; i < n; ++i) {
            int32_t c = 0;
            uint8_t* ch = nullptr;
            block(sets + i * 16 + 4, c, ch);
            for (int32_t j = 0; j < c; ++j) {
                uint32_t r = 0;
                std::memcpy(&r, ch + j * 12, 4);
                if (std::find(eliteRegions.begin(), eliteRegions.end(), r) == eliteRegions.end()) eliteRegions.push_back(r);
            }
        }
    }
    if (eliteRegions.empty()) { NOBLE_WARN("ElitePack: the pack's Elite sets change no regions; nothing to merge"); return; }
    std::string regions;
    for (uint32_t r : eliteRegions) { char b[16]; std::snprintf(b, sizeof(b), "%s%X", regions.empty() ? "" : " ", r); regions += b; }

    int matched = 0, mapOnly = 0, added = 0, failed = 0;
    for (size_t list = 0x20; list <= 0x68; list += 0x0C) {
        int32_t mc = 0, pc = 0;
        uint8_t* ms = nullptr;
        uint8_t* ps = nullptr;
        block(mapList + list, mc, ms);
        block(packList + list, pc, ps);
        int listAdded = 0;
        for (int32_t m = 0; m < mc; ++m) {
            uint8_t* msel = ms + m * 16;
            uint32_t name = 0;
            std::memcpy(&name, msel, 4);
            uint8_t* psel = nullptr;
            for (int32_t q = 0; q < pc && !psel; ++q) {
                uint32_t pn = 0;
                std::memcpy(&pn, ps + q * 16, 4);
                if (pn == name) psel = ps + q * 16;
            }
            if (!psel) { ++mapOnly; continue; }
            ++matched;
            int32_t c1 = 0, c2 = 0;
            uint8_t* ch1 = nullptr;
            uint8_t* ch2 = nullptr;
            block(msel + 4, c1, ch1);
            block(psel + 4, c2, ch2);
            std::vector<uint8_t> extra;
            for (int32_t j = 0; j < c2; ++j) {
                uint32_t r = 0;
                std::memcpy(&r, ch2 + j * 12, 4);
                if (std::find(eliteRegions.begin(), eliteRegions.end(), r) != eliteRegions.end())
                    extra.insert(extra.end(), ch2 + j * 12, ch2 + j * 12 + 12);
            }
            if (extra.empty()) continue;
            const size_t bytes = static_cast<size_t>(c1) * 12 + extra.size();
            auto* mem = static_cast<uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            if (!mem) { ++failed; continue; }
            if (c1) std::memcpy(mem, ch1, static_cast<size_t>(c1) * 12);
            std::memcpy(mem + static_cast<size_t>(c1) * 12, extra.data(), extra.size());
            const uint32_t packed = EncodePointer(mem);
            if (!packed) { VirtualFree(mem, 0, MEM_RELEASE); ++failed; continue; }
            const int32_t total = c1 + static_cast<int32_t>(extra.size() / 12);
            std::memcpy(msel + 4, &total, 4);
            std::memcpy(msel + 8, &packed, 4);
            added += static_cast<int>(extra.size() / 12);
            listAdded += static_cast<int>(extra.size() / 12);
        }
        // Selections only the pack has (e.g. the Elite legs and forearms beyond the map's 7 utility / 6 wrist
        // entries) are appended in the pack's order, so pack indices stay valid in the merged list.
        std::vector<uint8_t> packOnly;
        for (int32_t q = 0; q < pc; ++q) {
            uint32_t pn = 0;
            std::memcpy(&pn, ps + q * 16, 4);
            bool inMap = false;
            for (int32_t m = 0; m < mc && !inMap; ++m) {
                uint32_t mn = 0;
                std::memcpy(&mn, ms + m * 16, 4);
                inMap = mn == pn;
            }
            if (!inMap) packOnly.insert(packOnly.end(), ps + q * 16, ps + q * 16 + 16);
        }
        if (!packOnly.empty()) {
            const size_t bytes = static_cast<size_t>(mc) * 16 + packOnly.size();
            auto* mem = static_cast<uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            const uint32_t packed = mem ? (std::memcpy(mem, ms, static_cast<size_t>(mc) * 16),
                                           std::memcpy(mem + static_cast<size_t>(mc) * 16, packOnly.data(), packOnly.size()),
                                           EncodePointer(mem)) : 0;
            if (packed) {
                const int32_t total = mc + static_cast<int32_t>(packOnly.size() / 16);
                std::memcpy(mapList + list, &total, 4);
                std::memcpy(mapList + list + 4, &packed, 4);
            } else {
                if (mem) VirtualFree(mem, 0, MEM_RELEASE);
                ++failed;
            }
        }
        NOBLE_INFO("ElitePack: piece list +0x{:X}: map {} selections, pack {}, {} Elite changes added, {} pack-only selections appended",
                   list, mc, pc, listAdded, packOnly.size() / 16);
    }
    // Ensure Elite armor effects list (+0x8C) is populated in mapList so AppearanceBuilder doesn't exit early.
    // If packList has entries at +0x8C, copy them to mapList + 0x8C; otherwise copy from mapList + 0x80.
    {
        int32_t pec = 0, mec = 0;
        uint8_t* pes = nullptr;
        uint8_t* mes = nullptr;
        block(packList + 0x8C, pec, pes);
        block(mapList + 0x8C, mec, mes);
        if (pec > 0 && pes) {
            std::memcpy(mapList + 0x8C, packList + 0x8C, 12);
            NOBLE_INFO("ElitePack: Populated mapList Elite armor effects (+0x8C) from packList ({} effects)", pec);
        } else {
            int32_t sec = 0;
            uint8_t* ses = nullptr;
            block(mapList + 0x80, sec, ses);
            if (sec > 0 && ses) {
                std::memcpy(mapList + 0x8C, mapList + 0x80, 12);
                NOBLE_INFO("ElitePack: Populated mapList Elite armor effects (+0x8C) from mapList Spartan effects (+0x80, {} effects)", sec);
            }
        }
    }

    NOBLE_INFO("ElitePack: armour list: the map's, with the pack's Elite pieces: {} selections matched by name, {} only in the map, "
               "{} Elite changes added{} (Elite regions {})", matched, mapOnly, added,
               failed ? " (" + std::to_string(failed) + " failed)" : std::string(), regions);
}

void MatchStockDamage(const TagTable& tt, uint32_t packElite, uint32_t stockElite);

// ---- First-person Elite arms and body ---------------------------------------------------------
// The globals' player representation (matg +0x474, 0x6C each) names three models per character: +0x14 the
// first-person arms (render model objects\characters\elite\fp\fp, datum at +0x20), +0x24 the first-person body
// (objects\characters\elite\fp_body\fp_body, datum +0x30) and +0x34 the third-person biped (datum +0x40).
// Both first-person models have one armour region, "armor_pieces", with a permutation per Elite set (general,
// officer, space, spec_ops, ultra, story_elite, zealot, default = Minor) and no shoulders / Honor Guard / H2A.
// With the pack's per-piece Elite the first-person arms came out bare (no "armor_pieces" in the third-person
// model to follow). Override: every permutation of the region gets the mesh of the wanted set, so whatever the
// engine picks shows that set. The arms follow the Armory's Arms piece, the body follows the shoulders; pieces
// without a first-person version (Honor Guard, H2A, Council...) use the Armor Set. "-no-fp-override" turns it off.
// Render model: +0x0C regions {i32 count, u32 pointer, u32}, region 0x10 bytes {name, permutations block},
// permutation 0x18 bytes {+0 name, +4.. mesh data}. Set order (pmcg list +0x74): Minor, Spec Ops, Ranger,
// Ultra, Zealot, General, Field Marshal, Officer.
constexpr uint32_t kMode = 0x6D6F6465;
constexpr size_t kPermSize = 0x18;
struct FpRegion {
    uint8_t* perms = nullptr;          // permutation array of "armor_pieces"
    int32_t count = 0;
    std::vector<uint8_t> original;     // untouched copy, restored before every change
    int applied = -1;                  // set shown now (-1 = the model's own)
    const char* label = "";
};
FpRegion s_fp[2];                      // 0 = arms, 1 = body
uint32_t s_setPermutation[8] = {};     // permutation string id of each Elite set (from the map's pmcg list +0x74)
bool s_fpOverride = true;

void SetupFirstPerson(const TagTable& tt, const uint8_t* rep1, const uint8_t* mapPmcg) {
    if (!s_fpOverride || !rep1 || !mapPmcg) return;
    // armor_pieces region name and each set's permutation name, from the map's Elite set list.
    int32_t sets = 0;
    uint32_t setsPtr = 0;
    std::memcpy(&sets, mapPmcg + 0x74, 4);
    std::memcpy(&setsPtr, mapPmcg + 0x78, 4);
    if (sets < 8 || !setsPtr) { NOBLE_WARN("ElitePack: first-person override: Elite set list has {} entries", sets); return; }
    uint32_t region = 0;
    const uint8_t* sel = TagAt(setsPtr);
    for (int k = 0; k < 8; ++k) {
        int32_t changes = 0;
        uint32_t changesPtr = 0;
        std::memcpy(&changes, sel + k * 16 + 4, 4);
        std::memcpy(&changesPtr, sel + k * 16 + 8, 4);
        if (changes < 1 || !changesPtr) { NOBLE_WARN("ElitePack: first-person override: set {} has no change", k); return; }
        const uint8_t* ch = TagAt(changesPtr);
        std::memcpy(&region, ch, 4);
        std::memcpy(&s_setPermutation[k], ch + 4, 4);
    }
    const size_t datumAt[2] = {0x20, 0x30};
    const char* labels[2] = {"arms", "body"};
    for (int w = 0; w < 2; ++w) {
        FpRegion& fp = s_fp[w];
        fp = FpRegion{};
        fp.label = labels[w];
        uint32_t datum = 0xFFFFFFFFu;
        std::memcpy(&datum, rep1 + datumAt[w], 4);
        if (!tt.Is(datum, kMode)) { NOBLE_WARN("ElitePack: first-person {} model not found (0x{:X})", labels[w], datum); continue; }
        uint8_t* mode = TagAt(tt.Address(datum & 0xFFFF));
        int32_t regions = 0;
        uint32_t regionsPtr = 0;
        std::memcpy(&regions, mode + 0x0C, 4);
        std::memcpy(&regionsPtr, mode + 0x10, 4);
        if (regions <= 0 || regions > 64 || !regionsPtr) continue;
        uint8_t* r = TagAt(regionsPtr);
        for (int32_t i = 0; i < regions; ++i) {
            uint32_t name = 0;
            std::memcpy(&name, r + i * 0x10, 4);
            if (name != region) continue;
            int32_t perms = 0;
            uint32_t permsPtr = 0;
            std::memcpy(&perms, r + i * 0x10 + 4, 4);
            std::memcpy(&permsPtr, r + i * 0x10 + 8, 4);
            if (perms <= 0 || perms > 64 || !permsPtr) break;
            fp.perms = TagAt(permsPtr);
            fp.count = perms;
            fp.original.assign(fp.perms, fp.perms + size_t(perms) * kPermSize);
            break;
        }
        NOBLE_INFO("ElitePack: first-person {} model 0x{:X}: {} armour permutations{}", labels[w], datum, fp.count,
                   fp.perms ? "" : " (no armor_pieces region found)");
    }
}

// Shows Elite set 0..7 on first-person model w (arms / body).
void ApplyFirstPersonSet(int w, int set) {
    FpRegion& fp = s_fp[w];
    if (!fp.perms || set < 0 || set > 7 || set == fp.applied) return;
    std::memcpy(fp.perms, fp.original.data(), fp.original.size());
    const uint8_t* want = nullptr;
    for (int32_t i = 0; i < fp.count; ++i) {
        uint32_t name = 0;
        std::memcpy(&name, fp.original.data() + i * kPermSize, 4);
        if (name == s_setPermutation[set]) { want = fp.original.data() + i * kPermSize; break; }
    }
    if (!want) {
        NOBLE_WARN("ElitePack: first-person {} has no permutation for set {}", fp.label, set);
        fp.applied = set;
        return;
    }
    for (int32_t i = 0; i < fp.count; ++i) std::memcpy(fp.perms + i * kPermSize + 4, want + 4, kPermSize - 4);
    fp.applied = set;
    NOBLE_INFO("ElitePack: first-person {} now show Elite set {}", fp.label, set);
}
void DropDeathArmourEffect(const TagTable& tt, uint32_t packElite, uint32_t bodyName);
bool HasArg(const wchar_t* name);

void AfterPackStep(uint8_t* tags, uint8_t* cache) {
    TagTable tt{tags};
    uint64_t a = 0;
    uint32_t size = 0;
    std::memcpy(&a, cache, 8);
    std::memcpy(&size, cache + 0x24, 4);
    const uint64_t end = a + size;

    // The map keeps its own armour list for now (merging the pack's Elite pieces in is the next step).
    // The pack replaces the armour list (pmcg) with its own. Like RR, the map's list stays the one in use
    // (its Elite sets also dress the first-person Elite arms) and the pack's Elite piece changes are copied
    // into the map's Spartan piece lists (MergeElitePieces). "-elite-pack-list" keeps the pack's list (test).
    if (s_pmcgIndex >= 0 && tt.Address(s_pmcgIndex) != s_pmcgAddress) {
        uint8_t* packList = TagAt(tt.Address(s_pmcgIndex));
        uint8_t* mapList = TagAt(s_pmcgAddress);
        if (s_keepPackList) {
            MergeEliteSets(mapList, packList);
            NOBLE_INFO("ElitePack: keeping the pack's armour list (-elite-pack-list)");
        } else {
            tt.SetAddress(s_pmcgIndex, s_pmcgAddress);
            MergeElitePieces(mapList, packList);
        }
    }
    RestoreMapGlobals(tt, a, end);

    // The pack's player Elite: the biped whose tag lies inside the pack's section.
    std::vector<uint32_t> packBipeds;
    for (int32_t i = 0; i < tt.Count(); ++i) {
        if (!tt.Valid(i, kBipd)) continue;
        const uint64_t at = reinterpret_cast<uint64_t>(TagAt(tt.Address(i)));
        if (at >= a && at < end) packBipeds.push_back(tt.Datum(i));
    }
    if (packBipeds.size() != 1) {
        NOBLE_WARN("ElitePack: {} bipeds inside the pack (expected 1); players keep the map's Elite", packBipeds.size());
        return;
    }
    const uint32_t packElite = packBipeds[0];

    const int32_t globals = tt.First(kMatg);
    if (globals < 0) { NOBLE_WARN("ElitePack: no globals tag; players keep the map's Elite"); return; }
    uint8_t* matg = TagAt(tt.Address(globals));
    int32_t reps = 0;
    uint32_t repsPtr = 0;
    std::memcpy(&reps, matg + 0x474, 4);
    std::memcpy(&repsPtr, matg + 0x478, 4);
    if (reps < 2 || !repsPtr) { NOBLE_WARN("ElitePack: globals has {} player representations", reps); return; }
    uint8_t* rep = TagAt(repsPtr);
    reps = std::min(reps, 16);
    uint32_t stockElite = 0;
    std::memcpy(&stockElite, rep + 1 * 0x6C + 0x40, 4);
    if (!tt.Is(stockElite, kBipd)) { NOBLE_WARN("ElitePack: representation 1 isn't a biped (0x{:X})", stockElite); return; }
    std::string changed;
    for (int32_t r = 0; r < reps; ++r) {
        uint32_t d = 0;
        std::memcpy(&d, rep + r * 0x6C + 0x40, 4);
        if (d != stockElite) continue;
        std::memcpy(rep + r * 0x6C + 0x40, &packElite, 4);
        changed += (changed.empty() ? "" : ", ") + std::to_string(r);
    }

    // Keep the pack Elite's biped (+0x70 model) and the model's render model (+0x0C) active.
    MatchStockDamage(tt, packElite, stockElite);
    DropDeathArmourEffect(tt, packElite, 0xE2 /* "body" */);

    SetupFirstPerson(tt, rep + 1 * 0x6C, s_pmcgAddress ? TagAt(s_pmcgAddress) : nullptr);

    s_activeDatums[0] = packElite;
    uint32_t model = 0xFFFFFFFFu, render = 0xFFFFFFFFu;
    std::memcpy(&model, TagAt(tt.Address(packElite & 0xFFFF)) + 0x70, 4);
    if (model != 0xFFFFFFFFu && (model & 0xFFFF) < uint32_t(tt.Count()))
        std::memcpy(&render, TagAt(tt.Address(model & 0xFFFF)) + 0x0C, 4);
    s_activeDatums[1] = model;
    s_activeDatums[2] = render;
    s_haveActive = true;
    NOBLE_INFO("ElitePack: player representation {} -> the pack's player Elite biped 0x{:X} (AI Elites keep the map's 0x{:X}); "
               "kept active: 0x{:X}, 0x{:X}, 0x{:X}", changed, packElite, stockElite, s_activeDatums[0], model, render);
}

// Model (hlmt) +0xC0: "new damage info" block {i32 count, u32 packed pointer, u32}. Element: +0x00 flags,
// +0x04 global indirect material (string id), +0x08 indirect damage section (i16), body from +0x28 (maximum
// vitality), shield from +0x7C (maximum vitality, +0x80 shield material, +0x88 stun time, +0x8C recharge time),
// effect references up to +0xC8, then +0xCC the damage sections block (left alone: it lists the pack model's own).
constexpr uint32_t kHlmt = 0x686C6D74;
constexpr size_t kModelDamageInfo = 0xC0, kDamageCopyEnd = 0xCC;

constexpr size_t kModelShield = 0xD0;

// Model tag of a biped (biped +0x70); DamageInfo() has already checked it is an hlmt.
uint8_t* ModelOf(const TagTable& tt, uint32_t biped) {
    uint32_t model = 0xFFFFFFFFu;
    std::memcpy(&model, TagAt(tt.Address(biped & 0xFFFF)) + 0x70, 4);
    return TagAt(tt.Address(model & 0xFFFF));
}

uint8_t* DamageInfo(const TagTable& tt, uint32_t biped) {
    uint32_t model = 0xFFFFFFFFu;
    std::memcpy(&model, TagAt(tt.Address(biped & 0xFFFF)) + 0x70, 4);
    if (!tt.Is(model, kHlmt)) return nullptr;
    uint8_t* m = TagAt(tt.Address(model & 0xFFFF));
    int32_t count = 0;
    uint32_t packed = 0;
    std::memcpy(&count, m + kModelDamageInfo, 4);
    std::memcpy(&packed, m + kModelDamageInfo + 4, 4);
    return (count >= 1 && packed) ? TagAt(packed) : nullptr;
}

std::string VitalityText(const uint8_t* info) {
    float body = 0, shield = 0, stun = 0, recharge = 0;
    std::memcpy(&body, info + 0x28, 4);
    std::memcpy(&shield, info + 0x7C, 4);
    std::memcpy(&stun, info + 0x88, 4);
    std::memcpy(&recharge, info + 0x8C, 4);
    char text[128];
    std::snprintf(text, sizeof(text), "body %g, shield %g, shield stun %g s, recharge %g s", body, shield, stun, recharge);
    return text;
}

std::string Hex(const uint8_t* p, size_t n) {
    std::string out;
    char b[12];
    for (size_t i = 0; i < n; i += 4) {
        uint32_t v = 0;
        std::memcpy(&v, p + i, 4);
        std::snprintf(b, sizeof(b), "%s%08X", (i % 32 == 0 && i) ? " | " : " ", v);
        out += b;
    }
    return out;
}

// Diagnostic (Elite toughness): logs the model's damage-related data so the pack Elite and the stock Elite
// can be compared line by line. Raw dwords; " | " every 32 bytes.
void DumpDamageData(const char* label, const TagTable& tt, uint32_t biped) {
    uint32_t model = 0xFFFFFFFFu;
    std::memcpy(&model, TagAt(tt.Address(biped & 0xFFFF)) + 0x70, 4);
    if (!tt.Is(model, kHlmt)) { NOBLE_WARN("ElitePack dump {}: no model", label); return; }
    const uint8_t* m = TagAt(tt.Address(model & 0xFFFF));
    NOBLE_INFO("ElitePack dump {}: biped 0x{:X} model 0x{:X} hlmt[0x00..0x1A0]:{}", label, biped, model, Hex(m, 0x1A0));
    auto block = [&](const uint8_t* base, size_t at, const char* name, size_t stride, int maxItems) {
        int32_t count = 0;
        uint32_t packed = 0;
        std::memcpy(&count, base + at, 4);
        std::memcpy(&packed, base + at + 4, 4);
        if (count <= 0 || !packed) { NOBLE_INFO("ElitePack dump {}: {} (+0x{:X}) empty", label, name, at); return; }
        const uint8_t* e = TagAt(packed);
        for (int i = 0; i < std::min(count, maxItems); ++i)
            NOBLE_INFO("ElitePack dump {}: {} (+0x{:X}) {}/{}:{}", label, name, at, i, count, Hex(e + i * stride, stride));
    };
    block(m, 0xA8, "hlmt block A8", 0x14, 4);
    block(m, 0xB4, "hlmt block B4", 0x14, 4);
    block(m, 0xC0, "damage info", 0x140, 1);
    const uint8_t* info = DamageInfo(tt, biped);
    if (info) {
        block(info, 0xCC, "damage sections", 0x44, 6);
        block(info, 0xD8, "damage info block D8", 0x10, 2);
        block(info, 0xE4, "damage info block E4", 0x10, 2);
    }
    uint32_t coll = 0xFFFFFFFFu;
    std::memcpy(&coll, m + 0x1C, 4);
    if (coll != 0xFFFFFFFFu && (coll & 0xFFFF) < uint32_t(tt.Count()))
        NOBLE_INFO("ElitePack dump {}: coll 0x{:X} [0x00..0x80]:{}", label, coll, Hex(TagAt(tt.Address(coll & 0xFFFF)), 0x80));
}

bool TryDump(const char* label, const TagTable& tt, uint32_t biped) {
    __try {
        DumpDamageData(label, tt, biped);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// The pack's Elite comes with its own damage settings, which make it much tougher than MCC's multiplayer Elite
// (two grenades don't take its shields down). Give it the stock player Elite's settings instead.
// "-elite-pack-own-health" keeps the pack's (test).
void MatchStockDamage(const TagTable& tt, uint32_t packElite, uint32_t stockElite) {
    uint8_t* pack = DamageInfo(tt, packElite);
    const uint8_t* stock = DamageInfo(tt, stockElite);
    if (!pack || !stock) {
        NOBLE_WARN("ElitePack: damage info not found (pack {}, stock {}); the pack Elite keeps its own health",
                   pack != nullptr, stock != nullptr);
        return;
    }
    if (HasArg(L"-elite-pack-dump")) {
        if (!TryDump("stock", tt, stockElite)) NOBLE_WARN("ElitePack: stock Elite dump stopped early");
        if (!TryDump("pack", tt, packElite)) NOBLE_WARN("ElitePack: pack Elite dump stopped early");
    }
    const std::string before = VitalityText(pack);
    if (s_keepPackHealth) {
        NOBLE_INFO("ElitePack: pack Elite keeps its own health (-elite-pack-own-health): {}; stock Elite: {}",
                   before, VitalityText(stock));
        return;
    }
    std::memcpy(pack + 0x04, stock + 0x04, 4);
    std::memcpy(pack + 0x10, stock + 0x10, kDamageCopyEnd - 0x10);

    // The model's own shield strength (hlmt +0xD0, float; +0xCC is a count, 1): the stock multiplayer Elite
    // has 79 (its damage info's shield vitality), the pack's Elite 210, so two grenades never took its shields down.
    float* packShield = reinterpret_cast<float*>(ModelOf(tt, packElite) + kModelShield);
    const float stockShield = *reinterpret_cast<const float*>(ModelOf(tt, stockElite) + kModelShield);
    const float oldShield = *packShield;
    if (stockShield > 0.0f && stockShield < 10000.0f) *packShield = stockShield;
    NOBLE_INFO("ElitePack: pack Elite now uses the stock Elite's damage settings ({}; model shield {:g}, was {:g}; before: {})",
               VitalityText(pack), *packShield, oldShield, before);
}

// Death armour effect. The model's damage sections (hlmt +0xE0, {i32 count, u32 pointer, u32}, 0xC0 bytes each:
// +0x00 name, e.g. "body") carry what happens when a section is used up; the pack Elite's "body" section plays
// objects\characters\spartans\fx\armor_fx\armor_fx_death (in its blocks at +0x1C and +0x84). That effect picks
// the armour effect from the character's "armor_fx" model region, which the pack's Elite doesn't have, so every
// pack Elite played Heart Attack and Grunt Birthday Party. Drop it for the pack Elite only (Elites have no armour
// effect in Noble yet). "-elite-pack-death-fx" keeps it (test).
constexpr uint32_t kEffe = 0x65666665;
constexpr size_t kModelSections = 0xE0, kSectionSize = 0xC0;

void DropDeathArmourEffect(const TagTable& tt, uint32_t packElite, uint32_t bodyName) {
    uint8_t* m = ModelOf(tt, packElite);
    int32_t count = 0;
    uint32_t packed = 0;
    std::memcpy(&count, m + kModelSections, 4);
    std::memcpy(&packed, m + kModelSections + 4, 4);
    if (count <= 0 || count > 32 || !packed) { NOBLE_WARN("ElitePack: pack Elite has no damage sections"); return; }
    uint8_t* sections = TagAt(packed);
    int dropped = 0;
    std::string datums;
    for (int32_t i = 0; i < count; ++i) {
        uint8_t* sec = sections + i * kSectionSize;
        uint32_t name = 0;
        std::memcpy(&name, sec, 4);
        if (name != bodyName) continue;
        for (size_t blockAt : {size_t(0x1C), size_t(0x84)}) {
            int32_t n = 0;
            uint32_t p = 0;
            std::memcpy(&n, sec + blockAt, 4);
            std::memcpy(&p, sec + blockAt + 4, 4);
            if (n <= 0 || n > 8 || !p) continue;
            uint8_t* e = TagAt(p);
            for (size_t k = 0; k + 16 <= size_t(n) * 0x40; k += 4) {
                uint32_t group = 0, datum = 0;
                std::memcpy(&group, e + k, 4);
                if (group != kEffe) continue;
                std::memcpy(&datum, e + k + 12, 4);
                if (datum == 0xFFFFFFFFu) continue;
                if (s_keepDeathEffect) { datums += fmt_hex(datum); continue; }
                const uint32_t none = 0xFFFFFFFFu;
                std::memcpy(e + k + 12, &none, 4);
                datums += fmt_hex(datum);
                ++dropped;
            }
        }
    }
    if (s_keepDeathEffect)
        NOBLE_INFO("ElitePack: pack Elite keeps its death armour effect (-elite-pack-death-fx):{}", datums);
    else
        NOBLE_INFO("ElitePack: dropped {} death armour effect reference(s) from the pack Elite's body section:{}", dropped, datums);
}

bool TryAfterPackStep(uint8_t* tags, uint8_t* cache) {
    __try {
        AfterPackStep(tags, cache);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool TryBeforePackStep(uint8_t* tags) {
    __try {
        BeforePackStep(tags);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uint8_t __fastcall CacheStepHook(void* a1, void* a2, uint32_t a3, void* tags, void* cache, void* a6) {
    bool ours = false;
    if (cache && s_relocPending) {
        if (!TryRelocate(static_cast<uint8_t*>(cache))) {
            s_relocPending = false;
            NOBLE_ERROR("ElitePack: relocation faulted; the pack is left as loaded");
        } else if (!s_relocPending && tags) {
            ours = TryBeforePackStep(static_cast<uint8_t*>(tags));   // Relocate cleared it: this is the pack
        }
    }
    const uint8_t ok = s_cacheStepOriginal(a1, a2, a3, tags, cache, a6);
    if (ours && ok && !TryAfterPackStep(static_cast<uint8_t*>(tags), static_cast<uint8_t*>(cache)))
        NOBLE_ERROR("ElitePack: switching players to the pack's Elite faulted");
    return ok;
}

// Resource adapters. A resource interface (vtable at 0x996978, 14 methods 0x1BB6C0..0x1BBDA0) can
// cover several ranges of resource indices, one sub-object per range; with the pack loaded, its
// resources form extra ranges. These methods expect an index inside a single range, so for an index
// in range k (lo != 0) the call is made on a one-range copy of the interface with the index rebased
// (index - lo), as RR's resadapt does. Context: +0x10 range table (+0x18 ranges {lo, hi}, +0x20 count),
// +0x18 sub-object count, +0x20 sub-objects (16 bytes each).
using ResourceFn = uint64_t(__fastcall*)(uint8_t*, uint64_t, uint64_t, uint64_t);

struct AdapterSlot { uintptr_t rva; bool indexInR8; };
constexpr AdapterSlot kAdapters[14] = {
    {0x1BB6C0, true},  {0x1BB7B4, true},  {0x1BB830, false}, {0x1BBBC4, false}, {0x1BBC30, false},
    {0x1BBC9C, true},  {0x1BBD34, true},  {0x1BBDA0, true},  {0x1BB864, false}, {0x1BB898, false},
    {0x1BB8CC, false}, {0x1BB9B4, false}, {0x1BBA9C, false}, {0x1BBB30, false},
};
ResourceFn s_resourceOriginal[14] = {};

uint64_t Adapt(int slot, uint8_t* ctx, uint64_t rdx, uint64_t r8, uint64_t r9) {
    const ResourceFn original = s_resourceOriginal[slot];
    const bool inR8 = kAdapters[slot].indexInR8;
    const uint32_t index = static_cast<uint32_t>(inR8 ? r8 : rdx);
    uint8_t* table = nullptr;
    uint8_t* subs = nullptr;
    std::memcpy(&table, ctx + 0x10, 8);
    std::memcpy(&subs, ctx + 0x20, 8);
    if (index == 0xFFFFFFFFu || !table || !subs) return original(ctx, rdx, r8, r9);
    const int32_t* ranges = nullptr;
    int32_t count = 0;
    std::memcpy(&ranges, table + 0x18, 8);
    std::memcpy(&count, table + 0x20, 4);
    if (count <= 0 || !ranges) return original(ctx, rdx, r8, r9);
    const int32_t i = static_cast<int32_t>(index & 0xFFFF);
    uint32_t subCount = 0;
    std::memcpy(&subCount, ctx + 0x18, 4);
    for (int32_t k = 0; k < count; ++k) {
        const int32_t lo = ranges[k * 2], hi = ranges[k * 2 + 1];
        if (i - lo < 0 || i >= hi) continue;
        if (static_cast<uint32_t>(k) >= subCount || lo == 0) break;
        int32_t oneRange[2] = {0, hi - lo};
        alignas(16) uint8_t oneTable[0x28] = {};
        const int32_t* oneRangePtr = oneRange;
        const uint32_t one = 1;
        std::memcpy(oneTable + 0x18, &oneRangePtr, 8);
        std::memcpy(oneTable + 0x20, &one, 4);
        alignas(16) uint8_t sub[16];
        std::memcpy(sub, subs + static_cast<size_t>(k) * 16, 16);
        alignas(16) uint8_t copy[0x30];
        std::memcpy(copy, ctx, 0x30);
        uint8_t* tablePtr = oneTable;
        uint8_t* subPtr = sub;
        std::memcpy(copy + 0x10, &tablePtr, 8);
        std::memcpy(copy + 0x18, &one, 4);
        std::memcpy(copy + 0x20, &subPtr, 8);
        const uint64_t rebased = (index & 0xFFFF0000u) | static_cast<uint32_t>(i - lo);
        ++s_adaptedCalls;
        return inR8 ? original(copy, rdx, rebased, r9) : original(copy, rebased, r8, r9);
    }
    return original(ctx, rdx, r8, r9);
}

template <int Slot>
uint64_t __fastcall AdapterHook(uint8_t* ctx, uint64_t rdx, uint64_t r8, uint64_t r9) {
    return Adapt(Slot, ctx, rdx, r8, r9);
}

using AdapterHookFn = uint64_t(__fastcall*)(uint8_t*, uint64_t, uint64_t, uint64_t);
constexpr AdapterHookFn kAdapterHooks[14] = {
    &AdapterHook<0>, &AdapterHook<1>, &AdapterHook<2>, &AdapterHook<3>, &AdapterHook<4>, &AdapterHook<5>, &AdapterHook<6>,
    &AdapterHook<7>, &AdapterHook<8>, &AdapterHook<9>, &AdapterHook<10>, &AdapterHook<11>, &AdapterHook<12>, &AdapterHook<13>,
};

bool InstallAdapters() {
    for (int k = 0; k < 14; ++k) {
        // Each method must still be in the interface's vtable where this build has it.
        uint64_t inTable = 0;
        std::memcpy(&inTable, At<void*>(0x996978 + (k < 3 ? k * 8 : k * 8 + 8)), 8);
        if (inTable != s_base + kAdapters[k].rva) {
            NOBLE_WARN("ElitePack: resource method {} isn't at RVA 0x{:X}", k, kAdapters[k].rva);
            return false;
        }
        if (!HookManager::InstallHook(At<void*>(kAdapters[k].rva), reinterpret_cast<void*>(kAdapterHooks[k]),
                                      reinterpret_cast<void**>(&s_resourceOriginal[k]), "elitepack_resource")) return false;
    }
    return true;
}

// ---- Stage A3: the Armory's six Elite pieces -------------------------------------------------
// The appearance builder 0x1055EC(record, mode, unit, player) reads one byte per armour list from the
// appearance record: +0x10..+0x16 the Spartan piece lists (helmet, left shoulder, right shoulder, chest,
// wrist, utility, knees), +0x17 the Elite set, +0x18 / +0x19 effects. With the pack merged in, the
// pack's Elite pieces ride on the first six lists (helmet, shoulders, chest, forearms, legs), so for an
// Elite a copy of the record gets the six piece indices and knees 0xFF (RR elitepicks). The Elite set
// byte is kept: the map's Elite set still dresses the first-person arms. Calls: spawn 0x4AC36, live change
// 0x447B0, another machine's player 0x3B184F, unit copy 0x524CD5.
constexpr uintptr_t kAppearanceBuilder = 0x1055EC;
constexpr uintptr_t kBuilderCalls[4] = {0x04AC36, 0x0447B0, 0x3B184F, 0x524CD5};
using BuilderFn = uint64_t(__fastcall*)(uint8_t*, uint32_t, void*, void*);

// Set -> pieces table (helmet, left, right, chest, forearms, legs), sets in Armory order (the same
// kEliteSets order as ArmorCustomizer: 0..8 Reach's own + Honor Guard, 9 H2A Ranger, 10 Counselor).
constexpr uint8_t kSetPieces[11][6] = {
    {3, 3, 3, 0, 0, 2},   {7, 7, 7, 4, 5, 5},   {6, 6, 6, 3, 4, 4},   {9, 9, 9, 5, 7, 7},
    {10, 10, 10, 7, 8, 8}, {1, 1, 1, 1, 1, 0},  {8, 8, 8, 6, 6, 6},   {4, 4, 4, 2, 3, 3},   {2, 2, 2, 8, 2, 10},
    {11, 11, 11, 3, 4, 9},  // H2A Ranger: h2a helmet/shoulders/legs, Ranger chest (3) and arms (4)
    {0, 5, 5, 8, 0, 9},     // Counselor: Council helmet, Schism shoulders, Honor Guard chest (8), Minor arms, H2A legs
};

// Armory piece ids -> the pack's index in that slot's list (from the indices above; extras are best guesses).
int PieceIndex(int slot, const std::string& id) {
    static const std::vector<std::string> kHelmet = {"council", "general", "honor_guard", "minor", "officer", "ossoona",
        "space", "spec_ops", "story_elite", "ultra", "zealot", "h2a_space_helmet"};
    static const std::vector<std::string> kShoulder = {"combat", "general", "honor_guard", "minor", "officer", "schism",
        "space", "spec_ops", "story_elite", "ultra", "zealot", "h2a_shoulder"};
    static const std::vector<std::string> kChest = {"minor", "general", "officer", "space", "spec_ops", "ultra",
        "story_elite", "zealot", "honor_guard"};
    static const std::vector<std::string> kForearms = {"default", "general", "honor_guard", "officer", "space",
        "spec_ops", "story_elite", "ultra", "zealot"};
    static const std::vector<std::string> kLegs = {"general", "honor_guard", "minor", "officer", "space", "spec_ops",
        "story_elite", "ultra", "zealot", "h2a_legs", "honor_guard_legs"};
    const std::vector<std::string>* lists[6] = {&kHelmet, &kShoulder, &kShoulder, &kChest, &kForearms, &kLegs};
    std::string want = id;
    // Orders checked against the pack's armour list (2026-10-28): H2A shoulders are piece 11 on both sides,
    // H2A legs 9 and Honor Guard (robe) legs 10.
    if ((slot == 1 || slot == 2) && want.rfind("h2a_", 0) == 0) want = "h2a_shoulder";
    if (slot == 4 && want == "minor") want = "default";
    const auto& l = *lists[slot];
    const auto it = std::find(l.begin(), l.end(), want);
    return it == l.end() ? -1 : static_cast<int>(it - l.begin());
}

// Elite set (0..7, pmcg list +0x74 order) whose first-person version matches a pack piece, or -1 when the
// piece has none (Honor Guard, H2A, Council, Ossoona, Schism, Combat). slot: 1 left shoulder, 4 forearms.
int SetOfPiece(int slot, int index) {
    static const char* kSetIds[8] = {"minor", "spec_ops", "space", "ultra", "zealot", "general", "story_elite", "officer"};
    static const char* kShoulder[] = {"combat", "general", "honor_guard", "minor", "officer", "schism", "space",
                                      "spec_ops", "story_elite", "ultra", "zealot", "h2a_shoulder"};
    static const char* kForearms[] = {"minor" /* default */, "general", "honor_guard", "officer", "space",
                                      "spec_ops", "story_elite", "ultra", "zealot"};
    const char* id = nullptr;
    if (slot == 1 && index >= 0 && index < int(std::size(kShoulder))) id = kShoulder[index];
    if (slot == 4 && index >= 0 && index < int(std::size(kForearms))) id = kForearms[index];
    if (!id) return -1;
    for (int k = 0; k < 8; ++k) if (std::strcmp(kSetIds[k], id) == 0) return k;
    return -1;
}

std::filesystem::path s_appearancePath;
std::mutex s_picksMutex;
uint8_t s_localPicks[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t s_localEliteEffect = 0xFF;
int s_localSet = -1;
uint8_t s_localColors[4] = {1, 10, 14, 10};
uint8_t s_localEmblem[8] = {0, 0, 0, 0, 0, 17, 0, 0};
uint8_t s_localCustomFlags = 0;
float s_localCustomRgb[4][3] = {};
bool s_localSpeciesElite = false;   // appearance.json "species" == 1 (the local player's preferred species)
std::atomic<int> s_localSettingsIndex{-1};   // settings index the engine last applied the local profile to
std::filesystem::file_time_type s_picksStamp{};
std::atomic<int> s_picksLogged{0};

// Re-reads data\appearance.json when it changes (an Armory save mid-match reaches the next spawn).
void RefreshLocalPicks() {
    std::error_code ec;
    const auto stamp = std::filesystem::last_write_time(s_appearancePath, ec);
    if (ec || stamp == s_picksStamp) return;
    s_picksStamp = stamp;
    nlohmann::json j;
    if (!SaveCodec::ReadJson(s_appearancePath, j)) return;
    try {
        const auto& el = j.at("elite");
        s_localSpeciesElite = j.value("species", 0) == 1;
        const int set = std::clamp(el.value("armor_set", 0), 0, static_cast<int>(std::size(kSetPieces)) - 1);
        static const char* kKeys[6] = {"helmet_id", "left_shoulder_id", "right_shoulder_id", "chest_id", "arms_id", "legs_id"};
        for (int k = 0; k < 6; ++k) {
            const int idx = PieceIndex(k, el.value(kKeys[k], std::string()));
            s_localPicks[k] = static_cast<uint8_t>(idx >= 0 ? idx : kSetPieces[set][k]);
        }
        s_localSet = set;

        int fxIdx = el.value("armor_effect_index", 0);
        const std::string fxId = el.value("armor_effect_id", std::string());
        if (fxId == "flames" || fxIdx == 5) fxIdx = 1;
        else if (fxId == "blue_flames" || fxIdx == 6) fxIdx = 2;
        else if (fxId == "heart_attack" || fxIdx == 2) fxIdx = 3;
        else if (fxId == "pestilence" || fxIdx == 3) fxIdx = 4;
        else if (fxId == "inclement_weather" || fxIdx == 4) fxIdx = 5;
        else if (fxId == "birthday_party" || fxIdx == 1) fxIdx = 6;
        else fxIdx = 0;
        s_localEliteEffect = (fxIdx >= 1 && fxIdx <= 6) ? static_cast<uint8_t>(fxIdx) : 0xFF;

        const bool useElColors = el.contains("primary_color");
        const auto& colSrc = useElColors ? el : j;

        s_localColors[0] = static_cast<uint8_t>(std::clamp(colSrc.value("primary_color", 1), 0, 29));
        s_localColors[1] = static_cast<uint8_t>(std::clamp(colSrc.value("secondary_color", 10), 0, 29));
        s_localColors[2] = static_cast<uint8_t>(std::clamp(colSrc.value("accent_color", 14), 0, 29));
        s_localColors[3] = static_cast<uint8_t>(std::clamp(colSrc.value("lights_color", 10), 0, 29));

        s_localCustomFlags = 0;
        if (colSrc.value("primary_custom", false))   s_localCustomFlags |= (1 << 0);
        if (colSrc.value("secondary_custom", false)) s_localCustomFlags |= (1 << 1);
        if (colSrc.value("accent_custom", false))    s_localCustomFlags |= (1 << 2);
        if (colSrc.value("lights_custom", false))    s_localCustomFlags |= (1 << 3);

        auto readRgb = [&](const char* key, float out[3]) {
            if (colSrc.contains(key) && colSrc[key].is_array() && colSrc[key].size() == 3) {
                for (int i = 0; i < 3; ++i) out[i] = std::clamp(colSrc[key][i].get<float>(), 0.0f, 1.0f);
            }
        };
        readRgb("primary_custom_rgb", s_localCustomRgb[0]);
        readRgb("secondary_custom_rgb", s_localCustomRgb[1]);
        readRgb("accent_custom_rgb", s_localCustomRgb[2]);
        readRgb("lights_custom_rgb", s_localCustomRgb[3]);
        s_localEmblem[0] = static_cast<uint8_t>(std::clamp(j.value("emblem_index", 0), 0, 255));
        s_localEmblem[1] = static_cast<uint8_t>(std::clamp(j.value("emblem_background_index", 0), 0, 255));
        s_localEmblem[2] = 0;
        s_localEmblem[3] = static_cast<uint8_t>(std::clamp(j.value("emblem_primary_color", 0), 0, 29));
        s_localEmblem[4] = static_cast<uint8_t>(std::clamp(j.value("emblem_secondary_color", 0), 0, 29));
        s_localEmblem[5] = static_cast<uint8_t>(std::clamp(j.value("emblem_background_color", 17), 0, 29));
        s_localEmblem[6] = 0;
        s_localEmblem[7] = 0;

        NOBLE_INFO("ElitePack: Armory Elite pieces {} {} {} {} {} {} (set {}), colors {}/{}/{}/{}",
                   s_localPicks[0], s_localPicks[1], s_localPicks[2], s_localPicks[3],
                   s_localPicks[4], s_localPicks[5], set,
                   s_localColors[0], s_localColors[1], s_localColors[2], s_localColors[3]);
    } catch (...) {
    }
}

// ---- Replicated Elite appearance ------------------------------------------------------------
// The engine's appearance record (player + 0xD8, 0x28 bytes) is also the per-player block it syncs
// between machines: 0x2C837C(player, block) copies a 0x20-byte block starting at record+0x10 into
// player+0xE8 and pushes it to the profile manager (0x3A9E4 -> settings +0x310), and 0x2C82F0 hands
// that block back for the settings sync. Layout, verified in the builder 0x1055EC:
//   +0x00 flags, +0x04 species, +0x10..+0x16 seven armour-list indices, +0x17 Elite armor set,
//   +0x18 Spartan armor effect, +0x19 Elite armor effect.
// For an Elite the seven list indices are the player's SPARTAN pieces and the pack reuses them for the
// six Elite piece indices (the pack merges Elite-region changes into the same-named selections), with
// 0xFF in +0x16 as a marker. Writing the marker + pieces into the local player's record makes the
// engine replicate the sender's own Elite pieces; the receiving machine then no longer has to guess
// with its own Armory picks (the old behaviour made every Elite look like the local player).
constexpr int kRecordPieceBase = 0x10;
constexpr int kRecordEliteSet = 0x17;
constexpr int kRecordEliteFx = 0x19;
constexpr uint8_t kElitePiecesMarker = 0xFF;
constexpr uintptr_t kPlayerRecordOffset = 0xD8;

// Return addresses of the 0x1055EC call sites. Only spawn (0x04AC36) and live change (0x0447B0) pass
// the local player's own record (player + 0xD8) straight from the player struct; 0x3B184F (another
// machine's player) and 0x524CD5 (unit copy) pass a network/copy record and must keep its bytes.
constexpr uintptr_t kBuilderReturnSpawn = 0x04AC3B;
constexpr uintptr_t kBuilderReturnLive  = 0x0447B5;

constexpr uintptr_t kApplyProfile = 0x2EDA08;   // void(int settingsIndex, uint8_t* player)
using ApplyProfileFn = void(__fastcall*)(int, uint8_t*);
using RecordSetFn = void(__fastcall*)(uint8_t*, uint8_t*);
static ApplyProfileFn s_applyProfileOriginal = nullptr;
static RecordSetFn s_recordSetOriginal = nullptr;
constexpr uintptr_t kRecordSet = 0x2C837C;   // void(playerSettings*, uint8_t block[0x20])

BuilderFn s_builder = nullptr;

// Second chance for the settings sync: 0x2C837C(player, block) is the call the engine makes when it
// pushes a player's appearance block to the profile manager. ApplyProfileHook runs while the map is
// still loading (before the pack is in), so its push is usually too early; this hook catches the sync
// afterwards. The settings index recorded by ApplyProfileHook tells the local player's block apart from
// the other players' blocks, so a remote player's appearance is never overwritten with local picks.
void __fastcall RecordSetHook(uint8_t* player, uint8_t* block) {
    int localIndex = s_localSettingsIndex.load();
    if (localIndex < 0) {
        // Fallback if RecordSetHook fires before ApplyProfileHook records the index:
        // Client connects with -connect as guest slot (typically 1 or 0), host starts at 0.
        localIndex = (wcsstr(GetCommandLineW(), L"-connect") != nullptr) ? 1 : 0;
    }
    if (player && block && s_loaded && s_haveActive && localIndex >= 0) {
        int32_t index = -1;
        std::memcpy(&index, player + 0x10, sizeof(index));
        if (index == localIndex) {
            std::lock_guard<std::mutex> lock(s_picksMutex);
            RefreshLocalPicks();
            if (s_localSpeciesElite) {
                const int set = (s_localSet >= 0 && s_localSet < static_cast<int>(std::size(kSetPieces)))
                                    ? s_localSet : 0;
                uint8_t picks[6];
                for (int k = 0; k < 6; ++k)
                    picks[k] = s_localPicks[k] != 0xFF ? s_localPicks[k] : kSetPieces[set][k];
                std::memcpy(block, picks, 6);
                block[6] = kElitePiecesMarker;
                block[7] = static_cast<uint8_t>(set);
                if (s_localEliteEffect >= 1 && s_localEliteEffect <= 6) block[9] = s_localEliteEffect;
                block[10] = s_localColors[0];
                block[11] = s_localColors[1];
                block[12] = s_localColors[2];
                block[13] = s_localColors[3];
                block[14] = s_localCustomFlags;
                for (int c = 0; c < 4; ++c) {
                    block[16 + c * 3 + 0] = static_cast<uint8_t>(std::clamp(std::lround(s_localCustomRgb[c][0] * 255.0f), 0L, 255L));
                    block[16 + c * 3 + 1] = static_cast<uint8_t>(std::clamp(std::lround(s_localCustomRgb[c][1] * 255.0f), 0L, 255L));
                    block[16 + c * 3 + 2] = static_cast<uint8_t>(std::clamp(std::lround(s_localCustomRgb[c][2] * 255.0f), 0L, 255L));
                }
                static std::atomic<int> s_synced{0};
                if (s_synced.fetch_add(1) < 6)
                    NOBLE_INFO("ElitePack: Elite pieces & colors written to settings sync block (player index {}):{}", index,
                               Hex(block, 0x20));
            }
        }
    }
    if (s_recordSetOriginal) s_recordSetOriginal(player, block);
}

// 0x2EDA08 (profile -> player copy) is called only for the local player and knows its settings index.
// The index is what tells the local player's appearance block apart from the other players' blocks in
// RecordSetHook; the block itself is rewritten there, after the pack has loaded.
void __fastcall ApplyProfileHook(int settingsIndex, uint8_t* player) {
    if (s_applyProfileOriginal) s_applyProfileOriginal(settingsIndex, player);
    if (settingsIndex >= 0 && settingsIndex < 16) s_localSettingsIndex.store(settingsIndex);
    if (player) {
        std::lock_guard<std::mutex> lock(s_picksMutex);
        RefreshLocalPicks();
        std::memcpy(player + 0x9E, s_localEmblem, 8);
        std::memcpy(player + kPlayerRecordOffset + 0x08, s_localEmblem, 8);
    }
}

// Inspects the player datum associated with an appearance record to verify if it belongs
// to the local machine's player. Separate function with no C++ objects with destructors
// so MSVC allows __try / __except under /EHsc.
static bool IsLocalPlayerRecord(const uint8_t* record, uintptr_t caller) {
    if (!record) return false;
    if (caller == kBuilderReturnLive) return true;
    if (caller != kBuilderReturnSpawn) return false;

    __try {
        const uint8_t* p = record - kPlayerRecordOffset;
        const uint32_t flags = *reinterpret_cast<const uint32_t*>(p + 4);
        const uint8_t isLocalByte = p[0x2DE];
        // In Blam! Reach, bit 3 of flags (0x08: _player_is_local_machine_bit) and p[0x2DE] == 1
        // indicate that this player datum belongs to the local machine.
        // Remote peers have flags & 0x08 == 0 and p[0x2DE] == 0.
        return ((flags & 0x08) != 0) || (isLocalByte == 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uint64_t __fastcall BuilderHook(uint8_t* record, uint32_t mode, void* unit, void* player) {
    const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress()) - s_base;
    const bool localRecord = IsLocalPlayerRecord(record, caller);
    // Mirrors the builder: mode 0 forces Spartan, mode 1 forces Elite, 2 returns, otherwise record[4].
    const bool elite = record && mode != 2 && (mode == 1 || (mode != 0 && record[4] == 1));
    static std::atomic<int> s_recordsLogged{0};
    if (record && s_recordsLogged.fetch_add(1) < 12)
        NOBLE_INFO("ElitePack: appearance record (caller 0x{:X}, mode {}, elite {}, local {}):{}",
                   caller, mode, elite, localRecord, Hex(record, 0x28));
    if (localRecord && record) {
        std::lock_guard<std::mutex> lock(s_picksMutex);
        RefreshLocalPicks();
        std::memcpy(record + 0x08, s_localEmblem, 8);
    }
    if (!elite || !s_loaded || !s_haveActive) return s_builder(record, mode, unit, player);

    alignas(8) uint8_t copy[0x28];
    std::memcpy(copy, record, sizeof(copy));

    uint8_t picks[6] = {0, 0, 0, 0, 0, 0};
    uint8_t eliteEffect = 0xFF;
    int effectiveSet = (record[kRecordEliteSet] < static_cast<int>(std::size(kSetPieces)))
                           ? record[kRecordEliteSet] : 0;

    if (localRecord) {
        std::lock_guard<std::mutex> lock(s_picksMutex);
        RefreshLocalPicks();
        if (s_localSet >= 0 && s_localSet < static_cast<int>(std::size(kSetPieces))) effectiveSet = s_localSet;
        for (int k = 0; k < 6; ++k)
            picks[k] = s_localPicks[k] != 0xFF ? s_localPicks[k] : kSetPieces[effectiveSet][k];
        eliteEffect = s_localEliteEffect;
        if (eliteEffect == 0xFF && record[kRecordEliteFx] >= 1 && record[kRecordEliteFx] <= 6)
            eliteEffect = record[kRecordEliteFx];
        // Store the sender's picks in the real record: the engine syncs this block to every machine.
        std::memcpy(record + kRecordPieceBase, picks, 6);
        record[kRecordPieceBase + 6] = kElitePiecesMarker;
        record[kRecordEliteSet] = static_cast<uint8_t>(effectiveSet);
        if (eliteEffect != 0xFF) record[kRecordEliteFx] = eliteEffect;
        record[0x1A] = s_localColors[0];
        record[0x1B] = s_localColors[1];
        record[0x1C] = s_localColors[2];
        record[0x1D] = s_localColors[3];
    } else if (record[kRecordPieceBase + 6] == kElitePiecesMarker) {
        // The record came from its owner (another machine or a unit copy): use their pieces.
        std::memcpy(picks, record + kRecordPieceBase, 6);
        if (record[kRecordEliteSet] < static_cast<int>(std::size(kSetPieces))) {
            effectiveSet = record[kRecordEliteSet];
        }
        eliteEffect = record[kRecordEliteFx];
    } else {
        // Old/unmodified record (no marker): fall back to the Elite armor set the engine replicated.
        for (int k = 0; k < 6; ++k) picks[k] = kSetPieces[effectiveSet][k];
        eliteEffect = record[kRecordEliteFx];
    }
    if (eliteEffect < 1 || eliteEffect > 6) eliteEffect = 0xFF;

    std::memcpy(copy + kRecordPieceBase, picks, 6);
    copy[kRecordPieceBase + 6] = kElitePiecesMarker;
    copy[kRecordEliteSet] = static_cast<uint8_t>(effectiveSet);
    if (eliteEffect != 0xFF) copy[kRecordEliteFx] = eliteEffect;
    if (localRecord) {
        // first-person arms follow the forearms, the first-person body the shoulders (else the Armor Set)
        // ONLY update first-person meshes for the local machine's player!
        const int fpSet = effectiveSet < 8 ? effectiveSet : 0;
        const int arms = SetOfPiece(4, picks[4]), body = SetOfPiece(1, picks[1]);
        ApplyFirstPersonSet(0, arms >= 0 ? arms : fpSet);
        ApplyFirstPersonSet(1, body >= 0 ? body : fpSet);
    }
    if (s_picksLogged.fetch_add(1) < 12)
        NOBLE_INFO("ElitePack: Elite dressed with pieces {} {} {} {} {} {} (mode {}, set {}, fx {}, local {})",
                   picks[0], picks[1], picks[2], picks[3], picks[4], picks[5], mode, copy[kRecordEliteSet],
                   eliteEffect, localRecord);
    return s_builder(copy, mode, unit, player);
}

bool InstallPicks(const std::filesystem::path& dataDir) {
    s_appearancePath = dataDir / "appearance.dat";
    void* original = nullptr;
    for (uintptr_t site : kBuilderCalls) {
        void* t = RetargetCall(site, kAppearanceBuilder, reinterpret_cast<void*>(&BuilderHook));
        if (!t) { NOBLE_WARN("ElitePack: appearance call at 0x{:X} not redirected", site); continue; }
        original = t;
    }
    s_builder = reinterpret_cast<BuilderFn>(original);
    // The profile -> player copy (local player only, records the settings index).
    if (!HookManager::InstallHook(At<void*>(kApplyProfile), reinterpret_cast<void*>(&ApplyProfileHook),
                                  reinterpret_cast<void**>(&s_applyProfileOriginal), "elitepack_apply_profile"))
        NOBLE_WARN("ElitePack: profile hook (0x{:X}) not installed; other players keep the set only", kApplyProfile);
    if (!HookManager::InstallHook(At<void*>(kRecordSet), reinterpret_cast<void*>(&RecordSetHook),
                                  reinterpret_cast<void**>(&s_recordSetOriginal), "elitepack_settings_sync"))
        NOBLE_WARN("ElitePack: settings-sync hook (0x{:X}) not installed; other players keep the set only", kRecordSet);
    return s_builder != nullptr;
}

bool HasArg(const wchar_t* name) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool found = false;
    for (int i = 1; argv && i < argc; ++i) if (_wcsicmp(argv[i], name) == 0) found = true;
    if (argv) LocalFree(argv);
    return found;
}

} // namespace

std::optional<std::filesystem::path> ResolveFile(const std::wstring& requested) {
    if (s_packPath.empty()) return std::nullopt;
    std::wstring want = std::filesystem::path(requested).filename().wstring();
    std::wstring have = s_packPath.filename().wstring();
    std::transform(want.begin(), want.end(), want.begin(), ::towlower);
    std::transform(have.begin(), have.end(), have.begin(), ::towlower);
    if (want == have) return s_packPath;
    return std::nullopt;
}

bool Loaded() { return s_loaded; }

void Install(HMODULE engine, const std::filesystem::path& dataDir, const std::string& mode) {
    if (!engine || s_base) return;
    if (mode != "multiplayer" && mode != "firefight" && mode != "forge") return;
    if (HasArg(L"-no-elite-pack")) {
        NOBLE_INFO("ElitePack: off (-no-elite-pack)");
        return;
    }
    s_keepPackList = HasArg(L"-elite-pack-list");
    s_keepPackHealth = HasArg(L"-elite-pack-own-health");
    s_keepPackGlobals = HasArg(L"-elite-pack-keep-globals");
    s_keepDeathEffect = HasArg(L"-elite-pack-death-fx");
    s_fpOverride = !HasArg(L"-no-fp-override");
    // First <name>.map in data\armor that has a .rrreloc sidecar.
    std::error_code ec;
    const auto folder = dataDir / "armor";
    for (const auto& e : std::filesystem::directory_iterator(folder, ec)) {
        if (!e.is_regular_file(ec) || e.path().extension() != ".map") continue;
        auto sidecar = e.path();
        sidecar += ".rrreloc";
        if (std::filesystem::is_regular_file(sidecar, ec)) { s_packPath = e.path(); break; }
    }
    if (s_packPath.empty()) {
        NOBLE_INFO("ElitePack: no pack in {} (players use the map's Elite)", folder.string());
        return;
    }
    auto sidecar = s_packPath;
    sidecar += ".rrreloc";
    if (!ReadReloc(sidecar, s_reloc)) { s_packPath.clear(); return; }
    s_packStem = s_packPath.stem().string();
    s_base = reinterpret_cast<uintptr_t>(engine);

    static const uint8_t kMovEdx1[2] = {0x8B, 0xD2}, kMovEdx2[2] = {0x89, 0xD2}, kNop2[2] = {0x66, 0x90};
    const bool patched = PatchCode(kDeltaTruncation, kMovEdx1, kNop2, 2) || PatchCode(kDeltaTruncation, kMovEdx2, kNop2, 2);
    // Elite armour slots (RR --elite-armor-slots): the appearance builder 0x1055EC applies only the Elite
    // set list (+0x74) to Elites ("cmp esi,7; sete r15b" at 0x105796). With the pack, Elites take their
    // pieces from the Spartan piece lists (which now carry the pack's Elite changes), so lists 0..7 apply
    // ("cmp esi,8; setb r15b"). Not 8+ (+0x80 Spartan armour effects, +0x8C Elite armour effects, +0x98): with
    // every list on (RR's "mov r15b,1"), Elites played Heart Attack and Birthday Party at once from leftover bytes.
    static const uint8_t kSlotCheck[7] = {0x83, 0xFE, 0x07, 0x41, 0x0F, 0x94, 0xC7};
    static const uint8_t kSlotOld[7]   = {0x83, 0xFE, 0x08, 0x41, 0x0F, 0x92, 0xC7};
    static const uint8_t kSlotAll[7]   = {0x83, 0xFE, 0x08, 0x41, 0x0F, 0x95, 0xC7};
    const bool slots = PatchCode(0x105796, kSlotCheck, kSlotAll, 7) ||
                       PatchCode(0x105796, kSlotOld,   kSlotAll, 7);
    static const uint8_t kStepPrologue[7] = {0x48, 0x8B, 0xC4, 0x4C, 0x89, 0x48, 0x20};
    const bool stepOk = std::memcmp(At<void*>(kCacheStep), kStepPrologue, 7) == 0;
    if (!patched || !stepOk || !slots) {
        NOBLE_WARN("ElitePack: haloreach.dll doesn't look as expected (patch {}, step {}, slots {}); pack not used",
                   patched, stepOk, slots);
        s_packPath.clear();
        return;
    }
    // Order matters: the map-load detour only adds the pack once every other hook is in place (s_ready).
    const bool adapters = InstallAdapters();
    const bool stepHooked = adapters && HookManager::InstallHook(At<void*>(kCacheStep), reinterpret_cast<void*>(&CacheStepHook),
                                                     reinterpret_cast<void**>(&s_cacheStepOriginal), "elitepack_cache_step");
    static const uint8_t kRebuildPrologue[7] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10};
    if (stepHooked && std::memcmp(At<void*>(kZoneRebuild), kRebuildPrologue, 7) == 0)
        HookManager::InstallHook(At<void*>(kZoneRebuild), reinterpret_cast<void*>(&RebuildHook),
                                 reinterpret_cast<void**>(&s_rebuildOriginal), "elitepack_zone_rebuild");
    if (stepHooked && !s_rebuildOriginal) NOBLE_WARN("ElitePack: zone-set rebuild hook not installed");
    if (stepHooked) s_extraLoad = reinterpret_cast<ExtraLoadFn>(RetargetCall(kExtraCallSite, kExtraLoad, reinterpret_cast<void*>(&ExtraLoadHook)));
    if (s_extraLoad) s_mapLoad = reinterpret_cast<MapLoadFn>(RetargetCall(kLoadCallSite, kMapLoad, reinterpret_cast<void*>(&MapLoadHook)));
    if (!stepHooked || !s_extraLoad || !s_mapLoad) {
        NOBLE_ERROR("ElitePack: hooks failed (adapters {}, step {}, extra {}, load {}); the pack is not used",
                    adapters, stepHooked, s_extraLoad != nullptr, s_mapLoad != nullptr);
        s_packPath.clear();
        return;
    }
    if (!InstallPicks(dataDir)) NOBLE_WARN("ElitePack: Armory Elite pieces won't reach the game (calls not redirected)");
    s_ready = true;
    NOBLE_INFO("ElitePack: {} ({} packed pointers to relocate) will load with {} matches",
               s_packPath.string(), s_reloc.offsets.size(), mode);
}

} // namespace ElitePack
} // namespace noble
