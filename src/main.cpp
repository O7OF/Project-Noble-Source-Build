#include "common/Logger.hpp"
#include "engine/MapGameTypes.hpp"
#include "common/MapNames.hpp"
#include "engine/PadLayouts.hpp"
#include "engine/InputTuning.hpp"
#include <io.h>
#include <fcntl.h>
#include "common/Types.hpp"
#include "common/Version.hpp"
#include "host/SteamLocator.hpp"
#include "host/SteamWorkshop.hpp"
#include "host/OwnershipVerifier.hpp"
#include "host/Window.hpp"
#include "host/D3D11Renderer.hpp"
#include "host/Loader.hpp"
#include "hooks/HookManager.hpp"
#include "hooks/SigScanner.hpp"
#include "vfs/VFS.hpp"
#include "engine/BlamEngine.hpp"
#include "engine/CameraFov.hpp"
#include "engine/CameraThirdPerson.hpp"
#include "engine/ReachArmory.hpp"
#include "engine/ElitePack.hpp"
#include "engine/VariantFields.hpp"
#include "ui/UIManager.hpp"
#include "ui/ForgeMenu.hpp"
#include "ui/FrontEndMenu.hpp"
#include "ui/LoadScreen.hpp"
#include "network/SessionManager.hpp"
#include "network/MasterServerClient.hpp"
#include "network/AuthClient.hpp"
#include "network/SocialClient.hpp"
#include "network/NetworkTransport.hpp"
#include "network/UPnP.hpp"
#include "common/Settings.hpp"
#include "common/Updater.hpp"
#include "common/Relaunch.hpp"
#include "ui/PauseMenu.hpp"
#include "ui/UiArt.hpp"
#include "audio/ReachAudio.hpp"
#include "audio/ProcessAudio.hpp"
#include "audio/VoiceChat.hpp"
#include "ui/SocialOverlay.hpp"
#include "hooks/LobbyPatches.hpp"
#include "hooks/ForgePatches.hpp"
#include "hooks/EnginePatches.hpp"

#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <thread>

namespace fs = std::filesystem;

#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

static LONG WINAPI NobleCrashHandler(EXCEPTION_POINTERS* pExceptionInfo) {
    if (pExceptionInfo && pExceptionInfo->ExceptionRecord) {
        uintptr_t faultAddr = reinterpret_cast<uintptr_t>(pExceptionInfo->ExceptionRecord->ExceptionAddress);
        HMODULE hMod = nullptr;
        WCHAR modPath[MAX_PATH] = L"Unknown Module";
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(faultAddr), &hMod)) {
            GetModuleFileNameW(hMod, modPath, MAX_PATH);
        }

        NOBLE_ERROR("FATAL EXCEPTION CAUGHT: Code 0x{:08X} at Address 0x{:X} (Module: {})",
            pExceptionInfo->ExceptionRecord->ExceptionCode,
            faultAddr,
            std::filesystem::path(modPath).string());

        // Capture call stack
        void* stackFrames[32] = { nullptr };
        USHORT frameCount = CaptureStackBackTrace(0, 32, stackFrames, nullptr);
        for (USHORT i = 0; i < frameCount; ++i) {
            uintptr_t frameAddr = reinterpret_cast<uintptr_t>(stackFrames[i]);
            HMODULE frameMod = nullptr;
            WCHAR frameModPath[MAX_PATH] = L"Unknown";
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   reinterpret_cast<LPCWSTR>(frameAddr), &frameMod)) {
                GetModuleFileNameW(frameMod, frameModPath, MAX_PATH);
            }
            NOBLE_ERROR(" Frame {:02d}: 0x{:X} [{}]", i, frameAddr, std::filesystem::path(frameModPath).filename().string());
        }

        // Minidumps contain the full process image and are a decompilation aid: development builds
        // keep them, release builds only leave the redacted crash text in the log.
#ifdef NOBLE_DEV_BUILD
        HANDLE hDumpFile = CreateFileW(L"noble-crash.dmp", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hDumpFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei{};
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = pExceptionInfo;
            mei.ClientPointers = FALSE;
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &mei, nullptr, nullptr);
            CloseHandle(hDumpFile);
            spdlog::error("Crash minidump written to noble-crash.dmp");
        }
