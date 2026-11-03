#include "audio/VoiceChat.hpp"
#include "ForgeMenu.hpp"
#include "SocialOverlay.hpp"
#include "PauseMenu.hpp"
#include "MenuSounds.hpp"
#include "NobleStyle.hpp"
#include "NobleUi.hpp"
#include "SettingsPanel.hpp"
#include "InputPrompts.hpp"
#include "UiArt.hpp"
#include "UiPad.hpp"
#include "audio/ReachAudio.hpp"
#include "career/Career.hpp"
#include "common/Logger.hpp"
#include "common/MapNames.hpp"
#include "common/Relaunch.hpp"
#include "network/MasterServerClient.hpp"
#include "network/SessionManager.hpp"
#include "network/NetworkTransport.hpp"
#include "common/Settings.hpp"
#include "engine/FrameCapture.hpp"
#include "engine/ReachHost.hpp"
#include "engine/CameraThirdPerson.hpp"
#include "engine/CameraThirdPerson.hpp"
#include "engine/KeyBindings.hpp"
#include "host/Window.hpp"

#include <chrono>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <d3d11.h>
#include <dxgi.h>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shellapi.h>
#include <string>
#include <thread>

namespace noble {

namespace {

std::atomic<bool> s_open{false};
std::atomic<bool> s_gameOver{false};
std::atomic<bool> s_scoreboardShotRequest{false};
std::atomic<bool> s_boardPointer{false}; // scoreboard actions own the pointer
std::atomic<bool> s_boardReport{false};  // the Report window is up
std::atomic<bool> s_showSettings{
    false};               // the SETTINGS page opened from the pause menu
std::mutex s_renderMutex; // Render (engine thread) vs HoldFrame (main thread)
std::atomic<IDXGISwapChain *> s_swapChain{
    nullptr}; // last swap chain the engine presented
std::atomic<ULONGLONG> s_lastEngineFrame{
    0}; // GetTickCount64 of the last slot-1 frame
std::atomic<ULONGLONG> s_gameOverAt{0};
std::mutex s_mapMutex;
std::string s_loadingMap;
std::string s_loadingMapName;
std::string s_displayMapName;
std::filesystem::path s_loadingImage;
std::filesystem::path s_loadingThumb;

struct MapName {
  const char *scenario;
  const char *name;
};

// Display names for the loading screen (same names as the front-end menu).
const MapName kMapNames[] = {
    {"50_panopticon", "Boardwalk"},
    {"70_boneyard", "Boneyard"},
    {"45_launch_station", "Countdown"},
    {"30_settlement", "Powerhouse"},
    {"52_ivory_tower", "Reflection"},
    {"35_island", "Spire"},
    {"20_sword_slayer", "Sword Base"},
    {"45_aftship", "Zealot"},
    {"dlc_slayer", "Anchor 9"},
    {"dlc_invasion", "Breakpoint"},
    {"dlc_medium", "Tempest"},
    {"condemned", "Condemned"},
    {"trainingpreserve", "Highlands"},
    {"cex_beaver_creek", "Battle Canyon"},
    {"cex_damnation", "Penance"},
    {"cex_timberland", "Ridgeline"},
    {"cex_prisoner", "Solitary"},
    {"cex_hangemhigh", "High Noon"},
    {"cex_headlong", "Breakneck"},
    {"forge_halo", "Forge World"},
    {"m05", "Noble Actual"},
    {"m10", "Winter Contingency"},
    {"m20", "ONI: Sword Base"},
    {"m30", "Nightfall"},
    {"m35", "Tip of the Spear"},
    {"m45", "Long Night of Solace"},
    {"m50", "Exodus"},
    {"m52", "New Alexandria"},
    {"m60", "The Package"},
    {"m70", "The Pillar of Autumn"},
    {"m70_bonus", "Lone Wolf"},
    {"ff10_prototype", "Overlook"},
    {"ff20_courtyard", "Courtyard"},
    {"ff30_waterfront", "Waterfront"},
    {"ff45_corvette", "Corvette"},
    {"ff50_park", "Beachhead"},
    {"ff60_airview", "Outpost"},
    {"ff60_icecave", "Glacier"},
    {"ff70_holdout", "Holdout"},
    {"ff_unearthed", "Unearthed"},
    {"cex_ff_halo", "Installation 04"},
};

std::string DisplayName(const std::string &scenario) {
  for (const MapName &m : kMapNames) {
    if (scenario == m.scenario)
      return m.name;
  }
  return scenario;
}

std::string Upper(std::string s) {
  for (char &c : s)
    c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
  return s;
}

// Starts a new Noble (empty args = front-end menu) and closes this one.
// One value from a command line built by Relaunch ("-map x -variant \"a b\"").
std::wstring ArgValue(const std::wstring &args, const wchar_t *name) {
  std::wstring value;
  int argc = 0;
  LPWSTR *argv = CommandLineToArgvW((L"x " + args).c_str(), &argc);
  for (int i = 1; argv && i + 1 < argc; ++i) {
    if (_wcsicmp(argv[i], name) == 0) value = argv[i + 1];
  }
  if (argv) LocalFree(argv);
  return value;
}

// The arguments that bring this match's lobby back in a new process (FrontEndMenu reads the -lobby-* set).
std::wstring LobbyReturnArgs(bool host, bool advance = false) {
  const std::wstring game = Relaunch::CurrentGameArgs();
  const auto quoted = [](const std::wstring &value) { return L"\"" + value + L"\""; };
  std::wstring out = host ? L"-lobby-return host" : L"-lobby-return guest";
  const auto copy = [&](const wchar_t *from, const wchar_t *to) {
    std::wstring value = ArgValue(game, from);
    if (value.empty()) value = ArgValue(GetCommandLineW(), from);
    if (!value.empty()) out += std::wstring(L" ") + to + L" " + quoted(value);
  };
  copy(L"-map", L"-lobby-map");
  copy(L"-mode", L"-lobby-mode");
  copy(L"-variant", L"-lobby-variant");
  copy(L"-difficulty", L"-lobby-difficulty");
  copy(L"-skulls", L"-lobby-skulls");
  copy(L"-mod", L"-lobby-mod-folder");
  copy(L"-map-override", L"-lobby-map-override");
  copy(L"-map-title", L"-lobby-map-title");
  copy(L"-map-image", L"-lobby-map-image");
  copy(L"-map-thumb", L"-lobby-map-thumb");

  // Keep session id and token for both host and guest so the session is preserved!
  copy(L"-session-id", L"-lobby-session");
  copy(L"-session-token", L"-lobby-session-token");

  if (host) {
    copy(L"-max-players", L"-lobby-max-players");
    const std::wstring own = GetCommandLineW();
    out += (game.find(L" -lan") != std::wstring::npos || own.find(L" -lan") != std::wstring::npos) ? L" -lobby-net lan" : L" -lobby-net online";
    out += (game.find(L" -private") != std::wstring::npos || own.find(L" -private") != std::wstring::npos) ? L" -lobby-privacy private" : L" -lobby-privacy public";
    // Only the menu that started this match knows the playlist, so it's on this process's own command line.
    const std::wstring list = ArgValue(own, L"-lobby-playlist");
    if (!list.empty()) {
      out += L" -lobby-playlist " + quoted(list) + L" -lobby-playlist-pos " + quoted(ArgValue(own, L"-lobby-playlist-pos"));
      if (advance) out += L" -lobby-advance 1";
    }
  } else {
    std::wstring endpoint = ArgValue(game, L"-connect");
    if (endpoint.empty()) endpoint = ArgValue(GetCommandLineW(), L"-connect");
    if (!endpoint.empty()) out += L" -lobby-endpoint " + quoted(endpoint);
  }
  return out;
}

// For a guest on the post-game panel: 0 still waiting, 1 the host is back in the lobby, 2 the session has gone. The server
// says the match is running until the host's new process registers again.
int PollHostBackInLobby() {
  static std::atomic<int> s_state{0};
  static bool s_started = false;
  if (!s_started) {
    s_started = true;
    const std::string session = [] {
      std::wstring wide = ArgValue(Relaunch::CurrentGameArgs(), L"-session-id");
      if (wide.empty()) wide = ArgValue(GetCommandLineW(), L"-session-id");
      char text[128] = {};
      WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, text, sizeof(text) - 1, nullptr, nullptr);
      return std::string(text);
    }();
    if (session.empty()) {
      // In LAN / direct connect mode where no master server session was created:
      // poll LAN discovery to see when host has returned to lobby
      std::thread([] {
        for (int i = 0; i < 25; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1000));
          auto sessions = SessionManager::Get().GetDiscoveredSessions();
          if (!sessions.empty()) {
            NOBLE_INFO("PauseMenu: detected host LAN lobby, pulling back to lobby");
            s_state = 1;
            return;
          }
        }
        s_state = 2;
      }).detach();
    } else {
      std::thread([session] {
        int missing = 0;
        while (true) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1000));
          const LobbyState state = MasterServerClient::Get().FetchLobbyStateSync(session);
          if (!state.ok || state.isClosed) {
            if (++missing >= 25) {
              NOBLE_WARN("PauseMenu: host lobby closed or timed out (missing: {})", missing);
              s_state = 2;
              return;
            }
            continue;
          }
          missing = 0;
          if (!state.inProgress) {
            NOBLE_INFO("PauseMenu: the host is back in the lobby (session: {})", session);
            s_state = 1;
            return;
          }
        }
      }).detach();
    }
  }
  return s_state;
}

// Finds " -name value" in a built command line. Returns false if it is not there; [begin, end) covers the separator,
// the name and the value (quoted or not).
bool FindArgSpan(const std::wstring &args, const std::wstring &name, size_t &begin, size_t &end) {
  for (size_t at = args.find(name); at != std::wstring::npos; at = args.find(name, at + 1)) {
    const size_t after = at + name.size();
    if ((at != 0 && args[at - 1] != L' ') || after >= args.size() || args[after] != L' ') continue;
    size_t value = after + 1;
    if (value < args.size() && args[value] == L'"') {
      const size_t close = args.find(L'"', value + 1);
      end = close == std::wstring::npos ? args.size() : close + 1;
    } else {
      const size_t space = args.find(L' ', value);
      end = space == std::wstring::npos ? args.size() : space;
    }
    begin = at != 0 ? at - 1 : 0;
    return true;
  }
  return false;
}

void RemoveArg(std::wstring &args, const std::wstring &name) {
  size_t begin = 0, end = 0;
  if (FindArgSpan(args, name, begin, end)) args.erase(begin, end - begin);
}

void SetArg(std::wstring &args, const std::wstring &name, const std::wstring &value) {
  RemoveArg(args, name);
  if (!args.empty() && args.front() == L' ') args.erase(0, 1);
  args += (args.empty() ? L"" : L" ") + name + L" \"" + value + L"\"";
}

// The mission after this one in campaign order, or "" at the end of the campaign or for anything that isn't a stock
// mission. Missions can be named with a scene suffix ("m10_b").
std::wstring NextCampaignMission(const std::wstring &game) {
  static const wchar_t *const kOrder[] = {L"m05", L"m10", L"m20", L"m30", L"m35", L"m45",
                                          L"m50", L"m52", L"m60", L"m70", L"m70_bonus"};
  std::wstring mode = ArgValue(game, L"-mode");
  if (mode.empty()) mode = ArgValue(GetCommandLineW(), L"-mode");
  if (mode.empty()) mode = ArgValue(GetCommandLineW(), L"-lobby-mode");
  if (_wcsicmp(mode.c_str(), L"campaign") != 0 && wcsstr(GetCommandLineW(), L"-campaign") == nullptr && game.find(L"-campaign") == std::wstring::npos) return {};
  if (!ArgValue(game, L"-mod").empty() || !ArgValue(game, L"-map-override").empty()) return {};
  std::wstring map = ArgValue(game, L"-map");
  if (map.empty()) map = ArgValue(GetCommandLineW(), L"-map");
  if (map.empty()) map = ArgValue(GetCommandLineW(), L"-lobby-map");
  std::transform(map.begin(), map.end(), map.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
  constexpr int kCount = static_cast<int>(sizeof(kOrder) / sizeof(kOrder[0]));
  int at = -1;
  for (int i = 0; i < kCount; ++i) if (map == kOrder[i]) at = i;
  if (at < 0) {
    for (int i = 0; i < kCount; ++i) if (map.rfind(std::wstring(kOrder[i]) + L"_", 0) == 0) at = i;
  }
  return at >= 0 && at + 1 < kCount ? kOrder[at + 1] : L"";
}

std::wstring MissionTitle(const std::wstring &mission) {
  char narrow[64] = {};
  WideCharToMultiByte(CP_UTF8, 0, mission.c_str(), -1, narrow, sizeof(narrow) - 1, nullptr, nullptr);
  const std::string title = MapNames::Display(narrow);
  wchar_t wide[128] = {};
  MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wide, 127);
  return wide;
}

