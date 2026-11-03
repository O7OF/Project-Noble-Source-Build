#include "ui/SocialOverlay.hpp"
#include "ReachHost.hpp"
#include "EmbeddedBlocks.hpp"
#include "ui/LoadScreen.hpp"
#include "ui/FrontEndMenu.hpp"
#include "ui/UiArt.hpp"
#include "common/Logger.hpp"
#include "common/SaveCodec.hpp"
#include "common/DeviceIdentity.hpp"
#include "hooks/EnginePatches.hpp"
#include "hooks/HookManager.hpp"
#include "ui/PauseMenu.hpp"
#include "ui/ForgeMenu.hpp"
#include "career/Career.hpp"
#include "engine/FrameCapture.hpp"
#include "engine/PadLayouts.hpp"
#include "engine/InputTuning.hpp"
#include "engine/KeyBindings.hpp"
#include "engine/CameraFov.hpp"
#include "engine/CameraThirdPerson.hpp"
#include "engine/ReachArmory.hpp"
#include "engine/ElitePack.hpp"
#include "vfs/VFS.hpp"
#include "common/Settings.hpp"
#include "common/Relaunch.hpp"
#include "host/Window.hpp"
#include "host/D3D11Renderer.hpp"
#include "network/NetworkTransport.hpp"
#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <intrin.h>
#include <atomic>
#include <functional>
#include <array>
#include <utility>
#include <cwctype>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <string>
#include <cstdlib>
#include <cmath>
#include <cwchar>
#include <cstdio>
#include <locale.h>
#include <unordered_set>
#include <deque>
#include <iterator>
#include <cstddef>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace noble {

void* ReachHost::s_vtable[160] = { nullptr };
void* ReachHost::s_vtable2[160] = { nullptr };
bool ReachHost::s_vtableInitialized = false;