#endif
    }
    spdlog::default_logger()->flush();
    return EXCEPTION_CONTINUE_SEARCH;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // Test-only: prove the relaunch handoff leaves no old process behind.
    {
        const std::wstring cmd = pCmdLine ? pCmdLine : L"";
        if (cmd.find(L"-relaunch-test") != std::wstring::npos &&
            cmd.find(L"-relaunch-test-child") == std::wstring::npos) {
            noble::Relaunch::HandOff(L"-relaunch-test-child -quit-after 15");
            return 0;
        }
    }

    // Headless test of the map / game-type guard: no MCC, no window, no audio. `-test-map-gametypes`.
    if (std::wstring(pCmdLine ? pCmdLine : L"").find(L"-test-map-gametypes") != std::wstring::npos) {
        const int types = noble::MapGameTypes::SelfTest();
        const int names = noble::MapNames::SelfTest();
        const int layouts = noble::PadLayouts::SelfTest();
        const int tuning = noble::InputTuning::SelfTest();
        const bool variantFields = noble::VariantFields::SelfTest();
        const bool hostVariant = noble::NetworkTransport::SelfTest();
        std::printf("%s: variant fields - read/write round trip\n", variantFields ? "PASS" : "FAIL");
        std::printf("%s: host variant materialize round trip\n", hostVariant ? "PASS" : "FAIL");
        return (types == 0 && names == 0 && layouts == 0 && tuning == 0 && variantFields && hostVariant) ? 0 : 1;
    }

    // Steam Workshop subscription helper: the join prompt starts us as "-steam-subscribe <id> -steam-dll <path>",
    // we load MCC's steam_api64.dll, ask Steam for the item and exit. This runs before any window, engine or
    // logger init so it is fast and cannot be blocked by the menu.
    {
        int subArgc = 0;
        LPWSTR* subArgv = CommandLineToArgvW(GetCommandLineW(), &subArgc);
        std::string workshopId;
        std::wstring steamApiDll;
        for (int i = 1; subArgv && i < subArgc; ++i) {
            if (_wcsicmp(subArgv[i], L"-steam-subscribe") == 0 && i + 1 < subArgc) {
                for (const wchar_t* c = subArgv[++i]; c && *c; ++c) workshopId.push_back(static_cast<char>(*c));
            } else if (_wcsicmp(subArgv[i], L"-steam-dll") == 0 && i + 1 < subArgc) {
                steamApiDll = subArgv[++i];
            }
        }
        if (subArgv) LocalFree(subArgv);
        if (!workshopId.empty()) {
            return noble::SteamWorkshop::SubscribeChildMain(workshopId, fs::path(steamApiDll));
        }
    }

    SetUnhandledExceptionFilter(&NobleCrashHandler);
    DisableProcessWindowsGhosting();

    // 1. Initialize logging
    // "-log <file>" lets a second instance (or an automated test) keep its own log instead of truncating
    // noble-client.log, which basic_file_sink does on every start.
    {
        std::string logName = "noble-client.log";
        int argcPre = 0;
        LPWSTR* argvPre = CommandLineToArgvW(GetCommandLineW(), &argcPre);
        for (int i = 1; argvPre && i + 1 < argcPre; ++i) {
            if (_wcsicmp(argvPre[i], L"-log") == 0) {
                const std::wstring w(argvPre[i + 1]);
                std::string ascii;
                for (const wchar_t c : w) ascii.push_back(static_cast<char>(c & 0x7F));   // log names are ASCII
                logName = ascii;
            }
        }
        if (argvPre) LocalFree(argvPre);
        noble::Logger::Init(logName);
    }
    NOBLE_INFO("Starting {} - {}", noble::APP_NAME, noble::VERSION_STRING);

    // Headless voice tests need no MCC, window or engine.
    {
        int voiceTestExit = 0;
        if (noble::RunVoiceTestIfRequested(voiceTestExit)) {
            noble::Logger::Shutdown();
            return voiceTestExit;
        }
    }

    // 2. Discover Steam MCC installation and verify Reach DLC assets
    auto reachInstall = noble::SteamLocator::LocateAndVerifyReach();
    if (!reachInstall) {
        NOBLE_ERROR("Halo: Reach installation could not be located. Aborting launch.");
        MessageBoxW(nullptr, 
            L"Could not locate Halo: Reach DLC in any Steam library.\n\n"
            L"Please verify that Halo: The Master Chief Collection with the Halo: Reach DLC is installed on Steam.",
            L"Halo: Reach Community Edition - Launch Error", 
            MB_ICONERROR | MB_OK);
        return 1;
    }

    // 3. Strict Steam Ownership Verification
    // Unowned copies are strictly prohibited from launching
    if (!noble::OwnershipVerifier::VerifySteamOwnership(*reachInstall)) {
        NOBLE_ERROR("Steam ownership verification failed. Aborting launch.");
        return 1;
    }

    // 4. Initialize MinHook hooking framework early (required by VFS and engine hooks)
    if (!noble::HookManager::Initialize()) {
        NOBLE_ERROR("Failed to initialize MinHook.");
        return 1;
    }

    // 5. Locate authentic data directory (resolves blocks, menumaps, armor, and userdata)
    auto FindDataDirectory = []() -> fs::path {
        wchar_t exePathBuf[MAX_PATH] = { 0 };
        if (GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH)) {
            fs::path exeDir = fs::path(exePathBuf).parent_path();
            std::vector<fs::path> candidates = {
                exeDir / "data",
                exeDir / ".." / "data",
                exeDir / ".." / ".." / "data",
                exeDir / ".." / ".." / ".." / "data",
                fs::current_path() / "data"
            };
            for (const auto& cand : candidates) {
                std::error_code ec;
                // Release packages carry no loose menu map: it and its string_ids are embedded in
                // Noble.exe and VFS extracts them on first run. The small JSON/UI files identify the
                // data folder in those installs.
                if (fs::exists(cand / "menumaps" / "mainmenu_all.map", ec) ||
                    fs::exists(cand / "launch_block_1.bin", ec) ||
                    fs::exists(cand / "forge_palette.json", ec) ||
                    fs::exists(cand / "strings_en.json", ec)) {
                    return fs::absolute(cand);
                }
            }
        }
        return fs::absolute("data");
    };

    // Test hook: a second local instance can point at its own userdata so both sides have distinct profiles.
    std::string dataDirOverride;
    {
        int preArgc = 0;
        if (LPWSTR* preArgv = CommandLineToArgvW(GetCommandLineW(), &preArgc)) {
            for (int i = 1; i + 1 < preArgc; ++i) {
                if (std::wstring(preArgv[i]) == L"-data-dir") {
                    for (const wchar_t* c = preArgv[i + 1]; c && *c; ++c) {
                        dataDirOverride.push_back(static_cast<char>(*c));
                    }
                    break;
                }
            }
            LocalFree(preArgv);
        }
    }
    fs::path userStoragePath = FindDataDirectory();
    if (!dataDirOverride.empty()) {
        userStoragePath = fs::path(dataDirOverride);
        NOBLE_INFO("main: user data directory overridden to '{}'", userStoragePath.string());
    }
    NOBLE_INFO("Resolved authentic Project Noble data directory: {}", userStoragePath.string());

    // Initialize Virtual File System (VFS)
    // Directs all file I/O: Retail MCC files remain read-only; custom mods and configs route to data/
    if (!noble::VFS::Initialize(*reachInstall, userStoragePath)) {
        NOBLE_ERROR("Failed to initialize Virtual File System (VFS).");
        return 1;
    }

    // Configure Host object paths early
    noble::ReachHost::Get().SetPaths(reachInstall->reachDirectory, reachInstall->mapsDirectory, userStoragePath);
    noble::ReachArmory::Initialize(userStoragePath);

    // Player settings (data\userdata\settings.json): window size, controls, volumes, name, servers.
    noble::Settings::Get().Load();
    // Headless: move settings to or from a file.
    {
        int transferArgc = 0;
        LPWSTR* transferArgv = CommandLineToArgvW(GetCommandLineW(), &transferArgc);
        for (int i = 1; transferArgv && i + 1 < transferArgc; ++i) {
            const bool isExport = _wcsicmp(transferArgv[i], L"-settings-export") == 0;
            if (!isExport && _wcsicmp(transferArgv[i], L"-settings-import") != 0) continue;
            const bool ok = isExport ? noble::Settings::Get().ExportTo(transferArgv[i + 1]) : noble::Settings::Get().ImportFrom(transferArgv[i + 1]);
            NOBLE_INFO("Settings: {} {}", isExport ? "export" : "import", ok ? "done" : "failed");
            noble::Logger::Shutdown();
            return ok ? 0 : 1;
        }
        if (transferArgv) LocalFree(transferArgv);
    }
    noble::AuthClient::Get().Initialize();
    noble::SocialClient::Get().Initialize();
    const noble::SettingsData startupSettings = noble::Settings::Get().Snapshot();

    // Front-end menu or a direct map launch? Needed before the swap chain is created: the menu process
    // renders at 1.5x its window (menu-only supersampling) so the Armory can zoom the engine frame
    // without pixelating it. The match process is launched separately and stays at native resolution.
    const bool showMenu = noble::FrontEndMenu::ShouldShow();

    // Auto-updater: the menu checks the public release repository once per launch. Match processes do
    // not check (they are short-lived and are relaunched from the menu). "-no-update" is for tests.
    {
        wchar_t updaterExe[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, updaterExe, MAX_PATH) > 0) {
            noble::Updater::Get().Initialize(std::filesystem::path(updaterExe).parent_path());
        }
        if (showMenu && wcsstr(GetCommandLineW(), L"-no-update") == nullptr) {
            noble::Updater::Get().StartCheck();
        }
    }

    // 6. Create Win32 host window
    auto window = std::make_unique<noble::Window>(L"Halo: Reach Community Edition",
        static_cast<uint32_t>(startupSettings.width), static_cast<uint32_t>(startupSettings.height));
    window->SetBorderless(startupSettings.borderless);
    window->SetExclusiveFullscreen(startupSettings.exclusiveFullscreen);
    if (!window->Create()) {
        NOBLE_ERROR("Failed to create Win32 host window.");
        return 1;
    }

    // 7. Initialize Direct3D 11 runtime environment.
    // The engine always renders at the display's native resolution (or the window's, if that is
    // larger), never at a small window size. Windows scales that image down into a window or shows it
    // 1:1 in borderless full screen, so text, HUD and gameplay stay crisp in both modes and the window
    // mode can be switched live without ever resizing the engine's swap chain. The menu process also
    // supersamples 1.5x for the enlarged Armory view. Cap at 3840x2160.
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    const HMONITOR monitor = MonitorFromWindow(window->GetHwnd(), MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfoW(monitor, &monitorInfo);
    const uint32_t monitorWidth = static_cast<uint32_t>(std::max<LONG>(0, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left));
    const uint32_t monitorHeight = static_cast<uint32_t>(std::max<LONG>(0, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top));
    constexpr uint32_t kMaxRenderWidth = 3840;
    constexpr uint32_t kMaxRenderHeight = 2160;
    uint32_t renderWidth = std::clamp(std::max(window->GetWidth(), monitorWidth), 320u, kMaxRenderWidth);
    uint32_t renderHeight = std::clamp(std::max(window->GetHeight(), monitorHeight), 240u, kMaxRenderHeight);
    if (showMenu) {
        NOBLE_INFO("main: menu render resolution {}x{} (window {}x{}, display {}x{})", renderWidth, renderHeight,
                   window->GetWidth(), window->GetHeight(), monitorWidth, monitorHeight);
    } else {
        NOBLE_INFO("main: match render resolution {}x{} (window {}x{}, display {}x{})", renderWidth, renderHeight,
                   window->GetWidth(), window->GetHeight(), monitorWidth, monitorHeight);
    }
    auto renderer = std::make_unique<noble::D3D11Renderer>();
    if (!renderer->Initialize(window->GetHwnd(), renderWidth, renderHeight)) {
        NOBLE_ERROR("Failed to initialize Direct3D 11 renderer.");
        return 1;
    }

    // Initialize LoadScreen wireframe particle renderer
    noble::LoadScreen::Get().Initialize(renderer->GetDevice(), renderer->GetContext());

    // Initialize ImGui UI Manager
    noble::UIManager::Initialize(window->GetHwnd(), renderer->GetDevice(), renderer->GetContext());
    noble::ForgeMenu::Initialize(userStoragePath.wstring());
    window->SetCustomWndProc([](HWND h, UINT m, WPARAM w, LPARAM l) {
        return noble::UIManager::HandleWndProc(h, m, w, l);
    });
    {
        // Menu and loading-screen pictures: the player's own art, Noble's art, or MCC's UI packs.
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        noble::UiArt::Init(renderer->GetDevice(), reachInstall->reachDirectory.parent_path(),
                           std::filesystem::path(exePath).parent_path());
        // Reach's menu sounds and menu music, decoded from MCC's sound bank into data\userdata\cache\audio.
        noble::ReachAudio::Init(reachInstall->reachDirectory,
                                std::filesystem::path(exePath).parent_path() / "data" / "userdata" / "cache" / "audio");
        {
            const auto& s = noble::Settings::Get().Snapshot();
            noble::ProcessAudio::ApplyAll(s.volumeGame, s.volumeMusic, s.volumeOther, s.masterMute);
        }
    }

    window->Show(nCmdShow);
    noble::ReachHost::Get().SetDisplaySize(renderWidth, renderHeight);

    // LAN discovery and hosting must already run while the front-end menu is open, so a game
    // hosted from the SERVERS tab (or by someone else on the LAN) shows up in the list.
    noble::SessionManager::Get().Initialize();
    const std::string masterUrl = startupSettings.masterServerUrl;  // empty = LAN/direct only
    noble::MasterServerClient::Get().SetUrl(masterUrl);
    noble::MasterServerClient::Get().RequestOnlineCount();

    // Parse command line arguments for direct map and variant launches (e.g. from NobleLauncher or Relaunch)
    std::string launchMap = "";
    std::string launchMode = "";
    std::string customMvar = "";
    std::string customVariant = "";
    fs::path modFolder;
    fs::path mapOverride;   // modded .map served in place of the stock launchMap file
    std::string mapTitle;   // mod map's own title and pictures (loading screen / poster)
    fs::path mapImage;
    fs::path mapThumb;
    int launchDifficulty = 1;
    uint64_t launchSkulls = 0;
    bool launchHost = false;
    bool hostOnline = true;
    bool hostPublic = true;
    int hostMaxPlayers = 16;
    int expectedPeers = 0;
    int engineModeOverride = 0;
    int liveWindowTestAt = -1;   // test-only: switch to borderless after N seconds
    bool guestPostStart = false;
    bool thirdPerson = false;
    bool voiceProbe = false;
    std::string variantDumpPath;
    std::string variantPatch;
    int thirdPersonMode = 0;
    int thirdPersonDistance = 100;
    std::string engineCommand;
    int engineCommandAt = -1;
    std::string connectAddress = "";
    std::string connectSessionId = "";
    std::string connectSessionToken = "";
    std::string relayUrl = "";

    auto WideToNarrow = [](const std::wstring& w) -> std::string {
        if (w.empty()) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return "";
        std::string s(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), len, nullptr, nullptr);
        return s;
    };

    int numArgs = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &numArgs);
    if (argvW) {
        for (int i = 1; i < numArgs; ++i) {
            std::wstring arg = argvW[i];
            if (arg == L"-map" && i + 1 < numArgs) {
                launchMap = WideToNarrow(argvW[++i]);
            } else if (arg == L"-mode" && i + 1 < numArgs) {
                launchMode = WideToNarrow(argvW[++i]);
            } else if (arg == L"-mvar" && i + 1 < numArgs) {
                customMvar = WideToNarrow(argvW[++i]);
            } else if (arg == L"-variant" && i + 1 < numArgs) {
                customVariant = WideToNarrow(argvW[++i]);
            } else if (arg == L"-mod" && i + 1 < numArgs) {
                modFolder = fs::path(argvW[++i]);
            } else if (arg == L"-map-override" && i + 1 < numArgs) {
                mapOverride = fs::path(argvW[++i]);
            } else if (arg == L"-map-title" && i + 1 < numArgs) {
                mapTitle = WideToNarrow(argvW[++i]);
            } else if (arg == L"-map-image" && i + 1 < numArgs) {
                mapImage = fs::path(argvW[++i]);
            } else if (arg == L"-map-thumb" && i + 1 < numArgs) {
                mapThumb = fs::path(argvW[++i]);
            } else if (arg == L"-skulls" && i + 1 < numArgs) {
                launchSkulls = _wcstoui64(argvW[++i], nullptr, 0);
            } else if (arg == L"-difficulty" && i + 1 < numArgs) {
                launchDifficulty = _wtoi(argvW[++i]);
            } else if (arg == L"-host") {
                launchHost = true;
            } else if (arg == L"-online") {
                hostOnline = true;
            } else if (arg == L"-lan") {
                hostOnline = false;
            } else if (arg == L"-public") {
                hostPublic = true;
            } else if (arg == L"-private") {
                hostPublic = false;
            } else if (arg == L"-max-players" && i + 1 < numArgs) {
                hostMaxPlayers = _wtoi(argvW[++i]);
            } else if (arg == L"-wait-peers" && i + 1 < numArgs) {
                expectedPeers = _wtoi(argvW[++i]);
            } else if (arg == L"-third-person") {
                thirdPerson = true;
            } else if (arg == L"-variant-dump" && i + 1 < numArgs) {
                variantDumpPath = WideToNarrow(argvW[++i]);
            } else if (arg == L"-variant-override" && i + 1 < numArgs) {
                variantPatch = WideToNarrow(argvW[++i]);
            } else if (arg == L"-voice-probe") {
                voiceProbe = true;
            } else if (arg == L"-live-window-test" && i + 1 < numArgs) {
                liveWindowTestAt = _wtoi(argvW[++i]);
            } else if (arg == L"-third-person-distance" && i + 1 < numArgs) {
                thirdPersonDistance = _wtoi(argvW[++i]);
                thirdPerson = true;
            } else if (arg == L"-third-person-mode" && i + 1 < numArgs) {
                thirdPersonMode = _wtoi(argvW[++i]);
                thirdPerson = true;
            } else if (arg == L"-guest-post-start") {
                guestPostStart = true;
            } else if (arg == L"-engine-cmd" && i + 1 < numArgs) {
                engineCommand = WideToNarrow(argvW[++i]);
            } else if (arg == L"-engine-cmd-at" && i + 1 < numArgs) {
                engineCommandAt = _wtoi(argvW[++i]);
            } else if (arg == L"-connect" && i + 1 < numArgs) {
                connectAddress = WideToNarrow(argvW[++i]);
            } else if (arg == L"-session-id" && i + 1 < numArgs) {
                connectSessionId = WideToNarrow(argvW[++i]); 
            } else if (arg == L"-session-token" && i + 1 < numArgs) {
                connectSessionToken = WideToNarrow(argvW[++i]);
            } else if (arg == L"-relay" && i + 1 < numArgs) {
                relayUrl = WideToNarrow(argvW[++i]);
            }
        }
        LocalFree(argvW);
    }
    // Launch the Front-End Menu + 3D Armory (mainmenu_all.map inside haloreach.dll), or the chosen map.
    if (showMenu) {
        noble::ReachArmory::SetMenuMode(true);
        noble::FrontEndMenu::Initialize(reachInstall->reachDirectory);
        launchMap = "mainmenu";
        launchMode = "mainmenu";
    } else {
        noble::ReachArmory::SetMenuMode(false);
        if (launchMap.empty()) {
            launchMap = "20_sword_slayer";
            launchMode = "multiplayer";
        }
    }


    if (!modFolder.empty()) {
        NOBLE_INFO("Activating mod folder '{}'", modFolder.string());
        noble::VFS::SetActiveMod(modFolder);
    }
    if (!mapOverride.empty()) {
        NOBLE_INFO("Map override: serving '{}' as maps\\{}.map", mapOverride.string(), launchMap);
        noble::VFS::SetMapOverride(launchMap, mapOverride);
    } else {
        noble::VFS::ClearMapOverride();
    }

    // Map-info sandbox. The engine builds its map list by enumerating "haloreach\\maps\\info\\*.*"
    // RELATIVE TO THE WORKING DIRECTORY (root prefix "haloreach\\" at RVA 0x985D28, FindFirstFileW).
    // With an empty list every launch fails with "no map selected" and the menu has no backdrop.
    {
        std::error_code ec;
        const fs::path sandboxRoot = userStoragePath / "sandbox";
        const fs::path sandboxInfo = sandboxRoot / "haloreach" / "maps" / "info";
        const fs::path mccInfo = reachInstall->mapsDirectory / "info";
        fs::create_directories(sandboxInfo, ec);
        size_t copied = 0;
        for (const auto& entry : fs::directory_iterator(mccInfo, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            fs::copy_file(entry.path(), sandboxInfo / entry.path().filename(), fs::copy_options::overwrite_existing, ec);
            if (!ec) ++copied;
        }
        if (copied > 0 && SetCurrentDirectoryW(sandboxRoot.c_str())) {
            NOBLE_INFO("Map-info sandbox: copied {} file(s) from '{}'; working directory is now '{}'",
                copied, mccInfo.string(), sandboxRoot.string());
        } else {
            NOBLE_ERROR("Map-info sandbox setup failed (copied {} from '{}'). The engine will report 'no map selected'.",
                copied, mccInfo.string());
        }
    }

    // Test/debug: apply a comma-separated variant patch after the selected variant is decoded.
    if (!variantPatch.empty()) {
        noble::BlamEngine::SetVariantPatch(variantPatch);
        NOBLE_INFO("main: -variant-override '{}'", variantPatch);
    }

    // Remember this launch for the post-game "PLAY AGAIN" button and the loading screen.
    if (!showMenu) {
    auto toWide = [](const std::string& s) -> std::wstring {
        if (s.empty()) return L"";
        const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len <= 0) return L"";
        std::wstring w(len - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), len);
        return w;
    };
    std::wstring args = L"-map \"" + toWide(launchMap) + L"\" -mode \"" + toWide(launchMode) + L"\"";
    args += L" -difficulty " + std::to_wstring(launchDifficulty);
    if (!customVariant.empty()) args += L" -variant \"" + toWide(customVariant) + L"\"";
    if (!customMvar.empty()) args += L" -mvar \"" + toWide(customMvar) + L"\"";
    if (launchSkulls != 0) args += L" -skulls " + std::to_wstring(launchSkulls);
    if (!modFolder.empty()) args += L" -mod \"" + modFolder.wstring() + L"\"";
    if (!mapOverride.empty()) args += L" -map-override \"" + mapOverride.wstring() + L"\"";
    if (!mapTitle.empty()) args += L" -map-title \"" + toWide(mapTitle) + L"\"";
    if (!mapImage.empty()) args += L" -map-image \"" + mapImage.wstring() + L"\"";
    if (!mapThumb.empty()) args += L" -map-thumb \"" + mapThumb.wstring() + L"\"";

    // In-match scoreboard overlay data: fetch the session roster once so the panel can show names, ranks,
    // emblems and teams while the scoreboard key is held.
    if (!connectSessionId.empty()) {
        std::thread([connectSessionId]() {
            noble::LobbyState st = noble::MasterServerClient::Get().FetchLobbyStateSync(connectSessionId);
            if (!st.ok || st.roster.empty()) return;
            noble::PauseMenu::SetScoreboardSession(connectSessionId);
            std::vector<noble::PauseMenu::ScoreboardEntry> rows;
            const std::string me = noble::Settings::Get().Snapshot().playerName;
            for (const auto& p : st.roster) {
                noble::PauseMenu::ScoreboardEntry e;
                e.name = p.username;
                e.tag = p.serviceTag;
                e.rank = p.rankIndex;
                e.emblem = p.emblemId;
                e.team = p.team;
                e.hostPeer = p.isHost;
                try { if (!p.machineId.empty()) e.machine = std::stoull(p.machineId, nullptr, 0); } catch (...) {}
                e.local = !me.empty() && _stricmp(p.username.c_str(), me.c_str()) == 0;
                if (e.local) noble::VoiceChat::SetLocalTeam(p.team);
                if (e.machine != 0) noble::SocialOverlay::SetPlayerName(e.machine, p.username);
                rows.push_back(e);
            }
            noble::PauseMenu::SetScoreboardRoster(std::move(rows), st.map, st.mode);
        }).detach();
    }

    if (!connectAddress.empty()) {
        args += L" -connect \"" + toWide(connectAddress) + L"\"";
        if (!connectSessionId.empty()) {
            args += L" -session-id \"" + toWide(connectSessionId) + L"\"";
            if (!connectSessionToken.empty()) {
                args += L" -session-token \"" + toWide(connectSessionToken) + L"\"";
            }
        }
    } else if (launchHost) {
        args += L" -host";
        if (hostOnline) args += L" -online";
        else args += L" -lan";
        if (hostPublic) args += L" -public";
        else args += L" -private";
        args += L" -max-players " + std::to_wstring(hostMaxPlayers);
        if (expectedPeers > 0) args += L" -wait-peers " + std::to_wstring(expectedPeers);
        if (!connectSessionId.empty()) {
            args += L" -session-id \"" + toWide(connectSessionId) + L"\"";
            if (!connectSessionToken.empty()) {
                args += L" -session-token \"" + toWide(connectSessionToken) + L"\"";
            }
        }
    }
    noble::Relaunch::SetCurrentGameArgs(args);
    noble::PauseMenu::SetLoadingMap(launchMap);
    if (!mapTitle.empty() || !mapImage.empty() || !mapThumb.empty()) {
        NOBLE_INFO("Loading screen: mod map '{}' (image '{}')", mapTitle, mapImage.string());
        noble::PauseMenu::SetLoadingMapInfo(mapTitle, mapImage, mapThumb);
    }
    if (!mapImage.empty()) noble::UiArt::ImageFile(mapImage);
    else noble::UiArt::LoadingBackground(launchMap);
    if (!mapThumb.empty()) noble::UiArt::ImageFile(mapThumb);
    else noble::UiArt::MapPreview(launchMap, true);
    }

    // 8. Load and bootstrap haloreach.dll directly in process memory
    auto loader = std::make_unique<noble::Loader>();
    if (!loader->LoadReachDll(reachInstall->reachDllPath)) {
        NOBLE_ERROR("Failed to load haloreach.dll into process space.");
        return 1;
    }

    // Direct all haloreach.dll asset and map file I/O through Virtual File System (VFS)
    noble::VFS::HookTargetModule(loader->GetModuleHandle());

    // The SETTINGS > VIDEO field-of-view slider writes the engine's own camera FOV global.
    noble::CameraFov::SetEngineModule(loader->GetModuleHandle());
    noble::CameraThirdPerson::Install(loader->GetModuleHandle());
    if (thirdPersonMode > 0) noble::CameraThirdPerson::SetMode(thirdPersonMode);
    if (thirdPersonDistance != 100) noble::CameraThirdPerson::SetDistancePercent(thirdPersonDistance);
    if (voiceProbe) noble::ProbeVoiceDevices();
    noble::VoiceChat::Start();   // idle until a relay session exists
    noble::SocialOverlay::Start();
    if (thirdPerson) noble::CameraThirdPerson::SetEnabled(true);

    // Apply 32/64 player lobby expansion detours
    noble::LobbyPatches::Apply(loader->GetModuleHandle());

    // Apply engine stability and appearance cap patches (unlocks all 200+ armor permutations in haloreach.dll)
    noble::EnginePatches::Apply(loader->GetModuleHandle());

    // 9. Instantiate Blam! Engine & Data Access interfaces
    auto blamEngine = std::make_unique<noble::BlamEngine>();
    noble::EngineHostContext hostContext;
    hostContext.hWnd = window->GetHwnd();
    hostContext.pDevice = renderer->GetDevice();
    hostContext.pContext = renderer->GetContext();
    hostContext.pSwapChain = renderer->GetSwapChain();
    hostContext.width = renderWidth;
    hostContext.height = renderHeight;

    if (!blamEngine->Initialize(*reachInstall, loader->GetModuleHandle(), loader->GetExports(), hostContext)) {
        NOBLE_ERROR("Failed to initialize Blam! engine host interfaces.");
        return 1;
    }

    // Dev tool: decode a game variant with the engine and exit.
    if (!variantDumpPath.empty()) {
        const bool ok = blamEngine->DumpGameVariant(variantDumpPath);
        noble::Logger::Shutdown();
        TerminateProcess(GetCurrentProcess(), ok ? 0 : 1);
    }

    // Elite armour pack (data\armor\<name>.map + .rrreloc, local only): loaded with multiplayer and Firefight maps.
    if (!showMenu) noble::ElitePack::Install(loader->GetModuleHandle(), userStoragePath, launchMode);

    if (!connectAddress.empty()) {
        launchHost = false; // Guest joining remote match
        NOBLE_INFO("Launching Halo: Reach Engine as Guest connecting to '{}' (Map: {}, Mode: {}, Session: '{}')",
                   connectAddress, launchMap, launchMode, connectSessionId);

        std::string tip = connectAddress;
        uint16_t tport = 11774;
        const size_t c = connectAddress.rfind(':');
        if (c != std::string::npos) {
            tip = connectAddress.substr(0, c);
            const int p = std::atoi(connectAddress.substr(c + 1).c_str());
            if (p > 0 && p <= 65535) tport = static_cast<uint16_t>(p);
        }

        // Initialize Guest NetworkTransport and register the join with the Master Server / WebSocket Relay
        // BEFORE starting haloreach.dll's engine thread so the Host's NAT hole-punching and the WebSocket
        // relay are already active when the engine sends its first join-request packet.
        noble::NetworkTransport::Get().Initialize(false, tip, tport);
        const uint64_t guestMachineId = noble::NetworkTransport::Get().GetLocalMachineId();
        const uint16_t guestPort = noble::NetworkTransport::Get().GetLocalPort();
        if (!relayUrl.empty()) {
            noble::NetworkTransport::Get().StartRelay(relayUrl, connectSessionId.empty() ? "HALOREAC" : connectSessionId, false, guestMachineId);
        } else if (noble::MasterServerClient::Get().Enabled()) {
            const noble::MasterJoinInfo joinInfo = noble::MasterServerClient::Get().JoinSessionSync(
                connectSessionId, guestMachineId, guestPort, tip, tport);
            if (joinInfo.ok && !joinInfo.relayUrl.empty()) {
                noble::NetworkTransport::Get().StartRelay(
                    joinInfo.relayUrl,
                    joinInfo.sessionId.empty() ? "HALOREAC" : joinInfo.sessionId,
                    false,
                    guestMachineId);
            }
        }

    if (guestPostStart) noble::BlamEngine::SetGuestPostStart(true);
    if (engineModeOverride > 0) {
        noble::BlamEngine::SetEngineModeOverride(engineModeOverride);
        NOBLE_WARN("main: -engine-mode {} (research hook - forcing launch-block +0x0C)", engineModeOverride);
    }
    blamEngine->LaunchMap(launchMap, launchMode, launchDifficulty, launchSkulls, customMvar, customVariant, connectAddress, expectedPeers);
    } else {
        if (launchHost) {
            // Bind UDP port 11774 and start Master Server advertisement + WebSocket Relay BEFORE
            // blamEngine->LaunchMap loads the map so the Host's UDP socket and relay are ready before
            // any Guest sends its first join-request packet.
            noble::NetworkTransport::Get().Initialize(true);

            noble::HostConfig cfg;
            std::string modeTitle = launchMode;
            if (!modeTitle.empty()) modeTitle[0] = static_cast<char>(toupper(static_cast<unsigned char>(modeTitle[0])));
            cfg.sessionName = startupSettings.playerName + "'s " + modeTitle;
            if (launchMode == "campaign") cfg.sessionName = startupSettings.playerName + "'s Co-op Campaign";
            else if (launchMode == "firefight") cfg.sessionName = startupSettings.playerName + "'s Firefight";
            else if (launchMode == "forge") cfg.sessionName = startupSettings.playerName + "'s Forge";

            cfg.gameMode = launchMode;
            cfg.mapName = launchMap;
            cfg.gametypeName = (launchMode == "firefight" ? "Firefight" : launchMode == "forge" ? "Forge" : "Slayer");
            cfg.maxPlayers = hostMaxPlayers;
            // The port the game socket really got: 11774 unless another program holds it (the socket is exclusive
            // since 2026-10-29 and then falls back to a free port). Advertising 11774 regardless sent guests elsewhere.
            cfg.port = noble::NetworkTransport::Get().IsActive() ? noble::NetworkTransport::Get().GetLocalPort() : 11774;
            cfg.mode = hostOnline ? noble::NetworkMode::Online_P2P : noble::NetworkMode::Local_LAN;

            if (noble::SessionManager::Get().StartHosting(cfg)) {
                NOBLE_INFO("main: Started self-hosting match '{}' on port {} (Mode: {})", cfg.sessionName, cfg.port, hostOnline ? "Online P2P" : "Local LAN");
                if (hostOnline && noble::MasterServerClient::Get().Enabled()) {
                    noble::MasterAdvert advert;
                    advert.name = cfg.sessionName;
                    advert.port = cfg.port;
                    advert.maxPlayers = cfg.maxPlayers;
                    advert.map = cfg.mapName;
                    advert.mode = cfg.gameMode;
                    advert.variant = customVariant.empty() ? cfg.gametypeName : fs::path(customVariant).filename().string();
                    advert.difficulty = launchDifficulty;
                    advert.skulls = launchSkulls;
                    advert.inProgress = true;
                    advert.hostPeerId = noble::AuthClient::Get().IsLoggedIn() ? static_cast<uint64_t>(noble::AuthClient::Get().GetUserId()) : 1ULL;
                    advert.hostMachineId = "0x484F535400000001";
                    advert.hostUsername = noble::AuthClient::Get().IsLoggedIn() ? noble::AuthClient::Get().GetUsername() : (startupSettings.playerName.empty() ? "Host" : startupSettings.playerName);
                    advert.hostIsDev = noble::AuthClient::Get().IsDeveloper();
                    advert.hostServiceTag = startupSettings.serviceTag.empty() ? "117" : startupSettings.serviceTag;
                    advert.hasPassword = false;
                    advert.isPrivate = !hostPublic;
                    advert.isMod = !modFolder.empty();
                    advert.address = noble::UPnP::Get().GetExternalIP();
                    if (!connectSessionId.empty()) {
                        advert.id = connectSessionId;
                        advert.token = connectSessionToken;
                        advert.inProgress = true;
                    }
                    noble::MasterServerClient::Get().StartAdvertising(advert);
                    NOBLE_INFO("main: Advertised hosted match to master server ({}:{}) [Session: '{}']", advert.address, advert.port, connectSessionId.empty() ? "new" : connectSessionId);
                }
            } else {
                NOBLE_WARN("main: Failed to start self-hosting match");
            }
        }

        NOBLE_INFO("Launching Halo: Reach Engine: Target Map='{}', Mode='{}'", launchMap, launchMode);
        blamEngine->LaunchMap(launchMap, launchMode, launchDifficulty, launchSkulls, customMvar, customVariant, "", expectedPeers);
    }

    if (!engineCommand.empty() && engineCommandAt >= 0) {
        // Research hook: dispatch one engine command after N seconds (e.g. -engine-cmd "enter_editor_mode" -engine-cmd-at 16)
        const std::string cmd = engineCommand;
        const int delayMs = engineCommandAt * 1000;
        std::thread([cmd, delayMs]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            NOBLE_WARN("main: dispatching engine command '{}' (research hook)", cmd);
            if (noble::BlamEngine::Active()) noble::BlamEngine::Active()->ExecuteCommand(cmd);
            else NOBLE_WARN("main: no active engine for command '{}'", cmd);
        }).detach();
    }

    NOBLE_INFO("Authentic Halo: Reach host message loop active. Driving frames...");

    // 11. Native Win32 message pump: Drive frame rendering, wireframe dots, UI overlays, and swap chain Present
    const ULONGLONG liveWindowStart = GetTickCount64();
    bool liveWindowSwitched = false;
    while (window->ProcessMessages()) {
        if (!liveWindowSwitched && liveWindowTestAt >= 0 &&
            GetTickCount64() - liveWindowStart >= static_cast<ULONGLONG>(liveWindowTestAt) * 1000ull) {
            liveWindowSwitched = true;
            window->ApplyWindowSettings(startupSettings.width, startupSettings.height, true);
            NOBLE_INFO("main: -live-window-test switched to borderless full screen without restarting");
        }
        blamEngine->Update(0.016f);

        // haloreach.dll's engine thread presents (host slot 1). After a match it stops drawing; then the
        // post-game panel is drawn and presented from here (PauseMenu::HoldFrame, as RR does).
        // The front-end menu likewise once the engine drops its menu map (FrontEndMenu::HoldFrame).
        if (!noble::PauseMenu::HoldFrame() && !noble::FrontEndMenu::HoldFrame()) {
            MsgWaitForMultipleObjectsEx(0, nullptr, 10, QS_ALLINPUT, MWMO_ALERTABLE);
        }
    }

    // Watchdog in case network shutdown stalls on an unresponsive router or server.
    std::thread([] {
        Sleep(5000);
        NOBLE_WARN("Shutdown took over 5 s; exiting anyway.");
        noble::Logger::Shutdown();
        TerminateProcess(GetCurrentProcess(), 0);  // not ExitProcess: its DLL shutdown can hang (see below)
    }).detach();

    // 13. Clean shutdown: stop master server advertisement, UPnP/LAN discovery, and UDP/relay sockets,
    // flush logs, and terminate the process without tearing down live D3D11/ImGui or c_game_engine
    // heap objects under haloreach.dll's running engine/audio threads (fixes the ntdll access violation on exit).
    NOBLE_INFO("Initiating clean host shutdown...");
    noble::VoiceChat::Shutdown();
    noble::SocialClient::Get().Shutdown();
    noble::MasterServerClient::Get().Shutdown();
    noble::SessionManager::Get().Shutdown();
    blamEngine->Shutdown();

    NOBLE_INFO("Network and host services shut down; exiting process cleanly.");
    noble::Logger::Shutdown();
    TerminateProcess(GetCurrentProcess(), 0);  // not ExitProcess: its DLL shutdown can hang
    return 0;   // not reached
}