// Arguments that start the next mission, or "" if there is none. A hosted game goes back to its lobby with the next
// mission selected, so guests stay together; a solo game starts it straight away. Difficulty and skulls carry over.
std::wstring NextMissionArgs(const std::wstring &next, std::wstring lobbyArgs) {
  if (next.empty()) return {};
  const std::wstring title = MissionTitle(next);
  std::wstring game = Relaunch::CurrentGameArgs();
  if (game.find(L" -host") != std::wstring::npos) {
    SetArg(lobbyArgs, L"-lobby-map", next);
    SetArg(lobbyArgs, L"-lobby-map-title", title);
    RemoveArg(lobbyArgs, L"-lobby-map-image");
    RemoveArg(lobbyArgs, L"-lobby-map-thumb");
    return lobbyArgs;
  }
  SetArg(game, L"-map", next);
  SetArg(game, L"-map-title", title);
  RemoveArg(game, L"-map-image");
  RemoveArg(game, L"-map-thumb");
  return game;
}

void RelaunchAndClose(HWND hwnd, const std::wstring &args, const char *what) {
  NOBLE_INFO("PauseMenu: {} requested", what);
  Relaunch::HandOff(args);  // ends this process; returns only if the new one could not start
  PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

ImVec2 CenteredWindow(float width) {
  const ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos(
      ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
      ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
  return ImVec2(-1.0f, 38.0f);
}

constexpr ImGuiWindowFlags kPanelFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

// The SETTINGS page is the same panel the front-end menu shows, reused inside
// the pause menu so the player can change settings during a match (FOV, VSync,
// frame cap, mouse, ...). It lives for the life of the process because it keeps
// the edited copy and the key-rebind state between frames.
SettingsPanel &PauseSettings() {
  static SettingsPanel panel;
  return panel;
}

void DrawSettingsPage(HWND hwnd);

} // namespace

bool PauseMenu::IsOpen() { return s_open.load(); }

void PauseMenu::Toggle() {
  if (s_gameOver.load())
    return; // the post-game panel replaces the pause menu
  // Esc steps back out of the scoreboard actions instead of opening the pause menu.
  if (s_boardPointer.load()) {
    if (!s_boardReport.exchange(false))
      s_boardPointer.store(false);
    MenuSounds::Back();
    return;
  }
  // Esc on the SETTINGS page goes back to the pause menu instead of resuming.
  // While a key rebind is waiting for a key, Esc belongs to the rebind
  // (SettingsPanel::PollRebind cancels it).
  if (s_open.load() && s_showSettings.load()) {
    if (!PauseSettings().IsCapturingKey() && !PauseSettings().DiscardOnEscape()) {
      s_showSettings.store(false);
      MenuSounds::Back();
      NOBLE_INFO("PauseMenu: settings closed");
    }
    return;
  }
  const bool nowOpen = !s_open.load();
  if (nowOpen)
    Window::TakeLeftClicks(); // forget clicks made in game (shots) before the
                              // menu opened
  s_open.store(nowOpen);
  NOBLE_INFO("PauseMenu: {}", nowOpen ? "opened" : "closed");
}

void PauseMenu::Close() {
    // (the open-time guard in DrawMenu is a function-local static; it re-arms by re-reading the clock below)
  s_showSettings.store(false);
  if (s_open.exchange(false)) {
    NOBLE_INFO("PauseMenu: closed");
  }
}

void PauseMenu::ShowGameOver() {
  if (!s_gameOver.exchange(true)) {
    s_open.store(false);
    s_showSettings.store(false);
    s_gameOverAt.store(GetTickCount64());
    Window::TakeLeftClicks();
    NOBLE_INFO("PauseMenu: game over, showing post-game panel");
  }
}

bool PauseMenu::IsGameOver() { return s_gameOver.load(); }

namespace {
std::mutex s_boardMutex;
std::vector<PauseMenu::ScoreboardEntry> s_boardRows;
std::string s_boardMap;
std::string s_boardMode;
bool s_boardLogged = false;
} // namespace

static std::string s_boardSession;

void PauseMenu::SetScoreboardSession(const std::string& sessionId) {
  std::lock_guard<std::mutex> lock(s_boardMutex);
  s_boardSession = sessionId;
}

void PauseMenu::SetScoreboardRoster(std::vector<ScoreboardEntry> entries, const std::string& mapName, const std::string& modeName) {
    std::lock_guard<std::mutex> lock(s_boardMutex);
    s_boardRows = std::move(entries);
    s_boardMap = mapName;
    s_boardMode = modeName;
    if (!s_boardLogged) {
        s_boardLogged = true;
        NOBLE_INFO("PauseMenu: scoreboard roster set ({} player(s), {} on {})", s_boardRows.size(), s_boardMode, s_boardMap);
    }
}

bool PauseMenu::WantsPointer() { return s_open.load() || s_gameOver.load() || s_boardPointer.load(); }

void PauseMenu::EnterScoreboardPointer() {
  if (s_open.load() || s_gameOver.load() || s_boardPointer.exchange(true))
    return;
  Window::TakeLeftClicks();
  NOBLE_INFO("PauseMenu: scoreboard actions opened");
}

void PauseMenu::SetLoadingMap(const std::string &scenario) {
  std::lock_guard<std::mutex> lock(s_mapMutex);
  s_loadingMap = scenario;
  s_loadingMapName = Upper(DisplayName(scenario));
  s_displayMapName.clear();
  s_loadingImage.clear();
  s_loadingThumb.clear();
}

void PauseMenu::SetLoadingMapInfo(const std::string &title, const std::filesystem::path &image,
                                  const std::filesystem::path &thumbnail) {
  std::lock_guard<std::mutex> lock(s_mapMutex);
  if (!title.empty()) {
    s_displayMapName = title;
    s_loadingMapName = Upper(title);
  }
  s_loadingImage = image;
  s_loadingThumb = thumbnail;
}

void PauseMenu::DrawMenu(HWND hwnd) {
    ImGuiIO& io = ImGui::GetIO();

    // Dim the game behind the menu.
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 150));

    if (s_showSettings.load()) {
        DrawSettingsPage(hwnd);
        return;
    }

    // A held game pauses; a match with other players in it must not - MCC reaches the same conclusion and just shows
    // the menu. Noble cannot stop the engine's simulation itself (no pause command has been found), so the header
    // says which of the two this is instead of pretending.
    static ULONGLONG s_openedAt = 0;
    if (s_openedAt == 0) s_openedAt = GetTickCount64();
    const bool interactive = GetTickCount64() - s_openedAt > 250;   // ignore the opening frames
    if (!interactive) {
        static bool s_resetOnClose = false;
        (void)s_resetOnClose;
    }

    const size_t peers = NetworkTransport::Get().GetGamePeerCount();
    const bool solo = peers == 0;
    const std::wstring restartArgs = Relaunch::CurrentGameArgs();

    // ImGui works in a logical space (1280x720 here even on a 1920x1080 buffer), so scale from 720 to keep the
    // panel the size the reference screens suggest (about 84% of the width).
    const float s = std::clamp(io.DisplaySize.y / 720.0f, 0.8f, 2.0f);
    const ImVec2 size(1080.0f * s, 560.0f * s);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - size.x) * 0.5f, (io.DisplaySize.y - size.y) * 0.5f),
                            ImGuiCond_Always);
    if (!ImGui::Begin("##NoblePauseMenu", nullptr, kPanelFlags)) {
        ImGui::End();
        return;
    }

    // Header: the match type and map, like MCC's pause header.
    NobleStyle::Header(solo ? "PAUSED" : "MENU");
    ImGui::PushStyleColor(ImGuiCol_Text, NobleStyle::TextMuted());
    if (solo) {
        ImGui::TextUnformatted("The game is paused.");
    } else {
        ImGui::Text("The match keeps running for the other %zu player%s.", peers, peers == 1 ? "" : "s");
    }
    ImGui::PopStyleColor();
    ImGui::Spacing();

    const float availW = ImGui::GetContentRegionAvail().x;
    const float listW = 330.0f * s;
    const float gap = 18.0f * s;
    const float contentH = 420.0f * s;

    // ---- Left: the action list (selection + description, like Reach's pause menu) ----
    ImGui::BeginGroup();
    struct Entry {
        const char* label;
        const char* description;
        bool danger;
        bool enabled;
    };
    const bool canRestart = !restartArgs.empty();
    const Entry entries[] = {
        { "RESUME", "Resume the game.", false, true },
        { "RESTART MATCH", canRestart ? "Starts this match again from the beginning." : "Needs the match arguments (only available in a launched match).",
          false, canRestart },
        { "SETTINGS", "Change any setting without leaving the match.", false, true },
        { "CONTROLS", "Change key, mouse and controller bindings and look speed without leaving the match.", false, true },
        { "LEAVE GAME", "Close this match and go back to the front-end menu.", false, true },
        { "QUIT", "Close Noble and end the match.", true, true },
    };
    const int entryCount = static_cast<int>(sizeof(entries) / sizeof(entries[0]));

    static int s_focus = 0;
    static ULONGLONG s_lastMove = 0;
    const ULONGLONG now = GetTickCount64();
    const bool up = ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) || UiPad::Up();
    const bool down = ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) || UiPad::Down();
    if (interactive && (up || down) && now - s_lastMove > 120) {
        s_lastMove = now;
        s_focus = (s_focus + (down ? 1 : entryCount - 1)) % entryCount;
        MenuSounds::Scroll();
    }
    s_focus = std::clamp(s_focus, 0, entryCount - 1);

    const float rowH = 46.0f * s;
    int activated = -1;
    for (int i = 0; i < entryCount; ++i) {
        ImGui::PushID(i);
        const bool focused = i == s_focus;
        const bool enabled = entries[i].enabled;
        ImGui::PushStyleColor(ImGuiCol_Text, enabled ? (focused ? NobleStyle::Accent() : ImVec4(0.86f, 0.90f, 0.94f, 1.0f))
                                                     : NobleStyle::TextMuted());
        if (ImGui::Selectable(entries[i].label, focused, ImGuiSelectableFlags_None,
                              ImVec2(listW, rowH + 6.0f * s)) && enabled) {
            s_focus = i;
            activated = i;
        }
        if (ImGui::IsItemHovered()) {
            MenuSounds::Hover(ImGui::GetItemID());
            // Only a moving mouse takes the focus: a pointer resting here used to pin the row against the D-pad.
            if (enabled && (ImGui::GetIO().MouseDelta.x != 0.0f || ImGui::GetIO().MouseDelta.y != 0.0f)) s_focus = i;
        }
        ImGui::PopStyleColor();
        // Underline, like the reference screens.
        const ImVec2 p = ImGui::GetItemRectMin();
        const ImVec2 q = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x + 8.0f * s, q.y - 4.0f * s), ImVec2(p.x + listW - 8.0f * s, q.y - 4.0f * s),
                                            focused ? IM_COL32(140, 200, 255, 200) : IM_COL32(90, 105, 120, 120), 1.0f);
        ImGui::PopID();
    }
    if (interactive && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false))) {
        if (entries[s_focus].enabled) activated = s_focus;
    }
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, NobleStyle::TextMuted());
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + listW);
    ImGui::TextWrapped("%s", entries[s_focus].description);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::EndGroup();

    // ---- Right: daily and weekly challenges, scrollable (the reference screens' sidebar) ----
    ImGui::SameLine(0.0f, gap);
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, NobleStyle::TextMuted());
    ImGui::TextUnformatted(("Daily challenges - " + Career::DailyResetText()).c_str());
    ImGui::PopStyleColor();
    const float sideW = availW - listW - gap;
    ImGui::BeginChild("##pausechallenges", ImVec2(sideW, contentH), ImGuiChildFlags_Borders);
    if (interactive) UiPad::ScrollWindow(false, true);
    const std::vector<Career::Challenge> challenges = Career::Challenges();
    int shown = 0;
    for (int weekly = 0; weekly <= 1; ++weekly) {
        bool heading = false;
        for (const Career::Challenge& c : challenges) {
            if (static_cast<int>(c.weekly) != weekly) continue;
            if (!heading) {
                heading = true;
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, weekly ? ImVec4(1.0f, 0.75f, 0.47f, 1.0f) : NobleStyle::Accent());
                ImGui::TextUnformatted(weekly ? "WEEKLY" : "DAILY");
                ImGui::PopStyleColor();
            }
            ImGui::PushID(shown++);
            const ImVec4 accent = c.done ? ImVec4(0.55f, 0.85f, 0.55f, 1.0f) : NobleStyle::Accent();
            ImGui::PushStyleColor(ImGuiCol_Text, accent);
            ImGui::TextUnformatted(c.title.c_str());
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, NobleStyle::TextMuted());
            ImGui::TextWrapped("%s", c.description.c_str());
            const float fraction = c.target > 0 ? std::clamp(static_cast<float>(c.progress) / c.target, 0.0f, 1.0f) : 0.0f;
            char bar[64];
            std::snprintf(bar, sizeof(bar), "%lld / %lld   %lld XP", static_cast<long long>(c.progress),
                          static_cast<long long>(c.target), static_cast<long long>(c.reward));
            ImGui::ProgressBar(fraction, ImVec2(-1.0f, 4.0f * s), "");
            ImGui::TextUnformatted(bar);
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::PopID();
        }
    }
    if (shown == 0) ImGui::TextDisabled("No challenges yet - finish a match to start them.");
    ImGui::EndChild();
    ImGui::EndGroup();

    // ---- Act on the selection ----
    if (activated == 0) {
        Close();
    } else if (activated == 1) {
        MenuSounds::Click();
        Close();
        if (!Relaunch::HandOff(restartArgs)) NOBLE_WARN("PauseMenu: restart failed, the match keeps running");
        else NOBLE_INFO("PauseMenu: restarting the match");
    } else if (activated == 2) {
        MenuSounds::Click();
        Window::TakeLeftClicks();
        s_showSettings.store(true);
        NOBLE_INFO("PauseMenu: settings opened from the pause menu");
    } else if (activated == 3) {
        MenuSounds::Click();
        Window::TakeLeftClicks();
        PauseSettings().SetTab(InputPrompts::Current() == InputPrompts::Device::Gamepad ? 5 : 4);   // Controller, or Keyboard & Mouse
        s_showSettings.store(true);
        NOBLE_INFO("PauseMenu: controls opened from the pause menu");
    } else if (activated == 4) {
        // The engine can't switch maps once started, so start a fresh Noble with no -map: that opens the menu.
        MenuSounds::Click();
        Close();
        RelaunchAndClose(hwnd, L"", "leave game");
    } else if (activated == 5) {
        NOBLE_INFO("PauseMenu: quit requested");
        Close();
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    } else if (interactive && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
        // B resumes. Esc is not read here: the input poll already toggles on Esc, and closing here as well reopened the
        // menu when both saw the same press.
        Close();
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, NobleStyle::TextMuted());
    ImGui::TextUnformatted("Esc resumes");
    ImGui::PopStyleColor();
    ImGui::End();
}