// -----------------------------------------------------------------------------
// Low-level SEH helpers (no C++ local objects with destructors allowed here)
// -----------------------------------------------------------------------------
static inline bool SafeReadUint32(uintptr_t addr, uint32_t* outVal) {
    if (addr <= 0x10000 || !outVal) return false;
    __try {
        *outVal = *reinterpret_cast<const uint32_t*>(addr);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static inline bool SafeReadPointer(uintptr_t addr, uintptr_t* outPtr) {
    if (addr <= 0x10000 || !outPtr) return false;
    __try {
        *outPtr = *reinterpret_cast<const uintptr_t*>(addr);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static inline bool SafeWriteUint64(uintptr_t addr, uint64_t val) {
    if (addr <= 0x10000) return false;
    __try {
        *reinterpret_cast<uint64_t*>(addr) = val;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static inline bool SafeWriteUint8(uintptr_t addr, uint8_t val) {
    if (addr <= 0x10000) return false;
    __try {
        *reinterpret_cast<uint8_t*>(addr) = val;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static inline bool SafeWriteUint16(uintptr_t addr, uint16_t val) {
    if (addr <= 0x10000) return false;
    __try {
        *reinterpret_cast<uint16_t*>(addr) = val;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static inline bool SafeReadInt32(uintptr_t addr, int* outVal) {
    if (addr <= 0x10000 || !outVal) return false;
    __try {
        *outVal = *reinterpret_cast<const int*>(addr);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static inline bool SafeZeroMemory(void* dest, size_t size) {
    if (reinterpret_cast<uintptr_t>(dest) <= 0x10000 || size == 0) return false;
    __try {
        memset(dest, 0, size);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static inline bool SafeCopyMemory(void* dest, const void* src, size_t size) {
    if (reinterpret_cast<uintptr_t>(dest) <= 0x10000 || !src || size == 0) return false;
    __try {
        memcpy(dest, src, size);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static inline bool SafeCopyWideString(wchar_t* dest, size_t maxChars, const wchar_t* src) {
    if (reinterpret_cast<uintptr_t>(dest) <= 0x10000 || !src || maxChars == 0) return false;
    __try {
        wcsncpy_s(dest, maxChars, src, _TRUNCATE);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static inline bool SafeCopyAnsiString(char* dest, size_t maxChars, const char* src) {
    if (reinterpret_cast<uintptr_t>(dest) <= 0x10000 || !src || maxChars == 0) return false;
    __try {
        strncpy_s(dest, maxChars, src, _TRUNCATE);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static inline bool SafeExtractString(uintptr_t inPtr, wchar_t* outBufW, size_t maxW, char* outBufA, size_t maxA, bool* outIsWide) {
    if (inPtr <= 0x10000) return false;
    __try {
        const char* rawBytes = reinterpret_cast<const char*>(inPtr);
        if (rawBytes[0] == 0) return false;

        // In Windows UTF-16 LE ASCII, byte 1 is always 0 (e.g. L"m" is 0x6D, 0x00).
        // In UTF-8 / ANSI ASCII, byte 1 is non-zero (e.g. "ma" is 0x6D, 0x61).
        if (rawBytes[1] == 0 && rawBytes[2] != 0 && rawBytes[3] == 0) {
            const wchar_t* wInput = reinterpret_cast<const wchar_t*>(inPtr);
            wcsncpy_s(outBufW, maxW, wInput, _TRUNCATE);
            *outIsWide = true;
            return true;
        } else {
            strncpy_s(outBufA, maxA, rawBytes, _TRUNCATE);
            *outIsWide = false;
            return true;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// -----------------------------------------------------------------------------
// Slot Dispatcher
// -----------------------------------------------------------------------------
template<size_t Slot>
struct SlotDispatcher {
    static uint64_t __fastcall Stub(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
        (void)a4;
        static std::atomic<bool> s_firstCall{false};
        if (!s_firstCall.exchange(true)) {
            NOBLE_INFO("ReachHost: slot {:3d} first call (rcx=0x{:X}, a1=0x{:X}, a2=0x{:X}, a3=0x{:X})",
                Slot, reinterpret_cast<uintptr_t>(thisPtr), a1, a2, a3);
        }
        return 0;
    }
};

// Secondary interface (returned by slot 10, stored at host[1]) stubs: one per index so the log
// shows exactly which method the engine calls. RR's VTABLE2 has 160 entries; a shorter table made
// calls past index 63 jump to address 0.
template<size_t Slot>
struct SlotDispatcher2 {
    static uint64_t __fastcall Stub(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
        (void)a4;
        static std::atomic<bool> s_firstCall{false};
        if (!s_firstCall.exchange(true)) {
            NOBLE_INFO("ReachHost: iface2 slot {:3d} first call (rcx=0x{:X}, a1=0x{:X}, a2=0x{:X}, a3=0x{:X})",
                Slot, reinterpret_cast<uintptr_t>(thisPtr), a1, a2, a3);
        }
        return 0;
    }
};

// iface2 slot 147 (0x93): RR's host::hit2 returns a pointer to a static 16-byte SESSION_GUID
// (all zero) for this index and 0 for every other index. Returning 0 here made the engine read
// the session id from address 0 while loading a map (crash at haloreach+0x7924D).
alignas(16) static uint8_t s_sessionGuid[16] = { 0 };

static uint64_t __fastcall Iface2Slot147_SessionGuid(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a4;
    static std::atomic<bool> s_logged{false};
    if (!s_logged.exchange(true)) {
        NOBLE_INFO("ReachHost: iface2 slot 147 (SessionGuid) first call (a1=0x{:X}, a2=0x{:X}, a3=0x{:X})", a1, a2, a3);
    }
    NetworkTransport::Get().GetSessionGuid(s_sessionGuid);
    return reinterpret_cast<uint64_t>(s_sessionGuid);
}

template<size_t... Is>
static void InitVTable2Helper(void** vtable, std::index_sequence<Is...>) {
    ((vtable[Is] = reinterpret_cast<void*>(&SlotDispatcher2<Is>::Stub)), ...);
}

// Slot 0: QueryInterface handler
static uint64_t __fastcall HostSlot0_QueryInterface(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)a3; (void)a4;
    static std::atomic<bool> s_logged{false};
    if (!s_logged.exchange(true)) {
        NOBLE_INFO("ReachHost [Slot 0 - QueryInterface]: riid=0x{:X}, outPtr=0x{:X}", a1, a2);
    }
    if (a2 > 0x10000) {
        SafeCopyMemory(reinterpret_cast<void*>(a2), &thisPtr, sizeof(void*));
    }
    return 0; // S_OK
}

static void ExecuteGammaPass(IDXGISwapChain* swapChain);
static void ResetGammaPass();
static std::atomic<bool> s_sessionStarted{false};
static std::atomic<bool> s_testCaptureRequest{false};  // -scoreboard-at: screenshot on the next frame  // set on the first slot-10 call (loading screen ends)

// Test option "-end-round-after <seconds>": ends the current round that many seconds after the
// game session starts, the same as the script command mp_round_end (haloreach+0x7B3EC(-1, 0, 1),
// called by its evaluator at +0x1B699C). Lets automated runs reach the post-game results (slot 6).
// Runs on the engine thread (slot 1), which is where the engine's own scripts run.
static void EndRoundForTest() {
    static double s_seconds = -1.0;
    static bool s_parsed = false;
    static bool s_done = false;
    static ULONGLONG s_start = 0;
    if (!s_parsed) {
        s_parsed = true;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i + 1 < argc; ++i) {
            if (_wcsicmp(argv[i], L"-end-round-after") == 0) s_seconds = _wtof(argv[i + 1]);
        }
        if (argv) LocalFree(argv);
    }
    if (s_done || s_seconds < 0.0) return;
    if (s_start == 0) s_start = GetTickCount64();
    if (GetTickCount64() - s_start < static_cast<ULONGLONG>(s_seconds * 1000.0)) return;
    s_done = true;
    HMODULE reach = GetModuleHandleW(L"haloreach.dll");
    if (!reach) return;
    using RoundEndFn = void(__fastcall*)(int, int, int);
    auto roundEnd = reinterpret_cast<RoundEndFn>(reinterpret_cast<uintptr_t>(reach) + 0x7B3EC);
    NOBLE_INFO("ReachHost: -end-round-after {} s reached, ending the round", s_seconds);
    roundEnd(-1, 0, 1);
}

// Slot 1: Frame tick / Render loop update callback (host_probe case 1)
using SwapChainPresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
static SwapChainPresentFn s_originalSwapChainPresent = nullptr;
static std::atomic<bool> s_presentHookInstalled{false};
static std::atomic<bool> s_presentHookAttempted{false};

static HRESULT STDMETHODCALLTYPE HookSwapChainPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
    (void)syncInterval;
    // DISPLAY MODE = FULLSCREEN: enter/leave DXGI exclusive full screen on this (the render) thread.
    SyncExclusiveFullscreen(swapChain);
    const SettingsData settings = Settings::Get().Snapshot();
    const UINT effectiveSyncInterval = settings.verticalSync ? 1u : 0u;

    static thread_local LONGLONG nextPresentTicks = 0;
    static thread_local int previousLimit = -1;
    LARGE_INTEGER frequency{}, now{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);

    DXGI_SWAP_CHAIN_DESC scDesc{};
    HWND hwnd = (swapChain && SUCCEEDED(swapChain->GetDesc(&scDesc))) ? scDesc.OutputWindow : nullptr;
    const bool isFocused = (hwnd == nullptr || GetForegroundWindow() == hwnd);

    // FRAME-RATE LIMIT: 0 means what it says - no pacing at all while the window is active. (It used to be
    // silently capped at 120, which made "No limit" a lie and confused the FPS counter.)
    int targetFps = settings.fpsLimit;
    if (!isFocused && settings.throttleUnfocused) {
        // While unfocused (alt-tabbed, or waiting as a guest) throttle to 30 FPS so a background window cannot peg a
        // CPU core or trip Windows' unresponsive-app detection. SETTINGS > Graphics can turn this off for streaming.
        targetFps = (targetFps > 0 && targetFps < 30) ? targetFps : 30;
    }

    if (targetFps != previousLimit) {
        previousLimit = targetFps;
        nextPresentTicks = now.QuadPart;
    }

    if (targetFps <= 0) {
        nextPresentTicks = now.QuadPart;   // uncapped: present immediately
    } else if (frequency.QuadPart > 0) {
        const LONGLONG interval = frequency.QuadPart / targetFps;
        if (now.QuadPart < nextPresentTicks) {
            LONGLONG remaining = nextPresentTicks - now.QuadPart;
            while (remaining > (frequency.QuadPart / 500)) { // >= 2ms
                Sleep(1);
                QueryPerformanceCounter(&now);
                remaining = nextPresentTicks - now.QuadPart;
            }
            while (now.QuadPart < nextPresentTicks) {
                YieldProcessor();
                QueryPerformanceCounter(&now);
            }
        }
        QueryPerformanceCounter(&now);
        nextPresentTicks = now.QuadPart + interval;
    }

    return s_originalSwapChainPresent
        ? s_originalSwapChainPresent(swapChain, effectiveSyncInterval, flags)
        : DXGI_ERROR_INVALID_CALL;
}

static void InstallPresentSettingsHook(IDXGISwapChain* swapChain) {
    if (!swapChain || s_presentHookAttempted.exchange(true)) return;
    void** vtable = *reinterpret_cast<void***>(swapChain);
    if (!vtable || !vtable[8]) {
        NOBLE_WARN("ReachHost: swap chain has no IDXGISwapChain::Present entry");
        return;
    }

    if (HookManager::InstallHook(vtable[8], reinterpret_cast<void*>(&HookSwapChainPresent),
        reinterpret_cast<void**>(&s_originalSwapChainPresent), "dxgi_swapchain_present_settings")) {
        s_presentHookInstalled.store(true);
    } else {
        NOBLE_WARN("ReachHost: VSync and frame-rate settings could not hook IDXGISwapChain::Present");
    }
}

static uint64_t __fastcall HostSlot1_FrameTick(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a2; (void)a3; (void)a4;
    // haloreach.dll invokes Slot 1 every frame before Present. a1 = the swap chain it is about to
    // present (RR host::hit case 1). Our passes draw here, on the engine thread, because the engine
    // owns the immediate context.
    static std::atomic<uint64_t> s_frameCount{0};
    uint64_t frames = ++s_frameCount;
    if (ReachArmory::IsMenuMode()) {
        ReachArmory::Tick();
        if (a1 > 0x10000) {
            IDXGISwapChain* swapChain = reinterpret_cast<IDXGISwapChain*>(a1);
            InstallPresentSettingsHook(swapChain);
            ExecuteGammaPass(swapChain);
            try {
                FrontEndMenu::RenderOverlay(swapChain);
            } catch (const std::exception& e) {
                NOBLE_ERROR("ReachHost [Slot 1]: exception in FrontEndMenu::RenderOverlay: {}", e.what());
            } catch (...) {
                NOBLE_ERROR("ReachHost [Slot 1]: unknown exception in FrontEndMenu::RenderOverlay");
            }
            if (s_testCaptureRequest.exchange(false)) FrameCapture::SaveNow(swapChain, 777777);
            FrameCapture::OnFrame(swapChain, frames);
        }
        return 0;
    }
    // SETTINGS > VIDEO field of view: keep the engine's camera FOV in step with the saved setting
    // (cheap: it only reads the settings and writes haloreach+0xB42C10 when the value changed).
    CameraFov::Tick();
    if (a1 > 0x10000) {
        IDXGISwapChain* swapChain = reinterpret_cast<IDXGISwapChain*>(a1);
        InstallPresentSettingsHook(swapChain);
        ExecuteGammaPass(swapChain);   // brightness/gamma first
        // Loading screen until the game session starts. Launch state 11 comes ~5 s too early (the
        // screen is still black); the first slot-10 call (secondary interface, game session start) is
        // when the world appears. Fallback: 60 s.
        static bool s_loadingDone = false;
        static ULONGLONG s_firstTick = GetTickCount64();
        bool loading = false;
        if (!s_loadingDone) {
            int curLaunchState = -1;
            if (HMODULE reach = GetModuleHandleW(L"haloreach.dll")) {
                SafeReadInt32(reinterpret_cast<uintptr_t>(reach) + 0xC1A114, &curLaunchState);
            }
            const bool waitingForRemoteJoin = (curLaunchState >= 0 && curLaunchState < 10 &&
                                               NetworkTransport::Get().IsActive() && !NetworkTransport::Get().IsHost());
            if (s_sessionStarted.load() || (!waitingForRemoteJoin && GetTickCount64() - s_firstTick > 60000)) {
                s_loadingDone = true;
                NOBLE_INFO("ReachHost [Slot 1 - FrameTick]: game session started at frame {}, loading screen off", frames);
            } else {
                loading = true;
            }
        }
        if (!loading) EndRoundForTest();
        ForgeMenu::Tick(loading);
        std::function<void()> forgeOverlay;
        if (ForgeMenu::IsOpen() && !PauseMenu::IsOpen() && !PauseMenu::IsGameOver())
            forgeOverlay = [] { ForgeMenu::DrawOverlay(); };
        PauseMenu::Render(swapChain, loading, forgeOverlay);  // overlays on top, unaffected by gamma
        if (s_testCaptureRequest.exchange(false)) FrameCapture::SaveNow(swapChain, 777777);
        FrameCapture::OnFrame(swapChain, frames);  // test screenshots (-capture-frames)
    }
    if (frames == 1 || (frames % 300) == 0) {
        // Engine "external launch" state machine (haloreach+0xC1A114): 0..10 while launching, 11 = launched.
        // State 10 waits for the game session to be ready.
        int launchState = -1;
        if (HMODULE reach = GetModuleHandleW(L"haloreach.dll")) {
            SafeReadInt32(reinterpret_cast<uintptr_t>(reach) + 0xC1A114, &launchState);
        }
        NOBLE_INFO("ReachHost [Slot 1 - FrameTick]: Frame {} pumped from haloreach.dll (LoadProgress: {:.1f}%, launch state {})",
            frames, LoadScreen::Get().GetLoadProgress() * 100.0f, launchState);
    }
    if (frames == 1 || (frames % 1800) == 0) {
        // Memory use over time (leak tracking): private bytes and working set of the process.
        PROCESS_MEMORY_COUNTERS_EX pmc{};
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
            NOBLE_INFO("Memory: frame {} private {} MB, working set {} MB", frames,
                       pmc.PrivateUsage / (1024 * 1024), pmc.WorkingSetSize / (1024 * 1024));
        }
    }
    return 0;
}

// Slot 2: SwapChain / Buffer resize callback (host_probe case 2)
static uint64_t __fastcall HostSlot2_ResizeBuffers(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a1; (void)a2; (void)a3; (void)a4;
    ResetGammaPass();
    static std::atomic<bool> s_logged{false};
    if (!s_logged.exchange(true)) {
        NOBLE_INFO("ReachHost [Slot 2 - ResizeBuffers]: Engine requested swap chain buffer refresh");
    }
    return 0;
}

// Slot 4: Engine logging stream (host_probe case 4)
// Slot 4 (RR host::hit case 4): a1 = reason code, a2 = const char* message.
// The engine reports session/launch events here, e.g. reason 4 with
// "external_launch_individual_state_timeout" / "external_launch_overall_timeout".
static uint64_t __fastcall HostSlot4_LogMessage(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a2; (void)a3; (void)a4;
    char buf[512] = { 0 };
    if (a2 > 0x10000) {
        SafeCopyAnsiString(buf, sizeof(buf), reinterpret_cast<const char*>(a2));
    }
    NOBLE_WARN("ReachHost [Slot 4 - EngineEvent]: reason={} message='{}'", a1, buf);
    if (a1 == 4 && NetworkTransport::Get().IsActive() && !NetworkTransport::Get().IsHost() && !s_sessionStarted.load()) {
        if (std::strstr(buf, "timeout") != nullptr) {
            NOBLE_ERROR("ReachHost [Slot 4 - EngineEvent]: Remote session join timed out ('{}'); returning to main menu.", buf);
            if (Relaunch::HandOff(L"")) {  // ends this process; true is never returned
                if (HWND hwnd = FindWindowW(L"HaloReachCE_WindowClass", nullptr)) {
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                }
            }
        }
    }
    return 0;
}

// Slot 10: Secondary host interface (host_probe case 10 returns SECOND, the iface2 object
// that host::new_host stores at host[1]). Returning 1 here would hand the engine a bad pointer.
static uint64_t __fastcall HostSlot10_Heartbeat(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a1; (void)a2; (void)a3; (void)a4;
    static std::atomic<bool> s_logged{false};
    if (!s_logged.exchange(true)) {
        NOBLE_INFO("ReachHost [Slot 10 - SecondInterface]: returning secondary host interface");
    }
    s_sessionStarted.store(true);
    return reinterpret_cast<uint64_t>(ReachHost::Get().GetSecondInterface());
}

// Slot 18: Asset lookup (host_probe case 0x12)
static uint64_t __fastcall HostSlot18_AssetLookup(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a1; (void)a2; (void)a3; (void)a4;
    return 0;
}

// Slot 23: Load progress handler (host-probe slot23 at 0x1400822a0)
static std::atomic<uint32_t> s_loadPhases{0};
static std::atomic<float> s_loadFraction{0.0f};

// Slot 6: match over. haloreach+0xF95CB builds MCC's game-results record (0x5D138 bytes, filled
// from the engine's own results by haloreach+0x39128) and calls host slot 6 with a pointer to it
// (a1). a2 isn't a size: it is whatever r8 held after the conversion loop (0xD5A8).
// Noble shows its post-game panel and hands the record to the career code (cR, stats).
// Layout notes: docs/HANDOFF.md, "Game results".
static uint64_t __fastcall HostSlot6_GameResults(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a2; (void)a4;
    int launchState = -1;
    if (HMODULE reach = GetModuleHandleW(L"haloreach.dll")) {
        SafeReadInt32(reinterpret_cast<uintptr_t>(reach) + 0xC1A114, &launchState);
    }
    NOBLE_INFO("ReachHost [Slot 6 - GameResults]: record=0x{:X} (a2=0x{:X} a3=0x{:X}, launch state {})", a1, a2, a3, launchState);
    void* stack[16];
    WORD frames = RtlCaptureStackBackTrace(0, 16, stack, nullptr);
    uintptr_t reachBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"haloreach.dll"));
    std::string stackStr;
    for (WORD i = 0; i < frames; ++i) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(stack[i]);
        if (reachBase && addr >= reachBase && addr < reachBase + 0x5000000) {
            char buf[64];
            snprintf(buf, sizeof(buf), "hr+0x%llX ", static_cast<unsigned long long>(addr - reachBase));
            stackStr += buf;
        }
    }
    if (!stackStr.empty()) {
        NOBLE_INFO("ReachHost [Slot 6 - GameResults]: caller stack: {}", stackStr);
    }
    if (launchState >= 11 && a1 > 0x10000) {
        static std::vector<uint8_t> s_record;
        s_record.assign(Career::kResultsSize, 0);
        if (SafeCopyMemory(s_record.data(), reinterpret_cast<const void*>(a1), s_record.size())) {
            Career::OnGameResults(s_record.data(), s_record.size());
        } else {
            NOBLE_WARN("ReachHost [Slot 6 - GameResults]: results record not readable");
        }
    }
    if (launchState >= 11) PauseMenu::ShowGameOver();  // only for a match that was actually running
    return 0;
}

static uint64_t __fastcall HostSlot23_LoadProgress(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a2; (void)a3; (void)a4;
    void* caller = _ReturnAddress();
    
    // a1 is phase index or integer percentage, a2 is optional progress float
    uint32_t phaseIndex = static_cast<uint32_t>(a1 & 0xFFFFFFFF);
    if (phaseIndex < 32) {
        s_loadPhases.fetch_or(1u << (phaseIndex & 0x1F));
    }

    float fraction = 0.0f;
    if (a1 <= 100) {
        fraction = static_cast<float>(a1) / 100.0f;
    } else {
        memcpy(&fraction, &a1, sizeof(float));
    }

    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    s_loadFraction.store(fraction);
    LoadScreen::Get().SetLoadProgress(fraction);

    static float s_lastLoggedFraction = -1.0f;
    if (std::abs(fraction - s_lastLoggedFraction) >= 0.01f || fraction >= 1.0f) {
        s_lastLoggedFraction = fraction;
        NOBLE_INFO("ReachHost [Slot 23 - LoadProgress]: progress={:.1f}% (raw=0x{:X}, phases=0x{:08X}) [Caller: 0x{:X}]",
            fraction * 100.0f, a1, s_loadPhases.load(), reinterpret_cast<uintptr_t>(caller));
    }
    return 0;
}

// Slot 32: Display settings fill (host_probe case 0x20)
// Matches Reach Renaissance host::hit case 0x20: a1 = engine-owned buffer, a2 = its size (0x88 seen).
// RR zeroes the buffer, writes {width, height} as two u32 at +0x00, quality bytes at +0x08..+0x17
// (01 02 02 02 02 02 01 01 | 00 01 01 01 00 02 00 00), and returns 1.
// The previous version treated this as an allocation request and returned its own buffer, leaving
// the engine's settings struct uninitialised.
static uint64_t __fastcall HostSlot32_DisplaySettings(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a2; (void)a3; (void)a4;
    static std::atomic<bool> s_logged{false};
    if (!s_logged.exchange(true)) {
        NOBLE_INFO("ReachHost [Slot 32 - DisplaySettings]: buffer=0x{:X} size=0x{:X}", a1, a2);
    }
    if (a1 <= 0x10000 || a2 < 0x18) {
        return 0;
    }

    const size_t size = static_cast<size_t>(a2);
    if (!SafeZeroMemory(reinterpret_cast<void*>(a1), size)) {
        return 0;
    }

    uint8_t header[0x18] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // width, height (filled below)
        0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01,   // quality settings (RR defaults)
        0x00, 0x01, 0x01, 0x01, 0x00, 0x02, 0x00, 0x00
    };
    // Size of Noble's window / swap chain (set by main from the DISPLAY settings).
    const uint32_t width = ReachHost::Get().GetDisplayWidth();
    const uint32_t height = ReachHost::Get().GetDisplayHeight();
    memcpy(&header[0], &width, sizeof(width));
    memcpy(&header[4], &height, sizeof(height));
    SafeCopyMemory(reinterpret_cast<void*>(a1), header, sizeof(header));
    return 1;
}

// Slot 33: Display Quality settings handler (host_probe case 0x21)
static uint64_t __fastcall HostSlot33_DisplayQuality(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a2; (void)a3; (void)a4;
    // RR host::hit case 0x21 writes these 16 bytes straight into the buffer a1 points to. (The old
    // code followed a1 again when the buffer already held a pointer-like value, writing into
    // whatever that value pointed at and leaving the engine's settings uninitialised.)
    const uintptr_t targetPtr = a1;
    if (targetPtr > 0x10000) {
        uint64_t val1 = 0x3F80000000000201ULL; // Scale = 1.0f (0x3F800000)
        uint64_t val2 = 0x2BF800000ULL;
        SafeWriteUint64(targetPtr, val1);
        SafeWriteUint64(targetPtr + 8, val2);
    }
    return 1;
}

// Slot 34: Profile & Armor settings handler (host_probe case 0x22)
//
// The engine takes its keyboard/mouse bindings from this profile. Without them every key
// reaches the engine (slot 36) but is bound to nothing; only mouse look works.
// Binding table: 66 entries of 0x18 bytes from +0x42C, each { u32 action, u32 VK, 16 zero bytes },
// actions 0..65. The keys below are MCC's default PC layout (actions 62..65 are unbound).
static constexpr size_t kBindingTableOffset = 0x42C;
static constexpr size_t kBindingEntrySize = 0x18;
static bool ApplyBindingsToEngineSEH(uintptr_t base, const uint8_t* profile) {
    (void)profile;
    using ApplyBindingsFn = void(__fastcall*)(uint8_t* ps);
    bool appliedAny = false;
    __try {
        uint8_t* pool = reinterpret_cast<uint8_t*>(base + 0xEC6EA0);
        for (int slot = 0; slot < 4; ++slot) {
            uint8_t* ps = pool + slot * 0x1018;
            if (*reinterpret_cast<const uint32_t*>(ps + 0x04) == 2) {
                reinterpret_cast<ApplyBindingsFn>(base + 0x2C8DFC)(ps);
                *reinterpret_cast<uint32_t*>(ps + 0x0C) = 0;
                *reinterpret_cast<uint32_t*>(ps + 0x00) |= 0xC00u;
                appliedAny = true;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return appliedAny;
}

static void WriteEngineEmblemAndDirtySEH(uintptr_t base, const uint8_t emblem[8]) {
    using SetEmblemFn = void(__fastcall*)(uint8_t* ps, const uint8_t* emb);
    __try {
        uint8_t* pool = reinterpret_cast<uint8_t*>(base + 0xEC6EA0);
        for (int slot = 0; slot < 4; ++slot) {
            uint8_t* ps = pool + slot * 0x1018;
            if (*reinterpret_cast<const uint32_t*>(ps + 0x04) == 2) {
                reinterpret_cast<SetEmblemFn>(base + 0x2C7FF4)(ps, emblem);
                *reinterpret_cast<uint32_t*>(ps + 0x0C) = 0;
                *reinterpret_cast<uint32_t*>(ps + 0x00) |= 0xC00u;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void WriteDefaultKeyBindings(uint8_t* profile) {
    const SettingsData settings = Settings::Get().Snapshot();
    for (uint32_t action = 0; action < 66; ++action) {
        uint8_t* entry = profile + kBindingTableOffset + action * kBindingEntrySize;
        // MCC's default layout plus the player's changes from the SETTINGS tab.
        auto it = settings.keyOverrides.find(static_cast<int>(action));
        const uint32_t vk = static_cast<uint32_t>(it != settings.keyOverrides.end() ? it->second
                                                                                  : KeyBindings::DefaultKey(static_cast<int>(action)));
        const uint32_t none = 0;
        memset(entry, 0, kBindingEntrySize);
        uint32_t effectiveVk = vk;
        if (action == 9 && effectiveVk == 0) {
            const int toggleVk = KeyBindings::EffectiveKey(KeyBindings::kActionToggleCrouch);
            if (toggleVk > 0 && toggleVk < 256) effectiveVk = static_cast<uint32_t>(toggleVk);
        }
        memcpy(entry + 0, &action, sizeof(action));
        memcpy(entry + 4, &effectiveVk, sizeof(effectiveVk));
        memcpy(entry + 8, &none, sizeof(none));
    }
    // Entries are {u32 action, u32 key, u32 second key, 12 zero bytes}. The mouse wheel is bound
    // through the second key: the engine turns the wheel pulse at input +0x114 into key 0xAF (up,
    // +1) or 0xAE (down, -1). RR binds 0xAF to action 4 (change weapon). Tested 2026-10-28:
    // 0xAE as action 4's second key + a -1 pulse switches weapons; 0xB0 does nothing. Only one
    // second key per action works, so when both directions go to the same action only 0xAF is bound
    // and FillLocalInput sends down notches as up (see WheelPulseSign).
    const int up = settings.wheelUpAction, down = settings.wheelDownAction;
    if (up >= 0 && up < 66) {
        const uint32_t code = 0xAF;
        memcpy(profile + kBindingTableOffset + up * kBindingEntrySize + 8, &code, sizeof(code));
    }
    if (down >= 0 && down < 66) {
        const uint32_t code = down == up ? 0xAF : 0xAE;
        memcpy(profile + kBindingTableOffset + down * kBindingEntrySize + 8, &code, sizeof(code));
    }
    profile[0x22] = 0;
    profile[0x24] = 0;
    static uint8_t s_lastAppliedTable[66 * kBindingEntrySize] = {};
    static bool s_hasAppliedTable = false;
    if (!s_hasAppliedTable || std::memcmp(s_lastAppliedTable, profile + kBindingTableOffset, sizeof(s_lastAppliedTable)) != 0) {
        if (HMODULE hReach = GetModuleHandleW(L"haloreach.dll")) {
            if (ApplyBindingsToEngineSEH(reinterpret_cast<uintptr_t>(hReach), profile)) {
                std::memcpy(s_lastAppliedTable, profile + kBindingTableOffset, sizeof(s_lastAppliedTable));
                s_hasAppliedTable = true;
                NOBLE_INFO("ReachHost [Slot 34]: Live-applied updated keyboard & mouse bindings to haloreach.dll");
            }
        }
    }
    if (false) {
    }
}

// Controller look speed (SETTINGS > CONTROLS > GAMEPAD). Tested 2026-10-28 with -fake-pad and
// -profile-set: +0x1B6 = 0 turns the view about a third as far as the stock 0x12 (horizontal look
// speed, engine range 0..18); +0x1B5 and +0x1B7 = 0 left horizontal turning unchanged. +0x1B5 is
// taken to be the vertical speed (same range and default as +0x1B6).
static void WritePadLookSettings(uint8_t* profile) {
    const SettingsData s = Settings::Get().Snapshot();
    profile[0x1B5] = static_cast<uint8_t>(InputTuning::LookProfileByte(s.padLookSpeedY));
    profile[0x1B6] = static_cast<uint8_t>(InputTuning::LookProfileByte(s.padLookSpeedX));
}

// Pulse to send for one wheel notch (+1 = up, -1 = down), or 0 to drop it (direction unbound).
static int WheelPulseSign(bool upNotch) {
    const SettingsData s = Settings::Get().Snapshot();
    const int action = upNotch ? s.wheelUpAction : s.wheelDownAction;
    if (action < 0) return 0;
    if (upNotch) return 1;
    return s.wheelDownAction == s.wheelUpAction ? 1 : -1;
}

// Audio volumes are profile settings too (RR live::audio::set_volume): f32 at +0xA5C (game/master),
// +0xA60 (music) and +0xA64 (the third volume). A blank profile leaves them at 0.0, i.e. silence.
static uint8_t s_hostProfileBuffer[0x10000] = { 0 };

static constexpr size_t kVolumeGameOffset = 0xA5C;
static constexpr size_t kVolumeMusicOffset = 0xA60;
static constexpr size_t kVolumeOtherOffset = 0xA64;

static void WriteDefaultVolumes(uint8_t* profile) {
    const SettingsData settings = Settings::Get().Snapshot();
    memcpy(profile + kVolumeGameOffset, &settings.volumeGame, sizeof(float));
    memcpy(profile + kVolumeMusicOffset, &settings.volumeMusic, sizeof(float));
    memcpy(profile + kVolumeOtherOffset, &settings.volumeOther, sizeof(float));
}

void ReachHost::SetRuntimeVolumes(float game, float music, float other) {
    const float g = std::clamp(game, 0.0f, 1.0f);
    const float m = std::clamp(music, 0.0f, 1.0f);
    const float o = std::clamp(other, 0.0f, 1.0f);
    memcpy(s_hostProfileBuffer + kVolumeGameOffset, &g, sizeof(float));
    memcpy(s_hostProfileBuffer + kVolumeMusicOffset, &m, sizeof(float));
    memcpy(s_hostProfileBuffer + kVolumeOtherOffset, &o, sizeof(float));
}

// Look / controller settings block (+0x1B4..+0x1DF). Without it zoomed look sensitivity is 0 and
// the view freezes while scoped (walking still works). RR's live::save_pad_look writes into this
// block (+0x1B5, +0x1B7 as u8, +0x1B8 as u32/f32). Values are the stock settings: sensitivity
// bytes, two dead-zone floats (0.13, 0.12), two 1.0 multipliers, then on/off option bytes.
static constexpr size_t kLookSettingsOffset = 0x1B4;
static const uint8_t kDefaultLookSettings[0x2C] = {
    0x00, 0x12, 0x12, 0x07,                         // +0x1B4 sensitivity bytes
    0xB8, 0x1E, 0x05, 0x3E,                         // +0x1B8 f32 0.13
    0x8F, 0xC2, 0xF5, 0x3D,                         // +0x1BC f32 0.12
    0x00, 0x00, 0x80, 0x3F,                         // +0x1C0 f32 1.0
    0x00, 0x00, 0x80, 0x3F,                         // +0x1C4 f32 1.0
    0x09, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, // +0x1C8 option bytes
    0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, // +0x1D0
    0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, // +0x1D8
};

static void WriteDefaultLookSettings(uint8_t* profile) {
    memcpy(profile + kLookSettingsOffset, kDefaultLookSettings, sizeof(kDefaultLookSettings));
}

// "-profile <path>" (or "--profile <path>") on the command line, else empty.
static std::wstring ProfileArgPath() {
    std::wstring result;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i + 1 < argc; ++i) {
            if (wcscmp(argv[i], L"-profile") == 0 || wcscmp(argv[i], L"--profile") == 0) {
                result = argv[i + 1];
            }
        }
        LocalFree(argv);
    }
    return result;
}

static std::filesystem::path ProfileCaptureDirArgPath() {
    std::wstring result;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i + 1 < argc; ++i) {
            if (wcscmp(argv[i], L"-capture-profile") == 0 || wcscmp(argv[i], L"--capture-profile") == 0) {
                result = argv[i + 1];
            }
        }
        LocalFree(argv);
    }
    if (result.empty()) return {};

    std::filesystem::path captureDir(result);
    if (captureDir.is_relative()) {
        wchar_t exe[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, exe, MAX_PATH)) {
            captureDir = std::filesystem::path(exe).parent_path() / captureDir;
        }
    }
    return captureDir;
}

static bool s_hostProfileInitialized = false;
static bool s_profileBuiltIn = false;   // false when -profile supplied the profile
static std::vector<std::pair<uint32_t, uint8_t>> s_profilePatches;   // -profile-set test option

static void CaptureProfileSnapshotIfChanged(const uint8_t* profile) {
    static const std::filesystem::path captureDir = ProfileCaptureDirArgPath();
    if (captureDir.empty() || !profile) return;

    static std::array<uint8_t, sizeof(s_hostProfileBuffer)> lastSnapshot{};
    static bool hasSnapshot = false;
    static size_t snapshotIndex = 0;
    static const std::string captureSessionId = [] {
        FILETIME time = {};
        GetSystemTimeAsFileTime(&time);
        ULARGE_INTEGER ticks = {};
        ticks.LowPart = time.dwLowDateTime;
        ticks.HighPart = time.dwHighDateTime;
        return std::to_string(GetCurrentProcessId()) + "-" + std::to_string(ticks.QuadPart);
    }();
    static std::mutex captureMutex;
    std::lock_guard<std::mutex> lock(captureMutex);

    if (hasSnapshot && std::equal(lastSnapshot.begin(), lastSnapshot.end(), profile)) return;

    try {
        std::filesystem::create_directories(captureDir);
        const auto snapshotPath = captureDir / ("profile-" + captureSessionId + "-" + std::to_string(snapshotIndex) + ".bin");
        std::ofstream file(snapshotPath, std::ios::binary | std::ios::trunc);
        if (!file) {
            NOBLE_WARN("ReachHost [Slot 34]: could not create profile capture '{}'", snapshotPath.string());
            return;
        }
        file.write(reinterpret_cast<const char*>(profile), sizeof(s_hostProfileBuffer));
        if (!file) {
            NOBLE_WARN("ReachHost [Slot 34]: failed writing profile capture '{}'", snapshotPath.string());
            return;
        }

        std::copy_n(profile, lastSnapshot.size(), lastSnapshot.begin());
        hasSnapshot = true;
        ++snapshotIndex;
        NOBLE_INFO("ReachHost [Slot 34]: captured changed profile buffer to '{}'", snapshotPath.string());
    } catch (const std::filesystem::filesystem_error& e) {
        NOBLE_WARN("ReachHost [Slot 34]: could not create profile capture in '{}': {}", captureDir.string(), e.what());
    }
}

// Armory appearance -> profile. haloreach+0x2EDA08 copies these profile fields into the player's
// settings (Elite flag at +0x27/+0x38, then dword "global item ids" at +0x40..+0xA0). A global item
// id is the item's position in MCC's own Reach customization list (the HR_* names in
// MCC-Win64-Shipping.exe, HR_Helmet_MarkVB_Base = 0 ... HR_Chest_UACounterAssault_RoboArm = 239);
// haloreach+0x3B2F4 turns it into the engine's index for that slot. A field left at 0 becomes
// item 0 of its slot, which is why the Elite always spawned as a Minor before.
namespace armory_ids {
constexpr uint32_t kLeftShoulder = 90;     // HR_LeftShoulder_Default .. _Security (18, Armory order)
constexpr uint32_t kRightShoulder = 108;   // HR_RightShoulder_Default .. _Security (18)
constexpr uint32_t kWrist = 144;           // 6, Armory order
constexpr uint32_t kUtility = 150;         // 7, Armory order
constexpr uint32_t kVisor = 157;           // 5, Armory order
constexpr uint32_t kKnees = 162;           // 4, Armory order
constexpr uint32_t kEffect = 166;          // 7, Armory order
constexpr uint32_t kEliteEffect = 173;     // 7, same order
constexpr uint32_t kSpartanMale = 192;     // HR_Spartan_Male, +1 = Female
constexpr uint32_t kEliteSet = 194;        // HR_Elite_Minor .. _Officer (8, Armory order)
constexpr uint32_t kColor = 202;           // HR_Color_Steel .. _Pale (30, Armory order)

// Helmets by the Armory's helmet_id (permutation string id) -> MCC item.
static const std::unordered_map<std::string, uint32_t>& Helmets() {
    // 2026-10-29: 13 helmets' ids were swapped round in the Armory (Tyler checked each one in the menu's 3D
    // Armory: e.g. the menu model's player_malcolm head is the ODST). Each item number below stays with its
    // helmet name; only the id next to it changed.
    static const std::unordered_map<std::string, uint32_t> k = {
        {"default", 0}, {"default_a", 1}, {"default_b", 2},                 // Mark V [B]
        {"rosenda", 3}, {"rosenda_a", 4}, {"rosenda_b", 5},                          // CQC
        {"malcolm", 6}, {"malcolm_a", 7}, {"malcolm_b", 8},                             // ODST
        {"skull", 9}, {"skull_a", 10}, {"skull_b", 11},                     // HAZOP
        {"vinh", 12}, {"vinh_a", 13}, {"vinh_b", 14},                       // EOD
        {"isaac", 15}, {"isaac_a", 16}, {"isaac_b", 17},                 // Operator
        {"joshua", 18}, {"joshua_a", 19}, {"joshua_b", 20},                 // Grenadier
        {"female", 21}, {"female_a", 22}, {"female_b", 23},                 // Air Assault
        {"jun", 24}, {"jun_a", 25}, {"jun_b", 26},                          // Scout
        {"player_li", 27}, {"player_li_a", 28}, {"player_li_b", 29},                    // EVA
        {"nicole", 30}, {"nicole_a", 31}, {"nicole_b", 32},           // JFO
        {"carter", 33}, {"carter_a", 34}, {"carter_b", 35},                 // Commando
        {"emile", 36}, {"emile_a", 37}, {"emile_b", 38},                    // EVA [C]
        {"john117", 39}, {"john117_a", 40}, {"john117_b", 41},                       // Mjolnir Mk. V
        {"sam", 42}, {"sam_a", 43}, {"sam_b", 44}, {"sam_c", 45},     // Pilot (+ Haunted)
        {"jai", 46}, {"jai_a", 47}, {"jai_b", 48},              // Recon
        {"will", 49}, {"will_a", 50}, {"will_b", 51},              // Mjolnir Mk. VI
        {"kelly", 52}, {"kelly_a", 53}, {"kelly_b", 54},                          // Gungnir
        {"linda", 55}, {"linda_a", 56}, {"linda_b", 57},                    // Security
        {"adrianna", 58}, {"adrianna_a", 59}, {"adrianna_b", 60},        // Military Police
        {"kurt", 61}, {"kurt_a", 62}, {"kurt_b", 63},              // CQB
        {"cndmnd", 64}, {"cndmnd_a", 67}, {"cndmnd_b", 65}, {"cndmnd_c", 66}, // Mariner: base, Optrex, Defiant, Mister Chief
        {"hilands", 68}, {"hilands_b", 69}, {"hilands_a", 70}, {"hilands_c", 71}, // GRD: base, Optrex, CBRN, Optrex/CBRN
        {"s8_helmet_belos_corinthian", 72}, {"s8_helmet_belos_transverse", 73},
        {"s8_helmet_belos_kerata", 74}, {"s8_helmet_belos_leontokardos", 75},
        {"s8_helmet_belos_stheno", 76}, {"s8_helmet_belos_cerberus", 77},
        {"s8_helmet_blackguard_base", 78}, {"s8_helmet_blackguard_bishop", 79},
        {"s8_helmet_blackguard_dragon", 80}, {"s8_helmet_blackguard_wind", 81},
        {"s8_helmet_blackguard_visage", 82}, {"s8_helmet_blackguard_crown", 83},
        {"s8_helmet_drengr_base", 84}, {"s8_helmet_drengr_valdemar", 85},
        {"s8_helmet_drengr_dvalinn", 86}, {"s8_helmet_drengr_nari", 87},
        {"s8_helmet_drengr_thyra", 88}, {"s8_helmet_drengr_tyr", 89},
    };
    return k;
}

// Chests by the Armory's chest_id -> MCC item (the [W]/[R] and later chests sit after the colours).
static const std::unordered_map<std::string, uint32_t>& Chests() {
    static const std::unordered_map<std::string, uint32_t> k = {
        {"chest_blank", 126}, {"chest_1", 127}, {"chest_2", 128}, {"chest_3", 129}, {"chest_4", 130},
        {"chest_5", 131}, {"chest_6", 132}, {"chest_7", 133}, {"chest_8", 134}, {"chest_9", 135},
        {"chest_10", 136}, {"chest_11", 137}, {"chest_12", 138},
        {"chest_wp_2", 139}, {"chest_robo_1", 140}, {"chest_robo_2", 141}, {"chest_robo_3", 142},
        {"dlc2_chest_cndmnd", 143},
        {"chest_kurt", 232}, {"chest_13", 233}, {"chest_wp_1", 234},
        {"chest_default_roboarm", 235}, {"chest_7_roboarm", 236}, {"chest_5_roboarm", 237},
        {"chest_12_roboarm", 238}, {"chest_2_roboarm", 239},
        // The same [R] chests under the engine's own permutation names (the Armory uses these since 2026-10-29).
        {"chest_tacticalpatrol_roboarm", 236}, {"chest_tacticalrecon_roboarm", 237},
        {"chest_collargrenadierua_roboarm", 238}, {"chest_uacounterassault_roboarm", 239},
    };
    return k;
}

// Firefight voices by the Armory's voice_id -> MCC item (HR_FirefightVoice_*). Unknown ids -> Noble Six.
static uint32_t VoiceItem(const std::string& id) {
    static const std::unordered_map<std::string, uint32_t> k = {
        {"noble_six", 180}, {"jorge", 181}, {"jun", 182}, {"emile", 183}, {"kat", 184}, {"dot", 185},
        {"carter", 186}, {"stacker", 187}, {"buck", 188}, {"johnson", 189}, {"cortana", 190}, {"chief", 191},
    };
    const auto it = k.find(id);
    return it != k.end() ? it->second : 180;
}
} // namespace armory_ids

static std::filesystem::path ResolveAppearanceFilePath() {
    const auto dataDir = ReachHost::Get().GetDataDir();
    const auto datPath = dataDir / "appearance.dat";
    std::error_code ec;
    if (std::filesystem::exists(datPath, ec)) return datPath;
    const auto jsonPath = dataDir / "appearance.json";
    if (std::filesystem::exists(jsonPath, ec)) return jsonPath;
    return datPath;
}

static void ApplyArmoryAppearance(uint8_t* profile) {
    if (!profile) return;
    const auto appearancePath = ResolveAppearanceFilePath();
    nlohmann::json appearance;
    if (!SaveCodec::ReadJson(appearancePath, appearance)) return;

    try {
        using namespace armory_ids;
        auto num = [](const nlohmann::json& j, const char* key, int lo, int hi, int def) {
            const auto it = j.find(key);
            return (it != j.end() && it->is_number_integer()) ? std::clamp(it->get<int>(), lo, hi) : def;
        };
        auto str = [](const nlohmann::json& j, const char* key) {
            const auto it = j.find(key);
            return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
        };
        auto put = [profile](uint32_t offset, uint32_t id) { memcpy(profile + offset, &id, sizeof(id)); };

        const uint8_t species = static_cast<uint8_t>(num(appearance, "species", 0, 1, 0));
        profile[0x27] = species;
        profile[0x38] = species;

        const nlohmann::json empty = nlohmann::json::object();
        const nlohmann::json& sp = appearance.contains("spartan") && appearance["spartan"].is_object() ? appearance["spartan"] : empty;
        const nlohmann::json& el = appearance.contains("elite") && appearance["elite"].is_object() ? appearance["elite"] : empty;

        uint32_t helmet = 0;
        if (auto it = Helmets().find(str(sp, "helmet_id")); it != Helmets().end()) helmet = it->second;
        uint32_t chest = 126;
        if (auto it = Chests().find(str(sp, "chest_id")); it != Chests().end()) chest = it->second;
        // haloreach.dll+0x2EDA08 passes profile[+0x68] (Spartan effect) and profile[+0x90] (Elite effect)
        // through haloreach.dll+0x3B2F4, which expects MCC global item ids kEffect + 0..6 (166..172) and
        // kEliteEffect + 0..6 (173..179) in Armory order:
        // 0 = default, 1 = birthday_party, 2 = heart_attack, 3 = pestilence,
        // 4 = inclement_weather, 5 = flames, 6 = blue_flames.
        auto armoryEffectOffset = [](int idx, const std::string& id) -> uint32_t {
            if (id == "birthday_party") return 1;
            if (id == "heart_attack") return 2;
            if (id == "pestilence") return 3;
            if (id == "inclement_weather") return 4;
            if (id == "flames") return 5;
            if (id == "blue_flames") return 6;
            if (id == "default") return 0;
            return static_cast<uint32_t>(std::clamp(idx, 0, 6));
        };
        // haloreach.dll+0x3B2F4 maps MCC visor item ids 157..161 (kVisor + 0..4) as:
        // 157 (+0) -> 0 (Default), 158 (+1) -> 1 (Silver), 159 (+2) -> 2 (Blue),
        // 160 (+3) -> 4 (Gold),    161 (+4) -> 3 (Black).
        // ArmorCustomizer's kVisorColors has index 3 = Black and index 4 = Gold, so swap 3 <-> 4.
        auto visorOffset = [](int idx, const std::string& id) -> uint32_t {
            if (id == "silver") return 1;
            if (id == "blue") return 2;
            if (id == "gold") return 3;
            if (id == "black") return 4;
            if (id == "default") return 0;
            // Noble Armory visor colours have no stock profile item. Until the in-game tint override lands,
            // use the closest stock visor so the profile stays valid instead of writing item id 162+.
            if (id == "rose_gold") return 3;
            if (id == "emerald" || id == "violet") return 2;
            if (id == "ice") return 1;
            if (id == "crimson") return 0;
            if (id == "onyx") return 4;
            if (id == "amber") return 0;
            if (id == "cyan") return 2;
            if (id == "ruby") return 0;
            if (id == "sapphire") return 2;
            if (id == "amethyst") return 2;
            if (id == "bronze") return 3;
            if (id == "sunfire") return 3;
            if (id == "lime") return 1;
            if (id == "platinum") return 1;
            if (id == "obsidian") return 4;
            if (id == "pearl") return 1;
            if (id == "electric_blue") return 2;
            if (id == "blood_orange") return 0;
            if (id == "mint") return 1;
            if (id == "copper") return 3;
            if (id == "midnight") return 4;
            if (id == "magenta") return 2;
            if (id == "aquamarine") return 2;
            if (id == "titanium") return 1;
            if (idx == 3) return 4;
            if (idx == 4) return 3;
            return static_cast<uint32_t>(std::clamp(idx, 0, 4));
        };

        const uint32_t spEffectOff = armoryEffectOffset(num(sp, "armor_effect_index", 0, 6, 0), str(sp, "armor_effect_id"));
        const uint32_t elEffectOff = armoryEffectOffset(num(el, "armor_effect_index", 0, 6, 0), str(el, "armor_effect_id"));
        const uint32_t visorOff = visorOffset(num(sp, "visor_color_index", 0, 29, 0), str(sp, "visor_id"));
        const int utility = num(sp, "utility_index", 0, 6, 0);

        put(0x40, helmet);
        put(0x44, kLeftShoulder + num(sp, "left_shoulder_index", 0, 17, 0));
        put(0x48, kRightShoulder + num(sp, "right_shoulder_index", 0, 17, 0));
        put(0x4C, chest);
        put(0x50, kWrist + num(sp, "wrist_index", 0, 5, 0));
        put(0x54, kUtility + utility);
        put(0x64, kSpartanMale + num(appearance, "body_type", 0, 1, 0));
        // Keep the two effect fields separate from the equipped character (Tyler 2026-10-30): +0x68 is
        // the SPARTAN armor effect and +0x90 the ELITE armor effect regardless of the species picked.
        // The engine copies them to appearance-record bytes +0x18 (Spartan, pmcg list 80) and +0x19
        // (Elite, pmcg list 8C) - mixing species into +0x68 is what used to make an armor effect pick
        // the Elite variant. (Verified in haloreach.dll 0x2EDA08 / 0x1055EC.)
        put(0x68, kEffect + spEffectOff);
        put(0x6C, kKnees + num(sp, "knee_guards_index", 0, 3, 0));
        put(0x70, kVisor + visorOff);
        // Armory set indices: 0..7 = MCC's eight Elite sets (194..201), 8 = Honor Guard, 9 = H2A Ranger,
        // 10 = Counselor. MCC has no set item for 8..10 - those presets are carried by their per-piece
        // entries in the appearance record (ElitePack) - so the profile set item stays at Minor (194).
        const int eliteSet = num(el, "armor_set", 0, 10, 0);
        put(0x8C, kEliteSet + (eliteSet < 8 ? eliteSet : 0));
        put(0x90, kEliteEffect + elEffectOff);
        put(0x94, VoiceItem(str(sp, "voice_id")));
        const int primaryColor = (species == 1 && el.contains("primary_color"))
            ? num(el, "primary_color", 0, 29, 1)
            : num(appearance, "primary_color", 0, 29, 1);
        const int secondaryColor = (species == 1 && el.contains("secondary_color"))
            ? num(el, "secondary_color", 0, 29, 10)
            : num(appearance, "secondary_color", 0, 29, 10);
        const int accentColor = (species == 1 && el.contains("accent_color"))
            ? num(el, "accent_color", 0, 29, 14)
            : num(appearance, "accent_color", 0, 29, 14);

        put(0x98, kColor + primaryColor);
        put(0x9C, kColor + secondaryColor);
        // +0xA0 is the third/detail colour ("accent"); Reach has no third picker in the stock Armory, but
        // the profile field exists (RR: Steel) and the Armory's third colour word follows it.
        put(0xA0, kColor + accentColor);

        const uint8_t emblem8[8] = {
            static_cast<uint8_t>(num(appearance, "emblem_index", 0, 255, 0)),
            static_cast<uint8_t>(num(appearance, "emblem_background_index", 0, 255, 0)),
            0u, // bit0 = toggle secondary layer (0 = show secondary layer)
            static_cast<uint8_t>(num(appearance, "emblem_primary_color", 0, 29, 0)),
            static_cast<uint8_t>(num(appearance, "emblem_secondary_color", 0, 29, 0)),
            static_cast<uint8_t>(num(appearance, "emblem_background_color", 0, 29, 17)),
            0u,
            0u
        };
        if (HMODULE hReach = GetModuleHandleW(L"haloreach.dll")) {
            WriteEngineEmblemAndDirtySEH(reinterpret_cast<uintptr_t>(hReach), emblem8);
        }

        const std::string armoryTag = str(appearance, "service_tag");
        if (!armoryTag.empty()) {
            wchar_t serviceTag[4] = {};
            for (size_t i = 0; i < 4 && i < armoryTag.size(); ++i) {
                serviceTag[i] = static_cast<wchar_t>(armoryTag[i]);
            }
            memcpy(profile + 0x1AC, serviceTag, sizeof(serviceTag));
        }
        NOBLE_INFO("ReachHost [Slot 34]: applied Armory appearance ({}, helmet {}, chest {}, effect {}/{}, Elite set {}, colours {}/{})",
                   species == 0 ? "Spartan" : "Elite", helmet, chest, spEffectOff, elEffectOff, eliteSet,
                   primaryColor, secondaryColor);
    } catch (const nlohmann::json::exception& e) {
        NOBLE_WARN("ReachHost [Slot 34]: could not read Armory appearance from '{}': {}", appearancePath.string(), e.what());
    }
}

// Re-applies appearance.dat when the Armory saves it, so a change made from the menu reaches
// the profile without a restart (the game reads the profile again when the player spawns).
static void RefreshArmoryAppearance(uint8_t* profile, bool force = false) {
    static std::filesystem::file_time_type s_lastWrite{};
    static ULONGLONG s_lastCheck = 0;
    const ULONGLONG now = GetTickCount64();
    if (!force && now - s_lastCheck < 500) return;
    s_lastCheck = now;
    std::error_code ec;
    const auto path = ResolveAppearanceFilePath();
    const auto t = std::filesystem::last_write_time(path, ec);
    if (!force && (ec || t == s_lastWrite)) return;
    if (!ec) s_lastWrite = t;
    ApplyArmoryAppearance(profile);
}

static uint64_t __fastcall HostSlot34_ProfileSettings(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a1; (void)a2; (void)a3; (void)a4;
    static std::atomic<bool> s_logged{false};
    if (!s_logged.exchange(true)) {
        NOBLE_INFO("ReachHost [Slot 34 - ProfileSettings]: Providing the local player profile and settings");
    }

    if (!s_hostProfileInitialized) {
        memset(s_hostProfileBuffer, 0, sizeof(s_hostProfileBuffer));
        bool loaded = false;
        try {
            // Optional: "-profile <file>" loads a captured profile, like RR's --profile.
            // RR host::load_profile copies at most 0xC00 bytes of it.
            const std::wstring path = ProfileArgPath();
            if (!path.empty()) {
                std::filesystem::path profilePath(path);
                if (profilePath.is_relative()) {
                    // The working directory is moved to data\sandbox during start-up, so resolve
                    // relative paths against the folder noble-client.exe is in.
                    wchar_t exe[MAX_PATH] = {};
                    if (GetModuleFileNameW(nullptr, exe, MAX_PATH)) {
                        profilePath = std::filesystem::path(exe).parent_path() / profilePath;
                    }
                }
                std::ifstream pFile(profilePath, std::ios::binary);
                if (pFile) {
                    pFile.read(reinterpret_cast<char*>(s_hostProfileBuffer), 0xC00);
                    loaded = pFile.gcount() > 0;
                    NOBLE_INFO("ReachHost [Slot 34]: loaded profile from -profile ({} bytes)", pFile.gcount());
                } else {
                    NOBLE_WARN("ReachHost [Slot 34]: could not open -profile file; using built-in key bindings");
                }
            }
        } catch (...) {
            NOBLE_WARN("ReachHost [Slot 34]: failed to read -profile file; using built-in key bindings");
        }
        if (!loaded) {
            size_t pSize = 0;
            const uint8_t* embProfile = EmbeddedBlocks::GetBlock("profile_1.bin", &pSize);
            if (embProfile && pSize > 0) {
                size_t toCopy = std::min(pSize, sizeof(s_hostProfileBuffer));
                std::memcpy(s_hostProfileBuffer, embProfile, toCopy);
                NOBLE_INFO("ReachHost [Slot 34]: loaded base profile from embedded resource 'profile_1.bin' ({} bytes)", pSize);
            } else {
                WriteDefaultLookSettings(s_hostProfileBuffer);
            }
            WriteDefaultKeyBindings(s_hostProfileBuffer);
            WritePadLookSettings(s_hostProfileBuffer);
            WriteDefaultVolumes(s_hostProfileBuffer);
            loaded = true;
            s_profileBuiltIn = true;
                loaded = true;
            if (false) {
                NOBLE_INFO("ReachHost [Slot 34]: loaded authentic profile from embedded resource 'profile_1.bin' ({} bytes)", pSize);
            }
        }
        if (!loaded) {
            WriteDefaultKeyBindings(s_hostProfileBuffer);
            WriteDefaultVolumes(s_hostProfileBuffer);
            WriteDefaultLookSettings(s_hostProfileBuffer);
            NOBLE_INFO("ReachHost [Slot 34]: using built-in default key bindings, volumes and look settings");
        }
        // RR host::hit case 0x22: the profile is a zeroed 64 KB block with only the 4-character
        // service tag (UTF-16) written at +0x1AC. The old defaults here (+0x00 = 1, armour colours
        // at +0x18/+0x1C) are not what RR sends, so they are no longer written.
        // The tag is the player's (SETTINGS > GAMEPLAY > SERVICE TAG, default NOBL), 1-4 characters.
        wchar_t serviceTag[4] = {};
        const std::string tag = Settings::Get().Snapshot().serviceTag;
        for (size_t i = 0; i < 4 && i < tag.size(); ++i) serviceTag[i] = static_cast<wchar_t>(tag[i]);
        memcpy(s_hostProfileBuffer + 0x1AC, serviceTag, sizeof(serviceTag));
        RefreshArmoryAppearance(s_hostProfileBuffer, true);
        s_hostProfileInitialized = true;
        // s_profileBuiltIn stays true unless -profile was supplied on CLI
        // Test option "-profile-set <hex offset>=<hex byte>[,...]": patch profile bytes (used to find
        // what the controller look bytes at +0x1B4.. do; see FillLocalPad and -fake-pad).
        {
            // Also read from the NOBLE_PROFILE_SET environment variable: starting a game relaunches Noble without
            // the menu's command line, but child processes keep the environment (set NOBLE_PROFILE_SET=90=B0).
            std::vector<std::wstring> lists;
            wchar_t envList[512] = {};
            if (GetEnvironmentVariableW(L"NOBLE_PROFILE_SET", envList, 512) > 0) lists.push_back(envList);
            int argc = 0;
            LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            for (int i = 1; argv && i + 1 < argc; ++i) {
                if (_wcsicmp(argv[i], L"-profile-set") == 0) lists.push_back(argv[i + 1]);
            }
            for (std::wstring list : lists) {
                while (!list.empty() && (list.back() == L' ' || list.back() == L'\t')) list.pop_back();
                size_t pos = 0;
                while (pos < list.size()) {
                    size_t comma = list.find(L',', pos);
                    if (comma == std::wstring::npos) comma = list.size();
                    const std::wstring item = list.substr(pos, comma - pos);
                    const size_t eq = item.find(L'=');
                    if (eq != std::wstring::npos) {
                        const unsigned long off = wcstoul(item.substr(0, eq).c_str(), nullptr, 16);
                        const unsigned long val = wcstoul(item.substr(eq + 1).c_str(), nullptr, 16);
                        if (off < 0xC00) {
                            s_profilePatches.push_back({ static_cast<uint32_t>(off), static_cast<uint8_t>(val) });
                            NOBLE_INFO("ReachHost [Slot 34]: -profile-set +0x{:X} = 0x{:02X}", off, val & 0xFF);
                        }
                    }
                    pos = comma + 1;
                }
            }
            if (argv) LocalFree(argv);
        }
    }
    // Key bindings and the mouse wheel from SETTINGS > KEY BINDINGS also reach a running game
    // (the pause menu's SETTINGS page): the built-in profile's table is rewritten twice a second.
    if (s_profileBuiltIn) {
        static ULONGLONG s_lastBindings = 0;
        const ULONGLONG now = GetTickCount64();
        if (now - s_lastBindings >= 500) {
            s_lastBindings = now;
            WriteDefaultKeyBindings(s_hostProfileBuffer);
            WritePadLookSettings(s_hostProfileBuffer);
            WriteDefaultVolumes(s_hostProfileBuffer);
        }
    }
    RefreshArmoryAppearance(s_hostProfileBuffer);
    for (const auto& [off, val] : s_profilePatches) s_hostProfileBuffer[off] = val;

    CaptureProfileSnapshotIfChanged(s_hostProfileBuffer);

    // RR only returns the pointer; it never writes into a1/a2 (the old code copied 0x100 bytes
    // into whichever argument looked like a pointer).
    return reinterpret_cast<uint64_t>(s_hostProfileBuffer);
}

// Slot 36: local-user input poll (RR host::hit case 0x24). Called every engine tick.
// a1 = user index, a2 = 0x130-byte input state the host fills. Layout (from RR's handler):
//   +0x000 u32      1 = a local user is present
//   +0x004 u8[256]  key state indexed by VK code: 0 = up, 0x80 = pressed (< 500 ms),
//                   0x81..0xFF = held (repeat counter, as RR computes it)
//   +0x104 f32 x2   mouse look delta (x, y), already scaled
//   +0x114 f32      mouse wheel pulse (+1/-1 for 40 ms per notch, see FillLocalInput)
//   +0x118 f32      1.0
//   +0x11C u32      mouse buttons: L=1, M=2, R=4, X1=8, X2=0x10
// Index 1 is the split-screen second user: zeroed, as RR does.
static ULONGLONG s_keyDownSince[256] = {};
static std::atomic<uint32_t> s_engineKeyMask[8] = {};   // 8 x 32 bits = 256 virtual keys, mirroring the engine pad state
static bool s_inputCaptured = false;
static HCURSOR s_savedClassCursor = nullptr;
static std::atomic<bool> s_slot36Logged{false};

// Look speed (x, y) and inversion from SETTINGS > CONTROLS, re-read twice a second so changes apply in a
// running game. RR's default 0.0008 per pixel times the sensitivity; "-mouse-scale <v>" overrides both axes.
static float InputMouseScaleOverride();
static void InputLookSettings(float& scaleX, float& scaleY, bool& invert, float& accel, float& smoothing) {
    static ULONGLONG s_next = 0;
    static float s_x = 0.0008f, s_y = 0.0008f;
    static float s_accel = 0.0f, s_smoothing = 0.0f;
    static bool s_invert = false;
    const ULONGLONG now = GetTickCount64();
    if (now >= s_next) {
        s_next = now + 500;
        const SettingsData d = Settings::Get().Snapshot();
        s_x = 0.0008f * d.mouseSensitivity;
        s_y = s_x * d.mouseSensitivityY;
        s_invert = d.invertLook;
        s_accel = std::clamp(d.mouseAcceleration, 0.0f, 1.0f);
        s_smoothing = std::clamp(d.mouseSmoothing, 0.0f, 0.9f);
        if (const float o = InputMouseScaleOverride(); o > 0.0f) s_x = s_y = o;
    }
    scaleX = s_x;
    scaleY = s_y;
    invert = s_invert;
    accel = s_accel;
    smoothing = s_smoothing;
}

static float InputMouseScaleOverride() {
    static float s_scale = [] {
        float scale = 0.0f;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            for (int i = 1; i + 1 < argc; ++i) {
                if (wcscmp(argv[i], L"-mouse-scale") == 0 || wcscmp(argv[i], L"--mouse-scale") == 0) {
                    float v = static_cast<float>(_wtof(argv[i + 1]));
                    if (v > 0.0f) scale = v;
                }
            }
            LocalFree(argv);
        }
        return scale;
    }();
    return s_scale;
}

// Returns our window if it is the foreground window, else nullptr.
static HWND InputFocusedWindow() {
    HWND fg = GetForegroundWindow();
    if (!fg) return nullptr;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return (pid == GetCurrentProcessId()) ? fg : nullptr;
}

// UI cursor (menus such as the class/loadout picker). As in RR's uicursor module, haloreach+0x2D2018
// runs only while a Reach screen that uses the mouse pointer is up; we hook it and record when it
// last ran. While it ran within the last 250 ms the pointer is "free": not clipped, not re-centred,
// the normal arrow is shown, and no mouse-look delta is sent. The engine reads the pointer position
// itself (GetCursorPos/ScreenToClient).
static constexpr uintptr_t kUiScreenRva = 0x2D2018;
using UiScreenFn = uint64_t(__fastcall*)(uint64_t, uint64_t, uint64_t, uint64_t);
static UiScreenFn s_origUiScreen = nullptr;
static std::atomic<ULONGLONG> s_uiScreenLastTick{0};

static uint64_t __fastcall UiScreenHook(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    s_uiScreenLastTick.store(GetTickCount64(), std::memory_order_relaxed);
    return s_origUiScreen(a1, a2, a3, a4);
}

// Subtitles. Before drawing cinematic/dialogue subtitles the engine asks haloreach+0xD5244
// ("should subtitles show?"), which reads each local player's subtitle setting (player profile
// +0xA30: 0 automatic, 1 on, 2 off) from its own player table. Automatic only shows them when the
// voice and text languages differ, so English players never see any. With the SUBTITLES setting on,
// Noble answers "yes" here; with it off the engine decides as before.
static constexpr uintptr_t kSubtitlesEnabledRva = 0xD5244;
using SubtitlesEnabledFn = uint8_t(__fastcall*)();
static SubtitlesEnabledFn s_origSubtitlesEnabled = nullptr;
static std::atomic<bool> s_forceSubtitles{false};

static uint8_t __fastcall SubtitlesEnabledHook() {
    if (s_forceSubtitles.load(std::memory_order_relaxed)) return 1;
    return s_origSubtitlesEnabled();
}

static std::wstring SafeReadWide(const wchar_t* text, size_t maxChars);
static std::string NarrowForLog(const std::wstring& w);
static size_t SafeCopyAnsiRaw(const char* src, char* dst, size_t cap);

static void EnsureSubtitleHook(HMODULE reach) {
    s_forceSubtitles.store(Settings::Get().Snapshot().subtitles);
    uint8_t* target = reinterpret_cast<uint8_t*>(reach) + kSubtitlesEnabledRva;
    static const uint8_t kPrologue[5] = { 0x48, 0x89, 0x5C, 0x24, 0x08 };  // mov [rsp+8], rbx
    uint8_t current[5] = {};
    if (!SafeCopyMemory(current, target, sizeof(current)) || memcmp(current, kPrologue, sizeof(kPrologue)) != 0) {
        NOBLE_WARN("ReachHost [Subtitles]: haloreach+0x{:X} does not match the expected build; subtitle setting disabled",
                   kSubtitlesEnabledRva);
        return;
    }
    if (HookManager::InstallHook(target, reinterpret_cast<void*>(&SubtitlesEnabledHook),
                                 reinterpret_cast<void**>(&s_origSubtitlesEnabled), "subtitles_enabled")) {
        NOBLE_INFO("ReachHost [Subtitles]: hook installed (subtitles {})", s_forceSubtitles.load() ? "on" : "engine default");
    }
}

static void EnsureUiCursorHook() {
    static bool s_tried = false;
    if (s_tried) return;
    s_tried = true;
    HMODULE reach = GetModuleHandleW(L"haloreach.dll");
    if (!reach) return;
    EnsureSubtitleHook(reach);
    uint8_t* target = reinterpret_cast<uint8_t*>(reach) + kUiScreenRva;
    // Same check RR makes: the function must start with "mov [rsp+18h], rbx" for this build.
    static const uint8_t kPrologue[5] = { 0x48, 0x89, 0x5C, 0x24, 0x18 };
    uint8_t current[5] = {};
    if (!SafeCopyMemory(current, target, sizeof(current)) || memcmp(current, kPrologue, sizeof(kPrologue)) != 0) {
        NOBLE_WARN("ReachHost [UI cursor]: haloreach+0x{:X} does not match the expected build; menu cursor disabled", kUiScreenRva);
        return;
    }
    if (HookManager::InstallHook(target, reinterpret_cast<void*>(&UiScreenHook),
                                 reinterpret_cast<void**>(&s_origUiScreen), "ui_cursor_screen")) {
        NOBLE_INFO("ReachHost [UI cursor]: screen hook installed");
    }
}

static bool UiCursorActive() {
    const ULONGLONG last = s_uiScreenLastTick.load(std::memory_order_relaxed);
    return last != 0 && GetTickCount64() - last < 250;
}

bool ReachHost::IsUiScreenActive() { return UiCursorActive(); }

static void InputRelease(HWND hwnd, const char* reason) {
    CameraThirdPerson::SetZoomHeld(false);
    if (!s_inputCaptured) return;
    ClipCursor(nullptr);
    if (hwnd) {
        HCURSOR arrow = s_savedClassCursor ? s_savedClassCursor : LoadCursorW(nullptr, IDC_ARROW);
        SetClassLongPtrW(hwnd, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(arrow));
    }
    s_inputCaptured = false;
    NOBLE_INFO("ReachHost [Slot 36 - Input]: {}, mouse released", reason);
}

// XInput, loaded on first use (xinput1_4.dll ships with Windows 8+). The engine loads it as well and
// reads the pads itself; Noble only needs XInputSetState for rumble and GetState for tests.
struct NobleXInputGamepad { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; };
struct NobleXInputState { DWORD dwPacketNumber; NobleXInputGamepad Gamepad; };
struct NobleXInputVibration { WORD wLeftMotorSpeed, wRightMotorSpeed; };
using XInputGetStateFn = DWORD(WINAPI*)(DWORD, NobleXInputState*);
using XInputSetStateFn = DWORD(WINAPI*)(DWORD, NobleXInputVibration*);
static HMODULE XInputModule() {
    static HMODULE s_module = LoadLibraryW(L"xinput1_4.dll");
    return s_module;
}

// ------------------------------------------------------------------ controller
// With a host, haloreach.dll never loads XInput itself (haloreach+0x23855: only when the host global
// at +0xC1A100 is null); MCC hands it the pad through this same input buffer. From the engine's
// reader (haloreach+0x23CF0..0x23D87, which rebuilds an XINPUT_STATE from it):
//   +0x000 u32  device: 0 = controller (the pad fields below are used), 1 = keyboard and mouse
//   +0x120 u16  buttons (XINPUT_GAMEPAD_* bits)
//   +0x124 u8   left trigger, +0x125 u8 right trigger
//   +0x126 i16  left stick x, +0x128 y, +0x12A right stick x, +0x12C y
// Noble reads pad 0 with XInput and switches the device to whichever was used last, as MCC does.
static bool AnyKeyDown(const uint8_t* st) {
    for (int vk = 8; vk < 256; ++vk) if (st[0x004 + vk]) return true;
    return false;
}

static double FakePadAt();
static bool ReadPad(NobleXInputState& state);

// Research hook: "-pad-press <hexbutton> -pad-press-at <seconds> [-pad-press-ms <ms>]" injects one synthetic gamepad
// button press at a given time, so engine actions that no Noble UI exposes (Forge's editor-mode camera, camera modes) can be
// tested without a physical controller. Nothing is injected when the flags are absent.
struct SyntheticPadPress {
    WORD button = 0;
    int atMs = -1;
    int durationMs = 200;
    ULONGLONG startedAt = 0;
    bool started = false;
};
static SyntheticPadPress ParseSyntheticPadPress() {
    SyntheticPadPress press;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int atSeconds = -1;
    for (int i = 1; argv && i + 1 < argc; ++i) {
        if (_wcsicmp(argv[i], L"-pad-press") == 0) {
            press.button = static_cast<WORD>(std::wcstoul(argv[i + 1], nullptr, 16));
        } else if (_wcsicmp(argv[i], L"-pad-press-at") == 0) {
            atSeconds = _wtoi(argv[i + 1]);
        } else if (_wcsicmp(argv[i], L"-pad-press-ms") == 0) {
            press.durationMs = _wtoi(argv[i + 1]);
        }
    }
    if (argv) LocalFree(argv);
    if (press.button && atSeconds >= 0) {
        press.atMs = atSeconds * 1000;
        press.startedAt = GetTickCount64() + static_cast<ULONGLONG>(press.atMs);
    }
    return press;
}

bool ReachHost::IsKeyHeld(int vk) {
    if (vk < 0 || vk > 255) return false;
    return (s_engineKeyMask[vk >> 5].load() & (1u << (vk & 31))) != 0;
}

// Paddles arrive as keyboard keys or mouse buttons. FillLocalInput takes them out of the keyboard state (so they neither fire a
// keyboard action nor switch away from the controller) and FillLocalPad reads them as controller inputs. Only while a
// controller is connected: otherwise those keys are ordinary keys.
static uint8_t s_paddlesHeld = 0;
static bool s_padConnected = false;
static std::array<int, PadLayouts::kPaddleCount> s_paddleKeys{};

bool ReachHost::IsPaddleKey(int vk) {
    return s_padConnected && std::find(s_paddleKeys.begin(), s_paddleKeys.end(), vk) != s_paddleKeys.end();
}

static void FillLocalPad(uint8_t* st, bool keyboardMouseUsed) {
    static bool s_padMode = false;
    static WORD s_lastButtons = 0;
    static SyntheticPadPress s_synth = ParseSyntheticPadPress();
    NobleXInputState state{};
    bool connected = ReadPad(state);
    bool synthActive = false;
    if (s_synth.button) {
        const ULONGLONG now = GetTickCount64();
        if (!s_synth.started && now >= s_synth.startedAt) {
            s_synth.started = true;
            NOBLE_INFO("ReachHost [Slot 36 - Input]: synthetic pad press 0x{:04X} for {} ms", s_synth.button, s_synth.durationMs);
        }
        if (s_synth.started && now - s_synth.startedAt < static_cast<ULONGLONG>(s_synth.durationMs)) {
            synthActive = true;
            state = NobleXInputState{};
            state.Gamepad.wButtons = s_synth.button;
            connected = true;
            s_padMode = true;
        }
    }
    if (!connected) {
        s_padConnected = false;
        if (s_padMode) NOBLE_INFO("ReachHost [Slot 36 - Input]: controller disconnected, back to keyboard and mouse");
        s_padMode = false;
        s_lastButtons = 0;
        return;
    }
    const NobleXInputGamepad& g = state.Gamepad;
    s_padConnected = true;
    const bool padUsed = s_paddlesHeld != 0 || g.wButtons != 0 || g.bLeftTrigger > 40 || g.bRightTrigger > 40 ||
                         std::abs(g.sThumbLX) > 9000 || std::abs(g.sThumbLY) > 9000 ||
                         std::abs(g.sThumbRX) > 9000 || std::abs(g.sThumbRY) > 9000;
    if (padUsed && !s_padMode) {
        s_padMode = true;
        NOBLE_INFO("ReachHost [Slot 36 - Input]: controller in use");
    } else if (keyboardMouseUsed && !padUsed && s_padMode) {
        s_padMode = false;
        NOBLE_INFO("ReachHost [Slot 36 - Input]: keyboard and mouse in use");
    }
    // Start opens Noble's pause menu (the engine has none outside MCC), like Esc.
    // Noble's own options: extra radial dead zones (rescaled so the edge still reaches full speed)
    // and look inversion. The engine applies its own dead zones and look speed after this.
    static SettingsData s_pad;
    static ULONGLONG s_padRead = 0;
    if (GetTickCount64() - s_padRead > 500) {
        s_padRead = GetTickCount64();
        s_pad = Settings::Get().Snapshot();
    }
    // The preset and the player's bindings decide every button, Start and Back included.
    WORD buttons = g.wButtons;
    uint8_t lt = g.bLeftTrigger, rt = g.bRightTrigger;
    PadLayouts::Apply(s_pad.padLayout, buttons, lt, rt, &s_pad.padBindings, g.sThumbLX, g.sThumbLY, g.sThumbRX, g.sThumbRY, s_paddlesHeld);
    const WORD pressed = static_cast<WORD>(buttons & ~s_lastButtons);
    s_lastButtons = buttons;
    if ((pressed & 0x0010) && !PauseMenu::IsGameOver()) PauseMenu::Toggle();
    if (!s_padMode) return;
    // Controller shortcut: D-pad Left opens/closes the custom palette (monitor mode is enforced by Toggle).
    if ((pressed & 0x0004) && !PauseMenu::IsGameOver()) {
        ForgeMenu::Toggle();
        buttons = static_cast<WORD>(buttons & ~0x0004);
    }
    // The scoreboard button shows Noble's panel, like the key: the engine never opens its own screen.
    if (buttons & 0x0020) {
        const int boardKey = KeyBindings::EffectiveKey(20);
        if (boardKey > 0 && boardKey < 256) s_engineKeyMask[boardKey >> 5].fetch_or(1u << (boardKey & 31));
    }
    buttons = static_cast<WORD>(buttons & ~(0x0010 | 0x0020));   // Start and the scoreboard stay Noble's
    // Hold-to-zoom: the engine only toggles, so a short press on the way down and another on the way up.
    {
        static bool s_zoomWas = false;
        static int s_zoomPulse = 0;
        const bool zoomNow = (buttons & 0x0080) != 0;
        if (!s_pad.padToggleZoom) {
            if (zoomNow != s_zoomWas) s_zoomPulse = 2;
            buttons = static_cast<WORD>(s_zoomPulse > 0 ? (buttons | 0x0080) : (buttons & ~0x0080));
            if (s_zoomPulse > 0) --s_zoomPulse;
        }
        s_zoomWas = zoomNow;
    }
    // Toggle crouch on controller (SETTINGS > CONTROLS > CONTROLLER > TOGGLE CROUCH):
    // press crouch button (0x0040) once to stay crouched, press again to stand up.
    {
        static bool s_padCrouchWas = false;
        static bool s_padCrouchLatched = false;
        const bool crouchNow = (buttons & 0x0040) != 0;
        if (s_pad.padToggleCrouch) {
            if (crouchNow && !s_padCrouchWas) s_padCrouchLatched = !s_padCrouchLatched;
            buttons = static_cast<WORD>(s_padCrouchLatched ? (buttons | 0x0040) : (buttons & ~0x0040));
        } else {
            s_padCrouchLatched = false;
        }
        s_padCrouchWas = crouchNow;
    }
    // The maths lives in engine/InputTuning.cpp so the self-test can prove these settings change the output.
    SHORT lx = g.sThumbLX, ly = g.sThumbLY, rx = g.sThumbRX, ry = g.sThumbRY;
    InputTuning::ApplyStickLayout(s_pad.padStickLayout, lx, ly, rx, ry);
    InputTuning::ApplyDeadZone(lx, ly, s_pad.padLeftDeadZone);
    InputTuning::ApplyDeadZone(rx, ry, s_pad.padRightDeadZone);
    if (s_pad.padInvertLook) ry = static_cast<SHORT>(ry == -32768 ? 32767 : -ry);
    // Look speed above stock (see InputTuning); below stock it is the profile value.
    rx = InputTuning::ApplyLookGain(rx, InputTuning::LookStickGain(s_pad.padLookSpeedX));
    ry = InputTuning::ApplyLookGain(ry, InputTuning::LookStickGain(s_pad.padLookSpeedY));

    *reinterpret_cast<uint32_t*>(st + 0x000) = 0;
    // Pad Back belongs to the Noble overlay: the engine would open Reach's own scoreboard over our panel. The overlay
    // reads the physical pad directly (GamepadUi), so drop Back from the state haloreach.dll sees.
    buttons = static_cast<WORD>(buttons & ~0x0020u);
    memcpy(st + 0x120, &buttons, sizeof(buttons));
    st[0x124] = lt;
    st[0x125] = rt;
    memcpy(st + 0x126, &lx, sizeof(SHORT));
    memcpy(st + 0x128, &ly, sizeof(SHORT));
    memcpy(st + 0x12A, &rx, sizeof(SHORT));
    memcpy(st + 0x12C, &ry, sizeof(SHORT));
}

static void FillLocalInput(uint8_t* st) {
    *reinterpret_cast<uint32_t*>(st + 0x000) = 1;
    *reinterpret_cast<float*>(st + 0x118) = 1.0f;

    static HWND s_lastWindow = nullptr;
    HWND hwnd = InputFocusedWindow();
    if (hwnd && Window::InSizeMove()) {
        InputRelease(hwnd, "window is being moved");
        memset(s_keyDownSince, 0, sizeof(s_keyDownSince));
        return;
    }
    if (!hwnd) {
        InputRelease(s_lastWindow, "window lost focus");
        memset(s_keyDownSince, 0, sizeof(s_keyDownSince));
        return;
    }
    s_lastWindow = hwnd;

    if (ReachArmory::IsMenuMode()) {
        InputRelease(hwnd, "front-end menu open");
        memset(s_keyDownSince, 0, sizeof(s_keyDownSince));
        return;
    }

    const bool uiPointer = UiCursorActive();

    // Esc opens/closes Noble's pause menu, except while a Reach menu owns the pointer (there Esc
    // goes to the engine, e.g. to back out of a screen).
    static bool s_escWasDown = false;
    const bool escDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    if (escDown && !s_escWasDown && !uiPointer && !PauseMenu::IsGameOver() && !SocialOverlay::OwnsEscape()) {
        if (ForgeMenu::IsOpen()) ForgeMenu::Close();
        else PauseMenu::Toggle();
    }
    s_escWasDown = escDown;

    // Insert opens/closes the custom palette; it never uses the native Forge menu's keys.
    static bool s_insertWasDown = false;
    const bool insertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (insertDown && !s_insertWasDown && !uiPointer && !PauseMenu::IsGameOver()) ForgeMenu::Toggle();
    s_insertWasDown = insertDown;

    ReachArmory::NotifyEngineTick();
    if (s_hostProfileInitialized && s_profileBuiltIn) {
        static ULONGLONG s_lastLiveSync = 0;
        const ULONGLONG syncNow = GetTickCount64();
        if (syncNow - s_lastLiveSync >= 250) {
            s_lastLiveSync = syncNow;
            WriteDefaultKeyBindings(s_hostProfileBuffer);
            WritePadLookSettings(s_hostProfileBuffer);
            WriteDefaultVolumes(s_hostProfileBuffer);
            RefreshArmoryAppearance(s_hostProfileBuffer);
        }
    }

    // While the pause menu, post-game panel or custom palette is up the game gets no keys or mouse, and the pointer is free.
    static bool s_holdMouseUntilRelease = false;
    if (PauseMenu::WantsPointer() || ForgeMenu::WantsPointer() || SocialOverlay::IsTyping()) {
        InputRelease(hwnd, SocialOverlay::IsTyping() ? "typing in chat" : ForgeMenu::WantsPointer() ? "Forge palette open" : (PauseMenu::IsGameOver() ? "post-game panel open" : "pause menu open"));
        memset(s_keyDownSince, 0, sizeof(s_keyDownSince));
        s_holdMouseUntilRelease = true;
        return;
    }
    // After the menu closes, ignore the mouse buttons until they are released, so the click on
    // RESUME doesn't fire a shot.
    if (s_holdMouseUntilRelease) {
        const bool anyButton = (GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON) | GetAsyncKeyState(VK_MBUTTON)) & 0x8000;
        if (!anyButton) s_holdMouseUntilRelease = false;
    }

    bool firstCapturedFrame = false;
    POINT center{};
    if (uiPointer) {
        // A menu is up: free the pointer and show the arrow (the window proc shows the class cursor).
        InputRelease(hwnd, "menu with pointer is up");
    } else {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        POINT tl{rc.left, rc.top}, br{rc.right, rc.bottom};
        ClientToScreen(hwnd, &tl);
        ClientToScreen(hwnd, &br);
        RECT clip{tl.x, tl.y, br.x, br.y};
        center = POINT{(tl.x + br.x) / 2, (tl.y + br.y) / 2};

        if (!s_inputCaptured) {
            HCURSOR cls = reinterpret_cast<HCURSOR>(GetClassLongPtrW(hwnd, GCLP_HCURSOR));
            if (cls) s_savedClassCursor = cls;
            SetClassLongPtrW(hwnd, GCLP_HCURSOR, 0); // hide the arrow over our window
            s_inputCaptured = true;
            firstCapturedFrame = true;
            {
                float lx, ly, la, ls;
                bool li;
                InputLookSettings(lx, ly, li, la, ls);
                NOBLE_INFO("ReachHost [Slot 36 - Input]: mouse captured for look (scale {} x {}, invert {}, accel {:.2f}, smoothing {:.2f})",
                           lx, ly, li, la, ls);
            }
        }
        ClipCursor(&clip);
    }

    // Keyboard and mouse buttons by VK code.
    const ULONGLONG now = GetTickCount64();
    for (int vk = 1; vk < 256; ++vk) {
        uint8_t v = 0;
        if (GetAsyncKeyState(vk) < 0) {
            const bool isMouse = (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON || vk == VK_XBUTTON1 || vk == VK_XBUTTON2);
            if (isMouse) {
                // Mouse buttons (primary fire, zoom, melee) stay continuous down (0x80).
                // Do not apply typing repeat arithmetic, which turns 0x80 into 0x81+ and breaks full-auto firing!
                v = 0x80;
            } else {
                if (s_keyDownSince[vk] == 0) s_keyDownSince[vk] = now;
                ULONGLONG held = now - s_keyDownSince[vk];
                if (held < 500) {
                    v = 0x80;
                } else {
                    ULONGLONG n = (held - 500) / 33;
                    if (n > 0x7E) n = 0x7E;
                    v = static_cast<uint8_t>(static_cast<int>(n) - 0x7F);
                }
            }
        } else {
            s_keyDownSince[vk] = 0;
        }
        st[0x004 + vk] = v;
    }
    {
        // Test option "-scoreboard-at <seconds>": hold Tab (scoreboard) for 3 s that long after the first
        // input poll, and save capture_777777.bmp halfway through (checks the service tag in game).
        static double s_at = -1.0;
        static bool s_parsed = false, s_shot = false;
        static ULONGLONG s_start = 0;
        if (!s_parsed) {
            s_parsed = true;
            int argc = 0;
            LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            for (int i = 1; argv && i + 1 < argc; ++i) {
                if (_wcsicmp(argv[i], L"-scoreboard-at") == 0) s_at = _wtof(argv[i + 1]);
            }
            if (argv) LocalFree(argv);
            s_start = now;
        }
        if (s_at >= 0.0) {
            const double t = static_cast<double>(now - s_start) / 1000.0;
            if (t >= s_at && t < s_at + 3.0) {
                st[0x004 + VK_TAB] = 0x80;
                if (!s_shot && t >= s_at + 1.5) {
                    s_shot = true;
                    s_testCaptureRequest.store(true);
                    NOBLE_INFO("ReachHost: -scoreboard-at {} s: scoreboard screenshot requested", s_at);
                }
            }
        }
    }
    {
        // Test option "-pause-at <seconds>": open the pause menu that long after the first input poll and save a
        // screenshot, so the menu can be reviewed without a hand on the keyboard.
        static double s_pauseAt = -1.0;
        static bool s_parsePause = false, s_opened = false;
        static ULONGLONG s_pauseBase = 0;
        if (!s_parsePause) {
            s_parsePause = true;
            int argc = 0;
            LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            for (int i = 1; argv && i + 1 < argc; ++i) {
                if (_wcsicmp(argv[i], L"-pause-at") == 0) s_pauseAt = _wtof(argv[i + 1]);
            }
            if (argv) LocalFree(argv);
            s_pauseBase = now;
        }
        if (s_pauseAt >= 0.0 && !s_opened && static_cast<double>(now - s_pauseBase) / 1000.0 >= s_pauseAt) {
            s_opened = true;
            if (!PauseMenu::IsOpen()) PauseMenu::Toggle();
            s_testCaptureRequest.store(true);
            NOBLE_INFO("ReachHost: -pause-at {} s: pause menu opened, screenshot requested", s_pauseAt);
        }
    }

    if (!uiPointer) {
        st[0x004 + VK_ESCAPE] = 0; // Esc was ours (pause menu)
    }

    {
        static ULONGLONG s_paddleRead = 0;
        if (GetTickCount64() - s_paddleRead > 500) {
            s_paddleRead = GetTickCount64();
            const SettingsData snap = Settings::Get().Snapshot();
            s_paddleKeys = snap.padPaddleKeys;
            const int thirdPersonVk = KeyBindings::EffectiveKey(KeyBindings::kActionThirdPerson);
            for (int n = 0; n < PadLayouts::kPaddleCount; ++n) {
                if (s_paddleKeys[static_cast<size_t>(n)] == thirdPersonVk) {
                    bool paddleBound = false;
                    for (const auto& [k, v] : snap.padBindings) {
                        if (v == PadLayouts::InputPaddle1 + n) { paddleBound = true; break; }
                    }
                    if (!paddleBound) s_paddleKeys[static_cast<size_t>(n)] = 0;
                }
            }
        }
        s_paddlesHeld = 0;
        for (int n = 0; s_padConnected && n < PadLayouts::kPaddleCount; ++n) {
            const int vk = s_paddleKeys[static_cast<size_t>(n)];
            if (vk <= 0 || vk > 255) continue;
            if (st[0x004 + vk]) s_paddlesHeld = static_cast<uint8_t>(s_paddlesHeld | (1u << n));
            st[0x004 + vk] = 0;
        }
    }

    // The scoreboard key is ours too: Noble's panel replaces Reach's own scoreboard, so the engine never sees the key
    // and never opens its screen. The key still reaches the overlay through s_engineKeyMask (published below). A click
    // while it is held frees the pointer for the panel's player actions instead of firing a shot.
    const int boardVk = KeyBindings::EffectiveKey(20);
    const bool boardDown = !uiPointer && boardVk > 0 && boardVk < 256 && st[0x004 + boardVk] != 0;
    if (boardDown) {
        st[0x004 + boardVk] = 0;
        if (st[0x004 + VK_LBUTTON]) {
            st[0x004 + VK_LBUTTON] = 0;
            PauseMenu::EnterScoreboardPointer();
        }
    }
    if (s_holdMouseUntilRelease) {
        st[0x004 + VK_LBUTTON] = st[0x004 + VK_RBUTTON] = st[0x004 + VK_MBUTTON] = 0;
    }

    // Lower weapon keybind (SETTINGS > CONTROLS > KEYBOARD & MOUSE > ACTIONS, unbound by default):
    // rising-edge press queues a one-shot Alert Carry toggle in PlayerControlUpdate.
    {
        const int lowerVk = KeyBindings::EffectiveKey(KeyBindings::kActionLowerWeapon);
        const bool lowerDown = !uiPointer && lowerVk > 0 && lowerVk < 256 && st[0x004 + lowerVk] != 0;
        static bool s_lowerWasDown = false;
        if (lowerDown && !s_lowerWasDown) {
            EnginePatches::RequestLowerWeaponToggle();
        }
        s_lowerWasDown = lowerDown;
    }

    // Crouch / Toggle Crouch handling (SETTINGS > CONTROLS > KEYBOARD & MOUSE > MOVEMENT):
    // Supports both the TOGGLE CROUCH setting for the primary Crouch key (slot 9) and the
    // dedicated CROUCH (TOGGLE) keybind (kActionToggleCrouch).
    {
        static SettingsData s_crouchSettings;
        static ULONGLONG s_crouchSettingsRead = 0;
        if (now - s_crouchSettingsRead > 250) {
            s_crouchSettingsRead = now;
            s_crouchSettings = Settings::Get().Snapshot();
        }
        const int crouchVk = KeyBindings::EffectiveKey(9);
        const int toggleVk = KeyBindings::EffectiveKey(KeyBindings::kActionToggleCrouch);
        const int engineCrouchVk = (crouchVk > 0 && crouchVk < 256) ? crouchVk
                                 : ((toggleVk > 0 && toggleVk < 256) ? toggleVk : 0);
        const bool crouchDown = !uiPointer && crouchVk > 0 && crouchVk < 256 && st[0x004 + crouchVk] != 0;
        const bool toggleDown = !uiPointer && toggleVk > 0 && toggleVk < 256 && st[0x004 + toggleVk] != 0;
        static bool s_crouchWasDown = false;
        static bool s_toggleWasDown = false;
        static bool s_crouchLatched = false;
        static bool s_prevToggleSetting = false;
        if (s_prevToggleSetting && !s_crouchSettings.toggleCrouch) {
            s_crouchLatched = false;
        }
        s_prevToggleSetting = s_crouchSettings.toggleCrouch;

        if (toggleDown && !s_toggleWasDown) {
            s_crouchLatched = !s_crouchLatched;
        }
        if (crouchDown && !s_crouchWasDown && toggleVk != crouchVk) {
            if (s_crouchSettings.toggleCrouch) {
                s_crouchLatched = !s_crouchLatched;
            } else if (s_crouchLatched) {
                s_crouchLatched = false;
            }
        }
        s_crouchWasDown = crouchDown;
        s_toggleWasDown = toggleDown;

        if (toggleVk > 0 && toggleVk < 256 && toggleVk != engineCrouchVk) {
            st[0x004 + toggleVk] = 0;
        }
        if (engineCrouchVk > 0 && engineCrouchVk < 256 && !uiPointer) {
            if (s_crouchSettings.toggleCrouch || s_crouchLatched) {
                const bool active = s_crouchSettings.toggleCrouch ? s_crouchLatched : (crouchDown || s_crouchLatched);
                st[0x004 + engineCrouchVk] = active ? 0x80 : 0;
            }
        }
    }

    // Vehicle 1st-person camera keybind (SETTINGS > CONTROLS > KEYBOARD & MOUSE, unbound by default):
    // rising-edge press toggles 1st-person view while seated in a vehicle.
    {
        const int vehFpVk = KeyBindings::EffectiveKey(KeyBindings::kActionVehicleFirstPerson);
        const bool vehFpDown = !uiPointer && vehFpVk > 0 && vehFpVk < 256 && st[0x004 + vehFpVk] != 0;
        CameraThirdPerson::PollVehicleFirstPersonHotkey(vehFpDown);
    }

    // Third-person weapon aim zoom: holding the bound Zoom key (slot 10, default Right Click) zooms the
    // third-person camera according to SETTINGS > GAME OPTIONS > CAMERA > THIRD-PERSON ZOOM.
    // Pressing Q must never trigger zoom unless the user explicitly set the Zoom keybind (slot 10) to Q.
    {
        const int zoomVk = KeyBindings::EffectiveKey(10);
        const bool boundZoomDown = zoomVk > 0 && zoomVk < 256 && st[0x004 + zoomVk] != 0;
        const bool wantZoom = !uiPointer && !boardDown && !s_holdMouseUntilRelease &&
                              (CameraThirdPerson::IsEnabled() || CameraThirdPerson::EngineWantsThirdPerson()) &&
                              boundZoomDown;
        CameraThirdPerson::SetZoomHeld(wantZoom);
    }

    // Mouse look: delta from the window centre, then recentre (not while a menu owns the pointer).
    float dx = 0.0f, dy = 0.0f;
    if (!uiPointer) {
        POINT pos{};
        if (GetCursorPos(&pos) && !firstCapturedFrame) {
            dx = static_cast<float>(pos.x - center.x);
            dy = static_cast<float>(pos.y - center.y);
        }
        SetCursorPos(center.x, center.y);
    }
    float scaleX = 0.0f, scaleY = 0.0f;
    bool invertLook = false;
    float accel = 0.0f, smoothing = 0.0f;
    InputLookSettings(scaleX, scaleY, invertLook, accel, smoothing);

    // Mouse smoothing + acceleration, both from SETTINGS > KEYBOARD & MOUSE, applied by the tested maths in
    // engine/InputTuning.cpp. The filter state is dropped whenever a menu owns the pointer.
    static float s_smoothX = 0.0f, s_smoothY = 0.0f;
    if (uiPointer || firstCapturedFrame) InputTuning::ResetMouseCurve(s_smoothX, s_smoothY);
    InputTuning::ApplyMouseCurve(dx, dy, accel, smoothing, s_smoothX, s_smoothY);

    if (invertLook) dy = -dy;
    *reinterpret_cast<float*>(st + 0x104) = dx * scaleX;
    *reinterpret_cast<float*>(st + 0x108) = dy * scaleY;

    uint32_t buttons = 0;
    if (st[0x004 + VK_LBUTTON])  buttons |= 0x01;
    if (st[0x004 + VK_MBUTTON])  buttons |= 0x02;
    if (st[0x004 + VK_RBUTTON])  buttons |= 0x04;
    if (st[0x004 + VK_XBUTTON1]) buttons |= 0x08;
    if (st[0x004 + VK_XBUTTON2]) buttons |= 0x10;
    *reinterpret_cast<uint32_t*>(st + 0x11C) = buttons;

    // Mouse wheel (+0x114, f32), as RR's WheelPulse: each notch becomes a +1/-1 pulse held for 40 ms,
    // at most one pulse every 80 ms, up to 3 notches queued (wheel up = +1).
    static int s_wheelPending = 0;
    static int s_pulseSign = 0;
    static ULONGLONG s_pulseStart = 0;
    s_wheelPending += Window::TakeWheelNotches();
    {
        // Test option "-wheel-after <seconds>": one wheel-down notch that long after the first input poll.
        static double s_testAt = -1.0;
        static bool s_testParsed = false, s_testDone = false;
        static ULONGLONG s_testStart = 0;
        if (!s_testParsed) {
            s_testParsed = true;
            int argc = 0;
            LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            for (int i = 1; argv && i + 1 < argc; ++i) {
                if (_wcsicmp(argv[i], L"-wheel-after") == 0) s_testAt = _wtof(argv[i + 1]);
            }
            if (argv) LocalFree(argv);
            s_testStart = GetTickCount64();
        }
        if (!s_testDone && s_testAt >= 0.0 && GetTickCount64() - s_testStart >= static_cast<ULONGLONG>(s_testAt * 1000.0)) {
            s_testDone = true;
            s_wheelPending -= 1;
            NOBLE_INFO("ReachHost: -wheel-after {} s: one wheel notch down", s_testAt);
        }
    }
    if (s_wheelPending > 3) s_wheelPending = 3;
    if (s_wheelPending < -3) s_wheelPending = -3;
    const ULONGLONG wheelNow = GetTickCount64();
    float wheel = 0.0f;
    if (s_pulseSign != 0 && wheelNow - s_pulseStart < 40) {
        wheel = static_cast<float>(s_pulseSign);
    } else if (s_pulseSign != 0 && wheelNow - s_pulseStart < 80) {
        wheel = 0.0f;
    } else if (s_wheelPending != 0) {
        const int pendingBefore = s_wheelPending;
        s_wheelPending -= s_wheelPending > 0 ? 1 : -1;
        s_pulseSign = WheelPulseSign(pendingBefore > 0);
        s_pulseStart = wheelNow;
        wheel = static_cast<float>(s_pulseSign);
    } else {
        s_pulseSign = 0;
    }
    *reinterpret_cast<float*>(st + 0x114) = wheel;

    for (int vk = 0; vk < 256; ++vk) {
        const bool down = st[0x004 + vk] != 0;
        const uint32_t bit = 1u << (vk & 31);
        if (down) s_engineKeyMask[vk >> 5].fetch_or(bit);
        else s_engineKeyMask[vk >> 5].fetch_and(~bit);
    }
    if (boardDown) s_engineKeyMask[boardVk >> 5].fetch_or(1u << (boardVk & 31));
    FillLocalPad(st, dx != 0.0f || dy != 0.0f || buttons != 0 || wheel != 0.0f || AnyKeyDown(st));
}

static uint64_t __fastcall HostSlot36_InputController(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a2; (void)a3; (void)a4;
    uint64_t userIndex = a1;
    uintptr_t outBuf = (a2 > 0x10000) ? a2 : 0;
    if (a1 > 0x10000) { // older calling-convention guess: a1 -> { int index; void* buffer; }
        int idx = 0;
        uintptr_t pSub = 0;
        if (SafeReadInt32(a1, &idx)) userIndex = static_cast<uint32_t>(idx);
        if (SafeReadPointer(a1 + 8, &pSub) && pSub > 0x10000) outBuf = pSub;
    }
    EnsureUiCursorHook();
    if (!s_slot36Logged.exchange(true)) {
        NOBLE_INFO("ReachHost [Slot 36 - Input]: first call (a1=0x{:X}, a2=0x{:X}) -> user {} buffer 0x{:X}", a1, a2, userIndex, outBuf);
    }
    if (outBuf <= 0x10000) return 1;

    alignas(16) uint8_t state[0x130] = {};
    if (static_cast<uint32_t>(userIndex) == 0) {
        FillLocalInput(state);
    }
    SafeCopyMemory(reinterpret_cast<void*>(outBuf), state, sizeof(state));
    return 1;
}


// Slot 39: pad rumble (host_probe case 0x27). haloreach+0x24194 calls it once per pad (0..3) with
// a1 = pad index and a2 -> four u16 motor speeds {low-frequency, high-frequency, left trigger,
// right trigger}; without a host it calls XInputSetState itself with the first two. Noble passes
// them on, scaled by SETTINGS > CONTROLS > VIBRATION (trigger motors aren't reachable through XInput).
static uint64_t __fastcall HostSlot39_PadRumble(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a2; (void)a3; (void)a4;
    if (a1 > 3 || a2 <= 0x10000) return 0;
    uint16_t motors[4] = {};
    if (!SafeCopyMemory(motors, reinterpret_cast<const void*>(a2), sizeof(motors))) return 0;
    static XInputSetStateFn s_set = reinterpret_cast<XInputSetStateFn>(XInputModule() ? GetProcAddress(XInputModule(), "XInputSetState") : nullptr);
    if (!s_set) return 0;

    static bool s_enabled = true;
    static float s_strength = 1.0f;
    static ULONGLONG s_lastRead = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - s_lastRead > 500) {   // settings can change from the pause menu
        s_lastRead = now;
        const SettingsData settings = Settings::Get().Snapshot();
        s_enabled = settings.padVibration;
        s_strength = settings.padVibrationStrength;
    }
    const float scale = s_enabled ? s_strength : 0.0f;
    NobleXInputVibration v{ static_cast<WORD>(motors[0] * scale), static_cast<WORD>(motors[1] * scale) };

    // XInputSetState is only sent when the value changes (the engine calls every frame for all pads).
    static WORD s_last[4][2] = { { 0xFFFF, 0xFFFF }, { 0xFFFF, 0xFFFF }, { 0xFFFF, 0xFFFF }, { 0xFFFF, 0xFFFF } };
    if (s_last[a1][0] == v.wLeftMotorSpeed && s_last[a1][1] == v.wRightMotorSpeed) return 0;
    s_last[a1][0] = v.wLeftMotorSpeed;
    s_last[a1][1] = v.wRightMotorSpeed;
    const DWORD r = s_set(static_cast<DWORD>(a1), &v);
    static std::atomic<int> s_logged{0};
    if ((v.wLeftMotorSpeed || v.wRightMotorSpeed) && s_logged.fetch_add(1) < 5) {
        NOBLE_INFO("ReachHost [Slot 39 - Rumble]: pad {} motors {} / {} (triggers {} / {}) -> XInputSetState {}",
                   a1, v.wLeftMotorSpeed, v.wRightMotorSpeed, motors[2], motors[3], r);
    }
    return 0;
}

// Test option "-fake-pad <seconds>": that many seconds after the first input poll, pad 0 reads as
// connected for 1.5 s with the right stick pushed right (no controller needed to test pad look).
static double FakePadAt() {
    static double s_at = -2.0;
    if (s_at == -2.0) {
        s_at = -1.0;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i + 1 < argc; ++i) {
            if (_wcsicmp(argv[i], L"-fake-pad") == 0) s_at = _wtof(argv[i + 1]);
        }
        if (argv) LocalFree(argv);
    }
    return s_at;
}

static bool ReadPad(NobleXInputState& state) {
    const double fakeAt = FakePadAt();
    if (fakeAt >= 0.0) {
        static ULONGLONG s_start = GetTickCount64();
        const ULONGLONG t = GetTickCount64() - s_start;
        const ULONGLONG at = static_cast<ULONGLONG>(fakeAt * 1000.0);
        static bool s_logged = false;
        if (t >= at && t < at + 1500) {
            if (!s_logged) { s_logged = true; NOBLE_INFO("ReachHost: -fake-pad: right stick held right for 1.5 s"); }
            memset(&state, 0, sizeof(state));
            state.dwPacketNumber = static_cast<DWORD>(t);
            static const bool s_vertical = wcsstr(GetCommandLineW(), L"-fake-pad-y") != nullptr;
            // Test option "-fake-pad-amount <0..32767>": how far the fake stick is pushed.
            static const SHORT s_amount = [] {
                SHORT amount = 20000;
                int argc = 0;
                LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
                for (int i = 1; argv && i + 1 < argc; ++i) {
                    if (_wcsicmp(argv[i], L"-fake-pad-amount") == 0) amount = static_cast<SHORT>(_wtoi(argv[i + 1]));
                }
                if (argv) LocalFree(argv);
                return amount;
            }();
            if (s_vertical) state.Gamepad.sThumbRY = s_amount;
            else state.Gamepad.sThumbRX = s_amount;
            return true;
        }
        if (t >= at && t < at + 1600) { memset(&state, 0, sizeof(state)); return true; }   // let go
    }
    static XInputGetStateFn s_get = reinterpret_cast<XInputGetStateFn>(XInputModule() ? GetProcAddress(XInputModule(), "XInputGetState") : nullptr);
    if (!s_get) return false;
    // A missing pad makes XInputGetState slow; ask again only every 2 s while none is connected.
    static ULONGLONG s_nextTry = 0;
    const ULONGLONG now = GetTickCount64();
    if (now < s_nextTry) return false;
    if (s_get(0, &state) != ERROR_SUCCESS) {
        s_nextTry = now + 2000;
        return false;
    }
    return true;
}

// Slot 41 & 43: Network Packet handlers (host_probe case 0x29, 0x2B)
// a4 is the endpoint port (1000 channel / 1001 out-of-band): destination port for send, the polling
// endpoint's own port for recv (hr+0x43AB88 / hr+0x43AA78). Only the low 16 bits are set by the caller.
static uint64_t __fastcall HostSlot41_NetSend(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr;
    return NetworkTransport::Get().OnNetSend(a1, reinterpret_cast<const uint8_t*>(a2), static_cast<size_t>(a3),
                                             static_cast<uint16_t>(a4 & 0xFFFF));
}

static uint64_t __fastcall HostSlot43_NetRecv(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr;
    return NetworkTransport::Get().OnNetRecv(reinterpret_cast<uint8_t*>(a1), static_cast<size_t>(a2), reinterpret_cast<uint64_t*>(a3),
                                             static_cast<uint16_t>(a4 & 0xFFFF));
}

// Slot 47: User-data folder handler (host_probe case 0x2F)
static wchar_t s_userDataFolderW[MAX_PATH] = L"data";

static uint64_t __fastcall HostSlot47_UserDataFolder(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a2; (void)a3; (void)a4;
    try {
        uint32_t kind = 0;
        uintptr_t outBufferPtr = 0;
        size_t charCapacity = (a2 > 0 && a2 < 4096) ? static_cast<size_t>(a2) : 260;

        if (a1 > 0x10000) {
            // In host-probe: a1 is struct { uint32_t kind; uint32_t pad; wchar_t* outBuffer; }*
            uint32_t k = 0;
            uintptr_t pBuf = 0;
            if (SafeReadUint32(a1, &k)) kind = k;
            if (SafeReadPointer(a1 + 8, &pBuf) && pBuf > 0x10000) outBufferPtr = pBuf;
        } else {
            kind = static_cast<uint32_t>(a1 & 0xFFFFFFFF);
            outBufferPtr = a2;
            if (a3 > 0 && a3 < 4096) charCapacity = static_cast<size_t>(a3);
        }

        auto& host = ReachHost::Get();
        const std::string devId = DeviceIdentityStore::Get().deviceId;
        std::filesystem::path userPath = host.GetDataDir() / "userdata" / devId / "saves" / "HaloReach";
        std::error_code ec;
        std::filesystem::create_directories(userPath, ec);

        // Migrate any legacy kind1/kind2 saves if they exist
        std::filesystem::path legacyKind = host.GetDataDir() / "userdata" / ("kind" + std::to_string(kind)) / "HaloReach";
        if (std::filesystem::exists(legacyKind, ec)) {
            for (const auto& item : std::filesystem::directory_iterator(legacyKind, ec)) {
                std::filesystem::path dest = userPath / item.path().filename();
                if (!std::filesystem::exists(dest, ec)) {
                    std::filesystem::copy(item.path(), dest, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
                }
            }
        }

        std::filesystem::path absPath = std::filesystem::absolute(userPath, ec);
        if (ec) absPath = userPath;

        std::wstring pathW = absPath.wstring();
        SafeCopyWideString(s_userDataFolderW, MAX_PATH, pathW.c_str());

        NOBLE_INFO("ReachHost [Slot 47 - UserDataFolder]: kind={} outBuf=0x{:X} cap={} -> '{}'", 
            kind, outBufferPtr, charCapacity, userPath.string());

        if (outBufferPtr > 0x10000) {
            wchar_t* pDest = reinterpret_cast<wchar_t*>(outBufferPtr);
            size_t copyLen = (std::min)(charCapacity > 0 ? charCapacity - 1 : 259, pathW.length());
            SafeCopyMemory(pDest, pathW.c_str(), copyLen * sizeof(wchar_t));
            SafeWriteUint16(reinterpret_cast<uintptr_t>(pDest + copyLen), 0);
        }

        return 1; // Return 1 (success) - caller in haloreach.dll checks 'test al, al; je ...'
    } catch (...) {
        return 0;
    }
}

// Slot 48: File path resolver handler (host_probe case 0x30)
static thread_local wchar_t s_resolvedPathBufW[MAX_PATH];
static thread_local char s_resolvedPathBufA[MAX_PATH];

static uint64_t __fastcall HostSlot48_FilePathResolver(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a4; // haloreach.dll passes 3 arguments; a4 is uninitialized stack memory and must never be touched
    try {
        uint32_t kind = static_cast<uint32_t>(a1 & 0xFFFFFFFF);
        uintptr_t inOutPtr = 0;
        size_t capacity = (a3 > 0 && a3 <= 4096) ? static_cast<size_t>(a3) : 260;

        if (a1 > 0x10000) {
            uint32_t k = 0;
            uintptr_t pSub = 0;
            if (SafeReadUint32(a1, &k)) kind = k;
            if (SafeReadPointer(a1 + 8, &pSub) && pSub > 0x10000) {
                inOutPtr = pSub;
            } else {
                inOutPtr = a1;
            }
        } else {
            inOutPtr = a2;
        }

        if (inOutPtr <= 0x10000) return 0;

        wchar_t localW[MAX_PATH] = { 0 };
        char localA[MAX_PATH] = { 0 };
        bool isWide = false;
        if (!SafeExtractString(inOutPtr, localW, MAX_PATH, localA, MAX_PATH, &isWide)) {
            return 0;
        }

        std::wstring inStr;
        if (isWide) {
            inStr = localW;
        } else {
            int len = MultiByteToWideChar(CP_UTF8, 0, localA, -1, nullptr, 0);
            if (len > 0) {
                inStr.resize(len - 1);
                MultiByteToWideChar(CP_UTF8, 0, localA, -1, inStr.data(), len);
            }
        }

        if (inStr.empty()) return 0;

        auto& host = ReachHost::Get();
        std::error_code ec;
        std::filesystem::path resolved;

        std::filesystem::path checkUser = host.GetDataDir() / inStr;
        std::filesystem::path checkUserMenuMap = host.GetDataDir() / "menumaps" / inStr;
        std::filesystem::path checkCustomMainMenu = host.GetDataDir() / "menumaps" / "mainmenu_all.map";
        std::filesystem::path checkUserArmor = host.GetDataDir() / "armor" / inStr;
        std::filesystem::path checkUserMaps = host.GetDataDir() / "maps" / inStr;
        std::filesystem::path mccRoot = host.GetReachDir().has_parent_path() ? host.GetReachDir().parent_path() : host.GetReachDir();
        std::filesystem::path checkMccRelative = mccRoot / inStr;
        std::filesystem::path checkRetailReach = host.GetReachDir() / inStr;
        std::filesystem::path checkRetailMaps = host.GetMapsDir() / inStr;

        std::wstring inLower = inStr;
        std::transform(inLower.begin(), inLower.end(), inLower.begin(), ::towlower);

        bool isMainMenu = (inLower.find(L"mainmenu.map") != std::wstring::npos);

        // Let the VFS/mod layer win over the retail map: Slot 48 never went through CreateFileW,
        // so before this check a `-mod` map override was silently ignored and the retail maps\<name>.map
        // was always loaded. ResolvePath already prefers the active mod, then user data, then retail.
        std::wstring vfsPath = VFS::ResolvePath(inStr, GENERIC_READ);
        bool hasVfsPath = !vfsPath.empty() && vfsPath != inStr && std::filesystem::is_regular_file(vfsPath, ec);

        if (auto pack = ElitePack::ResolveFile(inStr)) {
            resolved = *pack;  // the Elite armour pack the engine loads with the match map
        } else if (isMainMenu && std::filesystem::is_regular_file(checkCustomMainMenu, ec)) {
            resolved = checkCustomMainMenu;
        } else if (std::filesystem::is_regular_file(checkUser, ec)) {
            resolved = checkUser;
        } else if (std::filesystem::is_regular_file(checkUserMenuMap, ec)) {
            resolved = checkUserMenuMap;
        } else if (std::filesystem::is_regular_file(checkUserArmor, ec)) {
            resolved = checkUserArmor;
        } else if (std::filesystem::is_regular_file(checkUserMaps, ec)) {
            resolved = checkUserMaps;
        } else if (hasVfsPath) {
            resolved = vfsPath;
        } else if (std::filesystem::is_regular_file(checkMccRelative, ec)) {
            resolved = checkMccRelative;
        } else if (std::filesystem::is_regular_file(checkRetailReach, ec)) {
            resolved = checkRetailReach;
        } else if (std::filesystem::is_regular_file(checkRetailMaps, ec)) {
            resolved = checkRetailMaps;
        } else {
            // Matching host_probe case 0x30 (0x14007c600.c lines 3070-3210):
            // If the file does not exist on disk (e.g. "haloreach\maps\.map"), return 0 without writing to inOutPtr!
            return 0;
        }

        std::filesystem::path absPath = std::filesystem::absolute(resolved, ec);
        if (ec) absPath = resolved;

        std::wstring resolvedW = absPath.wstring();
        SafeCopyWideString(s_resolvedPathBufW, MAX_PATH, resolvedW.c_str());

        std::string resolvedA = absPath.string();
        SafeCopyAnsiString(s_resolvedPathBufA, MAX_PATH, resolvedA.c_str());

        // Safely write the resolved path back into inOutPtr without touching caller stack frames
        if (isWide) {
            SafeCopyWideString(reinterpret_cast<wchar_t*>(inOutPtr), capacity, resolvedW.c_str());
        } else {
            SafeCopyAnsiString(reinterpret_cast<char*>(inOutPtr), capacity, resolvedA.c_str());
        }

        static std::unordered_map<std::string, bool> s_seenPaths;
        static std::mutex s_seenMutex;
        bool isNew = false;
        {
            std::lock_guard<std::mutex> lock(s_seenMutex);
            std::string key = std::string(localA) + "->" + resolvedA;
            if (s_seenPaths.find(key) == s_seenPaths.end()) {
                s_seenPaths[key] = true;
                isNew = true;
            }
        }
        if (isNew) {
            NOBLE_INFO("ReachHost [Slot 48 - FilePathResolver]: kind={} in='{}' -> resolved='{}'", 
                kind, localA, resolvedA);
        }

        return 1;
    } catch (const std::exception& e) {
        NOBLE_ERROR("ReachHost [Slot 48]: Exception caught: {}", e.what());
        spdlog::default_logger()->flush();
        return 0;
    } catch (...) {
        NOBLE_ERROR("ReachHost [Slot 48]: Unknown exception caught!");
        spdlog::default_logger()->flush();
        return 0;
    }
}

// Slot 51: Engine capability query handler (host_probe case 0x33)
// Slot 51: feature query (RR host::hit case 0x33). a1 = feature id, a2 = bool* answer,
// a3 = optional u64* that RR clears. RR answers from a bit mask (FEATURES_ON, default 0x6E, the
// value its launcher also passes with --features-on): features 1, 2, 3, 5 and 6 on, the rest off.
// Noble used to answer "on" for every feature, which turns on features RR leaves off.
static constexpr uint32_t kFeaturesOn = 0x6E;
static uint64_t __fastcall HostSlot51_EngineCapability(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a4;
    const uint32_t feature = static_cast<uint32_t>(a1);
    const bool on = feature < 32 && ((kFeaturesOn >> feature) & 1u) != 0;
    static std::atomic<uint32_t> s_loggedMask{0};
    if (feature < 32 && !(s_loggedMask.fetch_or(1u << feature) & (1u << feature))) {
        NOBLE_INFO("ReachHost [Slot 51 - Feature]: feature {} -> {}", feature, on ? "on" : "off");
    }
    if (a2 <= 0x10000) return 0;
    SafeWriteUint8(a2, on ? 1 : 0);
    if (a3 > 0x10000) SafeWriteUint64(a3, 0);
    return 1;
}

// Slot 52: Engine State query (host_probe case 0x34)
static uint64_t __fastcall HostSlot52_EngineState(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a1; (void)a2; (void)a3; (void)a4;
    return 1;
}

// -----------------------------------------------------------------------------
// Localization String Table (Slots 55, 56, 57).
// haloreach.dll queries localized strings for loadout names, armor abilities, frag grenade
// counts, weapon HUD icons, game mode variants, scoreboard headers, and team names.
// -----------------------------------------------------------------------------
static const wchar_t s_emptyWideString[] = L"";

static std::wstring Utf8ToWideString(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (sizeNeeded <= 0) return L"";
    std::wstring wide(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &wide[0], sizeNeeded);
    return wide;
}

static std::string NarrowForLog(const std::wstring& w) {
    std::string out;
    for (wchar_t c : w) out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    return out;
}

struct FallbackStringEntry {
    uint32_t tableId;
    uint32_t stringId;
    const char* str;
};

static const FallbackStringEntry s_fallbackStrings[] = {
    // Selectable Loadout Classes (fixes class boxes / missing names)
    { 0x84141d30, 0x8e52, "sprint" },
    { 0x84141d30, 0x8e54, "armor lock" },
    { 0x84141d30, 0x8e55, "active camo" },
    { 0x84141d30, 0x8e56, "hologram" },
    { 0x84141d30, 0x8e58, "jet pack" },

    // Frag Grenade Count in Loadout (evaluated dynamically by engine from token)
    { 0x84141d30, 0x8e34, "\x07" "data_loadout.prop_frag_grenade_count" "\x07" },

    // Loadout Weapon Glyphs
    { 0x84141d30, 0x80ca, "\xEE\x84\x92" }, // \ue112 (Assault Rifle)
    { 0x84141d30, 0x8e1a, "\xEE\x84\x94" }, // \ue114 (Magnum)
    { 0x84141d30, 0x8e19, "\xEE\x84\x9E" }, // \ue11e (DMR)

    // Loadout UI Prompts
    { 0x84141d30, 0x8e11, "Select Loadout \xEE\x91\x9A   Cancel \xEE\x91\x9B" },
    { 0x848f1dab, 0x02dd, "Press \xEE\x91\x9E to access Loadout Menu" },

    // Gametypes & Teams
    { 0x8e95263a, 0xb726, "Slayer" },
    { 0x90dc1ddf, 0x0175, "Red Team" },
    { 0x90dc1ddf, 0x0176, "Blue Team" },
    { 0x90dc1ddf, 0x0177, "Green Team" },
    { 0x90dc1ddf, 0x0178, "Orange Team" },
    { 0x90dc1ddf, 0x0179, "Purple Team" },
    { 0x90dc1ddf, 0x017a, "Gold Team" },
    { 0x90dc1ddf, 0x017b, "Brown Team" },
    { 0x90dc1ddf, 0x017c, "Pink Team" },

    // Scoreboard & Timer
    { 0x83771c93, 0x03cf, "%d:%02d" },
    { 0x83981cb4, 0x8087, "\x07" "data_player_scoreboard.prop_player_score" "\x07" },
    { 0x83981cb4, 0x8088, "\x07" "data_player_scoreboard.prop_player_place" "\x07" },
    { 0x83981cb4, 0x8048, "\xEE\x91\xA9 Scroll  \xEE\x91\x9A Select" },
    { 0x83981cb4, 0x807f, "\x07" "countdown_time_formatter.prop_time_string" "\x07" },
    { 0x84091d25, 0x8da3, "Respawn in \x07" "camera_list.prop_respawn_time" "\x07" },
    { 0x83981cb4, 0x8099, "Saving stats..." },
    { 0x8e95263a, 0xb745, "Game over" },
    { 0x914d28b3, 0x75af, "#winning_team wins!" },
    { 0x8e95263a, 0xb58c, "You committed suicide" },

    // Weapon pickups & icons
    { 0x848f1dab, 0x0414, "\xEE\x84\x92" },
    { 0x848f1dab, 0x041c, "\xEE\x84\x94" },
    { 0x848f1dab, 0x0422, "\xEE\x84\xA5" },
    { 0x848f1dab, 0x0424, "\xEE\x84\xAE" },
    { 0x848f1dab, 0x0417, "\xEE\x84\xB1" },
    { 0x848f1dab, 0x0416, "\xEE\x84\xAF" },
    { 0x848f1dab, 0x3331, "Hold \xEE\x91\x87 to swap for\r\n\xEE\x84\x92" },
    { 0x848f1dab, 0x3332, "Picked up an Assault Rifle" },
    { 0x848f1dab, 0x2d93, "Hold  \xEE\x91\x87 to swap for\r\n\xEE\x84\xA5" },
    { 0x848f1dab, 0x2d94, "Picked up a Needler" },
    { 0x848f1dab, 0x3245, "Hold \xEE\x91\x87 to swap for\r\n\xEE\x84\xAE" },
    { 0x848f1dab, 0x3246, "Picked up a Plasma Repeater" },
    { 0x848f1dab, 0x2e75, "Hold \xEE\x91\x87 to swap for\r\n\xEE\x84\xB1" },
    { 0x848f1dab, 0x2e76, "Picked up a Sniper Rifle" },
    { 0x848f1dab, 0x2f77, "Hold \xEE\x91\x87 to swap for\r\n\xEE\x84\xAF" },
    { 0x848f1dab, 0x2f78, "Picked up a Shotgun" },
    { 0x848f1dab, 0x3194, "Hold \xEE\x91\x87 to swap for\r\n\xEE\x84\x94" },

    // Map Locations (Sword Base)
    { 0x82091b8a, 0x074f, "Rest Stop Balcony" },
    { 0x82091b8a, 0x074c, "Cafe Square" },
    { 0x82091b8a, 0x0750, "Rest Stop 2nd Floor" },
    { 0x82091b8a, 0x0753, "Courtyard Balcony" },
    { 0x82091b8a, 0x0756, "Courtyard" },
    { 0x82091b8a, 0x0752, "Upper Courtyard" },
    { 0x82091b8a, 0x0749, "Cafe Hall" },
    { 0x82091b8a, 0x074a, "Visitor Center" },
    { 0x82091b8a, 0x074b, "Overlook" },
    { 0x82091b8a, 0x074d, "Service Tunnels" },
    { 0x82091b8a, 0x0751, "Rest Stop 1st Floor" },

    // Game Variant / Date
    { 0x83771c93, 0x60000e, "<game_variant_name/>" },
    { 0x83771c93, 0x60000f, "<game_variant_name/> on <map_variant_name/>, <day_of_week/> <month_name/> <day/>, <year/> <time_hms/>" },
    { 0x83771c93, 0x0166, "Friday" },
    { 0x83771c93, 0x0172, "October" },
    { 0x837b1c97, 0x800035, "[L]" },
    { 0x837b1c97, 0x800021, "[E]" },
    { 0x848f1dab, 0x93e5, "25m" },
    { 0x848f1dab, 0x9391, "WARNING" },
    { 0x848f1dab, 0x2ea1, "DIST\r\nELEV" },
    { 0x848f1dab, 0x2e9e, "5x" },
    { 0x848f1dab, 0x935e, "01000100011000010111011001100101" },
    { 0x848f1dab, 0x935f, "RANGE FOR 90\xC2\xB0 FOV CALIBRATION" },
    { 0x848f1dab, 0x935d, "SPARTAN III BIOS S56.7854.3354-DKW" }
};

static size_t SafeCopyWideRaw(const wchar_t* src, wchar_t* dst, size_t cap);
static size_t SafeCopyAnsiRaw(const char* src, char* dst, size_t cap);
static std::wstring SafeReadWide(const wchar_t* text, size_t maxChars);

// Localised text for the engine (slots 55-57), following RR's strings module:
//  * slot 56 register(table, id, const char* name, const char* suffix): the engine names a string
//    "<name>_<suffix>"; its key is CRC-32/MPEG-2 (poly 0x04C11DB7, init 0xFFFFFFFF, no final xor)
//    of that name in upper case, without a leading '$';
//  * slot 57 lookup(table, id, wchar_t* out, size_t capacity): copy the text for that key into the
//    engine's buffer and return 1, or return 0 when there is none;
//  * the text comes from MCC's own language file, <MCC>\data\ui\Localization\EN_HaloReach.bin,
//    read at run time from the player's install (nothing is copied into Noble).
// File layout: u32 magic 0x90CE6B7A at +0x00 and +0x40; u32 at +0x78 = size of the key array
// (u32 keys) that starts at +0x80; u32 at +0x7C = size of the string block right after it, which
// holds one NUL-terminated UTF-8 string per key, in the same order.
// Noble's small hard-coded table (s_fallbackStrings) is still used for ids that are not found.
class StringTable {
public:
    static StringTable& Get() {
        static StringTable instance;
        return instance;
    }

    void Initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return;
        for (const auto& entry : s_fallbackStrings) {
            m_fallback[MakeKey(entry.tableId, entry.stringId)] = Utf8ToWideString(entry.str);
        }
        m_initialized = true;
    }

    bool LoadLanguageFile(const std::filesystem::path& file) {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            NOBLE_WARN("StringTable: language file not found: {}", file.string());
            return false;
        }
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto u32 = [&](size_t off) -> uint32_t {
            uint32_t v = 0;
            if (off + 4 <= data.size()) memcpy(&v, data.data() + off, 4);
            return v;
        };
        constexpr uint32_t kMagic = 0x90CE6B7A;
        if (data.size() < 0x80 || u32(0) != kMagic || u32(0x40) != kMagic) {
            NOBLE_WARN("StringTable: {} is not a language file Noble understands", file.string());
            return false;
        }
        const size_t keyBytes = u32(0x78) & ~3u;
        const size_t textStart = 0x80 + static_cast<size_t>(u32(0x78));
        size_t textEnd = textStart + u32(0x7C);
        if (textStart > data.size()) return false;
        textEnd = std::min(textEnd, data.size());

        std::unordered_map<uint32_t, std::wstring> language;
        size_t pos = textStart;
        for (size_t k = 0; k < keyBytes / 4 && pos < textEnd; ++k) {
            const uint32_t key = u32(0x80 + k * 4);
            size_t end = pos;
            while (end < textEnd && data[end] != 0) ++end;
            std::string text(reinterpret_cast<const char*>(data.data() + pos), end - pos);
            pos = end + 1;
            // Speaker tags in subtitles: "civ&idCIVILIAN<U+E900>text" -> "text".
            const size_t tag = text.find("&id");
            if (tag != std::string::npos) {
                const size_t mark = text.find("\xEE\xA4\x80", tag);
                if (mark != std::string::npos) text = text.substr(mark + 3);
            }
            language.emplace(key, Utf8ToWideString(text));  // first string wins for a key
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_language = std::move(language);
        NOBLE_INFO("StringTable: loaded {} strings from {}", m_language.size(), file.string());
        return true;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_registered.clear();
    }

    void Register(uint32_t tableId, uint32_t stringId, const char* name, const char* suffix) {
        if (!name || !suffix) return;
        char nameBuf[256] = {};
        char suffixBuf[256] = {};
        SafeCopyAnsiRaw(name, nameBuf, sizeof(nameBuf));
        SafeCopyAnsiRaw(suffix, suffixBuf, sizeof(suffixBuf));
        std::string full = std::string(nameBuf) + "_" + suffixBuf;
        const uint32_t key = KeyFor(full);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_registered[MakeKey(tableId, stringId)] = key;
        if (m_loggedRegistrations < 40) {
            ++m_loggedRegistrations;
            const auto it = m_language.find(key);
            NOBLE_INFO("StringTable: register 0x{:08X}:0x{:X} '{}' -> key 0x{:08X} ({})", tableId, stringId, full, key,
                       it != m_language.end() ? "found" : "not in language file");
        }
    }

    // Copies the text into out (capacity in characters, including the terminator).
    bool Lookup(uint32_t tableId, uint32_t stringId, wchar_t* out, size_t capacity) {
        std::wstring text;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const uint64_t id = MakeKey(tableId, stringId);
            bool found = false;
            auto reg = m_registered.find(id);
            if (reg != m_registered.end()) {
                auto it = m_language.find(reg->second);
                if (it != m_language.end()) {
                    text = it->second;
                    found = true;
                }
            }
            if (!found) {
                auto fb = m_fallback.find(id);
                if (fb == m_fallback.end()) {
                    ++m_misses;
                    return false;
                }
                text = fb->second;
            }
            ++m_hits;
        }
        if (!out || capacity == 0 || text.size() >= capacity) return false;
        return SafeCopyMemory(out, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
    }

    void LogStats() {
        std::lock_guard<std::mutex> lock(m_mutex);
        NOBLE_INFO("StringTable: {} registered, {} lookups answered, {} not found", m_registered.size(), m_hits, m_misses);
    }

private:
    static uint64_t MakeKey(uint32_t tableId, uint32_t stringId) {
        return (static_cast<uint64_t>(tableId) << 32) | static_cast<uint64_t>(stringId);
    }

    // CRC-32/MPEG-2 of the upper-cased name, skipping a leading '$' (RR strings::register).
    static uint32_t KeyFor(const std::string& name) {
        static const std::array<uint32_t, 256> table = [] {
            std::array<uint32_t, 256> t{};
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i << 24;
                for (int b = 0; b < 8; ++b) c = (c & 0x80000000u) ? (c << 1) ^ 0x04C11DB7u : (c << 1);
                t[i] = c;
            }
            return t;
        }();
        size_t start = (!name.empty() && name[0] == '$') ? 1 : 0;
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = start; i < name.size(); ++i) {
            uint8_t c = static_cast<uint8_t>(name[i]);
            if (c >= 'a' && c <= 'z') c = static_cast<uint8_t>(c - 0x20);
            crc = (crc << 8) ^ table[((crc >> 24) ^ c) & 0xFF];
        }
        return crc;
    }

    bool m_initialized = false;
    int m_loggedRegistrations = 0;
    uint64_t m_hits = 0;
    uint64_t m_misses = 0;
    std::mutex m_mutex;
    std::unordered_map<uint32_t, std::wstring> m_language;    // key -> text (MCC language file)
    std::unordered_map<uint64_t, uint32_t> m_registered;      // (table, id) -> key (slot 56)
    std::unordered_map<uint64_t, std::wstring> m_fallback;    // Noble's built-in strings
};

// Slot 55: strings clear (RR host::hit case 0x37)
static uint64_t __fastcall HostSlot55_StringsClear(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a1; (void)a2; (void)a3; (void)a4;
    StringTable::Get().Clear();
    // Must return non-zero: the engine (haloreach+0x3140D) only registers its string tables through slot 56
    // when slot 55 returns true ("test al,al / je skip"). Returning 0 left every slot 57 lookup unanswered, so
    // string-id fields (score, place, respawn time, loadout grenades, Forge menus) showed the raw id in hex.
    return 1;
}

// Slot 56: strings register (case 0x38): (table, id, const char* name, const char* suffix)
static uint64_t __fastcall HostSlot56_StringsRegister(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr;
    if (a3 > 0x10000 && a4 > 0x10000) {
        StringTable::Get().Register(static_cast<uint32_t>(a1), static_cast<uint32_t>(a2),
                                    reinterpret_cast<const char*>(a3), reinterpret_cast<const char*>(a4));
    }
    return 0;
}

// Slot 57: string lookup (case 0x39): (table, id, wchar_t* out, size_t capacity) -> 1 if written
static uint64_t __fastcall HostSlot57_StringLookup(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr;
    wchar_t* out = a3 > 0x10000 ? reinterpret_cast<wchar_t*>(a3) : nullptr;
    const size_t capacity = static_cast<size_t>(a4 > 0x100000 ? 0 : a4);
    const bool ok = StringTable::Get().Lookup(static_cast<uint32_t>(a1), static_cast<uint32_t>(a2), out, capacity);
    static std::atomic<int> s_logged{0};
    if (s_logged.fetch_add(1) < 25) {
        NOBLE_INFO("ReachHost [Slot 57 - StringLookup]: table=0x{:08X} id=0x{:X} cap={} -> {}",
                   static_cast<uint32_t>(a1), static_cast<uint32_t>(a2), capacity,
                   ok && out ? NarrowForLog(SafeReadWide(out, 60)) : std::string("(none)"));
    }
    return ok ? 1 : 0;
}


// -----------------------------------------------------------------------------
// Text host slots (61-67). In MCC's PC build the host owns font rendering: Reach Renaissance
// installs text::{prepare, glyph, atlas, has_icon, kerning, register_icon, metrics} at
// slots 0x3D-0x43 (host::new_host @ 0x1400828E0). Signatures below come from those functions'
// register/stack usage. This is a first, minimal implementation written for Noble:
//   61 prepare(host, const wchar_t* text, u32 style, float size, const char* face) -> 1
//   67 metrics(host, u32 style, float size, const char* face, u16* out0, u16* out1) -> 1 if font
// Glyph/atlas rendering (62/63) is not implemented yet.
// -----------------------------------------------------------------------------
// SEH-only copy helpers (no C++ objects in these frames).
static size_t SafeCopyAnsiRaw(const char* src, char* dst, size_t cap) {
    if (!src || !dst || cap == 0) return 0;
    size_t n = 0;
    __try {
        while (n + 1 < cap && src[n]) { dst[n] = src[n]; ++n; }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    dst[n] = 0;
    return n;
}

static size_t SafeCopyWideRaw(const wchar_t* src, wchar_t* dst, size_t cap) {
    if (!src || !dst || cap == 0) return 0;
    size_t n = 0;
    __try {
        while (n + 1 < cap && src[n]) { dst[n] = src[n]; ++n; }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    dst[n] = 0;
    return n;
}

static bool SafeWriteU16(uint16_t* dst, uint16_t value) {
    if (reinterpret_cast<uintptr_t>(dst) <= 0x10000) return false;
    __try {
        *dst = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static std::wstring WidenFace(const char* face) {
    char buf[64];
    const size_t n = SafeCopyAnsiRaw(face, buf, sizeof(buf));
    std::wstring w;
    for (size_t i = 0; i < n; ++i) w.push_back(static_cast<wchar_t>(static_cast<unsigned char>(buf[i])));
    return w;
}

static std::wstring MapReachFace(const std::wstring& requested) {
    if (requested.empty()) return L"Bahnschrift";
    std::wstring lower = requested;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    
    // Fixedsys is queried by haloreach.dll for HUD ammo / bullet counter digits.
    // Windows legacy Fixedsys is a .fon bitmap font which fails GetGlyphOutlineW.
    // Consolas provides monospaced TrueType vector glyphs matching Reach's ammo display.
    if (lower.find(L"fixedsys") != std::wstring::npos) return L"Consolas";
    if (lower.find(L"tvnord") != std::wstring::npos) return L"Bahnschrift Condensed";
    if (lower.find(L"eurostile") != std::wstring::npos) return L"Bahnschrift";
    if (lower.find(L"bankgothic") != std::wstring::npos) return L"Bahnschrift";
    return requested;
}

static uint16_t HashFace(const char* face) {
    if (!face) return 0;
    uint16_t h = 0;
    while (*face) {
        char c = *face++;
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
        h = (h * 31) + static_cast<uint8_t>(c);
    }
    return h;
}

static inline uint64_t MakeGlyphKey(wchar_t ch, uint16_t faceId, uint32_t style, uint32_t scaleKey) {
    return (static_cast<uint64_t>(static_cast<uint16_t>(ch)) << 48) |
           (static_cast<uint64_t>(faceId) << 32) |
           (static_cast<uint64_t>(style & 0xFFFF) << 16) |
           (scaleKey & 0xFFFF);
}

struct HostFontMetrics {
    uint16_t ascent = 0;
    uint16_t descent = 0;
    bool ok = false;
};

static HostFontMetrics MeasureFont(const std::wstring& face, uint32_t pointSize, float scale, int weight = FW_NORMAL) {
    HostFontMetrics m;
    // RR's Atlas::font: pixels = round(size * scale * SIZE_SCALE); the engine passes size as an
    // integer (9, 13, 15, 18, 24, 30, 42 seen) and scale as a float (1.0 seen).
    int pixelHeight = static_cast<int>(static_cast<float>(pointSize) * scale + 0.5f);
    if (pixelHeight < 1) pixelHeight = 1;
    if (pixelHeight > 512) pixelHeight = 512;

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return m;
    for (int attempt = 0; attempt < 2 && !m.ok; ++attempt) {
        const wchar_t* name = attempt == 0 ? face.c_str() : L"Segoe UI";
        HFONT font = CreateFontW(-pixelHeight, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, name);
        if (!font) continue;
        HGDIOBJ old = SelectObject(dc, font);
        TEXTMETRICW tm{};
        if (GetTextMetricsW(dc, &tm)) {
            m.ascent = static_cast<uint16_t>(tm.tmAscent);
            m.descent = static_cast<uint16_t>(tm.tmDescent);
            m.ok = true;
        }
        SelectObject(dc, old);
        DeleteObject(font);
    }
    DeleteDC(dc);
    return m;
}

static std::wstring SafeReadWide(const wchar_t* text, size_t maxChars) {
    wchar_t buf[128];
    if (maxChars > 127) maxChars = 127;
    SafeCopyWideRaw(text, buf, maxChars + 1);
    return std::wstring(buf);
}

static uint64_t __fastcall HostSlot61_TextPrepare(void* thisPtr, const wchar_t* text, uint32_t style, float size, const char* face) {
    (void)thisPtr;
    static std::atomic<int> s_calls{0};
    if (s_calls.fetch_add(1) < 20) {
        NOBLE_INFO("ReachHost [Slot 61 - TextPrepare]: size={} scale={:.2f} face='{}' text='{}'",
            style, size, NarrowForLog(WidenFace(face)), NarrowForLog(SafeReadWide(text, 48)));
    }
    return 1;
}

// -----------------------------------------------------------------------------
// Font Glyph and Atlas Subsystem (Slots 62-66). Matches Reach Renaissance text::{glyph, atlas, has_icon, kerning, register_icon}.
// haloreach.dll queries glyph data (Slot 62) and binds the font texture atlas SRV (Slot 63)
// to render in-game HUD numbers, weapon bullet counts, clip reserves, and scoreboard text.
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
// Glyph record returned to the engine by slot 62 (26 bytes). Layout checked against RR's
// text::Atlas::glyph (host-probe 0x1402869c3..0x140286a0f) and text::register_icon:
// the atlas X is the top 16 bits of a u64 stored at +0x02, i.e. it lands at +0x08, and the
// glyph origin is stored y first, then x.
struct HostGlyphData {
    uint16_t ch;        // +0x00 character code
    uint16_t pad[3];    // +0x02 .. +0x07 zero (low 48 bits of the u64 x << 48)
    uint16_t atlasX;    // +0x08 X in the atlas page (pixels)
    uint16_t atlasY;    // +0x0A Y in the atlas page (pixels)
    uint16_t width;     // +0x0C gmBlackBoxX (0 for empty glyphs such as space)
    uint16_t height;    // +0x0E gmBlackBoxY
    int16_t  originY;   // +0x10 gmptGlyphOrigin.y (icons: "top" argument)
    int16_t  originX;   // +0x12 gmptGlyphOrigin.x (icons: 0)
    int16_t  advanceX;  // +0x14 gmCellIncX (icons: width)
    int16_t  ascent;    // +0x16 -tmAscent (icons: 0)
    uint8_t  page;      // +0x18 atlas page index (slot 63 argument)
    uint8_t  reserved;  // +0x19 0
};
#pragma pack(pop)
static_assert(sizeof(HostGlyphData) == 26, "HostGlyphData must be exactly 26 bytes");
static_assert(offsetof(HostGlyphData, atlasX) == 0x08, "atlas X must be at +0x08");
static_assert(offsetof(HostGlyphData, originY) == 0x10, "origin y must be at +0x10");
static_assert(offsetof(HostGlyphData, page) == 0x18, "page must be at +0x18");

static bool FaceIsBold(const std::wstring& face) {
    std::wstring lower = face;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return lower.find(L"bold") != std::wstring::npos;
}

// Text atlas shared by the text slots (62-66). Behaviour follows RR's text::Atlas:
//  * glyphs are rasterised with GDI (GGO_GRAY8_BITMAP) into 1024x1024 RGBA pages, white with
//    coverage in alpha; when a page is full a new page is started (RR keeps up to 16), so old
//    glyphs are never overwritten (overwriting them garbled long-running HUD text);
//  * icons registered by the engine (slot 66) are copied unchanged and keyed by
//    (face, size, scale, char), like glyphs;
//  * slot 64 only reports icons that were really registered, so the engine registers each icon
//    before using it;
//  * glyph/icon calls only touch CPU memory; the GPU textures are created and updated when the
//    engine asks for a page's SRV (slot 63), i.e. on the thread that renders.
class TextAtlas {
public:
    static constexpr uint32_t kPageSize = 1024;
    static constexpr uint32_t kMaxPages = 16;

    static TextAtlas& Get() {
        static TextAtlas instance;
        return instance;
    }

    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_device == device && m_context == context) return;
        m_device = device;
        m_context = context;
        // New device: GPU copies are recreated (from the CPU pixels) on the next SRV request.
        for (auto& page : m_pages) {
            page->texture.Reset();
            page->srv.Reset();
            page->MarkAllDirty();
        }
        if (!m_loggedInit) {
            m_loggedInit = true;
            NOBLE_INFO("TextAtlas: ready ({}x{} pages, up to {}).", kPageSize, kPageSize, kMaxPages);
        }
    }

    // Slot 63. Called by the engine's render code when it binds the font texture.
    ID3D11ShaderResourceView* GetSRV(uint32_t pageIndex) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (pageIndex >= m_pages.size() || !m_device || !m_context) return nullptr;
        Page& page = *m_pages[pageIndex];
        if (!page.texture) {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = kPageSize;
            desc.Height = kPageSize;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA init{};
            init.pSysMem = page.pixels.data();
            init.SysMemPitch = kPageSize * sizeof(uint32_t);
            if (FAILED(m_device->CreateTexture2D(&desc, &init, page.texture.GetAddressOf())) ||
                FAILED(m_device->CreateShaderResourceView(page.texture.Get(), nullptr, page.srv.GetAddressOf()))) {
                page.texture.Reset();
                page.srv.Reset();
                return nullptr;
            }
            page.ClearDirty();
        } else if (page.dirty) {
            D3D11_BOX box{};
            box.left = page.dirtyLeft;
            box.top = page.dirtyTop;
            box.right = page.dirtyRight;
            box.bottom = page.dirtyBottom;
            box.front = 0;
            box.back = 1;
            const uint32_t* src = page.pixels.data() + page.dirtyTop * kPageSize + page.dirtyLeft;
            m_context->UpdateSubresource(page.texture.Get(), 0, &box, src, kPageSize * sizeof(uint32_t), 0);
            page.ClearDirty();
        }
        return page.srv.Get();
    }

    // Slot 62.
    bool GetGlyph(wchar_t ch, uint32_t style, float scale, const char* face, HostGlyphData* outGlyph) {
        if (!outGlyph) return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        const uint64_t key = MakeGlyphKey(ch, HashFace(face), style, ScaleKey(scale));
        auto it = m_cache.find(key);
        if (it == m_cache.end()) {
            HostGlyphData data{};
            const bool ok = RasterizeGlyphLocked(ch, style, scale, face, &data);
            if (ch >= 0x2000 || ch < 0x20) {
                static std::atomic<int> s_logged{0};
                if (s_logged.fetch_add(1) < 60) {
                    NOBLE_INFO("TextAtlas: glyph U+{:04X} size={} scale={:.2f} face='{}' -> {} ({}x{})",
                               static_cast<uint16_t>(ch), style, scale, NarrowForLog(WidenFace(face)),
                               ok ? "rasterised" : "no glyph", data.width, data.height);
                }
            }
            if (!ok) return false;
            it = m_cache.emplace(key, data).first;
        }
        return SafeCopyMemory(outGlyph, &it->second, sizeof(HostGlyphData));
    }

    // Slot 66. Pixels are 32-bit RGBA rows (pitch in bytes), copied as they are.
    bool RegisterIcon(wchar_t ch, uint32_t style, float scale, const char* face,
                      int32_t width, int32_t height, int32_t top,
                      const void* pixels, int32_t pitch) {
        if (width <= 0 || height <= 0 || !pixels || pitch < width * 4) return false;
        if (width > static_cast<int32_t>(kPageSize) - 2 || height > static_cast<int32_t>(kPageSize) - 2) return false;

        std::lock_guard<std::mutex> lock(m_mutex);
        const uint32_t w = static_cast<uint32_t>(width);
        const uint32_t h = static_cast<uint32_t>(height);
        uint32_t pageIndex = 0, gx = 0, gy = 0;
        if (!PlaceLocked(w, h, pageIndex, gx, gy)) return false;

        Page& page = *m_pages[pageIndex];
        const uint8_t* src = static_cast<const uint8_t*>(pixels);
        for (uint32_t y = 0; y < h; ++y) {
            if (!SafeCopyMemory(&page.pixels[(gy + y) * kPageSize + gx], src + static_cast<size_t>(y) * pitch, w * 4)) {
                return false;
            }
        }
        page.MarkDirty(gx, gy, w, h);

        HostGlyphData data{};
        data.ch = static_cast<uint16_t>(ch);
        data.atlasX = static_cast<uint16_t>(gx);
        data.atlasY = static_cast<uint16_t>(gy);
        data.width = static_cast<uint16_t>(w);
        data.height = static_cast<uint16_t>(h);
        data.originY = static_cast<int16_t>(top);
        data.originX = 0;
        data.advanceX = static_cast<int16_t>(w);
        data.ascent = 0;
        data.page = static_cast<uint8_t>(pageIndex);

        const uint64_t key = MakeGlyphKey(ch, HashFace(face), style, ScaleKey(scale));
        m_cache[key] = data;
        m_icons.insert(key);

        static std::atomic<int> s_logged{0};
        if (s_logged.fetch_add(1) < 40) {
            NOBLE_INFO("TextAtlas: icon ch=0x{:04X} {}x{} top={} size={} scale={:.2f} face='{}' -> page {} ({},{})",
                       static_cast<uint16_t>(ch), w, h, top, style, scale,
                       NarrowForLog(WidenFace(face)), pageIndex, gx, gy);
        }
        return true;
    }

    // Slot 64: true only for icons registered through slot 66 with the same font key.
    bool HasIcon(uint64_t ch, uint32_t style, float scale, const char* face) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const uint64_t key = MakeGlyphKey(static_cast<wchar_t>(ch & 0xFFFF), HashFace(face), style, ScaleKey(scale));
        const bool found = m_icons.find(key) != m_icons.end();
        static std::atomic<int> s_logged{0};
        if (!found && s_logged.fetch_add(1) < 40) {
            NOBLE_INFO("TextAtlas: has_icon U+{:04X} size={} scale={:.2f} face='{}' -> no (engine should register it)",
                       static_cast<uint16_t>(ch & 0xFFFF), style, scale, NarrowForLog(WidenFace(face)));
        }
        return found;
    }

private:
    struct Page {
        std::vector<uint32_t> pixels = std::vector<uint32_t>(kPageSize * kPageSize, 0);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        uint32_t cursorX = 1, cursorY = 1, rowHeight = 0;
        bool dirty = false;
        uint32_t dirtyLeft = 0, dirtyTop = 0, dirtyRight = 0, dirtyBottom = 0;

        void MarkDirty(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
            if (w == 0 || h == 0) return;
            if (!dirty) {
                dirty = true;
                dirtyLeft = x; dirtyTop = y; dirtyRight = x + w; dirtyBottom = y + h;
            } else {
                dirtyLeft = std::min(dirtyLeft, x);
                dirtyTop = std::min(dirtyTop, y);
                dirtyRight = std::max(dirtyRight, x + w);
                dirtyBottom = std::max(dirtyBottom, y + h);
            }
        }
        void MarkAllDirty() { dirty = true; dirtyLeft = 0; dirtyTop = 0; dirtyRight = kPageSize; dirtyBottom = kPageSize; }
        void ClearDirty() { dirty = false; }
    };

    static uint32_t ScaleKey(float scale) {
        return static_cast<uint32_t>(scale * 1000.0f + 0.5f);
    }

    // Shelf packing with a 1-pixel gap; starts a new page when the current one is full.
    bool PlaceLocked(uint32_t w, uint32_t h, uint32_t& pageIndex, uint32_t& x, uint32_t& y) {
        if (m_pages.empty()) m_pages.push_back(std::make_unique<Page>());
        for (int attempt = 0; attempt < 2; ++attempt) {
            Page& page = *m_pages.back();
            if (page.cursorX + w + 1 > kPageSize) {
                page.cursorX = 1;
                page.cursorY += page.rowHeight + 1;
                page.rowHeight = 0;
            }
            if (page.cursorY + h + 1 <= kPageSize) {
                pageIndex = static_cast<uint32_t>(m_pages.size() - 1);
                x = page.cursorX;
                y = page.cursorY;
                page.cursorX += w + 1;
                page.rowHeight = std::max(page.rowHeight, h);
                return true;
            }
            if (m_pages.size() >= kMaxPages) {
                static std::atomic<bool> s_warned{false};
                if (!s_warned.exchange(true)) {
                    NOBLE_WARN("TextAtlas: all {} pages are full; new glyphs are dropped.", kMaxPages);
                }
                return false;
            }
            m_pages.push_back(std::make_unique<Page>());
            NOBLE_INFO("TextAtlas: started page {}", m_pages.size() - 1);
        }
        return false;
    }

    bool RasterizeGlyphLocked(wchar_t ch, uint32_t style, float scale, const char* face, HostGlyphData* outGlyph) {
        const std::wstring requestedFace = WidenFace(face);
        const std::wstring mappedFace = MapReachFace(requestedFace);
        const int weight = FaceIsBold(requestedFace) ? FW_BOLD : FW_NORMAL;

        int pixelHeight = static_cast<int>(static_cast<float>(style) * scale + 0.5f);
        pixelHeight = std::clamp(pixelHeight, 1, 256);

        HDC hdc = CreateCompatibleDC(nullptr);
        if (!hdc) return false;
        HFONT font = CreateFontW(-pixelHeight, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, mappedFace.c_str());
        if (!font) {
            font = CreateFontW(-pixelHeight, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        }
        if (!font) {
            DeleteDC(hdc);
            return false;
        }
        HGDIOBJ oldFont = SelectObject(hdc, font);
        TEXTMETRICW tm{};
        GetTextMetricsW(hdc, &tm);

        GLYPHMETRICS gm{};
        const MAT2 identity = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
        std::vector<uint8_t> gray;
        const DWORD size = GetGlyphOutlineW(hdc, ch, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &identity);
        bool ok = true;
        if (size == GDI_ERROR) {
            ok = false;
        } else if (size > 0) {
            gray.resize(size);
            if (GetGlyphOutlineW(hdc, ch, GGO_GRAY8_BITMAP, &gm, size, gray.data(), &identity) == GDI_ERROR) ok = false;
        } else {
            // Empty outline (space): no bitmap, keep the advance.
            gm.gmBlackBoxX = 0;
            gm.gmBlackBoxY = 0;
        }
        SelectObject(hdc, oldFont);
        DeleteObject(font);
        DeleteDC(hdc);
        if (!ok) return false;

        const uint32_t w = gray.empty() ? 0 : gm.gmBlackBoxX;
        const uint32_t h = gray.empty() ? 0 : gm.gmBlackBoxY;
        uint32_t pageIndex = m_pages.empty() ? 0 : static_cast<uint32_t>(m_pages.size() - 1);
        uint32_t gx = 0, gy = 0;
        if (w > 0 && h > 0) {
            if (!PlaceLocked(w, h, pageIndex, gx, gy)) return false;
            Page& page = *m_pages[pageIndex];
            const uint32_t pitch = (w + 3) & ~3u;  // GGO_GRAY8 rows are DWORD aligned
            for (uint32_t y = 0; y < h; ++y) {
                for (uint32_t x = 0; x < w; ++x) {
                    const size_t i = static_cast<size_t>(y) * pitch + x;
                    const uint32_t g = i < gray.size() ? gray[i] : 0;  // 0..64
                    const uint32_t alpha = std::min<uint32_t>((g * 255 + 32) >> 6, 255);
                    page.pixels[(gy + y) * kPageSize + gx + x] = (alpha << 24) | 0x00FFFFFF;
                }
            }
            page.MarkDirty(gx, gy, w, h);
        }

        HostGlyphData data{};
        data.ch = static_cast<uint16_t>(ch);
        data.atlasX = static_cast<uint16_t>(gx);
        data.atlasY = static_cast<uint16_t>(gy);
        data.width = static_cast<uint16_t>(w);
        data.height = static_cast<uint16_t>(h);
        data.originY = static_cast<int16_t>(gm.gmptGlyphOrigin.y);
        data.originX = static_cast<int16_t>(gm.gmptGlyphOrigin.x);
        data.advanceX = static_cast<int16_t>(gm.gmCellIncX);
        data.ascent = static_cast<int16_t>(-tm.tmAscent);
        data.page = static_cast<uint8_t>(pageIndex);
        *outGlyph = data;
        return true;
    }

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    bool m_loggedInit = false;
    std::vector<std::unique_ptr<Page>> m_pages;
    std::unordered_map<uint64_t, HostGlyphData> m_cache;  // glyphs and icons, by (char, face, size, scale)
    std::unordered_set<uint64_t> m_icons;                 // keys registered through slot 66
    std::mutex m_mutex;
};

// Slot 62: Text Glyph handler (host_probe case 0x3E)
static uint64_t __fastcall HostSlot62_TextGlyph(void* thisPtr, uint64_t ch, void* outGlyph, uint32_t style, float scale, const char* face) {
    (void)thisPtr;
    if (outGlyph && reinterpret_cast<uintptr_t>(outGlyph) > 0x10000) {
        if (TextAtlas::Get().GetGlyph(static_cast<wchar_t>(ch & 0xFFFF), style, scale, face, reinterpret_cast<HostGlyphData*>(outGlyph))) {
            return 1;
        }
    }
    return 0;
}

// Slot 63: Text Atlas handler (host_probe case 0x3F)
static uint64_t __fastcall HostSlot63_TextAtlas(void* thisPtr, uint32_t pageIndex) {
    (void)thisPtr;
    return reinterpret_cast<uint64_t>(TextAtlas::Get().GetSRV(pageIndex));
}

// Slot 64: Text Has Icon (host_probe case 0x40)
static uint64_t __fastcall HostSlot64_TextHasIcon(void* thisPtr, uint64_t ch, uint32_t style, float scale, const char* face) {
    (void)thisPtr;
    return TextAtlas::Get().HasIcon(ch, style, scale, face) ? 1 : 0;
}

// Slot 65: Text Kerning (host_probe case 0x41)
static uint64_t __fastcall HostSlot65_TextKerning(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a1; (void)a2; (void)a3; (void)a4;
    return 0;
}

// Slot 66: Text Register Icon (host_probe case 0x42)
static uint64_t __fastcall HostSlot66_TextRegisterIcon(void* thisPtr, uint64_t ch, uint32_t style, float scale,
                                                      const char* face, int32_t width, int32_t height,
                                                      int32_t advanceX, const void* pixels, int32_t pitch) {
    (void)thisPtr;
    return TextAtlas::Get().RegisterIcon(static_cast<wchar_t>(ch & 0xFFFF), style, scale, face,
                                         width, height, advanceX, pixels, pitch) ? 1 : 0;
}


// -----------------------------------------------------------------------------
// Gamma post-process pass (fixes dark/purple HUD icons, crosshair, radar, and shield bar)
// Reach Renaissance executes a post-process gamma pass in Host Slot 1 before Present.
// Halo Reach outputs linear HDR backbuffer; without gamma correction, HUD icons and UI
// elements appear dim, dark navy, or purplish.
// -----------------------------------------------------------------------------
// GammaPass: Full-screen post-processing for authentic brightness / gamma adjustments.
// Note: Armory 3D character scene renders natively to preserve full fidelity
// of dark undersuits, skin suits, and emissive particle armor effects.
// -----------------------------------------------------------------------------
// Must match the HLSL cbuffer below exactly: gamma_exp, fxaa, padding (float2), screen_size (float2), padding2.
struct GammaCB {
    float gamma_exp;    // pow() exponent: 1.0 = untouched, < 1 = brighter
    float fxaa;         // 0 = off, 1 = post-process anti-aliasing (Noble's own pass)
    float padding[2];
    float screen_w;     // the FXAA taps need the frame size
    float screen_h;
    float padding2[2];
};

class GammaPass {
public:
    static GammaPass& Get() {
        static GammaPass instance;
        return instance;
    }

    void SetExponent(float exp) {
        m_exponent = exp;
    }

    void OnResize() {
        m_initialized = false;
        m_copyTexture.Reset();
        m_copySRV.Reset();
        m_backBufferRTV.Reset();
    }

    void Execute(IDXGISwapChain* swapChain) {
        if (!swapChain) return;

        Microsoft::WRL::ComPtr<ID3D11Device> device;
        if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(device.GetAddressOf())))) return;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())))) return;

        D3D11_TEXTURE2D_DESC desc{};
        backBuffer->GetDesc(&desc);

        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        device->GetImmediateContext(context.GetAddressOf());
        if (!context) return;

        // Lazy initialize TextAtlas with device and context if not already done
        TextAtlas::Get().Initialize(device.Get(), context.Get());

        // BRIGHTNESS and ANTI-ALIASING from SETTINGS > GRAPHICS. Brightness is the post-process exponent
        // (1/brightness, so a bigger number brightens); 1.0 and FXAA off is the identity and costs nothing.
        {
            static ULONGLONG s_nextRead = 0;
            static float s_brightness = 1.0f;
            static bool s_fxaa = false;
            const ULONGLONG now = GetTickCount64();
            if (now >= s_nextRead) {
                s_nextRead = now + 500;
                const SettingsData d = Settings::Get().Snapshot();
                s_brightness = std::clamp(d.brightness, 0.5f, 2.0f);
                s_fxaa = d.fxaa;
            }
            m_exponent = 1.0f / s_brightness;
            m_fxaa = s_fxaa ? 1.0f : 0.0f;
        }
        if (std::abs(m_exponent - 1.0f) < 0.001f && m_fxaa < 0.5f) {
            return;
        }

        if (!m_initialized || m_width != desc.Width || m_height != desc.Height) {
            if (!InitResources(device.Get(), backBuffer.Get(), desc)) {
                return;
            }
        }

        // Copy current frame backbuffer to our shader resource texture
        context->CopyResource(m_copyTexture.Get(), backBuffer.Get());

        // Save pipeline state
        ID3D11RenderTargetView* oldRTV = nullptr;
        ID3D11DepthStencilView* oldDSV = nullptr;
        context->OMGetRenderTargets(1, &oldRTV, &oldDSV);

        UINT numViewports = 1;
        D3D11_VIEWPORT oldViewport{};
        context->RSGetViewports(&numViewports, &oldViewport);

        ID3D11VertexShader* oldVS = nullptr;
        ID3D11PixelShader* oldPS = nullptr;
        ID3D11ClassInstance* oldInstances[4] = { nullptr };
        UINT numInstances = 0;
        context->VSGetShader(&oldVS, oldInstances, &numInstances);
        context->PSGetShader(&oldPS, oldInstances, &numInstances);

        ID3D11ShaderResourceView* oldSRV = nullptr;
        context->PSGetShaderResources(0, 1, &oldSRV);

        ID3D11SamplerState* oldSampler = nullptr;
        context->PSGetSamplers(0, 1, &oldSampler);
        ID3D11SamplerState* oldSampler1 = nullptr;
        context->PSGetSamplers(1, 1, &oldSampler1);

        ID3D11Buffer* oldCB = nullptr;
        context->PSGetConstantBuffers(0, 1, &oldCB);

        D3D11_PRIMITIVE_TOPOLOGY oldTopology;
        context->IAGetPrimitiveTopology(&oldTopology);

        ID3D11InputLayout* oldLayout = nullptr;
        context->IAGetInputLayout(&oldLayout);

        ID3D11BlendState* oldBlend = nullptr;
        float oldBlendFactor[4] = { 0 };
        UINT oldSampleMask = 0;
        context->OMGetBlendState(&oldBlend, oldBlendFactor, &oldSampleMask);

        ID3D11DepthStencilState* oldDepthState = nullptr;
        UINT oldStencilRef = 0;
        context->OMGetDepthStencilState(&oldDepthState, &oldStencilRef);

        // Update constant buffer with gamma exponent
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            GammaCB* cbData = static_cast<GammaCB*>(mapped.pData);
            cbData->gamma_exp = m_exponent;
            cbData->fxaa = m_fxaa;
            cbData->padding[0] = cbData->padding[1] = 0.0f;
            cbData->screen_w = static_cast<float>(desc.Width);
            cbData->screen_h = static_cast<float>(desc.Height);
            cbData->padding2[0] = cbData->padding2[1] = 0.0f;
            context->Unmap(m_constantBuffer.Get(), 0);
        }

        // Bind gamma pass state
        D3D11_VIEWPORT vp{};
        vp.Width = static_cast<float>(desc.Width);
        vp.Height = static_cast<float>(desc.Height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);

        ID3D11RenderTargetView* rtv = m_backBufferRTV.Get();
        context->OMSetRenderTargets(1, &rtv, nullptr);
        context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        context->OMSetDepthStencilState(nullptr, 0);

        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        context->VSSetShader(m_vs.Get(), nullptr, 0);
        context->PSSetShader(m_ps.Get(), nullptr, 0);

        ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
        context->PSSetConstantBuffers(0, 1, cbs);

        ID3D11ShaderResourceView* srvs[] = { m_copySRV.Get() };
        context->PSSetShaderResources(0, 1, srvs);
        ID3D11SamplerState* frameSamplers[] = { m_frameSampler.Get() };
        context->PSSetSamplers(1, 1, frameSamplers);

        // Draw full-screen triangle
        context->Draw(3, 0);

        // Restore pipeline state
        ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
        context->PSSetShaderResources(0, 1, nullSRVs);
        ID3D11SamplerState* nullSampler = nullptr;
        context->PSSetSamplers(1, 1, &nullSampler);

        context->OMSetRenderTargets(1, &oldRTV, oldDSV);
        if (oldRTV) oldRTV->Release();
        if (oldDSV) oldDSV->Release();

        context->RSSetViewports(numViewports, &oldViewport);

        context->VSSetShader(oldVS, nullptr, 0);
        if (oldVS) oldVS->Release();
        context->PSSetShader(oldPS, nullptr, 0);
        if (oldPS) oldPS->Release();

        context->PSSetShaderResources(0, 1, &oldSRV);
        if (oldSRV) oldSRV->Release();

        context->PSSetSamplers(0, 1, &oldSampler);
        if (oldSampler) oldSampler->Release();
        context->PSSetSamplers(1, 1, &oldSampler1);
        if (oldSampler1) oldSampler1->Release();

        context->PSSetConstantBuffers(0, 1, &oldCB);
        if (oldCB) oldCB->Release();

        context->IASetInputLayout(oldLayout);
        if (oldLayout) oldLayout->Release();

        context->IASetPrimitiveTopology(oldTopology);

        context->OMSetBlendState(oldBlend, oldBlendFactor, oldSampleMask);
        if (oldBlend) oldBlend->Release();

        context->OMSetDepthStencilState(oldDepthState, oldStencilRef);
        if (oldDepthState) oldDepthState->Release();
    }

private:
    bool InitResources(ID3D11Device* device, ID3D11Texture2D* backBuffer, const D3D11_TEXTURE2D_DESC& desc) {
        m_width = desc.Width;
        m_height = desc.Height;

        if (!m_vs) {
            static const char* s_gammaShaderCode = R"(
                Texture2D frame_copy : register(t0);

                cbuffer GammaParams : register(b0) {
                    float gamma_exp;
                    float fxaa;
                    float2 padding;
                    float2 screen_size;
                    float2 padding2;
                };

                float4 vs_main(uint id : SV_VertexID) : SV_Position {
                    float2 p = float2((id << 1) & 2, id & 2);
                    return float4(p * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
                }

                SamplerState frame_sampler : register(s1);

                float lumaOf(float3 c) { return dot(c, float3(0.299f, 0.587f, 0.114f)); }

                // Compact FXAA: luma-edge detection, blend along the gradient direction. Runs before the brightness
                // curve so the edge tests work on the frame the engine actually drew.
                float3 Fxaa(float2 uv, float2 texel) {
                    float3 n = frame_copy.Sample(frame_sampler, uv + float2(0.0f, -texel.y)).rgb;
                    float3 s = frame_copy.Sample(frame_sampler, uv + float2(0.0f, texel.y)).rgb;
                    float3 e = frame_copy.Sample(frame_sampler, uv + float2(texel.x, 0.0f)).rgb;
                    float3 w = frame_copy.Sample(frame_sampler, uv + float2(-texel.x, 0.0f)).rgb;
                    float lumaM = lumaOf(frame_copy.Load(int3(uv * screen_size, 0)).rgb);
                    float lumaN = lumaOf(n), lumaS = lumaOf(s), lumaE = lumaOf(e), lumaW = lumaOf(w);
                    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaE, lumaW)));
                    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaE, lumaW)));
                    if ((lumaMax - lumaMin) < max(0.0312f, lumaMax * 0.125f)) {
                        return frame_copy.Load(int3(uv * screen_size, 0)).rgb;   // no edge: leave it alone
                    }
                    float2 dir;
                    dir.x = -((lumaN + lumaS) - 2.0f * lumaM);
                    dir.y = ((lumaE + lumaW) - 2.0f * lumaM);
                    const float reduce = max((lumaN + lumaS + lumaE + lumaW) * 0.03125f, 0.0078125f);
                    const float rcpMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + reduce);
                    dir = clamp(dir * rcpMin, -8.0f, 8.0f) * texel;
                    float3 rgbA = 0.5f * (frame_copy.Sample(frame_sampler, uv + dir * (1.0f / 3.0f - 0.5f)).rgb +
                                          frame_copy.Sample(frame_sampler, uv + dir * (2.0f / 3.0f - 0.5f)).rgb);
                    float3 rgbB = rgbA * 0.5f + 0.25f * (frame_copy.Sample(frame_sampler, uv - dir * 0.5f).rgb +
                                                         frame_copy.Sample(frame_sampler, uv + dir * 0.5f).rgb);
                    float lumaB = lumaOf(rgbB);
                    return (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
                }

                float4 ps_main(float4 pos : SV_Position) : SV_Target {
                    float4 c = frame_copy.Load(int3((int2)pos.xy, 0));
                    float3 rgb = c.rgb;
                    if (fxaa > 0.5f) {
                        rgb = Fxaa(pos.xy / screen_size, float2(1.0f, 1.0f) / screen_size);
                    }
                    return float4(pow(saturate(rgb), gamma_exp), c.a);
                }
            )";

            Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
            Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
            HRESULT hr = D3DCompile(s_gammaShaderCode, strlen(s_gammaShaderCode), "GammaVS", nullptr, nullptr,
                                    "vs_main", "vs_4_0", 0, 0, vsBlob.GetAddressOf(), errorBlob.GetAddressOf());
            if (FAILED(hr)) {
                if (errorBlob) {
                    NOBLE_ERROR("GammaPass: VS compile failed: {}", static_cast<const char*>(errorBlob->GetBufferPointer()));
                }
                return false;
            }

            Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
            hr = D3DCompile(s_gammaShaderCode, strlen(s_gammaShaderCode), "GammaPS", nullptr, nullptr,
                            "ps_main", "ps_4_0", 0, 0, psBlob.GetAddressOf(), errorBlob.GetAddressOf());
            if (FAILED(hr)) {
                if (errorBlob) {
                    NOBLE_ERROR("GammaPass: PS compile failed: {}", static_cast<const char*>(errorBlob->GetBufferPointer()));
                }
                return false;
            }

            D3D11_SAMPLER_DESC frameSampler{};
            frameSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            frameSampler.AddressU = frameSampler.AddressV = frameSampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            frameSampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
            frameSampler.MaxLOD = D3D11_FLOAT32_MAX;
            if (FAILED(device->CreateSamplerState(&frameSampler, m_frameSampler.GetAddressOf()))) {
                return false;
            }

            if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vs.GetAddressOf()))) {
                return false;
            }
            if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_ps.GetAddressOf()))) {
                return false;
            }

            D3D11_BUFFER_DESC cbDesc{};
            cbDesc.ByteWidth = sizeof(GammaCB);
            cbDesc.Usage = D3D11_USAGE_DYNAMIC;
            cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if (FAILED(device->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf()))) {
                return false;
            }
        }

        D3D11_TEXTURE2D_DESC copyDesc = desc;
        copyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        copyDesc.Usage = D3D11_USAGE_DEFAULT;
        copyDesc.CPUAccessFlags = 0;
        copyDesc.MiscFlags = 0;
        m_copyTexture.Reset();
        if (FAILED(device->CreateTexture2D(&copyDesc, nullptr, m_copyTexture.GetAddressOf()))) {
            return false;
        }

        m_copySRV.Reset();
        if (FAILED(device->CreateShaderResourceView(m_copyTexture.Get(), nullptr, m_copySRV.GetAddressOf()))) {
            return false;
        }

        m_backBufferRTV.Reset();
        if (FAILED(device->CreateRenderTargetView(backBuffer, nullptr, m_backBufferRTV.GetAddressOf()))) {
            return false;
        }

        m_initialized = true;
        NOBLE_INFO("GammaPass: Initialized post-process gamma pass ({}x{}, exponent: {:.4f})", m_width, m_height, m_exponent);
        return true;
    }

    bool m_initialized = false;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    float m_exponent = 1.0f; // Default 1.0f: identity (no gamma overexposure, fixes washed-out picture)
    float m_fxaa = 0.0f;     // 0 = off, 1 = post-process anti-aliasing

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_ps;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_frameSampler;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_copyTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_copySRV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_backBufferRTV;
};

