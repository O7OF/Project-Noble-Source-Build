#include "SettingsPanel.hpp"
#include "host/Window.hpp"
#include "audio/VoiceChat.hpp"
#include "common/Logger.hpp"
#include "NobleStyle.hpp"
#include "NobleUi.hpp"
#include "MenuSounds.hpp"
#include "audio/ReachAudio.hpp"
#include "audio/ProcessAudio.hpp"
#include "UiArt.hpp"
#include "common/Version.hpp"
#include "InputPrompts.hpp"
#include "UiPad.hpp"
#include "engine/KeyBindings.hpp"
#include "engine/CameraFov.hpp"
#include "engine/PadLayouts.hpp"
#include <map>
#include "network/MasterServerClient.hpp"

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>   // GetOpenFileNameW for IMPORT BACKGROUND
#include <dxgi.h>
#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>
#include <fstream>
#include <vector>

namespace noble {

namespace {

using namespace NobleUi;

struct Resolution {
    int w, h;
};
const Resolution kResolutions[] = {
    {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1080}, {2560, 1440}, {3440, 1440}, {3840, 2160},
};
constexpr int kFpsLimits[] = { 30, 60, 120, 144, 165, 200, 0 };
const int kWheelActions[] = { 4, 1, 3, 5, 7, 2, 23, 6, 11, 12, 20, -1 };

std::string Upper(std::string s) {
    for (char& c : s) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
    return s;
}
std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string Format(const char* fmt, double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), fmt, v);
    return buf;
}
std::string Percent(float v) { return Format("%.0f %%", v * 100.0f); }

struct CatInfo {
    const char* group;   // printed above the first category of a group
    const char* name;
    const char* footer;  // when this category's changes take effect
};
const CatInfo kCats[] = {
    { "DISPLAY & SOUND", "GRAPHICS", "Window mode and size apply immediately; the game always renders at your display's native resolution, so text, HUD and gameplay stay crisp." },
    { nullptr, "AUDIO", "Volumes take effect when you apply changes; a menu song plays as soon as you pick it." },
    { "GAME", "GAME OPTIONS", "Takes effect when you apply changes, also in a game you return to from the pause menu." },
    { nullptr, "PLAYER", "Saved when you apply changes; shown from the next game you start." },
    { "CONTROLS", "KEYBOARD & MOUSE", "Takes effect when you apply changes, also mid-game from the pause menu." },
    { nullptr, "CONTROLLER", "Takes effect when you apply changes, also mid-game from the pause menu." },
    { "ONLINE", "ONLINE SERVICES", "Saved when you apply changes." },
    { "SYSTEM", "SYSTEM", "Diagnostics and backups. These act straight away." },
};
static_assert(std::size(kCats) == 8);

const char* WhenText(int when) {
    switch (when) {
    case 0: return "Takes effect when you apply changes";
    case 1: return "Saved when you apply changes; takes effect the next time Noble starts";
    case 2: return "Saved and used as soon as you change it";
    default: return "Information";
    }
}

// The graphics card Windows uses for Noble (adapter 0), read once.
std::string GpuLine() {
    static std::string s_line;
    static bool s_done = false;
    if (s_done) return s_line;
    s_done = true;
    IDXGIFactory1* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) && factory) {
        IDXGIAdapter1* adapter = nullptr;
        if (SUCCEEDED(factory->EnumAdapters1(0, &adapter)) && adapter) {
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                char name[128] = {};
                WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr, nullptr);
                char buf[200];
                std::snprintf(buf, sizeof(buf), "%s (%.1f GB)", name, desc.DedicatedVideoMemory / (1024.0 * 1024.0 * 1024.0));
                s_line = buf;
            }
            adapter->Release();
        }
        factory->Release();
    }
    if (s_line.empty()) s_line = "Unknown";
    return s_line;
}

// First connected XInput controller (0..3), or -1. Polled at most once a second.
int ConnectedPad() {
    struct State { DWORD packet; WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; };
    using GetStateFn = DWORD(WINAPI*)(DWORD, State*);
    static GetStateFn s_get = [] {
        HMODULE m = LoadLibraryW(L"xinput1_4.dll");
        if (!m) m = LoadLibraryW(L"xinput9_1_0.dll");
        return m ? reinterpret_cast<GetStateFn>(GetProcAddress(m, "XInputGetState")) : nullptr;
    }();
    static int s_pad = -1;
    static double s_checked = -10.0;
    const double now = ImGui::GetTime();
    if (now - s_checked < 1.0) return s_pad;
    s_checked = now;
    s_pad = -1;
    if (!s_get) return s_pad;
    for (DWORD i = 0; i < 4; ++i) {
        State st{};
        if (s_get(i, &st) == ERROR_SUCCESS) { s_pad = static_cast<int>(i); break; }
    }
    return s_pad;
}

// Noble's controller picture (assets\ui\noble\controller.png, by O7OF), or MCC's own when Noble's isn't there. Each callout
// is tied to one physical input and says what that input does in the current preset and bindings, so the leader lines stay
// honest when a preset moves actions around. Listed top to bottom on each side.
struct Callout {
    float bx, by;
    bool right;
    int input;
    InputPrompts::Button button;
};
const Callout kCallouts[] = {
    { 0.22f, 0.07f, false, PadLayouts::InputLT, InputPrompts::Button::LT },
    { 0.20f, 0.19f, false, 0x0100, InputPrompts::Button::LB },
    { 0.39f, 0.50f, false, 0x0020, InputPrompts::Button::Back },
    { 0.20f, 0.53f, false, 0x0040, InputPrompts::Button::LStick },
    { 0.35f, 0.74f, false, 0x0001, InputPrompts::Button::DpadUp },
    { 0.77f, 0.07f, true, PadLayouts::InputRT, InputPrompts::Button::RT },
    { 0.81f, 0.19f, true, 0x0200, InputPrompts::Button::RB },
    { 0.80f, 0.39f, true, 0x8000, InputPrompts::Button::Y },
    { 0.87f, 0.49f, true, 0x2000, InputPrompts::Button::B },
    { 0.72f, 0.50f, true, 0x4000, InputPrompts::Button::X },
    { 0.61f, 0.50f, true, 0x0010, InputPrompts::Button::Start },
    { 0.79f, 0.60f, true, 0x1000, InputPrompts::Button::A },
    { 0.64f, 0.76f, true, 0x0080, InputPrompts::Button::RStick },
};

// What one physical input does right now ("Fire", "Melee, Zoom"). The stick clicks also say what the stick moves.
std::string CalloutText(const Callout& c, int layout, bool sticksSwapped, const std::map<std::string, int>& bindings) {
    std::string text;
    for (int a = 0; a < PadLayouts::ActionCount; ++a) {
        if (PadLayouts::EffectiveInput(layout, a, &bindings) != c.input) continue;
        if (!text.empty()) text += ", ";
        text += PadLayouts::ActionName(a);
    }
    if (c.input == 0x0040 || c.input == 0x0080) {
        const bool leftStick = c.input == 0x0040;
        const std::string stick = leftStick != sticksSwapped ? "Move" : "Look";
        text = text.empty() ? stick : stick + " / " + text;
    }
    return text.empty() ? std::string("Unused") : text;
}

// Controller picture with straight leader lines to the labels, fitted into `width`.
void DrawControllerDiagram(float width, int layout, bool sticksSwapped, const std::map<std::string, int>& bindings) {
    UiTexture pad = UiArt::NobleControllerImage();
    if (!pad) pad = UiArt::ControllerImage();
    const float glyphH = ImGui::GetTextLineHeight() * 1.05f;
    float leftWidth = 0.0f, rightWidth = 0.0f;
    for (const Callout& c : kCallouts) {
        const float glyphW = InputPrompts::ButtonWidth(ImGui::GetFont(), glyphH, c.button) + 6.0f;
        float& side = c.right ? rightWidth : leftWidth;
        side = std::max(side, glyphW + ImGui::CalcTextSize(CalloutText(c, layout, sticksSwapped, bindings).c_str()).x);
    }
    const float w = std::clamp(width - leftWidth - rightWidth - 70.0f, 160.0f, 440.0f);
    const float h = pad ? w * pad.height / pad.width : w * 0.62f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float padX = origin.x + leftWidth + 30.0f, padY = origin.y + 16.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (pad) dl->AddImage(pad.id, ImVec2(padX, padY), ImVec2(padX + w, padY + h));
    else dl->AddRect(ImVec2(padX, padY), ImVec2(padX + w, padY + h), IM_COL32(200, 210, 220, 120), 30.0f);
    int leftRow = 0, rightRow = 0;
    const float leftStep = h / 5.0f, rightStep = h / 8.0f;
    const float lh = ImGui::GetTextLineHeight();
    for (const Callout& c : kCallouts) {
        const ImVec2 btn(padX + c.bx * w, padY + c.by * h);
        const float rowY = c.right ? padY + (rightRow++ + 0.5f) * rightStep - lh * 0.5f
                                   : padY + (leftRow++ + 0.5f) * leftStep - lh * 0.5f;
        const ImVec2 elbow = c.right ? ImVec2(padX + w + 14.0f, rowY + lh * 0.5f) : ImVec2(padX - 10.0f, rowY + lh * 0.5f);
        const std::string text = CalloutText(c, layout, sticksSwapped, bindings);
        const float glyphW = InputPrompts::ButtonWidth(ImGui::GetFont(), glyphH, c.button) + 6.0f;
        const float rowW = glyphW + ImGui::CalcTextSize(text.c_str()).x;
        const ImVec2 rowPos = c.right ? ImVec2(elbow.x + 8.0f, rowY) : ImVec2(elbow.x - 8.0f - rowW, rowY);
        dl->AddLine(btn, elbow, IM_COL32(200, 210, 220, 140), 1.2f);
        dl->AddCircleFilled(btn, 3.0f, kAccent);
        InputPrompts::DrawButton(dl, ImGui::GetFont(), ImVec2(rowPos.x, rowY - (glyphH - lh) * 0.5f), glyphH, c.button);
        dl->AddText(ImVec2(rowPos.x + glyphW, rowY), kText, text.c_str());
    }
    ImGui::Dummy(ImVec2(leftWidth + 30.0f + w + 22.0f + rightWidth, h + 30.0f));
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------------------------

void SettingsPanel::SetTab(int tab) {
    const int n = static_cast<int>(Cat::Count);
    m_cat = static_cast<Cat>(((tab % n) + n) % n);
    m_focus = 0;
}

void SettingsPanel::SyncBuffers() {
    std::string n = m_nameBuf;
    n.erase(n.begin(), std::find_if(n.begin(), n.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    n.erase(std::find_if(n.rbegin(), n.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), n.end());
    if (n.size() > 20) n.resize(20);
    m_edit.playerName = n.empty() ? "NobleSix" : n;
    m_edit.serviceTag = m_tagBuf;
    m_edit.masterServerUrl = m_masterBuf;
}

void SettingsPanel::Render() {
    if (!m_loaded) {
        m_saved = Settings::Get().Snapshot();
        // Old saved menu-music values ("shuffle", piece names) play Reach's menu theme: treat them as
        // that track so they don't count as an unsaved change.
        {
            const std::vector<ReachAudio::TrackInfo> tracks = ReachAudio::MusicTracks();
            if (std::none_of(tracks.begin(), tracks.end(), [&](const ReachAudio::TrackInfo& t) { return t.id == m_saved.menuMusicTrack; }) &&
                std::any_of(tracks.begin(), tracks.end(), [](const ReachAudio::TrackInfo& t) { return t.id == "reach:menu"; })) {
                m_saved.menuMusicTrack = "reach:menu";
            }
        }
        m_edit = m_saved;
        std::snprintf(m_nameBuf, sizeof(m_nameBuf), "%s", m_edit.playerName.c_str());
        std::snprintf(m_tagBuf, sizeof(m_tagBuf), "%s", m_edit.serviceTag.c_str());
        std::snprintf(m_masterBuf, sizeof(m_masterBuf), "%s", m_edit.masterServerUrl.c_str());
        m_loaded = true;
    }
    PollRebind();
    SyncBuffers();

    // Shared key state for this frame (rows read it; text fields and rebinding take the keys).
    m_navAllowed = !ImGui::GetIO().WantTextInput && m_rebindAction < 0 && m_padRebindAction < 0 && m_paddleKeyCapture < 0;
    m_valueTookLeftRight = false;
    m_enterAppliesHere = m_focus < 0;
    m_keyLeft = m_navAllowed && (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft));
    m_keyRight = m_navAllowed && (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight));
    m_keyAccept = m_navAllowed && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
                                   ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false));
    if (m_navAllowed) {
        const int rows = std::max(1, m_lastRowCount);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown)) {
            m_focus = m_focus + 1 >= rows ? -1 : m_focus + 1; m_scrollToFocus = true; MenuSounds::Scroll();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickUp)) {
            m_focus = m_focus < 0 ? rows - 1 : m_focus - 1; m_scrollToFocus = true; MenuSounds::Scroll();
        }
        if (UiPad::PageDown() || UiPad::PageUp()) {   // the triggers jump a screenful of rows
            const int jump = UiPad::PageDown() ? 8 : -8;
            m_focus = std::clamp(m_focus + jump, 0, rows - 1); m_scrollToFocus = true; MenuSounds::Scroll();
        }
        const int n = static_cast<int>(Cat::Count);
        if (ImGui::IsKeyPressed(ImGuiKey_Q, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false)) {
            m_cat = static_cast<Cat>((static_cast<int>(m_cat) + n - 1) % n); m_focus = 0; m_search[0] = 0; m_scrollToFocus = true; MenuSounds::Tab(-1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)) {
            m_cat = static_cast<Cat>((static_cast<int>(m_cat) + 1) % n); m_focus = 0; m_search[0] = 0; m_scrollToFocus = true; MenuSounds::Tab(1);
        }
        if (TotalChanges() > 0) {
            if (ImGui::IsKeyPressed(ImGuiKey_Y, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false)) Apply();
            else if (ImGui::IsKeyPressed(ImGuiKey_X, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false)) Discard();
        }
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float s = Scale();
    const float gap = 14.0f * s;
    const float leftW = std::clamp(avail.x * 0.24f, 230.0f * s, 330.0f * s);
    const float rightW = std::clamp(avail.x * 0.28f, 250.0f * s, 430.0f * s);
    const float midW = std::max(200.0f, avail.x - leftW - rightW - gap * 2.0f);

    RenderCategories(leftW, avail.y);
    ImGui::SameLine(0, gap);
    RenderRows(midW, avail.y);
    ImGui::SameLine(0, gap);
    RenderDetails(rightW, avail.y);

    // Left/right moves between tabs only when nothing on the page wanted it: on the tab list (above the first row) or on
    // a row without a value. A value row adjusts its value instead. This runs after the rows, which is how it can tell.
    if (m_navAllowed && !m_valueTookLeftRight && m_search[0] == 0) {
        const bool left = ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft, false) ||
                          ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false);
        const bool right = ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight, false) ||
                           ImGui::IsKeyPressed(ImGuiKey_RightArrow, false);
        if (left || right) {
            const int n = static_cast<int>(Cat::Count);
            m_cat = static_cast<Cat>((static_cast<int>(m_cat) + (right ? 1 : n - 1)) % n);
            if (m_focus > 0) m_focus = 0;
            m_scrollToFocus = true;
            MenuSounds::Tab(right ? 1 : -1);
        }
    }
    if (m_navAllowed && m_enterAppliesHere && TotalChanges() > 0 &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
        Apply();
    }
}