// The front-end SETTINGS panel, reused while paused so settings can be changed
// during a match. Every value that Noble can reach live (FOV, VSync, frame cap,
// mouse, audio, key bindings) is applied on APPLY; the panels' own help lines
// list the few that need a restart.
namespace {

void DrawSettingsPage(HWND hwnd) {
  (void)hwnd;
  ImGuiIO &io = ImGui::GetIO();
  // Full screen (small margin): the three-column settings need the room.
  const float width = io.DisplaySize.x - 32.0f;
  const float height = io.DisplaySize.y - 32.0f;
  ImGui::SetNextWindowPos(
      ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
      ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings;
  if (ImGui::Begin("##NoblePauseSettings", nullptr, flags)) {
    NobleStyle::Header("SETTINGS");
    if (ImGui::Button("< BACK", ImVec2(110, 0))) {
      s_showSettings.store(false);
      MenuSounds::Back();
      NOBLE_INFO("PauseMenu: settings closed");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Applied changes also reach this match. B goes back to "
                        "the pause menu.");
    ImGui::Separator();
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) && !PauseSettings().IsCapturingKey() &&
        !ImGui::GetIO().WantTextInput) {
      s_showSettings.store(false);
      MenuSounds::Back();
    }
    PauseSettings().Render();
  }
  ImGui::End();
}

} // namespace

// ---------------------------------------------------------------- post-game
// "exit experience" Reach-style staged reveal (spec 4.8): scoreboard, then each
// cR line counting up (Totals, or the slot machine for a random bonus), the
// total, challenge / commendation progress bars, and the rank bar filling up to
// the new total (RankUp and the new icon on a promotion). Enter, Space or a
// click on an empty spot skips to the end. PLAY AGAIN / RETURN TO MENU / QUIT
// work at any time.
namespace {

struct Reveal {
  ULONGLONG start = 0; // s_gameOverAt the reveal belongs to
  bool skipped = false;
  int linesSounded = 0;
  bool completionSounded = false;
  bool totalSounded = false;
  bool rankUpSounded = false;
  bool keyWasDown = true; // ignore a key still held from the game
};
Reveal s_reveal;

constexpr float kFirstLine = 0.8f, kLineStep = 0.5f, kLineCount = 0.35f,
                kSlotRoll = 0.9f;

float Ease(float x) {
  x = std::clamp(x, 0.0f, 1.0f);
  return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x);
}

bool IsSlotLine(const Career::AwardLine &l) {
  return l.label == "Random bonus" || l.label == "Jackpot!";
}

// The credit count and bar sounds repeat several times in a few seconds and sat well above the rest of the UI.
constexpr float kCountSfxVolume = 0.25f;

// Follows the Other volume slider, like the menu sounds.
void Sfx(ReachAudio::Sfx sfx, float volume = 1.0f) {
  const auto settings = Settings::Get().Snapshot();
  if (settings.menuSounds)
    ReachAudio::PlaySfx(sfx, volume * settings.volumeOther);
}

std::string Grouped(long long v) {
  std::string digits = std::to_string(v < 0 ? -v : v), out;
  for (size_t i = 0; i < digits.size(); ++i) {
    if (i && (digits.size() - i) % 3 == 0)
      out.push_back(',');
    out.push_back(digits[i]);
  }
  return v < 0 ? "-" + out : out;
}

// Section heading drawn straight into a draw list (letter-spaced caps + a
// rule).
void Heading(ImDrawList *dl, ImVec2 pos, float width, float s,
             const char *text) {
  ImFont *f = NobleStyle::SmallFont();
  const float px = 15.0f * s;
  float x = pos.x;
  char ch[2] = {0, 0};
  for (const char *c = text; *c; ++c) {
    ch[0] = *c;
    NobleUi::Text(dl, f, px, ImVec2(x, pos.y), IM_COL32(150, 190, 220, 255),
                  ch);
    x += NobleUi::TextWidth(f, px, ch) + 1.8f * s;
  }
  if (x + 10.0f * s < pos.x + width)
    dl->AddLine(ImVec2(x + 10.0f * s, pos.y + px * 0.55f),
                ImVec2(pos.x + width, pos.y + px * 0.55f), NobleUi::kRule,
                1.0f);
}

} // namespace


// Test option "-scoreboard-demo N": draws the panel with a sample roster as the host, without needing a real session.
// 1 = as seen while the key is held, 2 = pointer free with a player selected, 3 = the Report window as well.
static int ScoreboardDemo() {
  static const int s_demo = [] {
    int value = 0, argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i + 1 < argc; ++i) {
      if (_wcsicmp(argv[i], L"-scoreboard-demo") == 0)
        value = _wtoi(argv[i + 1]);
    }
    if (argv)
      LocalFree(argv);
    return value;
  }();
  return s_demo;
}

// A report that could not reach the master server stays on this PC (one JSON object per line).
static bool SaveReport(const std::string &player, const char *reason, const std::string &map, const std::string &mode) {
  wchar_t exe[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  const std::filesystem::path file = std::filesystem::path(exe).parent_path() / "data" / "userdata" / "reports.jsonl";
  std::error_code ec;
  std::filesystem::create_directories(file.parent_path(), ec);
  std::ofstream out(file, std::ios::app);
  if (!out)
    return false;
  const nlohmann::json line = {{"time", static_cast<long long>(std::time(nullptr))}, {"player", player}, {"reason", reason}, {"map", map}, {"mode", mode}};
  out << line.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) << "\n";
  return out.good();
}

// A button that cannot work yet still shows, greyed, and says why when hovered.
static void UnavailableButton(const char *label, const ImVec2 &size, const char *why) {
  ImGui::BeginDisabled();
  ImGui::Button(label, size);
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("%s", why);
}

