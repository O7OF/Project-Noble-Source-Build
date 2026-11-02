#include "VFS.hpp"
#include "hooks/HookManager.hpp"
#include "common/Logger.hpp"
#include "engine/EmbeddedBlocks.hpp"
#include <fstream>

#include <algorithm>
#include <cctype>

namespace noble {

VFS::CreateFileW_Fn VFS::s_origCreateFileW = nullptr;
VFS::GetFileAttributesW_Fn VFS::s_origGetFileAttributesW = nullptr;
VFS::GetFileAttributesExW_Fn VFS::s_origGetFileAttributesExW = nullptr;

fs::path VFS::s_baseMccPath;
fs::path VFS::s_userStoragePath;
fs::path VFS::s_workshopPath;
fs::path VFS::s_activeMod;
fs::path VFS::s_mapOverrideFile;
std::string VFS::s_mapOverrideCarrier;
bool VFS::s_isInitialized = false;
std::mutex VFS::s_mutex;

static thread_local bool t_inVfsHook = false;

static std::wstring NormalizePathString(std::wstring pathStr) {
    std::replace(pathStr.begin(), pathStr.end(), L'/', L'\\');
    return pathStr;
}

// Low-level existence check that directly invokes the unhooked original Win32 API
bool VFS::RawPathExists(const std::wstring& path) {
    if (!s_origGetFileAttributesW) return false;
    DWORD attr = s_origGetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);
}

// Fast filter: only intercept game assets (maps, variants, audio banks)
// Never intercept system libraries, DirectX drivers, or binaries
static bool IsGameAssetPath(const std::wstring& path) {
    std::wstring lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    if (lower.find(L"\\windows\\") != std::wstring::npos ||
        lower.find(L"\\system32\\") != std::wstring::npos ||
        lower.find(L".dll") != std::wstring::npos ||
        lower.find(L".exe") != std::wstring::npos) {
        return false;
    }

    return (lower.find(L".map") != std::wstring::npos ||
            lower.find(L".bin") != std::wstring::npos ||
            lower.find(L".mvar") != std::wstring::npos ||
            lower.find(L".bank") != std::wstring::npos ||
            lower.find(L".bik") != std::wstring::npos ||
            lower.find(L".bk2") != std::wstring::npos ||
            lower.find(L"maps\\") != std::wstring::npos ||
            lower.find(L"fmod\\") != std::wstring::npos ||
            lower.find(L"haloreach") != std::wstring::npos);
}

bool VFS::Initialize(const ReachInstallInfo& installInfo, const fs::path& userStoragePath) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_isInitialized) return true;

    s_baseMccPath = fs::absolute(installInfo.reachDirectory);
    s_userStoragePath = fs::absolute(userStoragePath);
    s_workshopPath = fs::absolute(installInfo.workshopDirectory);

    // Create user storage subfolders for mods, saves, and logs
    std::error_code ec;
    fs::create_directories(s_userStoragePath / "maps", ec);
    fs::create_directories(s_userStoragePath / "mods", ec);
    fs::create_directories(s_userStoragePath / "game_variants", ec);
    fs::create_directories(s_userStoragePath / "map_variants", ec);

    NOBLE_INFO("Initializing Virtual File System (VFS):");
    NOBLE_INFO(" - Base MCC Read-Only Path:  {}", s_baseMccPath.string());
    NOBLE_INFO(" - User Storage Output Path: {}", s_userStoragePath.string());
    NOBLE_INFO(" - Workshop Content Path:    {}", s_workshopPath.string());

    s_origCreateFileW = &::CreateFileW;
    s_origGetFileAttributesW = &::GetFileAttributesW;
    s_origGetFileAttributesExW = &::GetFileAttributesExW;

    s_isInitialized = true;
    NOBLE_INFO("VFS initialized. Ready to hook target engine modules.");
    return true;
}

void VFS::Shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_isInitialized) return;

    UnhookTargetModule();
    s_isInitialized = false;
    NOBLE_INFO("VFS shutdown complete.");
}