bool SettingsPanel::DiscardOnEscape() {
    if (TotalChanges() == 0) return false;
    Apply(); // Auto-save on Escape/Back so settings & keybinds are never silently lost.
    return true;
}

// ---------------------------------------------------------------------------------------------
// Left column: search, categories, unsaved box
// ---------------------------------------------------------------------------------------------

void SettingsPanel::RenderCategories(float width, float height) {
    const float s = Scale();
    // The panel does its own controller navigation (see Render); ImGui's would press these buttons a second time.
    constexpr ImGuiWindowFlags kNoNav = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
    ImGui::BeginChild("##setcats", ImVec2(width, height), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | kNoNav);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::SetNextItemWidth(width);
    if (ImGui::InputTextWithHint("##setsearch", "Search all settings", m_search, sizeof(m_search))) {
        m_focus = 0;
    }
    ImGui::Dummy(ImVec2(0, 4.0f * s));

    const int total = TotalChanges();
    const float boxH = total > 0 ? 142.0f * s : 52.0f * s;
    ImGui::BeginChild("##setcatlist", ImVec2(width, ImGui::GetContentRegionAvail().y - boxH), ImGuiChildFlags_None, kNoNav);
    for (int i = 0; i < static_cast<int>(Cat::Count); ++i) {
        const CatInfo& ci = kCats[i];
        if (ci.group) SectionHeader(ci.group, width - 8.0f * s);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        const float h = 50.0f * s;
        ImGui::PushID(i);
        const bool clicked = ImGui::InvisibleButton("##cat", ImVec2(w, h));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool sel = static_cast<int>(m_cat) == i && m_search[0] == 0;
        if (sel && m_scrollToFocus) ImGui::SetScrollHereY(0.5f);   // the unsaved box can hide the last tabs
        RowBackground(dl, p, ImVec2(p.x + w, p.y + h), sel, hovered);
        ImDrawList* wdl = ImGui::GetWindowDrawList();
        if (sel && m_focus < 0) {   // keys and pad are on the tab list: left/right switches tabs
            wdl->AddRect(p, ImVec2(p.x + w, p.y + h), kAccent, 0.0f, 0, 2.0f * s);
            wdl->AddText(NobleStyle::SmallFont(), 14.0f * s, ImVec2(p.x + w - 34.0f * s, p.y + 18.0f * s), kAccent, "< >");
        }
        Text(wdl, NobleStyle::TitleFont(), 19.0f * s, ImVec2(p.x + 14.0f * s, p.y + 5.0f * s), sel ? kText : kTextDim, ci.name);
        const std::string summary = Summary(static_cast<Cat>(i));
        Text(wdl, NobleStyle::SmallFont(), 14.0f * s, ImVec2(p.x + 14.0f * s, p.y + 28.0f * s), kTextDim, summary.c_str());
        if (ChangesIn(static_cast<Cat>(i)) > 0) ChangedDot(wdl, ImVec2(p.x + w - 12.0f * s, p.y + h * 0.5f), s);
        if (hovered) MenuSounds::Hover(ImGui::GetItemID());
        if (clicked) {
            if (static_cast<int>(m_cat) != i) MenuSounds::Tab(i > static_cast<int>(m_cat) ? 1 : -1);
            m_cat = static_cast<Cat>(i);
            m_focus = 0;
            m_search[0] = 0;
        }
    }
    ImGui::EndChild();

    // Bottom: unsaved-changes box, or RESTORE DEFAULTS for this category.
    const ImVec2 b0 = ImGui::GetCursorScreenPos();
    if (total > 0) {
        const ImVec2 b1(b0.x + width, b0.y + boxH - 6.0f * s);
        dl->AddRectFilled(b0, b1, IM_COL32(40, 30, 14, 200));
        dl->AddRect(b0, b1, IM_COL32(255, 186, 74, 120), 0.0f, 0, 1.0f);
        ChangedDot(dl, ImVec2(b0.x + 14.0f * s, b0.y + 16.0f * s), s);
        char head[64];
        std::snprintf(head, sizeof(head), "%d UNSAVED CHANGE%s", total, total == 1 ? "" : "S");
        Text(dl, NobleStyle::TitleFont(), 17.0f * s, ImVec2(b0.x + 24.0f * s, b0.y + 6.0f * s), kAmber, head);
        std::string where = "In ";
        bool first = true;
        for (int i = 0; i < static_cast<int>(Cat::Count); ++i) {
            if (ChangesIn(static_cast<Cat>(i)) == 0) continue;
            if (!first) where += ", ";
            std::string n = kCats[i].name;
            for (size_t k = 1; k < n.size(); ++k) if (n[k - 1] != ' ' && n[k - 1] != '&') n[k] = static_cast<char>(::tolower(static_cast<unsigned char>(n[k])));
            where += n;
            first = false;
        }
        Text(dl, NobleStyle::SmallFont(), 14.0f * s, ImVec2(b0.x + 24.0f * s, b0.y + 30.0f * s), kTextDim, where.c_str(), width - 30.0f * s);
        // Controller button and key side by side, both rows clickable.
        auto action = [&](const char* id, float y, InputPrompts::Button button, const char* key, const char* label) {
            const ImVec2 r0(b0.x + 10.0f * s, y), r1(b0.x + width - 10.0f * s, y + 32.0f * s);
            ImGui::SetCursorScreenPos(r0);
            const bool clicked = ImGui::InvisibleButton(id, ImVec2(r1.x - r0.x, r1.y - r0.y));
            const bool over = ImGui::IsItemHovered();
            dl->AddRectFilled(r0, r1, over ? IM_COL32(70, 110, 150, 220) : IM_COL32(30, 44, 58, 220), 2.0f * s);
            ImFont* font = ImGui::GetFont();
            const float glyph = 22.0f * s;
            float x = r0.x + 6.0f * s;
            x += InputPrompts::DrawButton(dl, font, ImVec2(x, y + 5.0f * s), glyph, button) + 5.0f * s;
            x += InputPrompts::DrawKey(dl, font, ImVec2(x, y + 5.0f * s), glyph, key) + 9.0f * s;
            Text(dl, NobleStyle::TitleFont(), 16.0f * s, ImVec2(x, y + 7.0f * s), kText, label);
            return clicked;
        };
        if (action("##apply", b1.y - 72.0f * s, InputPrompts::Button::Y, "ENTER", "APPLY")) Apply();
        if (action("##discard", b1.y - 37.0f * s, InputPrompts::Button::X, "ESC", "DISCARD")) Discard();
    } else {
        // Greyed when this category already has its default values.
        const bool differs = CountDiff(m_edit, WithDefaults(m_edit, m_cat), m_cat) > 0;
        ImGui::SetCursorScreenPos(ImVec2(b0.x, b0.y + 8.0f * s));
        ImGui::BeginDisabled(!differs || m_search[0] != 0);
        const bool padRestore = m_navAllowed && differs && m_search[0] == 0 && ImGui::IsKeyPressed(ImGuiKey_GamepadBack, false);
        const bool pad = InputPrompts::Current() == InputPrompts::Device::Gamepad;
        if (ImGui::Button(pad ? "RESTORE DEFAULTS (BACK)" : "RESTORE DEFAULTS", ImVec2(width, 32.0f * s)) || padRestore) {
            RestoreDefaults(m_cat);
            MenuSounds::Click();
        }
        ImGui::EndDisabled();
        if (!m_message.empty() && ImGui::GetTime() - m_messageTime < 4.0) {
            Text(dl, NobleStyle::SmallFont(), 14.0f * s, ImVec2(b0.x, b0.y + 44.0f * s), kAccent, m_message.c_str());
        }
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------------
// Middle column: the rows
// ---------------------------------------------------------------------------------------------

void SettingsPanel::RenderRows(float width, float height) {
    const float s = Scale();
    ImGui::BeginChild("##setrows", ImVec2(width, height), ImGuiChildFlags_None, ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
    if (m_focus < 0 && m_scrollToFocus) ImGui::SetScrollY(0.0f);
    UiPad::ScrollWindow(false, false);   // right stick reads ahead of the focused row
    m_rowCount = 0;
    m_hover = Details{};
    m_focusDetails = Details{};
    m_pendingSection = nullptr;
    m_pendingCat = -1;

    if (m_search[0] != 0) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kTextDim), "Results for \"%s\"", m_search);
        for (int i = 0; i < static_cast<int>(Cat::Count); ++i) {
            m_pendingCat = i;
            RenderCategory(static_cast<Cat>(i));
            m_pendingCat = -1;
        }
        if (m_rowCount == 0) {
            ImGui::Dummy(ImVec2(0, 10.0f * s));
            Paragraph("No setting matches. Try another word, or clear the search to browse the categories.");
        }
    } else {
        const CatInfo& ci = kCats[static_cast<int>(m_cat)];
        const ImVec2 p = ImGui::GetCursorScreenPos();
        Text(ImGui::GetWindowDrawList(), NobleStyle::TitleFont(), 26.0f * s, p, kText, ci.name);
        ImGui::Dummy(ImVec2(0, 30.0f * s));
        Paragraph(ci.footer);
        RenderCategory(m_cat);
    }
    m_lastRowCount = m_rowCount;
    if (m_rowCount > 0 && m_focus >= m_rowCount) m_focus = m_rowCount - 1;
    m_scrollToFocus = false;
    ImGui::Dummy(ImVec2(0, 20.0f * s));
    ImGui::EndChild();
}

void SettingsPanel::RenderCategory(Cat c) {
    switch (c) {
    case Cat::Graphics: RenderGraphics(); break;
    case Cat::Audio: RenderAudio(); break;
    case Cat::Game: RenderGame(); break;
    case Cat::Player: RenderPlayer(); break;
    case Cat::KeyboardMouse: RenderKeyboardMouse(); break;
    case Cat::Controller: RenderController(); break;
    case Cat::Online: RenderOnline(); break;
    case Cat::System: RenderSystem(); break;
    default: break;
    }
}

void SettingsPanel::Section(const char* text) {
    if (m_search[0] != 0) {
        m_pendingSection = text;   // printed only when a row under it matches
        return;
    }
    SectionHeader(text, ImGui::GetContentRegionAvail().x);
}