static void ExecuteGammaPass(IDXGISwapChain* swapChain) {
    GammaPass::Get().Execute(swapChain);
}

static void ResetGammaPass() {
    GammaPass::Get().OnResize();
}

static uint64_t __fastcall HostSlot67_TextMetrics(void* thisPtr, uint32_t style, float size, const char* face,
                                                  uint16_t* out0, uint16_t* out1) {
    (void)thisPtr;
    const std::wstring requested = WidenFace(face);
    const std::wstring mapped = MapReachFace(requested);
    const HostFontMetrics m = MeasureFont(mapped, style, size, FaceIsBold(requested) ? FW_BOLD : FW_NORMAL);

    static std::atomic<int> s_calls{0};
    if (s_calls.fetch_add(1) < 20) {
        NOBLE_INFO("ReachHost [Slot 67 - TextMetrics]: size={} scale={:.2f} face='{}' -> '{}' ascent={} descent={} ok={}",
            style, size, NarrowForLog(requested), NarrowForLog(mapped), m.ascent, m.descent, m.ok);
    }
    if (!m.ok) return 0;
    if (out0) SafeWriteU16(out0, m.ascent);
    if (out1) SafeWriteU16(out1, m.descent);
    return 1;
}

// Slot 88: User identity & Gamertag handler (host_probe case 0x58)
static uint64_t __fastcall HostSlot88_UserIdentity(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    // Matches Reach Renaissance host::hit case 0x58:
    //   a1 = u64* out XUID, a2 = wchar_t* out gamertag, a3 = gamertag capacity (chars), a4 = local user index.
    // Writes straight into a1/a2 (never dereferences their current contents) and returns 1.
    // Users other than 0 are reported as absent (return 0), as RR does without a second local user.
    (void)thisPtr;
    static std::atomic<bool> s_logged{false};
    const uint32_t userIndex = static_cast<uint32_t>(a4);
    if (userIndex != 0) {
        return 0;
    }

    uint64_t xuid = 0x0009000000000001ULL;
    if (NetworkTransport::Get().IsActive() && !NetworkTransport::Get().IsHost()) {
        uint64_t guestLow = NetworkTransport::Get().GetLocalMachineId() & 0x0000FFFFFFFFFFFFULL;
        if (guestLow <= 1ULL) guestLow = 2ULL;
        xuid = 0x0009000000000000ULL | guestLow;
    }
    if (!s_logged.exchange(true)) {
        NOBLE_INFO("ReachHost [Slot 88 - UserIdentity]: user {} -> '{}' (XUID: 0x{:X})", userIndex,
                   Settings::Get().Snapshot().playerName, xuid);
    }
    if (a1 > 0x10000) {
        SafeWriteUint64(a1, xuid);
    }

    // Live player name from settings (never blank, default "NobleSix", 1-20 characters)
    std::string currentName = Settings::Get().Snapshot().playerName;
    while (!currentName.empty() && std::isspace(static_cast<unsigned char>(currentName.front()))) currentName.erase(currentName.begin());
    while (!currentName.empty() && std::isspace(static_cast<unsigned char>(currentName.back()))) currentName.pop_back();
    if (currentName.empty() || currentName == "Player") currentName = "NobleSix";
    if (currentName.size() > 20) currentName.resize(20);

    std::wstring gamerTagW = Utf8ToWideString(currentName);
    if (gamerTagW.empty()) gamerTagW = L"NobleSix";
    if (gamerTagW.size() > 20) gamerTagW.resize(20);

    if (a2 > 0x10000 && a3 > 0) {
        size_t cap = static_cast<size_t>(a3);
        if (gamerTagW.size() >= cap) gamerTagW.resize(cap - 1);
        SafeCopyWideString(reinterpret_cast<wchar_t*>(a2), cap, gamerTagW.c_str());
    }
    return 1;
}