bool VFS::HasActiveMod() { return !s_activeMod.empty(); }

void VFS::SetActiveMod(const fs::path& modFolder) {
    std::error_code ec;
    s_activeMod = modFolder.empty() ? fs::path() : fs::absolute(modFolder, ec);
    if (!s_activeMod.empty()) NOBLE_INFO("VFS: mod active, reading its files first: {}", s_activeMod.string());
}

void VFS::SetMapOverride(const std::string& carrierScenario, const fs::path& mapFile) {
    std::error_code ec;
    s_mapOverrideFile = mapFile.empty() ? fs::path() : fs::absolute(mapFile, ec);
    s_mapOverrideCarrier.clear();
    for (char c : carrierScenario) {
        if (c == '/' || c == '\\') break;
        s_mapOverrideCarrier.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (s_mapOverrideCarrier.size() > 4 &&
        s_mapOverrideCarrier.compare(s_mapOverrideCarrier.size() - 4, 4, ".map") == 0) {
        s_mapOverrideCarrier.resize(s_mapOverrideCarrier.size() - 4);
    }
    if (!s_mapOverrideFile.empty() && !s_mapOverrideCarrier.empty()) {
        NOBLE_INFO("VFS: map override, serving '{}' for haloreach\\maps\\{}.map", s_mapOverrideFile.string(),
                   s_mapOverrideCarrier);
    }
}

void VFS::ClearMapOverride() {
    s_mapOverrideFile.clear();
    s_mapOverrideCarrier.clear();
}

std::wstring VFS::ResolvePath(const std::wstring& requestedPath, DWORD desiredAccess) {
    if (requestedPath.empty()) return requestedPath;

    try {
        std::wstring normalized = NormalizePathString(requestedPath);
        fs::path p(normalized);

        // Check if it's a writing operation
        bool isWriting = (desiredAccess & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA)) != 0;

        // Check if requested path starts with or is inside base MCC path
        std::wstring baseStr = s_baseMccPath.wstring();
        bool isTargetingMcc = false;

        std::error_code ec;
        if (p.is_absolute()) {
            if (!baseStr.empty() && normalized.rfind(baseStr, 0) == 0) {
                isTargetingMcc = true;
                p = fs::relative(p, s_baseMccPath, ec);
                if (ec) {
                    std::wstring sub = normalized.substr(baseStr.length());
                    if (!sub.empty() && (sub[0] == L'\\' || sub[0] == L'/')) {
                        sub = sub.substr(1);
                    }
                    p = fs::path(sub);
                }
            }
        } else {
            isTargetingMcc = true;
            // Strip leading "haloreach\" or "haloreach/" from relative path
            // s_baseMccPath already points to "<mccRoot>\haloreach", so prepending it without stripping
            // resulted in ".../haloreach/haloreach/maps/fonts/font_package_icon.bin" (double haloreach).
            std::wstring pStr = p.wstring();
            std::wstring pLower = pStr;
            std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::towlower);
            if (pLower.rfind(L"haloreach\\", 0) == 0) {
                p = fs::path(pStr.substr(10));
            } else if (pLower.rfind(L"haloreach/", 0) == 0) {
                p = fs::path(pStr.substr(10));
            }
        }

        if (isTargetingMcc) {
            // 0. A mod map served in place of a stock map (the engine picked the stock map by map id).
            // This wins over the active mod and user storage so a same-named stock file cannot shadow it.
            if (!isWriting && !s_mapOverrideFile.empty() && !s_mapOverrideCarrier.empty()) {
                std::wstring requested = p.wstring();
                std::transform(requested.begin(), requested.end(), requested.begin(), ::towlower);
                std::wstring carrier(s_mapOverrideCarrier.begin(), s_mapOverrideCarrier.end());
                const std::wstring withFolder = L"maps\\" + carrier + L".map";
                const std::wstring withoutFolder = carrier + L".map";
                const bool matches =
                    (requested.size() >= withFolder.size() &&
                     requested.compare(requested.size() - withFolder.size(), withFolder.size(), withFolder) == 0) ||
                    requested == withoutFolder;
                if (matches && RawPathExists(s_mapOverrideFile.wstring())) {
                    return s_mapOverrideFile.wstring();
                }
            }

            // 1. If writing, REDIRECT to User Storage! Never write to base MCC.
            if (isWriting) {
                fs::path redirected = s_userStoragePath / p;
                fs::create_directories(redirected.parent_path(), ec);
                return redirected.wstring();
            }

            // 2. If reading:
            std::wstring normLower = normalized;
            std::transform(normLower.begin(), normLower.end(), normLower.begin(), ::towlower);
            if (false ||
                false ||
                normLower.find(L"mainmenu.map") != std::wstring::npos) {
                fs::path customMenu = s_userStoragePath / "menumaps" / "mainmenu_all.map";
                if (!RawPathExists(customMenu.wstring())) {
                    size_t mapSize = 0;
                    const uint8_t* mapData = EmbeddedBlocks::GetBlock("mainmenu_all.map", &mapSize);
                    if (mapData && mapSize > 0) {
                        fs::create_directories(customMenu.parent_path(), ec);
                        std::ofstream out(customMenu, std::ios::binary);
                        if (out) {
                            out.write(reinterpret_cast<const char*>(mapData), static_cast<std::streamsize>(mapSize));
                            out.close();
                            NOBLE_INFO("VFS: Auto-extracted embedded mainmenu_all.map ({} bytes) to {}", mapSize, customMenu.string());
                        }
                    }
                }
                static const char* const kMenuCsvs[] = {
                    "mainmenu_all.string_ids.csv",
                    "mainmenu_armory.string_ids.csv",
                    "mainmenu_armory_elite.string_ids.csv"
                };
                for (const char* csvName : kMenuCsvs) {
                    fs::path csvPath = s_userStoragePath / "menumaps" / csvName;
                    if (!RawPathExists(csvPath.wstring())) {
                        size_t csvSize = 0;
                        const uint8_t* csvData = EmbeddedBlocks::GetBlock(csvName, &csvSize);
                        if (csvData && csvSize > 0) {
                            fs::create_directories(csvPath.parent_path(), ec);
                            std::ofstream out(csvPath, std::ios::binary);
                            if (out) {
                                out.write(reinterpret_cast<const char*>(csvData), static_cast<std::streamsize>(csvSize));
                                out.close();
                            }
                        }
                    }
                }
                if (RawPathExists(customMenu.wstring())) {
                    return customMenu.wstring();
                }
            }

            // Priority 0: the mod chosen in the front-end menu.
            if (!s_activeMod.empty()) {
                fs::path modFile = s_activeMod / p;
                if (RawPathExists(modFile.wstring())) {
                    return modFile.wstring();
                }
            }

            // Priority A: Check user storage (custom downloaded mods / custom variants)
            fs::path userOverride = s_userStoragePath / p;
            if (RawPathExists(userOverride.wstring())) {
                return userOverride.wstring();
            }

            // Priority B: Check user mods folder (e.g. data/mods/...)
            fs::path userMod = s_userStoragePath / "mods" / p;
            if (RawPathExists(userMod.wstring())) {
                return userMod.wstring();
            }

            // Priority C: Check base MCC retail directory (maps, audio, tags)
            fs::path baseAsset = s_baseMccPath / p;
            if (RawPathExists(baseAsset.wstring())) {
                return baseAsset.wstring();
            }

            // Priority D: Check inside maps directory (e.g. fonts/font_package_icon.bin)
            fs::path mapsAsset = s_baseMccPath / "maps" / p;
            if (RawPathExists(mapsAsset.wstring())) {
                return mapsAsset.wstring();
            }

            // Priority E: Check MCC root relative
            if (s_baseMccPath.has_parent_path()) {
                fs::path mccRootAsset = s_baseMccPath.parent_path() / normalized;
                if (RawPathExists(mccRootAsset.wstring())) {
                    return mccRootAsset.wstring();
                }
            }
        }
    } catch (...) {
        return requestedPath;
    }

    return requestedPath;
}