// In-match scoreboard. Noble's panel replaces Reach's own scoreboard (ReachHost keeps the key from the engine) and lists
// the session roster with rank badges, service tags and team colours. A click while the key is held frees the pointer for
// the player actions.
static void DrawScoreboardPanel() {
  // Real RTT: the transport already measures per-peer ping (GetPeerPingMs); overlay it on the server roster values, which
  // are placeholders. Unknown peers keep whatever the server sent.
  for (auto &row : s_boardRows) {
    if (row.machine == 0) continue;
    const int ms = NetworkTransport::Get().GetPeerPingMs(row.machine);
    if (ms >= 0) row.ping = ms;
  }

  using namespace NobleUi;
  const int demo = ScoreboardDemo();
  const bool pointer = s_boardPointer.load();
  const bool isHost = demo > 0 || (NetworkTransport::Get().IsActive() && NetworkTransport::Get().IsHost());
  static std::string s_selected;
  static std::string s_status;
  static int s_reportReason = 0;
  static std::atomic<int> s_reportResult{0};   // 0 = nothing pending, 1 = sent, 2 = saved on this PC, 3 = failed
  static std::atomic<bool> s_reportSending{false};
  ImGuiIO &io = ImGui::GetIO();
  const float W = io.DisplaySize.x, H = io.DisplaySize.y;
  const float s = std::clamp(H / 1080.0f, 0.7f, 2.0f);

  std::vector<PauseMenu::ScoreboardEntry> rows;
  std::string mapName, modeName;
  {
    std::lock_guard<std::mutex> lock(s_boardMutex);
    rows = s_boardRows;
    mapName = s_boardMap;
    modeName = s_boardMode;
  }
  if (rows.empty()) {
    PauseMenu::ScoreboardEntry me;
    me.name = Settings::Get().Snapshot().playerName;
    me.local = true;
    me.rank = Career::RankIndex(Career::Get().credits);
    rows.push_back(me);
  }
  if (demo > 0) {
    rows[0].team = 1;
    const char *const names[3] = {"Carter", "Kat", "Jorge"};
    for (int i = 0; i < 3; ++i) {
      PauseMenu::ScoreboardEntry e;
      e.name = names[i];
      e.tag = i == 0 ? "A259" : i == 1 ? "B320" : "S052";
      e.rank = 10 + i * 8;
      e.team = 1 + i % 2;
      e.machine = 1001ULL + static_cast<uint64_t>(i);
      rows.push_back(e);
    }
    {
      static const int s_extra = [] {
        int value = 0, argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i + 1 < argc; ++i)
          if (_wcsicmp(argv[i], L"-scoreboard-demo-players") == 0) value = _wtoi(argv[i + 1]);
        if (argv) LocalFree(argv);
        return std::clamp(value, 0, 128);
      }();
      for (int i = static_cast<int>(rows.size()); i < s_extra; ++i) {
        PauseMenu::ScoreboardEntry e;
        e.name = "Spartan " + std::to_string(i + 1);
        e.tag = "N" + std::to_string(i + 10);
        e.rank = i % 30;
        e.team = 1 + i % 2;
        rows.push_back(e);
      }
    }
    if (mapName.empty()) { mapName = "20_sword_slayer"; modeName = "slayer"; }
    if (s_selected.empty() && demo >= 2) s_selected = names[0];
    if ((demo == 3 || demo == 5) && s_boardPointer.load()) { static bool s_once = false; if (!s_once) { s_once = true; s_boardReport.store(true); } }
  }
  std::sort(rows.begin(), rows.end(), [](const PauseMenu::ScoreboardEntry &a,
                                         const PauseMenu::ScoreboardEntry &b) {
    if (a.team != b.team) return a.team < b.team;
    if (a.local != b.local) return a.local;
    return a.name < b.name;
  });

  ImDrawList *dl = ImGui::GetBackgroundDrawList();
  const float panelW = std::min(W * 0.72f, 1150.0f * s);
  const float rowH = 46.0f * s;
  const float top = 140.0f * s;
  const float barH = 86.0f * s; // hint or status line + action buttons
  // Red on the left and blue on the right, each scrolling on its own; a game without teams is one column. The panel
  // is sized from what fits, so 16 players no longer run off the bottom of a 1080p screen and 128 stay usable.
  bool hasTeams = false;
  for (const auto &r : rows) hasTeams = hasTeams || r.team == 1 || r.team == 2;
  std::vector<int> columnRows[2];
  for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    columnRows[hasTeams && rows[i].team == 2 ? 1 : 0].push_back(i);
  const int columnCount = hasTeams ? 2 : 1;
  const int maxVisible = std::max(4, static_cast<int>((H - top - 118.0f * s - 26.0f * s - barH - 30.0f * s) / rowH));
  const int tallest = static_cast<int>(std::max(columnRows[0].size(), columnRows[1].size()));
  const int visibleRows = std::min(maxVisible, tallest);
  const float panelH = 118.0f * s + rowH * static_cast<float>(visibleRows) + 26.0f * s + barH;
  const ImVec2 p0((W - panelW) * 0.5f, top), p1(p0.x + panelW, top + panelH);
  if (pointer) {
    ImGui::SetNextWindowPos(p0, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
    ImGui::Begin("##scoreboard", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
  }
  dl->AddRectFilled(p0, p1, IM_COL32(8, 14, 22, 232), 6.0f * s);
  dl->AddRect(p0, p1, IM_COL32(70, 120, 165, 200), 6.0f * s, 0, 1.5f * s);
  dl->AddRectFilled(ImVec2(p0.x, p0.y), ImVec2(p0.x + 6.0f * s, p1.y), kAccent, 6.0f * s);

  Text(dl, NobleStyle::TitleFont(), 34.0f * s, ImVec2(p0.x + 34.0f * s, p0.y + 24.0f * s),
       IM_COL32(236, 240, 244, 255), "SCOREBOARD");
  std::string sub = mapName.empty() ? std::string() : Upper(MapNames::Display(mapName));
  if (!modeName.empty()) sub += (sub.empty() ? "" : "  -  ") + Upper(modeName);
  Text(dl, NobleStyle::SmallFont(), 15.0f * s, ImVec2(p0.x + 36.0f * s, p0.y + 66.0f * s),
       IM_COL32(150, 190, 220, 255), sub.c_str());

  // Index = LobbyPlayer::team: 0 = no team, 1 = red, 2 = blue.
  static const ImU32 kTeamColors[3] = {IM_COL32(120, 132, 145, 255), IM_COL32(214, 72, 72, 255), IM_COL32(74, 132, 214, 255)};
  static int s_boardScroll[2] = {0, 0};
  bool selectedIsListed = false;
  for (const auto &r : rows)
    selectedIsListed = selectedIsListed || (!r.local && r.name == s_selected);
  // Controller: A takes the panel while Back is held. Then the D-pad or stick walks the other players, on the same selection
  // the mouse uses, and the buttons below act on it.
  const auto padPressed = [](ImGuiKey dpad, ImGuiKey stick) { return ImGui::IsKeyPressed(dpad) || ImGui::IsKeyPressed(stick); };
  if (!pointer && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false)) PauseMenu::EnterScoreboardPointer();
  if (pointer && !s_boardReport.load()) {
    int column = 0, row = -1;
    for (int c = 0; c < columnCount; ++c)
      for (int k = 0; k < static_cast<int>(columnRows[c].size()); ++k)
        if (!rows[columnRows[c][k]].local && rows[columnRows[c][k]].name == s_selected) { column = c; row = k; }
    const auto pick = [&](int c, int from, int step) {
      const std::vector<int> &list = columnRows[c];
      const int count = static_cast<int>(list.size());
      for (int k = std::clamp(from, 0, std::max(count - 1, 0)), tries = 0; count > 0 && k >= 0 && k < count && tries <= count; k += step, ++tries) {
        if (rows[list[k]].local) continue;
        s_selected = rows[list[k]].name;
        s_status.clear();
        selectedIsListed = true;
        s_boardScroll[c] = std::clamp(s_boardScroll[c], k - std::max(visibleRows - 1, 0), k);
        MenuSounds::Scroll();
        return true;
      }
      return false;
    };
    if (padPressed(ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadLStickDown)) { if (!pick(column, row + 1, 1) && columnCount > 1) pick(1 - column, 0, 1); }
    if (padPressed(ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadLStickUp) && row > 0) pick(column, row - 1, -1);
    if (columnCount > 1 && (padPressed(ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadLStickLeft) || padPressed(ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickRight)))
      pick(1 - column, std::max(row, 0), 1);
  }

  const float headY = p0.y + 104.0f * s;
  const float listY = headY + 26.0f * s;
  const float columnGap = 12.0f * s;
  const float columnW = (panelW - 48.0f * s - columnGap * (columnCount - 1)) / columnCount;
  for (int col = 0; col < columnCount; ++col) {
    const std::vector<int> &list = columnRows[col];
    const int count = static_cast<int>(list.size());
    const float cx = p0.x + 24.0f * s + col * (columnW + columnGap);
    const bool overflow = count > visibleRows;
    if (overflow && pointer && !s_boardReport.load() &&
        ImGui::IsMouseHoveringRect(ImVec2(cx, listY), ImVec2(cx + columnW, listY + rowH * visibleRows), false) && io.MouseWheel != 0.0f)
      s_boardScroll[col] -= static_cast<int>(io.MouseWheel) * 2;
    s_boardScroll[col] = std::clamp(s_boardScroll[col], 0, std::max(0, count - visibleRows));
    const int scroll = s_boardScroll[col];
    const float tagX = cx + columnW - 190.0f * s;
    const float pingX = cx + columnW - 62.0f * s;

    // Sticky header: the team, then the columns.
    const ImU32 headCol = hasTeams ? kTeamColors[col + 1] : IM_COL32(130, 170, 200, 255);
    char title[48];
    if (overflow) std::snprintf(title, sizeof(title), "%s  %d-%d of %d", hasTeams ? (col == 0 ? "RED TEAM" : "BLUE TEAM") : "PLAYERS", scroll + 1, scroll + visibleRows, count);
    else std::snprintf(title, sizeof(title), "%s  %d", hasTeams ? (col == 0 ? "RED TEAM" : "BLUE TEAM") : "PLAYERS", count);
    Text(dl, NobleStyle::SmallFont(), 14.0f * s, ImVec2(cx + 10.0f * s, headY), headCol, title);
    Text(dl, NobleStyle::SmallFont(), 14.0f * s, ImVec2(tagX, headY), IM_COL32(130, 170, 200, 255), "TAG");
    Text(dl, NobleStyle::SmallFont(), 14.0f * s, ImVec2(pingX, headY), IM_COL32(130, 170, 200, 255), "PING");

    // The local player stays in view: while their row is scrolled away it takes the last visible row.
    int localRow = -1;
    for (int k = 0; k < count; ++k)
      if (rows[list[k]].local) localRow = k;
    const bool pinLocal = localRow >= 0 && (localRow < scroll || localRow >= scroll + visibleRows);
    for (int v = 0; v < visibleRows && scroll + v < count; ++v) {
      const auto &r = rows[list[(pinLocal && v == visibleRows - 1) ? localRow : scroll + v]];
      const float y = listY + v * rowH;
      const ImVec2 r0(cx, y), r1(cx + columnW - (overflow ? 6.0f * s : 0.0f), y + rowH - 6.0f * s);
      bool hovered = false;
      if (pointer && !r.local) {
        ImGui::SetCursorScreenPos(r0);
        ImGui::BeginDisabled(s_boardReport.load());
        if (ImGui::InvisibleButton(r.name.c_str(), ImVec2(r1.x - r0.x, r1.y - r0.y))) {
          s_selected = r.name;
          s_status.clear();
          selectedIsListed = true;
          MenuSounds::Click();
        }
        hovered = ImGui::IsItemHovered();
        ImGui::EndDisabled();
      }
      const bool selected = !r.local && r.name == s_selected; // after the click, or a new selection is lost this frame
      dl->AddRectFilled(r0, r1, r.local ? IM_COL32(40, 80, 115, 200) : selected ? IM_COL32(70, 120, 165, 170) : hovered ? IM_COL32(255, 255, 255, 34) : IM_COL32(255, 255, 255, 12), 3.0f * s);
      if (selected)
        dl->AddRect(r0, r1, IM_COL32(143, 216, 255, 255), 3.0f * s, 0, 1.5f * s);
      const ImU32 teamCol = kTeamColors[r.team == 1 || r.team == 2 ? r.team : 0];
      dl->AddRectFilled(r0, ImVec2(r0.x + 4.0f * s, r1.y), teamCol, 3.0f * s);

      float nx = r0.x + 14.0f * s;
      const UiTexture icon = UiArt::RankIcon(r.rank);
      const float ty = y + (rowH - 6.0f * s) * 0.5f - 11.0f * s;
      if (icon) {
        UiArt::DrawContain(dl, icon, ImVec2(nx, ty - 2.0f * s), ImVec2(nx + 30.0f * s, ty + 26.0f * s));
        nx += 38.0f * s;
      }
      dl->PushClipRect(ImVec2(nx, r0.y), ImVec2(tagX - 6.0f * s, r1.y), true);
      Text(dl, ImGui::GetFont(), 19.0f * s, ImVec2(nx, ty),
           r.local ? IM_COL32(143, 216, 255, 255) : IM_COL32(236, 240, 244, 255), r.name.c_str());
      if (r.local)
        Text(dl, NobleStyle::SmallFont(), 12.0f * s, ImVec2(nx + TextWidth(ImGui::GetFont(), 19.0f * s, r.name.c_str()) + 10.0f * s, ty + 4.0f * s),
             IM_COL32(143, 216, 255, 200), "YOU");
      dl->PopClipRect();
      Text(dl, ImGui::GetFont(), 17.0f * s, ImVec2(tagX, ty), IM_COL32(200, 210, 220, 255), r.tag.c_str());
      SocialOverlay::DrawPlayerVoiceIcon(dl, ImVec2(tagX - 18.0f * s, (r0.y + r1.y) * 0.5f), s, r.machine, r.local);
      // Live round trip from the transport. The roster's own figure is a placeholder, and this PC can only measure
      // the peers it talks to, so anyone else shows "-".
      int ping = r.ping;
      if (!r.local && NetworkTransport::Get().IsActive()) {
        const int live = r.hostPeer ? NetworkTransport::Get().GetPeerPingMs(0) : r.machine ? NetworkTransport::Get().GetPeerPingMs(r.machine) : -1;
        if (live > 0) ping = live;
      }
      const std::string pingText = ping >= 0 ? std::to_string(ping) : "-";
      Text(dl, ImGui::GetFont(), 17.0f * s, ImVec2(pingX, ty), IM_COL32(200, 210, 220, 255), pingText.c_str());
    }
    if (overflow) {
      const float trackX = cx + columnW - 3.0f * s;
      const float trackH = rowH * visibleRows - 6.0f * s;
      dl->AddRectFilled(ImVec2(trackX, listY), ImVec2(trackX + 3.0f * s, listY + trackH), IM_COL32(255, 255, 255, 24));
      const float thumbH = std::max(trackH * visibleRows / count, 18.0f * s);
      const float thumbY = listY + (trackH - thumbH) * scroll / std::max(1, count - visibleRows);
      dl->AddRectFilled(ImVec2(trackX, thumbY), ImVec2(trackX + 3.0f * s, thumbY + thumbH), kAccent);
    }
  }
  if (!selectedIsListed)
    s_selected.clear();
  const float y = listY + visibleRows * rowH;

  // A report finished on its own thread.
  switch (s_reportResult.exchange(0)) {
  case 1: s_status = "Report sent"; break;
  case 2: s_status = "The server couldn't be reached, so the report was saved on this PC"; break;
  case 3: s_status = "The report could not be sent or saved"; break;
  default: break;
  }

  // Hint or status line, then the action bar.
  const ImVec2 barPos(p0.x + 34.0f * s, y + 6.0f * s);
  const char *line = !pointer ? "Click, or press A on a controller, for player actions"
                     : !s_status.empty() ? s_status.c_str()
                     : s_selected.empty() ? "Select a player with the mouse or D-pad"
                                          : nullptr;
  std::string selectedLine;
  if (!line) {
    selectedLine = "Selected: " + s_selected + "     Controller: Y mute   X report   LB mute all   B close";
    line = selectedLine.c_str();
  }
  Text(dl, NobleStyle::SmallFont(), 15.0f * s, barPos, IM_COL32(150, 190, 220, 255), line);

  if (pointer) {
    const ImVec2 button(126.0f * s, 34.0f * s);
    const float gap = 8.0f * s;
    const bool padActive = !s_boardReport.load();
    const bool padReport = padActive && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false);
    const bool padMute = padActive && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false);
    const bool padMuteAll = padActive && ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false);
    const bool padClose = padActive && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
    uint64_t selectedMachine = 0;
    bool allMuted = true, anyone = false;
    for (const auto &r : rows) {
      if (r.local || r.machine == 0) continue;
      if (r.name == s_selected) selectedMachine = r.machine;
      anyone = true;
      allMuted = allMuted && VoiceChat::IsPeerMuted(r.machine);
    }
    const bool selectedMuted = selectedMachine != 0 && VoiceChat::IsPeerMuted(selectedMachine);
    ImGui::BeginDisabled(s_boardReport.load());
    ImGui::SetCursorScreenPos(ImVec2(barPos.x, barPos.y + 30.0f * s));
    ImGui::BeginDisabled(selectedMachine == 0);
    if ((ImGui::Button(selectedMuted ? "UNMUTE" : "MUTE", button) || padMute) && selectedMachine != 0) {
      VoiceChat::SetPeerMuted(selectedMachine, !selectedMuted);
      s_status = std::string(selectedMuted ? "Unmuted " : "Muted ") + s_selected;
      MenuSounds::Click();
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(!anyone);
    if ((ImGui::Button(anyone && allMuted ? "UNMUTE ALL" : "MUTE ALL", button) || padMuteAll) && anyone) {
      for (const auto &r : rows)
        if (!r.local && r.machine != 0) VoiceChat::SetPeerMuted(r.machine, !allMuted);
      s_status = allMuted ? "Unmuted everyone" : "Muted everyone";
      MenuSounds::Click();
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(s_selected.empty());
    if ((ImGui::Button("REPORT", button) || padReport) && !s_selected.empty()) {
      s_reportReason = 0;
      s_boardReport.store(true);
      MenuSounds::Click();
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, gap);
    UnavailableButton("VOTE TO KICK", button, "Not available yet: the host cannot remove a player from the match.");
    if (isHost) {
      ImGui::SameLine(0.0f, gap);
      // mp_round_end's work function (haloreach+0x7B3EC) does not end a multiplayer round: tried twice on 2026-10-31, the
      // timer kept running. These stay grey until the multiplayer end path is found.
      UnavailableButton("END ROUND", button, "Not available yet: the way to end a multiplayer round in Reach's engine isn't known.");
      ImGui::SameLine(0.0f, gap);
      UnavailableButton("END MATCH", button, "Not available yet: the way to end a multiplayer match in Reach's engine isn't known.");
    }
    ImGui::SameLine(p1.x - p0.x - button.x - 34.0f * s);
    if (ImGui::Button("CLOSE", button) || padClose) {
      s_boardPointer.store(false);
      MenuSounds::Back();
    }
    ImGui::EndDisabled();
    ImGui::End();

    if (s_boardReport.load()) {
      static const char *const kReasons[4] = {"Cheating", "Abusive name or chat", "Griefing or team killing", "Something else"};
      ImGui::SetNextWindowFocus();
      CenteredWindow(460.0f * s);
      ImGui::Begin("##scoreboardreport", nullptr, kPanelFlags | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
      if (padPressed(ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadLStickDown)) s_reportReason = (s_reportReason + 1) % 4;
      if (padPressed(ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadLStickUp)) s_reportReason = (s_reportReason + 3) % 4;
      NobleStyle::Header(("REPORT " + Upper(s_selected)).c_str());
      ImGui::Spacing();
      for (int i = 0; i < 4; ++i)
        ImGui::RadioButton(kReasons[i], &s_reportReason, i);
      ImGui::Spacing();
      ImGui::TextWrapped("This goes to the Project Noble team, with the map, the mode and your name.");
      ImGui::Spacing();
      const float half = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
      static int s_demoSend = 0;
      const bool demoSend = demo == 5 && !s_reportSending.load() && ++s_demoSend == 240;   // test: send once the dialog is up
      if (NobleStyle::PrimaryButton("SEND REPORT", ImVec2(half, 38.0f * s)) || demoSend || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false)) {
        // On its own thread: the request can take seconds and this is the game's render thread.
        s_reportSending.store(true);
        std::string session;
        {
          std::lock_guard<std::mutex> lock(s_boardMutex);
          session = s_boardSession;
        }
        if (session.empty()) session = MasterServerClient::Get().GetCurrentSessionId();
        std::thread([session, target = s_selected, reason = std::string(kReasons[s_reportReason]), mapName, modeName] {
          const std::string reporter = Settings::Get().Snapshot().playerName;
          const bool sent = MasterServerClient::Get().SendReportSync(session, reporter, target, reason, mapName, modeName);
          const bool saved = sent || SaveReport(target, reason.c_str(), mapName, modeName);
          NOBLE_INFO("PauseMenu: report sent={} savedLocally={}", sent, !sent && saved);
          s_reportResult.store(sent ? 1 : saved ? 2 : 3);
          s_reportSending.store(false);
        }).detach();
        s_status = "Sending report";
        s_boardReport.store(false);
        MenuSounds::Click();
      }
      ImGui::SameLine(0.0f, gap);
      if (ImGui::Button("CANCEL", ImVec2(half, 38.0f * s)) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
        s_boardReport.store(false);
        MenuSounds::Back();
      }
      ImGui::End();
    }
  }
}