// Slot 25: User Privilege / Multiplayer Clearance Check.
// Called by haloreach when verifying remote guest players during connection handshake:
// rcx = HostInterface, rdx = guest XUID (e.g. 0x99CC93FAFC1B3), r8 = privilege type, r9 = context.
// Returning 0 causes haloreach.dll to treat the user as unauthorized, producing Join Refusal 15 (0xF).
// Returning 1 grants permission and allows the guest to join the session.
static uint64_t __fastcall HostSlot25_UserPrivilege(void* thisPtr, uint64_t xuid, uint64_t privilegeId, uint64_t context, uint64_t a4) {
    (void)thisPtr; (void)a4;
    NOBLE_INFO("ReachHost [Slot 25 - UserPrivilege]: xuid=0x{:X}, privilege=0x{:X}, context=0x{:X} -> returning 1 (Allowed)",
               xuid, privilegeId, context);
    return 1;
}

// Slot 97: colour filter. Every HUD colour goes through here before it reaches the HUD shader:
// haloreach+0x2DDF3C packs each chud_color_output (A..F) into ARGB and calls slot 97 with
// (a1 = a mode/filter index from haloreach+0xD1F71C, a2 = ARGB colour), then unpacks the returned
// ARGB. MCC uses it for its colour filters; RR (host::hit case 0x61) returns the colour unchanged.
// Noble used to return 1 here, which turned every HUD colour into (0, 0, 1/255, 0): the dark navy HUD.
static uint64_t __fastcall HostSlot97_ColorFilter(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a1; (void)a3; (void)a4;
    return a2 & 0xFFFFFFFFull;
}