HANDLE WINAPI VFS::HookedCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    if (t_inVfsHook || !s_isInitialized || !lpFileName || !IsGameAssetPath(lpFileName)) {
        return s_origCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    t_inVfsHook = true;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    try {
        std::wstring resolved = ResolvePath(lpFileName, dwDesiredAccess);
        hFile = s_origCreateFileW(
            resolved.c_str(),
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile
        );
    } catch (...) {
        hFile = s_origCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }
    t_inVfsHook = false;

    return hFile;
}

DWORD WINAPI VFS::HookedGetFileAttributesW(LPCWSTR lpFileName) {
    if (t_inVfsHook || !s_isInitialized || !lpFileName || !IsGameAssetPath(lpFileName)) {
        return s_origGetFileAttributesW(lpFileName);
    }

    t_inVfsHook = true;
    DWORD result = INVALID_FILE_ATTRIBUTES;
    try {
        std::wstring resolved = ResolvePath(lpFileName, GENERIC_READ);
        result = s_origGetFileAttributesW(resolved.c_str());
    } catch (...) {
        result = s_origGetFileAttributesW(lpFileName);
    }
    t_inVfsHook = false;

    return result;
}

BOOL WINAPI VFS::HookedGetFileAttributesExW(
    LPCWSTR lpFileName,
    GET_FILEEX_INFO_LEVELS fInfoLevelId,
    LPVOID lpFileInformation
) {
    if (t_inVfsHook || !s_isInitialized || !lpFileName || !IsGameAssetPath(lpFileName)) {
        return s_origGetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
    }

    t_inVfsHook = true;
    BOOL result = FALSE;
    try {
        std::wstring resolved = ResolvePath(lpFileName, GENERIC_READ);
        result = s_origGetFileAttributesExW(resolved.c_str(), fInfoLevelId, lpFileInformation);
    } catch (...) {
        result = s_origGetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
    }
    t_inVfsHook = false;

    return result;
}