bool SettingsPanel::Row(const char* label, const char* description, When when, const std::string& defaultText,
                        bool changed, bool disabled, Preview preview) {
    const float s = Scale();
    if (m_search[0] != 0) {
        const std::string needle = Lower(m_search);
        if (Lower(label).find(needle) == std::string::npos && Lower(description).find(needle) == std::string::npos) return false;
        if (m_pendingCat >= 0) {
            ImGui::Dummy(ImVec2(0, 8.0f * s));
            const ImVec2 p = ImGui::GetCursorScreenPos();
            Text(ImGui::GetWindowDrawList(), NobleStyle::TitleFont(), 20.0f * s, p, kText, kCats[m_pendingCat].name);
            ImGui::Dummy(ImVec2(0, 24.0f * s));
            m_pendingCat = -1;
        }
        if (m_pendingSection) {
            SectionHeader(m_pendingSection, ImGui::GetContentRegionAvail().x);
            m_pendingSection = nullptr;
        }
    }
    const int index = m_rowCount++;
    m_curRow = index;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = 40.0f * s;
    const ImVec2 q(p.x + w, p.y + h);
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(p, q);
    if (hovered && ImGui::IsMouseClicked(0) && m_focus != index) {
        m_focus = index;
        MenuSounds::Scroll();
    }
    const bool focused = index == m_focus;
    if (focused && m_scrollToFocus) ImGui::SetScrollFromPosY(p.y - ImGui::GetWindowPos().y, 0.4f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    RowBackground(dl, p, q, focused, hovered);
    if (changed) ChangedDot(dl, ImVec2(p.x + 10.0f * s, p.y + h * 0.5f), s);
    Text(dl, NobleStyle::TitleFont(), 17.0f * s, ImVec2(p.x + 20.0f * s, p.y + h * 0.5f - 10.0f * s),
         disabled ? kTextOff : (focused ? kText : IM_COL32(190, 214, 232, 255)), label);
    ImGui::Dummy(ImVec2(w, h));

    m_valMin = ImVec2(p.x + w * 0.46f, p.y + 4.0f * s);
    m_valMax = ImVec2(q.x - 10.0f * s, q.y - 4.0f * s);
    m_rowFocused = focused;
    m_rowHovered = hovered;
    m_rowDisabled = disabled;

    Details d;
    d.set = true;
    d.title = label;
    d.effect = WhenText(static_cast<int>(when));
    d.text = description;
    if (!defaultText.empty()) d.facts = "DEFAULT   " + defaultText;
    d.preview = preview;
    if (hovered) m_hover = d;
    if (focused) m_focusDetails = d;
    return true;
}

bool SettingsPanel::SliderValue(float& v, float lo, float hi, float step, float def, const char* text) {
    const float s = Scale();
    bool changed = false;
    const ImVec2 save = ImGui::GetCursorScreenPos();
    const float textW = std::max(86.0f * s, TextWidth(nullptr, 17.0f * s, text) + 14.0f * s);
    const bool percent = std::strchr(text, '%') != nullptr && hi <= 1.0f;   // shown as 0-100%, stored as 0-1
    ImGui::SetCursorScreenPos(m_valMin);
    ImGui::PushID(m_curRow);
    ImGui::InvisibleButton("##slider", ImVec2(std::max(1.0f, m_valMax.x - m_valMin.x - textW), m_valMax.y - m_valMin.y));
    const bool active = ImGui::IsItemActive();
    // The value on the right is a box: click it and type a number (Enter to set, Esc to cancel).
    const ImVec2 box0(m_valMax.x - textW + 6.0f * s, m_valMin.y), box1 = m_valMax;
    ImGui::SetCursorScreenPos(box0);
    const bool typingHere = m_typeRow == m_curRow;
    if (!typingHere && ImGui::InvisibleButton("##value", ImVec2(box1.x - box0.x, box1.y - box0.y)) && !m_rowDisabled) {
        m_typeRow = m_curRow;
        m_typeFocus = true;
        m_focus = m_curRow;
        std::snprintf(m_typeBuf, sizeof(m_typeBuf), step >= 1.0f && !percent ? "%.0f" : "%.2f", percent ? v * 100.0f : v);
        MenuSounds::Click();
    }
    const bool valueHovered = !typingHere && ImGui::IsItemHovered() && !m_rowDisabled;
    if (m_rowFocused) m_enterAppliesHere = true;
    if (m_rowFocused && !m_rowDisabled) m_valueTookLeftRight = true;
    if (!m_rowDisabled) {
        if (active) {
            m_focus = m_curRow;
            const float t = std::clamp((ImGui::GetIO().MousePos.x - m_valMin.x) / std::max(1.0f, m_valMax.x - m_valMin.x - textW), 0.0f, 1.0f);
            float nv = lo + t * (hi - lo);
            if (step > 0.0f) nv = std::round(nv / step) * step;
            nv = std::clamp(nv, lo, hi);
            if (std::fabs(nv - v) > 1e-6f) { v = nv; changed = true; }
        }
        if (m_rowFocused && (m_keyLeft || m_keyRight)) {
            const float st = step > 0.0f ? step : (hi - lo) / 50.0f;
            const float nv = std::clamp(v + (m_keyRight ? st : -st), lo, hi);
            if (std::fabs(nv - v) > 1e-6f) { v = nv; changed = true; MenuSounds::Change(); }
        }
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawSlider(dl, m_valMin, m_valMax, (v - lo) / (hi - lo), def >= lo && def <= hi ? (def - lo) / (hi - lo) : -1.0f,
               typingHere ? "" : text, active || m_rowFocused, m_rowDisabled, textW);
    if (valueHovered) dl->AddRect(box0, box1, IM_COL32(170, 220, 255, 110), 2.0f * s, 0, 1.0f);
    if (typingHere) {
        ImGui::SetCursorScreenPos(ImVec2(box0.x, (box0.y + box1.y) * 0.5f - ImGui::GetFrameHeight() * 0.5f));
        ImGui::SetNextItemWidth(box1.x - box0.x);
        if (m_typeFocus) {
            ImGui::SetKeyboardFocusHere();
            m_typeFocus = false;
        }
        const bool enter = ImGui::InputText("##type", m_typeBuf, sizeof(m_typeBuf),
                                            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        const bool done = enter || ImGui::IsItemDeactivated();
        if (done) {
            if (!ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                char* end = nullptr;
                float nv = std::strtof(m_typeBuf, &end);
                if (end != m_typeBuf) {
                    if (percent) nv /= 100.0f;
                    // Typed values keep two decimals (whole numbers for whole-number sliders), inside the slider's range.
                    nv = step >= 1.0f ? std::round(nv) : std::round(nv * 100.0f) / 100.0f;
                    nv = std::clamp(nv, lo, hi);
                    if (std::fabs(nv - v) > 1e-6f) { v = nv; changed = true; }
                }
            }
            m_typeRow = -1;
        }
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(save);
    return changed;
}

int SettingsPanel::CycleValue(const char* value, int index, int count, bool wraps) {
    const float s = Scale();
    const ImVec2 save = ImGui::GetCursorScreenPos();
    const float aw = 30.0f * s;
    const float h = m_valMax.y - m_valMin.y;
    int change = 0;
    ImGui::PushID(m_curRow);
    ImGui::SetCursorScreenPos(m_valMin);
    if (ImGui::InvisibleButton("##l", ImVec2(aw, h))) change = -1;
    const bool hl = ImGui::IsItemHovered();
    ImGui::SetCursorScreenPos(ImVec2(m_valMax.x - aw, m_valMin.y));
    if (ImGui::InvisibleButton("##r", ImVec2(aw, h))) change = 1;
    const bool hr = ImGui::IsItemHovered();
    ImGui::PopID();
    if (m_rowFocused && !m_rowDisabled) m_valueTookLeftRight = true;
    if (m_rowFocused) {
        if (m_keyLeft) change = -1;
        if (m_keyRight || m_keyAccept) change = 1;
    }
    if (m_rowDisabled) change = 0;
    if (change && !wraps) {
        if ((change < 0 && index <= 0) || (change > 0 && index >= count - 1)) change = 0;
    }
    if (change) {
        m_focus = m_curRow;
        MenuSounds::Change();
    }
    DrawCycle(ImGui::GetWindowDrawList(), m_valMin, m_valMax, value, index, count, wraps, hl, hr, m_rowDisabled);
    ImGui::SetCursorScreenPos(save);
    return change;
}

bool SettingsPanel::ToggleValue(bool& v) {
    const ImVec2 save = ImGui::GetCursorScreenPos();
    ImGui::PushID(m_curRow);
    ImGui::SetCursorScreenPos(m_valMin);
    const bool clicked = ImGui::InvisibleButton("##toggle", ImVec2(m_valMax.x - m_valMin.x, m_valMax.y - m_valMin.y));
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    bool changed = false;
    if (m_rowFocused && !m_rowDisabled) m_valueTookLeftRight = true;
    if (!m_rowDisabled && (clicked || (m_rowFocused && (m_keyAccept || m_keyLeft || m_keyRight)))) {
        v = !v;
        changed = true;
        m_focus = m_curRow;
        MenuSounds::Change();
    }
    DrawToggle(ImGui::GetWindowDrawList(), m_valMin, m_valMax, v, hovered, m_rowDisabled);
    ImGui::SetCursorScreenPos(save);
    return changed;
}

void SettingsPanel::InfoValue(const char* text) {
    const float s = Scale();
    const float tw = TextWidth(nullptr, 17.0f * s, text);
    const float x = std::max(m_valMin.x, m_valMax.x - tw);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(m_valMin, m_valMax, true);
    Text(dl, nullptr, 17.0f * s, ImVec2(x, (m_valMin.y + m_valMax.y) * 0.5f - 9.0f * s), m_rowDisabled ? kTextOff : kTextDim, text);
    dl->PopClipRect();
}

// ---------------------------------------------------------------------------------------------
// Right column: details
// ---------------------------------------------------------------------------------------------

void SettingsPanel::RenderDetails(float width, float height) {
    const float s = Scale();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.04f, 0.06f, 0.55f));
    ImGui::BeginChild("##setdetails", ImVec2(width, height), ImGuiChildFlags_Borders);
    const Details& d = m_hover.set ? m_hover : m_focusDetails;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (!d.set) {
        Paragraph("Pick a setting to see what it does, when it takes effect and its default value.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }
    ImGui::Dummy(ImVec2(0, 4.0f * s));
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    Text(dl, NobleStyle::TitleFont(), 22.0f * s, p, kText, d.title.c_str(), w);
    ImGui::Dummy(ImVec2(0, 28.0f * s));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kAccent));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(d.effect.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8.0f * s));

    // Preview.
    const ImVec2 pv = ImGui::GetCursorScreenPos();
    switch (d.preview) {
    case Preview::Fov: {
        // Top-down view cone with the chosen angle.
        const float r = std::min(w * 0.42f, 110.0f * s);
        const ImVec2 eye(pv.x + w * 0.5f, pv.y + r + 6.0f * s);
        const float half = m_edit.fieldOfView * 0.5f * 3.14159265f / 180.0f;
        const float a0 = -1.5707963f - half, a1 = -1.5707963f + half;
        dl->PathLineTo(eye);
        dl->PathArcTo(eye, r, a0, a1, 32);
        dl->PathFillConvex(IM_COL32(110, 180, 225, 70));
        dl->AddLine(eye, ImVec2(eye.x + std::cos(a0) * r, eye.y + std::sin(a0) * r), kAccent, 1.5f);
        dl->AddLine(eye, ImVec2(eye.x + std::cos(a1) * r, eye.y + std::sin(a1) * r), kAccent, 1.5f);
        const float hd = 78.0f * 0.5f * 3.14159265f / 180.0f;   // Reach's own 78 degrees, dashed
        dl->AddLine(eye, ImVec2(eye.x + std::cos(-1.5707963f - hd) * r, eye.y + std::sin(-1.5707963f - hd) * r), IM_COL32(255, 255, 255, 70), 1.0f);
        dl->AddLine(eye, ImVec2(eye.x + std::cos(-1.5707963f + hd) * r, eye.y + std::sin(-1.5707963f + hd) * r), IM_COL32(255, 255, 255, 70), 1.0f);
        dl->AddCircleFilled(eye, 4.0f * s, kText);
        const std::string deg = Format("%.0f\xC2\xB0", m_edit.fieldOfView);
        Text(dl, NobleStyle::TitleFont(), 22.0f * s, ImVec2(eye.x - TextWidth(NobleStyle::TitleFont(), 22.0f * s, deg.c_str()) * 0.5f, eye.y + 6.0f * s), kText, deg.c_str());
        ImGui::Dummy(ImVec2(w, r + 44.0f * s));
        break;
    }
    case Preview::FrameLimit: {
        const std::string big = m_edit.fpsLimit == 0 ? std::string("NO LIMIT") : std::to_string(m_edit.fpsLimit) + " FPS";
        Text(dl, NobleStyle::TitleFont(), 28.0f * s, pv, kText, big.c_str());
        const float ms = m_edit.fpsLimit == 0 ? 0.0f : 1000.0f / static_cast<float>(m_edit.fpsLimit);
        const ImVec2 b0(pv.x, pv.y + 40.0f * s), b1(pv.x + w, pv.y + 52.0f * s);
        dl->AddRectFilled(b0, b1, IM_COL32(255, 255, 255, 25));
        if (ms > 0.0f) dl->AddRectFilled(b0, ImVec2(b0.x + w * std::min(ms / 100.0f, 1.0f), b1.y), kAccent);
        const std::string cap = ms > 0.0f ? Format("%.1f ms per frame (scale 0 - 100 ms)", ms) : std::string("As many frames as your PC can draw");
        Text(dl, NobleStyle::SmallFont(), 14.0f * s, ImVec2(pv.x, b1.y + 6.0f * s), kTextDim, cap.c_str());
        ImGui::Dummy(ImVec2(w, 82.0f * s));
        break;
    }
    case Preview::Volume: {
        const struct { const char* name; float v; } meters[] = {
            { "GAME", m_edit.volumeGame }, { "MUSIC", m_edit.volumeMusic }, { "VOICE / OTHER", m_edit.volumeOther } };
        float y = pv.y;
        for (const auto& m : meters) {
            Text(dl, NobleStyle::SmallFont(), 14.0f * s, ImVec2(pv.x, y), kTextDim, m.name);
            const ImVec2 b0(pv.x + 110.0f * s, y + 3.0f * s), b1(pv.x + w, y + 13.0f * s);
            dl->AddRectFilled(b0, b1, IM_COL32(255, 255, 255, 25));
            const int segs = 20;
            for (int i = 0; i < segs; ++i) {
                if ((i + 0.5f) / segs > m.v) break;
                const float x0 = b0.x + (b1.x - b0.x) * i / segs, x1 = b0.x + (b1.x - b0.x) * (i + 1) / segs - 2.0f;
                dl->AddRectFilled(ImVec2(x0, b0.y), ImVec2(x1, b1.y), i >= 17 ? kAmber : kAccent);
            }
            y += 24.0f * s;
        }
        ImGui::Dummy(ImVec2(w, y - pv.y + 6.0f * s));
        break;
    }
    case Preview::Background: {
        // The details panel already drew the title and the effect line; a preview starts at the cursor (pv).
        const std::string value = m_edit.menuBackground;
        // The picture the menu is actually using: it was chosen and uploaded at menu init (WarmMenuBackground),
        // so nothing decodes a texture here - doing that on the fallback draw thread hung the client.
        const UiTexture tex = UiArt::MenuBackground();
        const float bw = ImGui::GetContentRegionAvail().x;
        const float bh = bw * 9.0f / 16.0f;
        const ImVec2 a = ImGui::GetCursorScreenPos();
        const ImVec2 b(a.x + bw, a.y + bh);
        dl->AddRectFilled(a, b, IM_COL32(10, 15, 22, 255));
        if (tex) {
            UiArt::DrawCover(dl, tex, a, b);
        } else {
            const char* none = "NO PICTURE";
            const ImVec2 ts = ImGui::CalcTextSize(none);
            dl->AddText(ImVec2(a.x + (bw - ts.x) * 0.5f, a.y + (bh - ts.y) * 0.5f), IM_COL32(120, 135, 150, 200), none);
        }
        dl->AddRect(a, b, IM_COL32(150, 185, 215, 110), 0.0f, 0, 1.0f);
        ImGui::Dummy(ImVec2(bw, bh));
        ImGui::Dummy(ImVec2(0, 6.0f * Scale()));
        ImGui::PushStyleColor(ImGuiCol_Text, NobleStyle::TextMuted());
        if (value.rfind("file:", 0) == 0) ImGui::TextWrapped("Your picture: %s", value.substr(5).c_str());
        else if (value == "reach") ImGui::TextWrapped("A random Halo: Reach campaign scene from your MCC install.");
        else if (value == "noble") ImGui::TextWrapped("Noble's own background art.");
        else ImGui::TextWrapped("A random picture from your own files, Noble's art and Reach's scenes.");
        ImGui::TextWrapped("A new choice is used from the next start of Noble.");
        ImGui::PopStyleColor();
        break;
    }
    case Preview::Controller: {
        const int pad = ConnectedPad();
        const std::string line = pad >= 0 ? "Controller " + std::to_string(pad + 1) + " connected." : std::string("No controller found.");
        Text(dl, NobleStyle::TitleFont(), 18.0f * s, pv, pad >= 0 ? kAccent : kTextDim, line.c_str());
        ImGui::Dummy(ImVec2(w, 28.0f * s));
        if (UiTexture img = UiArt::NobleControllerImage()) {
            const ImVec2 i0 = ImGui::GetCursorScreenPos();
            const float iw = w, ih = iw * img.height / img.width;
            dl->AddImage(img.id, i0, ImVec2(i0.x + iw, i0.y + ih));
            ImGui::Dummy(ImVec2(iw, ih + 6.0f * s));
        }
        break;
    }
    case Preview::Gpu: {
        Text(dl, NobleStyle::SmallFont(), 14.0f * s, pv, kTextDim, "IN USE NOW");
        Text(dl, NobleStyle::TitleFont(), 18.0f * s, ImVec2(pv.x, pv.y + 18.0f * s), kText, GpuLine().c_str(), w);
        ImGui::Dummy(ImVec2(w, 64.0f * s));
        break;
    }
    default: break;
    }

    if (!d.text.empty()) {
        Paragraph(d.text.c_str(), IM_COL32(205, 214, 222, 255));
    }
    if (!d.facts.empty()) {
        ImGui::Dummy(ImVec2(0, 8.0f * s));
        const ImVec2 f = ImGui::GetCursorScreenPos();
        dl->AddLine(f, ImVec2(f.x + w, f.y), kRule, 1.0f);
        ImGui::Dummy(ImVec2(0, 6.0f * s));
        Paragraph(d.facts.c_str(), kTextDim);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------------------------
// Categories
// ---------------------------------------------------------------------------------------------

void SettingsPanel::RenderGraphics() {
    const SettingsData def;
    Section("DISPLAY");
    if (Row("GRAPHICS CARD", "The graphics card Windows gave Noble. Choosing another card here will come later.",
            When::Info, "", false, false, Preview::Gpu)) {
        InfoValue(GpuLine().c_str());
    }
    {
        // 0 = Windowed, 1 = Borderless, 2 = Fullscreen (exclusive; stored as borderless + exclusiveFullscreen).
        static const char* const kModeNames[] = { "WINDOWED", "BORDERLESS FULL SCREEN", "FULLSCREEN" };
        static const char* const kModeDefaults[] = { "Windowed", "Borderless", "Fullscreen" };
        auto modeOf = [](const SettingsData& d) { return d.exclusiveFullscreen ? 2 : (d.borderless ? 1 : 0); };
        const int mode = modeOf(m_edit);
        if (Row("DISPLAY MODE", "Windowed: a fixed-size window you can move. Borderless: covers the whole display. Fullscreen: takes over the display (exclusive) for the lowest latency and the display's highest refresh rate; Alt-Tab drops back to the desktop and returning re-enters it. Switching applies immediately; the renderer keeps using the display's native resolution.",
                When::Apply, kModeDefaults[modeOf(def)], mode != modeOf(m_saved))) {
            if (const int d = CycleValue(kModeNames[mode], mode, 3)) {
                const int next = (mode + d + 3) % 3;
                m_edit.borderless = next >= 1;
                m_edit.exclusiveFullscreen = next == 2;
            }
        }
    }
    {
        int current = 0;
        const int n = static_cast<int>(std::size(kResolutions));
        for (int i = 0; i < n; ++i) if (kResolutions[i].w == m_edit.width && kResolutions[i].h == m_edit.height) current = i;
        char label[32];
        if (m_edit.borderless) std::snprintf(label, sizeof(label), "Fills your display");
        else std::snprintf(label, sizeof(label), "%d x %d", m_edit.width, m_edit.height);
        const bool changed = m_edit.width != m_saved.width || m_edit.height != m_saved.height;
        if (Row("WINDOW SIZE", "Size of the game window in windowed mode. Rendering stays at the display's native resolution and is scaled into the window, so it is crisp even at 720p. Borderless and Fullscreen fill the display, so this is greyed out.",
                When::Apply, "1280 x 720", changed, m_edit.borderless)) {
            if (const int d = CycleValue(label, current, n, false)) {
                current = std::clamp(current + d, 0, n - 1);
                m_edit.width = kResolutions[current].w;
                m_edit.height = kResolutions[current].h;
            }
        }
    }
    Section("FRAME RATE");
    if (Row("VERTICAL SYNC", "Waits for your display before showing each frame: no tearing, but a little more input delay.",
            When::Apply, def.verticalSync ? "On" : "Off", m_edit.verticalSync != m_saved.verticalSync)) {
        ToggleValue(m_edit.verticalSync);
    }
    {
        const int n = static_cast<int>(std::size(kFpsLimits));
        int current = n - 1;
        for (int i = 0; i < n; ++i) if (kFpsLimits[i] == m_edit.fpsLimit) current = i;
        const std::string label = m_edit.fpsLimit == 0 ? std::string("NO LIMIT") : std::to_string(m_edit.fpsLimit) + " FPS";
        if (Row("FRAME-RATE LIMIT", "Highest frame rate Noble draws. No limit means exactly that: no pacing while the window is active. A limit at your display's refresh rate keeps the PC cooler and quieter.",
                When::Apply, "No limit", m_edit.fpsLimit != m_saved.fpsLimit, false, Preview::FrameLimit)) {
            if (const int d = CycleValue(label.c_str(), current, n, false)) m_edit.fpsLimit = kFpsLimits[std::clamp(current + d, 0, n - 1)];
        }
    }
    if (Row("THROTTLE WHEN UNFOCUSED", "While Noble is not the active window (alt-tabbed, or waiting in a lobby as a guest) the frame rate drops to 30 so a background game cannot peg the CPU. Turn it off if you stream or capture from another window and want the full rate.",
            When::Apply, "On", m_edit.throttleUnfocused != m_saved.throttleUnfocused)) {
        ToggleValue(m_edit.throttleUnfocused);
    }
    if (Row("FPS COUNTER", "Shows the frame rate in the upper-right corner while you play.",
            When::Apply, "Off", m_edit.showFps != m_saved.showFps)) {
        ToggleValue(m_edit.showFps);
    }
    Section("POST-PROCESSING");
    // These two are real: Noble owns a full-screen pass over the frame the engine drew, so it can brighten it and
    // anti-alias it. Everything below is the engine's own and cannot be changed from here.
    if (Row("BRIGHTNESS", "Brightens or darkens the frame the engine draws, like a monitor's brightness control but without washing out the colours. 100 % leaves it exactly as Reach renders it.",
            When::Apply, "100 %", std::fabs(m_edit.brightness - m_saved.brightness) > 1e-4f, false, Preview::Volume)) {
        SliderValue(m_edit.brightness, 0.7f, 1.4f, 0.01f, 1.0f, Percent(m_edit.brightness).c_str());
    }
    if (Row("ANTI-ALIASING", "FXAA in Noble's own pass: smooths the stair-stepping on edges in the 3D scene. The menus are drawn afterwards, so text stays sharp. It is a cheap post-process, not MSAA.",
            When::Apply, "Off", m_edit.fxaa != m_saved.fxaa)) {
        ToggleValue(m_edit.fxaa);
    }
    Section("REACH'S OWN SETTINGS");
    Paragraph("Texture filtering, shadows, motion blur and draw distance are handled inside Halo: Reach itself. Noble has no "
              "handle on them yet, so they are not offered here rather than shown as switches that do nothing. Set them in MCC's "
              "own video options for now.");
}

void SettingsPanel::RenderAudio() {
    const SettingsData def;
    Section("VOLUME");
    auto volume = [&](const char* label, const char* text, float& v, float saved) {
        if (Row(label, text, When::Apply, "100 %", std::fabs(v - saved) > 1e-4f, false, Preview::Volume)) {
            if (SliderValue(v, 0.0f, 1.0f, 0.01f, 1.0f, Percent(v).c_str())) {
                ProcessAudio::ApplyAll(m_edit.volumeGame, m_edit.volumeMusic, m_edit.volumeOther, m_edit.masterMute);
            }
        }
    };
    volume("GAME", "Gunfire, vehicles, footsteps and everything else in the world.", m_edit.volumeGame, m_saved.volumeGame);
    volume("MUSIC", "Halo: Reach's score in game, and the menu music.", m_edit.volumeMusic, m_saved.volumeMusic);
    volume("VOICE / OTHER", "Dialogue, radio chatter, announcer and menu sounds.", m_edit.volumeOther, m_saved.volumeOther);
    Section("MENUS");
    if (Row("MENU SOUNDS", "Reach's own clicks and swooshes when you move through the menus.",
            When::Apply, "On", m_edit.menuSounds != m_saved.menuSounds)) {
        ToggleValue(m_edit.menuSounds);
    }
    if (Row("MENU MUSIC", "The song the menus play. Pick one in the list below: it plays at once; APPLY CHANGES keeps it, DISCARD goes back. Also plays in the pre-game lobby.",
            When::Instant, "Halo: Reach - Main Menu Theme", m_edit.menuMusicTrack != m_saved.menuMusicTrack)) {
        InfoValue(ReachAudio::TrackLabel(m_edit.menuMusicTrack).c_str());
    }
    if (m_search[0] == 0) RenderMusicPicker();
    ReachAudio::SetMusicVolume(m_edit.volumeMusic);
    const std::string problem = ReachAudio::Problem();
    if (!problem.empty() && m_search[0] == 0) {
        ImGui::Dummy(ImVec2(0, 4));
        Paragraph(("Reach menu audio is off: " + problem).c_str());
    }
    Section("VOICE CHAT");
    if (Row("VOICE CHAT", "Hear and talk to other players in online lobbies and matches. When it's on, the microphone stream opens in those sessions, but nothing is sent unless you talk with the settings below. Off closes both streams.",
            When::Apply, "On", m_edit.voiceEnabled != m_saved.voiceEnabled)) {
        ToggleValue(m_edit.voiceEnabled);
    }
    auto device = [&](const char* label, const char* text, bool capture, std::string& id, const std::string& saved) {
        const auto devices = VoiceChat::Devices(capture);
        int index = 0;
        for (size_t i = 0; i < devices.size(); ++i) if (devices[i].id == id) index = static_cast<int>(i) + 1;
        const int count = static_cast<int>(devices.size()) + 1;
        if (Row(label, text, When::Apply, "System default", id != saved)) {
            const std::string name = index == 0 ? std::string("System default") : devices[static_cast<size_t>(index - 1)].name;
            if (const int change = CycleValue(name.c_str(), index, count, true)) {
                index = (index + change + count) % count;
                id = index == 0 ? std::string() : devices[static_cast<size_t>(index - 1)].id;
            }
        }
    };
    device("MICROPHONE", "Which input other players hear.", true, m_edit.voiceInputDevice, m_saved.voiceInputDevice);
    device("VOICE OUTPUT", "Where other players' voices play. It's separate from the game's sound.", false, m_edit.voiceOutputDevice, m_saved.voiceOutputDevice);
    if (Row("VOICE VOLUME", "How loud other players are. 1.00x is as sent; up to 2.00x. MASTER MUTE silences this too.",
            When::Apply, "1.00x", std::fabs(m_edit.voiceVolume - m_saved.voiceVolume) > 1e-4f)) {
        char volumeText[16];
        std::snprintf(volumeText, sizeof(volumeText), "%.2fx", m_edit.voiceVolume);
        SliderValue(m_edit.voiceVolume, 0.0f, 2.0f, 0.05f, 1.0f, volumeText);
    }
    if (Row("OPEN MIC", "Off: you talk while holding push to talk. On: you also talk whenever your microphone is louder than -40 dB, without a button.",
            When::Apply, "Off", m_edit.voiceOpenMic != m_saved.voiceOpenMic)) {
        ToggleValue(m_edit.voiceOpenMic);
    }
    {
        // Either one talks. These are the same bindings as the KEYBOARD & MOUSE and CONTROLLER lists.
        constexpr int kPttKey = KeyBindings::kActionPushToTalk;
        const auto keyOf = [kPttKey](const SettingsData& d) {
            const auto it = d.keyOverrides.find(kPttKey);
            return it != d.keyOverrides.end() ? it->second : KeyBindings::DefaultKey(kPttKey);
        };
        if (Row("PUSH TO TALK: KEY OR MOUSE", "Click (or press Enter), then press any key or mouse button, including side buttons. Esc cancels, right-click unbinds, Delete puts V back. It can share a key with a game action, and then that action talks too.",
                When::Apply, KeyBindings::KeyName(KeyBindings::DefaultKey(kPttKey)), keyOf(m_edit) != keyOf(m_saved))) {
            KeyBindValue(kPttKey);
        }
        const int layout = std::clamp(m_edit.padLayout, 0, static_cast<int>(PadLayouts::Count) - 1);
        const bool padChanged = PadLayouts::EffectiveInput(layout, PadLayouts::PushToTalk, &m_edit.padBindings) !=
                                PadLayouts::EffectiveInput(layout, PadLayouts::PushToTalk, &m_saved.padBindings);
        if (Row("PUSH TO TALK: CONTROLLER", "Press A or click, then press the button. If a game action already uses it, that action moves to this row's old button. Wait 5 seconds to cancel; on a keyboard, Esc cancels and Delete puts the preset's button back.",
                When::Apply, PadLayouts::InputName(PadLayouts::LayoutInput(layout, PadLayouts::PushToTalk)), padChanged)) {
            PadBindValue(PadLayouts::PushToTalk);
        }
    }
    if (Row("VOICE QUALITY", "16, 20 or 24 kbps. Higher sounds clearer and uses a little more of your upload.",
            When::Apply, "20 kbps", m_edit.voiceBitrateKbps != m_saved.voiceBitrateKbps)) {
        static const int kRates[] = { 16, 20, 24 };
        const int index = m_edit.voiceBitrateKbps == 16 ? 0 : m_edit.voiceBitrateKbps == 24 ? 2 : 1;
        const std::string name = std::to_string(kRates[index]) + " kbps";
        if (const int change = CycleValue(name.c_str(), index, 3, false)) m_edit.voiceBitrateKbps = kRates[index + change];
    }
    if (Row("TALK TO", "Who hears you. TEAM reaches your own team once your team is known, and everyone until then. PARTY is the same as ALL for now, because Noble has no parties yet.",
            When::Apply, "All", m_edit.voiceChannel != m_saved.voiceChannel)) {
        static const char* const kChannels[] = { "All", "Team", "Party" };
        const int index = std::clamp(m_edit.voiceChannel, 0, 2);
        if (const int change = CycleValue(kChannels[index], index, 3, true)) m_edit.voiceChannel = (index + change + 3) % 3;
    }
    Section("MORE");
    if (Row("MASTER MUTE", "Silences Noble's own audio: the menu music and the menu clicks. The volumes below are kept, so turning it back off restores them. (Reach's in-game sound is the engine's own and has its own settings.)",
            When::Instant, "Off", m_edit.masterMute != m_saved.masterMute)) {
        if (ToggleValue(m_edit.masterMute)) {
            ProcessAudio::ApplyAll(m_edit.volumeGame, m_edit.volumeMusic, m_edit.volumeOther, m_edit.masterMute);
        }
    }
    if (Row("MUTE IN BACKGROUND", "Also silences Noble's audio whenever Noble is not the active window, so alt-tabbing to a video or a call leaves the game quiet.",
            When::Instant, "Off", m_edit.muteInBackground != m_saved.muteInBackground)) {
        ToggleValue(m_edit.muteInBackground);
    }
}

void SettingsPanel::RenderGame() {
    Section("CAMERA");
    if (Row("FIELD OF VIEW", "How wide you see, in horizontal degrees (70 - 120). Halo: Reach's own value is 78 (the faint lines in the picture). Also changes mid-match from the pause menu.",
            When::Apply, "78\xC2\xB0", std::fabs(m_edit.fieldOfView - m_saved.fieldOfView) > 0.01f, false, Preview::Fov)) {
        SliderValue(m_edit.fieldOfView, 70.0f, 120.0f, 1.0f, 78.0f, Format("%.0f\xC2\xB0", m_edit.fieldOfView).c_str());
    }
    const auto fovModText = [](float v) -> std::string {
        if (std::fabs(v) < 0.005f) return "0 %";
        return v < 0.0f ? Format("IN %.0f %%", std::fabs(v) * 100.0f) : Format("OUT %.0f %%", v * 100.0f);
    };
    if (Row("1ST-PERSON FOV MODIFIER (ON FOOT)", "Scales your 1st-person field of view on foot up to 120 % in (narrower) or 120 % out (wider). 0 % keeps your base Field of View.",
            When::Apply, "0 %", std::fabs(m_edit.firstPersonFovMod - m_saved.firstPersonFovMod) > 1e-4f, false)) {
        SliderValue(m_edit.firstPersonFovMod, -1.20f, 1.20f, 0.05f, 0.00f, fovModText(m_edit.firstPersonFovMod).c_str());
    }
    if (Row("1ST-PERSON FOV MODIFIER (VEHICLE)", "Scales your 1st-person field of view while in a vehicle up to 120 % in (narrower) or 120 % out (wider). 0 % keeps your base Field of View.",
            When::Apply, "0 %", std::fabs(m_edit.vehicleFirstPersonFovMod - m_saved.vehicleFirstPersonFovMod) > 1e-4f, false)) {
        SliderValue(m_edit.vehicleFirstPersonFovMod, -1.20f, 1.20f, 0.05f, 0.00f, fovModText(m_edit.vehicleFirstPersonFovMod).c_str());
    }
    if (Row("THIRD-PERSON ZOOM", "How much the camera zooms in when aiming a weapon with right click in third person (F5). 50 % is the default zoom.",
            When::Apply, "50 %", std::fabs(m_edit.thirdPersonZoom - m_saved.thirdPersonZoom) > 1e-4f, false)) {
        SliderValue(m_edit.thirdPersonZoom, 0.10f, 1.00f, 0.05f, 0.50f, Percent(m_edit.thirdPersonZoom).c_str());
    }
    const auto horizOffsetText = [](float v) -> std::string {
        if (std::fabs(v) < 0.005f) return "0 %";
        return v < 0.0f ? Format("LEFT %.0f %%", std::fabs(v) * 100.0f) : Format("RIGHT %.0f %%", v * 100.0f);
    };
    const auto vertOffsetText = [](float v) -> std::string {
        if (std::fabs(v) < 0.005f) return "0 %";
        return v < 0.0f ? Format("DOWN %.0f %%", std::fabs(v) * 100.0f) : Format("UP %.0f %%", v * 100.0f);
    };
    if (Row("1ST-PERSON VEHICLE HORIZONTAL OFFSET", "Shifts the 1st-person vehicle camera left or right relative to your character's head. Negative values move left, positive values move right; 0 % keeps the default head placement.",
            When::Apply, "0 %", std::fabs(m_edit.vehicleFirstPersonOffsetX - m_saved.vehicleFirstPersonOffsetX) > 1e-4f, false)) {
        SliderValue(m_edit.vehicleFirstPersonOffsetX, -1.00f, 1.00f, 0.05f, 0.00f, horizOffsetText(m_edit.vehicleFirstPersonOffsetX).c_str());
    }
    if (Row("1ST-PERSON VEHICLE VERTICAL OFFSET", "Shifts the 1st-person vehicle camera up or down relative to your character's head. Negative values move down, positive values move up; 0 % keeps the default head placement.",
            When::Apply, "0 %", std::fabs(m_edit.vehicleFirstPersonOffsetY - m_saved.vehicleFirstPersonOffsetY) > 1e-4f, false)) {
        SliderValue(m_edit.vehicleFirstPersonOffsetY, -1.00f, 1.00f, 0.05f, 0.00f, vertOffsetText(m_edit.vehicleFirstPersonOffsetY).c_str());
    }
    if (Row("THIRD-PERSON HORIZONTAL OFFSET", "Shifts the third-person camera left or right relative to your character (F5). Negative values move left, positive values move right; 0 % keeps Reach's default.",
            When::Apply, "0 %", std::fabs(m_edit.thirdPersonOffsetX - m_saved.thirdPersonOffsetX) > 1e-4f, false)) {
        SliderValue(m_edit.thirdPersonOffsetX, -1.00f, 1.00f, 0.05f, 0.00f, horizOffsetText(m_edit.thirdPersonOffsetX).c_str());
    }
    if (Row("THIRD-PERSON VERTICAL OFFSET", "Shifts the third-person camera up or down relative to your character (F5). Negative values move down, positive values move up; 0 % keeps Reach's default.",
            When::Apply, "0 %", std::fabs(m_edit.thirdPersonOffsetY - m_saved.thirdPersonOffsetY) > 1e-4f, false)) {
        SliderValue(m_edit.thirdPersonOffsetY, -1.00f, 1.00f, 0.05f, 0.00f, vertOffsetText(m_edit.thirdPersonOffsetY).c_str());
    }
    if (Row("IN-VEHICLE FOV", "A separate field of view while driving or flying. Needs the vehicle camera to be found in the engine first.",
            When::Info, "", false, true)) {
        InfoValue("Not available yet");
    }
    Section("HUD");
    if (Row("SUBTITLES", "Subtitles for cutscenes and radio dialogue.", When::Apply, "On", m_edit.subtitles != m_saved.subtitles)) {
        ToggleValue(m_edit.subtitles);
    }
    if (Row("HIDE HUD", "Turns the whole HUD off, for screenshots and videos. Needs HUD research first.", When::Info, "", false, true)) {
        InfoValue("Not available yet");
    }
    Section("MENUS");
    // Menu background: the three pools plus every picture the player has (their own files are "file:<name>"),
    // with the chosen picture previewed in the details column and an IMPORT button next to it.
    static const char* const kPoolValues[] = { "random", "noble", "reach" };
    static const char* const kPoolLabels[] = { "RANDOM", "NOBLE ART", "HALO: REACH (MCC)" };
    const std::vector<std::string> ownFiles = UiArt::MenuBackgroundFiles();
    std::vector<std::string> values(kPoolValues, kPoolValues + 3);
    std::vector<std::string> labels(kPoolLabels, kPoolLabels + 3);
    for (const std::string& f : ownFiles) {
        values.push_back("file:" + f);
        labels.push_back(f);
    }
    int choice = 0;
    for (size_t i = 0; i < values.size(); ++i) if (m_edit.menuBackground == values[i]) choice = static_cast<int>(i);
    if (Row("MENU BACKGROUND", "The picture behind every menu. Pick one of the pools or one of your own images (data\\ui\\custom\\backgrounds); IMPORT adds a new one.",
            When::Restart, "Random", m_edit.menuBackground != m_saved.menuBackground, false, Preview::Background)) {
        if (const int d = CycleValue(labels[static_cast<size_t>(choice)].c_str(), choice, static_cast<int>(values.size()))) {
            m_edit.menuBackground = values[static_cast<size_t>((choice + d + static_cast<int>(values.size())) % static_cast<int>(values.size()))];
        }
    }
    // Action row: Row() is a layout helper that always returns true, so the file dialog must be opened from
    // an explicit click on the row (or Accept while it is focused), not from the return value. Opening it every
    // frame hung the client, because the dialog waits for input (2026-10-30).
    const std::string ownCount = ownFiles.empty() ? std::string("No own pictures")
                                                  : std::to_string(ownFiles.size()) + " own picture(s)";
    if (Row("IMPORT BACKGROUND...", "Pick an image (PNG, JPG, BMP or WebP). It is copied into data\\ui\\custom\\backgrounds and selected here; the menu uses it from the next start.",
            When::Restart, ownCount.c_str(), false)) {
        InfoValue(ownFiles.empty() ? "No own pictures" : ownCount.c_str());
        const bool clicked = m_rowHovered && ImGui::IsMouseClicked(0);
        const bool accepted = m_rowFocused && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                               ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false));
        if (clicked || accepted) {
            wchar_t file[MAX_PATH] = {};
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = GetActiveWindow();
            ofn.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.webp\0All files\0*.*\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameW(&ofn)) {
                std::string name;
                if (UiArt::ImportMenuBackgroundImage(std::filesystem::path(file), name)) {
                    m_edit.menuBackground = "file:" + name;
                    MenuSounds::Click();
                } else {
                    MenuSounds::Error();
                }
            }
        }
    }
}