// Slot 116: Controller Mapping (host_probe case 0x74). Called with the player's XUID in a1; the
// other registers are leftovers (a2 was seen pointing at this function). RR's pad::controller_map
// only logs its pad layout. The old handler wrote 64 bytes of 0xFF through a2, so it now writes
// nothing and returns 0.
static uint64_t __fastcall HostSlot116_ControllerMap(void* thisPtr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)thisPtr; (void)a1; (void)a2; (void)a3; (void)a4;
    return 0;
}

template<size_t... Is>
static void InitVTableHelper(void** vtable, std::index_sequence<Is...>) {
    ((vtable[Is] = reinterpret_cast<void*>(&SlotDispatcher<Is>::Stub)), ...);
}

void ReachHost::InitializeVTable() {
    if (s_vtableInitialized) return;

    InitVTableHelper(s_vtable, std::make_index_sequence<160>{});

    // Slots verified from Reach Renaissance (host-probe.exe) decompilation
    s_vtable[0]   = reinterpret_cast<void*>(&HostSlot0_QueryInterface);
    s_vtable[1]   = reinterpret_cast<void*>(&HostSlot1_FrameTick);
    s_vtable[2]   = reinterpret_cast<void*>(&HostSlot2_ResizeBuffers);
    s_vtable[4]   = reinterpret_cast<void*>(&HostSlot4_LogMessage);
    s_vtable[6]   = reinterpret_cast<void*>(&HostSlot6_GameResults);
    s_vtable[10]  = reinterpret_cast<void*>(&HostSlot10_Heartbeat);
    s_vtable[18]  = reinterpret_cast<void*>(&HostSlot18_AssetLookup);
    s_vtable[23]  = reinterpret_cast<void*>(&HostSlot23_LoadProgress);
    s_vtable[25]  = reinterpret_cast<void*>(&HostSlot25_UserPrivilege);
    s_vtable[32]  = reinterpret_cast<void*>(&HostSlot32_DisplaySettings);
    s_vtable[33]  = reinterpret_cast<void*>(&HostSlot33_DisplayQuality);
    s_vtable[34]  = reinterpret_cast<void*>(&HostSlot34_ProfileSettings);
    s_vtable[36]  = reinterpret_cast<void*>(&HostSlot36_InputController);
    s_vtable[39]  = reinterpret_cast<void*>(&HostSlot39_PadRumble);
    s_vtable[41]  = reinterpret_cast<void*>(&HostSlot41_NetSend);
    s_vtable[43]  = reinterpret_cast<void*>(&HostSlot43_NetRecv);
    s_vtable[47]  = reinterpret_cast<void*>(&HostSlot47_UserDataFolder);
    s_vtable[48]  = reinterpret_cast<void*>(&HostSlot48_FilePathResolver);
    s_vtable[51]  = reinterpret_cast<void*>(&HostSlot51_EngineCapability);
    s_vtable[52]  = reinterpret_cast<void*>(&HostSlot52_EngineState);
    s_vtable[55]  = reinterpret_cast<void*>(&HostSlot55_StringsClear);
    s_vtable[56]  = reinterpret_cast<void*>(&HostSlot56_StringsRegister);
    s_vtable[57]  = reinterpret_cast<void*>(&HostSlot57_StringLookup);
    s_vtable[61]  = reinterpret_cast<void*>(&HostSlot61_TextPrepare);
    s_vtable[62]  = reinterpret_cast<void*>(&HostSlot62_TextGlyph);
    s_vtable[63]  = reinterpret_cast<void*>(&HostSlot63_TextAtlas);
    s_vtable[64]  = reinterpret_cast<void*>(&HostSlot64_TextHasIcon);
    s_vtable[65]  = reinterpret_cast<void*>(&HostSlot65_TextKerning);
    s_vtable[66]  = reinterpret_cast<void*>(&HostSlot66_TextRegisterIcon);
    s_vtable[67]  = reinterpret_cast<void*>(&HostSlot67_TextMetrics);
    s_vtable[88]  = reinterpret_cast<void*>(&HostSlot88_UserIdentity);
    s_vtable[97]  = reinterpret_cast<void*>(&HostSlot97_ColorFilter);
    s_vtable[116] = reinterpret_cast<void*>(&HostSlot116_ControllerMap);

    InitVTable2Helper(s_vtable2, std::make_index_sequence<160>{});
    s_vtable2[147] = reinterpret_cast<void*>(&Iface2Slot147_SessionGuid);

    s_vtableInitialized = true;
    StringTable::Get().Initialize();
    NOBLE_INFO("ReachHost: 160-slot host vtable and secondary vtable initialized matching Reach Renaissance host.");
}