// Guest-side connection-lost panel: the host vanished (no packets for a while) but the engine keeps pumping frames, so
// without this the guest just sits frozen in the mission. Offers the only reliable exit - relaunch the menu process.
static bool DrawConnectionLostPanel() {
  using namespace NobleUi;
  ImGuiIO &io = ImGui::GetIO();
  const float W = io.DisplaySize.x, H = io.DisplaySize.y;
  const float s = std::clamp(H / 1080.0f, 0.7f, 2.0f);
  ImDrawList *dl = ImGui::GetBackgroundDrawList();
  dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(4, 6, 10, 210));
  const float panelW = std::min(W * 0.5f, 720.0f * s), panelH = 260.0f * s;
  const ImVec2 p0((W - panelW) * 0.5f, (H - panelH) * 0.5f), p1(p0.x + panelW, p0.y + panelH);
  dl->AddRectFilled(p0, p1, IM_COL32(10, 16, 26, 245), 8.0f * s);
  dl->AddRect(p0, p1, IM_COL32(70, 120, 165, 220), 8.0f * s, 0, 2.0f * s);
  dl->AddRectFilled(ImVec2(p0.x, p0.y), ImVec2(p0.x + 6.0f * s, p1.y), kAccent, 8.0f * s);
  Text(dl, NobleStyle::TitleFont(), 34.0f * s, ImVec2(p0.x + 36.0f * s, p0.y + 30.0f * s),
       IM_COL32(236, 240, 244, 255), "CONNECTION LOST");
  Text(dl, ImGui::GetFont(), 20.0f * s, ImVec2(p0.x + 38.0f * s, p0.y + 84.0f * s),
       IM_COL32(200, 210, 220, 255), "The host connection was lost.");
  Text(dl, NobleStyle::SmallFont(), 15.0f * s, ImVec2(p0.x + 38.0f * s, p0.y + 116.0f * s),
       IM_COL32(150, 190, 220, 255), "Holding the match open for host migration. A / Enter or click returns to the menu.");

  ImGui::SetNextWindowPos(ImVec2(p0.x + panelW * 0.5f - 120.0f * s, p0.y + panelH - 78.0f * s));
  ImGui::SetNextWindowSize(ImVec2(240.0f * s, 52.0f * s));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
  ImGui::Begin("##NobleConnectionLost", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
  const bool activate = ImGui::Button("Return to menu", ImVec2(240.0f * s, 44.0f * s)) ||
                        ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                        ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false);
  if (activate) {
    NOBLE_INFO("PauseMenu: connection lost - returning to the menu");
    ImGui::End();
    ImGui::PopStyleColor();
    if (!Relaunch::HandOff(L"")) {
      NOBLE_ERROR("PauseMenu: relaunch to menu failed; keeping the connection-lost panel");
    }
    return true;
  }
  ImGui::End();
  ImGui::PopStyleColor();
  return false;
}