void SettingsPanel::RenderPlayer() {
    const float s = Scale();
    Section("IDENTITY");
    auto textRow = [&](const char* label, const char* text, const char* def, bool changed, char* buf, size_t size, ImGuiInputTextFlags flags) {
        if (!Row(label, text, When::Apply, def, changed)) return;
        const ImVec2 save = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(m_valMin.x, (m_valMin.y + m_valMax.y) * 0.5f - ImGui::GetFrameHeight() * 0.5f));
        ImGui::SetNextItemWidth(m_valMax.x - m_valMin.x);
        ImGui::PushID(m_curRow);
        if (m_rowFocused && m_keyAccept) ImGui::SetKeyboardFocusHere();
        ImGui::InputText("##text", buf, size, flags);
        if (ImGui::IsItemActive()) m_focus = m_curRow;
        ImGui::PopID();
        ImGui::SetCursorScreenPos(save);
        (void)s;
    };
    textRow("PLAYER NAME", "Your name on scoreboards, in the party box and (later) in chat. Up to 20 characters.",
            "NobleSix", m_edit.playerName != m_saved.playerName, m_nameBuf, 21, ImGuiInputTextFlags_None);
    textRow("SERVICE TAG", "Up to 4 letters or digits, shown next to your name in game (Reach's service tag).",
            "NOBL", m_edit.serviceTag != m_saved.serviceTag, m_tagBuf, 5, ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
}

