#include "Window.hpp"
#include "common/Logger.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>

namespace noble {

namespace {
Window* s_activeWindow = nullptr;
std::atomic<HWND> s_activeHwnd{ nullptr };
std::atomic<bool> s_exclusiveFullscreenSetting{ false };
std::atomic<bool> s_exclusiveFullscreenActive{ false };
// Posted by the render thread after it leaves exclusive full screen: re-apply the window geometry.
constexpr UINT kMsgRefreshGeometry = WM_APP + 0x4E;
}

Window* Window::Active() { return s_activeWindow; }

Window::Window(const std::wstring& title, uint32_t width, uint32_t height)
    : m_title(title), m_width(width), m_height(height), m_requestedWidth(width), m_requestedHeight(height) {
    s_activeWindow = this;
    m_hInstance = GetModuleHandleW(nullptr);
}

void Window::SetExclusiveFullscreen(bool on) {
    m_exclusiveFullscreen = on;
    if (on) m_borderless = true;
    s_exclusiveFullscreenSetting.store(on);
}

bool Window::WantsExclusiveFullscreen() {
    if (!s_exclusiveFullscreenSetting.load()) return false;
    const HWND hwnd = s_activeHwnd.load();
    // Only while Noble is the foreground app: Alt-Tab, a click on another monitor or Win+D drops out,
    // and coming back re-enters. DXGI itself also leaves full screen on focus loss.
    return hwnd != nullptr && GetForegroundWindow() == hwnd && !IsIconic(hwnd);
}

bool Window::IsExclusiveFullscreenActive() { return s_exclusiveFullscreenActive.load(); }
void Window::SetExclusiveFullscreenActive(bool on) { s_exclusiveFullscreenActive.store(on); }

void Window::RequestGeometryRefresh() {
    if (m_hwnd) PostMessageW(m_hwnd, kMsgRefreshGeometry, 0, 0);
}

Window::~Window() {
    if (s_activeWindow == this) s_activeWindow = nullptr;
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    UnregisterClassW(L"HaloReachCE_WindowClass", m_hInstance);
}

void Window::ApplyWindowSettings(uint32_t width, uint32_t height, bool borderless, bool exclusiveFullscreen) {
    if (!m_hwnd) return;
    m_exclusiveFullscreen = exclusiveFullscreen;
    m_borderless = borderless || exclusiveFullscreen;
    m_width = width;
    m_height = height;
    m_requestedWidth = width;
    m_requestedHeight = height;
    s_exclusiveFullscreenSetting.store(exclusiveFullscreen);

    // While DXGI owns the window in exclusive full screen, restyling it would fight the swap chain. The
    // render thread leaves full screen on its next Present (the setting is now off) and then posts
    // kMsgRefreshGeometry, which lands back here with the new geometry.
    if (IsExclusiveFullscreenActive()) return;

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    const HMONITOR monitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfoW(monitor, &monitorInfo);
    const RECT monitorRect = monitorInfo.rcMonitor;
    const RECT workRect = monitorInfo.rcWork;

    if (m_borderless) {
        if (IsZoomed(m_hwnd)) ShowWindow(m_hwnd, SW_RESTORE);
        SetWindowLongPtrW(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        const int w = monitorRect.right - monitorRect.left;
        const int h = monitorRect.bottom - monitorRect.top;
        SetWindowPos(m_hwnd, HWND_TOP, monitorRect.left, monitorRect.top, w, h,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        if (IsZoomed(m_hwnd)) ShowWindow(m_hwnd, SW_RESTORE);
        const DWORD style = WS_OVERLAPPEDWINDOW;
        RECT rect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        AdjustWindowRect(&rect, style, FALSE);
        int winW = rect.right - rect.left;
        int winH = rect.bottom - rect.top;
        const int workW = workRect.right - workRect.left;
        const int workH = workRect.bottom - workRect.top;
        const int maxClientW = std::max(320, workW - (winW - static_cast<int>(width)));
        const int maxClientH = std::max(240, workH - (winH - static_cast<int>(height)));
        if (static_cast<int>(width) > maxClientW || static_cast<int>(height) > maxClientH) {
            const float scale = std::min(static_cast<float>(maxClientW) / static_cast<float>(width),
                                         static_cast<float>(maxClientH) / static_cast<float>(height));
            const int fittedW = std::max(320, static_cast<int>(static_cast<float>(width) * scale));
            const int fittedH = std::max(240, static_cast<int>(static_cast<float>(height) * scale));
            rect = { 0, 0, fittedW, fittedH };
            AdjustWindowRect(&rect, style, FALSE);
            winW = rect.right - rect.left;
            winH = rect.bottom - rect.top;
        }
        const int posX = workRect.left + (workW - winW) / 2;
        const int posY = workRect.top + (workH - winH) / 2;
        SetWindowLongPtrW(m_hwnd, GWL_STYLE, style);
        SetWindowPos(m_hwnd, HWND_NOTOPMOST, posX, posY, winW, winH, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    SetForegroundWindow(m_hwnd);
}

bool Window::Create() {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = StaticWndProc;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(101));
    wc.hIconSm = LoadIconW(m_hInstance, MAKEINTRESOURCEW(101));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = L"HaloReachCE_WindowClass";

    if (!RegisterClassExW(&wc)) {
        NOBLE_ERROR("Failed to register Win32 window class.");
        return false;
    }

    // Fixed-size window: the engine renders at the size chosen in Settings and cannot change that once
    // it has started, so a maximized or dragged-larger window can only stretch (and pixelate) that picture.
    // WINDOW MODE = BORDERLESS FULL SCREEN is the full-screen path: it creates the window at the monitor's
    // size. The menu process additionally supersamples 1.5x and the window downscales that cleanly.
    DWORD style = WS_OVERLAPPEDWINDOW;
    int windowWidth = 0, windowHeight = 0, posX = 0, posY = 0;
    if (m_borderless) {
        // Borderless: a popup window the size of the primary monitor.
        style = WS_POPUP;
        m_width = static_cast<uint32_t>(GetSystemMetrics(SM_CXSCREEN));
        m_height = static_cast<uint32_t>(GetSystemMetrics(SM_CYSCREEN));
        windowWidth = static_cast<int>(m_width);
        windowHeight = static_cast<int>(m_height);
    } else {
        RECT work{};
        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
            work = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        }
        RECT rc = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
        AdjustWindowRect(&rc, style, FALSE);
        windowWidth = rc.right - rc.left;
        windowHeight = rc.bottom - rc.top;

        // A 1920x1080 client plus its frame is wider/taller than a 1920x1080 desktop. Shrink a window
        // that would not fit the work area (keeping its aspect), so normal windowed mode always opens
        // fully on screen and the engine renders at exactly the client size.
        const int workW = work.right - work.left;
        const int workH = work.bottom - work.top;
        const int borderW = windowWidth - static_cast<int>(m_width);
        const int borderH = windowHeight - static_cast<int>(m_height);
        const int maxClientW = std::max(320, workW - borderW);
        const int maxClientH = std::max(240, workH - borderH);
        if (static_cast<int>(m_width) > maxClientW || static_cast<int>(m_height) > maxClientH) {
            const float scale = std::min(static_cast<float>(maxClientW) / static_cast<float>(m_width),
                                         static_cast<float>(maxClientH) / static_cast<float>(m_height));
            const int fittedW = std::max(320, static_cast<int>(static_cast<float>(m_width) * scale));
            const int fittedH = std::max(240, static_cast<int>(static_cast<float>(m_height) * scale));
            NOBLE_INFO("Win32 window: {}x{} does not fit the {}x{} work area; opening at {}x{}",
                       m_width, m_height, workW, workH, fittedW, fittedH);
            m_width = static_cast<uint32_t>(fittedW);
            m_height = static_cast<uint32_t>(fittedH);
            RECT fitted = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
            AdjustWindowRect(&fitted, style, FALSE);
            windowWidth = fitted.right - fitted.left;
            windowHeight = fitted.bottom - fitted.top;
        }
        posX = work.left + (workW - windowWidth) / 2;
        posY = work.top + (workH - windowHeight) / 2;
        if (posX < work.left) posX = work.left;
        if (posY < work.top) posY = work.top;
    }

    m_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        m_title.c_str(),
        style,
        posX, posY,
        windowWidth, windowHeight,
        nullptr, nullptr,
        m_hInstance,
        this
    );

    if (!m_hwnd) {
        NOBLE_ERROR("Failed to create Win32 host window. Error: {}", GetLastError());
        return false;
    }

    s_activeHwnd.store(m_hwnd);

    SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wc.hIcon));
    SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(wc.hIconSm));

    NOBLE_INFO("Win32 host window created successfully ({}x{}). HWND: 0x{:X}", 
        m_width, m_height, reinterpret_cast<uintptr_t>(m_hwnd));

    return true;
}