void PauseMenu::DrawGameOver(HWND hwnd) {
  using namespace NobleUi;
  ImGuiIO &io = ImGui::GetIO();
  const float W = io.DisplaySize.x, H = io.DisplaySize.y;
  const float s = std::clamp(H / 1080.0f, 0.7f, 2.0f);
  const Career::Award a = Career::LastAward();
  const auto &ranks = Career::Ranks();

  std::string mapName;
  {
    std::lock_guard<std::mutex> lock(s_mapMutex);
    mapName = s_loadingMapName;
  }

  // Reveal clock (restarts for every new results screen).
  const ULONGLONG start = s_gameOverAt.load();
  if (s_reveal.start != start) {
    s_reveal = Reveal{};
    s_reveal.start = start;
  }
  const int lineCount = a.valid ? static_cast<int>(a.lines.size()) : 0;
  const float tTotal = kFirstLine + lineCount * kLineStep + 0.2f;
  const float tRank = tTotal + 0.9f;
  const float tEnd = tRank + 1.6f;
  float t = static_cast<float>(GetTickCount64() - start) / 1000.0f;

  // Skip: Enter / Space (fresh press, window in front) or a click away from the
  // buttons.
  bool skipKey = false;
  if (GetForegroundWindow() == hwnd) {
    const bool down = (GetAsyncKeyState(VK_RETURN) & 0x8000) ||
                      (GetAsyncKeyState(VK_SPACE) & 0x8000);
    skipKey = down && !s_reveal.keyWasDown;
    s_reveal.keyWasDown = down;
  }
  if (skipKey && t < tEnd)
    s_reveal.skipped = true;
  if (s_reveal.skipped)
    t = tEnd + 1.0f;

  if (!s_reveal.completionSounded) {
    s_reveal.completionSounded = true;
    if (!s_reveal.skipped)
      Sfx(ReachAudio::Sfx::GameCompletion);
  }

  ImDrawList *bg = ImGui::GetBackgroundDrawList();
  bg->AddRectFilledMultiColor(
      ImVec2(0, 0), ImVec2(W, H), IM_COL32(6, 12, 20, 235),
      IM_COL32(6, 12, 20, 235), IM_COL32(2, 4, 8, 245), IM_COL32(2, 4, 8, 245));

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(W, H));
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
      ImGuiWindowFlags_NoBringToFrontOnFocus;
  if (!ImGui::Begin("##NobleGameOver", nullptr, flags)) {
    ImGui::End();
    return;
  }
  ImDrawList *dl = ImGui::GetWindowDrawList();
  ImFont *hero = NobleStyle::HeroFont();
  ImFont *title = NobleStyle::TitleFont();
  ImFont *smallFont = NobleStyle::SmallFont();
  ImFont *body = ImGui::GetFont();
  const float margin = 64.0f * s;

  // ---- Header: GAME OVER, map, mode, time, difficulty.
  {
    const float d = 15.0f * s;
    const ImVec2 c(margin + d, 70.0f * s);
    dl->AddQuad(ImVec2(c.x, c.y - d), ImVec2(c.x + d, c.y),
                ImVec2(c.x, c.y + d), ImVec2(c.x - d, c.y), kAccent, 2.0f * s);
    dl->AddQuad(ImVec2(c.x, c.y - d * 0.45f), ImVec2(c.x + d * 0.45f, c.y),
                ImVec2(c.x, c.y + d * 0.45f), ImVec2(c.x - d * 0.45f, c.y),
                kAccent, 2.0f * s);
    Text(dl, hero, 56.0f * s, ImVec2(margin + 2.6f * d, 40.0f * s), kText,
         "GAME OVER");
    std::string sub = Upper(mapName.empty() ? a.map : mapName);
    if (a.valid) {
      char extra[96];
      std::snprintf(extra, sizeof(extra), "%s%s  %.0f MIN",
                    sub.empty() ? "" : "  -  ", Upper(a.type).c_str(),
                    a.minutes);
      sub += extra;
    }
    float x = margin + 2.6f * d;
    const float y = 104.0f * s;
    Text(dl, title, 24.0f * s, ImVec2(x, y), kTextDim, sub.c_str());
    x += TextWidth(title, 24.0f * s, sub.c_str()) + 20.0f * s;
    if (a.valid && a.difficulty >= 0) {
      static const char *const kDiff[4] = {"EASY", "NORMAL", "HEROIC",
                                           "LEGENDARY"};
      const int dIdx = std::clamp(a.difficulty, 0, 3);
      const UiTexture icon = UiArt::DifficultyIcon(dIdx);
      if (icon) {
        UiArt::DrawContain(dl, icon, ImVec2(x, y - 2.0f * s),
                           ImVec2(x + 30.0f * s, y + 28.0f * s));
        x += 36.0f * s;
      }
      Text(dl, title, 24.0f * s, ImVec2(x, y), kTextDim, kDiff[dIdx]);
    }
  }

  const float top = 170.0f * s;
  const float bottom = H - 120.0f * s;
  const float split = W * 0.58f;
  const float leftW = split - margin - 24.0f * s;
  const float rightX = split + 24.0f * s, rightW = W - margin - rightX;

  // ---- Left: scoreboard.
  float y = top;
  Heading(dl, ImVec2(margin, y), leftW, s, "SCOREBOARD");
  y += 30.0f * s;
  if (a.valid && !a.players.empty()) {
    const float colW = 74.0f * s;
    const char *const heads[5] = {"SCORE", "K", "D", "A", "MEDALS"};
    const float px = 15.0f * s;
    for (int c = 0; c < 5; ++c) {
      const float cx = margin + leftW - (5 - c) * colW;
      Text(dl, smallFont, px,
           ImVec2(cx + colW - TextWidth(smallFont, px, heads[c]) - 8.0f * s, y),
           kTextDim, heads[c]);
    }
    Text(dl, smallFont, px, ImVec2(margin + 12.0f * s, y), kTextDim, "PLAYER");
    y += 24.0f * s;
    const float rowH = 38.0f * s;
    const float appear = std::clamp(t / 0.4f, 0.0f, 1.0f);
    const int alpha = static_cast<int>(255 * appear);
    int shown = 0;
    for (const Career::ScoreRow &r : a.players) {
      if (y + rowH > top + (bottom - top) * 0.52f)
        break;
      const ImVec2 r0(margin, y), r1(margin + leftW, y + rowH - 4.0f * s);
      dl->AddRectFilled(
          r0, r1,
          r.local ? IM_COL32(40, 80, 115, static_cast<int>(160 * appear))
                  : IM_COL32(255, 255, 255, static_cast<int>(12 * appear)),
          2.0f * s);
      if (r.local)
        dl->AddRectFilled(r0, ImVec2(r0.x + 3.0f * s, r1.y), kAccent);
      float nx = margin + 12.0f * s;
      if (r.local) {
        const UiTexture icon = UiArt::RankIcon(a.rankAfter);
        if (icon) {
          UiArt::DrawContain(dl, icon, ImVec2(nx, y + 3.0f * s),
                             ImVec2(nx + 28.0f * s, y + rowH - 7.0f * s),
                             IM_COL32(255, 255, 255, alpha));
          nx += 36.0f * s;
        }
      }
      const float ty = y + (rowH - 4.0f * s) * 0.5f - 10.0f * s;
      const ImU32 nameCol = r.local ? IM_COL32(143, 216, 255, alpha)
                                    : IM_COL32(236, 240, 244, alpha);
      Text(dl, body, 18.0f * s, ImVec2(nx, ty), nameCol, r.name.c_str());
      const long long vals[5] = {r.score, r.kills, r.deaths, r.assists,
                                 r.medals};
      for (int c = 0; c < 5; ++c) {
        const std::string v = Grouped(vals[c]);
        const float cx = margin + leftW - (5 - c) * colW;
        Text(
            dl, body, 18.0f * s,
            ImVec2(cx + colW - TextWidth(body, 18.0f * s, v.c_str()) - 8.0f * s,
                   ty),
            IM_COL32(236, 240, 244, alpha), v.c_str());
      }
      y += rowH;
      ++shown;
    }
    if (shown < static_cast<int>(a.players.size())) {
      char more[48];
      std::snprintf(more, sizeof(more), "+%d more",
                    static_cast<int>(a.players.size()) - shown);
      Text(dl, smallFont, 15.0f * s, ImVec2(margin + 12.0f * s, y), kTextDim,
           more);
      y += 22.0f * s;
    }
  } else {
    Text(dl, body, 18.0f * s, ImVec2(margin, y), kTextDim,
         "No results from this game.");
    y += 30.0f * s;
  }

  // ---- Left: challenge and commendation progress made in this game.
  y += 18.0f * s;
  Heading(dl, ImVec2(margin, y), leftW, s, "PROGRESS");
  y += 30.0f * s;
  if (!a.valid || a.progress.empty()) {
    Text(dl, body, 18.0f * s, ImVec2(margin, y), kTextDim,
         "No challenge or commendation progress this game.");
  } else {
    static const char *const kKind[3] = {"DAILY", "WEEKLY", "COMMENDATION"};
    const float rowH = 56.0f * s;
    int i = 0;
    for (const Career::ProgressLine &p : a.progress) {
      if (y + rowH > bottom - 8.0f * s) {
        char more[64];
        std::snprintf(more, sizeof(more), "+%d more in the Service Record",
                      static_cast<int>(a.progress.size()) - i);
        Text(dl, smallFont, 15.0f * s, ImVec2(margin, y), kTextDim, more);
        break;
      }
      const float k = Ease((t - (0.6f + i * 0.25f)) / 0.9f);
      const ImVec2 r0(margin, y), r1(margin + leftW, y + rowH - 6.0f * s);
      const bool done = p.completed && k >= 1.0f;
      const ImU32 green = IM_COL32(140, 217, 140, 255);
      dl->AddRectFilled(
          r0, r1, done ? IM_COL32(30, 52, 36, 200) : IM_COL32(22, 30, 40, 200),
          3.0f * s);
      float x = margin + 12.0f * s;
      if (p.kind == 2) {
        const UiTexture art = UiArt::ReachArt("commendations", p.art);
        if (art) {
          const float bh = rowH - 16.0f * s, bw = bh * (287.0f / 90.0f);
          UiArt::DrawContain(dl, art, ImVec2(x, y + 5.0f * s),
                             ImVec2(x + bw, y + 5.0f * s + bh));
          x += bw + 12.0f * s;
        }
      }
      const float chipPx = 13.0f * s;
      const float chipW =
          TextWidth(smallFont, chipPx, kKind[p.kind]) + 10.0f * s;
      dl->AddRect(ImVec2(x, y + 8.0f * s),
                  ImVec2(x + chipW, y + 12.0f * s + chipPx), kTextDim,
                  2.0f * s);
      Text(dl, smallFont, chipPx, ImVec2(x + 5.0f * s, y + 10.0f * s), kTextDim,
           kKind[p.kind]);
      Text(dl, body, 18.0f * s, ImVec2(x + chipW + 10.0f * s, y + 6.0f * s),
           done ? green : kText, p.title.c_str());
      // Bar from the old value to the new one (old part blue-grey, this game's
      // part amber).
      const float denom = static_cast<float>(std::max<int64_t>(1, p.target));
      const float fBefore =
          std::clamp(static_cast<float>(p.before) / denom, 0.0f, 1.0f);
      const float fAfter =
          std::clamp(static_cast<float>(p.after) / denom, 0.0f, 1.0f);
      const float f = fBefore + (fAfter - fBefore) * k;
      std::string label;
      if (done)
        label = p.kind == 2
                    ? Upper(Career::CommendationTierName(p.tier)) + " REACHED"
                    : "COMPLETE";
      else
        label = Grouped(static_cast<long long>(p.before +
                                               (p.after - p.before) * k)) +
                " / " + Grouped(p.target);
      const float lw = TextWidth(smallFont, 15.0f * s, label.c_str());
      const float bx0 = x, bx1 = r1.x - lw - 20.0f * s, by = y + 36.0f * s;
      dl->AddRectFilled(ImVec2(bx0, by), ImVec2(bx1, by + 5.0f * s),
                        IM_COL32(255, 255, 255, 26), 2.0f * s);
      dl->AddRectFilled(ImVec2(bx0, by),
                        ImVec2(bx0 + (bx1 - bx0) * fBefore, by + 5.0f * s),
                        IM_COL32(110, 150, 180, 255), 2.0f * s);
      if (f > fBefore)
        dl->AddRectFilled(ImVec2(bx0 + (bx1 - bx0) * fBefore, by),
                          ImVec2(bx0 + (bx1 - bx0) * f, by + 5.0f * s),
                          done ? green : kAmber, 2.0f * s);
      Text(dl, smallFont, 15.0f * s,
           ImVec2(r1.x - lw - 10.0f * s, by - 6.0f * s), done ? green : kText,
           label.c_str());
      y += rowH;
      ++i;
    }
  }

  // ---- Right: credits, counting up line by line.
  float ry = top;
  Heading(dl, ImVec2(rightX, ry), rightW, s, "CREDITS");
  ry += 34.0f * s;
  if (a.valid) {
    for (int i = 0; i < lineCount; ++i) {
      const Career::AwardLine &l = a.lines[i];
      const float lt = kFirstLine + i * kLineStep;
      if (t < lt)
        break;
      const bool slot = IsSlotLine(l);
      if (s_reveal.linesSounded <= i && !s_reveal.skipped) {
        s_reveal.linesSounded = i + 1;
        if (slot)
          Sfx(ReachAudio::Sfx::SlotMachine, kCountSfxVolume);
        else if (l.credits > 0)
          Sfx(ReachAudio::Sfx::Totals, kCountSfxVolume);
      }
      const float k =
          std::clamp((t - lt) / (slot ? kSlotRoll : kLineCount), 0.0f, 1.0f);
      long long shownValue = static_cast<long long>(l.credits * Ease(k));
      if (slot && k < 1.0f) { // slot machine: rolling digits until it lands
        shownValue = (static_cast<long long>(t * 37.0f) % 10) * 1000 +
                     (static_cast<long long>(t * 91.0f) % 10) * 100 +
                     (static_cast<long long>(t * 53.0f) % 10) * 10;
      }
      const int fade =
          static_cast<int>(255 * std::clamp((t - lt) / 0.2f, 0.0f, 1.0f));
      Text(dl, body, 19.0f * s, ImVec2(rightX, ry),
           IM_COL32(160, 172, 184, fade), l.label.c_str());
      const std::string v = "+" + Grouped(shownValue) + " cR";
      Text(dl, body, 19.0f * s,
           ImVec2(rightX + rightW - TextWidth(body, 19.0f * s, v.c_str()), ry),
           slot ? IM_COL32(255, 214, 110, 255) : kAmber, v.c_str());
      ry += 30.0f * s;
      if (ry > top + (bottom - top) * 0.48f && i + 1 < lineCount) {
        int64_t rest = 0;
        for (int j = i + 1; j < lineCount; ++j)
          rest += a.lines[j].credits;
        const std::string more = std::to_string(lineCount - i - 1) +
                                 " more  +" + Grouped(rest) + " cR";
        if (t >= kFirstLine + (i + 1) * kLineStep)
          Text(dl, smallFont, 15.0f * s, ImVec2(rightX, ry), kTextDim,
               more.c_str());
        ry += 24.0f * s;
        break;
      }
    }
    // Total.
    ry += 8.0f * s;
    dl->AddLine(ImVec2(rightX, ry), ImVec2(rightX + rightW, ry), kRule, 1.0f);
    ry += 12.0f * s;
    if (t >= tTotal) {
      if (!s_reveal.totalSounded && !s_reveal.skipped) {
        s_reveal.totalSounded = true;
        Sfx(ReachAudio::Sfx::Totals, kCountSfxVolume);
      }
      const float k = Ease((t - tTotal) / 0.8f);
      Text(dl, title, 26.0f * s, ImVec2(rightX, ry), kAccent, "CREDITS EARNED");
      const std::string v =
          "+" + Grouped(static_cast<long long>(a.total * k)) + " cR";
      Text(dl, title, 30.0f * s,
           ImVec2(rightX + rightW - TextWidth(title, 30.0f * s, v.c_str()),
                  ry - 2.0f * s),
           kAmber, v.c_str());
    }
    ry += 54.0f * s;

    // ---- Right: rank. The bar fills from the old total to the new one.
    Heading(dl, ImVec2(rightX, ry), rightW, s, "RANK");
    ry += 30.0f * s;
    const float k = t < tRank ? 0.0f : Ease((t - tRank) / 1.4f);
    const int64_t shownCredits =
        a.creditsBefore +
        static_cast<int64_t>((a.creditsAfter - a.creditsBefore) * k);
    const int shownRank = std::clamp(Career::RankIndex(shownCredits), 0,
                                     static_cast<int>(ranks.size()) - 1);
    const bool promoted = shownRank > a.rankBefore;
    if (promoted && !s_reveal.rankUpSounded) {
      s_reveal.rankUpSounded = true;
      Sfx(ReachAudio::Sfx::RankUp);
    }
    const float iconSize = 120.0f * s;
    if (promoted) {
      const float pulse =
          0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f);
      dl->AddCircleFilled(
          ImVec2(rightX + iconSize * 0.5f, ry + iconSize * 0.5f),
          iconSize * 0.62f,
          IM_COL32(255, 186, 74, static_cast<int>(30 + 40 * pulse)));
    }
    const UiTexture icon = UiArt::RankIcon(shownRank);
    if (icon)
      UiArt::DrawContain(dl, icon, ImVec2(rightX, ry),
                         ImVec2(rightX + iconSize, ry + iconSize));
    else
      dl->AddRect(ImVec2(rightX, ry), ImVec2(rightX + iconSize, ry + iconSize),
                  IM_COL32(255, 255, 255, 50));
    const float tx = rightX + iconSize + 22.0f * s;
    if (promoted)
      Text(dl, title, 22.0f * s, ImVec2(tx, ry + 4.0f * s), kAmber, "PROMOTED");
    Text(dl, title, 30.0f * s, ImVec2(tx, ry + (promoted ? 30.0f : 14.0f) * s),
         kText, ranks[shownRank].name);
    const float barY = ry + iconSize - 34.0f * s;
    if (shownRank + 1 < static_cast<int>(ranks.size())) {
      const int64_t lo = ranks[shownRank].credits,
                    hi = ranks[shownRank + 1].credits;
      const float f =
          std::clamp(static_cast<float>(shownCredits - lo) /
                         static_cast<float>(std::max<int64_t>(1, hi - lo)),
                     0.0f, 1.0f);
      const float bx1 = rightX + rightW;
      dl->AddRectFilled(ImVec2(tx, barY), ImVec2(bx1, barY + 8.0f * s),
                        IM_COL32(255, 255, 255, 26), 3.0f * s);
      dl->AddRectFilled(ImVec2(tx, barY),
                        ImVec2(tx + (bx1 - tx) * f, barY + 8.0f * s), kAccent,
                        3.0f * s);
      const std::string v = Grouped(shownCredits) + " / " + Grouped(hi) + " cR";
      Text(dl, smallFont, 15.0f * s, ImVec2(tx, barY + 14.0f * s), kTextDim,
           v.c_str());
      const std::string next =
          std::string("Next: ") + ranks[shownRank + 1].name;
      Text(dl, smallFont, 15.0f * s,
           ImVec2(bx1 - TextWidth(smallFont, 15.0f * s, next.c_str()),
                  barY + 14.0f * s),
           kTextDim, next.c_str());
    } else {
      Text(dl, smallFont, 15.0f * s, ImVec2(tx, barY + 14.0f * s), kTextDim,
           "Highest rank reached");
    }
  }

  // ---- Buttons (usable at any time) and the skip hint.
  const float bh = 52.0f * s, bw = 260.0f * s, gap = 16.0f * s;
  const float by = H - 88.0f * s;
  float bx = W - margin - bw;
  bool overButton = false;
  // What happens next in a hosted game. "-postgame-role host|guest" shows either one without a second PC.
  static const int s_roleTest = [] {
    const wchar_t *const line = GetCommandLineW();
    return wcsstr(line, L"-postgame-role host") ? 1 : wcsstr(line, L"-postgame-role guest") ? 2 : 0;
  }();
  const bool isCmdGuest = (wcsstr(GetCommandLineW(), L"-connect") != nullptr || Relaunch::CurrentGameArgs().find(L" -connect") != std::wstring::npos);
  const bool isCmdHost = (wcsstr(GetCommandLineW(), L"-host") != nullptr || Relaunch::CurrentGameArgs().find(L" -host") != std::wstring::npos);
  const bool online = NetworkTransport::Get().IsActive();
  const bool guestRole = s_roleTest == 2 || (s_roleTest == 0 && (isCmdGuest || (online && !NetworkTransport::Get().IsHost())));
  const bool hostRole = s_roleTest == 1 || (s_roleTest == 0 && !guestRole && (isCmdHost || (online && NetworkTransport::Get().IsHost())));
  const std::wstring again = Relaunch::CurrentGameArgs();
  // A guest's "again" would reconnect to a host who may have gone; only the host or a local player can start another.
  const bool guestPlayer = guestRole;
  const bool canPlayAgain = !again.empty() && !guestPlayer;
  const bool hasPlaylist = wcsstr(GetCommandLineW(), L"-lobby-playlist") != nullptr;
  const std::wstring nextMission = guestPlayer ? std::wstring() : NextCampaignMission(again);
  const bool canNextMission = !nextMission.empty();
  // Controller: left and right pick a button, A presses it, and A during the reveal only skips it. The focus starts away
  // from QUIT, so one stray A can never close the game; a host starts on going back to the lobby.
  enum { kPadQuit, kPadMenu, kPadAgain, kPadNext, kPadLobby };
  static int s_padButton = hostRole ? kPadLobby : kPadMenu;
  std::vector<int> padOrder = { kPadQuit, kPadMenu };
  if (canPlayAgain) padOrder.push_back(kPadAgain);
  if ((hostRole && hasPlaylist) || canNextMission) padOrder.push_back(kPadNext);
  if (hostRole) padOrder.push_back(kPadLobby);
  int padAt = 1;
  for (size_t i = 0; i < padOrder.size(); ++i) if (padOrder[i] == s_padButton) padAt = static_cast<int>(i);
  if (UiPad::Left()) padAt = std::min(padAt + 1, static_cast<int>(padOrder.size()) - 1);
  if (UiPad::Right()) padAt = std::max(padAt - 1, 0);
  s_padButton = padOrder[padAt];
  const bool padA = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false);
  // A is jump: a player still pressing it as the panel finishes must not leave the screen, so it arms a second later.
  static double s_padArmedAt = 0.0;
  if (t < tEnd) s_padArmedAt = 0.0;
  else if (s_padArmedAt == 0.0) s_padArmedAt = ImGui::GetTime() + 1.0;
  const bool padPress = padA && t >= tEnd && ImGui::GetTime() >= s_padArmedAt;
  if (padA && t < tEnd) s_reveal.skipped = true;
  const auto padOutline = [&](int button) {
    if (s_padButton == button && (ImGui::GetIO().BackendFlags & ImGuiBackendFlags_HasGamepad))
      ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(143, 216, 255, 255), 0.0f, 0, 2.0f * s);
  };
  ImGui::SetCursorScreenPos(ImVec2(bx, by));
  if (NobleStyle::DangerButton("QUIT", ImVec2(bw, bh)) || (padPress && s_padButton == kPadQuit)) {
    NOBLE_INFO("PauseMenu: quit requested from post-game panel");
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
  }
  padOutline(kPadQuit);
  overButton |= ImGui::IsItemHovered();
  bx -= bw + gap;
  ImGui::SetCursorScreenPos(ImVec2(bx, by));
  if (ImGui::Button("RETURN TO MENU", ImVec2(bw, bh)) || (padPress && s_padButton == kPadMenu))
    RelaunchAndClose(hwnd, L"", "return to menu");
  padOutline(kPadMenu);
  overButton |= ImGui::IsItemHovered();
  if (canPlayAgain) {
    bx -= bw + gap;
    ImGui::SetCursorScreenPos(ImVec2(bx, by));
    if (NobleStyle::PrimaryButton("PLAY AGAIN", ImVec2(bw, bh)) || (padPress && s_padButton == kPadAgain))
      RelaunchAndClose(hwnd, again, "play again");
    padOutline(kPadAgain);
    overButton |= ImGui::IsItemHovered();
  }
  const auto nextMissionButton = [&] {
    bx -= bw + gap;
    ImGui::SetCursorScreenPos(ImVec2(bx, by));
    if (NobleStyle::PrimaryButton("NEXT MISSION", ImVec2(bw, bh)) || (padPress && s_padButton == kPadNext))
      RelaunchAndClose(hwnd, NextMissionArgs(nextMission, hostRole ? LobbyReturnArgs(true) : std::wstring()), "next mission");
    padOutline(kPadNext);
    overButton |= ImGui::IsItemHovered();
  };
  if (canNextMission && !hostRole) nextMissionButton();
  // The host takes everyone back to the lobby: its new process registers the same session again, and guests come back when
  // the server says the match is over. Next in rotation waits for rotation itself.
  if (hostRole) {
    if (canNextMission && !hasPlaylist) {
      nextMissionButton();
    } else {
    bx -= bw + gap;
    ImGui::SetCursorScreenPos(ImVec2(bx, by));
    if (hasPlaylist) {
      if (ImGui::Button("NEXT IN ROTATION", ImVec2(bw, bh)) || (padPress && s_padButton == kPadNext))
        RelaunchAndClose(hwnd, LobbyReturnArgs(true, true), "next in rotation");
      padOutline(kPadNext);
      overButton |= ImGui::IsItemHovered();
    } else {
      UnavailableButton("NEXT IN ROTATION", ImVec2(bw, bh), "Pick a playlist on Custom Game to use a rotation.");
      overButton |= ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    }
    }
    bx -= bw + gap;
    ImGui::SetCursorScreenPos(ImVec2(bx, by));
    if (NobleStyle::PrimaryButton("RETURN TO LOBBY", ImVec2(bw, bh)) || (padPress && s_padButton == kPadLobby))
      RelaunchAndClose(hwnd, LobbyReturnArgs(true), "return to lobby");
    padOutline(kPadLobby);
    overButton |= ImGui::IsItemHovered();
  } else if (guestRole) {
    bx -= bw + gap;
    ImGui::SetCursorScreenPos(ImVec2(bx, by));
    const int lobbyState = PollHostBackInLobby();
    if (lobbyState == 1 && (t >= 2.0f || s_reveal.skipped)) {
      static bool s_leaving = false;
      if (!s_leaving) {
        s_leaving = true;
        RelaunchAndClose(hwnd, LobbyReturnArgs(false), "guest back to lobby");
      }
    }
    const char* guestBtnText = lobbyState == 1 ? "RETURNING TO LOBBY..." : (lobbyState == 2 ? "HOST LEFT" : "WAITING FOR HOST");
    const char* guestBtnTip = lobbyState == 1 ? "Host is back in lobby; returning now..."
                                              : (lobbyState == 2 ? "The host's lobby closed. Return to the menu to play again."
                                                                 : "You'll go back to the lobby as soon as the host does.");
    UnavailableButton(guestBtnText, ImVec2(bw, bh), guestBtnTip);
    overButton |= ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
  }
  if (hostRole || guestRole) {
    std::string current, display;
    {
      std::lock_guard<std::mutex> lock(s_mapMutex);
      current = s_loadingMap;
      display = s_displayMapName;
    }
    const std::string shownMap = !display.empty() ? Upper(display)
                                : current.empty() ? std::string("-")
                                                  : Upper(MapNames::Display(current));
    char nextName[64] = {};
    if (canNextMission)
      WideCharToMultiByte(CP_UTF8, 0, nextMission.c_str(), -1, nextName, sizeof(nextName) - 1, nullptr, nullptr);
    const std::string strip = "THIS MAP: " + shownMap + "    NEXT: " +
                              (canNextMission ? Upper(MapNames::Display(nextName)) : std::string("NOT SET"));
    Text(dl, smallFont, 15.0f * s, ImVec2(bx, by - 26.0f * s), kTextDim, strip.c_str());
  }
  // Auto-return / auto-advance countdown for host
  if (hostRole && t >= tEnd) {
    static const int s_autoSeconds = _wtoi(ArgValue(GetCommandLineW(), L"-lobby-playlist-auto").c_str());
    static bool s_autoCancelled = false;
    static double s_autoAt = 0.0;
    static bool s_autoInited = false;
    if (!s_autoInited) {
      s_autoInited = true;
      if (s_autoSeconds > 0) {
        s_autoAt = ImGui::GetTime() + s_autoSeconds;
      } else {
        const std::wstring cmd = GetCommandLineW();
        const std::wstring gameArgs = Relaunch::CurrentGameArgs();
        const bool isParty = !ArgValue(cmd, L"-session-id").empty() || !ArgValue(gameArgs, L"-session-id").empty() || gameArgs.find(L"-wait-peers") != std::wstring::npos;
        if (isParty) {
          s_autoAt = ImGui::GetTime() + 25.0; // 25s auto-pull party back to lobby
        }
      }
    }
    if (s_autoAt > 0.0 && !s_autoCancelled && (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))) {
      s_autoAt = 0.0;
      s_autoCancelled = true;
      NOBLE_INFO("PauseMenu: lobby return countdown cancelled by host");
    }
    if (s_autoAt > 0.0 && !s_autoCancelled) {
      const int left = std::max(0, static_cast<int>(s_autoAt - ImGui::GetTime() + 0.999));
      char text[64];
      if (s_autoSeconds > 0) {
        std::snprintf(text, sizeof(text), "Next match in %d s. B or Esc cancels.", left);
      } else {
        std::snprintf(text, sizeof(text), "Returning party to lobby in %d s. B or Esc stays.", left);
      }
      Text(dl, smallFont, 16.0f * s, ImVec2(margin, by + bh * 0.5f - 9.0f * s), kAmber, text);
      if (ImGui::GetTime() >= s_autoAt) {
        s_autoAt = 0.0;
        if (s_autoSeconds > 0) {
          RelaunchAndClose(hwnd, LobbyReturnArgs(true, true), "next in rotation (countdown)");
        } else if (canNextMission && !hasPlaylist) {
          RelaunchAndClose(hwnd, NextMissionArgs(nextMission, LobbyReturnArgs(true)), "next mission (countdown)");
        } else {
          RelaunchAndClose(hwnd, LobbyReturnArgs(true), "return to lobby (countdown)");
        }
      }
    }
  }
  if (t < tEnd) {
    Text(dl, smallFont, 16.0f * s, ImVec2(margin, by + bh * 0.5f - 9.0f * s),
         kTextDim, "Enter, Space or click: skip");
    if (ImGui::IsMouseClicked(0) && !overButton)
      s_reveal.skipped = true;
  }
  ImGui::End();
}