static ULONG_PTR* s_pCreateFileWEntry = nullptr;
static ULONG_PTR* s_pGetFileAttributesWEntry = nullptr;
static ULONG_PTR* s_pGetFileAttributesExWEntry = nullptr;
static ULONG_PTR s_origCreateFileWValue = 0;
static ULONG_PTR s_origGetFileAttributesWValue = 0;
static ULONG_PTR s_origGetFileAttributesExWValue = 0;

bool VFS::HookTargetModule(HMODULE hTargetModule) {
    if (!hTargetModule) return false;

    auto pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hTargetModule);
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        NOBLE_ERROR("Invalid DOS signature in target module.");
        return false;
    }

    auto pNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
        reinterpret_cast<uint8_t*>(hTargetModule) + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        NOBLE_ERROR("Invalid NT signature in target module.");
        return false;
    }

    auto importDir = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.VirtualAddress == 0 || importDir.Size == 0) {
        NOBLE_WARN("No import directory found in target module.");
        return false;
    }

    auto pImportDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
        reinterpret_cast<uint8_t*>(hTargetModule) + importDir.VirtualAddress);

    int hookedCount = 0;

    for (; pImportDesc->Name != 0; ++pImportDesc) {
        const char* dllName = reinterpret_cast<const char*>(
            reinterpret_cast<uint8_t*>(hTargetModule) + pImportDesc->Name);

        if (_stricmp(dllName, "KERNEL32.dll") == 0) {
            auto pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
                reinterpret_cast<uint8_t*>(hTargetModule) + pImportDesc->FirstThunk);
            auto pOrigThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
                reinterpret_cast<uint8_t*>(hTargetModule) + pImportDesc->OriginalFirstThunk);

            for (; pThunk->u1.Function != 0; ++pThunk, ++pOrigThunk) {
                if (!(pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                    auto pImportByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
                        reinterpret_cast<uint8_t*>(hTargetModule) + pOrigThunk->u1.AddressOfData);
                    const char* funcName = reinterpret_cast<const char*>(pImportByName->Name);

                    auto PatchIAT = [](ULONG_PTR* pEntry, void* detour, ULONG_PTR& outOrig) -> bool {
                        DWORD oldProtect = 0;
                        if (!VirtualProtect(pEntry, sizeof(ULONG_PTR), PAGE_READWRITE, &oldProtect)) {
                            return false;
                        }
                        outOrig = *pEntry;
                        *pEntry = reinterpret_cast<ULONG_PTR>(detour);
                        VirtualProtect(pEntry, sizeof(ULONG_PTR), oldProtect, &oldProtect);
                        return true;
                    };

                    if (strcmp(funcName, "CreateFileW") == 0) {
                        s_pCreateFileWEntry = &pThunk->u1.Function;
                        if (PatchIAT(s_pCreateFileWEntry, reinterpret_cast<void*>(&HookedCreateFileW), s_origCreateFileWValue)) {
                            s_origCreateFileW = reinterpret_cast<CreateFileW_Fn>(s_origCreateFileWValue);
                            NOBLE_INFO("VFS IAT: Hooked CreateFileW in target module (Original: 0x{:X})", s_origCreateFileWValue);
                            hookedCount++;
                        }
                    } else if (strcmp(funcName, "GetFileAttributesW") == 0) {
                        s_pGetFileAttributesWEntry = &pThunk->u1.Function;
                        if (PatchIAT(s_pGetFileAttributesWEntry, reinterpret_cast<void*>(&HookedGetFileAttributesW), s_origGetFileAttributesWValue)) {
                            s_origGetFileAttributesW = reinterpret_cast<GetFileAttributesW_Fn>(s_origGetFileAttributesWValue);
                            NOBLE_INFO("VFS IAT: Hooked GetFileAttributesW in target module (Original: 0x{:X})", s_origGetFileAttributesWValue);
                            hookedCount++;
                        }
                    } else if (strcmp(funcName, "GetFileAttributesExW") == 0) {
                        s_pGetFileAttributesExWEntry = &pThunk->u1.Function;
                        if (PatchIAT(s_pGetFileAttributesExWEntry, reinterpret_cast<void*>(&HookedGetFileAttributesExW), s_origGetFileAttributesExWValue)) {
                            s_origGetFileAttributesExW = reinterpret_cast<GetFileAttributesExW_Fn>(s_origGetFileAttributesExWValue);
                            NOBLE_INFO("VFS IAT: Hooked GetFileAttributesExW in target module (Original: 0x{:X})", s_origGetFileAttributesExWValue);
                            hookedCount++;
                        }
                    }
                }
            }
        }
    }

    NOBLE_INFO("VFS: Target module IAT redirection active. Intercepted {} file APIs.", hookedCount);
    return (hookedCount > 0);
}

void VFS::UnhookTargetModule() {
    auto RestoreIAT = [](ULONG_PTR* pEntry, ULONG_PTR orig) {
        if (pEntry && orig) {
            DWORD oldProtect = 0;
            if (VirtualProtect(pEntry, sizeof(ULONG_PTR), PAGE_READWRITE, &oldProtect)) {
                *pEntry = orig;
                VirtualProtect(pEntry, sizeof(ULONG_PTR), oldProtect, &oldProtect);
            }
        }
    };

    RestoreIAT(s_pCreateFileWEntry, s_origCreateFileWValue);
    RestoreIAT(s_pGetFileAttributesWEntry, s_origGetFileAttributesWValue);
    RestoreIAT(s_pGetFileAttributesExWEntry, s_origGetFileAttributesExWValue);

    s_pCreateFileWEntry = nullptr;
    s_pGetFileAttributesWEntry = nullptr;
    s_pGetFileAttributesExWEntry = nullptr;
    NOBLE_INFO("VFS: Target module IAT restored to original imports.");
}

} // namespace noble