void Window::Show(int cmdShow) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, cmdShow);
        UpdateWindow(m_hwnd);
    }
}

bool Window::ProcessMessages() {
    MSG msg = { 0 };
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_shouldClose = true;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !m_shouldClose;
}

LRESULT CALLBACK Window::StaticWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Window* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<Window*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

namespace {
std::atomic<int> s_wheelDelta{0};
std::atomic<bool> s_sizeMove{false};
std::atomic<int> s_leftClicks{0};
} // namespace

int Window::TakeLeftClicks() { return s_leftClicks.exchange(0); }

namespace {
std::atomic<bool> s_charCapture{ false };
std::mutex s_typedMutex;
std::wstring s_typedChars;
}

void Window::SetCharCapture(bool on) {
    std::lock_guard<std::mutex> lock(s_typedMutex);
    s_typedChars.clear();
    s_charCapture.store(on);
}

std::wstring Window::TakeTypedChars() {
    std::lock_guard<std::mutex> lock(s_typedMutex);
    std::wstring out;
    out.swap(s_typedChars);
    return out;
}

int Window::TakeWheelNotches() {
    int delta = s_wheelDelta.load();
    for (;;) {
        const int notches = delta / WHEEL_DELTA;
        if (notches == 0) return 0;
        if (s_wheelDelta.compare_exchange_weak(delta, delta - notches * WHEEL_DELTA)) return notches;
    }
}