void PauseMenu::DrawLoading() {
  ImGuiIO &io = ImGui::GetIO();
  ImDrawList *dl = ImGui::GetBackgroundDrawList();
  const float w = io.DisplaySize.x;
  const float h = io.DisplaySize.y;

  // Reach-style loading screen: a full-screen scene (the player's own art,
  // Noble's art or the mission's scene from the MCC install), map name
  // bottom-left, a moving bar (the engine's progress callback doesn't report
  // real progress, so the bar only shows that it's working).
  std::string mapName, scenario;
  std::filesystem::path modImage, modThumb;
  {
    std::lock_guard<std::mutex> lock(s_mapMutex);
    mapName = s_loadingMapName;
    scenario = s_loadingMap;
    modImage = s_loadingImage;
    modThumb = s_loadingThumb;
  }
  dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(w, h),
                              IM_COL32(10, 16, 24, 255),
                              IM_COL32(10, 16, 24, 255), IM_COL32(2, 4, 8, 255),
                              IM_COL32(2, 4, 8, 255));
  const UiTexture scene = modImage.empty() ? UiArt::LoadingBackground(scenario) : UiArt::ImageFile(modImage);
  if (scene) {
    UiArt::DrawCover(dl, scene, ImVec2(0, 0), ImVec2(w, h));
    dl->AddRectFilledMultiColor(ImVec2(0, h * 0.55f), ImVec2(w, h),
                                IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
                                IM_COL32(0, 0, 0, 215), IM_COL32(0, 0, 0, 215));
  }
  // Multiplayer and Firefight have no per-map scene: show the map's preview as
  // well.
  const bool campaign = scenario.size() >= 3 && scenario[0] == 'm' &&
                        scenario[1] >= '0' && scenario[1] <= '9';
  if (!campaign) {
    const UiTexture preview = modThumb.empty() ? UiArt::MapPreview(scenario, true) : UiArt::ImageFile(modThumb);
    if (preview) {
      const float pw = w * 0.30f, ph = pw * 292.0f / 668.0f;
      const ImVec2 p0(w * 0.93f - pw, h * 0.80f - ph), p1(w * 0.93f, h * 0.80f);
      dl->AddRectFilled(ImVec2(p0.x - 4, p0.y - 4), ImVec2(p1.x + 4, p1.y + 4),
                        IM_COL32(0, 0, 0, 160));
      UiArt::DrawCover(dl, preview, p0, p1);
      dl->AddRect(p0, p1, IM_COL32(170, 200, 220, 140));
    }
  }

  const ImVec4 accent = NobleStyle::Accent();
  const ImU32 accentCol = ImGui::ColorConvertFloat4ToU32(accent);
  const ImU32 mutedCol =
      ImGui::ColorConvertFloat4ToU32(NobleStyle::TextMuted());

  const float left = w * 0.07f;
  const float base = h * 0.80f;

  ImFont *title = NobleStyle::TitleFont();
  ImFont *font = ImGui::GetFont();
  const float titleSize =
      title ? title->FontSize * 1.4f : ImGui::GetFontSize() * 2.0f;
  dl->AddText(font, ImGui::GetFontSize(),
              ImVec2(left, base - titleSize - 26.0f), mutedCol, "LOADING");
  if (!mapName.empty()) {
    dl->AddText(title ? title : font, titleSize,
                ImVec2(left, base - titleSize - 4.0f),
                IM_COL32(235, 240, 245, 255), mapName.c_str());
  }

  const float barW = w * 0.30f;
  const float barH = 4.0f;
  const ImVec2 b0(left, base + 8.0f);
  const ImVec2 b1(left + barW, base + 8.0f + barH);
  dl->AddRectFilled(b0, b1, IM_COL32(255, 255, 255, 30));
  const float t = static_cast<float>(ImGui::GetTime());
  const float seg = barW * 0.25f;
  const float pos = std::fmod(t * 0.6f, 1.0f) * (barW + seg) - seg;
  const float x0 = b0.x + (pos < 0.0f ? 0.0f : pos);
  const float x1 = b0.x + ((pos + seg) > barW ? barW : (pos + seg));
  if (x1 > x0)
    dl->AddRectFilled(ImVec2(x0, b0.y), ImVec2(x1, b1.y), accentCol);
}