void SettingsPanel::PollRebind() {
    if (m_rebindAction < 0) return;
    // Wait a moment so the click that started the rebind isn't taken as the new binding.
    if (ImGui::GetTime() - m_rebindStarted < 0.25) return;
    // A controller has no Esc: waiting too long cancels, so someone without a keyboard to hand can't get stuck here.
    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) || ImGui::GetTime() - m_rebindStarted > 5.0) {
        m_rebindAction = -1;
        return;
    }
    for (int vk = 1; vk < 256; ++vk) {
        if (vk == VK_ESCAPE || vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) continue;  // use the L/R variants
        if (GetAsyncKeyState(vk) & 0x8000) {
            if (vk == KeyBindings::DefaultKey(m_rebindAction)) m_edit.keyOverrides.erase(m_rebindAction);
            else m_edit.keyOverrides[m_rebindAction] = vk;
            m_rebindAction = -1;
            Settings::Get().Update(m_edit);
            m_saved = Settings::Get().Snapshot();
            MenuSounds::Click();
            return;
        }
    }
}

// The value side of a key-binding row. PollRebind takes the press, keyboard or mouse button alike.
void SettingsPanel::KeyBindValue(int action) {
    const float s = Scale();
    const int def = KeyBindings::DefaultKey(action);
    auto it = m_edit.keyOverrides.find(action);
    const int current = it != m_edit.keyOverrides.end() ? it->second : def;
    std::string keyText = m_rebindAction == action ? std::string("PRESS A KEY...")
                        : current == 0 ? std::string("\xE2\x80\x94") : KeyBindings::KeyName(current);
    const ImVec2 save = ImGui::GetCursorScreenPos();
    const float bw = std::min(220.0f * s, m_valMax.x - m_valMin.x);
    const ImVec2 b0(m_valMax.x - bw, m_valMin.y), b1(m_valMax.x, m_valMax.y);
    ImGui::SetCursorScreenPos(b0);
    ImGui::PushID(m_curRow);
    const bool clicked = ImGui::InvisibleButton("##key", ImVec2(bw, b1.y - b0.y), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const bool right = hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right);
    ImGui::PopID();
    if (right) {
        if (def == 0) m_edit.keyOverrides.erase(action);
        else m_edit.keyOverrides[action] = 0;
        Settings::Get().Update(m_edit);
        m_saved = Settings::Get().Snapshot();
    } else if ((clicked && !ImGui::IsMouseReleased(ImGuiMouseButton_Right)) || (m_rowFocused && m_keyAccept)) {
        m_rebindAction = action;
        m_rebindStarted = ImGui::GetTime();
        m_focus = m_curRow;
    }
    if (m_rowFocused && m_navAllowed && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) { m_edit.keyOverrides.erase(action); Settings::Get().Update(m_edit); m_saved = Settings::Get().Snapshot(); }
    DrawBox(ImGui::GetWindowDrawList(), b0, b1, keyText.c_str(), hovered, current != def || m_rebindAction == action, false);
    ImGui::SetCursorScreenPos(save);
}

void SettingsPanel::RenderKeyboardMouse() {
    Section("MOUSE");
    if (Row("LOOK SENSITIVITY", "How far you turn for each bit of mouse movement. 1.00x is Noble's default.",
            When::Apply, "1.00x", std::fabs(m_edit.mouseSensitivity - m_saved.mouseSensitivity) > 1e-4f)) {
        SliderValue(m_edit.mouseSensitivity, 0.1f, 5.0f, 0.05f, 1.0f, Format("%.2fx", m_edit.mouseSensitivity).c_str());
    }
    if (Row("VERTICAL SENSITIVITY", "Up/down speed compared with left/right. Below 1 makes aiming up and down calmer.",
            When::Apply, "1.00x", std::fabs(m_edit.mouseSensitivityY - m_saved.mouseSensitivityY) > 1e-4f)) {
        SliderValue(m_edit.mouseSensitivityY, 0.25f, 4.0f, 0.05f, 1.0f, Format("%.2fx", m_edit.mouseSensitivityY).c_str());
    }
    if (Row("INVERT LOOK", "Inverted: moving the mouse forward looks down, as in flight controls.",
            When::Apply, "Off", m_edit.invertLook != m_saved.invertLook)) {
        ToggleValue(m_edit.invertLook);
    }
    if (Row("MOUSE ACCELERATION", "Fast flicks turn further than slow movements, like most shooters' default. 0 % is raw input, which is what the best players use for consistency.",
            When::Apply, "Off", std::fabs(m_edit.mouseAcceleration - m_saved.mouseAcceleration) > 1e-4f)) {
        SliderValue(m_edit.mouseAcceleration, 0.0f, 1.0f, 0.01f, 0.0f,
                    m_edit.mouseAcceleration < 0.001f ? "Off" : Percent(m_edit.mouseAcceleration).c_str());
    }
    if (Row("MOUSE SMOOTHING", "Averages the mouse's movement over a few frames. It can steady a shaky hand, but it adds latency, so Off is the default.",
            When::Apply, "Off", std::fabs(m_edit.mouseSmoothing - m_saved.mouseSmoothing) > 1e-4f)) {
        SliderValue(m_edit.mouseSmoothing, 0.0f, 0.9f, 0.05f, 0.0f,
                    m_edit.mouseSmoothing < 0.001f ? "Off" : Percent(m_edit.mouseSmoothing).c_str());
    }
    Section("MOUSE WHEEL");
    auto wheelName = [](int action) -> std::string {
        if (action < 0) return "NOTHING";
        const char* n = KeyBindings::ActionName(action);
        return n ? Upper(n) : "SLOT " + std::to_string(action);
    };
    auto wheelRow = [&](const char* label, int& action, int saved) {
        if (!Row(label, "The action the mouse wheel triggers. With both directions on CHANGE WEAPON (the default) either way switches weapons.",
                 When::Apply, "Change weapon", action != saved)) return;
        const int n = static_cast<int>(std::size(kWheelActions));
        int idx = 0;
        for (int i = 0; i < n; ++i) if (kWheelActions[i] == action) idx = i;
        if (const int d = CycleValue(wheelName(action).c_str(), idx, n)) { action = kWheelActions[(idx + d + n) % n]; Settings::Get().Update(m_edit); m_saved = Settings::Get().Snapshot(); }
    };
    wheelRow("WHEEL UP", m_edit.wheelUpAction, m_saved.wheelUpAction);
    wheelRow("WHEEL DOWN", m_edit.wheelDownAction, m_saved.wheelDownAction);

    using KeyBindings::ActionGroup;
    const ActionGroup groups[] = { ActionGroup::Movement, ActionGroup::Actions, ActionGroup::Vehicle, ActionGroup::Interface,
                                   ActionGroup::Forge, ActionGroup::Theater };
    // ActionGroup::Other (bindings MCC shares with its other Halo games) is not listed: Reach doesn't
    // use them. They still get their default keys in the profile.
    for (ActionGroup g : groups) {
        const std::vector<int> actions = KeyBindings::ActionsIn(g);
        if (actions.empty()) continue;
        Section(Upper(KeyBindings::GroupName(g)).c_str());
        for (int action : actions) {
            const int def = KeyBindings::DefaultKey(action);
            auto it = m_edit.keyOverrides.find(action);
            const int current = it != m_edit.keyOverrides.end() ? it->second : def;
            auto sit = m_saved.keyOverrides.find(action);
            const int saved = sit != m_saved.keyOverrides.end() ? sit->second : def;
            const char* name = KeyBindings::ActionName(action);
            const std::string label = name ? Upper(name) : "SLOT " + std::to_string(action);
            const std::string defText = def ? KeyBindings::KeyName(def) : std::string("-");
            if (!Row(label.c_str(), "Click (or press Enter), then press the key or mouse button you want. Esc cancels, right-click unbinds, Delete puts the default back.",
                     When::Apply, defText, current != saved)) continue;
            KeyBindValue(action);
            if (action == 9) {
                const SettingsData defData;
                if (Row("TOGGLE CROUCH", "On: press your Crouch key once to stay crouched, and press it again to stand up. Off: crouched only while you hold the Crouch key.",
                        When::Apply, defData.toggleCrouch ? "On" : "Off", m_edit.toggleCrouch != m_saved.toggleCrouch)) {
                    if (ToggleValue(m_edit.toggleCrouch)) {
                        Settings::Get().Update(m_edit);
                        m_saved = Settings::Get().Snapshot();
                    }
                }
            }
        }
    }
}