bool Window::InSizeMove() { return s_sizeMove.load(); }

LRESULT Window::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Test-only: keep a capture run from reacting to the mouse/keyboard (see "-menu-ignore-input").
    static const bool s_ignoreInput = wcsstr(GetCommandLineW(), L"-menu-ignore-input") != nullptr;
    if (s_ignoreInput) {
        const bool mouse = (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) || uMsg == WM_MOUSEWHEEL ||
                           uMsg == WM_MOUSELEAVE || uMsg == WM_SETCURSOR;
        const bool key = (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST);
        if (mouse || key) return 0;
    }
    switch (uMsg) {
        case WM_MOUSEWHEEL: s_wheelDelta.fetch_add(GET_WHEEL_DELTA_WPARAM(wParam)); break;
        case WM_LBUTTONDOWN: s_leftClicks.fetch_add(1); break;
        case WM_CHAR:
            if (s_charCapture.load()) {
                std::lock_guard<std::mutex> lock(s_typedMutex);
                if (s_typedChars.size() < 256) s_typedChars.push_back(static_cast<wchar_t>(wParam));
            }
            break;
        case WM_ENTERSIZEMOVE: s_sizeMove.store(true); ClipCursor(nullptr); break;
        case WM_EXITSIZEMOVE: s_sizeMove.store(false); break;
        case WM_KILLFOCUS: ClipCursor(nullptr); break;
        default: break;
    }
    if (m_customWndProc) {
        LRESULT res = m_customWndProc(hwnd, uMsg, wParam, lParam);
        if (res != 0) return res;
    }

    switch (uMsg) {
        case WM_ERASEBKGND:
            return 1; // Prevent GDI from clearing background over DirectX 11 presentation
        case WM_CLOSE:
            m_shouldClose = true;
            DestroyWindow(hwnd);
            return 0;
        case kMsgRefreshGeometry:
            if (!IsExclusiveFullscreenActive()) {
                ApplyWindowSettings(m_requestedWidth, m_requestedHeight, m_borderless, m_exclusiveFullscreen);
            }
            return 0;
        case WM_DESTROY:
            s_activeHwnd.store(nullptr);
            m_shouldClose = true;
            PostQuitMessage(0);
            return 0;
        case WM_SIZING: {
            RECT* r = reinterpret_cast<RECT*>(lParam);
            const DWORD curStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
            RECT borderRect = { 0, 0, 1600, 900 };
            AdjustWindowRect(&borderRect, curStyle, FALSE);
            const int borderW = (borderRect.right - borderRect.left) - 1600;
            const int borderH = (borderRect.bottom - borderRect.top) - 900;

            int curClientW = (r->right - r->left) - borderW;
            int curClientH = (r->bottom - r->top) - borderH;
            if (curClientW < 640) curClientW = 640;
            if (curClientH < 360) curClientH = 360;

            constexpr double kAspect = 16.0 / 9.0;
            if (wParam == WMSZ_LEFT || wParam == WMSZ_RIGHT) {
                curClientH = static_cast<int>(std::round(curClientW / kAspect));
            } else {
                curClientW = static_cast<int>(std::round(curClientH * kAspect));
            }
            r->right = r->left + curClientW + borderW;
            r->bottom = r->top + curClientH + borderH;
            return TRUE;
        }
        case WM_SIZE:
            m_width = LOWORD(lParam);
            m_height = HIWORD(lParam);
            break;
            // DefWindowProcW handles sizing finish
            return 0;
        case WM_SETCURSOR:
            // Hide the arrow over the game area while the window is focused (the engine thread
            // can't do this itself: SetCursor/ShowCursor only work from the window's own thread).
            // ReachHost clears the class cursor while it owns the mouse (mouse look) and restores the
            // arrow while a menu needs the pointer, so hide it only when the class cursor is cleared.
            if (LOWORD(lParam) == HTCLIENT && GetForegroundWindow() == hwnd &&
                GetClassLongPtrW(hwnd, GCLP_HCURSOR) == 0) {
                SetCursor(nullptr);
                return TRUE;
            }
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

} // namespace noble