void PauseMenu::Render(IDXGISwapChain *swapChain, bool loading, const std::function<void()> &extraOverlay) {
  if (swapChain)
    s_swapChain.store(swapChain);
  s_lastEngineFrame.store(GetTickCount64());
  std::lock_guard<std::mutex> lock(s_renderMutex);
  RenderImpl(swapChain, loading, false, extraOverlay);
  if (s_scoreboardShotRequest.exchange(false) && swapChain) FrameCapture::SaveNow(swapChain, 888888);
}

bool PauseMenu::HoldFrame() {
  if (!s_gameOver.load())
    return false;
  const ULONGLONG now = GetTickCount64();
  const ULONGLONG gameOverAt = s_gameOverAt.load();
  // Give the engine a moment: it keeps drawing for several seconds after the
  // results.
  if (now - s_lastEngineFrame.load() < 1000 || now - gameOverAt < 1000)
    return false;
  IDXGISwapChain *swapChain = s_swapChain.load();
  if (!swapChain)
    return false;
  static bool s_logged = false;
  if (!s_logged) {
    s_logged = true;
    NOBLE_INFO("PauseMenu: engine stopped drawing after the match; Noble draws "
               "the post-game panel itself");
  }
  static unsigned long long s_holdFrames = 0;
  ++s_holdFrames;
  {
    std::lock_guard<std::mutex> lock(s_renderMutex);
    RenderImpl(swapChain, false, true, {});
    // Test runs: one screenshot of the held panel, and -quit-after keeps
    // working (it is normally checked from slot 1, which no longer runs).
    static const bool s_testCapture =
        wcsstr(GetCommandLineW(), L"-capture-frames") != nullptr;
    if (s_testCapture && s_holdFrames == 60)
      FrameCapture::SaveNow(swapChain, 900000);
    FrameCapture::OnFrame(nullptr, 0);
  }
  swapChain->Present(1, 0);
  return true;
}

void PauseMenu::RenderImpl(IDXGISwapChain *swapChain, bool loading,
                           bool clear, const std::function<void()> &extraOverlay) {
  // F5 edge-toggles the third-person camera (physical key; this runs every engine frame).
  // Not while F5 is standing in for a controller paddle.
  const int tpVk = KeyBindings::EffectiveKey(KeyBindings::kActionThirdPerson);
  const bool tpBlocked = s_open.load() || loading || s_gameOver.load() || ForgeMenu::WantsPointer() || SocialOverlay::IsTyping();
  CameraThirdPerson::PollHotkey(!tpBlocked && tpVk > 0 && tpVk < 256 && !ReachHost::IsPaddleKey(tpVk) && (GetAsyncKeyState(tpVk) & 0x8000) != 0);
  const bool gameOver = s_gameOver.load();
  // -postgame-test: force the clean GAME OVER panel so its composition over the engine's post-game UI can be captured
  // without playing a match to its score limit.
  if (!gameOver && (wcsstr(GetCommandLineW(), L"-postgame-test") || wcsstr(GetCommandLineW(), L"-postgame-role"))) {
    static int s_postGameTestFrames = 0;
    if (!loading && ++s_postGameTestFrames == 900) {
      NOBLE_INFO("PauseMenu: -postgame-test forcing the game-over panel");
      ShowGameOver();
    }
  }

  const bool open = s_open.load();
  SocialOverlay::Poll(!open && !loading && !ForgeMenu::IsOpen());
  const bool showFps = Settings::Get().Snapshot().showFps;
  static const bool s_scoreboardTest = wcsstr(GetCommandLineW(), L"-scoreboard-test") != nullptr;
  const int boardVk = KeyBindings::EffectiveKey(20);
  const int demo = ScoreboardDemo();
  static int s_demoFrames = 0;
  if (demo >= 2 && !loading && !open && !gameOver && s_demoFrames == 0)
    EnterScoreboardPointer();
  // A fresh press of the scoreboard key hands the pointer back to the game.
  const bool boardPhysical = boardVk > 0 && (GetAsyncKeyState(boardVk) & 0x8000) != 0;
  static bool s_boardWasDown = false, s_boardWasPointer = false;
  const bool wasPointer = s_boardPointer.load();
  if (wasPointer && s_boardWasPointer && boardPhysical && !s_boardWasDown && demo == 0)
    s_boardPointer.store(false);
  s_boardWasDown = boardPhysical;
  s_boardWasPointer = wasPointer;
  if (gameOver || open || loading) {
    s_boardPointer.store(false);
    s_boardReport.store(false);
  }
  const bool boardPointer = s_boardPointer.load();
  const bool scoreboardHeld = !loading && !open && !gameOver &&
                              (ReachHost::IsKeyHeld(boardVk) || s_scoreboardTest || demo > 0 || boardPointer);
  // Guests: if the host has been silent for a while, the engine keeps pumping frames but the session is dead. Show a panel
  // instead of freezing in the mission.
  static bool s_connectionLost = false;
  const bool guest = NetworkTransport::Get().IsActive() && !NetworkTransport::Get().IsHost();
  const double hostSilence = guest ? NetworkTransport::Get().SecondsSinceRemotePacket() : 0.0;
  if (!loading && !open && !gameOver && guest && hostSilence > 8.0) {
    if (!s_connectionLost) {
      s_connectionLost = true;
      NOBLE_WARN("PauseMenu: host connection lost ({:.1f}s without packets) - holding the match for host migration",
                 hostSilence);
    }
  } else if (guest && hostSilence < 2.0) {
    // Packets resumed, e.g. after the engine migrated the host. Drop the panel instead of latching it forever.
    s_connectionLost = false;
  }
  const bool connectionLost = s_connectionLost;
  if (!(open || gameOver || loading || showFps || scoreboardHeld || connectionLost || extraOverlay || SocialOverlay::WantsFrame()) || !swapChain ||      !ImGui::GetCurrentContext())
    return;

  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(swapChain->GetDesc(&desc)) || !desc.OutputWindow)
    return;
  const HWND hwnd = desc.OutputWindow;

  ID3D11Device *device = nullptr;
  if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device),
                                  reinterpret_cast<void **>(&device))) ||
      !device)
    return;
  ID3D11DeviceContext *context = nullptr;
  device->GetImmediateContext(&context);

  static IDXGISwapChain *s_cachedSwapChain = nullptr;
  static ID3D11Texture2D *s_cachedBackBuffer = nullptr;
  static ID3D11RenderTargetView *s_cachedRtv = nullptr;

  ID3D11Texture2D *backBuffer = nullptr;
  if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                  reinterpret_cast<void **>(&backBuffer))) ||
      !backBuffer) {
    if (context)
      context->Release();
    device->Release();
    return;
  }

  if (s_cachedSwapChain != swapChain || s_cachedBackBuffer != backBuffer ||
      !s_cachedRtv) {
    if (s_cachedRtv) {
      s_cachedRtv->Release();
      s_cachedRtv = nullptr;
    }
    if (s_cachedBackBuffer) {
      s_cachedBackBuffer->Release();
      s_cachedBackBuffer = nullptr;
    }
    s_cachedSwapChain = swapChain;
    s_cachedBackBuffer = backBuffer;
    s_cachedBackBuffer->AddRef();
    device->CreateRenderTargetView(backBuffer, nullptr, &s_cachedRtv);
  }
  ID3D11RenderTargetView *rtv = s_cachedRtv;

  if (context && rtv) {
    D3D11_TEXTURE2D_DESC bbDesc{};
    backBuffer->GetDesc(&bbDesc);

    // Feed ImGui ourselves on this thread (the Win32 backend's message handler
    // runs on the window thread and its event queue is not thread safe).
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NoMouseCursorChange; // the window proc shows the arrow
    io.DisplaySize = ImVec2(static_cast<float>(bbDesc.Width),
                            static_cast<float>(bbDesc.Height));

    static LARGE_INTEGER s_freq{}, s_last{};
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    if (s_freq.QuadPart == 0)
      QueryPerformanceFrequency(&s_freq);
    float dt = s_last.QuadPart
                   ? static_cast<float>(now.QuadPart - s_last.QuadPart) /
                         static_cast<float>(s_freq.QuadPart)
                   : 1.0f / 60.0f;
    if (dt <= 0.0f || dt > 0.25f)
      dt = 1.0f / 60.0f;
    s_last = now;
    io.DeltaTime = dt;

    RECT client{};
    GetClientRect(hwnd, &client);
    POINT cursor{};
    static bool s_boardWasPointerFrame = false;
    if ((open || gameOver || boardPointer || extraOverlay) && GetForegroundWindow() == hwnd &&
        GetCursorPos(&cursor) && ScreenToClient(hwnd, &cursor) &&
        client.right > 0 && client.bottom > 0) {
      // Window may be resized while the back buffer stays the same size: scale
      // to back-buffer pixels.
      const float sx = io.DisplaySize.x / static_cast<float>(client.right);
      const float sy = io.DisplaySize.y / static_cast<float>(client.bottom);
      io.AddMousePosEvent(static_cast<float>(cursor.x) * sx,
                          static_cast<float>(cursor.y) * sy);
      // A click shorter than a frame is seen only as a WM_LBUTTONDOWN; give
      // ImGui a full press + release for it (ImGui spreads queued events over
      // frames).
      bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
      // The click that freed the scoreboard pointer is not a click on the panel: wait for its release.
      static bool s_boardClickHeld = false;
      if (boardPointer && !s_boardWasPointerFrame && down)
        s_boardClickHeld = true;
      s_boardWasPointerFrame = boardPointer;
      if (s_boardClickHeld) {
        if (down) {
          down = false;
          Window::TakeLeftClicks();
        } else {
          s_boardClickHeld = false;
          Window::TakeLeftClicks();
        }
      }
      if (Window::TakeLeftClicks() > 0 && !down) {
        io.AddMouseButtonEvent(0, true);
        io.AddMouseButtonEvent(0, false);
      } else {
        io.AddMouseButtonEvent(0, down);
      }
    } else {
      s_boardWasPointerFrame = false;
      io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
      io.AddMouseButtonEvent(0, false);
    }

    UiPad::Feed(io);
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();
    if (SocialOverlay::IsTyping()) io.ClearInputKeys();   // the line takes the keyboard, not the panels below it
    if (showFps) {
      char fpsLabel[32];
      std::snprintf(fpsLabel, sizeof(fpsLabel), "FPS %.0f", io.Framerate);
      ImDrawList *foreground = ImGui::GetForegroundDrawList();
      const ImVec2 textSize = ImGui::CalcTextSize(fpsLabel);
      const ImVec2 pos(io.DisplaySize.x - textSize.x - 24.0f, 16.0f);
      foreground->AddRectFilled(
          ImVec2(pos.x - 8.0f, pos.y - 5.0f),
          ImVec2(pos.x + textSize.x + 8.0f, pos.y + textSize.y + 5.0f),
          IM_COL32(10, 16, 24, 190), 5.0f);
      foreground->AddText(pos, IM_COL32(143, 216, 255, 255), fpsLabel);
    }
    if (gameOver) {
      DrawGameOver(hwnd);
    } else {
      if (loading)
        DrawLoading();
      if (open)
        DrawMenu(hwnd);
      if (connectionLost && DrawConnectionLostPanel()) return;
      if (scoreboardHeld && !connectionLost) {
        DrawScoreboardPanel();
        // -scoreboard-demo: one screenshot about 30 s in, so the match is on screen behind the panel, not a loading frame.
        if (demo > 0 && ++s_demoFrames == 900) {
          s_scoreboardShotRequest.store(true);
          NOBLE_INFO("PauseMenu: -scoreboard-demo {} capture requested", demo);
        }
        if (s_scoreboardTest) {
          static bool s_shot = false;
          if (!s_shot) {
            s_shot = true;
            s_scoreboardShotRequest.store(true);
            NOBLE_INFO("PauseMenu: -scoreboard-test capture requested");
          }
        }
      }
    }
    if (extraOverlay) extraOverlay();
    SocialOverlay::Draw(24.0f, 24.0f, 778.0f);   // chat stays above the motion tracker
    ImGui::Render();

    // The DX11 backend saves/restores most pipeline state but not the
    // output-merger targets.
    ID3D11RenderTargetView *oldRtv = nullptr;
    ID3D11DepthStencilView *oldDsv = nullptr;
    context->OMGetRenderTargets(1, &oldRtv, &oldDsv);
    context->OMSetRenderTargets(1, &rtv, nullptr);
    if (clear) {
      const float dark[4] = {0.016f, 0.031f, 0.055f, 1.0f};
      context->ClearRenderTargetView(rtv, dark);
    }
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    context->OMSetRenderTargets(1, &oldRtv, oldDsv);
    if (oldRtv)
      oldRtv->Release();
    if (oldDsv)
      oldDsv->Release();
  }

  if (backBuffer)
    backBuffer->Release();
  if (context)
    context->Release();
  device->Release();
}

} // namespace noble