ReachHost::ReachHost() {
    InitializeVTable();
    // Link host objects matching host::new_host (0x1400828e0)
    // m_secondObject[0] = s_vtable2
    m_secondObject[0] = s_vtable2;
    // m_hostObject[0] = s_vtable, m_hostObject[1] = m_secondObject
    m_hostObject[0] = s_vtable;
    m_hostObject[1] = m_secondObject;
}

ReachHost& ReachHost::Get() {
    static ReachHost s_instance;
    return s_instance;
}

void ReachHost::SetPaths(const std::filesystem::path& reachDir,
                         const std::filesystem::path& mapsDir,
                         const std::filesystem::path& dataDir) {
    m_reachDir = reachDir;
    m_mapsDir = mapsDir;
    // MCC's own localised text for Reach (read-only, from the player's install).
    if (reachDir.has_parent_path()) {
        StringTable::Get().LoadLanguageFile(reachDir.parent_path() / "data" / "ui" / "Localization" / "EN_HaloReach.bin");
    }
    if (!dataDir.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(dataDir / "menumaps" / "mainmenu_all.map", ec) ||
            std::filesystem::exists(dataDir / "launch_block_1.bin", ec) ||
            m_dataDir.empty()) {
            m_dataDir = dataDir;
        }
    }
}

void ReachHost::SetDeviceAndContext(ID3D11Device* device, ID3D11DeviceContext* context) {
    m_device = device;
    m_context = context;
    TextAtlas::Get().Initialize(device, context);
}

} // namespace noble