// The value side of a controller-binding row, and the capture of the next input once it's waiting for one.
void SettingsPanel::PadBindValue(int action) {
    struct KeyMap { ImGuiKey key; int input; };
    static const KeyMap kKeys[] = {
        { ImGuiKey_GamepadFaceDown, 0x1000 }, { ImGuiKey_GamepadFaceRight, 0x2000 },
        { ImGuiKey_GamepadFaceLeft, 0x4000 }, { ImGuiKey_GamepadFaceUp, 0x8000 },
        { ImGuiKey_GamepadL1, 0x0100 },       { ImGuiKey_GamepadR1, 0x0200 },
        { ImGuiKey_GamepadL2, PadLayouts::InputLT }, { ImGuiKey_GamepadR2, PadLayouts::InputRT },
        { ImGuiKey_GamepadL3, 0x0040 },       { ImGuiKey_GamepadR3, 0x0080 },
        { ImGuiKey_GamepadDpadUp, 0x0001 },   { ImGuiKey_GamepadDpadDown, 0x0002 },
        { ImGuiKey_GamepadDpadLeft, 0x0004 }, { ImGuiKey_GamepadDpadRight, 0x0008 },
        { ImGuiKey_GamepadStart, 0x0010 },    { ImGuiKey_GamepadBack, 0x0020 },
        { ImGuiKey_GamepadLStickUp, PadLayouts::InputLStickUp },     { ImGuiKey_GamepadLStickDown, PadLayouts::InputLStickDown },
        { ImGuiKey_GamepadLStickLeft, PadLayouts::InputLStickLeft }, { ImGuiKey_GamepadLStickRight, PadLayouts::InputLStickRight },
        { ImGuiKey_GamepadRStickUp, PadLayouts::InputRStickUp },     { ImGuiKey_GamepadRStickDown, PadLayouts::InputRStickDown },
        { ImGuiKey_GamepadRStickLeft, PadLayouts::InputRStickLeft }, { ImGuiKey_GamepadRStickRight, PadLayouts::InputRStickRight },
    };
    const int layout = std::clamp(m_edit.padLayout, 0, static_cast<int>(PadLayouts::Count) - 1);
    const std::string key = PadLayouts::ActionKey(action);
    const bool custom = m_edit.padBindings.count(key) != 0;
    const char* effective = PadLayouts::InputName(PadLayouts::EffectiveInput(layout, action, &m_edit.padBindings));
    const std::string value = m_padRebindAction == action ? std::string("PRESS AN INPUT...")
                            : custom ? std::string(effective) + " (custom)"
                                     : std::string(effective) + " (preset)";
    InfoValue(value.c_str());
    const bool clicked = m_rowHovered && ImGui::IsMouseClicked(0);
    if (clicked || (m_rowFocused && m_keyAccept)) {
        m_padRebindAction = action;
        m_padRebindStarted = ImGui::GetTime();
        m_padRebindFrame = ImGui::GetFrameCount();
        MenuSounds::Click();
    }
    if (m_padRebindAction != action || m_padRebindFrame == ImGui::GetFrameCount()) return;

    // Capturing: the next input becomes the binding.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::GetTime() - m_padRebindStarted > 5.0) {
        m_padRebindAction = -1;
    } else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
        m_edit.padBindings.erase(key);
        m_padRebindAction = -1;
        MenuSounds::Click();
    } else {
        for (const KeyMap& map : kKeys) {
            if (!ImGui::IsKeyPressed(map.key, false)) continue;
            PadLayouts::Assign(layout, action, map.input, m_edit.padBindings);
            m_padRebindAction = -1;
            MenuSounds::Click();
            break;
        }
        // A paddle press arrives as its key. The short wait keeps the click that opened this from counting.
        for (int n = 0; m_padRebindAction == action && n < PadLayouts::kPaddleCount && ImGui::GetTime() - m_padRebindStarted > 0.25; ++n) {
            const int vk = m_edit.padPaddleKeys[static_cast<size_t>(n)];
            if (vk <= 0 || (GetAsyncKeyState(vk) & 0x8000) == 0) continue;
            PadLayouts::Assign(layout, action, PadLayouts::InputPaddle1 + n, m_edit.padBindings);
            m_padRebindAction = -1;
            MenuSounds::Click();
        }
    }
}

void SettingsPanel::RenderController() {
    const SettingsData def;
    const float s = Scale();
    Section("STATUS");
    {
        const int pad = ConnectedPad();
        const std::string line = pad >= 0 ? "Controller " + std::to_string(pad + 1) + " connected" : std::string("No controller found");
        if (Row("CONTROLLER", "Xbox and other XInput controllers work as soon as Windows sees them. Noble switches between the controller and keyboard + mouse by whichever you used last; Start opens the pause menu.",
                When::Info, "", false, false, Preview::Controller)) {
            InfoValue(line.c_str());
        }
    }
    // One preset: Reach's six button layouts, each with the sticks normal or swapped. Stored as padLayout + padStickLayout.
    Section("CONTROLLER PRESET");
    {
        const int layouts = static_cast<int>(PadLayouts::Count);
        const int layout = std::clamp(m_edit.padLayout, 0, layouts - 1);
        const bool swapped = m_edit.padStickLayout == 1;
        const int preset = layout + (swapped ? layouts : 0);
        // Reach has no layouts called Reverse: these two names are Noble's, for that layout with the sticks swapped.
        const bool reverse = swapped && (layout == PadLayouts::Southpaw || layout == PadLayouts::BumperJumper);
        const std::string name = reverse ? std::string("REVERSE ") + PadLayouts::Name(layout)
                                         : std::string(PadLayouts::Name(layout)) + (swapped ? " + SWAPPED STICKS" : "");
        const std::string text = std::string(PadLayouts::Description(layout)) +
                                 (swapped ? " Sticks swapped: left stick looks, right stick moves." : " Left stick moves, right stick looks.") +
                                 (reverse ? " Noble's name for this preset; Reach's own list has no Reverse layouts." : "");
        if (Row("CONTROLLER PRESET", text.c_str(), When::Apply, "Default",
                m_edit.padLayout != m_saved.padLayout || m_edit.padStickLayout != m_saved.padStickLayout)) {
            if (const int d = CycleValue(name.c_str(), preset, layouts * 2)) {
                const int next = (preset + d + layouts * 2) % (layouts * 2);
                m_edit.padLayout = next % layouts;
                m_edit.padStickLayout = next >= layouts ? 1 : 0;
                m_edit.padBindings.clear();   // a binding made for the old preset would leave buttons unused or doubled up
            }
        }
        if (m_search[0] == 0) DrawControllerDiagram(ImGui::GetContentRegionAvail().x, layout, swapped, m_edit.padBindings);
    }

    // Per-action bindings: select a row, press A (or click), then press the input it should use. An override replaces the
    // preset's input for that action, and whichever action had that input takes the old one.
    Section("BUTTON BINDINGS");
    {
        const int layout = std::clamp(m_edit.padLayout, 0, static_cast<int>(PadLayouts::Count) - 1);

        for (int action = 0; action < PadLayouts::ActionCount; ++action) {
            const std::string key = PadLayouts::ActionKey(action);
            const bool custom = m_edit.padBindings.count(key) != 0;
            const char* layoutName = PadLayouts::InputName(PadLayouts::LayoutInput(layout, action));
            const std::string text = action == PadLayouts::TeamChat
                ? "Can be assigned, but does nothing yet: a controller can read chat but can't type in it."
                : "Press A or click, then press the input this action should use. The action on that input takes this one's old input. Wait 5 seconds to cancel; on a keyboard, Esc cancels and Delete puts the preset's input back.";

            if (!Row(PadLayouts::ActionName(action), text.c_str(), When::Apply, layoutName, custom)) continue;
            PadBindValue(action);
        }
        if (!m_edit.padBindings.empty()) {
            if (Row("RESET BUTTON BINDINGS", "Puts every action back on the input its preset gives it.",
                    When::Apply, "Preset", true)) {
                InfoValue("Preset");
                const bool clicked = m_rowHovered && ImGui::IsMouseClicked(0);
                if (clicked || (m_rowFocused && m_keyAccept)) {
                    m_edit.padBindings.clear();
                    m_padRebindAction = -1;
                    MenuSounds::Click();
                }
            }
        }
    }
    // Windows' XInput doesn't report paddles, so each one is a keyboard key or mouse button that a tool like Steam Input sends.
    // Noble then treats that key as the paddle, so it can be picked in the bindings above.
    Section("PADDLES");
    for (int n = 0; n < PadLayouts::kPaddleCount; ++n) {
        const std::string label = "PADDLE " + std::to_string(n + 1) + " KEY";
        int& vk = m_edit.padPaddleKeys[static_cast<size_t>(n)];
        const SettingsData defaults;
        if (!Row(label.c_str(), "Have your controller's software (Steam Input, for example) send this key when the paddle is pressed, then pick the paddle in BUTTON BINDINGS. While a controller is connected this key stops being a keyboard key in game. Press A or click, then press a key, or the middle or a side mouse button. Wait 5 seconds to cancel.",
                 When::Apply, KeyBindings::KeyName(defaults.padPaddleKeys[static_cast<size_t>(n)]), vk != m_saved.padPaddleKeys[static_cast<size_t>(n)])) {
            continue;
        }
        InfoValue(m_paddleKeyCapture == n ? "PRESS A KEY..." : vk > 0 ? KeyBindings::KeyName(vk).c_str() : "-");
        if (m_paddleKeyCapture < 0 && ((m_rowHovered && ImGui::IsMouseClicked(0)) || (m_rowFocused && m_keyAccept))) {
            m_paddleKeyCapture = n;
            m_paddleKeyStarted = ImGui::GetTime();
            MenuSounds::Click();
        } else if (m_paddleKeyCapture == n && ImGui::GetTime() - m_paddleKeyStarted > 0.25) {
            if (ImGui::GetTime() - m_paddleKeyStarted > 5.0 || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
                m_paddleKeyCapture = -1;
                continue;
            }
            for (int key = 3; key < 256; ++key) {   // from 3: left and right click stay the menu's and the game's
                if (key == VK_ESCAPE || key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU) continue;  // use the L/R variants
                if ((GetAsyncKeyState(key) & 0x8000) == 0) continue;
                for (int& other : m_edit.padPaddleKeys) {
                    if (other == key) other = 0;   // one key, one paddle
                }
                vk = key;
                m_paddleKeyCapture = -1;
                MenuSounds::Click();
                break;
            }
        }
    }
    if (Row("TOGGLE ZOOM", "On: press to zoom in, press again to zoom out. Off: zoomed only while you hold the button.", When::Apply,
            def.padToggleZoom ? "On" : "Off", m_edit.padToggleZoom != m_saved.padToggleZoom)) {
        ToggleValue(m_edit.padToggleZoom);
    }
    if (Row("TOGGLE CROUCH", "On: press the crouch button once to stay crouched, and press it again to stand up. Off: crouched only while you hold the button.", When::Apply,
            def.padToggleCrouch ? "On" : "Off", m_edit.padToggleCrouch != m_saved.padToggleCrouch)) {
        ToggleValue(m_edit.padToggleCrouch);
    }
    Section("STICKS");
    auto speed = [&](const char* label, const char* text, float& v, float saved) {
        if (!Row(label, text, When::Apply, "2.00x", std::fabs(v - saved) > 1e-4f)) return;
        SliderValue(v, 1.0f, 10.0f, 0.25f, 2.0f, Format("%.2fx", v).c_str());
    };
    speed("HORIZONTAL SENSITIVITY", "How fast the look stick turns you left and right. 2.00x is Reach's normal speed. Lower is slower, and 1.00x is about half speed; a change below 2.00x applies from the next match. Higher makes partial stick pushes turn faster, but a full push tops out at Reach's own maximum. Moves in 0.25x steps; click the number to type a value like 2.60.", m_edit.padLookSpeedX, m_saved.padLookSpeedX);
    speed("VERTICAL SENSITIVITY", "How fast the look stick looks up and down. Same scale as horizontal.", m_edit.padLookSpeedY, m_saved.padLookSpeedY);
    if (Row("INVERT LOOK", "Pushing the right stick forward looks down.", When::Apply, "Off", m_edit.padInvertLook != m_saved.padInvertLook)) {
        ToggleValue(m_edit.padInvertLook);
    }
    auto deadZone = [&](const char* label, float& v, float saved) {
        if (!Row(label, "Raise a dead zone if a worn stick drifts. 0 keeps the game's own dead zone.", When::Apply, "Game default",
                 std::fabs(v - saved) > 1e-4f)) return;
        float percent = v * 100.0f;
        const std::string text = percent < 0.5f ? std::string("Game default") : Format("%.0f %%", percent);
        if (SliderValue(percent, 0.0f, 50.0f, 1.0f, 0.0f, text.c_str())) v = percent / 100.0f;
    };
    deadZone("LEFT STICK DEAD ZONE", m_edit.padLeftDeadZone, m_saved.padLeftDeadZone);
    deadZone("RIGHT STICK DEAD ZONE", m_edit.padRightDeadZone, m_saved.padRightDeadZone);
    Section("VIBRATION");
    if (Row("VIBRATION", "Rumble from the game (explosions, vehicles, melee) on Xbox-style controllers.", When::Apply,
            def.padVibration ? "On" : "Off", m_edit.padVibration != m_saved.padVibration)) {
        ToggleValue(m_edit.padVibration);
    }
    if (Row("VIBRATION STRENGTH", "How strong the rumble is.", When::Apply, "100 %",
            std::fabs(m_edit.padVibrationStrength - m_saved.padVibrationStrength) > 1e-4f, !m_edit.padVibration)) {
        SliderValue(m_edit.padVibrationStrength, 0.0f, 1.0f, 0.05f, 1.0f, Percent(m_edit.padVibrationStrength).c_str());
    }
    (void)s;
}

void SettingsPanel::RenderOnline() {
    Section("INVITES");
    if (Row("INVITE NOTIFICATIONS", "Show a notification with a JOIN button when a friend invites you to a game, on any menu screen. Turn it off to see invites only in the COMMUNITY screen.",
            When::Apply, "On", m_edit.inviteNotifications != m_saved.inviteNotifications)) {
        ToggleValue(m_edit.inviteNotifications);
    }

    Section("SERVERS");
    if (!Row("MASTER SERVER", "The list of online games for the GAMES screen. Leave it empty to only see games on your LAN.",
             When::Apply, "Noble's test server", m_edit.masterServerUrl != m_saved.masterServerUrl)) return;
    const ImVec2 save = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(m_valMin.x, (m_valMin.y + m_valMax.y) * 0.5f - ImGui::GetFrameHeight() * 0.5f));
    ImGui::SetNextItemWidth(m_valMax.x - m_valMin.x);
    ImGui::PushID(m_curRow);
    if (m_rowFocused && m_keyAccept) ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##master", "empty = LAN only", m_masterBuf, sizeof(m_masterBuf));
    if (ImGui::IsItemActive()) m_focus = m_curRow;
    ImGui::PopID();
    ImGui::SetCursorScreenPos(save);
}

// Menu music picker: the games on the left, the chosen game's tracks on the right (or every game's
// tracks matching the search text). Clicking a track plays it straight away.
void SettingsPanel::RenderMusicPicker() {
    const float s = Scale();
    const std::vector<ReachAudio::TrackInfo> tracks = ReachAudio::MusicTracks();
    std::vector<std::string> games;
    std::vector<int> counts;
    for (const ReachAudio::TrackInfo& t : tracks) {
        if (t.game.empty()) continue;
        auto it = std::find(games.begin(), games.end(), t.game);
        if (it == games.end()) { games.push_back(t.game); counts.push_back(1); }
        else ++counts[it - games.begin()];
    }
    if (m_musicGame < 0) {
        m_musicGame = 0;
        for (const ReachAudio::TrackInfo& t : tracks) {
            if (t.id == m_edit.menuMusicTrack && !t.game.empty()) {
                m_musicGame = static_cast<int>(std::find(games.begin(), games.end(), t.game) - games.begin());
            }
        }
    }
    if (m_musicGame >= static_cast<int>(games.size())) m_musicGame = 0;
    // Old saved values ("shuffle", piece names) play Reach's menu theme: show that one as chosen.
    if (std::none_of(tracks.begin(), tracks.end(), [&](const ReachAudio::TrackInfo& t) { return t.id == m_edit.menuMusicTrack; }) &&
        std::any_of(tracks.begin(), tracks.end(), [](const ReachAudio::TrackInfo& t) { return t.id == "reach:menu"; })) {
        m_edit.menuMusicTrack = "reach:menu";
    }
    auto pick = [&](const std::string& id) {
        m_edit.menuMusicTrack = id;
        MenuSounds::Click();
        ReachAudio::PlayMenuMusic(id);
    };
    auto isOff = [&](const std::string& id) {
        return std::find(m_edit.menuMusicDisabled.begin(), m_edit.menuMusicDisabled.end(), id) != m_edit.menuMusicDisabled.end();
    };
    auto toggleTrack = [&](const std::string& id) {
        auto& list = m_edit.menuMusicDisabled;
        const auto it = std::find(list.begin(), list.end(), id);
        if (it == list.end()) list.push_back(id); else list.erase(it);
        MenuSounds::Click();
        // Shuffle mode is playing already: restart it so a track switched off stops coming up.
        if (m_edit.menuMusicTrack == "all") ReachAudio::PlayMenuMusic("all");
    };
    // The lists below are for the mouse. These rows reach the same choices with the D-pad and arrow keys.
    std::vector<const ReachAudio::TrackInfo*> choices;
    for (const ReachAudio::TrackInfo& t : tracks) {
        if (t.game.empty() || (!games.empty() && t.game == games[m_musicGame])) choices.push_back(&t);
    }
    if (!games.empty() && Row("BROWSE GAME", "Which game's tracks BROWSE TRACK steps through.", When::Info, "", false)) {
        const int n = static_cast<int>(games.size());
        const int change = CycleValue(games[m_musicGame].c_str(), 0, 1, true);
        if (change != 0) {
            m_musicGame = (m_musicGame + change + n) % n;
            m_musicSearch[0] = 0;
            choices.clear();
            for (const ReachAudio::TrackInfo& t : tracks) {
                if (t.game.empty() || t.game == games[m_musicGame]) choices.push_back(&t);
            }
        }
    }
    int current = -1;
    for (int i = 0; i < static_cast<int>(choices.size()); ++i) {
        if (choices[i]->id == m_edit.menuMusicTrack) current = i;
    }
    if (!choices.empty() && Row("BROWSE TRACK", "Steps through this game's tracks, plus Shuffle and Off. Each one plays at once.", When::Instant, "", false)) {
        const std::string shown = current >= 0 ? choices[current]->label : std::string("Press to choose");
        const int n = static_cast<int>(choices.size());
        const int change = CycleValue(shown.c_str(), 0, 1, true);
        if (change != 0) {
            current = current < 0 ? (change > 0 ? 0 : n - 1) : (current + change + n) % n;
            pick(choices[current]->id);
        }
    }
    const bool inGameTrack = current >= 0 && !choices[current]->game.empty();
    if (!choices.empty() && Row("TRACK IN SHUFFLE", "Whether SHUFFLE ALL GAMES can pick the track above.", When::Apply, "On", false, !inGameTrack)) {
        bool on = inGameTrack && !isOff(choices[current]->id);
        if (ToggleValue(on) && inGameTrack) toggleTrack(choices[current]->id);
    }

    // A tiny ON/OFF switch drawn at the right edge of a track row.
    auto trackSwitch = [&](const ReachAudio::TrackInfo& t) {
        const bool off = isOff(t.id);
        const float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine(std::max(0.0f, avail - 44.0f * s));
        ImGui::PushStyleColor(ImGuiCol_Text, off ? NobleStyle::TextMuted() : NobleStyle::Accent());
        char label[16];
        std::snprintf(label, sizeof(label), off ? "OFF" : "ON");
        if (ImGui::SmallButton(label)) toggleTrack(t.id);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(off ? "Switched off: SHUFFLE ALL GAMES skips it." : "On: SHUFFLE ALL GAMES can pick it.");
    };

    ImGui::Dummy(ImVec2(0, 4.0f * s));
    const float total = ImGui::GetContentRegionAvail().x;
    const float leftW = std::max(150.0f * s, total * 0.40f);
    const float rightW = std::max(150.0f * s, total - leftW - ImGui::GetStyle().ItemSpacing.x);
    const float height = 300.0f * s;
    ImGui::BeginChild("##musicgames", ImVec2(leftW, height), ImGuiChildFlags_Borders);
    for (const ReachAudio::TrackInfo& t : tracks) {
        if (!t.game.empty()) continue;   // "Shuffle all games", "Off"
        const bool selected = t.id == m_edit.menuMusicTrack;
        if (ImGui::Selectable(t.label.c_str(), selected)) pick(t.id);
        if (ImGui::IsItemHovered()) MenuSounds::Hover(ImGui::GetItemID());
    }
    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(games.size()); ++i) {
        int offCount = 0;
        for (const ReachAudio::TrackInfo& t : tracks) {
            if (t.game == games[i] && isOff(t.id)) ++offCount;
        }
        char label[160];
        if (offCount > 0) std::snprintf(label, sizeof(label), "%s  (%d, %d off)", games[i].c_str(), counts[i], offCount);
        else std::snprintf(label, sizeof(label), "%s  (%d)", games[i].c_str(), counts[i]);
        ImGui::PushID(i);
        const bool open = i == m_musicGame && m_musicSearch[0] == 0;
        if (open) ImGui::PushStyleColor(ImGuiCol_Text, NobleStyle::Accent());
        if (ImGui::Selectable(label, open)) {
            m_musicGame = i;
            m_musicSearch[0] = 0;
            MenuSounds::Scroll();
        }
        if (open) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) MenuSounds::Hover(ImGui::GetItemID());
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(rightW);
    ImGui::InputTextWithHint("##musicsearch", "Search every game's tracks", m_musicSearch, sizeof(m_musicSearch));
    const std::string needle = Lower(m_musicSearch);
    ImGui::BeginChild("##musictracks", ImVec2(rightW, height - ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Borders);
    int shown = 0;
    std::string lastGame;
    for (const ReachAudio::TrackInfo& t : tracks) {
        if (t.game.empty()) continue;
        if (needle.empty()) {
            if (games.empty() || t.game != games[m_musicGame]) continue;
        } else {
            if (Lower(t.label + " " + t.game).find(needle) == std::string::npos) continue;
            if (t.game != lastGame) {
                lastGame = t.game;
                ImGui::TextColored(NobleStyle::Accent(), "%s", t.game.c_str());
            }
        }
        const bool selected = t.id == m_edit.menuMusicTrack;
        ImGui::PushID(t.id.c_str());
        const float switchW = 48.0f * s;
        if (ImGui::Selectable(t.label.c_str(), selected, ImGuiSelectableFlags_AllowOverlap,
                              ImVec2(std::max(60.0f, ImGui::GetContentRegionAvail().x - switchW), 0.0f))) {
            pick(t.id);
        }
        if (ImGui::IsItemHovered()) MenuSounds::Hover(ImGui::GetItemID());
        trackSwitch(t);
        if (selected && m_musicScrollToCurrent) {
            ImGui::SetScrollHereY(0.4f);
            m_musicScrollToCurrent = false;
        }
        ImGui::PopID();
        ++shown;
    }
    if (shown == 0) ImGui::TextDisabled(needle.empty() ? "No tracks found in the MCC install." : "No track matches.");
    ImGui::EndChild();
    ImGui::EndGroup();
}

// ---------------------------------------------------------------------------------------------
// Changes
// ---------------------------------------------------------------------------------------------

int SettingsPanel::ChangesIn(Cat c) const {
    return CountDiff(m_edit, m_saved, c);
}

int SettingsPanel::CountDiff(const SettingsData& a, const SettingsData& b, Cat c) {
    auto f = [](float x, float y) { return std::fabs(x - y) > 1e-4f ? 1 : 0; };
    switch (c) {
    case Cat::Graphics:
        return (a.width != b.width || a.height != b.height) + (a.borderless != b.borderless || a.exclusiveFullscreen != b.exclusiveFullscreen) + (a.verticalSync != b.verticalSync) +
               (a.fpsLimit != b.fpsLimit) + (a.showFps != b.showFps);
    case Cat::Audio:
        return f(a.volumeGame, b.volumeGame) + f(a.volumeMusic, b.volumeMusic) + f(a.volumeOther, b.volumeOther) +
               (a.masterMute != b.masterMute) + (a.muteInBackground != b.muteInBackground) +
               (a.menuSounds != b.menuSounds) + (a.menuMusicTrack != b.menuMusicTrack) +
               (a.voiceEnabled != b.voiceEnabled) + (a.voiceInputDevice != b.voiceInputDevice) + (a.voiceOutputDevice != b.voiceOutputDevice) +
               f(a.voiceVolume, b.voiceVolume) + (a.voiceOpenMic != b.voiceOpenMic) + (a.voiceBitrateKbps != b.voiceBitrateKbps) + (a.voiceChannel != b.voiceChannel);
    case Cat::Game:
        return f(a.fieldOfView, b.fieldOfView) + f(a.firstPersonFovMod, b.firstPersonFovMod) +
               f(a.vehicleFirstPersonFovMod, b.vehicleFirstPersonFovMod) +
               f(a.vehicleFirstPersonOffsetX, b.vehicleFirstPersonOffsetX) + f(a.vehicleFirstPersonOffsetY, b.vehicleFirstPersonOffsetY) +
               f(a.thirdPersonZoom, b.thirdPersonZoom) +
               f(a.thirdPersonOffsetX, b.thirdPersonOffsetX) + f(a.thirdPersonOffsetY, b.thirdPersonOffsetY) +
               (a.subtitles != b.subtitles) + (a.menuBackground != b.menuBackground) +
               f(a.brightness, b.brightness) + (a.fxaa != b.fxaa) + (a.throttleUnfocused != b.throttleUnfocused);
    case Cat::Player:
        return (a.playerName != b.playerName) + (a.serviceTag != b.serviceTag);
    case Cat::KeyboardMouse: {
        int n = f(a.mouseSensitivity, b.mouseSensitivity) + f(a.mouseSensitivityY, b.mouseSensitivityY) + (a.invertLook != b.invertLook) +
                (a.wheelUpAction != b.wheelUpAction) + (a.wheelDownAction != b.wheelDownAction) + (a.toggleCrouch != b.toggleCrouch);
        for (int i = 0; i < KeyBindings::kActionCount; ++i) {
            auto ia = a.keyOverrides.find(i);
            auto ib = b.keyOverrides.find(i);
            const int ka = ia != a.keyOverrides.end() ? ia->second : KeyBindings::DefaultKey(i);
            const int kb = ib != b.keyOverrides.end() ? ib->second : KeyBindings::DefaultKey(i);
            if (ka != kb) ++n;
        }
        return n;
    }
    case Cat::Controller:
        return f(a.padLookSpeedX, b.padLookSpeedX) + f(a.padLookSpeedY, b.padLookSpeedY) + (a.padInvertLook != b.padInvertLook) +
               (a.padPaddleKeys != b.padPaddleKeys) +
               (a.padLayout != b.padLayout) + (a.padStickLayout != b.padStickLayout) + (a.padBindings != b.padBindings) +
               (a.padToggleZoom != b.padToggleZoom) + (a.padToggleCrouch != b.padToggleCrouch) +
               f(a.padLeftDeadZone, b.padLeftDeadZone) + f(a.padRightDeadZone, b.padRightDeadZone) +
               (a.padVibration != b.padVibration) + f(a.padVibrationStrength, b.padVibrationStrength);
    case Cat::Online:
        return (a.masterServerUrl != b.masterServerUrl) + (a.inviteNotifications != b.inviteNotifications);
    default:
        return 0;
    }
}

int SettingsPanel::TotalChanges() const {
    int n = 0;
    for (int i = 0; i < static_cast<int>(Cat::Count); ++i) n += ChangesIn(static_cast<Cat>(i));
    return n;
}

void SettingsPanel::RestoreDefaults(Cat c) {
    m_edit = WithDefaults(m_edit, c);
    Settings::Get().Update(m_edit);
    m_saved = Settings::Get().Snapshot();
    std::snprintf(m_nameBuf, sizeof(m_nameBuf), "%s", m_edit.playerName.c_str());
    std::snprintf(m_tagBuf, sizeof(m_tagBuf), "%s", m_edit.serviceTag.c_str());
    std::snprintf(m_masterBuf, sizeof(m_masterBuf), "%s", m_edit.masterServerUrl.c_str());
}

SettingsData SettingsPanel::WithDefaults(SettingsData e, Cat c) {
    const SettingsData d;
    switch (c) {
    case Cat::Graphics:
        e.width = d.width; e.height = d.height; e.borderless = d.borderless; e.exclusiveFullscreen = d.exclusiveFullscreen; e.verticalSync = d.verticalSync;
        e.fpsLimit = d.fpsLimit; e.showFps = d.showFps;
        break;
    case Cat::Audio:
        e.volumeGame = d.volumeGame; e.volumeMusic = d.volumeMusic; e.volumeOther = d.volumeOther;
        e.menuSounds = d.menuSounds; e.menuMusicTrack = d.menuMusicTrack;
        e.voiceEnabled = d.voiceEnabled; e.voiceInputDevice = d.voiceInputDevice; e.voiceOutputDevice = d.voiceOutputDevice;
        e.voiceVolume = d.voiceVolume; e.voiceOpenMic = d.voiceOpenMic; e.voiceBitrateKbps = d.voiceBitrateKbps; e.voiceChannel = d.voiceChannel;
        break;
    case Cat::Game:
        e.fieldOfView = d.fieldOfView; e.firstPersonFovMod = d.firstPersonFovMod;
        e.vehicleFirstPersonFovMod = d.vehicleFirstPersonFovMod;
        e.vehicleFirstPersonOffsetX = d.vehicleFirstPersonOffsetX; e.vehicleFirstPersonOffsetY = d.vehicleFirstPersonOffsetY;
        e.thirdPersonZoom = d.thirdPersonZoom;
        e.thirdPersonOffsetX = d.thirdPersonOffsetX; e.thirdPersonOffsetY = d.thirdPersonOffsetY;
        e.subtitles = d.subtitles; e.menuBackground = d.menuBackground;
        break;
    case Cat::Player:
        e.playerName = d.playerName; e.serviceTag = d.serviceTag;
        break;
    case Cat::KeyboardMouse:
        e.mouseSensitivity = d.mouseSensitivity; e.mouseSensitivityY = d.mouseSensitivityY; e.invertLook = d.invertLook;
        e.wheelUpAction = d.wheelUpAction; e.wheelDownAction = d.wheelDownAction; e.toggleCrouch = d.toggleCrouch; e.keyOverrides.clear();
        break;
    case Cat::Controller:
        e.padLookSpeedX = d.padLookSpeedX; e.padLookSpeedY = d.padLookSpeedY; e.padInvertLook = d.padInvertLook;
        e.padPaddleKeys = d.padPaddleKeys; e.padToggleZoom = d.padToggleZoom; e.padToggleCrouch = d.padToggleCrouch;
        e.padLeftDeadZone = d.padLeftDeadZone; e.padRightDeadZone = d.padRightDeadZone;
        e.padVibration = d.padVibration; e.padVibrationStrength = d.padVibrationStrength;
        break;
    case Cat::Online:
        e.masterServerUrl = d.masterServerUrl;
        break;
    default:
        break;
    }
    return e;
}

void SettingsPanel::Apply() {
    SyncBuffers();
    // The engine renders at the display's native resolution for the whole session, so changing the
    // window mode or size only restyles/resizes the window: Windows keeps showing the same crisp image.
    // No restart and no swap-chain resize are needed.
    const bool displayChanged = m_saved.width != m_edit.width || m_saved.height != m_edit.height ||
                                m_saved.borderless != m_edit.borderless ||
                                m_saved.exclusiveFullscreen != m_edit.exclusiveFullscreen;
    Settings::Get().Update(m_edit);
    if (displayChanged) {
        if (auto* win = Window::Active()) win->ApplyWindowSettings(m_edit.width, m_edit.height, m_edit.borderless, m_edit.exclusiveFullscreen);
    }
    ReachAudio::PlayMenuMusic(m_edit.menuMusicTrack);
    MasterServerClient::Get().SetUrl(m_edit.masterServerUrl);
    // Field of view goes straight into the running engine (haloreach+0xB42C10, degrees); the
    // per-frame CameraFov::Tick keeps it there and picks it up for the next game too.
    CameraFov::Apply(m_edit.fieldOfView);
    ProcessAudio::ApplyAll(m_edit.volumeGame, m_edit.volumeMusic, m_edit.volumeOther, m_edit.masterMute);
    m_saved = Settings::Get().Snapshot();  // clamped values
    m_edit = m_saved;
    std::snprintf(m_nameBuf, sizeof(m_nameBuf), "%s", m_edit.playerName.c_str());
    std::snprintf(m_tagBuf, sizeof(m_tagBuf), "%s", m_edit.serviceTag.c_str());
    std::snprintf(m_masterBuf, sizeof(m_masterBuf), "%s", m_edit.masterServerUrl.c_str());
    m_message = displayChanged ? "Display updated." : "Saved.";
    m_messageTime = ImGui::GetTime();
    MenuSounds::Click();
}

void SettingsPanel::Discard() {
    m_loaded = false;          // reload from the saved settings next frame
    m_rebindAction = -1;
    const auto snap = Settings::Get().Snapshot();
    ReachAudio::PlayMenuMusic(snap.menuMusicTrack);
    ProcessAudio::ApplyAll(snap.volumeGame, snap.volumeMusic, snap.volumeOther, snap.masterMute);
    m_message = "Changes discarded.";
    m_messageTime = ImGui::GetTime();
    MenuSounds::Back();
}

std::string SettingsPanel::Summary(Cat c) const {
    const SettingsData& e = m_edit;
    char buf[96];
    switch (c) {
    case Cat::Graphics:
        if (e.exclusiveFullscreen) std::snprintf(buf, sizeof(buf), "Fullscreen \xC2\xB7 %s", e.fpsLimit ? (std::to_string(e.fpsLimit) + " FPS").c_str() : "No limit");
        else if (e.borderless) std::snprintf(buf, sizeof(buf), "Borderless \xC2\xB7 %s", e.fpsLimit ? (std::to_string(e.fpsLimit) + " FPS").c_str() : "No limit");
        else std::snprintf(buf, sizeof(buf), "%d x %d \xC2\xB7 %s", e.width, e.height, e.fpsLimit ? (std::to_string(e.fpsLimit) + " FPS").c_str() : "No limit");
        return buf;
    case Cat::Audio:
        std::snprintf(buf, sizeof(buf), "Game %.0f %% \xC2\xB7 Music %s", e.volumeGame * 100.0f, e.menuMusicTrack == "off" ? "off" : "on");
        return buf;
    case Cat::Game:
        std::snprintf(buf, sizeof(buf), "%.0f\xC2\xB0 FOV \xC2\xB7 Subtitles %s", e.fieldOfView, e.subtitles ? "on" : "off");
        return buf;
    case Cat::Player:
        std::snprintf(buf, sizeof(buf), "%s [%s]", m_nameBuf, m_tagBuf);
        return buf;
    case Cat::KeyboardMouse: {
        int rebound = 0;
        for (const auto& [action, key] : e.keyOverrides) if (key != KeyBindings::DefaultKey(action)) ++rebound;
        std::snprintf(buf, sizeof(buf), "Sensitivity %.2f \xC2\xB7 %d rebound", e.mouseSensitivity, rebound);
        return buf;
    }
    case Cat::Controller: {
        const int pad = ConnectedPad();
        return pad >= 0 ? std::string("Controller connected") : std::string("No controller connected");
    }
    case Cat::Online:
        return e.masterServerUrl.empty() ? std::string("LAN only") : std::string("Master server set");
    case Cat::System:
        return std::string(VERSION_STRING);
    default:
        return {};
    }
}


void SettingsPanel::RenderSystem() {
    const std::string version(VERSION_STRING);
    const std::filesystem::path settingsFile = Settings::Get().FilePath();
    const std::filesystem::path exeDir = settingsFile.parent_path().parent_path().parent_path();
    const std::filesystem::path userData = settingsFile.parent_path();
    const std::filesystem::path backups = userData / "backups";
    static std::string s_note;

    // Action rows: Row() is a layout helper that always returns true, so the click test is explicit (see the
    // 2026-10-30 09:35 entry). The outcome of the last action is shown under the rows.
    auto action = [&](const char* label, const char* description, const char* value, auto&& run) {
        if (!Row(label, description, When::Instant, value, false)) return;
        InfoValue(value);
        const bool clicked = m_rowHovered && ImGui::IsMouseClicked(0);
        const bool accepted = m_rowFocused && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                               ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false));
        if (!clicked && !accepted) return;
        MenuSounds::Click();
        run();
    };
    auto copyFile = [](const std::filesystem::path& from, const std::filesystem::path& to) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(from, ec)) return false;
        std::filesystem::create_directories(to.parent_path(), ec);
        std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
        return !ec;
    };

    Section("NOBLE");
    if (Row("VERSION", "The build you are running. Worth quoting when you report a problem.", When::Info, version, false, true)) {
        InfoValue(version.c_str());
    }
    if (Row("LOG FILE", "noble-client.log, next to the program. It is what a bug report needs.", When::Info, "noble-client.log", false, true)) {
        InfoValue("noble-client.log");
    }
    action("OPEN LOGS FOLDER", "Opens the folder that holds the log, the crash dump and this settings file.", "Explorer",
           [&] {
               ShellExecuteW(nullptr, L"open", exeDir.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
               s_note = "Opened " + exeDir.string();
           });
    action("COPY DIAGNOSTICS", "Puts the version and the last 40 log lines on the clipboard, ready to paste into a report. Your player name and Windows user name are replaced first.",
           "Clipboard", [&] {
               std::string text = std::string("Project Noble ") + version + "\n" + exeDir.string() + "\n";
               std::ifstream log(exeDir / "noble-client.log");
               if (log) {
                   std::vector<std::string> lines;
                   for (std::string line; std::getline(log, line);) {
                       lines.push_back(line);
                       if (lines.size() > 40) lines.erase(lines.begin());
                   }
                   text += "\n--- last log lines ---\n";
                   for (const std::string& line : lines) text += line + "\n";
               }
               const auto scrub = [&text](const std::string& secret, const char* stand) {
                   if (secret.size() < 2) return;
                   for (size_t at = text.find(secret); at != std::string::npos; at = text.find(secret, at + std::strlen(stand))) text.replace(at, secret.size(), stand);
               };
               scrub(Settings::Get().Snapshot().playerName, "<player>");
               char windowsUser[256]{};
               if (GetEnvironmentVariableA("USERNAME", windowsUser, sizeof(windowsUser)) > 0) scrub(windowsUser, "<user>");
               NOBLE_INFO("SettingsPanel: diagnostics copied, {} bytes", text.size());
               ImGui::SetClipboardText(text.c_str());
               s_note = "Diagnostics copied to the clipboard.";
           });

    Section("BACKUPS");
    const bool haveBackup = std::filesystem::is_directory(backups);
    action("BACK UP SETTINGS", "Copies settings.json and appearance.dat into data\\userdata\\backups\\<date-time>, so they can be put back later.",
           haveBackup ? "Backups exist" : "None yet", [&] {
               SYSTEMTIME now{};
               GetLocalTime(&now);
               char stamp[64];
               std::snprintf(stamp, sizeof(stamp), "%04u-%02u-%02u_%02u%02u%02u", now.wYear, now.wMonth, now.wDay,
                             now.wHour, now.wMinute, now.wSecond);
               const std::filesystem::path dir = backups / stamp;
               int copied = 0;
               if (copyFile(userData / "settings.json", dir / "settings.json")) ++copied;
               if (copyFile(userData.parent_path() / "appearance.dat", dir / "appearance.dat") ||
                   copyFile(userData / "appearance.dat", dir / "appearance.dat")) ++copied;
               if (copied == 0) {
                   s_note = "Nothing to back up yet.";
                   MenuSounds::Error();
               } else {
                   s_note = "Backed up to " + dir.string();
               }
           });
    action("RESTORE NEWEST BACKUP", "Puts the most recent backup back and reloads it. Anything unsaved is lost.", "Reloads now", [&] {
        std::error_code ec;
        std::filesystem::path newest;
        for (const auto& entry : std::filesystem::directory_iterator(backups, ec)) {
            if (!entry.is_directory(ec)) continue;
            if (newest.empty() || entry.path().filename() > newest.filename()) newest = entry.path();
        }
        if (newest.empty()) {
            s_note = "No backup to restore yet.";
            MenuSounds::Error();
            return;
        }
        int restored = 0;
        if (copyFile(newest / "settings.json", userData / "settings.json")) ++restored;
        if (copyFile(newest / "appearance.dat", userData.parent_path() / "appearance.dat")) ++restored;
        if (restored == 0) {
            s_note = "That backup is empty.";
            MenuSounds::Error();
            return;
        }
        Settings::Get().Load();
        m_saved = Settings::Get().Snapshot();
        m_edit = m_saved;
        SyncBuffers();
        s_note = "Restored " + newest.filename().string() + " and reloaded it.";
    });
    action("OPEN BACKUP FOLDER", "Shows the backups on disk, so an old one can be copied back by hand.", "Explorer", [&] {
        std::error_code ec;
        std::filesystem::create_directories(backups, ec);
        ShellExecuteW(nullptr, L"open", backups.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        s_note = "Opened " + backups.string();
    });

    Section("MOVE TO ANOTHER PC");
    const auto pickFile = [](bool save, std::filesystem::path& out) {
        wchar_t file[MAX_PATH] = L"noble-settings.json";
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFilter = L"Noble settings\0*.json\0All files\0*.*\0";
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"json";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
        if (!(save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn))) return false;
        out = file;
        return true;
    };
    action("EXPORT SETTINGS...", "Saves your key bindings and preferences to a file you choose. Your account, the server address and this PC's audio devices are left out, so it's safe to share.", "Choose a file", [&] {
        std::filesystem::path file;
        if (!pickFile(true, file)) return;
        const bool ok = Settings::Get().ExportTo(file);
        s_note = ok ? "Exported to " + file.string() : "That file could not be written.";
        NOBLE_INFO("SettingsPanel: export {}", ok ? "done" : "failed");
        if (!ok) MenuSounds::Error();
    });
    action("IMPORT SETTINGS...", "Loads key bindings and preferences from a file, in place of your current ones. Use BACK UP SETTINGS first to keep them. Your account and server stay as they are.", "Choose a file", [&] {
        std::filesystem::path file;
        if (!pickFile(false, file)) return;
        const bool ok = Settings::Get().ImportFrom(file);
        NOBLE_INFO("SettingsPanel: import {}", ok ? "done" : "failed");
        if (!ok) {
            s_note = "That isn't a settings file Noble can read. Nothing was changed.";
            MenuSounds::Error();
            return;
        }
        m_saved = Settings::Get().Snapshot();
        m_edit = m_saved;
        SyncBuffers();
        s_note = "Imported " + file.filename().string() + ".";
    });
    Section("SETUP");
    action("RUN FIRST-TIME SETUP", "Shows the first-start questions again the next time you're on the main menu.", "Main menu", [&] {
        SettingsData d = Settings::Get().Snapshot();
        d.firstStartDone = false;
        Settings::Get().Update(d);
        m_saved = Settings::Get().Snapshot();
        m_edit = m_saved;
        SyncBuffers();
        s_note = "The setup will show on the main menu.";
    });

    if (!s_note.empty()) {
        ImGui::Dummy(ImVec2(0, 8.0f * Scale()));
        ImGui::PushStyleColor(ImGuiCol_Text, NobleStyle::TextMuted());
        ImGui::TextWrapped("%s", s_note.c_str());
        ImGui::PopStyleColor();
    }
}

} // namespace noble
