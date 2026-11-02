#include <map>
#include "FrontEndMenu.hpp"
#include "audio/VoiceChat.hpp"
#include "SocialOverlay.hpp"
#include "NobleUi.hpp"
#include "network/UPnP.hpp"
#include "network/AuthClient.hpp"
#include "network/MasterServerClient.hpp"
#include "network/NetworkTransport.hpp"
#include "network/SessionManager.hpp"
#include "network/SocialClient.hpp"
#include <format>
#include <nlohmann/json.hpp>
#include "UIManager.hpp"
#include "NobleStyle.hpp"
#include "common/MapNames.hpp"
#include "SettingsPanel.hpp"
#include "UiPad.hpp"
#include "CareerPanel.hpp"
#include "MenuSounds.hpp"
#include "InputPrompts.hpp"
#include "audio/ReachAudio.hpp"
#include "audio/ProcessAudio.hpp"
#include "UiArt.hpp"
#include "ServerBrowser.hpp"
#include "ArmorCustomizer.hpp"
#include "WorkshopManager.hpp"
#include "ModCatalog.hpp"
#include "ModDownloadPrompt.hpp"
#include "vfs/VFS.hpp"
#include "career/Career.hpp"
#include "common/Logger.hpp"
#include "common/Relaunch.hpp"
#include "common/Settings.hpp"
#include "common/Version.hpp"
#include "common/Updater.hpp"
#include "common/DeviceIdentity.hpp"
#include "engine/FrameCapture.hpp"
#include "engine/ReachArmory.hpp"
#include "engine/MapGameTypes.hpp"
#include "engine/VariantRules.hpp"
#include "engine/Playlists.hpp"
#include <random>
#include "host/Window.hpp"
#include "host/D3D11Renderer.hpp"

#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <cwctype>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <utility>

namespace noble {

namespace {

// ---------------------------------------------------------------------------------------------
// Content
// ---------------------------------------------------------------------------------------------
struct MenuMap {
    const char* scenario;   // passed to BlamEngine::LaunchMap
    const char* name;       // shown in the list
    const char* blurb;      // one line under the preview
};

// Same maps as BlamEngine's multiplayer table (launch indices 190-209).
const MenuMap kMaps[] = {
    { "50_panopticon",     "Boardwalk",     "New Alexandria waterfront." },
    { "70_boneyard",       "Boneyard",      "A ship-breaking yard on the coast." },
    { "45_launch_station", "Countdown",     "Inside a Sabre launch facility." },
    { "30_settlement",     "Powerhouse",    "A hydroelectric plant in the hills." },
    { "52_ivory_tower",    "Reflection",    "A penthouse high above the city." },
    { "35_island",         "Spire",         "A Covenant spire on a rocky island." },
    { "20_sword_slayer",   "Sword Base",    "The atrium of ONI's Sword Base." },
    { "45_aftship",        "Zealot",        "Aboard a Covenant corvette." },
    { "dlc_slayer",        "Anchor 9",      "A space station in orbit over Reach." },
    { "dlc_invasion",      "Breakpoint",    "An ONI research site under attack." },
    { "dlc_medium",        "Tempest",       "A Forerunner coastline." },
    { "condemned",         "Condemned",     "An orbital station breaking apart." },
    { "trainingpreserve",  "Highlands",     "A training preserve in the hills." },
    { "cex_beaver_creek",  "Battle Canyon", "A remake of a canyon classic." },
    { "cex_damnation",     "Penance",       "A remake of a Covenant classic." },
    { "cex_timberland",    "Ridgeline",     "A remake of a forest classic." },
    { "cex_prisoner",      "Solitary",      "A remake of a prison classic." },
    { "cex_hangemhigh",    "High Noon",     "A remake of a tomb classic." },
    { "cex_headlong",      "Breakneck",     "A remake of a city classic." },
    { "forge_halo",        "Forge World",   "The largest canvas Reach has." },
};
constexpr int kDefaultMap = 6; // Sword Base: the map tested most

// Campaign missions (launch index 178-189).
const MenuMap kMissions[] = {
    { "m05",       "Noble Actual",          "Arrive on Reach and meet Noble Team. (Cinematic)" },
    { "m10",       "Winter Contingency",    "A relay station on the frontier has gone silent." },
    { "m20",       "ONI: Sword Base",       "The Covenant are attacking a vital ONI base." },
    { "m30",       "Nightfall",             "Scout behind enemy lines under cover of dark." },
    { "m35",       "Tip of the Spear",      "Two armies collide. Take out the spire." },
    { "m45",       "Long Night of Solace",  "Take the fight into orbit." },
    { "m50",       "Exodus",                "Get civilians out of an occupied city." },
    { "m52",       "New Alexandria",        "Air support over a city under siege." },
    { "m60",       "The Package",           "Back to Sword Base for one last job." },
    { "m70",       "The Pillar of Autumn",  "Deliver the package to the ship." },
    { "m70_bonus", "Lone Wolf",             "Hold out for as long as you can." },
};
const char* const kDifficulties[] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };
const float kDifficultyMultiplier[] = { 0.75f, 1.0f, 1.25f, 1.5f };
static bool s_menuMusicMuted = false;

// Firefight maps (launch index 210-219, as BlamEngine::ResolveMapId maps them).
const MenuMap kFirefightMaps[] = {
    { "ff50_park",       "Beachhead",       "" },
    { "ff45_corvette",   "Corvette",        "" },
    { "ff20_courtyard",  "Courtyard",       "" },
    { "ff60_icecave",    "Glacier",         "" },
    { "ff70_holdout",    "Holdout",         "" },
    { "ff60_airview",    "Outpost",         "" },
    { "ff10_prototype",  "Overlook",        "" },
    { "ff30_waterfront", "Waterfront",      "" },
    { "ff_unearthed",    "Unearthed",       "" },
    { "cex_ff_halo",     "Installation 04", "" },
};

// Reach's campaign skulls. `bit` is MCC's skull number (launch block +0x48), `icon` the name in
// MCC's skulls texture pack, `multiplier` the campaign-scoring bonus.
struct Skull {
    const char* name;
    int bit;
    const char* icon;
    const char* description;
    float multiplier;
    bool primary;
};
const Skull kSkulls[] = {
    { "Iron",         18, "IRON",         "Co-op: dying sends everyone back to the last checkpoint. Solo: death restarts the mission.", 1.2f, true },
    { "Black Eye",     3, "BLACKEYE",     "Your shields only recharge when you melee enemies.",                                          1.1f, true },
    { "Tough Luck",   36, "TOUGHLUCK",    "Enemies dodge grenades and dive out of danger far more often.",                               1.1f, true },
    { "Catch",         7, "CATCH",        "Enemies throw many more grenades.",                                                           1.1f, true },
    { "Cloud",        13, "CLOUD",        "Your motion tracker is turned off.",                                                         1.1f, true },
    { "Famine",       11, "FAMINE",       "Weapons dropped by enemies carry half the usual ammunition.",                                 1.2f, true },
    { "Thunderstorm", 34, "THUNDERSTORM", "Enemies are promoted to their higher ranks.",                                                 1.2f, true },
    { "Tilt",         35, "TILT",         "Enemy strengths and weaknesses are exaggerated.",                                             1.2f, true },
    { "Mythic",       23, "MYTHIC",       "Enemies have much more health.",                                                              1.2f, true },
    { "Blind",         4, "BLIND",        "Your HUD and first-person weapon are hidden.",                                                1.0f, false },
    { "Cowbell",       8, "COWBELL",      "Explosions throw things much farther.",                                                       1.0f, false },
    { "Grunt Birthday Party", 16, "GRUNTBIRTHDAYPARTY", "Headshots on Grunts end in a burst of confetti.",                              1.0f, false },
    { "IWHBYD",       19, "IWHBYD",       "Rare combat dialogue becomes much more common.",                                              1.0f, false },
};
constexpr int kSkullCount = static_cast<int>(sizeof(kSkulls) / sizeof(kSkulls[0]));
const char* const kDifficultyNames[] = { "Easy", "Normal", "Heroic", "Legendary" };

template <typename T, size_t N>
constexpr int Count(const T (&)[N]) { return static_cast<int>(N); }

// ---------------------------------------------------------------------------------------------
// Game variants (MCC's game_variants folder)
// ---------------------------------------------------------------------------------------------
struct MenuVariant {
    std::string label;
    std::filesystem::path path;
    std::string description;   // the file's own text; empty when it only holds a key for a string table Noble can't read
    bool custom = false;       // a player-made game mode (a .json preset), listed under its own heading
    std::string type;          // MapGameTypes key from the file name; empty when it can't be told
};

struct VariantName {
    const char* stem;   // lower-case file stem
    const char* label;
};
const VariantName kVariantNames[] = {
    { "slayer_054", "Slayer" }, { "slayer_pro_054", "Slayer Pro" }, { "slayer_classic_054", "Classic Slayer" },
    { "slayer_covy_054", "Elite Slayer" }, { "swat_054", "SWAT" }, { "ctf_054", "Capture the Flag" },
    { "ctf_1flag_054", "One Flag CTF" }, { "ctf_multiteam_054", "Multi Flag CTF" }, { "ctf_neutralflag_054", "Neutral Flag" },
    { "assault_054", "Assault" }, { "assault_one_bomb_054", "One Bomb" }, { "assault_neutral_bomb_054", "Neutral Bomb" },
    { "headhunter_054", "Headhunter" }, { "headhunter_pro_054", "Headhunter Pro" }, { "infection_054", "Infection" },
    { "infection_safehavens_054", "Safe Havens" }, { "juggernaut_054", "Juggernaut" }, { "koth_054", "King of the Hill" },
    { "koth_crazyking_054", "Crazy King" }, { "oddball_054", "Oddball" }, { "race_054", "Race" }, { "rally_054", "Rally" },
    { "skeeball_default_054", "Halo Ball" }, { "stockpile_054", "Stockpile" }, { "territories_054", "Territories" },
    { "territories-3plot_054", "3 Plots" }, { "territories-landgrab_054", "Land Grab" }, { "3nvasion_054", "Invasion" },
    { "3nvasion_boneyard_054", "Invasion: Boneyard" }, { "3nvasion_spire_054", "Invasion: Spire" },
    { "3nvasion_dlc_054", "Invasion: Breakpoint" }, { "2nvasion_slayer_054", "Invasion Slayer" },
    { "hr_4v4_team_3plots_dmr_ar_500points_tu", "Team 3 Plots (MCC)" },
    { "hr_4v4_team_assault_dmr_ar_3points_tu", "Team Assault (MCC)" },
    { "hr_4v4_team_crazyking_dmr_ar_150points_tu", "Team Crazy King (MCC)" },
    { "hr_4v4_team_headhunter_dmr_ar_50points_tu", "Team Headhunter (MCC)" },
    { "hr_4v4_team_multiflag_dmr_ar_3points_tu", "Team Multi Flag (MCC)" },
    { "hr_4v4_team_oddball_dmr_ar_150points_tu", "Team Oddball (MCC)" },
    { "hr_4v4_team_onebomb_dmr_ar_3points_tu", "Team One Bomb (MCC)" },
    { "hr_4v4_team_oneflag_dmr_ar_4rounds_tu", "Team One Flag (MCC)" },
    { "hr_4v4_team_slayer_dmr_ar_50points_tu", "Team Slayer (MCC)" },
    { "hr_4v4_team_stockpile_dmr_ar_10points_tu", "Team Stockpile (MCC)" },
    { "hr_4v4_team_territories_dmr_ar_4rounds_3min_tu", "Team Territories (MCC)" },
    { "hr_ffa_escalationslayer_mixed_tu", "Escalation Slayer (MCC)" },
    { "hr_ffa_escalationslayer_mixed_vanilla", "Escalation Slayer (original)" },
    { "hr_ffa_juggernaut_ar_mag_150points_tu", "Juggernaut (MCC)" },
    { "hr_ff_classic_1set", "Classic Firefight" }, { "hr_ff_generatordefense_1set", "Generator Defense" },
    { "hr_ff_gruntpocalypse_1round", "Gruntpocalypse" }, { "hr_ff_rocketfight_1set", "Rocketfight" },
    { "hr_ff_scoreattack_1round", "Score Attack" }, { "hr_ff_standard_1set", "Firefight" },
    { "hr_ff_versus_2turns", "Firefight Versus" },
};
constexpr const wchar_t* kForgeVariant = L"00_basic_editing_054.bin";

std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

// A shipped variant starts with a _blf header holding its name (UTF-16, offset 0xC0) and description (0x1C0), checked
// against all 54 files in MCC's game_variants. Only these two strings are read: the rules need the field map.
struct VariantHeaderText {
    std::string name;
    std::string description;
};
VariantHeaderText ReadVariantHeaderText(const std::filesystem::path& path) {
    constexpr size_t kNameAt = 0xC0, kNameChars = 64, kDescriptionAt = 0x1C0, kDescriptionChars = 256;
    VariantHeaderText out;
    std::ifstream file(path, std::ios::binary);
    std::vector<char> data(kDescriptionAt + kDescriptionChars * 2);
    file.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (static_cast<size_t>(file.gcount()) < data.size() || std::memcmp(data.data(), "_blf", 4) != 0) return out;
    const auto read = [&](size_t at, size_t chars) {
        std::wstring wide;
        for (size_t i = 0; i < chars; ++i) {
            const wchar_t c = static_cast<wchar_t>(static_cast<unsigned char>(data[at + i * 2]) | (static_cast<unsigned char>(data[at + i * 2 + 1]) << 8));
            if (c == 0) break;
            wide.push_back(c);
        }
        char text[1024] = {};
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), text, sizeof(text) - 1, nullptr, nullptr);
        return std::string(text);
    };
    out.name = read(kNameAt, kNameChars);
    out.description = read(kDescriptionAt, kDescriptionChars);
    if (out.description.find("hr_gvar_") != std::string::npos) out.description.clear();   // a key, not text
    return out;
}

// A known file keeps Noble's wording; any other uses the name inside the file, then a tidied file name.
std::string VariantLabel(const std::wstring& stem, const std::string& nameInFile) {
    const std::wstring lower = Lower(stem);
    for (const VariantName& v : kVariantNames) {
        std::wstring w(v.stem, v.stem + strlen(v.stem));
        if (w == lower) return v.label;
    }
    if (!nameInFile.empty()) return nameInFile;
    std::string out;
    bool up = true;
    std::wstring s = stem;
    if (s.size() > 4 && Lower(s.substr(s.size() - 4)) == L"_054") s.resize(s.size() - 4);
    for (wchar_t c : s) {
        if (c == L'_' || c == L'-') { out.push_back(' '); up = true; continue; }
        char ch = c < 0x80 ? static_cast<char>(c) : '?';
        out.push_back(up ? static_cast<char>(::toupper(static_cast<unsigned char>(ch))) : ch);
        up = false;
    }
    return out;
}

std::vector<MenuVariant> ScanVariants(const std::filesystem::path& reachDir, bool firefight) {
    std::vector<MenuVariant> list;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(reachDir / "game_variants", ec)) {
        if (!entry.is_regular_file(ec) || Lower(entry.path().extension().wstring()) != L".bin") continue;
        const std::wstring lower = Lower(entry.path().filename().wstring());
        if (lower == Lower(kForgeVariant)) continue;
        const bool isFirefightVariant = lower.rfind(L"hr_ff_", 0) == 0;
        if (isFirefightVariant != firefight) continue;
        const VariantHeaderText text = ReadVariantHeaderText(entry.path());
        list.push_back({ VariantLabel(entry.path().stem().wstring(), text.name), entry.path(), text.description, false,
                         MapGameTypes::FromVariantStem(entry.path().stem().wstring()) });
    }
    // Player-made game modes: this PC's own, and any a mod ships in a game_modes folder for its maps.
    std::vector<std::filesystem::path> modeFolders = { VariantRules::Directory() };
    for (const std::filesystem::path& mods : { VFS::GetWorkshopPath(), VFS::GetUserStoragePath() / "mods" }) {
        for (const auto& mod : std::filesystem::directory_iterator(mods, ec)) modeFolders.push_back(mod.path() / "game_modes");
        ec.clear();
    }
    for (const std::filesystem::path& folder : modeFolders) {
        for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
            VariantRules::Preset preset;
            if (!VariantRules::IsPresetFile(entry.path()) || !VariantRules::Load(entry.path(), preset)) continue;
            const bool baseIsFirefight = Lower(std::filesystem::path(preset.base).wstring()).rfind(L"hr_ff_", 0) == 0;
            if (baseIsFirefight != firefight || !std::filesystem::is_regular_file(reachDir / "game_variants" / preset.base, ec)) continue;
            MenuVariant mode;
            mode.label = preset.name.empty() ? entry.path().stem().string() : preset.name;
            mode.path = entry.path();
            mode.description = "Your own rules, starting from " + preset.base + ".";
            mode.custom = true;
            mode.type = MapGameTypes::FromVariantStem(std::filesystem::path(preset.base).stem().wstring());
            list.push_back(std::move(mode));
        }
        ec.clear();
    }
    std::sort(list.begin(), list.end(), [](const MenuVariant& a, const MenuVariant& b) { return a.label < b.label; });
    return list;
}

// The game type a variant path is, also for a player-made game mode (which takes it from its base file).
std::string VariantPathType(const std::filesystem::path& path) {
    VariantRules::Preset preset;
    if (VariantRules::IsPresetFile(path) && VariantRules::Load(path, preset)) return MapGameTypes::FromVariantStem(std::filesystem::path(preset.base).stem().wstring());
    return MapGameTypes::FromVariantStem(path.stem().wstring());
}

int FindVariant(const std::vector<MenuVariant>& list, const wchar_t* file) {
    for (size_t i = 0; i < list.size(); ++i) {
        if (Lower(list[i].path.filename().wstring()) == Lower(file)) return static_cast<int>(i);
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------------------------
struct Ui {
    ImDrawList* dl = nullptr;
    ImVec2 origin = ImVec2(0.0f, 0.0f);
    ImVec2 size;
    float s = 1.0f;
};

const ImU32 kWhite = IM_COL32(236, 240, 244, 255);
const ImU32 kDim = IM_COL32(160, 170, 180, 255);
const ImU32 kDisabled = IM_COL32(110, 118, 126, 200);
const ImU32 kAccent = IM_COL32(143, 216, 255, 255);

ImFont* Font(ImFont* f) { return f ? f : ImGui::GetFont(); }

float TextWidth(ImFont* font, float px, const char* text) {
    return Font(font)->CalcTextSizeA(px, FLT_MAX, 0.0f, text).x;
}

void Text(const Ui& ui, ImFont* font, float px, ImVec2 pos, ImU32 col, const char* text, float wrapRight = 0.0f) {
    ui.dl->AddText(Font(font), px * ui.s, pos, col, text, nullptr, wrapRight > pos.x ? wrapRight - pos.x : 0.0f);
}

// One-line label that fits |width| pixels: cuts at a word boundary and appends "...". Menu rows and the
// banner title use this instead of running under the next column / off the art.
std::string Ellipsize(ImFont* font, float px, const std::string& text, float width) {
    if (width <= 0.0f) return std::string();
    ImFont* f = Font(font);
    if (f->CalcTextSizeA(px, FLT_MAX, 0.0f, text.c_str()).x <= width) return text;
    const std::string dots = "...";
    const float dotsW = f->CalcTextSizeA(px, FLT_MAX, 0.0f, dots.c_str()).x;
    size_t lo = 0, hi = text.size();
    while (lo < hi) {
        const size_t mid = (lo + hi + 1) / 2;
        const std::string part = text.substr(0, mid);
        if (f->CalcTextSizeA(px, FLT_MAX, 0.0f, part.c_str()).x + dotsW <= width) lo = mid;
        else hi = mid - 1;
    }
    std::string cut = text.substr(0, lo);
    const size_t space = cut.find_last_of(' ');
    if (space != std::string::npos && space > cut.size() / 2) cut.resize(space);
    while (!cut.empty() && (cut.back() == ' ' || cut.back() == '\t')) cut.pop_back();
    return cut + dots;
}

// Word-aware fit for the one/two-line description under a map picture: keeps whole words, caps at
// |maxHeight| pixels and ends the last line with "...". Can't overflow into the option rows.
std::string FitWrapped(ImFont* font, float px, const std::string& text, float width, float maxHeight) {
    if (text.empty() || width <= 0.0f || maxHeight <= 0.0f) return std::string();
    ImFont* f = Font(font);
    std::string body = text;
    auto heightOf = [&](const std::string& s) { return f->CalcTextSizeA(px, FLT_MAX, width, s.c_str()).y; };
    if (heightOf(body) <= maxHeight) return body;
    for (int guard = 0; guard < 4096 && !body.empty(); ++guard) {
        const size_t space = body.find_last_of(' ');
        if (space == std::string::npos) break;
        body.resize(space);
        while (!body.empty() && (body.back() == ' ' || body.back() == '\t')) body.pop_back();
        std::string candidate = body + "...";
        if (heightOf(candidate) <= maxHeight) return candidate;
    }
    return Ellipsize(font, px, text, width);
}

std::string Upper(std::string s) {
    for (char& c : s) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
    return s;
}

// Workshop descriptions are written for a Steam page: BBCode tags, [h1]Map List[/h1] sections, hard line
// breaks and sometimes thousands of characters. A menu row shows two lines at most, so tags are dropped,
// whitespace is collapsed and the text is cut back to a whole word on a "...".
std::string PlainDescription(const std::string& in, size_t maxChars) {
    std::string out;
    out.reserve(std::min(in.size(), maxChars + 4));
    bool pendingSpace = false;
    for (size_t i = 0; i < in.size() && out.size() < maxChars; ++i) {
        const char c = in[i];
        if (c == '[') {                                   // BBCode / Steam markup tag, e.g. [h1] or [/b]
            const size_t close = in.find(']', i + 1);
            if (close != std::string::npos && close - i <= 40) {
                i = close;
                pendingSpace = !out.empty();
                continue;
            }
        }
        if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) {
            out.push_back(' ');
            pendingSpace = false;
        }
        out.push_back(c);
    }
    if (out.size() >= maxChars) {
        const size_t cut = out.find_last_of(' ');
        if (cut != std::string::npos && cut > maxChars / 2) out.resize(cut);
        out += "...";
    }
    return out;
}

// Height of the description under a map picture: one or two lines plus the gaps above and below. The
// setup screens reserve exactly this and HeroBanner draws exactly this, so a long mod blurb cannot push
// the option rows over the network strip (2026-10-30).
float BlurbBlockHeight(const Ui& ui, float width, const char* blurb) {
    if (!blurb || !*blurb) return 0.0f;
    const float lineH = 17.0f * ui.s * 1.35f;
    const float textH = Font(nullptr)->CalcTextSizeA(17.0f * ui.s, FLT_MAX, width, blurb).y;
    return std::min(textH, lineH * 2.0f) + 16.0f * ui.s;
}

enum class Screen { Main, Campaign, Skulls, Firefight, CustomGame, Forge, Servers, Settings, Armory, Mods, Career, PreGameLobby, Social };

std::string s_lobbyBackdropMap;   // set each frame from the lobby state
std::filesystem::path s_lobbyBackdropImage; // mod map art for the pre-game backdrop

// The Armory's presentation. The engine draws the model from a fixed scene camera. Noble copies the finished
// frame and places it on the right: whole-body cameras are enlarged around the character (the menu renders 1.5x,
// so this never upscales what the window shows), while per-slot close-ups keep the old fit-under-the-rule
// placement. A soft additive floor light lifts the black chamber below the model.
namespace ArmoryStage {

ID3D11RenderTargetView* s_frameRtv = nullptr;   // the back buffer holding this frame's engine image; null when there is none
ID3D11Texture2D* s_copy = nullptr;
ID3D11ShaderResourceView* s_copySrv = nullptr;
UINT s_copyW = 0, s_copyH = 0;
DXGI_FORMAT s_copyFormat = DXGI_FORMAT_UNKNOWN;
ID3D11BlendState* s_opaque = nullptr;
ID3D11BlendState* s_additive = nullptr;
ID3D11DeviceContext* s_context = nullptr;
float s_windowScale = 1.0f;   // back-buffer pixels per window pixel (menu supersampling)

void UseOpaque(const ImDrawList*, const ImDrawCmd*) {
    if (s_context) s_context->OMSetBlendState(s_opaque, nullptr, 0xFFFFFFFF);   // the engine's alpha is not coverage
}
void UseAdditive(const ImDrawList*, const ImDrawCmd*) {
    if (s_context) s_context->OMSetBlendState(s_additive, nullptr, 0xFFFFFFFF);
}

bool Prepare(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (!device || !context) return false;
    s_context = context;
    if (!s_frameRtv) {
        // Reuse existing captured stage if frame is temporarily holding, so character never flashes out
        return (s_copySrv != nullptr && s_opaque != nullptr && s_additive != nullptr);
    }
    ID3D11Resource* resource = nullptr;
    s_frameRtv->GetResource(&resource);
    ID3D11Texture2D* backBuffer = nullptr;
    if (resource) resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    if (resource) resource->Release();
    if (!backBuffer) return false;
    D3D11_TEXTURE2D_DESC desc{};
    backBuffer->GetDesc(&desc);
    bool ok = desc.SampleDesc.Count == 1;
    if (ok && (!s_copy || s_copyW != desc.Width || s_copyH != desc.Height || s_copyFormat != desc.Format)) {
        if (s_copySrv) { s_copySrv->Release(); s_copySrv = nullptr; }
        if (s_copy) { s_copy->Release(); s_copy = nullptr; }
        D3D11_TEXTURE2D_DESC copy = desc;
        copy.Usage = D3D11_USAGE_DEFAULT;
        copy.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        copy.CPUAccessFlags = 0;
        copy.MiscFlags = 0;
        copy.MipLevels = 1;
        copy.ArraySize = 1;
        ok = SUCCEEDED(device->CreateTexture2D(&copy, nullptr, &s_copy)) && s_copy &&
             SUCCEEDED(device->CreateShaderResourceView(s_copy, nullptr, &s_copySrv));
        s_copyW = desc.Width;
        s_copyH = desc.Height;
        s_copyFormat = desc.Format;
    }
    if (ok && s_copy) context->CopyResource(s_copy, backBuffer);
    backBuffer->Release();
    if (!s_opaque) {
        D3D11_BLEND_DESC blend{};
        blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&blend, &s_opaque);
        blend.RenderTarget[0].BlendEnable = TRUE;
        blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        device->CreateBlendState(&blend, &s_additive);
    }
    return ok && s_copySrv && s_opaque && s_additive;
}

void Draw(const Ui& ui, ID3D11Device* device, ID3D11DeviceContext* context, float ruleY) {
    (void)ruleY;
    if (!ReachArmory::IsSceneReady() || !Prepare(device, context)) return;
    const float W = ui.size.x, H = ui.size.y;
    constexpr float kStageX = 0.7425f;  // the middle of the free area right of the picker

    const int cameraMode = UIManager::GetArmorCustomizer().GetCameraMode();
    const bool isElite = ReachArmory::GetSelection().previewElite;

    float k = 1.45f;
    constexpr float kFocusX = 0.590f;   // character centre in the engine frame (0x0C / 0x0B)
    float kFocusY = 0.505f;
    float targetY = ui.origin.y + H * 0.50f;

    if (cameraMode == 2) {
        // CLOSE-UP: zoom into upper torso, helmet and visor!
        k = 1.75f;
        kFocusY = isElite ? 0.380f : 0.365f;
        targetY = ui.origin.y + H * 0.50f;
    } else {
        // FULL BODY (mode 1) and ARMORY VIEW (mode 0)
        k = 1.45f;
        kFocusY = 0.505f;
        k = 1.15f; kFocusY = isElite ? 0.510f : 0.520f; targetY = ui.origin.y + H * 0.50f;
    }

    const float targetX = ui.origin.x + W * kStageX;
    const ImVec2 p0(targetX - W * kFocusX * k, targetY - H * kFocusY * k);
    const ImVec2 p1(p0.x + W * k, p0.y + H * k);

    ui.dl->AddRectFilled(ui.origin, ImVec2(ui.origin.x + W, ui.origin.y + H), IM_COL32(0, 0, 0, 255));
    ui.dl->AddCallback(&UseOpaque, nullptr);
    ui.dl->AddImage(reinterpret_cast<ImTextureID>(s_copySrv), p0, p1);
    ui.dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    // Floor pool only. The old 30-ring chest glow was additive white/blue over the model and added ~30/255 to
    // every channel, which desaturated armour colours. The engine already lights the model; keep the atmosphere
    // below the feet and leave the body untouched.
    ui.dl->AddCallback(&UseAdditive, nullptr);
    const auto glow = [&](ImVec2 centre, float radiusX, float radiusY, int rings) {
        for (int i = 0; i < rings; ++i) {
            const float t = 1.0f - static_cast<float>(i) / rings;
            const ImU32 step = IM_COL32(0, i % 2 ? 0 : 40, 255, 1);
            constexpr int kPoints = 48;
            for (int n = 0; n < kPoints; ++n) {
                const float a = static_cast<float>(n) / kPoints * 6.2831853f;
                ui.dl->PathLineTo(ImVec2(centre.x + std::cos(a) * radiusX * t, centre.y + std::sin(a) * radiusY * t));
            }
            ui.dl->PathFillConvex(step);
        }
    };
    glow(ImVec2(ui.origin.x + W * kStageX, p0.y + H * k * 0.85f), H * 0.36f, H * 0.048f, 10);
    // Project the player's configured emblem onto the Spartan chest/shoulder emblem patch in the 3D Armory viewer when facing forward.
    if (!isElite) {
        const auto& customizer = UIManager::GetArmorCustomizer();
        const auto& prof = customizer.GetProfile();
        const float yaw = ReachArmory::GetYawRotation();
        const float cosY = std::cos(yaw + 0.32f);
        if (cosY > 0.20f) {
            const float sinY = std::sin(yaw + 0.32f);
            const float patchSize = H * k * (cameraMode == 2 ? 0.026f : 0.022f);
            const float cx = targetX - W * k * (0.018f + sinY * 0.032f);
            const float cy = p0.y + H * k * (cameraMode == 2 ? 0.345f : 0.318f);
            UiArt::EmblemColors ec = customizer.EmblemColorsNow();
            const uint8_t alpha = static_cast<uint8_t>(std::clamp(cosY * 215.0f, 0.0f, 215.0f));
            ec.primary = (ec.primary & 0x00FFFFFFu) | (static_cast<ImU32>(alpha) << 24);
            ec.secondary = (ec.secondary & 0x00FFFFFFu) | (static_cast<ImU32>(alpha) << 24);
            ec.background = (ec.background & 0x00FFFFFFu) | (static_cast<ImU32>(alpha) << 24);
            UiArt::DrawEmblem(ui.dl, prof.emblemIndex, prof.emblemBackgroundIndex,
                              ImVec2(cx - patchSize * 0.5f * cosY, cy - patchSize * 0.5f),
                              ImVec2(cx + patchSize * 0.5f * cosY, cy + patchSize * 0.5f), ec);
        }
    }
    ui.dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    // The panels on the left sit on a dark edge, and the picture's left end doesn't show as a seam.
    ui.dl->AddRectFilledMultiColor(ui.origin, ImVec2(ui.origin.x + W * 0.56f, ui.origin.y + H), IM_COL32(4, 8, 14, 255), IM_COL32(4, 8, 14, 0),
                                   IM_COL32(4, 8, 14, 0), IM_COL32(4, 8, 14, 255));
}

} // namespace ArmoryStage

void DrawBackdrop(const Ui& ui, Screen screen) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp && (ui.origin.x > 0.0f || ui.origin.y > 0.0f || ui.size.x < vp->Size.x || ui.size.y < vp->Size.y)) {
        ui.dl->AddRectFilled(ImVec2(0, 0), vp->Size, IM_COL32(0, 0, 0, 255));
    }
    const ImVec2 o = ui.origin;
    const ImVec2 extent(ui.origin.x + ui.size.x, ui.origin.y + ui.size.y);
    // Front-end screens use the selected menu background (the player's own pictures in
    // data\ui\custom\backgrounds first, then Noble/Reach art). This is the backdrop the player chose;
    // it also means the main menu never shows the engine map's camera cuts. The Armory is the one
    // exception: its live 3D Spartan/Elite is drawn straight into the backbuffer and must show through.
    const bool needBackdropImage = screen != Screen::Armory;
    if (needBackdropImage) {
        ui.dl->AddRectFilled(o, extent, IM_COL32(8, 12, 18, 255));
        // The lobby shows the map that is going to load, when there is a picture of it.
        UiTexture bg;
        if (screen == Screen::PreGameLobby && !s_lobbyBackdropImage.empty()) {
            bg = UiArt::ImageFile(s_lobbyBackdropImage);
        } else if (screen == Screen::PreGameLobby && !s_lobbyBackdropMap.empty()) {
            const std::string scenario = MapNames::ScenarioOf(s_lobbyBackdropMap);
            bg = UiArt::MapPhoto(scenario.empty() ? s_lobbyBackdropMap : scenario);
        }
        if (!bg) bg = UiArt::MenuBackground();
        if (bg) {
            UiArt::DrawCover(ui.dl, bg, o, extent, IM_COL32(255, 255, 255, 255));
        }
    }

    if (screen == Screen::Armory) {
        // In the Armory, provide sleek subtle left-side menu darkening while keeping the 3D Spartan/Elite
        // character preview completely unoccluded on the right side
        ui.dl->AddRectFilledMultiColor(o, ImVec2(ui.origin.x + ui.size.x * 0.44f, extent.y),
                                       IM_COL32(4, 8, 14, 180), IM_COL32(4, 8, 14, 85),
                                       IM_COL32(4, 8, 14, 85), IM_COL32(4, 8, 14, 180));
        ui.dl->AddRectFilledMultiColor(ImVec2(ui.origin.x + ui.size.x * 0.44f, ui.origin.y), ImVec2(ui.origin.x + ui.size.x * 0.60f, extent.y),
                                       IM_COL32(4, 8, 14, 85), IM_COL32(4, 8, 14, 0),
                                       IM_COL32(4, 8, 14, 0), IM_COL32(4, 8, 14, 85));
        ui.dl->AddRectFilledMultiColor(ImVec2(ui.origin.x, ui.origin.y + ui.size.y * 0.82f), extent,
                                       IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
                                       IM_COL32(0, 0, 0, 160), IM_COL32(0, 0, 0, 160));
    } else if (screen == Screen::PreGameLobby) {
        // Pre-game lobby: subtle left darkening for menu items and bottom vignette matching Halo: Reach 360/MCC
        ui.dl->AddRectFilledMultiColor(o, ImVec2(ui.origin.x + ui.size.x * 0.48f, extent.y),
                                       IM_COL32(0, 0, 0, 175), IM_COL32(0, 0, 0, 60),
                                       IM_COL32(0, 0, 0, 60), IM_COL32(0, 0, 0, 175));
        ui.dl->AddRectFilledMultiColor(ImVec2(ui.origin.x, ui.origin.y + ui.size.y * 0.75f), extent,
                                       IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
                                       IM_COL32(0, 0, 0, 185), IM_COL32(0, 0, 0, 185));
    } else {
        // Darken the left half (where the menu list is) and the bottom, like Reach's native menus.
        ui.dl->AddRectFilledMultiColor(o, ImVec2(ui.origin.x + ui.size.x * 0.45f, extent.y),
                                       IM_COL32(0, 0, 0, 190), IM_COL32(0, 0, 0, 105),
                                       IM_COL32(0, 0, 0, 105), IM_COL32(0, 0, 0, 190));
        ui.dl->AddRectFilledMultiColor(ImVec2(ui.origin.x + ui.size.x * 0.45f, ui.origin.y), ImVec2(ui.origin.x + ui.size.x * 0.78f, extent.y),
                                       IM_COL32(0, 0, 0, 105), IM_COL32(0, 0, 0, 0),
                                       IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 105));
        ui.dl->AddRectFilledMultiColor(ImVec2(ui.origin.x, ui.origin.y + ui.size.y * 0.72f), extent,
                                       IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
                                       IM_COL32(0, 0, 0, 165), IM_COL32(0, 0, 0, 165));
    }
}

void DrawTitle(const Ui& ui, ImVec2 pos, const char* title, const char* sideLabel = nullptr) {
    const float d = 15.0f * ui.s;
    const ImVec2 c(pos.x + d, pos.y + 28.0f * ui.s);
    ui.dl->AddQuad(ImVec2(c.x, c.y - d), ImVec2(c.x + d, c.y), ImVec2(c.x, c.y + d), ImVec2(c.x - d, c.y), kAccent, 2.0f * ui.s);
    ui.dl->AddQuad(ImVec2(c.x, c.y - d * 0.45f), ImVec2(c.x + d * 0.45f, c.y), ImVec2(c.x, c.y + d * 0.45f),
                   ImVec2(c.x - d * 0.45f, c.y), IM_COL32(143, 216, 255, 140), 1.5f * ui.s);
    Text(ui, NobleStyle::HeroFont(), 50.0f, ImVec2(pos.x + d * 2.6f, pos.y), IM_COL32(220, 226, 232, 215), title);
    if (sideLabel) {
        float y = pos.y + 70.0f * ui.s;
        for (const char* p = sideLabel; *p; ++p) {
            char ch[2] = { *p, 0 };
            Text(ui, NobleStyle::TitleFont(), 26.0f, ImVec2(pos.x + 2.0f * ui.s, y), IM_COL32(140, 150, 160, 70), ch);
            y += 24.0f * ui.s;
        }
    }
}

struct ListItem {
    std::string label;
    bool enabled = true;
    bool header = false;
    bool expanded = false;
    std::string thumb;   // scenario whose small preview is drawn before the label (empty = none)
    std::filesystem::path image; // a mod map's own thumbnail file (wins over thumb)
    int number = 0;      // shown before the thumbnail when > 0
};

bool AcceptPressed();

int DrawList(const Ui& ui, const char* id, ImVec2 pos, float width, const std::vector<ListItem>& items, int& selected,
             bool keyboard, float rowHeight = 30.0f, int maxRows = 0, bool hoverSelects = true) {
    const float rh = rowHeight * ui.s;
    int activated = -1;
    const int n = static_cast<int>(items.size());
    if (n == 0) return -1;
    selected = std::clamp(selected, 0, n - 1);

    if (keyboard) {
        // Rows that cannot be started (a modded map Reach has no launch index for) are still selectable, so
        // the map banner can say why; only Accept is blocked for them.
        auto step = [&](int dir) {
            selected = (selected + dir + n) % n;
            MenuSounds::Scroll();
        };
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown)) step(1);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickUp)) step(-1);
        if (AcceptPressed() && items[selected].enabled) {
            activated = selected;
        }
    }

    static std::unordered_map<ImGuiID, int> s_first, s_lastSelected;
    const ImGuiID key = ImGui::GetID(id);
    int& first = s_first[key];
    if (maxRows > 0 && n > maxRows) {
        auto lastSel = s_lastSelected.find(key);
        if (lastSel == s_lastSelected.end() || lastSel->second != selected) {
            if (selected < first) first = selected;
            else if (selected >= first + maxRows) first = selected - maxRows + 1;
            s_lastSelected[key] = selected;
        }
        const ImVec2 area0 = pos, area1(pos.x + width + 14.0f * ui.s, pos.y + maxRows * rh);
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f && ImGui::IsMouseHoveringRect(area0, area1, false)) first -= static_cast<int>(wheel * 3.0f);
        first = std::clamp(first, 0, n - maxRows);
        const float trackX = pos.x + width + 6.0f * ui.s, trackH = maxRows * rh;
        ui.dl->AddRectFilled(ImVec2(trackX, pos.y), ImVec2(trackX + 3.0f * ui.s, pos.y + trackH), IM_COL32(255, 255, 255, 25));
        const float thumbH = std::max(18.0f * ui.s, trackH * maxRows / n);
        const float thumbY = pos.y + (trackH - thumbH) * first / static_cast<float>(n - maxRows);
        ui.dl->AddRectFilled(ImVec2(trackX, thumbY), ImVec2(trackX + 3.0f * ui.s, thumbY + thumbH), IM_COL32(143, 216, 255, 170));
    } else {
        first = 0;
    }
    const int last = maxRows > 0 ? std::min(n, first + maxRows) : n;

    for (int i = first; i < last; ++i) {
        const ImVec2 a(pos.x, pos.y + (i - first) * rh);
        const ImVec2 b(a.x + width, a.y + rh);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(id);
        ImGui::PushID(i);
        ImGui::InvisibleButton("##row", ImVec2(width, rh));
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        ImGui::PopID();
        ImGui::PopID();
        const bool mouseMoved = ImGui::GetIO().MouseDelta.x * ImGui::GetIO().MouseDelta.x +
                                ImGui::GetIO().MouseDelta.y * ImGui::GetIO().MouseDelta.y > 0.0f;
        if (hovered && selected != i && mouseMoved) {
            if (hoverSelects) selected = i;
            MenuSounds::Hover(ImGui::GetID(id) + i);
        }
        if (hovered && !hoverSelects && i != selected && items[i].enabled) {
            ui.dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 16));
        }
        if (clicked) {
            selected = i;
            if (items[i].enabled) activated = i;
        }
        if (i == selected) {
            ui.dl->AddRectFilledMultiColor(a, b, IM_COL32(255, 255, 255, 46), IM_COL32(255, 255, 255, 4),
                                           IM_COL32(255, 255, 255, 4), IM_COL32(255, 255, 255, 46));
            ui.dl->AddRectFilled(a, ImVec2(a.x + 2.0f * ui.s, b.y), kWhite);
        }
        if (items[i].header) {
            const float cx = a.x + 14.0f * ui.s, cy = a.y + rh * 0.5f, t = 5.0f * ui.s;
            const ImU32 ac = i == selected ? kWhite : kAccent;
            if (items[i].expanded) ui.dl->AddTriangleFilled(ImVec2(cx - t, cy - t * 0.6f), ImVec2(cx + t, cy - t * 0.6f), ImVec2(cx, cy + t * 0.8f), ac);
            else ui.dl->AddTriangleFilled(ImVec2(cx - t * 0.6f, cy - t), ImVec2(cx - t * 0.6f, cy + t), ImVec2(cx + t * 0.8f, cy), ac);
            // Workshop mod names can be very long ("... (NEW CUSTOM MAP "STAGING GROUND" OUT NOW!)"), so the
            // heading is clipped to its row instead of running into the column on the right.
            ui.dl->PushClipRect(ImVec2(a.x, a.y), ImVec2(b.x - 4.0f * ui.s, b.y), true);
            const std::string headerLabel = Ellipsize(NobleStyle::TitleFont(), 19.0f * ui.s, Upper(items[i].label),
                                                       b.x - a.x - 32.0f * ui.s);
            Text(ui, NobleStyle::TitleFont(), 19.0f, ImVec2(a.x + 28.0f * ui.s, a.y + (rh - 19.0f * ui.s) * 0.5f), ac,
                 headerLabel.c_str());
            ui.dl->PopClipRect();
            ui.dl->AddLine(ImVec2(a.x, b.y - 1.0f), ImVec2(b.x, b.y - 1.0f), IM_COL32(143, 216, 255, 60), 1.0f);
            continue;
        }
        const ImU32 col = !items[i].enabled ? kDisabled : (i == selected ? kWhite : kDim);
        float textX = a.x + (items[0].header ? 28.0f : 9.0f) * ui.s;
        if ((!items[i].thumb.empty() || !items[i].image.empty()) && rh >= 34.0f * ui.s) {
            // Number, then the map's small preview (MCC's levels pack), then the name.
            if (items[i].number > 0) {
                char num[8];
                std::snprintf(num, sizeof(num), "%02d", items[i].number);
                Text(ui, NobleStyle::SmallFont(), 14.0f, ImVec2(textX, a.y + (rh - 14.0f * ui.s) * 0.5f),
                     i == selected ? kAccent : IM_COL32(120, 135, 150, 255), num);
                textX += 24.0f * ui.s;
            }
            const float th = rh - 8.0f * ui.s, tw = th * 316.0f / 160.0f;
            const ImVec2 t0(textX, a.y + 4.0f * ui.s), t1(textX + tw, a.y + 4.0f * ui.s + th);
            ui.dl->AddRectFilled(t0, t1, IM_COL32(14, 20, 26, 220));
            const UiTexture tex = items[i].image.empty() ? UiArt::MapPreview(items[i].thumb, false)
                                                          : UiArt::ImageFile(items[i].image);
            if (tex) {
                UiArt::DrawCover(ui.dl, tex, t0, t1, items[i].enabled ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 90));
            }
            ui.dl->AddRect(t0, t1, i == selected ? IM_COL32(170, 220, 255, 200) : IM_COL32(255, 255, 255, 40), 0.0f, 0, 1.0f);
            textX += tw + 10.0f * ui.s;
        }
        const std::string label = Ellipsize(NobleStyle::TitleFont(), 20.0f * ui.s, Upper(items[i].label),
                                            b.x - textX - 6.0f * ui.s);
        ui.dl->PushClipRect(ImVec2(textX, a.y), ImVec2(b.x - 2.0f * ui.s, b.y), true);
        Text(ui, NobleStyle::TitleFont(), 20.0f, ImVec2(textX, a.y + (rh - 20.0f * ui.s) * 0.5f), col,
             label.c_str());
        ui.dl->PopClipRect();
    }
    if (activated >= 0) MenuSounds::Click();
    return activated;
}

bool DrawSourceTabs(const Ui& ui, ImVec2 pos, int& active, int moddedCount) {
    const char* labels[2] = { "VANILLA", "MODDED" };
    int next = active;
    float x = pos.x;
    for (int i = 0; i < 2; ++i) {
        char text[32];
        if (i == 1) std::snprintf(text, sizeof(text), "%s (%d)", labels[i], moddedCount);
        else std::snprintf(text, sizeof(text), "%s", labels[i]);
        const float w = TextWidth(NobleStyle::TitleFont(), 20.0f * ui.s, text) + 24.0f * ui.s;
        const float h = 30.0f * ui.s;
        ImGui::SetCursorScreenPos(ImVec2(x, pos.y));
        ImGui::PushID("srctab");
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##t", ImVec2(w, h))) next = i;
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        ImGui::PopID();
        const bool on = active == i;
        if (on) {
            ui.dl->AddRectFilled(ImVec2(x, pos.y), ImVec2(x + w, pos.y + h), IM_COL32(60, 120, 170, 90));
            ui.dl->AddRectFilled(ImVec2(x, pos.y + h - 2.0f * ui.s), ImVec2(x + w, pos.y + h), kAccent);
        } else if (hovered) {
            ui.dl->AddRectFilled(ImVec2(x, pos.y), ImVec2(x + w, pos.y + h), IM_COL32(255, 255, 255, 18));
        }
        Text(ui, NobleStyle::TitleFont(), 20.0f, ImVec2(x + 12.0f * ui.s, pos.y + 4.0f * ui.s), on ? kWhite : kDim, text);
        x += w + 6.0f * ui.s;
    }
    const bool typing = ImGui::GetIO().WantTextInput;   // Q and E are letters in a search box
    if ((!typing && ImGui::IsKeyPressed(ImGuiKey_Q, false)) || ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false)) next = 0;
    if ((!typing && ImGui::IsKeyPressed(ImGuiKey_E, false)) || ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)) next = 1;
    if (next == active) return false;
    MenuSounds::Tab(next > active ? 1 : -1);
    active = next;
    return true;
}

using InputPrompts::Prompt;

// A button hint at the bottom (ESC Back, ENTER Select, ...) clicked with the mouse. Set by DrawHints,
// read on the next frame as if the key had been pressed.
int s_hintClicked = -1;      // Prompt clicked during the last frame, -1 = none
int s_hintThisFrame = -1;    // the click being handled this frame
bool HintClicked(Prompt p) { return s_hintThisFrame == static_cast<int>(p); }

bool AcceptPressed() {
    // A modal (Account, Developer, or the Workshop-mod gate) owns ENTER/A while it is up; otherwise the
    // screen behind it would also act on the same press.
    if (ModDownloadPrompt::Get().IsActive()) return false;
    return ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
           ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false) || HintClicked(Prompt::Accept);
}

// A prompt for a button with no Prompt of its own: Xbox glyph on a controller, key box on keyboard and mouse.
struct HintExtra {
    InputPrompts::Button button;
    const char* key;
    const char* label;
};

void DrawHints(const Ui& ui, const std::vector<std::pair<Prompt, const char*>>& hints, const std::vector<HintExtra>& extras = {}, float left = 0.075f) {
    float x = ui.origin.x + ui.size.x * left;
    const float y = ui.origin.y + ui.size.y - 46.0f * ui.s;
    ui.dl->AddLine(ImVec2(x, y - 12.0f * ui.s), ImVec2(ui.origin.x + ui.size.x * 0.62f, y - 12.0f * ui.s), IM_COL32(255, 255, 255, 50), 1.0f);
    for (const auto& [prompt, label] : hints) {
        const float x0 = x;
        x += InputPrompts::Draw(ui.dl, NobleStyle::SmallFont(), ImVec2(x, y), 22.0f * ui.s, prompt) + 7.0f * ui.s;
        Text(ui, NobleStyle::SmallFont(), 17.0f, ImVec2(x, y + 2.0f * ui.s), kWhite, label);
        x += TextWidth(NobleStyle::SmallFont(), 17.0f * ui.s, label);
        // Back and Select can also be clicked (Change / Tabs only describe keys, so they are not buttons).
        if (prompt == Prompt::Back || prompt == Prompt::Accept) {
            const ImVec2 r0(x0 - 6.0f * ui.s, y - 5.0f * ui.s);
            const ImVec2 r1(x + 8.0f * ui.s, y + 27.0f * ui.s);
            if (ImGui::IsMouseHoveringRect(r0, r1, false)) {
                ui.dl->AddRectFilled(r0, r1, IM_COL32(143, 216, 255, 40), 3.0f * ui.s);
                ui.dl->AddRect(r0, r1, IM_COL32(143, 216, 255, 120), 3.0f * ui.s);
                if (ImGui::IsMouseClicked(0)) {
                    s_hintClicked = static_cast<int>(prompt);
                    MenuSounds::Click();
                }
            }
        }
        x += 26.0f * ui.s;
    }
    const bool pad = InputPrompts::Current() == InputPrompts::Device::Gamepad;
    for (const HintExtra& extra : extras) {
        x += (pad ? InputPrompts::DrawButton(ui.dl, NobleStyle::SmallFont(), ImVec2(x, y), 22.0f * ui.s, extra.button)
                  : InputPrompts::DrawKey(ui.dl, NobleStyle::SmallFont(), ImVec2(x, y), 22.0f * ui.s, extra.key)) + 7.0f * ui.s;
        Text(ui, NobleStyle::SmallFont(), 17.0f, ImVec2(x, y + 2.0f * ui.s), kWhite, extra.label);
        x += TextWidth(NobleStyle::SmallFont(), 17.0f * ui.s, extra.label) + 26.0f * ui.s;
    }
}


static bool s_showAccountModal = false;
static bool s_openAccountPopup = false;
static int s_accountTab = 0;
static char s_accountUser[32] = "";
static char s_accountPass[32] = "";
static std::string s_accountStatusMsg = "";
static bool s_accountStatusError = false;
static bool s_showDevMenu = false;
static bool s_openDevMenuPopup = false;
static std::string s_devStatusMsg = "";

static void OpenAccountModal() {
    s_showAccountModal = true;
    s_openAccountPopup = true;
    s_accountStatusMsg.clear();
}

static void OpenDevMenuModal() {
    s_showDevMenu = true;
    s_openDevMenuPopup = true;
    s_devStatusMsg.clear();
}

void DrawDevMenuModal(const Ui& ui) {
    if (!s_showDevMenu) return;

    // Strict security check: only developers can access this menu!
    if (!AuthClient::Get().IsDeveloper()) {
        s_showDevMenu = false;
        return;
    }

    if (s_openDevMenuPopup) {
        ImGui::OpenPopup("NOBLE DEVELOPER CONSOLE");
        s_openDevMenuPopup = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f * ui.s, 0));

    if (ImGui::BeginPopupModal("NOBLE DEVELOPER CONSOLE", &s_showDevMenu, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "[DEV] NOBLE DEVELOPER MENU");
        ImGui::TextDisabled("Logged in as Developer: %s (User ID: %d)",
                           AuthClient::Get().GetUsername().c_str(), AuthClient::Get().GetUserId());
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4));

        // 1. UNLOCK ALL ARMORS
        if (ImGui::Button("UNLOCK ALL ARMORS", ImVec2(220, 32))) {
            Career::DevSetArmoryUnlockAll(true);
            s_devStatusMsg = "SUCCESS: All Spartan and Elite armors unlocked!";
            MenuSounds::Click();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Unlocks all armors & permutations");

        // 2. GIVE MAX CREDITS
        if (ImGui::Button("GIVE MAX CREDITS", ImVec2(220, 32))) {
            Career::DevGrantMaxCredits();
            s_devStatusMsg = "SUCCESS: Max credits granted (+999,999,999 cR)!";
            MenuSounds::Click();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Grants 999,999,999 cR spendable balance");

        // 3. GIVE MAX RANK
        if (ImGui::Button("GIVE MAX RANK", ImVec2(220, 32))) {
            Career::DevSetMaxRank();
            s_devStatusMsg = "SUCCESS: Promoted to highest rank (Inheritor)!";
            MenuSounds::Click();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Sets rank to Inheritor (20,000,000 cR)");

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4));

        // Convenience: UNLOCK ALL & MAX EVERYTHING
        if (ImGui::Button("MAX EVERYTHING", ImVec2(220, 32))) {
            Career::DevSetArmoryUnlockAll(true);
            Career::DevGrantMaxCredits();
            Career::DevSetMaxRank();
            s_devStatusMsg = "SUCCESS: All armors unlocked, Inheritor rank & 999M cR granted!";
            MenuSounds::Click();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Unlock all armors, max rank & credits");

        // Reset progress option
        if (ImGui::Button("RESET CAREER", ImVec2(220, 28))) {
            Career::DevResetProgress();
            s_devStatusMsg = "Career reset: 0 cR, Recruit rank, armors locked.";
            MenuSounds::Click();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Reset to default (testing only)");

        if (!s_devStatusMsg.empty()) {
            ImGui::Dummy(ImVec2(0, 6));
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", s_devStatusMsg.c_str());
        }

        ImGui::Dummy(ImVec2(0, 8));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4));
        if (ImGui::Button("CLOSE", ImVec2(100, 30))) {
            s_showDevMenu = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } else {
        s_showDevMenu = false;
    }
}

void DrawAccountModal(const Ui& ui) {
    if (!s_showAccountModal) return;

    if (s_openAccountPopup) {
        ImGui::OpenPopup("ACCOUNT LOGIN / REGISTER");
        s_openAccountPopup = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f * ui.s, 0));

    if (ImGui::BeginPopupModal("ACCOUNT LOGIN / REGISTER", &s_showAccountModal, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
        bool isLoggedIn = AuthClient::Get().IsLoggedIn();
        if (isLoggedIn) {
            static uint64_t s_lastDevCheck = 0;
            uint64_t now = GetTickCount64();
            if (now - s_lastDevCheck > 2500) {
                s_lastDevCheck = now;
                AuthClient::Get().RefreshDeveloperStatusAsync();
            }

            const bool isDev = AuthClient::Get().IsDeveloper();
            // One line for the account, one gold chip for developer status - no more "[DEV] [DEVELOPER]".
            ImGui::TextColored(isDev ? ImVec4(1.0f, 0.85f, 0.2f, 1.0f) : ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                               "Logged in as: %s", AuthClient::Get().GetUsername().c_str());
            if (isDev) {
                ImGui::SameLine();
                const ImVec2 chip = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(chip, ImVec2(chip.x + 46.0f * ui.s, chip.y + 18.0f * ui.s),
                                                          IM_COL32(220, 160, 20, 240), 3.0f * ui.s);
                ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), 11.0f * ui.s,
                                                    ImVec2(chip.x + 10.0f * ui.s, chip.y + 3.0f * ui.s),
                                                    IM_COL32(10, 10, 10, 255), "DEV");
                ImGui::Dummy(ImVec2(50.0f * ui.s, 18.0f * ui.s));
                ImGui::Dummy(ImVec2(0, 4));
                ImGui::PushStyleColor(ImGuiCol_Text, NobleStyle::TextMuted());
                ImGui::TextWrapped("Developer account (ID: %d). The Developer Menu is on the main menu, above Quit.",
                                   AuthClient::Get().GetUserId());
                ImGui::PopStyleColor();
            } else {
                ImGui::Dummy(ImVec2(0, 4));
                ImGui::TextDisabled("Standard account (ID: %d)", AuthClient::Get().GetUserId());
            }
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 8));
            ImGui::TextUnformatted("Display name");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##newname", s_accountUser, sizeof(s_accountUser));
            if (ImGui::Button("CHANGE NAME", ImVec2(-1, 30.0f * ui.s))) {
                if (AuthClient::Get().SetPlayerName(s_accountUser)) {
                    s_accountStatusMsg = "Display name changed. Your player ID is unchanged.";
                    s_accountStatusError = false;
                } else {
                    s_accountStatusMsg = "Enter a name.";
                    s_accountStatusError = true;
                }
            }
            if (!s_accountStatusMsg.empty()) {
                ImGui::TextColored(s_accountStatusError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                                   "%s", s_accountStatusMsg.c_str());
            }
            ImGui::Dummy(ImVec2(0, 8));
            if (ImGui::Button("CLOSE", ImVec2(-1, 32.0f * ui.s))) {
                s_showAccountModal = false;
                ImGui::CloseCurrentPopup();
            }
        } else {
            // Login / Register: two equal tabs over full-width fields, and the same button pairing as above.
            const float tabW = (ImGui::GetContentRegionAvail().x - 10.0f * ui.s) * 0.5f;
            const bool loginTab = (s_accountTab == 0);
            const ImVec4 on(0.20f, 0.43f, 0.61f, 1.0f);
            if (loginTab) ImGui::PushStyleColor(ImGuiCol_Button, on);
            if (ImGui::Button("LOG IN", ImVec2(tabW, 30.0f * ui.s))) s_accountTab = 0;
            if (loginTab) ImGui::PopStyleColor();
            ImGui::SameLine();
            if (!loginTab) ImGui::PushStyleColor(ImGuiCol_Button, on);
            if (ImGui::Button("REGISTER", ImVec2(tabW, 30.0f * ui.s))) s_accountTab = 1;
            if (!loginTab) ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 6));

            ImGui::TextDisabled(loginTab ? "Log in to your Noble account." : "Create a Noble account.");
            ImGui::Dummy(ImVec2(0, 4));
            ImGui::TextUnformatted("Username");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##acc_user", s_accountUser, sizeof(s_accountUser));
            ImGui::TextUnformatted("Password");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##acc_pass", s_accountPass, sizeof(s_accountPass), ImGuiInputTextFlags_Password);

            if (!s_accountStatusMsg.empty()) {
                ImGui::Dummy(ImVec2(0, 4));
                ImGui::TextColored(s_accountStatusError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                                   "%s", s_accountStatusMsg.c_str());
            }

            ImGui::Dummy(ImVec2(0, 8));
            const float subW = (ImGui::GetContentRegionAvail().x - 10.0f * ui.s) * 0.5f;
            if (s_accountTab == 0) {
                if (ImGui::Button("LOG IN##submit", ImVec2(subW, 32.0f * ui.s)) || (ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::IsWindowFocused())) {
                    auto res = AuthClient::Get().Login(s_accountUser, s_accountPass);
                    if (res.success) {
                        s_accountStatusMsg = "Login successful!";
                        s_accountStatusError = false;
                        s_showAccountModal = false;
                        ImGui::CloseCurrentPopup();
                    } else {
                        s_accountStatusMsg = res.error;
                        s_accountStatusError = true;
                    }
                }
            } else {
                if (ImGui::Button("REGISTER##submit", ImVec2(subW, 32.0f * ui.s)) || (ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::IsWindowFocused())) {
                    auto res = AuthClient::Get().Register(s_accountUser, s_accountPass);
                    if (res.success) {
                        s_accountStatusMsg = "Registration successful! Logged in.";
                        s_accountStatusError = false;
                        s_showAccountModal = false;
                        ImGui::CloseCurrentPopup();
                    } else {
                        s_accountStatusMsg = res.error;
                        s_accountStatusError = true;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("CANCEL", ImVec2(subW, 32.0f * ui.s))) {
                s_showAccountModal = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    } else {
        s_showAccountModal = false;
    }
}

// One player row the way MCC draws it: rank badge, emblem chip, then a nameplate bar that carries the service
// tag on its top line and the gamertag underneath. Used by MY PARTY and by the pre-game lobby roster so the two
// always match. rankIndex/emblemIndex/nameplateIndex may be 0 or negative when the data is not known (a remote
// player), in which case that piece is drawn as a plain placeholder or left out.
float PlayerRow(const Ui& ui, ImVec2 pos, float width, int rankIndex, int emblemIndex, int nameplateIndex,
                int team, const std::string& serviceTag, const std::string& name, bool dev, bool selected,
                int emblemBackground = 0, const UiArt::EmblemColors& emblemColors = {}) {
    const float h = 40.0f * ui.s;
    const float rankW = 29.0f * ui.s;      // square, centred on the row
    const float embW = 30.0f * ui.s;
    const float stripW = team ? 6.0f * ui.s : 0.0f;
    const float gap = 4.0f * ui.s;
    float x = pos.x;

    // Rank badge: a square box centred on the row height, so the art never sits high or low.
    const ImVec2 r0(x, pos.y + (h - rankW) * 0.5f);
    const ImVec2 r1(r0.x + rankW, r0.y + rankW);
    if (const UiTexture rank = UiArt::RankIcon(rankIndex)) {
        UiArt::DrawContain(ui.dl, rank, r0, r1);
    } else {
        ui.dl->AddRectFilled(r0, r1, IM_COL32(26, 34, 44, 220));
        ui.dl->AddRect(r0, r1, IM_COL32(90, 120, 150, 120), 0.0f, 0, 1.0f);
    }
    x += rankW + gap;

    // Emblem tile + team strip + nameplate bar are one block: the tile is flush with the bar and the block
    // gets a single frame, with a thin divider between tile and bar (two frames looked like a gap).
    const ImVec2 blockStart(x, pos.y);
    const ImVec2 blockEnd(pos.x + width, pos.y + h);

    const ImVec2 e0(x, pos.y), e1(x + embW, pos.y + h);
    ui.dl->AddRectFilled(e0, e1, IM_COL32(20, 14, 18, 250));
    UiArt::DrawEmblem(ui.dl, emblemIndex, emblemBackground, ImVec2(e0.x + 2.0f * ui.s, e0.y + 2.0f * ui.s),
                      ImVec2(e1.x - 2.0f * ui.s, e1.y - 2.0f * ui.s), emblemColors);
    x += embW;

    // Team strip: only when the game type assigns teams (Free for All, the main menu and a lobby with no
    // team mode show nothing).
    if (stripW > 0.0f) {
        const ImVec2 s0(x, pos.y), s1(x + stripW, pos.y + h);
        ui.dl->AddRectFilled(s0, s1, team == 1 ? IM_COL32(190, 34, 30, 255) : IM_COL32(36, 62, 170, 255));
        x += stripW;
    }

    // Nameplate bar: service tag on the top line, gamertag below.
    const ImVec2 p0(x, pos.y), p1(blockEnd);
    ui.dl->AddRectFilled(p0, p1, selected ? IM_COL32(36, 78, 112, 240) : IM_COL32(22, 26, 32, 240));
    if (nameplateIndex > 0) {
        if (const UiTexture plate = UiArt::Nameplate(nameplateIndex)) {
            // Drawn 8 s to the left and clipped, so the plate's own dark left border does not read as a gap
            // between the emblem tile and the bar.
            ui.dl->PushClipRect(p0, p1, true);
            UiArt::DrawCover(ui.dl, plate, ImVec2(p0.x - 8.0f * ui.s, p0.y), p1,
                             selected ? IM_COL32(205, 230, 255, 245) : IM_COL32(255, 255, 255, 245));
            ui.dl->PopClipRect();
            // Dark bands top and bottom so the tag and the name stay readable on the art.
            ui.dl->AddRectFilledMultiColor(p0, p1, IM_COL32(4, 8, 14, 150), IM_COL32(4, 8, 14, 95),
                                           IM_COL32(4, 8, 14, 185), IM_COL32(4, 8, 14, 185));
        }
    }

    // One frame around the whole block, plus a divider where the emblem meets the bar.
    ui.dl->AddRect(blockStart, blockEnd, IM_COL32(190, 200, 212, 110), 0.0f, 0, 1.0f);
    ui.dl->AddLine(ImVec2(p0.x, pos.y + 1.0f), ImVec2(p0.x, pos.y + h - 1.0f), IM_COL32(10, 12, 16, 220), 1.0f);

    // Service tag, and "[DEV]" beside it in gold text for developer accounts only (no chip, no extra tag
    // for regular players).
    const float pad = 8.0f * ui.s;
    const float tagY = pos.y + 3.0f * ui.s;
    const std::string tagText = serviceTag.empty() ? std::string("[117]") : "[" + serviceTag + "]";
    Text(ui, NobleStyle::SmallFont(), 11.5f, ImVec2(p0.x + pad, tagY), IM_COL32(205, 220, 235, 235), tagText.c_str());
    if (dev) {
        const float tagW = TextWidth(NobleStyle::SmallFont(), 11.5f * ui.s, tagText.c_str());
        Text(ui, NobleStyle::SmallFont(), 11.5f, ImVec2(p0.x + pad + tagW + 5.0f * ui.s, tagY),
             IM_COL32(240, 196, 70, 255), "[DEV]");
    }

    ui.dl->PushClipRect(ImVec2(p0.x + pad, pos.y + 13.0f * ui.s), ImVec2(p1.x - pad, p1.y), true);
    Text(ui, NobleStyle::TitleFont(), 16.5f, ImVec2(p0.x + pad, pos.y + 15.0f * ui.s), kWhite, name.c_str());
    ui.dl->PopClipRect();

    return h;
}

// The service tag the player is actually wearing: the Armory profile is what Reach is given (profile +0x1AC),
// so the menus follow it and only fall back to settings.json when no look has been saved yet. The two used to
// drift (settings said RTRD, the Armory said N06) and the party row disagreed with the Armory (2026-10-30).
std::string PlayerServiceTag(const SettingsData& sd) {
    const auto& prof = UIManager::GetArmorCustomizer().GetProfile();
    if (!prof.serviceTag.empty()) return prof.serviceTag;
    return sd.serviceTag.empty() ? std::string("117") : sd.serviceTag;
}

// A compact "update available / downloading / ready" panel under MY PARTY. Only the menu process
// checks for releases; the button starts the download or hands over to the update script.
bool DrawUpdateBanner(const Ui& ui) {
    const Updater::Status st = Updater::Get().Snapshot();
    if (st.state != Updater::State::Available && st.state != Updater::State::Downloading &&
        st.state != Updater::State::Ready) {
        return false;
    }
    const float w = 330.0f * ui.s;
    const float h = 74.0f * ui.s;
    const float x = ui.origin.x + ui.size.x - w - 40.0f * ui.s;
    const float y = ui.origin.y + 34.0f * ui.s + 128.0f * ui.s;
    ImDrawList* dl = ui.dl;
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(8, 16, 26, 236), 3.0f * ui.s);
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(143, 216, 255, 110), 3.0f * ui.s, 0, 1.0f);

    const char* head = st.state == Updater::State::Ready ? "UPDATE READY"
                     : st.state == Updater::State::Downloading ? "DOWNLOADING UPDATE"
                     : "UPDATE AVAILABLE";
    Text(ui, NobleStyle::SmallFont(), 13.0f, ImVec2(x + 12.0f * ui.s, y + 7.0f * ui.s), kAccent, head);
    Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(x + 12.0f * ui.s, y + 22.0f * ui.s), kWhite,
         st.latestTag.empty() ? "NEW VERSION" : st.latestTag.c_str());

    const ImVec2 b0(x + 12.0f * ui.s, y + h - 30.0f * ui.s);
    const ImVec2 b1(x + w - 12.0f * ui.s, y + h - 8.0f * ui.s);
    if (st.state == Updater::State::Downloading) {
        const float p = std::clamp(st.progress, 0.0f, 1.0f);
        dl->AddRectFilled(b0, b1, IM_COL32(20, 30, 42, 230), 2.0f * ui.s);
        dl->AddRectFilled(b0, ImVec2(b0.x + (b1.x - b0.x) * p, b1.y), IM_COL32(0, 120, 170, 230), 2.0f * ui.s);
        char label[64];
        std::snprintf(label, sizeof(label), "%.0f%% of %s", p * 100.0f, Updater::FormatBytes(st.assetSize).c_str());
        Text(ui, NobleStyle::SmallFont(), 13.0f, ImVec2(b0.x + 8.0f * ui.s, b0.y + 3.0f * ui.s), kWhite, label);
        return false;
    }

    std::string label = st.state == Updater::State::Ready
                            ? "RESTART & INSTALL"
                            : "DOWNLOAD  " + Updater::FormatBytes(st.assetSize);
    ImGui::SetCursorScreenPos(b0);
    ImGui::PushID("noble_update_button");
    const bool clicked = ImGui::InvisibleButton("##update", ImVec2(b1.x - b0.x, b1.y - b0.y));
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    dl->AddRectFilled(b0, b1, hovered ? IM_COL32(30, 78, 110, 245) : IM_COL32(18, 46, 68, 235), 2.0f * ui.s);
    dl->AddRect(b0, b1, hovered ? IM_COL32(180, 235, 255, 220) : IM_COL32(100, 170, 210, 120), 2.0f * ui.s);
    Text(ui, NobleStyle::SmallFont(), 14.0f,
         ImVec2(b0.x + ((b1.x - b0.x) - TextWidth(NobleStyle::SmallFont(), 14.0f * ui.s, label.c_str())) * 0.5f, b0.y + 3.0f * ui.s),
         kWhite, label.c_str());
    if (clicked) {
        MenuSounds::Click();
        if (st.state == Updater::State::Ready) {
            if (Updater::Get().ApplyAndRestart()) return true;
            MenuSounds::Back();
        } else {
            Updater::Get().StartDownload();
        }
    }
    return false;
}

void DrawParty(const Ui& ui, const std::string& playerName, const char* status) {
    const float w = 330.0f * ui.s;
    const float x = ui.origin.x + ui.size.x - w - 40.0f * ui.s;
    float y = ui.origin.y + 34.0f * ui.s;
    ui.dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w * 0.80f, y + 26.0f * ui.s), IM_COL32(0, 0, 0, 200));
    Text(ui, NobleStyle::TitleFont(), 17.0f, ImVec2(x + 8.0f * ui.s, y + 3.0f * ui.s), kWhite, "MY PARTY");
    y += 26.0f * ui.s;
    const float x2 = x + 36.0f * ui.s;
    ui.dl->AddRectFilled(ImVec2(x2, y), ImVec2(x + w, y + 28.0f * ui.s), IM_COL32(20, 26, 32, 215));
    Text(ui, NobleStyle::TitleFont(), 17.0f, ImVec2(x2 + 8.0f * ui.s, y + 5.0f * ui.s), kWhite, status);
    Text(ui, NobleStyle::TitleFont(), 17.0f, ImVec2(x + w - 42.0f * ui.s, y + 5.0f * ui.s), kWhite, "1/16");
    y += 28.0f * ui.s;
    // MCC-style player row: rank badge, emblem, then the nameplate bar with the service tag on top and the
    // gamertag under it (same helper the pre-game lobby uses).
    {
        const auto& prof = UIManager::GetArmorCustomizer().GetProfile();
        const SettingsData sd = Settings::Get().Snapshot();
        static int s_rank = -1;
        static ULONGLONG s_rankRead = 0;
        if (s_rank < 0 || GetTickCount64() - s_rankRead > 2000) {
            s_rank = Career::RankIndex(Career::Get().credits);
            s_rankRead = GetTickCount64();
        }
        y += PlayerRow(ui, ImVec2(x2, y), x + w - x2, s_rank, prof.emblemIndex, prof.nameplateIndex,
                       0 /* no team in the main menu */, PlayerServiceTag(sd), playerName,
                       AuthClient::Get().IsDeveloper(), false, prof.emblemBackgroundIndex, UIManager::GetArmorCustomizer().EmblemColorsNow());
    }
    const float accH = 22.0f * ui.s;
    const ImVec2 accP0(x2, y);
    const ImVec2 accP1(x + w, y + accH);
    ImGui::SetCursorScreenPos(accP0);
    ImGui::PushID("party_account_btn");
    bool accClicked = ImGui::InvisibleButton("##acc", ImVec2(accP1.x - accP0.x, accH));
    bool accHov = ImGui::IsItemHovered();
    ImGui::PopID();
    if (accClicked) {
        OpenAccountModal();
        MenuSounds::Click();
    }
    ui.dl->AddRectFilled(accP0, accP1, accHov ? IM_COL32(30, 48, 68, 230) : IM_COL32(15, 24, 34, 200));
    ui.dl->AddRect(accP0, accP1, accHov ? IM_COL32(143, 216, 255, 180) : IM_COL32(60, 90, 120, 100), 0.0f, 0, 1.0f);
    if (AuthClient::Get().IsLoggedIn()) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "ONLINE: %s", AuthClient::Get().GetUsername().c_str());
        Text(ui, NobleStyle::SmallFont(), 12.0f, ImVec2(x2 + 8.0f * ui.s, y + 4.0f * ui.s),
             AuthClient::Get().IsDeveloper() ? IM_COL32(255, 215, 0, 255) : IM_COL32(120, 220, 160, 255), buf);
    } else {
        Text(ui, NobleStyle::SmallFont(), 12.0f, ImVec2(x2 + 8.0f * ui.s, y + 4.0f * ui.s), IM_COL32(200, 210, 220, 200), "[ LOGIN / REGISTER ]");
    }

    // The developer menu moved into the main-menu list (above Quit, developer accounts only).
}

// DIFFICULTY as four tiles with Reach's difficulty icons (from the player's Reach art, see
// UiArt::DifficultyIcon; plain named tiles when there is none). Click picks; Left/Right change it
// when the row has focus. Returns the height used.
float DifficultyTiles(const Ui& ui, const char* id, ImVec2 pos, float width, int& difficulty, bool focused) {
    const float top = 6.0f * ui.s;      // keeps the label clear of the row above
    const float labelH = 26.0f * ui.s;
    const float gap = 8.0f * ui.s;
    const float tw = (width - gap * 3.0f) / 4.0f;
    const float th = 66.0f * ui.s;
    pos.y += top;
    Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(pos.x + 10.0f * ui.s, pos.y), focused ? kWhite : kDim, "DIFFICULTY");
    if (focused) ui.dl->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + 2.0f * ui.s, pos.y + labelH - 6.0f * ui.s), kAccent);
    const float ty = pos.y + labelH;
    static const char* const kNames[4] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };
    ImGui::PushID(id);
    for (int d = 0; d < 4; ++d) {
        const ImVec2 a(pos.x + d * (tw + gap), ty);
        const ImVec2 b(a.x + tw, ty + th);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(d);
        const bool clicked = ImGui::InvisibleButton("##tile", ImVec2(tw, th));
        const bool hovered = ImGui::IsItemHovered();
        const ImGuiID hoverId = ImGui::GetItemID();
        ImGui::PopID();
        const bool sel = d == difficulty;
        ui.dl->AddRectFilled(a, b, sel ? IM_COL32(52, 98, 138, 200) : (hovered ? IM_COL32(40, 56, 72, 200) : IM_COL32(16, 22, 30, 190)));
        ui.dl->AddRect(a, b, sel ? kAccent : IM_COL32(170, 220, 255, hovered ? 120 : 45), 0.0f, 0, sel ? 2.0f : 1.0f);
        const UiTexture icon = UiArt::DifficultyIcon(d);
        const float nameH = 20.0f * ui.s;
        if (icon) {
            UiArt::DrawContain(ui.dl, icon, ImVec2(a.x + 4.0f * ui.s, a.y + 4.0f * ui.s), ImVec2(b.x - 4.0f * ui.s, b.y - nameH),
                               sel || hovered ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 170));
        }
        const float nw = TextWidth(NobleStyle::TitleFont(), 15.0f * ui.s, kNames[d]);
        Text(ui, NobleStyle::TitleFont(), 15.0f, ImVec2(a.x + (tw - nw) * 0.5f, icon ? b.y - nameH : a.y + (th - nameH) * 0.5f),
             sel ? kWhite : kDim, kNames[d]);
        if (hovered) MenuSounds::Hover(hoverId);
        if (clicked && difficulty != d) {
            difficulty = d;
            MenuSounds::Change();
        }
    }
    ImGui::PopID();
    if (focused) {
        int change = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft)) change = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight)) change = 1;
        if (change) {
            difficulty = std::clamp(difficulty + change, 0, 3);
            MenuSounds::Change();
        }
    }
    return top + labelH + th;
}

int OptionRow(const Ui& ui, const char* id, ImVec2 pos, float width, const char* label, const char* value, bool focused) {
    const float h = 32.0f * ui.s;
    int change = 0;
    const ImVec2 b(pos.x + width, pos.y + h);
    // Faint plate under every row: the setup screens sit on bright map art, and dim rows were hard to read.
    ui.dl->AddRectFilled(pos, b, IM_COL32(8, 12, 18, 85));
    if (focused) {
        ui.dl->AddRectFilledMultiColor(pos, b, IM_COL32(60, 120, 170, 120), IM_COL32(60, 120, 170, 20),
                                       IM_COL32(60, 120, 170, 20), IM_COL32(60, 120, 170, 120));
        ui.dl->AddRectFilled(pos, ImVec2(pos.x + 2.0f * ui.s, b.y), kAccent);
    }
    Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(pos.x + 10.0f * ui.s, pos.y + 6.0f * ui.s), focused ? kWhite : kDim, label);
    const float vx = pos.x + width * 0.48f;
    const float vw = width * 0.52f;
    const float aw = 28.0f * ui.s;
    ImGui::PushID(id);
    ImGui::SetCursorScreenPos(ImVec2(vx, pos.y));
    if (ImGui::InvisibleButton("##l", ImVec2(aw, h))) change = -1;
    const bool hl = ImGui::IsItemHovered();
    ImGui::SetCursorScreenPos(ImVec2(vx + vw - aw, pos.y));
    if (ImGui::InvisibleButton("##r", ImVec2(aw, h))) change = 1;
    const bool hr = ImGui::IsItemHovered();
    ImGui::PopID();
    const float cy = pos.y + h * 0.5f, ah = 6.0f * ui.s;
    ui.dl->AddTriangleFilled(ImVec2(vx + aw * 0.65f, cy - ah), ImVec2(vx + aw * 0.65f, cy + ah), ImVec2(vx + aw * 0.3f, cy), hl ? kWhite : kDim);
    const float rx = vx + vw - aw;
    ui.dl->AddTriangleFilled(ImVec2(rx + aw * 0.35f, cy - ah), ImVec2(rx + aw * 0.35f, cy + ah), ImVec2(rx + aw * 0.7f, cy), hr ? kWhite : kDim);
    const std::string v = Upper(value);
    const float tw = TextWidth(NobleStyle::TitleFont(), 18.0f * ui.s, v.c_str());
    Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(vx + (vw - tw) * 0.5f, pos.y + 6.0f * ui.s), kWhite, v.c_str());
    if (focused) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft)) change = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight)) change = 1;
    }
    if (change) MenuSounds::Change();
    return change;
}

bool ActionButton(const Ui& ui, const char* id, ImVec2 pos, float width, const char* label, bool primary = true) {
    const float h = 40.0f * ui.s;
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushID(id);
    const bool pressed = ImGui::InvisibleButton("##b", ImVec2(width, h));
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    const ImU32 bg = primary ? (hovered ? IM_COL32(76, 150, 200, 255) : IM_COL32(52, 112, 156, 240))
                             : (hovered ? IM_COL32(60, 74, 88, 240) : IM_COL32(30, 38, 46, 230));
    ui.dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + h), bg);
    ui.dl->AddRect(pos, ImVec2(pos.x + width, pos.y + h), IM_COL32(170, 220, 255, hovered ? 220 : 90), 0.0f, 0, 1.0f);
    const float tw = TextWidth(NobleStyle::TitleFont(), 20.0f * ui.s, label);
    Text(ui, NobleStyle::TitleFont(), 20.0f, ImVec2(pos.x + (width - tw) * 0.5f, pos.y + 9.0f * ui.s), kWhite, label);
    if (hovered) MenuSounds::Hover(ImGui::GetID(id));
    if (pressed) MenuSounds::Click();
    return pressed;
}

float DrawPreview(const Ui& ui, ImVec2 pos, float width, const std::string& scenario, const char* name, const char* blurb) {
    const float h = width * 292.0f / 668.0f;
    const ImVec2 b(pos.x + width, pos.y + h);
    ui.dl->AddRectFilled(pos, b, IM_COL32(14, 20, 26, 230));
    UiTexture tex = UiArt::MapPreview(scenario, true);
    if (tex) UiArt::DrawCover(ui.dl, tex, pos, b);
    ui.dl->AddRect(pos, b, IM_COL32(170, 200, 220, 120), 0.0f, 0, 1.0f);
    float y = b.y + 10.0f * ui.s;
    Text(ui, NobleStyle::TitleFont(), 26.0f, ImVec2(pos.x, y), kWhite, name);
    y += 32.0f * ui.s;
    if (blurb && *blurb) {
        Text(ui, nullptr, 17.0f, ImVec2(pos.x, y), kDim, blurb, pos.x + width);
        ImFont* font = Font(nullptr);
        const float blurbHeight = font->CalcTextSizeA(17.0f * ui.s, FLT_MAX, width, blurb).y;
        y += blurbHeight + 8.0f * ui.s;
    }
    return y;
}

// Spec §3 HeroBanner: the map picture with a dark gradient, an accent kicker line, the title, and the
// blurb under the picture. Returns the y below the blurb.
float HeroBanner(const Ui& ui, ImVec2 pos, float width, float height, const std::string& scenario, const std::string& kicker,
                 const char* title, const char* blurb, const std::filesystem::path& image = {}) {
    const ImVec2 b(pos.x + width, pos.y + height);
    ui.dl->AddRectFilled(pos, b, IM_COL32(14, 20, 26, 230));
    const UiTexture art = image.empty() ? UiArt::MapPreview(scenario, true) : UiArt::ImageFile(image);
    if (art) UiArt::DrawCover(ui.dl, art, pos, b);
    const float gy = pos.y + height * 0.45f;
    ui.dl->AddRectFilledMultiColor(ImVec2(pos.x, gy), b, IM_COL32(4, 8, 12, 0), IM_COL32(4, 8, 12, 0),
                                   IM_COL32(4, 8, 12, 235), IM_COL32(4, 8, 12, 235));
    ui.dl->AddRect(pos, b, IM_COL32(170, 200, 220, 120), 0.0f, 0, 1.0f);
    const float pad = 16.0f * ui.s;
    // Long mod titles shrink until the whole name fits before falling back to an ellipsis.
    std::string bannerTitle = Upper(title);
    float titlePx = 34.0f * ui.s;
    while (titlePx > 22.0f * ui.s &&
           TextWidth(NobleStyle::TitleFont(), titlePx, bannerTitle.c_str()) > width - pad * 2.0f) {
        titlePx -= 2.0f * ui.s;
    }
    bannerTitle = Ellipsize(NobleStyle::TitleFont(), titlePx, bannerTitle, width - pad * 2.0f);
    Text(ui, NobleStyle::TitleFont(), titlePx / ui.s, ImVec2(pos.x + pad, b.y - pad - titlePx), kWhite, bannerTitle.c_str());
    if (!kicker.empty()) {
        const std::string bannerKicker = Ellipsize(NobleStyle::SmallFont(), 15.0f * ui.s, kicker, width - pad * 2.0f);
        Text(ui, NobleStyle::SmallFont(), 15.0f, ImVec2(pos.x + pad, b.y - pad - titlePx - 22.0f * ui.s), kAccent,
             bannerKicker.c_str());
    }
    float y = b.y + 8.0f * ui.s;
    if (blurb && *blurb) {
        // Two lines at most: a long workshop blurb is clipped here instead of running over the option rows.
        const float lineH = 17.0f * ui.s * 1.35f;
        const ImVec2 t0(pos.x, y), t1(pos.x + width, y + lineH * 2.0f);
        // The description sits over map art, so it gets a soft dark band and a 1 px shadow: plain grey text
        // on a bright picture was borderline unreadable (Tyler, 2026-10-30).
        ui.dl->AddRectFilledMultiColor(ImVec2(t0.x, t0.y - 4.0f * ui.s), ImVec2(t1.x, t1.y + 4.0f * ui.s),
                                       IM_COL32(4, 8, 12, 40), IM_COL32(4, 8, 12, 150),
                                       IM_COL32(4, 8, 12, 150), IM_COL32(4, 8, 12, 40));
        const std::string fitted = FitWrapped(nullptr, 17.0f * ui.s, blurb, width - 4.0f * ui.s, lineH * 2.0f);
        ui.dl->PushClipRect(ImVec2(pos.x, y - 2.0f * ui.s), ImVec2(pos.x + width, y + lineH * 2.0f), true);
        Text(ui, nullptr, 17.0f, ImVec2(pos.x + 1.0f * ui.s, y + 1.0f * ui.s), IM_COL32(0, 0, 0, 210), fitted.c_str(), pos.x + width);
        Text(ui, nullptr, 17.0f, ImVec2(pos.x, y), IM_COL32(214, 224, 236, 255), fitted.c_str(), pos.x + width);
        ui.dl->PopClipRect();
        y += std::min(Font(nullptr)->CalcTextSizeA(17.0f * ui.s, FLT_MAX, width, fitted.c_str()).y, lineH * 2.0f) + 8.0f * ui.s;
    }
    return y;
}

// Spec §3 BigAction: large button with a title and a subtitle line. Disabled buttons are greyed and
// show their subtitle as the reason.
bool BigAction(const Ui& ui, const char* id, ImVec2 pos, float width, const char* label, const char* sub, bool primary,
               bool enabled, bool focused = false) {
    const float h = 50.0f * ui.s;
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushID(id);
    const bool pressed = ImGui::InvisibleButton("##b", ImVec2(width, h)) && enabled;
    const bool hovered = ImGui::IsItemHovered() && enabled;
    ImGui::PopID();
    const ImVec2 b(pos.x + width, pos.y + h);
    const ImU32 bg = !enabled ? IM_COL32(24, 30, 36, 200)
                   : primary ? (hovered || focused ? IM_COL32(76, 150, 200, 255) : IM_COL32(52, 112, 156, 240))
                             : (hovered || focused ? IM_COL32(60, 74, 88, 240) : IM_COL32(30, 38, 46, 230));
    ui.dl->AddRectFilled(pos, b, bg);
    ui.dl->AddRect(pos, b, enabled ? IM_COL32(170, 220, 255, hovered || focused ? 220 : 90) : IM_COL32(255, 255, 255, 30), 0.0f, 0, 1.0f);
    const std::string actionLabel = Ellipsize(NobleStyle::TitleFont(), 20.0f * ui.s, label, width - 28.0f * ui.s);
    Text(ui, NobleStyle::TitleFont(), 20.0f, ImVec2(pos.x + 14.0f * ui.s, pos.y + 5.0f * ui.s), enabled ? kWhite : kDisabled,
         actionLabel.c_str());
    if (sub && *sub) {
        const std::string actionSub = Ellipsize(NobleStyle::SmallFont(), 14.0f * ui.s, sub, width - 28.0f * ui.s);
        Text(ui, NobleStyle::SmallFont(), 14.0f, ImVec2(pos.x + 14.0f * ui.s, pos.y + 29.0f * ui.s),
             enabled ? IM_COL32(200, 225, 245, 230) : kDisabled, actionSub.c_str());
    }
    if (hovered) MenuSounds::Hover(ImGui::GetID(id));
    if (pressed) MenuSounds::Click();
    return pressed;
}

// Small icons of the skulls that are on, in a row; returns the width used.
float SkullStrip(const Ui& ui, ImVec2 pos, float size, uint64_t mask, int maxIcons);

float SkullStrip(const Ui& ui, ImVec2 pos, float size, uint64_t mask, int maxIcons) {
    float x = pos.x;
    int shown = 0;
    for (int i = 0; i < kSkullCount && shown < maxIcons; ++i) {
        if (!(mask & (1ull << kSkulls[i].bit))) continue;
        if (UiTexture icon = UiArt::SkullIcon(kSkulls[i].icon)) {
            ui.dl->AddImage(icon.id, ImVec2(x, pos.y), ImVec2(x + size, pos.y + size));
        } else {
            ui.dl->AddRect(ImVec2(x, pos.y), ImVec2(x + size, pos.y + size), IM_COL32(255, 255, 255, 80));
        }
        x += size + 3.0f * ui.s;
        ++shown;
    }
    return x - pos.x;
}

std::string SkullSummary(uint64_t mask) {
    int count = 0;
    for (const Skull& s : kSkulls) if (mask & (1ull << s.bit)) ++count;
    if (count == 0) return "None";
    // All thirteen on reads as "All Skulls" (Tyler, 2026-10-30).
    if (count >= kSkullCount) return "All Skulls";
    if (count == 1) {
        for (const Skull& s : kSkulls) if (mask & (1ull << s.bit)) return s.name;
    }
    return std::to_string(count) + " on";
}

float ScoreMultiplier(int difficulty, uint64_t mask) {
    float m = kDifficultyMultiplier[std::clamp(difficulty, 0, 3)];
    for (const Skull& s : kSkulls) {
        if ((mask & (1ull << s.bit)) && s.multiplier > 1.0f) m += s.multiplier;
    }
    return m;
}

struct MainEntry {
    const char* label;
    const char* description;
    Screen target;
    bool enabled;
    bool quit;
};
const MainEntry kMainEntries[] = {
    { "Campaign",     "Play the story of Noble Team, from Winter Contingency to Lone Wolf.", Screen::Campaign, true, false },
    { "Server Browser", "Browse and join online multiplayer, co-op and custom games.",        Screen::Servers, true, false },
    { "Firefight",    "Hold out against waves of Covenant on the map and game type you choose.", Screen::Firefight, true, false },
    { "Custom Game",  "Play any multiplayer map and game type, host a game or join one.",     Screen::CustomGame, true, false },
    { "Forge",        "Edit multiplayer maps with Forge. Experimental in Noble.",             Screen::Forge, true, false },
    { "Theater",      "Saved films aren't supported in Noble yet.",                           Screen::Main, false, false },
    { "Social",       "Manage your friends list, find players, and view game invites.",       Screen::Social, true, false },
    { "Service Record", "Your rank, credits, challenges, commendations and recent games.",   Screen::Career, true, false },
    { "Armory",       "Customize your Spartan and Sangheili armor in Reach's 3D Armory.",     Screen::Armory, true, false },
    { "Account",      "Log in or register your account to sync data and unlock developer access.", Screen::Main, true, false },
    { "Mods",         "Workshop maps and mods.",                                              Screen::Mods, true, false },
    { "Settings",     "Controls, key bindings, audio, video and gameplay options.",          Screen::Settings, true, false },
    { "Quit",         "Close Noble.",                                                         Screen::Main, true, true },
};

// "Continue: <mode> on <map>" (spec 4.1), built into the main-menu list per frame, above Campaign, only when a
// match has been started before. Activating it relaunches that match with the same settings.
const MainEntry kContinueEntry = {
    "Continue", "Start the campaign mission you last played, on the same difficulty and with the same skulls.",
    Screen::Main, true, false
};

// Developer-only row, built into the main-menu list per frame so it sits directly above Quit. The gold
// DEV badge next to the player name is the only other place developer status is shown.
const MainEntry kDeveloperEntry = {
    "Developer Menu", "Unlock armors, grant credits, set rank or reset a career. Developer accounts only.",
    Screen::Main, true, false
};

struct MenuState {
    Screen screen = Screen::Main;
    int mainIndex = 0;
    int mission = 1;
    int difficulty = 1;
    int focusRow = 0;
    int map = kDefaultMap;
    int variant = 0;
    int ffMap = 0;
    int ffVariant = 0;
    int forgeMap = 19;
    int skull = 0;
    uint64_t skulls = 0;
    int campaignSource = 0;
    int ffSource = 0;
    int mpSource = 0;
    int forgeSource = 0;
    std::set<std::string> openMods;
    int campaignModded = 0;
    int ffModded = 0;
    int mpModded = 0;
    int forgeModded = 0;
    int lobbyNetMode = 1;       // 0 = Local (LAN), 1 = Online (P2P)
    int lobbyPrivacy = 0;       // 0 = Public (Master Server), 1 = Private (Direct Connect)
    bool confirmQuit = false;   // "Quit Project Noble?" box on the main menu (Esc or QUIT)
    int quitChoice = 1;         // 0 = Yes, 1 = No (No is preselected)
    bool showAccountModal = false;
    int accountTab = 0;         // 0 = Login, 1 = Register
    char accountUser[32] = "";
    char accountPass[32] = "";
    std::string accountStatusMsg = "";
    bool accountStatusError = false;

    // Pre-game lobby state
    bool isLobbyHost = false;
    std::string lobbySessionId;
    std::string lobbySessionToken;
    std::string lobbyMap = "20_sword_slayer";
    std::filesystem::path lobbyMapOverride; // modded .map served in place of the stock lobbyMap
    std::string lobbyMapTitle;
    std::filesystem::path lobbyMapImage, lobbyMapThumb;
    std::string lobbyMode = "multiplayer";
    std::string lobbyVariantPath;
    std::string lobbyVariantName = "Slayer";
    // Workshop mod this lobby runs. A guest without it sees the Download & Join prompt before joining.
    std::string lobbyModId;
    std::string lobbyModTitle;
    std::filesystem::path lobbyModFolder;
    int lobbyDifficulty = 1;
    uint64_t lobbySkulls = 0;
    int lobbyMaxPlayers = 16;
    int sessionMaxPlayers = 16;    // players the Custom Game / Forge lobby will allow (4..128)
    int ffMaxPlayers = 16;         // players the Firefight lobby will allow (4..16)
    std::string lobbyHostEndpoint;
    std::vector<LobbyPlayer> lobbyRoster;
    bool lobbyLaunchPending = false;
    std::string lobbyLaunchMap;
    std::string lobbyLaunchMode;
    std::string lobbyLaunchVariant;
    int lobbyLaunchDiff = 1;
    uint64_t lobbyLaunchSkulls = 0;
    double lastLobbyPoll = 0.0;
    int lobbyMenuRow = 0;          // Focused row in pre-game options list
    // Playlists. -1 = none chosen.
    std::vector<Playlists::Playlist> playlists;
    int playlistChoice = -1;
    int playlistPosition = 0;
    bool playlistEditOpen = false;
    Playlists::Playlist playlistEdit;
    char playlistName[48] = {};
    int playlistEditRow = 0, playlistEditMap = 0, playlistEditVariant = 0, playlistEditEntry = 0;
    std::string playlistMessage;
    // Voting in the lobby.
    std::string voteId;
    std::vector<std::string> voteOptions;
    std::vector<int> voteCounts;
    int voteMsLeft = 0;
    int voteRow = 0;
    int voteCast = -1;
    int voteRerolls = 0;
    bool voteWasUp = false;
    std::vector<Playlists::Entry> voteEntries;   // host only: what each option plays
    std::string voteMessage;
    // What the lobby's variant is, worked out once per variant: teams on or off, and a player-made mode's changed rules.
    std::string lobbyModeKey;
    bool lobbyTeamsOn = false;
    int lobbyModeChanges = 0;
    std::string lobbyModeSummary;
    bool teamsPushFailed = false;
    double autoStartAt = 0.0;           // ImGui time the next rotation match starts; 0 = not counting
    std::filesystem::path optionsFile;  // the saved mode being edited; empty for a new one
    std::string confirmAction;          // a save-over or delete waiting for its second press
    bool optionsOpen = false;           // Custom Game: the game options editor replaces the setup screen
    int optionsRow = 0;
    char optionsName[48] = {};
    std::string optionsBase;            // base variant file name
    std::vector<uint8_t> optionsDecoded;   // the base variant's real values
    std::map<std::string, int> optionsValues;   // only what differs from the base
    std::string optionsMessage;
    char mapSearch[64] = {};            // the map, mission and mod lists' search text
    int mapSearchRow = 0;
    char variantSearch[64] = {};        // the game type lists' search text
    std::string variantOpenMode;        // the one game type category dropped down
    bool variantPickerOpen = false;     // Custom Game / Firefight: the full game type list replaces the setup screen
    int variantPickerRow = 0;
    bool variantPickerModded = false;
    std::string variantPickerMap;       // the map it filters for and its display name
    std::string variantPickerMapName;
    bool lobbyPickerOpen = false;  // The game type list is showing in place of the option rows
    int lobbyPickerRow = 0;
    bool lobbyProfileOpen = false; // The selected player's card is showing in place of the roster
    bool lobbyReady = true;        // Player ready toggle
    int lobbyRosterSelection = 0;  // Selected player index in party roster
    int lobbyRosterScroll[2] = {}; // First row shown in each roster column (red or all, blue)
    // Social screen state
    int socialTab = 0;              // 0 = Friends, 1 = Requests, 2 = Find Players
    char socialSearchBuf[64] = "";
    std::vector<UserSearchResult> socialSearchResults;
    bool socialSearching = false;
    double lastSocialPoll = 0.0;
    Screen socialPreviousScreen = Screen::Main;
};


static void ClearPendingLobbyEvents();

static void SetupHostLobby(MenuState& st, const std::string& mapScenario, const std::string& mode,
                           const std::string& varPath, const std::string& varName, int diff, uint64_t skulls, int maxPlayers,
                           const ModEntry* mod = nullptr, const std::filesystem::path& mapOverride = {},
                           const std::string& mapTitle = {}, const std::filesystem::path& mapImage = {},
                           const std::filesystem::path& mapThumb = {},
                           const std::string& existingSessionId = {},
                           const std::string& existingSessionToken = {}) {
    st.isLobbyHost = true;
    st.lobbyMap = mapScenario;
    st.lobbyMapOverride = mapOverride;
    st.lobbyMapTitle = mapTitle;
    st.lobbyMapImage = mapImage;
    st.lobbyMapThumb = mapThumb;
    st.lobbyMode = mode;
    st.lobbyVariantPath = varPath;
    st.lobbyVariantName = varName;
    st.lobbyModId = mod ? mod->workshopId : std::string();
    st.lobbyModTitle = mod ? mod->title : std::string();
    st.lobbyModFolder = mod ? mod->folder : std::filesystem::path();
    st.lobbyDifficulty = diff;
    st.lobbySkulls = skulls;
    st.lobbyMaxPlayers = maxPlayers;
    st.lobbyLaunchPending = false;
    st.lobbyMenuRow = 0;
    NOBLE_INFO("FrontEndMenu: host lobby set up: map '{}' mode '{}' game type '{}' file '{}'", mapScenario, mode, varName,
               std::filesystem::path(varPath).filename().string());
    ClearPendingLobbyEvents();
    st.lastLobbyPoll = ImGui::GetTime();

    HostConfig cfg;
    std::string pName = AuthClient::Get().IsLoggedIn() ? AuthClient::Get().GetUsername() : Settings::Get().Snapshot().playerName;
    if (pName.empty()) pName = "Host";
    cfg.sessionName = pName + "'s Lobby";
    cfg.gameMode = st.lobbyMode;
    cfg.mapName = st.lobbyMap;
    cfg.gametypeName = st.lobbyVariantName;
    cfg.modId = st.lobbyModId;
    cfg.modTitle = st.lobbyModTitle;
    cfg.maxPlayers = st.lobbyMaxPlayers;
    cfg.port = 11774;
    cfg.mode = (st.lobbyNetMode == 1) ? NetworkMode::Online_P2P : NetworkMode::Local_LAN;
    SessionManager::Get().StartHosting(cfg);

    if (st.lobbyNetMode == 1 && MasterServerClient::Get().Enabled()) {
        MasterAdvert adv;
        if (!existingSessionId.empty()) {
            adv.id = existingSessionId;
            adv.token = existingSessionToken;
            NOBLE_INFO("FrontEndMenu: Reusing existing session id '{}' for lobby", existingSessionId);
        }
        adv.name = cfg.sessionName;
        adv.port = cfg.port;
        adv.maxPlayers = cfg.maxPlayers;
        adv.map = cfg.mapName;
        adv.mode = cfg.gameMode;
        adv.variant = st.lobbyVariantName;
        adv.difficulty = st.lobbyDifficulty;
        adv.skulls = st.lobbySkulls;
        adv.isMod = mod != nullptr || !st.lobbyModTitle.empty();
        adv.modId = st.lobbyModId;
        adv.modTitle = st.lobbyModTitle;
        adv.inProgress = false;
        adv.isPrivate = (st.lobbyPrivacy != 0);
        adv.hostPeerId = AuthClient::Get().IsLoggedIn() ? static_cast<uint64_t>(AuthClient::Get().GetUserId()) : 1ULL;
        adv.hostMachineId = "0x484F535400000001";
        adv.hostUsername = pName;
        adv.hostIsDev = AuthClient::Get().IsDeveloper();
        adv.hostServiceTag = PlayerServiceTag(Settings::Get().Snapshot());
        // The host's own row identity, so guests can draw it (rank / emblem / nameplate / team).
        {
            const auto& prof = UIManager::GetArmorCustomizer().GetProfile();
            adv.hostRankIndex = Career::RankIndex(Career::Get().credits);
            adv.hostEmblemId = prof.emblemIndex;
            adv.hostEmblemBackgroundId = prof.emblemBackgroundIndex;
            adv.hostNameplateId = prof.nameplateIndex;
            const auto rec = Career::GetRecord();
            int cd = 0;
            for (const auto& c : Career::Challenges()) if (c.done) ++cd;
            int ct = 0, ctm = 0;
            for (const auto& c : Career::Commendations()) {
                if (c.tracked) { ct += c.tier; ctm += 6; }
            }
            const int commPct = ctm > 0 ? (ct * 100 / ctm) : 0;
            adv.hostGamesPlayed = static_cast<int>(rec.totals.gamesPlayed);
            adv.hostKills = static_cast<int>(rec.totals.kills);
            adv.hostDeaths = static_cast<int>(rec.totals.deaths);
            adv.hostChallengesDone = cd;
            adv.hostCommendations = commPct;
            adv.hostArmoryOwned = static_cast<int>(rec.totals.armoryOwned.size());
        }
        MasterServerClient::Get().StartAdvertising(adv);
        st.lobbySessionId = MasterServerClient::Get().GetCurrentSessionId();
        st.lobbySessionToken = MasterServerClient::Get().GetCurrentSessionToken();
    }

    st.lobbyRoster.clear();
    LobbyPlayer me;
    me.peerId = AuthClient::Get().IsLoggedIn() ? static_cast<uint64_t>(AuthClient::Get().GetUserId()) : 1ULL;
    me.machineId = DeviceIdentityStore::Get().deviceId;
    me.username = pName;
    me.serviceTag = PlayerServiceTag(Settings::Get().Snapshot());
    me.isHost = true;
    me.isDev = AuthClient::Get().IsDeveloper();
    me.ping = 0;
    {
        const auto& prof = UIManager::GetArmorCustomizer().GetProfile();
        me.rankIndex = Career::RankIndex(Career::Get().credits);
        me.emblemId = prof.emblemIndex;
        me.emblemBackgroundId = prof.emblemBackgroundIndex;
        me.nameplateId = prof.nameplateIndex;
        const auto rec = Career::GetRecord();
        int cd = 0;
        for (const auto& c : Career::Challenges()) if (c.done) ++cd;
        int ct = 0, ctm = 0;
        for (const auto& c : Career::Commendations()) {
            if (c.tracked) { ct += c.tier; ctm += 6; }
        }
        const int commPct = ctm > 0 ? (ct * 100 / ctm) : 0;
        me.gamesPlayed = static_cast<int>(rec.totals.gamesPlayed);
        me.kills = static_cast<int>(rec.totals.kills);
        me.deaths = static_cast<int>(rec.totals.deaths);
        me.challengesDone = cd;
        me.commendations = commPct;
        me.armoryOwned = static_cast<int>(rec.totals.armoryOwned.size());
    }
    st.lobbyRoster.push_back(me);

    st.screen = Screen::PreGameLobby;
}

// The setup screens (Campaign, Firefight, Custom Game, Forge) all end their right column the same way:
// the two action buttons at the bottom, a strip of network/lobby chips above them, and the banner and
// option rows above that. These three heights come from one place so a screen cannot draw the strip on
// top of the rows above it (the overlap on the 2026-10-30 screenshots).
constexpr float kLobbyStripHeight = 34.0f;   // multiplied by ui.s

float LobbyStripHeight(const Ui& ui) { return kLobbyStripHeight * ui.s; }

struct RightColumnY {
    float actions = 0.0f;   // top of PLAY SOLO / HOST CO-OP (BigAction is 50 * s tall)
    float strip = 0.0f;     // top of the network / lobby chip strip
    float content = 0.0f;   // the option rows must end at or above this
};

RightColumnY RightColumnLayout(const Ui& ui) {
    RightColumnY y;
    const float hints = ui.origin.y + ui.size.y - 58.0f * ui.s;          // DrawHints' rule sits at size.y - 58 * s
    y.actions = hints - 14.0f * ui.s - 50.0f * ui.s;
    y.strip = y.actions - 12.0f * ui.s - LobbyStripHeight(ui);
    y.content = y.strip - 10.0f * ui.s;
    return y;
}

// Map picture height: fills whatever the rows and the strip leave, never taller than the picture's own
// aspect (292/668). Short or very narrow windows shrink it instead of pushing the rows into the strip.
float HeroBannerHeight(const Ui& ui, float columnWidth, float contentBottom, float rowsH, float blurbH, float bannerTop) {
    const float maxH = columnWidth * 292.0f / 668.0f;
    const float minH = std::min(64.0f * ui.s, maxH);
    return std::max(minH, std::min(maxH, contentBottom - rowsH - blurbH - bannerTop));
}

void DrawLobbyControls(const Ui& ui, ImVec2 pos, float width, MenuState& st) {
    const float stripH = LobbyStripHeight(ui);
    const float chipH = 30.0f * ui.s;
    const float chipY = pos.y + (stripH - chipH) * 0.5f;
    const float right = pos.x + width;
    bool online = (st.lobbyNetMode == 1);
    float x = pos.x;

    // Chip width follows its label, so NETWORK/LOBBY text is never clipped or overflows the column.
    auto drawChip = [&](const char* id, const char* label, ImU32 bg, ImU32 bgHover, ImU32 edge) {
        const float w = TextWidth(NobleStyle::SmallFont(), 13.0f * ui.s, label) + 22.0f * ui.s;
        const ImVec2 a(x, chipY);
        const ImVec2 b(a.x + w, a.y + chipH);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(id);
        const bool pressed = ImGui::InvisibleButton("##chip", ImVec2(w, chipH));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        ui.dl->AddRectFilled(a, b, hovered ? bgHover : bg);
        ui.dl->AddRect(a, b, hovered ? IM_COL32(205, 235, 255, 235) : edge, 0.0f, 0, 1.0f);
        Text(ui, NobleStyle::SmallFont(), 13.0f, ImVec2(a.x + 11.0f * ui.s, a.y + 7.0f * ui.s), kWhite, label);
        x = b.x + 8.0f * ui.s;
        if (pressed) MenuSounds::Click();
        return pressed;
    };

    // Network mode: Online P2P (master server + UPnP) or local LAN discovery.
    {
        const ImU32 bg = online ? IM_COL32(28, 80, 128, 215) : IM_COL32(52, 54, 62, 215);
        const ImU32 hov = online ? IM_COL32(40, 110, 170, 235) : IM_COL32(84, 84, 94, 235);
        const ImU32 edge = online ? IM_COL32(120, 170, 215, 110) : IM_COL32(150, 155, 165, 110);
        if (drawChip("btn_lobby_net", online ? "NETWORK: ONLINE" : "NETWORK: LOCAL", bg, hov, edge)) {
            online = !online;
            st.lobbyNetMode = online ? 1 : 0;
        }
    }

    // Lobby privacy only exists online.
    if (online) {
        const bool isPublic = (st.lobbyPrivacy == 0);
        const ImU32 bg = isPublic ? IM_COL32(24, 90, 68, 215) : IM_COL32(80, 58, 28, 215);
        const ImU32 hov = isPublic ? IM_COL32(36, 120, 90, 235) : IM_COL32(110, 80, 40, 235);
        const ImU32 edge = isPublic ? IM_COL32(100, 180, 140, 120) : IM_COL32(190, 150, 90, 120);
        if (drawChip("btn_lobby_priv", isPublic ? "LOBBY: PUBLIC" : "LOBBY: PRIVATE", bg, hov, edge)) {
            st.lobbyPrivacy = isPublic ? 1 : 0;
        }
    }

    // Short connection line, right-aligned in the column. The candidates go from most to least detailed
    // and the first one that fits is drawn; when none fits the line is left out instead of running into
    // the chips or off the screen.
    const bool upnpReady = UPnP::Get().IsActive() && UPnP::Get().GetExternalIP() != "Unknown";
    const bool listed = (st.lobbyPrivacy == 0);
    std::vector<std::string> lines;
    ImU32 dot = IM_COL32(140, 160, 180, 200);
    if (online) {
        const char* host = upnpReady ? "UPnP READY" : "PORT 11774";
        lines.push_back(std::string(host) + " \xC2\xB7 " + (listed ? "LISTED PUBLICLY" : "DIRECT CONNECT"));
        lines.push_back(host);
        lines.push_back(listed ? "LISTED" : "DIRECT");
        dot = upnpReady ? IM_COL32(110, 220, 150, 230) : IM_COL32(235, 190, 90, 230);
    } else {
        lines.push_back("LOCAL NETWORK \xC2\xB7 DISCOVERABLE");
        lines.push_back("LOCAL NETWORK");
        dot = IM_COL32(150, 180, 210, 220);
    }
    const float avail = right - x - 4.0f * ui.s;
    for (const std::string& line : lines) {
        const float w = TextWidth(NobleStyle::SmallFont(), 12.0f * ui.s, line.c_str());
        if (w > avail) continue;
        const float tx = right - w;
        // Backing plate: this line sits straight on the map art, and dim text vanished over bright maps (Forge World).
        ui.dl->AddRectFilled(ImVec2(tx - 17.0f * ui.s, pos.y + 2.0f * ui.s), ImVec2(right + 5.0f * ui.s, pos.y + stripH - 2.0f * ui.s),
                             IM_COL32(8, 14, 22, 190), 2.0f * ui.s);
        ui.dl->AddCircleFilled(ImVec2(tx - 9.0f * ui.s, pos.y + stripH * 0.5f), 3.0f * ui.s, dot);
        Text(ui, NobleStyle::SmallFont(), 12.0f, ImVec2(tx, pos.y + (stripH - 13.0f * ui.s) * 0.5f), IM_COL32(205, 218, 230, 255), line.c_str());
        break;
    }
}

struct MapEntry {
    std::string scenario, name, blurb;
    // A modded map with a new scenario name is loaded through a stock map the engine already knows:
    // launchScenario is that stock scenario (its launch index / map id select the match), and
    // mapOverride is the mod's .map file served in place of the stock maps<launchScenario>.map.
    std::string launchScenario;
    std::filesystem::path mapOverride;
    std::filesystem::path imageThumbnail, imageLarge, imageLoading;
    bool enabled = true;
    const ModEntry* mod = nullptr;
    bool header = false;
    bool custom = false;
};

bool KnownScenario(const std::string& scenario, const MenuMap* maps, int count) {
    for (int i = 0; i < count; ++i) if (scenario == maps[i].scenario) return true;
    return false;
}

// The stock map a modded map with a new scenario is loaded through. The engine picks a map by launch
// index/map id, so the mod map must carry the scenario map id of a stock map; the stock map's file is
// then replaced by the mod's .map through VFS::SetMapOverride. Boneyard (map id 1080) is what the
// campaign-to-Forge conversions and most Forge canvases are built on; Glacier is the Firefight canvas
// and Winter Contingency the campaign base. A mod built on a different base map can be pointed at it by
// adding "<scenario>": "<stock scenario>" to data\map_carriers.json (loaded by CarrierForScenario).
std::string CarrierForScenario(const std::string& scenario, const std::string& fallback) {
    static std::mutex s_mutex;
    static bool s_loaded = false;
    static std::unordered_map<std::string, std::string> s_carriers;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_loaded) {
        s_loaded = true;
        const std::filesystem::path path = VFS::GetUserStoragePath() / "map_carriers.json";
        std::ifstream file(path, std::ios::binary);
        if (file.is_open()) {
            try {
                const nlohmann::json j = nlohmann::json::parse(file);
                if (j.is_object()) {
                    for (const auto& item : j.items()) {
                        if (item.value().is_string()) {
                            std::string key = item.key();
                            for (char& c : key) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
                            s_carriers[key] = item.value().get<std::string>();
                        }
                    }
                }
                NOBLE_INFO("FrontEndMenu: {} map carrier override(s) from '{}'", s_carriers.size(), path.string());
            } catch (const std::exception& e) {
                NOBLE_WARN("FrontEndMenu: '{}' is not valid map carrier JSON ({})", path.string(), e.what());
            }
        }
    }
    std::string key = scenario;
    for (char& c : key) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    const auto it = s_carriers.find(key);
    return it != s_carriers.end() ? it->second : fallback;
}

std::vector<MapEntry> ModMaps(const ModEntry& mod, const std::vector<ModMap>& listed, const MenuMap* base,
                              int baseCount, const std::string& carrier) {
    std::vector<MapEntry> out;
    for (const ModMap& m : listed) {
        const bool known = KnownScenario(m.scenario, base, baseCount);
        if (known) {
            out.push_back({ m.scenario, m.title, PlainDescription(m.description, 150), {}, {}, {}, {}, {}, true, &mod });
            continue;
        }
        const std::filesystem::path mapFile = mod.folder / "maps" / (m.scenario + ".map");
        if (!m.hasMapFile) {
            out.push_back({ m.scenario, m.title,
                            "This map needs maps\\" + m.scenario + ".map, which the mod does not contain.",
                            {}, {}, {}, {}, {}, false, &mod, false, true });
            continue;
        }
        MapEntry entry;
        entry.scenario = m.scenario;
        entry.name = m.title;
        entry.launchScenario = CarrierForScenario(m.scenario, carrier);
        entry.mapOverride = mapFile;
        entry.imageThumbnail = m.imageThumbnail;
        entry.imageLarge = m.imageLarge.empty() ? m.imageThumbnail : m.imageLarge;
        entry.imageLoading = m.imageLoading.empty() ? m.imageLarge : m.imageLoading;
        entry.enabled = true;
        entry.mod = &mod;
        entry.custom = true;
        entry.blurb = "Custom map \xC2\xB7 loads through " + MapNames::Display(entry.launchScenario) + ".";
        out.push_back(std::move(entry));
    }
    for (const std::string& file : mod.mapFiles) {
        if (!KnownScenario(file, base, baseCount)) {
            if (std::any_of(out.begin(), out.end(), [&](const MapEntry& e) { return e.scenario == file; })) continue;
            const std::filesystem::path mapFile = mod.folder / "maps" / (file + ".map");
            MapEntry entry;
            entry.scenario = file;
            entry.name = MapNames::Display(file);
            entry.launchScenario = CarrierForScenario(file, carrier);
            entry.mapOverride = mapFile;
            entry.enabled = true;
            entry.mod = &mod;
            entry.custom = true;
            entry.blurb = "Custom map \xC2\xB7 loads on " + MapNames::Display(entry.launchScenario);
            out.push_back(std::move(entry));
            continue;
        }
        if (std::any_of(out.begin(), out.end(), [&](const MapEntry& e) { return e.scenario == file; })) continue;
        for (int i = 0; i < baseCount; ++i) {
            if (file == base[i].scenario) out.push_back({ file, base[i].name, base[i].blurb, {}, {}, {}, {}, {}, true, &mod });
        }
    }
    return out;
}

std::vector<MapEntry> BaseMaps(const MenuMap* maps, int count) {
    std::vector<MapEntry> out;
    for (int i = 0; i < count; ++i) out.push_back({ maps[i].scenario, maps[i].name, maps[i].blurb, {}, {}, {}, {}, {}, true });
    return out;
}

enum class ModMode { Campaign, Firefight, Multiplayer };
std::string OpenKey(ModMode mode, const ModEntry& mod) { return std::to_string(static_cast<int>(mode)) + ":" + mod.id; }

std::vector<MapEntry> ModdedMaps(const std::vector<const ModEntry*>& mods, ModMode mode, const MenuMap* base, int baseCount,
                                 const std::set<std::string>& open, const std::string& carrier) {
    std::vector<MapEntry> out;
    for (const ModEntry* mod : mods) {
        const std::vector<ModMap>& listed = mode == ModMode::Campaign ? mod->campaign
                                          : mode == ModMode::Firefight ? mod->firefight : mod->multiplayer;
        std::vector<MapEntry> maps = ModMaps(*mod, listed, base, baseCount, carrier);
        if (maps.empty()) continue;
        MapEntry heading;
        heading.name = mod->title + "  (" + std::to_string(maps.size()) + ")";
        heading.header = true;
        heading.mod = mod;
        // A heading has no map of its own; the banner shows the mod's first map picture instead of black.
        heading.scenario = maps.front().scenario;
        heading.imageLarge = maps.front().imageLarge;
        heading.imageThumbnail = maps.front().imageThumbnail;
        heading.blurb = PlainDescription(mod->description, 180);
        heading.enabled = open.count(OpenKey(mode, *mod)) != 0;
        out.push_back(heading);
        if (!heading.enabled) continue;
        for (MapEntry& m : maps) out.push_back(std::move(m));
    }
    return out;
}

void ToggleMod(std::set<std::string>& open, ModMode mode, const ModEntry& mod) {
    const std::string k = OpenKey(mode, mod);
    if (!open.erase(k)) open.insert(k);
}

bool ContainsNoCase(std::string text, std::string needle) {
    const auto lower = [](std::string& s) { for (char& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c))); };
    lower(text);
    lower(needle);
    return text.find(needle) != std::string::npos;
}

// One search box for every list. Returns true when the text changed. Typing needs a keyboard.
bool DrawSearchBox(const Ui& ui, const char* id, ImVec2 pos, float width, char* text, size_t size, const char* hint) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::SetNextItemWidth(width);
    (void)ui;
    return ImGui::InputTextWithHint(id, hint, text, size);
}

// The list to show: normally `ModdedMaps` or the stock maps; while searching, every matching map with no mod headings.
std::vector<MapEntry> ListedMaps(bool modded, const std::vector<const ModEntry*>& mods, ModMode mode, const MenuMap* base, int baseCount,
                                 const std::set<std::string>& open, const std::string& carrier, const char* query) {
    std::set<std::string> everyMod = open;
    if (query[0] != 0) for (const ModEntry* mod : mods) everyMod.insert(OpenKey(mode, *mod));
    std::vector<MapEntry> all = modded ? ModdedMaps(mods, mode, base, baseCount, everyMod, carrier) : BaseMaps(base, baseCount);
    if (query[0] == 0) return all;
    std::vector<MapEntry> hits;
    for (MapEntry& m : all) {
        if (m.header) continue;
        if (ContainsNoCase(m.name, query) || (m.mod && ContainsNoCase(m.mod->title, query))) hits.push_back(std::move(m));
    }
    return hits;
}

void FixSelection(const std::vector<MapEntry>& list, int& selected) {
    selected = list.empty() ? 0 : std::clamp(selected, 0, static_cast<int>(list.size()) - 1);
}

std::vector<ListItem> ToItems(const std::vector<MapEntry>& list) {
    std::vector<ListItem> items;
    for (const MapEntry& m : list) {
        if (m.header) {
            items.push_back({ m.name, true, true, m.enabled });
        } else {
            ListItem it{ m.name, m.enabled, false, false };
            if (m.imageThumbnail.empty()) it.thumb = m.scenario;
            else it.image = m.imageThumbnail;
            it.number = static_cast<int>(std::count_if(items.begin(), items.end(), [](const ListItem& x) { return !x.header; })) + 1;
            items.push_back(it);
        }
    }
    return items;
}


Screen ParseScreenArg(const std::wstring& v) {
    const std::wstring s = Lower(v);
    if (s == L"campaign") return Screen::Campaign;
    if (s == L"skulls") return Screen::Skulls;
    if (s == L"firefight") return Screen::Firefight;
    if (s == L"custom") return Screen::CustomGame;
    if (s == L"forge") return Screen::Forge;
    if (s == L"servers") return Screen::Servers;
    if (s == L"settings") return Screen::Settings;
    if (s == L"armory") return Screen::Armory;
    if (s == L"mods") return Screen::Mods;
    if (s == L"career") return Screen::Career;
    if (s == L"lobby" || s == L"pregame") return Screen::PreGameLobby;
    if (s == L"social" || s == L"friends" || s == L"community") return Screen::Social;
    return Screen::Main;
}

std::wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring w(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), len);
    return w;
}

// Persistent state for in-engine overlay mode
struct OverlayContext {
    bool initialized = false;
    std::filesystem::path reachDir;
    std::vector<MenuVariant> variants;
    std::vector<MenuVariant> ffVariants;
    std::filesystem::path forgeVariant;
    std::vector<ModEntry> mods;
    std::vector<const ModEntry*> campaignMods;
    std::vector<const ModEntry*> ffMods;
    std::vector<const ModEntry*> mpMods;
    MenuState st;
    SettingsPanel settingsPanel;
    CareerPanel careerPanel;
    bool modsScanned = false;
    unsigned long long captureFrame = 0;
    unsigned long long totalFrames = 0;
    unsigned long long sceneReadyFrames = 0;
    bool closed = false;
};

// The engine draws the menu scene and calls RenderOverlay from slot 1. MCC ships no Reach menu map,
// so about a minute after start the engine gives up on it ("_map_state_none") and stops calling
// slot 1; HoldFrame then draws the menu from the main thread so it keeps working.
std::mutex s_overlayMutex;
std::atomic<IDXGISwapChain*> s_overlaySwapChain{nullptr};
std::atomic<ULONGLONG> s_lastEngineOverlayFrame{0};

OverlayContext& GetOverlayCtx() {
    static OverlayContext s_ctx;
    return s_ctx;
}

// Rescans Workshop + data\mods and rebuilds the per-mode lists the setup screens use. Called at menu start
// and again after the Download & Join prompt installs the lobby's mod.
static void RefreshModCatalog(OverlayContext& ctx) {
    ctx.mods = ModCatalog::Scan(VFS::GetWorkshopPath(), VFS::GetUserStoragePath() / "mods");
    ctx.campaignMods.clear();
    ctx.ffMods.clear();
    ctx.mpMods.clear();
    for (const ModEntry& m : ctx.mods) {
        auto replaces = [&](const MenuMap* base, int count) {
            return std::any_of(m.mapFiles.begin(), m.mapFiles.end(),
                               [&](const std::string& f) { return KnownScenario(f, base, count); });
        };
        if (!m.campaign.empty() || replaces(kMissions, Count(kMissions))) ctx.campaignMods.push_back(&m);
        if (!m.firefight.empty() || replaces(kFirefightMaps, Count(kFirefightMaps))) ctx.ffMods.push_back(&m);
        if (!m.multiplayer.empty() || replaces(kMaps, Count(kMaps))) ctx.mpMods.push_back(&m);
    }
}

// "-auto-play": the capture harness cannot press PLAY LOCAL / HOST LOBBY, so a test run can ask the setup screens
// to start the selected match by themselves after a couple of seconds. Test-only, no settings involved.
bool AutoPlayRequested() {
    static const bool requested = [] {
        bool found = false;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i < argc; ++i) {
            if (_wcsicmp(argv[i], L"-auto-play") == 0) found = true;
        }
        if (argv) LocalFree(argv);
        return found;
    }();
    return requested;
}

bool AutoPlayNow() {
    if (!AutoPlayRequested()) return false;
    // Time, not frames: the menu can draw at 2 fps while the engine loads, which made a frame count take 45 s.
    static ULONGLONG firstDraw = 0;
    if (firstDraw == 0) firstDraw = GetTickCount64();
    return GetTickCount64() - firstDraw > 3000;
}

// "-auto-play-map <scenario>": select that map, so a test run can walk every map.
std::string AutoPlayMap() {
    static const std::string scenario = [] {
        std::string out;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i + 1 < argc; ++i) {
            if (_wcsicmp(argv[i], L"-auto-play-map") == 0) {
                for (const wchar_t* c = argv[i + 1]; c && *c; ++c) out.push_back(static_cast<char>(*c));
            }
        }
        if (argv) LocalFree(argv);
        return out;
    }();
    return scenario;
}

// "-auto-play-variant <text>": pick the first variant whose file stem contains <text> (e.g. "ctf", "3nvasion").
std::string AutoPlayVariantStem() {
    static const std::string stem = [] {
        std::string out;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i + 1 < argc; ++i) {
            if (_wcsicmp(argv[i], L"-auto-play-variant") == 0) {
                for (const wchar_t* c = argv[i + 1]; c && *c; ++c) out.push_back(static_cast<char>(*c));
            }
        }
        if (argv) LocalFree(argv);
        return out;
    }();
    return stem;
}

void UpdateSceneForScreen(Screen screen) {
    switch (screen) {
    case Screen::Main:
        ReachArmory::SetTargetScene("start_menu");
        break;
    case Screen::Campaign:
    case Screen::Skulls:
        ReachArmory::SetTargetScene("campaign");
        break;
    case Screen::Firefight:
        ReachArmory::SetTargetScene("firefight");
        break;
    case Screen::CustomGame:
    case Screen::Servers:
        ReachArmory::SetTargetScene("custom_games");
        break;
    case Screen::Forge:
        ReachArmory::SetTargetScene("forge");
        break;
    case Screen::Armory:
        UIManager::GetArmorCustomizer().UpdateArmoryCamera();
        break;
    default:
        // Settings, Service Record, Mods, ...: the main menu's scene, never the Armory's.
        ReachArmory::SetTargetScene("start_menu");
        break;
    }
}

void FeedEngineThreadInputs(HWND hwnd, ImGuiIO& io) {
    // Test-only: "-menu-ignore-input" freezes the menu for screenshots. The window still renders, but
    // the mouse/keyboard cannot move the selection or cycle the Armory camera while a capture is taken.
    static const bool s_ignoreInput = wcsstr(GetCommandLineW(), L"-menu-ignore-input") != nullptr;
    if (s_ignoreInput) {
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        io.AddMouseButtonEvent(0, false);
        io.AddMouseButtonEvent(1, false);
        return;
    }
    RECT client{};
    GetClientRect(hwnd, &client);
    POINT cursor{};
    const bool focused = (GetForegroundWindow() == hwnd);
    if (focused && GetCursorPos(&cursor) && ScreenToClient(hwnd, &cursor) && client.right > 0 && client.bottom > 0) {
        const float sx = io.DisplaySize.x / static_cast<float>(client.right);
        const float sy = io.DisplaySize.y / static_cast<float>(client.bottom);
        io.AddMousePosEvent(static_cast<float>(cursor.x) * sx, static_cast<float>(cursor.y) * sy);

        const bool lDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (Window::TakeLeftClicks() > 0 && !lDown) {
            io.AddMouseButtonEvent(0, true);
            io.AddMouseButtonEvent(0, false);
        } else {
            io.AddMouseButtonEvent(0, lDown);
        }
        const bool rDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        io.AddMouseButtonEvent(1, rDown);

        const int wheel = Window::TakeWheelNotches();
        if (wheel != 0) {
            io.AddMouseWheelEvent(0.0f, static_cast<float>(wheel));
        }
    } else {
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        io.AddMouseButtonEvent(0, false);
        io.AddMouseButtonEvent(1, false);
    }

    struct KeyMap {
        int vk;
        ImGuiKey key;
        char ch;
    };
    static const KeyMap kKeys[] = {
        { VK_UP,     ImGuiKey_UpArrow,     0 },
        { VK_DOWN,   ImGuiKey_DownArrow,   0 },
        { VK_LEFT,   ImGuiKey_LeftArrow,   0 },
        { VK_RIGHT,  ImGuiKey_RightArrow,  0 },
        { VK_RETURN, ImGuiKey_Enter,       0 },
        { VK_ESCAPE, ImGuiKey_Escape,      0 },
        { VK_TAB,    ImGuiKey_Tab,         0 },
        { VK_SPACE,  ImGuiKey_Space,       ' ' },
        { VK_BACK,   ImGuiKey_Backspace,   0 },
        { VK_DELETE, ImGuiKey_Delete,      0 },
        { 'Q',       ImGuiKey_Q,           'Q' },
        { 'E',       ImGuiKey_E,           'E' },
        { 'A',       ImGuiKey_A,           'A' },
        { 'B',       ImGuiKey_B,           'B' },
        { 'C',       ImGuiKey_C,           'C' },
        { 'D',       ImGuiKey_D,           'D' },
        { 'F',       ImGuiKey_F,           'F' },
        { 'G',       ImGuiKey_G,           'G' },
        { 'H',       ImGuiKey_H,           'H' },
        { 'I',       ImGuiKey_I,           'I' },
        { 'J',       ImGuiKey_J,           'J' },
        { 'K',       ImGuiKey_K,           'K' },
        { 'L',       ImGuiKey_L,           'L' },
        { 'M',       ImGuiKey_M,           'M' },
        { 'N',       ImGuiKey_N,           'N' },
        { 'O',       ImGuiKey_O,           'O' },
        { 'P',       ImGuiKey_P,           'P' },
        { 'R',       ImGuiKey_R,           'R' },
        { 'S',       ImGuiKey_S,           'S' },
        { 'T',       ImGuiKey_T,           'T' },
        { 'U',       ImGuiKey_U,           'U' },
        { 'V',       ImGuiKey_V,           'V' },
        { 'W',       ImGuiKey_W,           'W' },
        { 'X',       ImGuiKey_X,           'X' },
        { 'Y',       ImGuiKey_Y,           'Y' },
        { 'Z',       ImGuiKey_Z,           'Z' },
        { '0',       ImGuiKey_0,           '0' },
        { '1',       ImGuiKey_1,           '1' },
        { '2',       ImGuiKey_2,           '2' },
        { '3',       ImGuiKey_3,           '3' },
        { '4',       ImGuiKey_4,           '4' },
        { '5',       ImGuiKey_5,           '5' },
        { '6',       ImGuiKey_6,           '6' },
        { '7',       ImGuiKey_7,           '7' },
        { '8',       ImGuiKey_8,           '8' },
        { '9',       ImGuiKey_9,           '9' },
    };
    static bool s_prevDown[sizeof(kKeys) / sizeof(kKeys[0])] = {};
    for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); ++i) {
        const bool down = focused && ((GetAsyncKeyState(kKeys[i].vk) & 0x8000) != 0);
        if (down != s_prevDown[i]) {
            io.AddKeyEvent(kKeys[i].key, down);
            s_prevDown[i] = down;
        }
    }
    InputPrompts::Update();

}


// Lobby events from the WebSocket relay thread. The callbacks used to write MenuState's strings and roster on that
// thread while the menu read them on its own; two launch events 0.1 s apart (2026-10-29, joining O7OF) then killed
// the menu without a crash log (heap corruption ends the process before the crash handler runs). The relay
// thread now only fills this, and the menu takes it over at the top of the lobby screen.
struct PendingLobbyEvents {
    bool launch = false;
    std::string map, mode, variant, hostEndpoint, sessionId, sessionToken;
    int difficulty = 1;
    uint64_t skulls = 0;
    bool roster = false;
    std::vector<LobbyPlayer> rosterPlayers;
    bool closed = false;
    std::string closeReason;
};
static std::mutex s_lobbyEventMutex;
static PendingLobbyEvents s_lobbyEvents;

static void ClearPendingLobbyEvents() {
    std::lock_guard<std::mutex> lock(s_lobbyEventMutex);
    s_lobbyEvents = PendingLobbyEvents{};
}

static void TakePendingLobbyEvents(MenuState& st) {
    PendingLobbyEvents ev;
    {
        std::lock_guard<std::mutex> lock(s_lobbyEventMutex);
        if (!s_lobbyEvents.launch && !s_lobbyEvents.roster && !s_lobbyEvents.closed) return;
        ev = std::move(s_lobbyEvents);
        s_lobbyEvents = PendingLobbyEvents{};
    }
    if (ev.closed) {
        st.screen = Screen::Servers;
        st.lobbySessionId.clear();
        st.lobbySessionToken.clear();
        st.lobbyRoster.clear();
        st.isLobbyHost = false;
        NetworkTransport::Get().StopRelay();
        return;
    }
    if (ev.roster) {
        st.lobbyRoster = std::move(ev.rosterPlayers);
        if (st.isLobbyHost) {
            std::string pName = AuthClient::Get().IsLoggedIn() ? AuthClient::Get().GetUsername() : Settings::Get().Snapshot().playerName;
            if (pName.empty()) pName = "Host";
            const uint64_t hostPId = AuthClient::Get().IsLoggedIn() ? static_cast<uint64_t>(AuthClient::Get().GetUserId()) : 1ULL;
            const std::string hostTag = Settings::Get().Snapshot().serviceTag.empty() ? "117" : Settings::Get().Snapshot().serviceTag;
            const bool hostDev = AuthClient::Get().IsDeveloper();
            for (auto& p : st.lobbyRoster) {
                if (p.isHost) {
                    p.peerId = hostPId;
                    p.username = pName;
                    p.serviceTag = hostTag;
                    p.isDev = hostDev;
                }
            }
        }
    }
    if (ev.launch && !st.lobbyLaunchPending && !st.isLobbyHost) {
        NOBLE_INFO("Lobby launch received: map='{}' mode='{}' variant='{}' endpoint='{}' session='{}'",
                   ev.map, ev.mode, ev.variant, ev.hostEndpoint, ev.sessionId);
        st.lobbyLaunchPending = true;
        st.lobbyLaunchMap = ev.map;
        st.lobbyLaunchMode = ev.mode;
        st.lobbyLaunchVariant = ev.variant;
        st.lobbyLaunchDiff = ev.difficulty;
        st.lobbyLaunchSkulls = ev.skulls;
        if (st.lobbyHostEndpoint.empty() && !ev.hostEndpoint.empty()) st.lobbyHostEndpoint = ev.hostEndpoint;
        if (!ev.sessionId.empty()) st.lobbySessionId = ev.sessionId;
        if (!ev.sessionToken.empty()) st.lobbySessionToken = ev.sessionToken;
        else if (st.lobbySessionToken.empty()) st.lobbySessionToken = MasterServerClient::Get().GetCurrentSessionToken();
    }
}

// A line about each game type for the lobby's list. Only what the type itself is: a variant's own settings can't be read yet.
static const char* GameTypeBlurb(const std::string& type) {
    if (type == "slayer") return "Score points by killing the other players.";
    if (type == "ctf") return "Take the enemy flag back to your own base.";
    if (type == "assault") return "Carry the bomb into the enemy base and arm it.";
    if (type == "territories") return "Capture and hold the areas of the map.";
    if (type == "koth") return "Hold the hill to score.";
    if (type == "oddball") return "Hold the skull for as long as you can.";
    if (type == "juggernaut") return "Kill the Juggernaut to become the Juggernaut.";
    if (type == "infection") return "The infected hunt the survivors, and every kill turns another.";
    if (type == "headhunter") return "Collect skulls from kills and bank them at the drop points.";
    if (type == "stockpile") return "Gather flags and hold them at your base when the timer runs out.";
    if (type == "invasion") return "Spartans against Elites, fought in phases across the map.";
    if (type == "race") return "Drive through the checkpoints.";
    if (type == "rally") return "Reach the rally points before the others.";
    if (type == "firefight") return "Fight wave after wave of Covenant together.";
    return "A variant with its own rules.";
}

// The game types a map can host, grouped by mode. A heading drops down its variants, one mode at a time. Searching
// shows every match as a plain list, and a single mode (Firefight) needs no headings.
struct VariantRow {
    const MenuVariant* variant = nullptr;   // nullptr on a heading
    std::string mode;
    std::string type;
};
struct VariantChoices {
    std::vector<VariantRow> rows;
    std::vector<ListItem> items;
};
static std::string VariantMode(const MenuVariant& v) { return v.custom ? std::string("Custom Game Modes") : v.type.empty() ? std::string("Other") : std::string(MapGameTypes::Name(v.type)); }

static VariantChoices BuildVariantChoices(const std::vector<MenuVariant>& list, const std::string& scenario, bool unrestricted,
                                          const std::string& openMode, const char* query) {
    std::vector<const MenuVariant*> offered;
    for (const MenuVariant& v : list) {
        if (unrestricted || v.type.empty() || MapGameTypes::Supports(scenario, v.type)) offered.push_back(&v);
    }
    std::stable_sort(offered.begin(), offered.end(), [](const MenuVariant* a, const MenuVariant* b) {
        const std::string ma = VariantMode(*a), mb = VariantMode(*b);
        return ma != mb ? ma < mb : a->label < b->label;
    });
    VariantChoices out;
    const bool searching = query[0] != 0;
    const bool oneMode = !offered.empty() && VariantMode(*offered.front()) == VariantMode(*offered.back());
    std::string heading;
    for (const MenuVariant* v : offered) {
        const std::string mode = VariantMode(*v);
        if (searching) {
            if (!ContainsNoCase(v->label, query) && !ContainsNoCase(mode, query)) continue;
        } else if (!oneMode && mode != heading) {
            heading = mode;
            const auto count = std::count_if(offered.begin(), offered.end(), [&](const MenuVariant* o) { return VariantMode(*o) == mode; });
            out.rows.push_back({ nullptr, mode, v->type });
            out.items.push_back({ mode + "  (" + std::to_string(count) + ")", true, true, mode == openMode });
        }
        if (searching || oneMode || mode == openMode) {
            out.rows.push_back({ v, mode, v->type });
            out.items.push_back({ v->label });
        }
    }
    return out;
}

// Opens a game type list on the current variant: its mode dropped down and its row selected.
static void BeginVariantList(MenuState& st, const std::vector<MenuVariant>& list, const std::string& scenario, bool unrestricted, const MenuVariant* current) {
    st.variantSearch[0] = 0;
    st.variantOpenMode = current ? VariantMode(*current) : std::string();
    st.variantPickerRow = 0;
    const VariantChoices choices = BuildVariantChoices(list, scenario, unrestricted, st.variantOpenMode, st.variantSearch);
    for (size_t i = 0; i < choices.rows.size(); ++i) {
        if (choices.rows[i].variant == current) st.variantPickerRow = static_cast<int>(i);
    }
}

// Search box and list. A heading toggles; returns the variant chosen, if any. `shown` gets the selected row.
static const MenuVariant* DrawVariantList(const Ui& ui, MenuState& st, const std::vector<MenuVariant>& list, const std::string& scenario, bool unrestricted,
                                          ImVec2 pos, float width, float rowHeight, int rowCount, bool keyboard, VariantRow& shown) {
    if (DrawSearchBox(ui, "##variant_search", pos, width, st.variantSearch, sizeof(st.variantSearch), "Search game types")) st.variantPickerRow = 0;
    const ImVec2 listPos(pos.x, pos.y + 40.0f * ui.s);
    VariantChoices choices = BuildVariantChoices(list, scenario, unrestricted, st.variantOpenMode, st.variantSearch);
    if (choices.items.empty()) choices.items.push_back({ "No game type matches", false, false });
    const int hit = DrawList(ui, "variant_list", listPos, width, choices.items, st.variantPickerRow, keyboard && !ImGui::GetIO().WantTextInput, rowHeight, rowCount, true);
    const MenuVariant* picked = nullptr;
    if (hit >= 0 && hit < static_cast<int>(choices.rows.size())) {
        if (choices.rows[hit].variant) {
            picked = choices.rows[hit].variant;
        } else {
            // Rows shift when another mode closes, so the selection follows the heading that was pressed.
            const std::string mode = choices.rows[hit].mode;
            st.variantOpenMode = st.variantOpenMode == mode ? std::string() : mode;
            choices = BuildVariantChoices(list, scenario, unrestricted, st.variantOpenMode, st.variantSearch);
            for (size_t i = 0; i < choices.rows.size(); ++i) {
                if (!choices.rows[i].variant && choices.rows[i].mode == mode) st.variantPickerRow = static_cast<int>(i);
            }
            MenuSounds::Click();
        }
    }
    shown = choices.rows.empty() ? VariantRow{} : choices.rows[std::clamp(st.variantPickerRow, 0, static_cast<int>(choices.rows.size()) - 1)];
    return picked;
}

// Name and description of a list row; the file name and a note about rules too, unless `compact` (the lobby's tight column).
static void DrawVariantInfo(const Ui& ui, ImVec2 pos, float width, const VariantRow& row, bool compact = false) {
    if (row.mode.empty()) return;
    const float right = pos.x + width;
    const MenuVariant* variant = row.variant;
    Text(ui, NobleStyle::TitleFont(), 24.0f, pos, kWhite, Upper(variant ? variant->label : row.mode).c_str());
    const char* description = variant && !variant->description.empty() ? variant->description.c_str() : GameTypeBlurb(row.type);
    Text(ui, nullptr, 16.0f, ImVec2(pos.x, pos.y + 34.0f * ui.s), IM_COL32(205, 218, 230, 255), description, right);
    if (compact || !variant) return;
    const MenuVariant& v = *variant;
    const float textH = Font(nullptr)->CalcTextSizeA(16.0f * ui.s, FLT_MAX, width, description).y;
    const float y = pos.y + 34.0f * ui.s + textH + 10.0f * ui.s;
    Text(ui, NobleStyle::SmallFont(), 14.0f, ImVec2(pos.x, y), kDim, v.path.filename().string().c_str(), right);
    Text(ui, NobleStyle::SmallFont(), 14.0f, ImVec2(pos.x, y + 20.0f * ui.s), kDim, "Uses this variant's default rules.", right);
}

static std::string RuleValueText(const VariantRules::Rule& rule, int value) {
    const int nameIndex = value - rule.minValue;
    if (rule.names && nameIndex >= 0 && nameIndex < static_cast<int>(rule.names->size())) return (*rule.names)[nameIndex];
    if (rule.bit >= 0) return value ? "On" : "Off";
    return std::to_string(value) + rule.unit;
}

// Reads the lobby's variant when it changes. The Teams flag is in the decoded record, or in a player-made mode's rules.
static void RefreshLobbyMode(OverlayContext& ctx) {
    MenuState& st = ctx.st;
    if (st.lobbyModeKey == st.lobbyVariantPath && !st.lobbyVariantPath.empty()) return;
    st.lobbyModeKey = st.lobbyVariantPath;
    st.lobbyTeamsOn = false;
    st.lobbyModeChanges = 0;
    st.lobbyModeSummary.clear();
    if (st.lobbyVariantPath.empty()) return;
    const std::filesystem::path path(st.lobbyVariantPath);
    VariantRules::Preset preset;
    const bool isMode = VariantRules::IsPresetFile(path) && VariantRules::Load(path, preset);
    const std::filesystem::path base = isMode ? ctx.reachDir / "game_variants" / preset.base : path;
    std::vector<uint8_t> decoded;
    if (!VariantRules::Decode(base, decoded)) return;
    if (isMode) VariantRules::Apply(preset, decoded.data());
    for (const VariantRules::Rule& rule : VariantRules::Rules()) {
        if (std::strcmp(rule.key, "teams") == 0) st.lobbyTeamsOn = VariantRules::Read(rule, decoded.data()) != 0;
        const auto it = preset.values.find(rule.key);
        if (isMode && it != preset.values.end()) {
            ++st.lobbyModeChanges;
            if (st.lobbyModeChanges <= 5) st.lobbyModeSummary += std::string(st.lobbyModeSummary.empty() ? "" : ", ") + rule.label + " " + RuleValueText(rule, it->second);
        }
    }
    if (st.lobbyModeChanges > 5) st.lobbyModeSummary += " and " + std::to_string(st.lobbyModeChanges - 5) + " more";
    NOBLE_INFO("FrontEndMenu: lobby game type: teams {}, {} rule(s) changed", st.lobbyTeamsOn ? "on" : "off", st.lobbyModeChanges);
}

// Host only: everyone on a team while the variant has teams, nobody when it doesn't. Players are dealt in roster order, so a
// joining player doesn't reshuffle anyone. Empty when the roster already matches.
static void AssignLobbyTeams(MenuState& st) {
    std::vector<std::pair<uint64_t, int>> changes;
    for (size_t i = 0; i < st.lobbyRoster.size(); ++i) {
        const int want = st.lobbyTeamsOn ? static_cast<int>(i % 2) + 1 : 0;
        if (st.lobbyRoster[i].team != want) {
            st.lobbyRoster[i].team = want;
            changes.emplace_back(st.lobbyRoster[i].peerId, want);
            if (st.lobbyRoster[i].isHost) MasterServerClient::Get().SetAdvertHostTeam(want);
        }
    }
    if (changes.empty() || st.teamsPushFailed) return;
    if (MasterServerClient::Get().SetTeamsSync(st.lobbySessionId, changes)) {
        NOBLE_INFO("FrontEndMenu: teams updated for {} player(s), teams {}", changes.size(), st.lobbyTeamsOn ? "on" : "off");
    } else {
        // An older server: the host still sees teams, and this stops asking every poll.
        st.teamsPushFailed = true;
        NOBLE_WARN("FrontEndMenu: the server didn't accept the team list");
    }
}

// Opens the game options editor on a variant; Custom Game and Firefight share it.
static void OpenGameOptions(OverlayContext& ctx, const MenuVariant& current) {
    MenuState& st = ctx.st;
    VariantRules::Preset preset;
    const bool isMode = current.custom && VariantRules::Load(current.path, preset);
    st.optionsBase = isMode ? preset.base : current.path.filename().string();
    st.optionsValues = isMode ? preset.values : std::map<std::string, int>{};
    std::snprintf(st.optionsName, sizeof(st.optionsName), "%s", (isMode ? current.label : current.label + " Custom").c_str());
    st.optionsFile = isMode ? current.path : std::filesystem::path();
    st.optionsRow = 0;
    st.optionsMessage.clear();
    st.confirmAction.clear();
    if (VariantRules::Decode(ctx.reachDir / "game_variants" / st.optionsBase, st.optionsDecoded)) {
        NOBLE_INFO("FrontEndMenu: game options opened on '{}'", st.optionsBase);
        st.optionsOpen = true;
        MenuSounds::Click();
    } else {
        NOBLE_WARN("FrontEndMenu: could not read variant '{}' for its game options", st.optionsBase);
        MenuSounds::Error();
    }
}

// ---- Playlists ----
static void RefreshPlaylists(OverlayContext& ctx) {
    std::vector<std::filesystem::path> folders;
    for (const ModEntry& mod : ctx.mods) folders.push_back(mod.folder);
    ctx.st.playlists = Playlists::Scan(folders);
}

// Every multiplayer map a playlist can name: the stock ones, then each mod's.
static std::vector<MapEntry> AllMultiplayerMaps(OverlayContext& ctx) {
    std::vector<MapEntry> maps = BaseMaps(kMaps, Count(kMaps));
    std::set<std::string> everyMod;
    for (const ModEntry* mod : ctx.mpMods) everyMod.insert(OpenKey(ModMode::Multiplayer, *mod));
    for (MapEntry& m : ModdedMaps(ctx.mpMods, ModMode::Multiplayer, kMaps, Count(kMaps), everyMod, "70_boneyard")) {
        if (!m.header && m.enabled) maps.push_back(std::move(m));
    }
    return maps;
}

// A playlist entry turned into what the lobby needs. False when this PC lacks the map.
struct ResolvedEntry {
    MapEntry map;
    const MenuVariant* variant = nullptr;
};
static bool ResolveEntry(OverlayContext& ctx, const Playlists::Entry& entry, bool randomType, ResolvedEntry& out) {
    bool found = false;
    for (MapEntry& m : AllMultiplayerMaps(ctx)) {
        const std::string mod = m.mod ? m.mod->folder.filename().string() : std::string();
        if (m.scenario == entry.map && mod == entry.mod) { out.map = std::move(m); found = true; break; }
    }
    if (!found) return false;
    std::vector<const MenuVariant*> allowed;
    out.variant = nullptr;
    for (const MenuVariant& v : ctx.variants) {
        if (!entry.mod.empty() || v.type.empty() || MapGameTypes::Supports(entry.map, v.type)) allowed.push_back(&v);
        if (v.path.filename().string() == entry.variant) out.variant = &v;
    }
    static std::mt19937 s_random{ std::random_device{}() };
    if ((randomType || !out.variant) && !allowed.empty()) out.variant = allowed[s_random() % allowed.size()];
    return out.variant != nullptr;
}

static void ApplyEntryToLobby(MenuState& st, const ResolvedEntry& e) {
    st.lobbyMap = e.map.launchScenario.empty() ? e.map.scenario : e.map.launchScenario;
    st.lobbyMapOverride = e.map.mapOverride;
    st.lobbyMapTitle = e.map.name;
    st.lobbyMapImage = e.map.imageLoading;
    st.lobbyMapThumb = e.map.imageThumbnail;
    st.lobbyModId = e.map.mod ? e.map.mod->workshopId : std::string();
    st.lobbyModTitle = e.map.mod ? e.map.mod->title : std::string();
    st.lobbyModFolder = e.map.mod ? e.map.mod->folder : std::filesystem::path();
    st.lobbyVariantPath = e.variant->path.string();
    st.lobbyVariantName = e.variant->label;
    MasterServerClient::Get().UpdateLobbyAdvert(st.lobbyMap, st.lobbyMode, st.lobbyVariantName, st.lobbyDifficulty, st.lobbySkulls);
    NOBLE_INFO("FrontEndMenu: lobby now {} on {}", st.lobbyVariantName, st.lobbyMapTitle);
}

// Opens a vote with up to three different entries from the active playlist.
static void StartLobbyVote(OverlayContext& ctx) {
    MenuState& st = ctx.st;
    st.voteMessage.clear();
    if (st.playlistChoice < 0 || st.playlistChoice >= static_cast<int>(st.playlists.size())) return;
    const Playlists::Playlist& playlist = st.playlists[st.playlistChoice];
    std::vector<Playlists::Entry> pool = playlist.entries;
    static std::mt19937 s_random{ std::random_device{}() };
    std::shuffle(pool.begin(), pool.end(), s_random);
    st.voteEntries.clear();
    std::vector<std::string> labels;
    for (const Playlists::Entry& entry : pool) {
        if (labels.size() >= 3) break;
        ResolvedEntry resolved;
        if (!ResolveEntry(ctx, entry, playlist.randomType, resolved)) continue;
        Playlists::Entry fixed = entry;
        fixed.variant = resolved.variant->path.filename().string();   // a random type is settled before anyone votes
        st.voteEntries.push_back(fixed);
        labels.push_back(resolved.map.name + " - " + resolved.variant->label);
    }
    if (labels.size() < 2) {
        st.voteMessage = "A vote needs at least 2 playable entries in the playlist.";
    } else if (!MasterServerClient::Get().StartVoteSync(st.lobbySessionId, labels, 20)) {
        st.voteMessage = "The server didn't accept the vote. It may need updating.";
        MenuSounds::Error();
    } else {
        NOBLE_INFO("FrontEndMenu: vote opened with {} options", labels.size());
        st.voteCast = -1;
        MenuSounds::Click();
    }
}

// Host only, when time is up. "None of these" on top draws three new options once; a second time the lobby stays as it is.
static void FinishLobbyVote(OverlayContext& ctx) {
    MenuState& st = ctx.st;
    MasterServerClient::Get().EndVoteSync(st.lobbySessionId);
    std::vector<int> counts = st.voteCounts;
    counts.resize(st.voteEntries.size() + 1, 0);
    const int none = counts.back();
    const int best = *std::max_element(counts.begin(), counts.end() - 1);
    st.voteId.clear();
    if (none > best || best == 0) {
        NOBLE_INFO("FrontEndMenu: vote closed, none of these ({} votes, reroll {})", none, st.voteRerolls);
        if (none > best && st.voteRerolls < 1) {
            ++st.voteRerolls;
            StartLobbyVote(ctx);
        } else {
            st.voteRerolls = 0;
            st.voteMessage = "No choice won, so the lobby stays as it is.";
        }
        return;
    }
    std::vector<int> leaders;
    for (int i = 0; i + 1 < static_cast<int>(counts.size()); ++i) if (counts[i] == best) leaders.push_back(i);
    static std::mt19937 s_random{ std::random_device{}() };
    const int winner = leaders[s_random() % leaders.size()];
    st.voteRerolls = 0;
    ResolvedEntry resolved;
    if (ResolveEntry(ctx, st.voteEntries[winner], false, resolved)) {
        NOBLE_INFO("FrontEndMenu: vote closed, option {} won with {} votes", winner, best);
        ApplyEntryToLobby(st, resolved);
    }
}

static void DrawPreGameLobby(OverlayContext& ctx, const Ui& ui, float left, float top, FrontEndChoice& choice, bool& done, bool back) {
    MenuState& st = ctx.st;
    TakePendingLobbyEvents(st);

    if (st.isLobbyHost) {
        const std::string curId = MasterServerClient::Get().GetCurrentSessionId();
        const std::string curTok = MasterServerClient::Get().GetCurrentSessionToken();
        if (!curId.empty()) st.lobbySessionId = curId;
        if (!curTok.empty()) st.lobbySessionToken = curTok;
    }

    // Polling for lobby state updates
    const double now = ImGui::GetTime();
    if (now - st.lastLobbyPoll > 1.2) {
        st.lastLobbyPoll = now;
        if (st.isLobbyHost && st.lobbySessionId.empty()) {
            st.lobbySessionId = MasterServerClient::Get().GetCurrentSessionId();
            st.lobbySessionToken = MasterServerClient::Get().GetCurrentSessionToken();
        }
        if (!st.lobbySessionId.empty() && MasterServerClient::Get().Enabled()) {
            LobbyState current = MasterServerClient::Get().FetchLobbyStateSync(st.lobbySessionId);
            if (!current.ok || current.isClosed) {
                if (!st.isLobbyHost) {
                    st.screen = Screen::Servers;
                    st.lobbySessionId.clear();
                    st.lobbySessionToken.clear();
                    st.lobbyRoster.clear();
                    st.isLobbyHost = false;
                    NetworkTransport::Get().StopRelay();
                    return;
                }
            } else if (current.ok) {
                if (!st.isLobbyHost) {
                    st.lobbyMap = current.map;
                    st.lobbyMode = current.mode;
                    st.lobbyVariantName = current.variant;
                    st.lobbyDifficulty = current.difficulty;
                    st.lobbySkulls = current.skulls;
                    if (current.isLan && !current.hostLanIp.empty()) {
                        st.lobbyHostEndpoint = current.hostLanIp + ":" + std::to_string(current.hostPort);
                    }
                }
                if (!current.roster.empty()) {
                    st.lobbyRoster = current.roster;
                    if (st.isLobbyHost) {
                        std::string pName = AuthClient::Get().IsLoggedIn() ? AuthClient::Get().GetUsername() : Settings::Get().Snapshot().playerName;
                        if (pName.empty()) pName = "Host";
                        const uint64_t hostPId = AuthClient::Get().IsLoggedIn() ? static_cast<uint64_t>(AuthClient::Get().GetUserId()) : 1ULL;
                        const std::string hostTag = PlayerServiceTag(Settings::Get().Snapshot());
                        const bool hostDev = AuthClient::Get().IsDeveloper();
                        for (auto& p : st.lobbyRoster) {
                            if (p.isHost) {
                                p.peerId = hostPId;
                                p.machineId = DeviceIdentityStore::Get().deviceId;
                                p.username = pName;
                                p.serviceTag = hostTag;
                                p.isDev = hostDev;
                                const auto& prof = UIManager::GetArmorCustomizer().GetProfile();
                                p.rankIndex = Career::RankIndex(Career::Get().credits);
                                p.emblemId = prof.emblemIndex;
                                p.nameplateId = prof.nameplateIndex;
                                p.emblemBackgroundId = prof.emblemBackgroundIndex;
                                const auto rec = Career::GetRecord();
                                p.gamesPlayed = static_cast<int>(rec.totals.gamesPlayed);
                                p.kills = static_cast<int>(rec.totals.kills);
                                p.deaths = static_cast<int>(rec.totals.deaths);
                                int cd = 0;
                                for (const auto& c : Career::Challenges()) if (c.done) ++cd;
                                p.challengesDone = cd;
                                int ct = 0, ctm = 0;
                                for (const auto& c : Career::Commendations()) {
                                    if (c.tracked) { ct += c.tier; ctm += 6; }
                                }
                                p.commendations = ctm > 0 ? (ct * 100 / ctm) : 0;
                                p.armoryOwned = static_cast<int>(rec.totals.armoryOwned.size());
                            } else {
                                Career::PeerStats peer;
                                peer.playerId = !p.machineId.empty() ? p.machineId : (!p.username.empty() ? p.username : std::to_string(p.peerId));
                                peer.playerName = p.username;
                                peer.serviceTag = p.serviceTag;
                                peer.rank = p.rankIndex;
                                peer.gamesPlayed = p.gamesPlayed;
                                peer.kills = p.kills;
                                peer.deaths = p.deaths;
                                peer.challengesDone = p.challengesDone;
                                peer.commendations = p.commendations;
                                peer.armoryOwned = p.armoryOwned;
                                Career::SavePeerStats(peer);
                                Career::RegisterPlayerName(p.peerId, p.username, p.serviceTag);
                            }
                        }
                    }
                }
                // A vote: everyone sees it through the session; the host alone closes it and applies the result.
                if (current.voteId != st.voteId) st.voteCast = -1;
                st.voteId = current.voteId;
                st.voteOptions = current.voteOptions;
                st.voteCounts = current.voteCounts;
                st.voteMsLeft = current.voteMsLeft;
                if (st.isLobbyHost && !st.voteId.empty() && st.voteMsLeft <= 0) FinishLobbyVote(ctx);
                if (st.isLobbyHost && !st.lobbyRoster.empty() && st.lobbyMode == "multiplayer") {
                    RefreshLobbyMode(ctx);
                    AssignLobbyTeams(st);
                }
                // Launch only while the host's match is running. The server keeps the last launch_data when the host
                // re-registers the same address and port for its next lobby, so "launch_data present" alone sent a
                // guest straight into a match that wasn't starting (2026-10-29 review, tested against server.js).
                if (!st.isLobbyHost && current.inProgress) {
                    st.lobbyLaunchPending = true;
                    st.lobbyLaunchMap = current.launchMap.empty() ? current.map : current.launchMap;
                    st.lobbyLaunchMode = current.launchMode.empty() ? current.mode : current.launchMode;
                    st.lobbyLaunchVariant = current.launchVariant;
                    st.lobbyLaunchDiff = current.launchDifficulty;
                    st.lobbyLaunchSkulls = current.launchSkulls;
                }
            }
        }
    }

    // If launch was signaled (via WebSocket or HTTP poll) and we are guest, launch!
    if (!st.isLobbyHost && st.lobbyLaunchPending && !done) {
        choice.play = true;
        for (size_t p = 0; p < st.lobbyRoster.size(); ++p) {
            const auto& player = st.lobbyRoster[p];
            Career::RegisterPlayerName(player.peerId, player.username, player.serviceTag);
            Career::RegisterPlayerSlot(static_cast<int>(p), player.username, player.serviceTag);
        }
        choice.host = false;
        choice.map = st.lobbyLaunchMap.empty() ? st.lobbyMap : st.lobbyLaunchMap;
        choice.mapOverride = st.lobbyMapOverride;
        choice.mapTitle = st.lobbyMapTitle;
        choice.mapImage = st.lobbyMapImage;
        choice.mapThumb = st.lobbyMapThumb;
        choice.mode = st.lobbyLaunchMode.empty() ? st.lobbyMode : st.lobbyLaunchMode;
        choice.variantPath = st.lobbyLaunchVariant;
        choice.difficulty = st.lobbyLaunchDiff;
        choice.skulls = st.lobbyLaunchSkulls;
        // The lobby's map (and any replacement objects) live in the Workshop mod, so the match reads it first.
        choice.modFolder = st.lobbyModFolder;
        choice.modTitle = st.lobbyModTitle;
        NOBLE_INFO("Guest launching from lobby: map='{}' mode='{}' variant='{}' mod='{}' connect='{}'",
                   choice.map, choice.mode, choice.variantPath, st.lobbyModId, st.lobbyHostEndpoint);
        choice.connectAddress = st.lobbyHostEndpoint;
        choice.sessionId = st.lobbySessionId;
        if (st.lobbySessionToken.empty()) st.lobbySessionToken = MasterServerClient::Get().GetCurrentSessionToken();
        choice.sessionToken = st.lobbySessionToken;

        // The host sends its variant as a path on its own PC; use the guest's copy of the same file. Without a
        // variant the guest's launch block has no game variant (the 5% load hang from 2026-10-28).
        if (st.lobbyLaunchVariant.empty() && !st.lobbyVariantName.empty()) {
            st.lobbyLaunchVariant = st.lobbyVariantName;
        }
        if (!st.lobbyLaunchVariant.empty()) {
            const std::filesystem::path want = std::filesystem::path(st.lobbyLaunchVariant).filename();
            std::filesystem::path local;
            for (const auto* list : { &ctx.variants, &ctx.ffVariants })
                for (const MenuVariant& v : *list)
                    if (local.empty() && (_wcsicmp(v.path.filename().c_str(), want.c_str()) == 0 ||
                                          _stricmp(v.label.c_str(), st.lobbyLaunchVariant.c_str()) == 0))
                        local = v.path;
            if (local.empty() && (st.lobbyLaunchMode == "forge" || st.lobbyMode == "forge" || _wcsicmp(want.c_str(), kForgeVariant) == 0)) {
                if (std::filesystem::exists(ctx.forgeVariant)) local = ctx.forgeVariant;
            }
            if (!local.empty()) {
                choice.variantPath = local.string();
            } else {
                NOBLE_WARN("FrontEndMenu: host's game variant '{}' isn't in this PC's variants; launching without it",
                           st.lobbyLaunchVariant);
            }
        } else if (st.lobbyLaunchMode == "forge" || st.lobbyMode == "forge") {
            if (std::filesystem::exists(ctx.forgeVariant)) choice.variantPath = ctx.forgeVariant.string();
        }
        NOBLE_INFO("FrontEndMenu: Guest executing synchronized lobby launch to {}", st.lobbyHostEndpoint);
        done = true;
        return;
    }

    // Same frame as the setup screens: DrawTitle at (left, top), the option column under it, the right column
    // at px/pw, DrawHints at the bottom. Every block has a fixed place, so nothing moves between the empty, one
    // player and full states or between host and guest (design pass, 2026-10-31).
    ui.dl->AddRectFilled(ui.origin, ImVec2(ui.origin.x + ui.size.x, ui.origin.y + ui.size.y), IM_COL32(4, 8, 12, 110));
    const float colX = left + 34.0f * ui.s;
    const float colW = 380.0f * ui.s;
    const float px = ui.origin.x + ui.size.x * 0.47f;
    const float pw = ui.size.x * 0.44f;
    const ImU32 panelFill = IM_COL32(8, 12, 18, 190);
    const ImU32 panelEdge = IM_COL32(170, 200, 220, 120);
    const ImU32 kRuleLine = IM_COL32(255, 255, 255, 50);

    // Resolve Map Display Name. A mod map uses the title from its map JSON; a stock map uses the
    // Reach table. Without this the lobby says "Boneyard" for every Campaign-to-Forge selection
    // because 70_boneyard is only the carrier the engine launches through.
    std::string mapDisplay = !st.lobbyMapTitle.empty() ? st.lobbyMapTitle : st.lobbyMap;
    if (st.lobbyMapTitle.empty()) {
        for (int i = 0; i < Count(kMaps); ++i) {
            if (kMaps[i].scenario == st.lobbyMap) { mapDisplay = kMaps[i].name; break; }
        }
        for (int i = 0; i < Count(kFirefightMaps); ++i) {
            if (kFirefightMaps[i].scenario == st.lobbyMap) { mapDisplay = kFirefightMaps[i].name; break; }
        }
        for (int i = 0; i < Count(kMissions); ++i) {
            if (kMissions[i].scenario == st.lobbyMap) { mapDisplay = kMissions[i].name; break; }
        }
    }

    const bool campaign = st.lobbyMode == "campaign";
    const bool forge = st.lobbyMode == "forge";
    const bool firefight = st.lobbyMode == "firefight";
    DrawTitle(ui, ImVec2(left, top), campaign ? "CAMPAIGN" : forge ? "FORGE" : firefight ? "FIREFIGHT" : "CUSTOM GAME",
              campaign ? "CO-OP" : forge ? "EDIT" : firefight ? "SURVIVAL" : "MULTIPLAYER");

    const char* subtitle = !st.isLobbyHost ? (st.lobbyHostEndpoint.empty() ? "Connecting to the host..." : "The host chooses the game and starts the match.")
                         : st.lobbyNetMode == 0 ? "Local network game."
                         : st.lobbyPrivacy == 0 ? "Public game, listed in the server browser."
                                                : "Private game, join by invite.";
    Text(ui, NobleStyle::SmallFont(), 15.0f, ImVec2(colX, top + 70.0f * ui.s), kDim, subtitle);

    struct LobbyMenuOption {
        std::string label;
        std::string value;
        bool isStart;
        bool editable;
        std::string tooltip;
    };
    std::vector<LobbyMenuOption> optRows;
    const char* const kReadyTip = "Your own ready state. It isn't sent to the host yet.";
    const char* const kOptionsTip = "Uses the game type's own settings. Edit them from GAME OPTIONS on Custom Game, before hosting.";
    const bool playlistActive = st.isLobbyHost && st.playlistChoice >= 0 && st.playlistChoice < static_cast<int>(st.playlists.size());
    // A guest: is the host's game type one this PC has? A host's own game mode isn't sent to guests yet (@deepseek's
    // transport); until it is, say so rather than let the player find out from the match.
    bool hostModeKnown = st.isLobbyHost || st.lobbyVariantName.empty() || st.lobbyVariantName == "Default";
    if (!hostModeKnown) {
        for (const auto* list : { &ctx.variants, &ctx.ffVariants }) {
            for (const MenuVariant& v : *list) {
                if (_stricmp(v.label.c_str(), st.lobbyVariantName.c_str()) == 0 || _stricmp(v.path.filename().string().c_str(), st.lobbyVariantName.c_str()) == 0) hostModeKnown = true;
            }
        }
    }
    const char* const hostModeTip = hostModeKnown ? "You have the game type the host picked."
                                                  : "The host made this game mode, and this PC doesn't have it. Downloading it from the host is coming.";

    if (campaign) {
        optRows.push_back({ "READY", st.lobbyReady ? "READY" : "NOT READY", false, true, kReadyTip });
        optRows.push_back({ "MISSION", mapDisplay, false, st.isLobbyHost, "Select which campaign mission to play." });
        int skullCount = 0;
        for (int b = 0; b < 64; ++b) if ((st.lobbySkulls >> b) & 1ULL) ++skullCount;
        optRows.push_back({ "SKULLS", skullCount >= kSkullCount ? std::string("ALL ON") : std::to_string(skullCount) + " ON",
                            false, false, "Skulls are chosen on the Campaign screen before hosting." });
        static const char* kDiffs[] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };
        optRows.push_back({ "DIFFICULTY", kDiffs[std::clamp(st.lobbyDifficulty, 0, 3)], false, st.isLobbyHost, "Set campaign difficulty level." });
        if (st.isLobbyHost) optRows.push_back({ "START CO-OP CAMPAIGN", "", true, true, "Start the selected mission." });
    } else if (forge) {
        optRows.push_back({ "GAME TYPE", "BASIC EDITING", false, false, "Forge always uses Basic Editing." });
        optRows.push_back({ "MAP", mapDisplay, false, st.isLobbyHost, "Choose which map to forge." });
        optRows.push_back({ "GAME OPTIONS", "DEFAULTS", false, false, kOptionsTip });
        if (st.isLobbyHost) optRows.push_back({ "START GAME", "", true, true, "Start the selected Forge session." });
    } else if (firefight) {
        optRows.push_back({ "READY", st.lobbyReady ? "READY" : "NOT READY", false, true, kReadyTip });
        optRows.push_back({ "GAME TYPE", st.lobbyVariantName.empty() ? "NORMAL" : st.lobbyVariantName, false, st.isLobbyHost, "Select Firefight game variant." });
        optRows.push_back({ "MAP", mapDisplay, false, st.isLobbyHost, "Choose which Firefight map to play." });
        if (st.isLobbyHost) optRows.push_back({ "GAME OPTIONS", "DEFAULTS", false, false, kOptionsTip });
        else optRows.push_back({ "HOST MODE", hostModeKnown ? "ON THIS PC" : "UNKNOWN", false, false, hostModeTip });
        if (st.isLobbyHost) optRows.push_back({ "START FIREFIGHT", "", true, true, "Start the Firefight match." });
    } else { // Custom Game
        optRows.push_back({ "READY", st.lobbyReady ? "READY" : "NOT READY", false, true, kReadyTip });
        optRows.push_back({ "GAME TYPE", st.lobbyVariantName.empty() ? "SLAYER" : st.lobbyVariantName, false, st.isLobbyHost, "Choose game type and rule set." });
        optRows.push_back({ "MAP", mapDisplay, false, st.isLobbyHost, "Choose which map you want to play." });
        if (st.isLobbyHost) RefreshLobbyMode(ctx);
        const std::string modeValue = st.lobbyModeChanges > 0 ? std::to_string(st.lobbyModeChanges) + " CHANGED" : std::string("DEFAULTS");
        const std::string modeTip = st.lobbyModeChanges > 0 ? st.lobbyModeSummary : std::string(kOptionsTip);
        if (playlistActive) optRows.push_back({ "VOTE", "START", false, false, st.lobbyModeChanges > 0 ? "Vote on what's next. This mode: " + st.lobbyModeSummary : "Press to let everyone vote on what to play next from this playlist." });
        else if (st.isLobbyHost) optRows.push_back({ "GAME OPTIONS", modeValue, false, false, modeTip });
        else optRows.push_back({ "HOST MODE", hostModeKnown ? "ON THIS PC" : "UNKNOWN", false, false, hostModeTip });
        if (st.isLobbyHost) optRows.push_back({ "START GAME", "", true, true, "Start the selected custom game." });
    }

    const int totalRows = static_cast<int>(optRows.size());
    st.lobbyMenuRow = std::clamp(st.lobbyMenuRow, 0, totalRows - 1);

    // Controller & Keyboard Navigation
    auto stepRow = [&](int d) {
        st.lobbyMenuRow = (st.lobbyMenuRow + d + totalRows) % totalRows;
        MenuSounds::Scroll();
    };
    if (!st.lobbyPickerOpen) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown)) stepRow(1);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickUp)) stepRow(-1);
    }

    // A vanilla map only hosts the game types it was built for (Invasion on Sword Base crashed the engine), so
    // the lobby's cycling skips what the Custom Game screen refuses. Modded lobbies are never restricted.
    auto comboAllowed = [&](const std::string& scenario, const std::string& variantPath) {
        if (campaign || firefight || forge || !st.lobbyModFolder.empty() || variantPath.empty()) return true;
        const std::string type = VariantPathType(variantPath);
        return type.empty() || MapGameTypes::Supports(scenario, type);
    };

    // The pre-game map row offers the maps from the host's selected mod first, then the stock
    // Reach table. A Campaign-to-Forge map keeps its own title and art even though the engine
    // launches it through 70_boneyard; a stock choice clears the override again.
    struct LobbyMapChoice {
        std::string launch;   // scenario the engine launches (the carrier for a mod map)
        std::string label;    // title to show
        std::filesystem::path overrideFile;
        std::filesystem::path image;
        std::filesystem::path thumb;
    };
    const MenuMap* baseMaps = campaign ? kMissions : (firefight ? kFirefightMaps : kMaps);
    const int baseCount = campaign ? Count(kMissions) : (firefight ? Count(kFirefightMaps) : Count(kMaps));
    const std::string carrier = campaign ? "m10" : (firefight ? "ff60_icecave" : "70_boneyard");
    const ModEntry* lobbyMod = nullptr;
    for (const ModEntry& mod : ctx.mods) {
        if ((!st.lobbyModFolder.empty() && mod.folder == st.lobbyModFolder) ||
            (!st.lobbyModId.empty() && !mod.workshopId.empty() && mod.workshopId == st.lobbyModId)) {
            lobbyMod = &mod;
            break;
        }
    }
    std::vector<LobbyMapChoice> mapChoices;
    auto addMapChoice = [&](LobbyMapChoice choice) {
        for (const LobbyMapChoice& existing : mapChoices) {
            if (existing.launch == choice.launch && existing.overrideFile == choice.overrideFile &&
                existing.label == choice.label) {
                return;
            }
        }
        mapChoices.push_back(std::move(choice));
    };
    if (lobbyMod) {
        const std::vector<ModMap>& listed = campaign ? lobbyMod->campaign
                                            : firefight ? lobbyMod->firefight
                                                        : lobbyMod->multiplayer;
        const std::vector<MapEntry> maps = ModMaps(*lobbyMod, listed, baseMaps, baseCount, carrier);
        for (const MapEntry& m : maps) {
            if (m.header || !m.enabled || m.mapOverride.empty()) continue;
            addMapChoice({ m.launchScenario.empty() ? m.scenario : m.launchScenario, m.name, m.mapOverride,
                           m.imageLarge.empty() ? m.imageThumbnail : m.imageLarge, m.imageThumbnail });
        }
    }
    if (!st.lobbyMapOverride.empty()) {
        addMapChoice({ st.lobbyMap, st.lobbyMapTitle.empty() ? st.lobbyMap : st.lobbyMapTitle, st.lobbyMapOverride,
                       st.lobbyMapImage, st.lobbyMapThumb });
    }
    for (int i = 0; i < baseCount; ++i) {
        addMapChoice({ baseMaps[i].scenario, baseMaps[i].name, {}, {}, {} });
    }

    auto cycleOption = [&](int dir) {
        const auto& row = optRows[st.lobbyMenuRow];
        if (!row.editable || row.isStart) return;
        if (row.label == "READY") {
            st.lobbyReady = !st.lobbyReady;
            MenuSounds::Change();
        } else if (row.label == "MISSION" || row.label == "MAP") {
            if (mapChoices.empty()) return;
            const int count = static_cast<int>(mapChoices.size());
            int cur = 0;
            bool found = false;
            for (int i = 0; i < count; ++i) {
                if (mapChoices[i].launch == st.lobbyMap && mapChoices[i].overrideFile == st.lobbyMapOverride) {
                    cur = i;
                    found = true;
                    break;
                }
            }
            if (!found && !st.lobbyMapTitle.empty()) {
                for (int i = 0; i < count; ++i) {
                    if (mapChoices[i].label == st.lobbyMapTitle) { cur = i; found = true; break; }
                }
            }
            if (!found) {
                for (int i = 0; i < count; ++i) {
                    if (mapChoices[i].launch == st.lobbyMap && mapChoices[i].overrideFile.empty()) { cur = i; break; }
                }
            }
            for (int tries = 0; tries < count; ++tries) {
                cur = (cur + dir + count) % count;
                const LobbyMapChoice& candidate = mapChoices[cur];
                if (!candidate.overrideFile.empty() || comboAllowed(candidate.launch, st.lobbyVariantPath)) break;
            }
            const LobbyMapChoice& chosen = mapChoices[cur];
            st.lobbyMap = chosen.launch;
            st.lobbyMapOverride = chosen.overrideFile;
            st.lobbyMapTitle = chosen.overrideFile.empty() ? std::string() : chosen.label;
            st.lobbyMapImage = chosen.image;
            st.lobbyMapThumb = chosen.thumb;
            if (lobbyMod && !chosen.overrideFile.empty()) {
                st.lobbyModFolder = lobbyMod->folder;
                st.lobbyModId = lobbyMod->workshopId;
                st.lobbyModTitle = lobbyMod->title;
            }
            MasterServerClient::Get().UpdateLobbyAdvert(st.lobbyMap, st.lobbyMode, st.lobbyVariantName, st.lobbyDifficulty, st.lobbySkulls);
            MenuSounds::Change();
        } else if (row.label == "GAME TYPE") {
            const std::vector<MenuVariant>& varList = firefight ? ctx.ffVariants : ctx.variants;
            if (!varList.empty()) {
                const int count = static_cast<int>(varList.size());
                int cur = 0;
                for (int i = 0; i < count; ++i) {
                    if (varList[i].label == st.lobbyVariantName) { cur = i; break; }
                }
                for (int tries = 0; tries < count; ++tries) {
                    cur = (cur + dir + count) % count;
                    if (comboAllowed(st.lobbyMap, varList[cur].path.string())) break;
                }
                st.lobbyVariantPath = varList[cur].path.string();
                st.lobbyVariantName = varList[cur].label;
                MasterServerClient::Get().UpdateLobbyAdvert(st.lobbyMap, st.lobbyMode, st.lobbyVariantName, st.lobbyDifficulty, st.lobbySkulls);
                MenuSounds::Change();
            }
        } else if (row.label == "DIFFICULTY") {
            st.lobbyDifficulty = (st.lobbyDifficulty + dir + 4) % 4;
            MasterServerClient::Get().UpdateLobbyAdvert(st.lobbyMap, st.lobbyMode, st.lobbyVariantName, st.lobbyDifficulty, st.lobbySkulls);
            MenuSounds::Change();
        }
    };

    if (!st.lobbyPickerOpen) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft)) cycleOption(-1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight)) cycleOption(1);
    }

    auto startMatch = [&]() {
        const std::string variantFileName = st.lobbyVariantPath.empty()
            ? ""
            : std::filesystem::path(st.lobbyVariantPath).filename().string();
        {
            const std::string curId = MasterServerClient::Get().GetCurrentSessionId();
            const std::string curTok = MasterServerClient::Get().GetCurrentSessionToken();
            if (!curId.empty()) st.lobbySessionId = curId;
            if (!curTok.empty()) st.lobbySessionToken = curTok;
        }
        // Refresh the roster once at launch so the host's peer barrier counts every
        // guest that is actually in the lobby, not a snapshot that is up to 1.2 s old.
        if (!st.lobbySessionId.empty() && MasterServerClient::Get().Enabled()) {
            const LobbyState current = MasterServerClient::Get().FetchLobbyStateSync(st.lobbySessionId);
            if (current.ok && !current.roster.empty()) st.lobbyRoster = current.roster;
        }
        std::string variantBody;
        if (!st.lobbyVariantPath.empty() && VariantRules::IsPresetFile(st.lobbyVariantPath)) {
            std::ifstream in(st.lobbyVariantPath, std::ios::binary);
            if (in) variantBody.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        try {
            MasterServerClient::Get().LaunchLobbySync(st.lobbySessionId, st.lobbyMap, st.lobbyMode, variantFileName, st.lobbyDifficulty, st.lobbySkulls, variantBody);
        } catch (const std::exception& e) {
            NOBLE_ERROR("FrontEndMenu: LaunchLobbySync exception: {}", e.what());
        } catch (...) {
            NOBLE_ERROR("FrontEndMenu: LaunchLobbySync unknown exception");
        }
        NOBLE_INFO("Lobby start broadcast: session='{}' map='{}' mode='{}' variant='{}' difficulty={} skulls=0x{:X} roster={}",
                   st.lobbySessionId, st.lobbyMap, st.lobbyMode, variantFileName, st.lobbyDifficulty, st.lobbySkulls,
                   st.lobbyRoster.size());

        choice.play = true;
        choice.host = true;
        for (size_t p = 0; p < st.lobbyRoster.size(); ++p) {
            const auto& player = st.lobbyRoster[p];
            Career::RegisterPlayerName(player.peerId, player.username, player.serviceTag);
            Career::RegisterPlayerSlot(static_cast<int>(p), player.username, player.serviceTag);
        }
        choice.sessionId = st.lobbySessionId;
        choice.sessionToken = st.lobbySessionToken;
        choice.hostOnline = (st.lobbyNetMode == 1);
        choice.hostPublic = (st.lobbyPrivacy == 0);
        choice.hostMaxPlayers = st.lobbyMaxPlayers;
        choice.expectedPeers = 0;
        for (const auto& player : st.lobbyRoster) {
            if (!player.isHost) ++choice.expectedPeers;
        }
        choice.map = st.lobbyMap;
        choice.mapOverride = st.lobbyMapOverride;
        choice.mapTitle = st.lobbyMapTitle;
        choice.mapImage = st.lobbyMapImage;
        choice.mapThumb = st.lobbyMapThumb;
        choice.mode = st.lobbyMode;
        choice.variantPath = st.lobbyVariantPath;
        choice.difficulty = st.lobbyDifficulty;
        choice.skulls = st.lobbySkulls;
        // The host picked this lobby's map from a mod; the relaunched match must read that mod first.
        choice.modFolder = st.lobbyModFolder;
        choice.modTitle = st.lobbyModTitle;
        done = true;
    };

    // A rotation that starts by itself. B or Esc stops the countdown for this lobby, and a vote holds it.
    if (st.autoStartAt > 0.0 && st.isLobbyHost) {
        if (back) {
            st.autoStartAt = 0.0;
            back = false;
            NOBLE_INFO("FrontEndMenu: next-match countdown cancelled");
            MenuSounds::Back();
        } else if (!st.voteId.empty()) {
            st.autoStartAt += ImGui::GetIO().DeltaTime;
        } else if (ImGui::GetTime() >= st.autoStartAt) {
            st.autoStartAt = 0.0;
            NOBLE_INFO("FrontEndMenu: countdown finished, starting the next match");
            startMatch();
        }
    }
    bool pickerJustOpened = false;   // the press that opened the list must not also pick from it
    auto triggerAction = [&]() {
        const auto& cur = optRows[st.lobbyMenuRow];
        if (cur.isStart) {
            startMatch();
        } else if (cur.label == "VOTE") {
            StartLobbyVote(ctx);
        } else if (cur.label == "GAME TYPE" && cur.editable) {
            st.lobbyPickerOpen = true;
            pickerJustOpened = true;
            MenuSounds::Click();
        } else {
            cycleOption(1);
        }
    };

    if (!st.lobbyPickerOpen && AcceptPressed()) triggerAction();

    // Option rows. The start button keeps one place whichever mode's list is above it.
    const float rowsY = top + 108.0f * ui.s;
    const float rowStep = 36.0f * ui.s;
    const float startY = rowsY + rowStep * 4.0f + 6.0f * ui.s;
    const bool mouseMoved = ImGui::GetIO().MouseDelta.x != 0.0f || ImGui::GetIO().MouseDelta.y != 0.0f;
    float curY = rowsY;
    const bool pickerWasOpen = st.lobbyPickerOpen;
    const bool voteUp = !st.voteId.empty();
    if (voteUp) {
        // The vote takes the option rows' place for everyone: three choices and "none of these".
        st.lobbyPickerOpen = true;
        st.voteWasUp = true;
        const int choices = static_cast<int>(st.voteOptions.size()) + 1;
        char head[64];
        std::snprintf(head, sizeof(head), "VOTE: WHAT'S NEXT?   %d s", std::max(0, (st.voteMsLeft + 999) / 1000));
        Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(colX, rowsY - 26.0f * ui.s), kAccent, head);
        st.voteRow = std::clamp(st.voteRow, 0, choices - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || UiPad::Down()) { st.voteRow = (st.voteRow + 1) % choices; MenuSounds::Scroll(); }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || UiPad::Up()) { st.voteRow = (st.voteRow + choices - 1) % choices; MenuSounds::Scroll(); }
        int cast = AcceptPressed() ? st.voteRow : -1;
        for (int i = 0; i < choices; ++i) {
            const ImVec2 r0(colX, rowsY + i * 40.0f * ui.s), r1(colX + colW, r0.y + 36.0f * ui.s);
            ImGui::SetCursorScreenPos(r0);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##vote_option", ImVec2(colW, 36.0f * ui.s))) cast = i;
            if (ImGui::IsItemHovered() && mouseMoved) st.voteRow = i;
            ImGui::PopID();
            ui.dl->AddRectFilled(r0, r1, st.voteCast == i ? IM_COL32(52, 112, 156, 230) : IM_COL32(8, 12, 18, 170));
            if (st.voteRow == i) ui.dl->AddRect(r0, r1, kAccent, 0.0f, 0, 2.0f * ui.s);
            const std::string label = i < static_cast<int>(st.voteOptions.size()) ? st.voteOptions[i] : std::string("None of these");
            Text(ui, NobleStyle::TitleFont(), 17.0f, ImVec2(r0.x + 10.0f * ui.s, r0.y + 8.0f * ui.s), kWhite,
                 Ellipsize(NobleStyle::TitleFont(), 17.0f * ui.s, Upper(label), colW - 60.0f * ui.s).c_str());
            const std::string count = std::to_string(i < static_cast<int>(st.voteCounts.size()) ? st.voteCounts[i] : 0);
            Text(ui, NobleStyle::TitleFont(), 17.0f, ImVec2(r1.x - 26.0f * ui.s, r0.y + 8.0f * ui.s), kAccent, count.c_str());
        }
        if (cast >= 0) {
            st.voteCast = cast;
            uint64_t peer = 1;
            if (AuthClient::Get().IsLoggedIn()) peer = static_cast<uint64_t>(AuthClient::Get().GetUserId());
            else if (!st.isLobbyHost) { peer = MasterServerClient::GetLobbyMachineId() & 0x7FFFFFFFULL; if (peer < 2ULL) peer = 2ULL; }
            if (!MasterServerClient::Get().CastVoteSync(st.lobbySessionId, peer, cast)) NOBLE_WARN("FrontEndMenu: the vote wasn't accepted");
            MenuSounds::Click();
        }
    } else if (st.voteWasUp) {   // the vote just closed: the normal rows come back
        st.voteWasUp = false;
        st.lobbyPickerOpen = false;
    }
    if (st.lobbyPickerOpen && !voteUp) {
        // The game types this map can host, from the variants that ship with the game. Nothing here is invented: the
        // settings inside a variant can't be read until the variant viewer exists.
        const std::vector<MenuVariant>& varList = firefight ? ctx.ffVariants : ctx.variants;
        const bool anyMap = campaign || firefight || forge || !st.lobbyModFolder.empty();
        if (pickerJustOpened) {
            const MenuVariant* current = nullptr;
            for (const MenuVariant& v : varList) if (v.label == st.lobbyVariantName) current = &v;
            BeginVariantList(st, varList, st.lobbyMap, anyMap, current);
        }
        VariantRow shown;
        const MenuVariant* picked = DrawVariantList(ui, st, varList, st.lobbyMap, anyMap, ImVec2(colX, rowsY), colW - 10.0f * ui.s, 32.0f, 5, !pickerJustOpened, shown);
        DrawVariantInfo(ui, ImVec2(colX, rowsY + 40.0f * ui.s + 32.0f * ui.s * 5.0f + 10.0f * ui.s), colW, shown, true);
        if (picked) {
            st.lobbyVariantPath = picked->path.string();
            st.lobbyVariantName = picked->label;
            MasterServerClient::Get().UpdateLobbyAdvert(st.lobbyMap, st.lobbyMode, st.lobbyVariantName, st.lobbyDifficulty, st.lobbySkulls);
            st.lobbyPickerOpen = false;
            MenuSounds::Change();
        }
        if (back) {
            st.lobbyPickerOpen = false;
            MenuSounds::Back();
        }
    }
    for (int i = 0; i < totalRows && !pickerWasOpen; ++i) {
        const auto& row = optRows[i];
        const bool focused = st.lobbyMenuRow == i;
        if (row.isStart) {
            if (BigAction(ui, "lobby_start", ImVec2(colX, startY), colW, row.label.c_str(), row.tooltip.c_str(), true, true, focused)) {
                st.lobbyMenuRow = i;
                startMatch();
            }
            // Only a moved mouse takes the focus: a pointer left resting on a row used to pull the controller's
            // focus back to that row every frame.
            if (ImGui::IsItemHovered() && mouseMoved) st.lobbyMenuRow = i;
            continue;
        }
        char rowId[32];
        std::snprintf(rowId, sizeof(rowId), "##lobby_row_%d", i);
        const float h = 32.0f * ui.s;
        const ImVec2 r0(colX, curY), r1(colX + colW, curY + h);
        ImGui::SetCursorScreenPos(r0);
        if (ImGui::InvisibleButton(rowId, ImVec2(colW, h))) {
            st.lobbyMenuRow = i;
            cycleOption(ImGui::GetMousePos().x < colX + colW * 0.48f + 28.0f * ui.s ? -1 : 1);
        }
        if (ImGui::IsItemHovered() && mouseMoved && !focused) {
            st.lobbyMenuRow = i;
            MenuSounds::Scroll();
        }
        // The same plate, focus band, accent edge, font and arrows as OptionRow; a row that can't change has no arrows.
        ui.dl->AddRectFilled(r0, r1, IM_COL32(8, 12, 18, 150));
        if (focused) {
            ui.dl->AddRectFilledMultiColor(r0, r1, IM_COL32(60, 120, 170, 120), IM_COL32(60, 120, 170, 20),
                                           IM_COL32(60, 120, 170, 20), IM_COL32(60, 120, 170, 120));
            ui.dl->AddRectFilled(r0, ImVec2(r0.x + 2.0f * ui.s, r1.y), kAccent);
        }
        Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(r0.x + 10.0f * ui.s, r0.y + 6.0f * ui.s), focused ? kWhite : kDim, row.label.c_str());
        const float vx = colX + colW * 0.48f, vw = colW * 0.52f, aw = 28.0f * ui.s;
        const float cy = r0.y + h * 0.5f, ah = 6.0f * ui.s;
        if (row.editable) {
            ui.dl->AddTriangleFilled(ImVec2(vx + aw * 0.65f, cy - ah), ImVec2(vx + aw * 0.65f, cy + ah), ImVec2(vx + aw * 0.3f, cy), focused ? kWhite : kDim);
            const float rx = vx + vw - aw;
            ui.dl->AddTriangleFilled(ImVec2(rx + aw * 0.35f, cy - ah), ImVec2(rx + aw * 0.35f, cy + ah), ImVec2(rx + aw * 0.7f, cy), focused ? kWhite : kDim);
        }
        const std::string value = Upper(Ellipsize(NobleStyle::TitleFont(), 18.0f * ui.s, row.value,
                                                   vw - aw * 2.0f - 4.0f * ui.s));
        const float tw = TextWidth(NobleStyle::TitleFont(), 18.0f * ui.s, value.c_str());
        ui.dl->PushClipRect(ImVec2(vx + aw, r0.y), ImVec2(vx + vw - aw, r1.y), true);
        Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(std::max(vx + aw, vx + (vw - tw) * 0.5f), r0.y + 6.0f * ui.s), row.editable ? kWhite : kDim, value.c_str());
        ui.dl->PopClipRect();
        curY += rowStep;
    }
    if (!st.isLobbyHost) {
        BigAction(ui, "lobby_waiting", ImVec2(colX, startY), colW, "WAITING FOR HOST", "The match starts when the host starts it.", true, false);
    }
    // What the focused row does; the start button already says it on its own second line.
    float lobbyTipY = startY + 58.0f * ui.s;
    if (!hostModeKnown && !pickerWasOpen) {
        // Greyed until the host can send its mode: a plain row, because nothing can be pressed yet.
        const ImVec2 d0(colX, lobbyTipY), d1(colX + colW, lobbyTipY + 32.0f * ui.s);
        ui.dl->AddRectFilled(d0, d1, IM_COL32(8, 12, 18, 150));
        Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(d0.x + 10.0f * ui.s, d0.y + 6.0f * ui.s), kDisabled, "DOWNLOAD MODE");
        const char* soon = "Coming later";
        Text(ui, nullptr, 15.0f, ImVec2(d1.x - TextWidth(nullptr, 15.0f * ui.s, soon) - 10.0f * ui.s, d0.y + 7.0f * ui.s), kDisabled, soon);
        lobbyTipY += 38.0f * ui.s;
    }
    if (!pickerWasOpen && !optRows[st.lobbyMenuRow].isStart) {
        Text(ui, nullptr, 15.0f, ImVec2(colX, lobbyTipY), kDim, optRows[st.lobbyMenuRow].tooltip.c_str(), colX + colW);
    }
    if (playlistActive && !pickerWasOpen) {
        const Playlists::Playlist& active = st.playlists[st.playlistChoice];
        const std::string line = "PLAYLIST: " + active.name + (active.shuffle ? "  (shuffled)" : "");
        Text(ui, NobleStyle::SmallFont(), 15.0f, ImVec2(colX, lobbyTipY + 22.0f * ui.s), kAccent, line.c_str(), colX + colW);
    }
    if (!st.voteMessage.empty() && !pickerWasOpen) Text(ui, nullptr, 15.0f, ImVec2(colX, lobbyTipY + 42.0f * ui.s), IM_COL32(255, 190, 120, 255), st.voteMessage.c_str(), colX + colW);
    if (st.autoStartAt > 0.0 && st.isLobbyHost && !pickerWasOpen) {
        char countdown[64];
        std::snprintf(countdown, sizeof(countdown), "Next match in %d s. B or Esc cancels.", std::max(0, static_cast<int>(st.autoStartAt - ImGui::GetTime() + 0.999)));
        Text(ui, NobleStyle::TitleFont(), 17.0f, ImVec2(colX, lobbyTipY + 62.0f * ui.s), kAccent, countdown);
    }

    // Friends and the invite strip sit at the foot of the left column, above the prompts.
    const float netH = 44.0f * ui.s;
    const float netY = ui.origin.y + ui.size.y - 58.0f * ui.s - 8.0f * ui.s - netH;
    const float friendRowH = 25.0f * ui.s;
    const float friendsH = 26.0f * ui.s + friendRowH * 4.0f;
    const float friendsY = netY - 8.0f * ui.s - friendsH;

    const ImVec2 fr0(colX, friendsY), fr1(colX + colW, friendsY + friendsH);
    ui.dl->AddRectFilled(fr0, fr1, panelFill);
    ui.dl->AddRect(fr0, fr1, panelEdge);
    Text(ui, NobleStyle::TitleFont(), 16.0f, ImVec2(fr0.x + 10.0f * ui.s, fr0.y + 4.0f * ui.s), kWhite, "FRIENDS");
    ui.dl->AddLine(ImVec2(fr0.x, fr0.y + 26.0f * ui.s), ImVec2(fr1.x, fr0.y + 26.0f * ui.s), kRuleLine, 1.0f);
    const bool lobbyTyping = ImGui::GetIO().WantTextInput;   // these keys are letters in a search box
    const bool friendsPressed = (!lobbyTyping && ImGui::IsKeyPressed(ImGuiKey_F, false)) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false) ||
                                (ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringRect(fr0, ImVec2(fr1.x, fr0.y + 26.0f * ui.s), false));
    if (friendsPressed) {
        st.socialPreviousScreen = Screen::PreGameLobby;
        st.screen = Screen::Social;
        MenuSounds::Click();
    }
    float friendY = friendsY + 26.0f * ui.s;
    const auto realFriends = SocialClient::Get().GetFriends();
    if (realFriends.empty()) {
        Text(ui, NobleStyle::SmallFont(), 15.0f, ImVec2(fr0.x + 10.0f * ui.s, friendY + 5.0f * ui.s), kDim, "No friends online.");
    }
    int friendIdx = 0;
    for (const auto& f : realFriends) {
        if (friendIdx >= 4) break;
        Text(ui, NobleStyle::SmallFont(), 15.0f, ImVec2(fr0.x + 10.0f * ui.s, friendY + 4.0f * ui.s), kWhite, f.username.c_str());
        if (st.isLobbyHost) {
            const float invW = 64.0f * ui.s;
            const float invH = 20.0f * ui.s;
            ImGui::SetCursorScreenPos(ImVec2(fr1.x - invW - 6.0f * ui.s, friendY + 2.5f * ui.s));
            ImGui::PushID(1500 + friendIdx);
            if (ImGui::Button("INVITE", ImVec2(invW, invH))) {
                std::string myIp = UPnP::Get().GetExternalIP();
                if (myIp.empty() || myIp == "Unknown") myIp = "127.0.0.1";
                std::string invMode = "Custom Game";
                if (campaign) invMode = "Campaign";
                else if (firefight) invMode = "Firefight";
                else if (forge) invMode = "Forge";
                SocialClient::Get().SendInviteAsync(f.username, myIp, 11774, [](bool ok, const std::string& msg) {
                    (void)msg;
                    if (ok) MenuSounds::Click();
                }, invMode, st.lobbyMap, st.lobbySessionId);
            }
            ImGui::PopID();
        }
        friendY += friendRowH;
        ++friendIdx;
    }

    // The invite strip never prints the address: it copies it. A public IP on screen ends up in screenshots and streams.
    const std::string extIp = UPnP::Get().GetExternalIP();
    const bool ipKnown = !extIp.empty() && extIp != "Unknown";
    const std::string hostEp = st.isLobbyHost ? (ipKnown ? extIp + ":11774" : std::string()) : st.lobbyHostEndpoint;
    const ImVec2 net0(colX, netY), net1(colX + colW, netY + netH);
    ui.dl->AddRectFilled(net0, net1, panelFill);
    ui.dl->AddRect(net0, net1, panelEdge);
    const char* netTitle = st.isLobbyHost ? "HOSTING" : hostEp.empty() ? "CONNECTING" : "CONNECTED";
    const char* netDesc = st.isLobbyHost ? (st.lobbyNetMode == 1 ? "Online peer-to-peer" : "Local network") : "Peer-to-peer";
    Text(ui, NobleStyle::TitleFont(), 16.0f, ImVec2(net0.x + 10.0f * ui.s, net0.y + 3.0f * ui.s), kWhite, netTitle);
    static double s_inviteCopiedAt = -10.0;
    const bool justCopied = now - s_inviteCopiedAt < 2.0;
    Text(ui, NobleStyle::SmallFont(), 14.0f, ImVec2(net0.x + 10.0f * ui.s, net0.y + 23.0f * ui.s), justCopied ? kAccent : kDim,
         justCopied ? "Invite copied to the clipboard." : netDesc);
    bool copyPressed = false;
    if (!hostEp.empty()) {
        const float cpW = 112.0f * ui.s, cpH = 28.0f * ui.s;
        ImGui::SetCursorScreenPos(ImVec2(net1.x - cpW - 8.0f * ui.s, net0.y + 8.0f * ui.s));
        ImGui::PushID("btn_copy_inv");
        copyPressed = ImGui::Button("COPY INVITE", ImVec2(cpW, cpH));
        ImGui::PopID();
        copyPressed = copyPressed || (!lobbyTyping && ImGui::IsKeyPressed(ImGuiKey_X, false)) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false);
    }
    if (copyPressed) {
        ImGui::SetClipboardText(("/connect " + hostEp).c_str());
        s_inviteCopiedAt = now;
        MenuSounds::Click();
    }

    // Right column: the map banner, then the session's roster in a panel of fixed size, so a filling lobby never moves anything.
    const float bannerY = top + 96.0f * ui.s;
    const float bannerH = 92.0f * ui.s;
    std::string kicker = !st.lobbyModTitle.empty() ? "MOD: " + Upper(st.lobbyModTitle)
                       : campaign ? std::string("HALO: REACH \xC2\xB7 CAMPAIGN")
                       : forge ? std::string("HALO: REACH \xC2\xB7 FORGE")
                               : "HALO: REACH \xC2\xB7 " + Upper(st.lobbyVariantName.empty() ? "MULTIPLAYER" : st.lobbyVariantName);
    const std::filesystem::path bannerImage = st.lobbyMapImage.empty() ? st.lobbyMapThumb : st.lobbyMapImage;
    HeroBanner(ui, ImVec2(px, bannerY), pw, bannerH, st.lobbyMap, kicker, mapDisplay.c_str(), "", bannerImage);

    const float headY = bannerY + bannerH + 8.0f * ui.s;
    const float headH = 28.0f * ui.s;
    const float slotH = 40.0f * ui.s;
    const float slotGap = 2.0f * ui.s;
    const float slotPad = 6.0f * ui.s;
    const float teamHeadH = 20.0f * ui.s;
    const float panelH = headH + teamHeadH + slotPad + (slotH + slotGap) * 8.0f - slotGap;
    ui.dl->AddRectFilled(ImVec2(px, headY), ImVec2(px + pw, headY + panelH), panelFill);
    ui.dl->AddRect(ImVec2(px, headY), ImVec2(px + pw, headY + panelH), panelEdge);
    Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(px + 10.0f * ui.s, headY + 4.0f * ui.s), kWhite, "MY GAME SESSION");
    char countBuf[32];
    std::snprintf(countBuf, sizeof(countBuf), "%zu/%d", st.lobbyRoster.size(), st.lobbyMaxPlayers);
    const float countW = TextWidth(NobleStyle::TitleFont(), 18.0f * ui.s, countBuf);
    Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(px + pw - countW - 10.0f * ui.s, headY + 4.0f * ui.s), kWhite, countBuf);
    ui.dl->AddLine(ImVec2(px, headY + headH), ImVec2(px + pw, headY + headH), kRuleLine, 1.0f);

    const int rosterCount = static_cast<int>(st.lobbyRoster.size());
    const int slotCount = std::clamp(std::max(st.lobbyMaxPlayers, rosterCount), 1, 128);
    const int openSlots = std::max(0, slotCount - rosterCount);
    const auto& ownProf = UIManager::GetArmorCustomizer().GetProfile();
    const std::string localName = AuthClient::Get().IsLoggedIn() ? AuthClient::Get().GetUsername() : Settings::Get().Snapshot().playerName;
    const std::string localDevId = DeviceIdentityStore::Get().deviceId;
    auto isLocal = [&](const LobbyPlayer& p) {
        if (p.isHost && st.isLobbyHost) return true;
        if (!localName.empty() && _stricmp(p.username.c_str(), localName.c_str()) == 0) return true;
        if (!p.machineId.empty() && p.machineId == localDevId) return true;
        return false;
    };

    // Red on the left and blue on the right, each with its own scroll position; a game without teams is one list.
    // Only the rows in view are drawn, so a 128-player roster costs the same as a full 16.
    constexpr int kVisibleRows = 8;
    struct RosterColumn {
        const char* title;
        ImU32 color;
        std::vector<int> players;
        int* scroll;
        float x;
        float w;
        int rows;
    };
    bool hasTeams = false;
    for (const LobbyPlayer& p : st.lobbyRoster) hasTeams = hasTeams || p.team != 0;
    std::vector<RosterColumn> columns;
    const float teamW = (pw - slotPad * 3.0f) * 0.5f;
    if (hasTeams) {
        columns.push_back({ "RED TEAM", IM_COL32(190, 34, 30, 255), {}, &st.lobbyRosterScroll[0], px + slotPad, teamW, 0 });
        columns.push_back({ "BLUE TEAM", IM_COL32(36, 62, 170, 255), {}, &st.lobbyRosterScroll[1], px + slotPad * 2.0f + teamW, teamW, 0 });
    } else {
        columns.push_back({ "PLAYERS", kAccent, {}, &st.lobbyRosterScroll[0], px + slotPad, pw - slotPad * 2.0f, 0 });
    }
    for (int i = 0; i < rosterCount; ++i) {
        columns[hasTeams && st.lobbyRoster[i].team == 2 ? 1 : 0].players.push_back(i);
    }
    int openLeft = openSlots;  // shared out, so the two columns together never show more OPEN rows than there are free slots
    for (size_t col = 0; col < columns.size(); ++col) {
        RosterColumn& c = columns[col];
        const int count = static_cast<int>(c.players.size());
        const int share = col + 1 < columns.size() ? (openLeft + 1) / 2 : openLeft;
        openLeft -= share;
        c.rows = hasTeams ? std::max(count, std::min(kVisibleRows, count + share)) : slotCount;
    }

    // LB/RB step through the players, LT/RT page within the column, the wheel scrolls the column under the pointer.
    auto keepInView = [&](int rosterIndex) {
        for (RosterColumn& c : columns) {
            const auto it = std::find(c.players.begin(), c.players.end(), rosterIndex);
            if (it == c.players.end()) continue;
            const int row = static_cast<int>(it - c.players.begin());
            if (row < *c.scroll) *c.scroll = row;
            if (row >= *c.scroll + kVisibleRows) *c.scroll = row - kVisibleRows + 1;
        }
    };
    if (rosterCount > 0) {
        st.lobbyRosterSelection = std::clamp(st.lobbyRosterSelection, 0, rosterCount - 1);
        const bool prev = (!lobbyTyping && ImGui::IsKeyPressed(ImGuiKey_Q, false)) || ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false);
        const bool next = (!lobbyTyping && ImGui::IsKeyPressed(ImGuiKey_E, false)) || ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false);
        if (prev != next) {
            st.lobbyRosterSelection = (st.lobbyRosterSelection + (next ? 1 : -1) + rosterCount) % rosterCount;
            keepInView(st.lobbyRosterSelection);
            MenuSounds::Scroll();
        }
        const bool pageUp = ImGui::IsKeyPressed(ImGuiKey_PageUp, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadL2, false);
        const bool pageDown = ImGui::IsKeyPressed(ImGuiKey_PageDown, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadR2, false);
        if (pageUp != pageDown) {
            for (RosterColumn& c : columns) {
                const auto it = std::find(c.players.begin(), c.players.end(), st.lobbyRosterSelection);
                if (it == c.players.end()) continue;
                const int row = std::clamp(static_cast<int>(it - c.players.begin()) + (pageDown ? kVisibleRows : -kVisibleRows),
                                           0, static_cast<int>(c.players.size()) - 1);
                st.lobbyRosterSelection = c.players[row];
                *c.scroll += pageDown ? kVisibleRows : -kVisibleRows;
                keepInView(st.lobbyRosterSelection);
                MenuSounds::Scroll();
                break;
            }
        }
    }

    // The selected player's card: Start or Tab, Back closes it. Only the local player's own numbers are known here.
    const bool profileWasOpen = st.lobbyProfileOpen;
    if (rosterCount > 0 && !pickerWasOpen && (ImGui::IsKeyPressed(ImGuiKey_Tab, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false))) {
        st.lobbyProfileOpen = !st.lobbyProfileOpen;
        MenuSounds::Click();
    }
    if (rosterCount == 0) st.lobbyProfileOpen = false;
    bool rosterOverflows = false;
    const float teamY = headY + headH;
    const float listY = teamY + teamHeadH;
    const float listH = (slotH + slotGap) * kVisibleRows - slotGap;
    if (st.lobbyProfileOpen) {
        const LobbyPlayer& player = st.lobbyRoster[st.lobbyRosterSelection];
        const bool local = isLocal(player);
        const float cardX = px + slotPad, cardW = pw - slotPad * 2.0f;
        float y = headY + headH + slotPad;
        const int rank = player.rankIndex >= 0 ? player.rankIndex : (local ? Career::RankIndex(Career::Get().credits) : -1);
        PlayerRow(ui, ImVec2(cardX, y), cardW, rank,
                  player.emblemId > 0 ? player.emblemId : (local ? ownProf.emblemIndex : 0),
                  player.nameplateId > 0 ? player.nameplateId : (local ? ownProf.nameplateIndex : 0),
                  player.team, player.serviceTag, player.username, player.isDev, false,
                  player.emblemBackgroundId > 0 ? player.emblemBackgroundId : (local ? ownProf.emblemBackgroundIndex : 0),
                  local ? UIManager::GetArmorCustomizer().EmblemColorsNow() : UiArt::EmblemColors{});
        y += slotH + 14.0f * ui.s;
        auto statLine = [&](const char* label, const std::string& value) {
            Text(ui, NobleStyle::SmallFont(), 15.0f, ImVec2(cardX + 4.0f * ui.s, y), kDim, label);
            const float w = TextWidth(NobleStyle::TitleFont(), 18.0f * ui.s, value.c_str());
            Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(cardX + cardW - w - 4.0f * ui.s, y - 2.0f * ui.s), kWhite, value.c_str());
            y += 28.0f * ui.s;
        };
        const auto& ranks = Career::Ranks();
        statLine("RANK", rank >= 0 && rank < static_cast<int>(ranks.size()) ? std::string(ranks[rank].name) : std::string("Not shared yet"));
        if (local) {
            const Career::Record rec = Career::GetRecord();
            int challengesDone = 0, challengeCount = 0, tiers = 0, tierMax = 0;
            for (const auto& c : Career::Challenges()) {
                ++challengeCount;
                if (c.done) ++challengesDone;
            }
            for (const auto& c : Career::Commendations()) {
                if (c.tracked) {
                    tiers += c.tier;
                    tierMax += 6;
                }
            }
            statLine("GAMES PLAYED", std::to_string(rec.totals.gamesPlayed));
            statLine("KILLS / DEATHS", std::to_string(rec.totals.kills) + " / " + std::to_string(rec.totals.deaths));
            const int totalChallenges = challengeCount > 0 ? challengeCount : 4;
            statLine("CHALLENGES DONE NOW", std::to_string(challengesDone) + " / " + std::to_string(totalChallenges));
            statLine("COMMENDATIONS", tierMax > 0 ? std::to_string(tiers * 100 / tierMax) + "%" : std::string("0%"));
            size_t boughtCount = rec.totals.armoryOwned.size();
            if (rec.totals.armoryUnlockAll && boughtCount == 0) {
                boughtCount = 179; // Total catalog count when unlock-all is active
            }
            statLine("ARMORY ITEMS BOUGHT", std::to_string(boughtCount));
        } else {
            int games = player.gamesPlayed;
            int kills = player.kills;
            int deaths = player.deaths;
            int challenges = player.challengesDone;
            int commendations = player.commendations;
            int armoryItems = player.armoryOwned;

            Career::PeerStats cached;
            if (Career::LoadPeerStats(player.machineId, cached) || Career::LoadPeerStats(player.username, cached)) {
                if (games <= 0) games = static_cast<int>(cached.gamesPlayed);
                if (kills <= 0) kills = static_cast<int>(cached.kills);
                if (deaths <= 0) deaths = static_cast<int>(cached.deaths);
                if (challenges <= 0) challenges = cached.challengesDone;
                if (commendations <= 0) commendations = cached.commendations;
                if (armoryItems <= 0) armoryItems = cached.armoryOwned;
            }

            statLine("GAMES PLAYED", std::to_string(games));
            statLine("KILLS / DEATHS", std::to_string(kills) + " / " + std::to_string(deaths));
            statLine("CHALLENGES DONE NOW", std::to_string(challenges) + " / 4");
            statLine("COMMENDATIONS", std::to_string(commendations) + "%");
            statLine("ARMORY ITEMS BOUGHT", std::to_string(armoryItems));
        }
    }
    for (size_t col = 0; col < columns.size() && !st.lobbyProfileOpen; ++col) {
        RosterColumn& c = columns[col];
        const bool overflow = c.rows > kVisibleRows;
        rosterOverflows = rosterOverflows || overflow;
        if (overflow && ImGui::IsMouseHoveringRect(ImVec2(c.x, listY), ImVec2(c.x + c.w, listY + listH), false)) {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) *c.scroll -= static_cast<int>(wheel) * 2;
        }
        // The pinned local row takes a slot, so the list scrolls one further or the last player could never be seen.
        int localAt = -1;
        for (size_t r = 0; r < c.players.size(); ++r) {
            if (isLocal(st.lobbyRoster[c.players[r]])) { localAt = static_cast<int>(r); break; }
        }
        const int pinRoom = overflow && localAt >= 0 && localAt < c.rows - kVisibleRows ? 1 : 0;
        *c.scroll = std::clamp(*c.scroll, 0, std::max(0, c.rows - kVisibleRows + pinRoom));

        // Sticky header: stays put while the rows scroll under it.
        const float cy = teamY + teamHeadH * 0.5f;
        ui.dl->AddCircleFilled(ImVec2(c.x + 7.0f * ui.s, cy), 4.5f * ui.s, c.color, 16);
        Text(ui, NobleStyle::SmallFont(), 14.0f, ImVec2(c.x + 17.0f * ui.s, teamY + 2.0f * ui.s), kWhite, c.title);
        char range[48];
        const int shownLast = std::min(*c.scroll + kVisibleRows, c.rows);   // the pinned row may cover one of these
        if (overflow) std::snprintf(range, sizeof(range), "%d-%d of %d", *c.scroll + 1, shownLast, c.rows);
        else std::snprintf(range, sizeof(range), "%zu", c.players.size());
        const float rangeW = TextWidth(NobleStyle::SmallFont(), 14.0f * ui.s, range);
        Text(ui, NobleStyle::SmallFont(), 14.0f, ImVec2(c.x + c.w - rangeW, teamY + 2.0f * ui.s), kDim, range);

        // The local player is never scrolled out of sight: while their row is off screen it takes the last row.
        int localRow = -1;
        for (size_t r = 0; r < c.players.size(); ++r) {
            if (isLocal(st.lobbyRoster[c.players[r]])) { localRow = static_cast<int>(r); break; }
        }
        const bool pinLocal = localRow >= 0 && (localRow < *c.scroll || localRow >= *c.scroll + kVisibleRows);
        const float barW = overflow ? 6.0f * ui.s : 0.0f;
        const float rowW = c.w - barW;
        for (int v = 0; v < kVisibleRows && (*c.scroll + v < c.rows || (pinLocal && v == kVisibleRows - 1)); ++v) {
            const int row = (pinLocal && v == kVisibleRows - 1) ? localRow : *c.scroll + v;
            const ImVec2 s0(c.x, listY + v * (slotH + slotGap));
            const ImVec2 s1(s0.x + rowW, s0.y + slotH);
            if (row >= static_cast<int>(c.players.size())) {
                ui.dl->AddRectFilled(s0, s1, IM_COL32(255, 255, 255, 8));
                ui.dl->AddRect(s0, s1, IM_COL32(255, 255, 255, 24));
                Text(ui, NobleStyle::SmallFont(), 14.0f, ImVec2(s0.x + 12.0f * ui.s, s0.y + 12.0f * ui.s), kDisabled, "OPEN");
                continue;
            }
            const int idx = c.players[row];
            const LobbyPlayer& player = st.lobbyRoster[idx];
            const bool local = isLocal(player);
            const bool selected = st.lobbyRosterSelection == idx;
            PlayerRow(ui, s0, rowW,
                      player.rankIndex >= 0 ? player.rankIndex : (local ? Career::RankIndex(Career::Get().credits) : -1),
                      player.emblemId > 0 ? player.emblemId : (local ? ownProf.emblemIndex : 0),
                      player.nameplateId > 0 ? player.nameplateId : (local ? ownProf.nameplateIndex : 0),
                      player.team,
                      player.serviceTag, (local ? (Settings::Get().Snapshot().playerName.empty() ? std::string("Noble Six") : Settings::Get().Snapshot().playerName) : (player.username.empty() ? std::string(player.isHost ? "Host" : "Spartan") : player.username)), player.isDev, selected,
                      player.emblemBackgroundId > 0 ? player.emblemBackgroundId : (local ? ownProf.emblemBackgroundIndex : 0),
                      local ? UIManager::GetArmorCustomizer().EmblemColorsNow() : UiArt::EmblemColors{});
            if (selected) ui.dl->AddRect(s0, s1, kAccent, 0.0f, 0, 1.5f * ui.s);
            {
                uint64_t machine = 1000ULL + static_cast<uint64_t>(idx);   // demo rosters have no ids; only real ids ever talk
                try { if (!player.machineId.empty()) machine = std::stoull(player.machineId, nullptr, 0); } catch (...) {}
                SocialOverlay::DrawPlayerVoiceIcon(ui.dl, ImVec2(s1.x - 20.0f * ui.s, (s0.y + s1.y) * 0.5f), ui.s, machine, local);
            }
            char slotId[32];
            std::snprintf(slotId, sizeof(slotId), "##lobby_slot_%d", idx);
            ImGui::SetCursorScreenPos(s0);
            if (ImGui::InvisibleButton(slotId, ImVec2(rowW, slotH))) {
                if (st.lobbyRosterSelection == idx) {
                    st.lobbyProfileOpen = !st.lobbyProfileOpen;
                } else {
                    st.lobbyRosterSelection = idx;
                    st.lobbyProfileOpen = true;
                }
                MenuSounds::Scroll();
            }
        }
        if (overflow) {
            const float trackX = c.x + c.w - 3.0f * ui.s;
            ui.dl->AddRectFilled(ImVec2(trackX, listY), ImVec2(trackX + 3.0f * ui.s, listY + listH), IM_COL32(255, 255, 255, 24));
            const float thumbH = std::max(listH * kVisibleRows / c.rows, 18.0f * ui.s);
            const float thumbY = listY + (listH - thumbH) * *c.scroll / std::max(1, c.rows - kVisibleRows);
            ui.dl->AddRectFilled(ImVec2(trackX, thumbY), ImVec2(trackX + 3.0f * ui.s, thumbY + thumbH), kAccent);
        }
    }

    // Back / Cancel Lobby Handling: the host returns to the setup screen of the mode it came from.
    if (back && st.lobbyProfileOpen) {
        st.lobbyProfileOpen = false;
        MenuSounds::Back();
    } else if (back && !pickerWasOpen && !profileWasOpen) {
        if (st.isLobbyHost) {
            MasterServerClient::Get().StopAdvertising();
            SessionManager::Get().StopHosting();
            NetworkTransport::Get().StopRelay();
            st.screen = campaign ? Screen::Campaign : firefight ? Screen::Firefight : forge ? Screen::Forge : Screen::CustomGame;
        } else {
            uint64_t fallbackId = (MasterServerClient::GetLobbyMachineId() & 0x7FFFFFFFULL);
            if (fallbackId < 2ULL) fallbackId = 2ULL;
            uint64_t pId = AuthClient::Get().IsLoggedIn() ? static_cast<uint64_t>(AuthClient::Get().GetUserId()) : fallbackId;
            MasterServerClient::Get().LeaveLobbySync(st.lobbySessionId, pId);
            NetworkTransport::Get().StopRelay();
            st.screen = Screen::Servers;
        }
        st.focusRow = 0;
        MenuSounds::Back();
    }

    std::vector<HintExtra> extras = { { InputPrompts::Button::Y, "F", "Friends" } };
    if (!hostEp.empty()) extras.push_back({ InputPrompts::Button::X, "X", "Copy invite" });
    if (rosterOverflows) extras.push_back({ InputPrompts::Button::RT, "PGDN", "Page" });
    if (rosterCount > 0 && !pickerWasOpen) extras.push_back({ InputPrompts::Button::Start, "TAB", st.lobbyProfileOpen ? "Close card" : "Player card" });
    if (pickerWasOpen) DrawHints(ui, { { Prompt::Accept, "Select" }, { Prompt::Back, "Back" } });
    else DrawHints(ui, { { Prompt::Accept, "Select" }, { Prompt::Back, "Back" }, { Prompt::Tabs, "Players" } }, extras);
}

static void DrawSocialScreen(OverlayContext& ctx, const Ui& ui, float left, float top,
                             FrontEndChoice& choice, bool& done, bool back) {
    (void)left;
    (void)top;
    MenuState& st = ctx.st;

    const float px = ui.origin.x + 32.0f * ui.s;
    const float py = ui.origin.y + 88.0f * ui.s;
    const float panelW = ui.size.x - 64.0f * ui.s;
    const float panelH = ui.origin.y + ui.size.y - py - 60.0f * ui.s;

    ui.dl->AddRectFilled(ui.origin, ImVec2(ui.origin.x + ui.size.x, ui.origin.y + ui.size.y), IM_COL32(4, 8, 12, 225));
    DrawTitle(ui, ImVec2(px, ui.origin.y + 12.0f * ui.s), "COMMUNITY", "SOCIAL");

    // Account Status Badge (top right)
    const bool loggedIn = AuthClient::Get().IsLoggedIn();
    std::string userTag = loggedIn ? AuthClient::Get().GetUsername() : "Not logged in";
    char statBuf[128];
    if (loggedIn) {
        std::snprintf(statBuf, sizeof(statBuf), "ONLINE: %s (ID: %d)", userTag.c_str(), AuthClient::Get().GetUserId());
    } else {
        std::snprintf(statBuf, sizeof(statBuf), "OFFLINE - Auth Required");
    }
    const float tagW = TextWidth(NobleStyle::SmallFont(), 13.0f * ui.s, statBuf) + 24.0f * ui.s;
    const ImVec2 statP0(ui.origin.x + ui.size.x - 32.0f * ui.s - tagW, ui.origin.y + 20.0f * ui.s);
    const ImVec2 statP1(ui.origin.x + ui.size.x - 32.0f * ui.s, ui.origin.y + 48.0f * ui.s);
    ui.dl->AddRectFilled(statP0, statP1, loggedIn ? IM_COL32(18, 36, 24, 220) : IM_COL32(36, 18, 18, 220), 4.0f * ui.s);
    ui.dl->AddRect(statP0, statP1, loggedIn ? IM_COL32(60, 180, 80, 200) : IM_COL32(180, 60, 60, 200), 4.0f * ui.s);
    Text(ui, NobleStyle::SmallFont(), 13.0f, ImVec2(statP0.x + 12.0f * ui.s, statP0.y + 7.0f * ui.s),
         loggedIn ? IM_COL32(120, 240, 140, 240) : IM_COL32(240, 140, 140, 240), statBuf);

    if (!loggedIn) {
        const float actW = 140.0f * ui.s;
        ImGui::SetCursorScreenPos(ImVec2(statP0.x - actW - 12.0f * ui.s, ui.origin.y + 22.0f * ui.s));
        if (ImGui::Button("LOG IN / REGISTER", ImVec2(actW, 26.0f * ui.s))) {
            OpenAccountModal();
            MenuSounds::Click();
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(px, py));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.05f, 0.08f, 0.75f));
    ImGui::BeginChild("##social_panel", ImVec2(panelW, panelH), ImGuiChildFlags_Borders, ImGuiWindowFlags_None);

    // Controller: the D-pad or stick steps through every button on the page in reading order and A presses it. Fields that
    // need typing are left out; those still need a keyboard.
    static int s_padFocus = 0;
    static int s_padTab = -1;
    if (s_padTab != st.socialTab) { s_padTab = st.socialTab; s_padFocus = 0; }
    int padCount = 0;
    const bool padLive = InputPrompts::Current() == InputPrompts::Device::Gamepad;
    const bool padNext = ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) ||
                         ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight);
    const bool padPrev = ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) ||
                         ImGui::IsKeyPressed(ImGuiKey_GamepadLStickUp) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft);
    const bool padAccept = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false);
    if (padNext) { ++s_padFocus; MenuSounds::Scroll(); }
    if (padPrev) { --s_padFocus; MenuSounds::Scroll(); }
    const auto padButton = [&](const char* label, const ImVec2& size) {
        const int me = padCount++;
        const bool pressed = ImGui::Button(label, size);
        const bool focused = padLive && me == s_padFocus;
        if (focused) {
            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), kAccent, 2.0f, 0, 2.0f * ui.s);
            if (padNext || padPrev) ImGui::SetScrollHereY();
        }
        return pressed || (focused && padAccept);
    };

    // 1. Game Invites Card (prominent at top if any exist)
    const auto invites = SocialClient::Get().GetInvites();
    if (!invites.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.14f, 0.20f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9f, 0.7f, 0.2f, 0.8f));
        const float invBoxH = (36.0f * ui.s * static_cast<float>(invites.size())) + 32.0f * ui.s;
        ImGui::BeginChild("##invites_box", ImVec2(0, invBoxH), ImGuiChildFlags_Borders);
        
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "GAME INVITE RECEIVED:");
        for (size_t i = 0; i < invites.size(); ++i) {
            const auto& inv = invites[i];
            ImGui::PushID(static_cast<int>(500 + i));
            ImGui::Bullet();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s invited you to join their game!", inv.senderUsername.c_str());
            ImGui::SameLine();
            if (padButton("JOIN GAME", ImVec2(100.0f * ui.s, 24.0f * ui.s))) {
                choice.play = true;
                choice.host = false;
                choice.connectAddress = inv.hostIp + ":" + std::to_string(inv.hostPort);
                choice.map = "20_sword_slayer";
                choice.mode = "multiplayer";
                SocialClient::Get().ClearInviteAsync(inv.senderUsername);
                done = true;
                MenuSounds::Click();
            }
            ImGui::SameLine();
            if (padButton("DISMISS", ImVec2(80.0f * ui.s, 24.0f * ui.s))) {
                SocialClient::Get().ClearInviteAsync(inv.senderUsername);
                MenuSounds::Back();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::Spacing();
    }

    // 2. Navigation Tabs
    auto friends = SocialClient::Get().GetFriends();
    static const bool s_friendsDemo = wcsstr(GetCommandLineW(), L"-social-friends-demo") != nullptr;   // test: rows without an account
    if (s_friendsDemo && friends.empty()) {
        friends.emplace_back();
        friends.back().username = "Kat-B320";
        friends.emplace_back();
        friends.back().username = "Carter-A259";
    }
    const auto incoming = SocialClient::Get().GetIncomingRequests();
    const auto outgoing = SocialClient::Get().GetOutgoingRequests();

    // Tab switcher with Q/E
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Q, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false)) {
            st.socialTab = (st.socialTab + 2) % 3;
            MenuSounds::Click();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)) {
            st.socialTab = (st.socialTab + 1) % 3;
            MenuSounds::Click();
        }
    }

    const float tabW = 180.0f * ui.s;
    const float tabH = 32.0f * ui.s;

    auto TabButton = [&](int tabIdx, const std::string& label) {
        const bool active = (st.socialTab == tabIdx);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.7f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.12f, 0.16f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.8f, 0.9f, 0.8f));
        }
        if (ImGui::Button(label.c_str(), ImVec2(tabW, tabH))) {
            st.socialTab = tabIdx;
            MenuSounds::Click();
        }
        ImGui::PopStyleColor(2);
    };

    TabButton(0, "FRIENDS (" + std::to_string(friends.size()) + ")");
    ImGui::SameLine();
    TabButton(1, "REQUESTS (" + std::to_string(incoming.size() + outgoing.size()) + ")");
    ImGui::SameLine();
    TabButton(2, "FIND PLAYERS");
    ImGui::SameLine();

    // Refresh button
    if (padButton("REFRESH", ImVec2(100.0f * ui.s, tabH))) {
        SocialClient::Get().RefreshFriendsAsync();
        SocialClient::Get().PollInvitesAsync();
        MenuSounds::Click();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // 3. Tab Contents
    if (st.socialTab == 0) {
        // FRIENDS TAB: Quick Add Friend by Name or IP
        static char s_quickAddBuf[64] = "";
        ImGui::SetNextItemWidth(300.0f * ui.s);
        bool enterAdd = ImGui::InputTextWithHint("##quick_add_friend", "Enter player name or IP address...", s_quickAddBuf, sizeof(s_quickAddBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("ADD FRIEND", ImVec2(120.0f * ui.s, 24.0f * ui.s)) || enterAdd) && s_quickAddBuf[0] != '\0') {
            SocialClient::Get().AddFriendDirect(s_quickAddBuf);
            s_quickAddBuf[0] = '\0';
            MenuSounds::Click();
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (friends.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.8f, 1.0f), "You have no friends on your list yet.");
            ImGui::TextColored(ImVec4(0.5f, 0.6f, 0.7f, 0.8f), "Type a player name or IP above and click ADD FRIEND, or use the FIND PLAYERS tab!");
        } else {
            std::string myIp = UPnP::Get().GetExternalIP();
            if (myIp.empty() || myIp == "Unknown") myIp = "127.0.0.1";

            ImGui::BeginTable("##friends_tbl", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV);
            ImGui::TableSetupColumn("Spartan / Sangheili", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 140.0f * ui.s);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 250.0f * ui.s);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < friends.size(); ++i) {
                const auto& f = friends[i];
                ImGui::TableNextRow(0, 32.0f * ui.s);

                // Col 0: Name
                ImGui::TableSetColumnIndex(0);
                if (!f.endpoint.empty()) {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", f.username.c_str());   // never the address: this ends up in screenshots and streams
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", f.username.c_str());
                }

                // Col 1: Status
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), "Online / Friend");

                // Col 2: Actions
                ImGui::TableSetColumnIndex(2);
                ImGui::PushID(static_cast<int>(i));

                if (padButton("INVITE", ImVec2(75.0f * ui.s, 24.0f * ui.s))) {
                    std::string invMode = "Custom Game";
                    if (st.lobbyMode == "campaign") invMode = "Campaign";
                    else if (st.lobbyMode == "firefight") invMode = "Firefight";
                    else if (st.lobbyMode == "forge") invMode = "Forge";
                    if (s_friendsDemo) NOBLE_INFO("FrontEndMenu: -social-friends-demo INVITE pressed for {}", f.username);
                    else SocialClient::Get().SendInviteAsync(f.username, myIp, 11774, [](bool ok, const std::string& msg) {
                        (void)msg;
                        if (ok) MenuSounds::Click();
                    }, invMode, st.lobbyMap, st.lobbySessionId);
                }
                ImGui::SameLine();
                if (!f.endpoint.empty()) {
                    if (padButton("JOIN", ImVec2(75.0f * ui.s, 24.0f * ui.s))) {
                        choice.play = true;
                        choice.host = false;
                        choice.connectAddress = f.endpoint;
                        done = true;
                        MenuSounds::Click();
                    }
                    ImGui::SameLine();
                }
                if (padButton("REMOVE", ImVec2(75.0f * ui.s, 24.0f * ui.s)) && !s_friendsDemo) {
                    SocialClient::Get().RemoveFriendAsync(f.username);
                    MenuSounds::Back();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    } else if (st.socialTab == 1) {
        // REQUESTS TAB
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.4f, 1.0f), "INCOMING REQUESTS (%zu)", incoming.size());
        if (incoming.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.6f, 0.7f, 0.8f), "No incoming friend requests.");
        } else {
            for (size_t i = 0; i < incoming.size(); ++i) {
                const auto& req = incoming[i];
                ImGui::PushID(static_cast<int>(100 + i));
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s wants to be your friend.", req.username.c_str());
                ImGui::SameLine();
                if (padButton("ACCEPT", ImVec2(80.0f * ui.s, 22.0f * ui.s))) {
                    SocialClient::Get().AcceptFriendRequestAsync(req.username);
                    MenuSounds::Click();
                }
                ImGui::SameLine();
                if (padButton("DECLINE", ImVec2(80.0f * ui.s, 22.0f * ui.s))) {
                    SocialClient::Get().RemoveFriendAsync(req.username);
                    MenuSounds::Back();
                }
                ImGui::PopID();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1.0f), "OUTGOING REQUESTS (%zu)", outgoing.size());
        if (outgoing.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.6f, 0.7f, 0.8f), "No pending outgoing requests.");
        } else {
            for (size_t i = 0; i < outgoing.size(); ++i) {
                const auto& req = outgoing[i];
                ImGui::PushID(static_cast<int>(200 + i));
                ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "%s (Pending acceptance)", req.username.c_str());
                ImGui::SameLine();
                if (padButton("CANCEL", ImVec2(80.0f * ui.s, 22.0f * ui.s))) {
                    SocialClient::Get().RemoveFriendAsync(req.username);
                    MenuSounds::Back();
                }
                ImGui::PopID();
            }
        }
    } else if (st.socialTab == 2) {
        // FIND PLAYERS TAB
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Search for community players by username:");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(320.0f * ui.s);
        bool enterPressed = ImGui::InputTextWithHint("##search_player", "Enter player name...", st.socialSearchBuf, sizeof(st.socialSearchBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        bool doSearch = ImGui::Button("SEARCH", ImVec2(100.0f * ui.s, 26.0f * ui.s)) || enterPressed;

        if (doSearch && st.socialSearchBuf[0] != '\0') {
            st.socialSearching = true;
            std::string q = st.socialSearchBuf;
            SocialClient::Get().SearchUsersAsync(q, [&st](bool ok, const std::vector<UserSearchResult>& results) {
                (void)ok;
                st.socialSearchResults = results;
                st.socialSearching = false;
            });
            MenuSounds::Click();
        }

        ImGui::Spacing();
        if (st.socialSearching) {
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Searching players...");
        } else if (!st.socialSearchResults.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "SEARCH RESULTS (%zu):", st.socialSearchResults.size());
            ImGui::Spacing();

            for (size_t i = 0; i < st.socialSearchResults.size(); ++i) {
                const auto& u = st.socialSearchResults[i];
                ImGui::PushID(static_cast<int>(300 + i));
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s (ID: %d)", u.username.c_str(), u.id);
                ImGui::SameLine();
                if (padButton("ADD FRIEND", ImVec2(110.0f * ui.s, 22.0f * ui.s))) {
                    SocialClient::Get().SendFriendRequestAsync(u.username);
                    MenuSounds::Click();
                }
                ImGui::PopID();
            }
        } else if (st.socialSearchBuf[0] != '\0') {
            ImGui::TextColored(ImVec4(0.7f, 0.6f, 0.5f, 1.0f), "No players found matching '%s'.", st.socialSearchBuf);
        }
    }

    if (padCount > 0) s_padFocus = (s_padFocus % padCount + padCount) % padCount;
    // Status message feedback
    std::string statusMsg = SocialClient::Get().GetLastStatusMessage();
    if (!statusMsg.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "INFO: %s", statusMsg.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (back && !ImGui::GetIO().WantTextInput) {
        st.screen = st.socialPreviousScreen;
        st.focusRow = 0;
        MenuSounds::Back();
    }
    DrawHints(ui, { { Prompt::Accept, "Select" }, { Prompt::Back, "Back" }, { Prompt::Tabs, "Switch Tab" } }, {}, 0.01875f);   // the full-width panel's edge
}

// Shown once, the first time Noble starts (spec 4.12). New players get a short setup; anyone else goes straight to the menu.
// Noble has no graphics presets, so the spec's preset step is left out rather than offering one that does nothing.
// Returns true while it's up, and then the menu behind it takes no input.
bool DrawFirstStart(const Ui& ui) {
    static const bool s_forced = wcsstr(GetCommandLineW(), L"-first-start") != nullptr;
    static bool s_forcedDone = false;
    const SettingsData saved = Settings::Get().Snapshot();
    if (saved.firstStartDone && (!s_forced || s_forcedDone)) return false;

    static int s_step = 0, s_focus = 0;
    static bool s_controller = false, s_loaded = false;
    static float s_sensitivity = 1.0f;
    static char s_name[16] = "", s_tag[5] = "";
    if (!s_loaded) {
        s_loaded = true;
        std::snprintf(s_name, sizeof(s_name), "%s", saved.playerName.c_str());
        std::snprintf(s_tag, sizeof(s_tag), "%s", saved.serviceTag.c_str());
    }
    const auto finish = [&](bool setup) {
        SettingsData d = Settings::Get().Snapshot();
        d.firstStartDone = true;
        if (setup) {
            if (s_name[0]) d.playerName = s_name;
            if (s_tag[0]) d.serviceTag = s_tag;
            if (s_controller) d.padLookSpeedX = d.padLookSpeedY = s_sensitivity;
            else d.mouseSensitivity = s_sensitivity;
        }
        Settings::Get().Update(d);
        s_forcedDone = true;
        s_step = 0;
        s_focus = 0;
        NOBLE_INFO("FrontEndMenu: first start answered (setup: {})", setup);
        MenuSounds::Click();
    };

    ImGuiIO& io = ImGui::GetIO();
    const float s = ui.s;
    const ImVec2 size(720.0f * s, 440.0f * s);
    ImGui::SetNextWindowPos(ImVec2(ui.origin.x + (ui.size.x - size.x) * 0.5f, ui.origin.y + (ui.size.y - size.y) * 0.5f));
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowFocus();
    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::Begin("##firststart", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
    const bool typing = io.WantTextInput;
    const bool up = !typing && (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickUp));
    const bool down = !typing && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown));
    const bool left = !typing && (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft));
    const bool right = !typing && (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight));
    const bool accept = !typing && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false));
    const bool back = !typing && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false));

    const auto heading = [&](const char* title, const char* text) {
        ImGui::Dummy(ImVec2(0, 10.0f * s));
        ImGui::PushFont(NobleStyle::TitleFont());
        ImGui::TextUnformatted(title);
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 6.0f * s));
        ImGui::PushStyleColor(ImGuiCol_Text, NobleStyle::TextMuted());
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 14.0f * s));
    };
    int rows = 2;
    const auto option = [&](int index, const char* label) {
        const bool focused = s_focus == index;
        ImGui::PushStyleColor(ImGuiCol_Button, focused ? ImVec4(0.20f, 0.45f, 0.70f, 0.95f) : ImVec4(0.08f, 0.12f, 0.16f, 0.9f));
        const bool clicked = ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 44.0f * s));
        ImGui::PopStyleColor();
        return clicked || (accept && focused);
    };
    const auto nextStep = [&](int step) { s_step = step; s_focus = 0; MenuSounds::Click(); };

    if (s_step == 0) {
        heading("NEW TO HALO: REACH?", "This is only asked once. You can run the setup again from Settings > System.");
        if (option(0, "NEW PLAYER")) nextStep(1);
        if (option(1, "REGULAR OR RETURNING PLAYER")) finish(false);
    } else if (s_step == 1) {
        heading("HOW WILL YOU PLAY?", "Both always work, and Noble switches to whichever you touched last. This picks which look speed to set up next.");
        const auto choose = [&](bool controller) {
            s_controller = controller;
            s_sensitivity = controller ? saved.padLookSpeedX : saved.mouseSensitivity;
            nextStep(2);
        };
        if (option(0, "KEYBOARD AND MOUSE")) choose(false);
        if (option(1, "CONTROLLER")) choose(true);
    } else if (s_step == 2) {
        rows = 1;
        heading("YOUR NAME", "Shown on scoreboards and in chat. Typing needs a keyboard; on a controller, CONTINUE keeps what's here.");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
        const bool nameDone = ImGui::InputTextWithHint("Name", "Up to 15 characters", s_name, sizeof(s_name), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.3f);
        const bool tagDone = ImGui::InputTextWithHint("Service tag", "4 letters or digits", s_tag, sizeof(s_tag), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
        ImGui::Dummy(ImVec2(0, 10.0f * s));
        if (option(0, "CONTINUE") || nameDone || tagDone) nextStep(3);
    } else {
        rows = 1;
        heading("LOOK SPEED", s_controller ? "Left and right change how fast you turn. 2.00x is Reach's own speed." : "Left and right change how far you turn per mouse movement. 1.00x is Noble's default.");
        const float lo = s_controller ? 1.0f : 0.1f, hi = s_controller ? 10.0f : 5.0f, step = s_controller ? 0.25f : 0.05f;
        if (left) s_sensitivity = std::clamp(s_sensitivity - step, lo, hi);
        if (right) s_sensitivity = std::clamp(s_sensitivity + step, lo, hi);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::SliderFloat("##firststartlook", &s_sensitivity, lo, hi, "%.2fx");
        ImGui::Dummy(ImVec2(0, 10.0f * s));
        if (option(0, "FINISH")) finish(true);
    }
    if (up) s_focus = (s_focus + rows - 1) % rows;
    if (down) s_focus = (s_focus + 1) % rows;
    if (back && s_step > 0) {
        s_focus = 0;
        --s_step;
        MenuSounds::Back();
    }
    ImGui::End();
    return true;
}

// Menu music player: a compact now-playing strip (track, prev / play-pause / next, mute, shuffle) in the
// bottom-right of the main menu. Every action takes effect immediately and is saved to settings.json, the
// same rule the Settings > Audio page uses for the music track.
void DrawMusicPlayer(const Ui& ui) {
    const SettingsData sd = Settings::Get().Snapshot();
    // MusicTracks() builds the whole list, and this runs every frame on the main menu: build it once.
    static const std::vector<ReachAudio::TrackInfo> tracks = ReachAudio::MusicTracks();
    if (tracks.empty()) return;

    const std::string id = sd.menuMusicTrack;
    int index = -1;
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].id == id) { index = static_cast<int>(i); break; }
    }

    std::string label;
    if (id == "all") label = "Shuffle \xC2\xB7 every game";
    else if (id == "off") label = "Music off";
    else if (index >= 0) label = tracks[index].game + " \xC2\xB7 " + tracks[index].label;
    else label = ReachAudio::TrackLabel(id);
    if (ReachAudio::IsMusicPaused()) label += "   (paused)";

    auto saveTrack = [&](const std::string& next) {
        SettingsData d = Settings::Get().Snapshot();
        d.menuMusicTrack = next;
        Settings::Get().Update(d);
        ReachAudio::PlayMenuMusic(next);
    };
    auto step = [&](int dir) {
        if (tracks.empty()) return;
        int from = index >= 0 ? index : 0;
        from = (from + dir + static_cast<int>(tracks.size())) % static_cast<int>(tracks.size());
        saveTrack(tracks[from].id);
    };

    const float w = 470.0f * ui.s, h = 30.0f * ui.s;
    const ImVec2 p0(ui.origin.x + ui.size.x - w - 40.0f * ui.s, ui.origin.y + ui.size.y - 140.0f * ui.s);
    const ImVec2 p1(p0.x + w, p0.y + h);
    ui.dl->AddRectFilled(p0, p1, IM_COL32(8, 12, 18, 190));
    ui.dl->AddRect(p0, p1, IM_COL32(90, 120, 150, 90), 0.0f, 0, 1.0f);

    // A small note glyph, then the track name (clipped if the file names are long).
    const ImVec2 note(p0.x + 14.0f * ui.s, p0.y + h * 0.5f);
    ui.dl->AddCircleFilled(ImVec2(note.x - 2.0f * ui.s, note.y + 4.0f * ui.s), 3.5f * ui.s, kAccent);
    ui.dl->AddLine(ImVec2(note.x + 1.5f * ui.s, note.y + 4.0f * ui.s), ImVec2(note.x + 1.5f * ui.s, note.y - 7.0f * ui.s), kAccent, 1.6f * ui.s);
    ui.dl->AddLine(ImVec2(note.x + 1.5f * ui.s, note.y - 7.0f * ui.s), ImVec2(note.x + 9.0f * ui.s, note.y - 9.0f * ui.s), kAccent, 1.6f * ui.s);

    const float buttonsW = 186.0f * ui.s;   // five 34 s buttons with 4 s gaps
    const float textRight = p1.x - buttonsW - 10.0f * ui.s;
    const std::string shown = Upper(label);
    ui.dl->PushClipRect(ImVec2(p0.x + 30.0f * ui.s, p0.y), ImVec2(textRight, p1.y), true);
    Text(ui, NobleStyle::SmallFont(), 12.5f, ImVec2(p0.x + 30.0f * ui.s, p0.y + 8.0f * ui.s),
         ReachAudio::IsMusicPaused() ? kDim : IM_COL32(205, 220, 235, 235), shown.c_str());
    ui.dl->PopClipRect();

    const char* glyphs[5] = { "|<<", "||>", ">>|", "VOL", "SHF" };   // prev, play/pause, next, mute, shuffle
    if (ReachAudio::IsMusicPaused()) glyphs[1] = ">  ";
    if (s_menuMusicMuted) glyphs[3] = "MUT";
    float bx = textRight + 6.0f * ui.s;
    const float bw = 34.0f * ui.s;
    for (int i = 0; i < 5; ++i) {
        const ImVec2 a(bx + i * (bw + 4.0f * ui.s), p0.y + 4.0f * ui.s);
        const ImVec2 b(a.x + bw, p0.y + h - 4.0f * ui.s);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(3000 + i);
        const bool pressed = ImGui::InvisibleButton("##music", ImVec2(bw, h - 8.0f * ui.s));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool active = (i == 1 && ReachAudio::IsMusicPaused()) || (i == 3 && s_menuMusicMuted) || (i == 4 && id == "all");
        ui.dl->AddRectFilled(a, b, active ? IM_COL32(52, 112, 156, 230)
                                          : (hovered ? IM_COL32(40, 56, 72, 230) : IM_COL32(18, 24, 32, 220)));
        ui.dl->AddRect(a, b, hovered ? IM_COL32(170, 220, 255, 180) : IM_COL32(80, 110, 140, 90), 0.0f, 0, 1.0f);
        const float gw = TextWidth(NobleStyle::SmallFont(), 11.0f * ui.s, glyphs[i]);
        Text(ui, NobleStyle::SmallFont(), 11.0f, ImVec2(a.x + (bw - gw) * 0.5f, a.y + 4.0f * ui.s), kWhite, glyphs[i]);
        if (!pressed) continue;
        MenuSounds::Click();
        switch (i) {
        case 0: step(-1); break;
        case 1:
            if (ReachAudio::IsMusicPaused()) ReachAudio::ResumeMusic();
            else if (!ReachAudio::PauseMusic() || id == "off") saveTrack(index >= 0 ? tracks[index].id : "reach:menu");
            break;
        case 2: step(1); break;
        case 3:
            s_menuMusicMuted = !s_menuMusicMuted;
            ReachAudio::SetMuted(s_menuMusicMuted);
            break;
        case 4: saveTrack(id == "all" ? (index >= 0 ? tracks[index].id : "reach:menu") : "all"); break;
        default: break;
        }
    }
}

// A friend's game invite is shown on every menu screen as a small panel with JOIN / DISMISS, unless the
// player turned invites off in Settings > Online services. The invite carries the host endpoint, so JOIN is
// the same relaunch the server browser uses for a direct connect (2026-10-30).
void DrawInviteToast(const Ui& ui, MenuState& st) {
    if (st.confirmQuit || s_showAccountModal || s_showDevMenu || ModDownloadPrompt::Get().IsActive()) return;
    if (!Settings::Get().Snapshot().inviteNotifications) return;
    if (!AuthClient::Get().IsLoggedIn()) return;

    static double s_lastPoll = 0.0;
    const double now = ImGui::GetTime();
    if (now - s_lastPoll > 4.0) {
        s_lastPoll = now;
        SocialClient::Get().PollInvitesAsync();
    }

    const std::vector<GameInvite> invites = SocialClient::Get().GetInvites();
    if (invites.empty()) return;
    const GameInvite* newest = &invites.front();
    for (const GameInvite& inv : invites) {
        if (inv.timestamp >= newest->timestamp) newest = &inv;
    }

    static std::string s_answered;   // "sender|timestamp" already answered this session
    char key[160];
    std::snprintf(key, sizeof(key), "%s|%llu", newest->senderUsername.c_str(),
                  static_cast<unsigned long long>(newest->timestamp));
    if (s_answered == key) return;

    const float w = 470.0f * ui.s, h = 96.0f * ui.s;
    const ImVec2 p0(ui.origin.x + ui.size.x - w - 40.0f * ui.s, ui.origin.y + ui.size.y - h - 150.0f * ui.s);
    const ImVec2 p1(p0.x + w, p0.y + h);
    ui.dl->AddRectFilled(p0, p1, IM_COL32(12, 20, 30, 245));
    ui.dl->AddRect(p0, p1, IM_COL32(143, 216, 255, 170), 0.0f, 0, 1.2f);
    ui.dl->AddLine(ImVec2(p0.x, p0.y + 3.0f * ui.s), ImVec2(p1.x, p0.y + 3.0f * ui.s), kAccent, 3.0f * ui.s);

    Text(ui, NobleStyle::SmallFont(), 12.0f, ImVec2(p0.x + 16.0f * ui.s, p0.y + 11.0f * ui.s), kAccent, "GAME INVITE");
    Text(ui, NobleStyle::TitleFont(), 19.0f, ImVec2(p0.x + 16.0f * ui.s, p0.y + 30.0f * ui.s), kWhite,
         newest->senderUsername.c_str());
    const float nameW = TextWidth(NobleStyle::TitleFont(), 19.0f * ui.s, newest->senderUsername.c_str());
    const std::string inviteDesc = newest->mode.empty()
                                       ? "is inviting you to a match"
                                       : ("is inviting you to a " + newest->mode + " match");
    Text(ui, nullptr, 15.0f, ImVec2(p0.x + 16.0f * ui.s + nameW + 8.0f * ui.s, p0.y + 36.0f * ui.s), kDim,
         inviteDesc.c_str());

    const float bw = 160.0f * ui.s, bh = 30.0f * ui.s;
    const ImVec2 j0(p0.x + w - bw * 2.0f - 26.0f * ui.s, p1.y - bh - 12.0f * ui.s);
    const ImVec2 d0(j0.x + bw + 10.0f * ui.s, j0.y);
    auto button = [&](const char* id, ImVec2 a, const char* label, bool primary) {
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(id);
        const bool pressed = ImGui::InvisibleButton("##b", ImVec2(bw, bh));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const ImVec2 b(a.x + bw, a.y + bh);
        ui.dl->AddRectFilled(a, b, primary ? (hovered ? IM_COL32(76, 150, 200, 255) : IM_COL32(52, 112, 156, 240))
                                           : (hovered ? IM_COL32(60, 74, 88, 240) : IM_COL32(26, 32, 40, 230)));
        ui.dl->AddRect(a, b, primary ? IM_COL32(170, 220, 255, hovered ? 220 : 90) : IM_COL32(120, 140, 160, 90));
        const float tw = TextWidth(NobleStyle::TitleFont(), 16.0f * ui.s, label);
        Text(ui, NobleStyle::TitleFont(), 16.0f, ImVec2(a.x + (bw - tw) * 0.5f, a.y + 5.0f * ui.s), kWhite, label);
        if (pressed) MenuSounds::Click();
        return pressed;
    };

    if (button("invite_join", j0, newest->hostIp.empty() ? "WAITING..." : "JOIN", true)) {
        if (newest->hostIp.empty()) {
            // The server sent an invite without an endpoint (older server build): do not relaunch with "-connect :"
            NOBLE_WARN("FrontEndMenu: invite from {} has no host address; ignoring JOIN", newest->senderUsername);
        } else {
            s_answered = key;
            SocialClient::Get().ClearInviteAsync(newest->senderUsername);
            SocialClient::Get().RefreshFriendsAsync();
            NOBLE_INFO("FrontEndMenu: accepting invite from {} ({}, mode='{}')",
                       newest->senderUsername, newest->hostIp, newest->mode);
            std::wstring args = L"-connect \"" + ToWide(newest->hostIp + ":" + std::to_string(newest->hostPort)) + L"\"";
            if (!newest->sessionId.empty()) {
                args += L" -session-id \"" + ToWide(newest->sessionId) + L"\"";
            }
            Relaunch::HandOff(args);
        }
    }
    if (button("invite_dismiss", d0, "DISMISS", false)) {
        s_answered = key;
        SocialClient::Get().ClearInviteAsync(newest->senderUsername);
        SocialClient::Get().RefreshFriendsAsync();
    }
}

void DrawFrontEndContents(OverlayContext& ctx, ID3D11Device* device, ID3D11DeviceContext* context,
                          FrontEndChoice& choice, bool& done) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoScrollWithMouse;
    // These screens read the pad and keyboard themselves. With ImGui's own navigation on as well, its invisible
    // cursor sat on some other button and one A press could act twice (seen on the lobby's COPY INVITE, 2026-10-31).
    // The panel screens below keep it: their lists and buttons are plain ImGui widgets.
    if (ctx.st.screen != Screen::Servers && ctx.st.screen != Screen::Career && ctx.st.screen != Screen::Mods && ctx.st.screen != Screen::Social) {
        flags |= ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
    }
    ImGui::Begin("##NobleFrontEnd", nullptr, flags);
    ImGui::PopStyleVar(2);

    MenuState& st = ctx.st;
    {
        static int s_lastScreen = -1;
        if (static_cast<int>(st.screen) != s_lastScreen) {
            NOBLE_INFO("FrontEndMenu: screen {} -> {}", s_lastScreen, static_cast<int>(st.screen));
            s_lastScreen = static_cast<int>(st.screen);
        }
    }
    Ui ui;
    ui.dl = ImGui::GetWindowDrawList();
    constexpr float kAspect = 16.0f / 9.0f;
    const float currentAspect = (vp->Size.y > 0.0f) ? (vp->Size.x / vp->Size.y) : kAspect;
    if (std::abs(currentAspect - kAspect) < 0.01f) {
        ui.origin = ImVec2(0.0f, 0.0f);
        ui.size = vp->Size;
    } else if (currentAspect > kAspect) {
        // Wider than 16:9: pillarbox with black bars left/right
        const float targetW = vp->Size.y * kAspect;
        ui.origin = ImVec2((vp->Size.x - targetW) * 0.5f, 0.0f);
        ui.size = ImVec2(targetW, vp->Size.y);
    } else {
        // Taller than 16:9: letterbox with black bars top/bottom
        const float targetH = vp->Size.x / kAspect;
        ui.origin = ImVec2(0.0f, (vp->Size.y - targetH) * 0.5f);
        ui.size = ImVec2(vp->Size.x, targetH);
    }
    ui.s = std::clamp(ui.size.y / 900.0f, 0.75f, 4.0f);   // 4K back buffer = 2.4 (was capped at 2.0)

    s_lobbyBackdropMap = st.lobbyMap;
    s_lobbyBackdropImage = st.lobbyMapImage.empty() ? st.lobbyMapThumb : st.lobbyMapImage;
    DrawBackdrop(ui, st.screen);
    DrawAccountModal(ui);
    DrawDevMenuModal(ui);

    UpdateSceneForScreen(st.screen);

    const float left = ui.size.x * 0.075f;
    const float top = ui.size.y * 0.09f;
    const SettingsData settings = Settings::Get().Snapshot();
    s_hintThisFrame = std::exchange(s_hintClicked, -1);
    const bool modPromptOpen = ModDownloadPrompt::Get().IsActive();
    // Esc in a text box only leaves the box.
    const bool back = !modPromptOpen && !ImGui::GetIO().WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                      ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) || HintClicked(Prompt::Back));
    auto goBack = [&](Screen to) {
        st.screen = to;
        st.focusRow = 0;
        st.mapSearch[0] = 0;
        MenuSounds::Back();
    };
    auto saveSkulls = [&] {
        SettingsData d = Settings::Get().Snapshot();
        d.campaignSkulls = st.skulls;
        Settings::Get().Update(d);
    };

    switch (st.screen) {
    case Screen::Main: {
        // While the quit box or a modal is open nothing behind it reacts to the mouse (no hover, sounds or clicks).
        const bool firstStart = DrawFirstStart(ui);
        if (firstStart) ImGui::GetIO().ClearInputKeys();
        const bool modal = firstStart || st.confirmQuit || s_showAccountModal || s_showDevMenu || modPromptOpen;
        ImGui::BeginDisabled(modal);
        DrawParty(ui, settings.playerName, "In Main Menu Lobby");
        if (DrawUpdateBanner(ui)) done = true;   // update script takes over after this frame
        const float titleY = ui.size.y * 0.16f;
        DrawTitle(ui, ImVec2(left, titleY), "MAIN MENU");
        Text(ui, nullptr, 17.0f, ImVec2(left + 4.0f * ui.s, titleY + 66.0f * ui.s), kDim,
             "This party is local to this PC.");
        // The list is built per frame so the developer row can sit above Quit without shifting indices.
        std::vector<const MainEntry*> rows;
        const SettingsData mainSnap = Settings::Get().Snapshot();
        const bool canContinue = mainSnap.lastPlayedMode == "campaign" && !mainSnap.lastPlayedMap.empty();
        if (canContinue) rows.push_back(&kContinueEntry);
        for (const MainEntry& e : kMainEntries) {
            if (e.quit && AuthClient::Get().IsDeveloper()) rows.push_back(&kDeveloperEntry);
            rows.push_back(&e);
        }
        if (rows.empty()) rows.push_back(&kMainEntries[0]);
        st.mainIndex = std::clamp(st.mainIndex, 0, static_cast<int>(rows.size()) - 1);
        std::vector<ListItem> items;
        for (const MainEntry* e : rows) items.push_back({ e->label, e->enabled });

        // Fit the list, its description and the hint row: shrink the row spacing first, then lift the list,
        // so the description can never be drawn on top of "ENTER Select / ESC Quit" (2026-10-30).
        const float descWidth = 430.0f * ui.s;
        const char* desc = rows[st.mainIndex]->description;
        // Reserve two lines whatever the selected entry says. The layout used to be measured from the
        // *selected* description, so a one-line entry and a two-line entry put the list at different
        // heights and moving the cursor made the whole menu jump (reported 2026-10-30).
        const float descH = Font(nullptr)->CalcTextSizeA(17.0f * ui.s, FLT_MAX, 0.0f, "Ag").y * 2.0f + 2.0f * ui.s;
        const float descGap = 10.0f * ui.s;
        const float hintTop = ui.size.y - 54.0f * ui.s;
        float listY = titleY + 96.0f * ui.s;   // below the "This party is local" line at +66 (+76 overlapped it)
        float step = 26.0f;                                             // ui.s units
        const float rowCount = static_cast<float>(rows.size());
        auto blockHeight = [&](float s) { return rowCount * s * ui.s + descGap + descH + 8.0f * ui.s; };
        if (blockHeight(step) > hintTop - listY) {
            step = std::max(18.0f, (hintTop - listY - descGap - descH - 8.0f * ui.s) / (rowCount * ui.s));
            if (blockHeight(step) > hintTop - listY) {
                listY = std::max(ui.size.y * 0.14f, hintTop - blockHeight(step));
            }
        }
        const int hit = DrawList(ui, "main", ImVec2(left, listY), 440.0f * ui.s, items, st.mainIndex, !modal, step);
        ImGui::EndDisabled();
        const float descY = listY + rowCount * step * ui.s + descGap;
        std::string descLine = desc;
        if (rows[st.mainIndex] == &kContinueEntry) {
            std::string mapName = mainSnap.lastPlayedMap;
            for (const MenuMap& m : kMissions) if (m.scenario == mapName) { mapName = m.name; break; }
            const char* difficulty = kDifficulties[std::clamp(mainSnap.lastPlayedDifficulty, 0, 3)];
            // Honest about what it does: Noble has no save/checkpoint resume, so the mission starts again.
            descLine = "Continue the campaign: " + mapName + " on " + difficulty + " (starts the mission again).";
        }
        Text(ui, nullptr, 17.0f, ImVec2(left + 4.0f * ui.s, descY), kDim, descLine.c_str(), left + descWidth);
        if (!modal) {
            if (hit >= 0) {
                const MainEntry* picked = rows[hit];
                if (picked->quit) {
                    st.confirmQuit = true;
                    st.quitChoice = 1;
                } else if (picked == &kContinueEntry) {
                    choice.play = true;
                    choice.host = false;
                    choice.map = mainSnap.lastPlayedMap;
                    choice.mode = "campaign";   // CONTINUE is campaign-only by design
                    choice.variantPath = mainSnap.lastPlayedVariant;
                    choice.difficulty = mainSnap.lastPlayedDifficulty;
                    choice.skulls = mainSnap.lastPlayedSkulls;
                    // The mod may have been removed since; a stale folder would make the new process fail to
                    // launch, so fall back to the vanilla game instead.
                    if (!mainSnap.lastPlayedMod.empty() && std::filesystem::exists(mainSnap.lastPlayedMod)) {
                        choice.modFolder = mainSnap.lastPlayedMod;
                    }
                    done = true;
                } else if (picked == &kDeveloperEntry) {
                    OpenDevMenuModal();
                } else if (std::string(picked->label) == "Account") {
                    OpenAccountModal();
                } else {
                    st.screen = picked->target;
                    if (st.screen == Screen::Social) {
                        st.socialPreviousScreen = Screen::Main;
                    }
                }
                st.focusRow = 0;
            } else if (back) {
                // Esc on the main menu asks before closing Noble.
                st.confirmQuit = true;
                st.quitChoice = 1;
                MenuSounds::Back();
            }
            DrawHints(ui, { { Prompt::Accept, "Select" }, { Prompt::Back, "Quit" } });
        } else if (st.confirmQuit) {
            // Dim everything and show the question in the middle.
            ui.dl->AddRectFilled(ui.origin, ImVec2(ui.origin.x + ui.size.x, ui.origin.y + ui.size.y), IM_COL32(0, 0, 0, 150));
            const float bw = 460.0f * ui.s, bh = 190.0f * ui.s;
            const ImVec2 b0(ui.origin.x + (ui.size.x - bw) * 0.5f, ui.origin.y + (ui.size.y - bh) * 0.5f);
            const ImVec2 b1(b0.x + bw, b0.y + bh);
            ui.dl->AddRectFilled(b0, b1, IM_COL32(12, 20, 30, 245));
            ui.dl->AddRect(b0, b1, IM_COL32(143, 216, 255, 160), 0.0f, 0, 1.2f);
            ui.dl->AddLine(ImVec2(b0.x, b0.y + 3.0f * ui.s), ImVec2(b1.x, b0.y + 3.0f * ui.s), kAccent, 3.0f * ui.s);
            Text(ui, NobleStyle::TitleFont(), 26.0f, ImVec2(b0.x + 24.0f * ui.s, b0.y + 22.0f * ui.s), kWhite, "QUIT PROJECT NOBLE?");
            Text(ui, nullptr, 17.0f, ImVec2(b0.x + 24.0f * ui.s, b0.y + 62.0f * ui.s), kDim, "Are you sure you want to close the game?");
            const float btnW = (bw - 24.0f * ui.s * 3.0f) * 0.5f;
            const float btnY = b1.y - 64.0f * ui.s;
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow) ||
                ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow) ||
                ImGui::IsKeyPressed(ImGuiKey_Tab) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight)) {
                st.quitChoice = 1 - st.quitChoice;
                MenuSounds::Scroll();
            }
            const bool yes = ActionButton(ui, "quit_yes", ImVec2(b0.x + 24.0f * ui.s, btnY), btnW, "YES", st.quitChoice == 0);
            const bool no = ActionButton(ui, "quit_no", ImVec2(b0.x + 48.0f * ui.s + btnW, btnY), btnW, "NO", st.quitChoice == 1);
            if (yes || (AcceptPressed() && st.quitChoice == 0)) {
                st.confirmQuit = false;
                done = true;
            } else if (no || back || (AcceptPressed() && st.quitChoice == 1)) {
                st.confirmQuit = false;
                if (!no) MenuSounds::Back();
            }
            DrawHints(ui, { { Prompt::Accept, "Select" }, { Prompt::Back, "Cancel" } });
        }
        DrawMusicPlayer(ui);

        std::string version(VERSION_STRING);
        const size_t paren = version.find(" (");
        if (paren != std::string::npos) version.resize(paren);
        char ver[64];
        std::snprintf(ver, sizeof(ver), "PROJECT NOBLE  v%s", version.c_str());
        Text(ui, NobleStyle::SmallFont(), 14.0f,
             ImVec2(ui.origin.x + ui.size.x - TextWidth(NobleStyle::SmallFont(), 14.0f * ui.s, ver) - 24.0f * ui.s, ui.origin.y + ui.size.y - 30.0f * ui.s),
             IM_COL32(200, 210, 220, 110), ver);
        break;
    }

    case Screen::Campaign: {
        DrawTitle(ui, ImVec2(left, top), "CAMPAIGN", "SOLO");
        if (DrawSourceTabs(ui, ImVec2(left + 34.0f * ui.s, top + 70.0f * ui.s), st.campaignSource,
                           static_cast<int>(ctx.campaignMods.size()))) st.focusRow = 0;
        const bool modded = st.campaignSource == 1;
        if (DrawSearchBox(ui, "##mission_search", ImVec2(left + 34.0f * ui.s + 240.0f * ui.s, top + 70.0f * ui.s), 160.0f * ui.s, st.mapSearch, sizeof(st.mapSearch), "Search")) st.mapSearchRow = 0;
        if (ImGui::IsItemActive()) st.focusRow = 0;
        const bool searching = st.mapSearch[0] != 0;
        const std::vector<MapEntry> missions = ListedMaps(modded, ctx.campaignMods, ModMode::Campaign, kMissions, Count(kMissions), st.openMods, "m10", st.mapSearch);
        int& realPick = modded ? st.campaignModded : st.mission;
        int& pick = searching ? st.mapSearchRow : realPick;
        FixSelection(missions, pick);
        std::vector<ListItem> items = ToItems(missions);
        if (missions.empty()) items.push_back({ searching ? "Nothing matches your search" : "No Reach campaign mods installed", false, false });
        const float listY = top + 108.0f * ui.s;
        const float listRow = 42.0f;
        const int listRows = std::max(4, static_cast<int>((ui.size.y - 74.0f * ui.s - listY) / (listRow * ui.s)));
        const int hit = DrawList(ui, "missions", ImVec2(left + 34.0f * ui.s, listY), 400.0f * ui.s, items, pick,
                                 st.focusRow == 0 && !ImGui::GetIO().WantTextInput, listRow, listRows, false);
        if (hit >= 0 && !missions.empty() && missions[hit].header) ToggleMod(st.openMods, ModMode::Campaign, *missions[hit].mod);
        else if (hit >= 0) {
            if (searching) {
                // Land on the choice in the full list; the search clears on the next frame's list.
                const MapEntry chosen = missions[hit];
                if (chosen.mod) st.openMods.insert(OpenKey(ModMode::Campaign, *chosen.mod));
                const std::vector<MapEntry> full = ListedMaps(modded, ctx.campaignMods, ModMode::Campaign, kMissions, Count(kMissions), st.openMods, "m10", "");
                for (size_t i = 0; i < full.size(); ++i) {
                    if (!full[i].header && full[i].scenario == chosen.scenario && full[i].mod == chosen.mod) realPick = static_cast<int>(i);
                }
                st.mapSearch[0] = 0;
            }
            st.focusRow = 1;
        }
        FixSelection(missions, pick);
        const MapEntry* row = missions.empty() ? nullptr : &missions[pick];
        const MapEntry* mission = (row && !row->header) ? row : nullptr;
        const ModEntry* mod = mission ? mission->mod : nullptr;

        // Right column, top down: banner, difficulty tiles, skulls row, skull icons; the network strip and
        // the action buttons stay anchored at the bottom (RightColumnLayout) so nothing is drawn over
        // anything else.
        const float px = ui.size.x * 0.47f, pw = ui.size.x * 0.44f;
        const RightColumnY col = RightColumnLayout(ui);
        const float actionsY = col.actions;
        // Everything drawn between the banner and the strip: tiles (98), gap (8), skulls row (36), icons (30).
        const float rowsH = 98.0f * ui.s + 8.0f * ui.s + 36.0f * ui.s + 30.0f * ui.s;
        const float bannerTop = top + 96.0f * ui.s;
        const char* blurb = row ? row->blurb.c_str() : "";
        const float blurbH = BlurbBlockHeight(ui, pw, blurb);
        const float bannerH = HeroBannerHeight(ui, pw, col.content, rowsH, blurbH, bannerTop);
        const std::string kicker = mod ? "MOD: " + Upper(mod->title) : row && row->header ? std::string("MOD") : std::string("HALO: REACH \xC2\xB7 CAMPAIGN");
        float y = HeroBanner(ui, ImVec2(px, bannerTop), pw, bannerH, row ? row->scenario : "", kicker,
                             row ? (row->header ? row->mod->title.c_str() : row->name.c_str()) : "", blurb,
                             row ? row->imageLarge : std::filesystem::path());
        y += DifficultyTiles(ui, "diff", ImVec2(px, y), pw, st.difficulty, st.focusRow == 1) + 8.0f * ui.s;
        const std::string skullText = SkullSummary(st.skulls);
        if (OptionRow(ui, "skulls", ImVec2(px, y), pw, "SKULLS", skullText.c_str(), st.focusRow == 2)) {
            st.screen = Screen::Skulls;
        }
        ImGui::SetCursorScreenPos(ImVec2(px, y));
        if (ImGui::InvisibleButton("##skullrow", ImVec2(pw * 0.46f, 32.0f * ui.s))) st.screen = Screen::Skulls;
        y += 36.0f * ui.s;
        const float stripW = SkullStrip(ui, ImVec2(px + 10.0f * ui.s, y + 2.0f * ui.s), 20.0f * ui.s, st.skulls, 13);
        char mult[64];
        std::snprintf(mult, sizeof(mult), "Score multiplier  x%.2f", ScoreMultiplier(st.difficulty, st.skulls));
        Text(ui, nullptr, 16.0f, ImVec2(px + 10.0f * ui.s + (stripW > 0.0f ? stripW + 12.0f * ui.s : 0.0f), y + 3.0f * ui.s), kDim, mult);
        const float lobbyY = col.strip;
        DrawLobbyControls(ui, ImVec2(px, lobbyY), pw, st);

        const bool canStart = mission && mission->enabled;
        const float bw = (pw - 10.0f * ui.s) * 0.5f;
        std::string sub = mission ? std::string(mission->name) + " · " + kDifficultyNames[st.difficulty] : std::string("Pick a mission");
        const bool startSolo = ((BigAction(ui, "start", ImVec2(px, actionsY), bw, "PLAY SOLO", sub.c_str(), true, canStart, st.focusRow == 3) ||
                                 (st.focusRow == 3 && AcceptPressed())) && canStart) || (AutoPlayNow() && canStart);
        const char* coopSub = st.lobbyNetMode == 1 ? (st.lobbyPrivacy == 0 ? "Public Online P2P" : "Private Online P2P") : "Local LAN Co-op";
        const bool hostCoop = BigAction(ui, "coop", ImVec2(px + bw + 10.0f * ui.s, actionsY), pw - bw - 10.0f * ui.s, "HOST CO-OP", coopSub, true, canStart);
        const std::string launchScenario = mission ? (mission->launchScenario.empty() ? mission->scenario : mission->launchScenario) : std::string();
        if (hostCoop) {
            SetupHostLobby(st, launchScenario, "campaign", "", "Campaign", st.difficulty, st.skulls, 16, mission->mod,
                           mission->mapOverride, mission->name, mission->imageLoading, mission->imageThumbnail);
        } else if (startSolo) {
            choice.play = true;
            choice.host = false;
            choice.mode = "campaign";
            choice.map = launchScenario;
            choice.mapOverride = mission->mapOverride;
            choice.mapTitle = mission->name;
            choice.mapImage = mission->imageLoading;
            choice.mapThumb = mission->imageThumbnail;
            choice.difficulty = st.difficulty;
            choice.skulls = st.skulls;
            if (mod) { choice.modFolder = mod->folder; choice.modTitle = mod->title; }
            done = true;
        }
        if (st.focusRow > 0 && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_Tab) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown))) st.focusRow = std::min(3, st.focusRow + 1);
        if (st.focusRow > 0 && (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickUp))) st.focusRow = std::max(1, st.focusRow - 1);
        if (st.focusRow == 2 && AcceptPressed()) st.screen = Screen::Skulls;
        if (back) {
            if (st.focusRow > 0) st.focusRow = 0;
            else goBack(Screen::Main);
        }
        DrawHints(ui, { { Prompt::Accept, "Select" }, { Prompt::Back, "Back" }, { Prompt::Change, "Change" }, { Prompt::Tabs, "Vanilla / Modded" } });
        break;
    }

    case Screen::Skulls: {
        DrawTitle(ui, ImVec2(left, top), "SKULLS", "CAMPAIGN");
        const float gx = left + 46.0f * ui.s;
        float gy = top + 92.0f * ui.s;
        const float cell = 78.0f * ui.s, icon = 64.0f * ui.s;
        auto section = [&](const char* name, float y) {
            Text(ui, NobleStyle::TitleFont(), 20.0f, ImVec2(gx, y), kWhite, name);
            ui.dl->AddLine(ImVec2(gx, y + 26.0f * ui.s), ImVec2(gx + cell * 5.0f, y + 26.0f * ui.s), IM_COL32(255, 255, 255, 60));
        };
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight)) st.skull = std::min(kSkullCount - 1, st.skull + 1);
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft)) st.skull = std::max(0, st.skull - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown)) st.skull = std::min(kSkullCount - 1, st.skull + 5);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickUp)) st.skull = std::max(0, st.skull - 5);
        bool toggle = AcceptPressed() || ImGui::IsKeyPressed(ImGuiKey_Space, false);

        section("Primary Skulls", gy);
        gy += 36.0f * ui.s;
        int primaryIndex = 0, secondaryIndex = 0;
        float secondaryTop = gy + cell * 2.0f + 14.0f * ui.s;
        for (int i = 0; i < kSkullCount; ++i) {
            const Skull& sk = kSkulls[i];
            int idx = sk.primary ? primaryIndex++ : secondaryIndex++;
            const float baseY = sk.primary ? gy : secondaryTop + 36.0f * ui.s;
            const ImVec2 a(gx + (idx % 5) * cell, baseY + (idx / 5) * cell);
            ImGui::SetCursorScreenPos(a);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##skull", ImVec2(icon, icon))) {
                st.skull = i;
                toggle = true;
            }
            if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDelta.x != 0.0f) st.skull = i;
            ImGui::PopID();
            const bool on = (st.skulls & (1ull << sk.bit)) != 0;
            if (i == st.skull) ui.dl->AddCircle(ImVec2(a.x + icon * 0.5f, a.y + icon * 0.5f), icon * 0.56f, kWhite, 0, 2.0f * ui.s);
            if (on) ui.dl->AddCircleFilled(ImVec2(a.x + icon * 0.5f, a.y + icon * 0.5f), icon * 0.52f, IM_COL32(230, 160, 90, 70));
            UiTexture tex = UiArt::SkullIcon(sk.icon);
            const ImU32 tint = on ? IM_COL32(255, 205, 160, 255) : IM_COL32(255, 255, 255, 105);
            if (tex) ui.dl->AddImage(tex.id, a, ImVec2(a.x + icon, a.y + icon), ImVec2(0, 0), ImVec2(1, 1), tint);
            else Text(ui, nullptr, 14.0f, ImVec2(a.x, a.y + icon * 0.4f), on ? kWhite : kDim, sk.name, a.x + icon);
        }
        section("Secondary Skulls", secondaryTop);
        if (toggle) {
            st.skulls ^= 1ull << kSkulls[st.skull].bit;
            saveSkulls();
            MenuSounds::Click();
        }

        const Skull& sel = kSkulls[st.skull];
        const float dx = ui.origin.x + ui.size.x * 0.60f;
        float dy = top + 70.0f * ui.s;
        UiTexture big = UiArt::SkullIcon(sel.icon);
        const float bs = 170.0f * ui.s;
        if (big) ui.dl->AddImage(big.id, ImVec2(dx + 40.0f * ui.s, dy), ImVec2(dx + 40.0f * ui.s + bs, dy + bs));
        dy += bs + 18.0f * ui.s;
        // Dark panel behind the text: it sits on the background art, and the dim description was unreadable over it.
        ui.dl->AddRectFilled(ImVec2(dx - 14.0f * ui.s, dy - 8.0f * ui.s), ImVec2(dx + 314.0f * ui.s, dy + 176.0f * ui.s),
                             IM_COL32(8, 14, 22, 190), 3.0f * ui.s);
        Text(ui, NobleStyle::HeroFont(), 34.0f, ImVec2(dx, dy), kWhite, sel.name);
        dy += 42.0f * ui.s;
        Text(ui, nullptr, 17.0f, ImVec2(dx, dy), IM_COL32(205, 218, 230, 255), sel.description, dx + 300.0f * ui.s);
        dy += 74.0f * ui.s;
        const bool selOn = (st.skulls & (1ull << sel.bit)) != 0;
        Text(ui, nullptr, 17.0f, ImVec2(dx, dy), kWhite, selOn ? "Status: On" : "Status: Off");
        dy += 24.0f * ui.s;
        char mult[64];
        if (sel.multiplier > 1.0f) std::snprintf(mult, sizeof(mult), "Score bonus: +%.1f", sel.multiplier);
        else std::snprintf(mult, sizeof(mult), "No score bonus");
        Text(ui, nullptr, 17.0f, ImVec2(dx, dy), IM_COL32(205, 218, 230, 255), mult);

        const float by = ui.origin.y + ui.size.y - 104.0f * ui.s;
        ui.dl->AddRectFilledMultiColor(ImVec2(left + 40.0f * ui.s, by), ImVec2(ui.origin.x + ui.size.x * 0.80f, by + 40.0f * ui.s),
                                       IM_COL32(40, 52, 64, 210), IM_COL32(40, 52, 64, 60), IM_COL32(40, 52, 64, 60), IM_COL32(40, 52, 64, 210));
        Text(ui, NobleStyle::TitleFont(), 24.0f, ImVec2(left + 52.0f * ui.s, by + 6.0f * ui.s), kWhite, settings.playerName.c_str());
        char total[64];
        std::snprintf(total, sizeof(total), "x%.2f", ScoreMultiplier(st.difficulty, st.skulls));
        Text(ui, NobleStyle::TitleFont(), 24.0f, ImVec2(ui.origin.x + ui.size.x * 0.80f - 90.0f * ui.s, by + 6.0f * ui.s), kWhite, total);
        if (back) goBack(Screen::Campaign);
        DrawHints(ui, { { Prompt::Accept, "Toggle" }, { Prompt::Back, "Back" } });
        break;
    }

    case Screen::Firefight:
    case Screen::CustomGame:
    case Screen::Forge: {
        const bool ff = st.screen == Screen::Firefight;
        const bool forge = st.screen == Screen::Forge;
        if (st.playlistEditOpen) {
            // Playlist editor: choose a map and game type, add them, repeat; the list is on the right.
            DrawTitle(ui, ImVec2(left, top), "PLAYLIST", "MULTIPLAYER");
            const std::vector<MapEntry> allMaps = AllMultiplayerMaps(ctx);
            const float px = left + 34.0f * ui.s, pw = ui.size.x * 0.40f;
            float y = top + 70.0f * ui.s;
            DrawSearchBox(ui, "##playlist_name", ImVec2(px, y), pw, st.playlistName, sizeof(st.playlistName), "Playlist name");
            y += 44.0f * ui.s;
            const bool typing = ImGui::GetIO().WantTextInput;
            const bool canDeleteList = !st.playlistEdit.file.empty() && st.playlistEdit.file.parent_path() == Playlists::Directory();
            constexpr int kMapRow = 0, kTypeRow = 1, kAddRow = 2, kShuffleRow = 3, kRandomRow = 4, kAutoRow = 5, kSaveRow = 6, kDeleteRow = 7;
            const int kRows = canDeleteList ? 8 : 7;
            if (!typing && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || UiPad::Down())) { st.playlistEditRow = (st.playlistEditRow + 1) % kRows; MenuSounds::Scroll(); }
            if (!typing && (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || UiPad::Up())) { st.playlistEditRow = (st.playlistEditRow + kRows - 1) % kRows; MenuSounds::Scroll(); }
            st.playlistEditMap = allMaps.empty() ? 0 : std::clamp(st.playlistEditMap, 0, static_cast<int>(allMaps.size()) - 1);
            const MapEntry* editMap = allMaps.empty() ? nullptr : &allMaps[st.playlistEditMap];
            std::vector<const MenuVariant*> types;
            for (const MenuVariant& v : ctx.variants) {
                if (editMap && (editMap->mod || v.type.empty() || MapGameTypes::Supports(editMap->scenario, v.type))) types.push_back(&v);
            }
            st.playlistEditVariant = types.empty() ? 0 : std::clamp(st.playlistEditVariant, 0, static_cast<int>(types.size()) - 1);
            const int mapStep = OptionRow(ui, "pl_map", ImVec2(px, y), pw, "MAP", editMap ? editMap->name.c_str() : "-", !typing && st.playlistEditRow == kMapRow);
            if (mapStep != 0 && !allMaps.empty()) st.playlistEditMap = (st.playlistEditMap + mapStep + static_cast<int>(allMaps.size())) % static_cast<int>(allMaps.size());
            y += 36.0f * ui.s;
            const int typeStep = OptionRow(ui, "pl_type", ImVec2(px, y), pw, "GAME TYPE", types.empty() ? "-" : types[st.playlistEditVariant]->label.c_str(), !typing && st.playlistEditRow == kTypeRow);
            if (typeStep != 0 && !types.empty()) st.playlistEditVariant = (st.playlistEditVariant + typeStep + static_cast<int>(types.size())) % static_cast<int>(types.size());
            y += 40.0f * ui.s;
            const float bw = (pw - 10.0f * ui.s) * 0.5f;
            bool add = BigAction(ui, "pl_add", ImVec2(px, y), pw, "ADD TO PLAYLIST", "Adds this map and game type", true, editMap != nullptr && !types.empty(), st.playlistEditRow == kAddRow);
            y += 62.0f * ui.s;
            const int shuffleStep = OptionRow(ui, "pl_shuffle", ImVec2(px, y), pw, "ORDER", st.playlistEdit.shuffle ? "SHUFFLED" : "IN ORDER", !typing && st.playlistEditRow == kShuffleRow);
            if (shuffleStep != 0) st.playlistEdit.shuffle = !st.playlistEdit.shuffle;
            y += 36.0f * ui.s;
            const int randomStep = OptionRow(ui, "pl_random", ImVec2(px, y), pw, "RANDOM GAME TYPE", st.playlistEdit.randomType ? "ON" : "OFF", !typing && st.playlistEditRow == kRandomRow);
            if (randomStep != 0) st.playlistEdit.randomType = !st.playlistEdit.randomType;
            y += 36.0f * ui.s;
            static const int kAutoChoices[] = { 0, 15, 30, 60 };
            int autoAt = 0;
            for (int i = 0; i < 4; ++i) if (kAutoChoices[i] == st.playlistEdit.autoStart) autoAt = i;
            const std::string autoText = st.playlistEdit.autoStart > 0 ? std::to_string(st.playlistEdit.autoStart) + " S" : std::string("OFF");
            const int autoStep = OptionRow(ui, "pl_auto", ImVec2(px, y), pw, "NEXT MATCH STARTS BY ITSELF", autoText.c_str(), !typing && st.playlistEditRow == kAutoRow);
            if (autoStep != 0) st.playlistEdit.autoStart = kAutoChoices[(autoAt + autoStep + 4) % 4];
            y += 42.0f * ui.s;
            bool save = BigAction(ui, "pl_save", ImVec2(px, y), bw, "SAVE PLAYLIST", "Then pick it on Custom Game", true, !st.playlistEdit.entries.empty(), st.playlistEditRow == kSaveRow);
            bool eraseList = canDeleteList && BigAction(ui, "pl_delete", ImVec2(px + bw + 10.0f * ui.s, y), bw, "DELETE", "Removes this playlist", false, true, st.playlistEditRow == kDeleteRow);
            if (!typing && AcceptPressed() && canDeleteList && st.playlistEditRow == kDeleteRow) eraseList = true;
            if (eraseList) {
                if (st.confirmAction != "delete") {
                    st.confirmAction = "delete";
                    st.playlistMessage = "Press DELETE again to remove this playlist.";
                    MenuSounds::Error();
                } else {
                    std::error_code removeError;
                    std::filesystem::remove(st.playlistEdit.file, removeError);
                    NOBLE_INFO("FrontEndMenu: playlist deleted: {}", st.playlistEdit.file.filename().string());
                    RefreshPlaylists(ctx);
                    st.playlistChoice = -1;
                    st.playlistEditOpen = false;
                    MenuSounds::Back();
                }
            }
            if (!typing && AcceptPressed()) {
                if (st.playlistEditRow == kAddRow) add = editMap != nullptr && !types.empty();
                if (st.playlistEditRow == kSaveRow) save = !st.playlistEdit.entries.empty();
                if (st.playlistEditRow == kShuffleRow) st.playlistEdit.shuffle = !st.playlistEdit.shuffle;
                if (st.playlistEditRow == kRandomRow) st.playlistEdit.randomType = !st.playlistEdit.randomType;
            }
            if (add) {
                Playlists::Entry entry;
                entry.map = editMap->scenario;
                entry.variant = types[st.playlistEditVariant]->path.filename().string();
                entry.mod = editMap->mod ? editMap->mod->folder.filename().string() : std::string();
                st.playlistEdit.entries.push_back(std::move(entry));
                st.playlistEditEntry = static_cast<int>(st.playlistEdit.entries.size()) - 1;
                MenuSounds::Click();
            }
            // The list so far. Y or a click on a row takes it out again.
            const float lx = px + pw + 40.0f * ui.s, lw = (ui.origin.x + ui.size.x * 0.925f) - lx;
            Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(lx, top + 70.0f * ui.s), kAccent, ("ENTRIES (" + std::to_string(st.playlistEdit.entries.size()) + ")").c_str());
            std::vector<ListItem> rows;
            for (const Playlists::Entry& entry : st.playlistEdit.entries) {
                std::string mapName = entry.map, typeName = entry.variant;
                for (const MapEntry& m : allMaps) if (m.scenario == entry.map) { mapName = m.name; break; }
                for (const MenuVariant& v : ctx.variants) if (v.path.filename().string() == entry.variant) typeName = v.label;
                rows.push_back({ mapName + " - " + typeName });
            }
            if (rows.empty()) rows.push_back({ "Nothing added yet", false, false });
            const int gone = DrawList(ui, "playlist_rows", ImVec2(lx, top + 100.0f * ui.s), lw, rows, st.playlistEditEntry, false, 32.0f, 14, true);
            int removeRow = gone;
            if (!typing && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false)) removeRow = st.playlistEditEntry;
            if (removeRow >= 0 && removeRow < static_cast<int>(st.playlistEdit.entries.size())) {
                st.playlistEdit.entries.erase(st.playlistEdit.entries.begin() + removeRow);
                MenuSounds::Back();
            }
            std::error_code listExistsError;
            const std::filesystem::path listTarget = Playlists::PathFor(st.playlistName);
            if (save && !listTarget.empty() && std::filesystem::exists(listTarget, listExistsError) && listTarget != st.playlistEdit.file && st.confirmAction != "replace") {
                save = false;
                st.confirmAction = "replace";
                st.playlistMessage = "A playlist with this name exists. Press SAVE again to replace it.";
                MenuSounds::Error();
            }
            if (save) {
                st.playlistEdit.name = st.playlistName;
                std::filesystem::path savedAs;
                if (Playlists::Save(st.playlistEdit, savedAs)) {
                    NOBLE_INFO("FrontEndMenu: playlist saved: '{}' with {} entries", st.playlistEdit.name, st.playlistEdit.entries.size());
                    RefreshPlaylists(ctx);
                    st.playlistChoice = -1;
                    for (size_t i = 0; i < st.playlists.size(); ++i) if (st.playlists[i].file == savedAs) st.playlistChoice = static_cast<int>(i);
                    st.playlistEditOpen = false;
                    MenuSounds::Click();
                } else {
                    st.playlistMessage = "Give it a name first.";
                    MenuSounds::Error();
                }
            }
            if (!st.playlistMessage.empty()) Text(ui, nullptr, 16.0f, ImVec2(px + bw + 20.0f * ui.s, y + 8.0f * ui.s), IM_COL32(255, 190, 120, 255), st.playlistMessage.c_str());
            if (back) { st.playlistEditOpen = false; MenuSounds::Back(); }
            DrawHints(ui, { { Prompt::Accept, "Select" }, { Prompt::Back, "Back" }, { Prompt::Change, "Change" } }, { { InputPrompts::Button::Y, "CLICK", "Remove Entry" } });
            break;
        }
        if (st.optionsOpen) {
            // Game options: General Settings and Player Traits, saved as the player's own named game mode.
            const std::vector<VariantRules::Rule>& rules = VariantRules::Rules();
            DrawTitle(ui, ImVec2(left, top), "GAME OPTIONS", "MULTIPLAYER");
            const float px = left + 34.0f * ui.s, pw = ui.size.x * 0.44f;
            float y = top + 70.0f * ui.s;
            DrawSearchBox(ui, "##mode_name", ImVec2(px, y), pw, st.optionsName, sizeof(st.optionsName), "Game mode name");
            y += 40.0f * ui.s;
            // Only this PC's own saved modes can be deleted; a mod's stay with the mod.
            const bool canDelete = !st.optionsFile.empty() && st.optionsFile.parent_path() == VariantRules::Directory();
            const int saveRow = static_cast<int>(rules.size()), resetRow = saveRow + 1, deleteRow = saveRow + 2, rowCount = saveRow + (canDelete ? 3 : 2);
            const bool typing = ImGui::GetIO().WantTextInput;
            if (!typing && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || UiPad::Down())) { st.optionsRow = (st.optionsRow + 1) % rowCount; MenuSounds::Scroll(); }
            if (!typing && (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || UiPad::Up())) { st.optionsRow = (st.optionsRow + rowCount - 1) % rowCount; MenuSounds::Scroll(); }
            const char* group = nullptr;
            // Too many rules for one screen: the list scrolls with the focus, and the buttons stay put below it.
            constexpr int kVisibleRows = 12;
            const int focusForWindow = std::min(st.optionsRow, saveRow - 1);
            const int firstRow = std::clamp(focusForWindow - kVisibleRows / 2, 0, std::max(0, saveRow - kVisibleRows));
            const int lastRow = std::min(saveRow, firstRow + kVisibleRows);
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f && st.optionsRow < saveRow) st.optionsRow = std::clamp(st.optionsRow - static_cast<int>(wheel) * 2, 0, saveRow - 1);
            const float buttonsY = ui.origin.y + ui.size.y - 74.0f * ui.s - 54.0f * ui.s;
            for (int i = firstRow; i < lastRow; ++i) {
                const VariantRules::Rule& rule = rules[i];
                if (y + 58.0f * ui.s > buttonsY) break;   // never under the buttons
                if (!group || std::strcmp(group, rule.group) != 0) {
                    group = rule.group;
                    Text(ui, NobleStyle::TitleFont(), 16.0f, ImVec2(px, y + 2.0f * ui.s), kAccent, Upper(group).c_str());
                    y += 22.0f * ui.s;
                }
                const int baseValue = VariantRules::Read(rule, st.optionsDecoded.data());
                const auto edited = st.optionsValues.find(rule.key);
                int value = edited != st.optionsValues.end() ? edited->second : baseValue;
                const std::string text = RuleValueText(rule, value);
                const int change = OptionRow(ui, rule.key, ImVec2(px, y), pw, rule.label, text.c_str(), !typing && st.optionsRow == i);
                if (change != 0) {
                    int next = std::clamp(value, rule.minValue, rule.maxValue);
                    if (rule.order) {
                        const std::vector<int>& order = *rule.order;
                        const int count = static_cast<int>(order.size());
                        const auto at = std::find(order.begin(), order.end(), value);
                        // A base value outside the choices (the game's explicit 100%, say) starts from the nearest end.
                        const int position = at == order.end() ? (change > 0 ? -1 : 0) : static_cast<int>(at - order.begin());
                        next = order[(position + change + count) % count];
                    } else {
                        const int span = rule.maxValue - rule.minValue + 1;
                        for (int step = 0; step < span; ++step) {
                            next = next + change > rule.maxValue ? rule.minValue : next + change < rule.minValue ? rule.maxValue : next + change;
                            if (!rule.names || !(*rule.names)[next - rule.minValue].empty()) break;
                        }
                    }
                    NOBLE_INFO("FrontEndMenu: game option {} -> {}", rule.key, next);
                    st.optionsRow = i;
                    if (next == baseValue) st.optionsValues.erase(rule.key);
                    else st.optionsValues[rule.key] = next;
                }
                if (st.optionsRow == i) Text(ui, nullptr, 16.0f, ImVec2(px + pw + 24.0f * ui.s, y + 6.0f * ui.s), kDim, rule.help, ui.origin.x + ui.size.x * 0.925f);
                y += 28.0f * ui.s;
            }
            y = buttonsY;
            const float bw = (pw - 20.0f * ui.s) / 3.0f;
            bool save = BigAction(ui, "mode_save", ImVec2(px, y), bw, "SAVE", "Adds it to the game type list", true, true, st.optionsRow == saveRow);
            bool reset = BigAction(ui, "mode_reset", ImVec2(px + bw + 10.0f * ui.s, y), bw, "RESET", "Back to the base rules", false, true, st.optionsRow == resetRow);
            bool erase = canDelete && BigAction(ui, "mode_delete", ImVec2(px + (bw + 10.0f * ui.s) * 2.0f, y), bw, "DELETE", "Removes this saved mode", false, true, st.optionsRow == deleteRow);
            if (!typing && AcceptPressed()) {
                if (st.optionsRow == saveRow) save = true;
                if (st.optionsRow == resetRow) reset = true;
                if (canDelete && st.optionsRow == deleteRow) erase = true;
            }
            if (reset) { st.optionsValues.clear(); MenuSounds::Back(); }
            const auto reloadModes = [&](const std::filesystem::path& select) {
                const bool ffScreen = ff;
                ctx.variants = ScanVariants(ctx.reachDir, false);
                ctx.ffVariants = ScanVariants(ctx.reachDir, true);
                int& index = ffScreen ? st.ffVariant : st.variant;
                const std::vector<MenuVariant>& modes = ffScreen ? ctx.ffVariants : ctx.variants;
                index = FindVariant(modes, ffScreen ? L"hr_ff_standard_1set.bin" : L"slayer_054.bin");
                for (size_t i = 0; i < modes.size(); ++i) if (modes[i].path == select) index = static_cast<int>(i);
            };
            if (erase) {
                if (st.confirmAction != "delete") {
                    st.confirmAction = "delete";
                    st.optionsMessage = "Press DELETE again to remove this mode for good.";
                    MenuSounds::Error();
                } else {
                    std::error_code removeError;
                    std::filesystem::remove(st.optionsFile, removeError);
                    NOBLE_INFO("FrontEndMenu: game mode deleted: {}", st.optionsFile.filename().string());
                    reloadModes({});
                    st.optionsOpen = false;
                    MenuSounds::Back();
                }
            }
            if (save) {
                std::string file;
                for (const char c : std::string(st.optionsName)) {
                    if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' || c == '_') file.push_back(c);
                }
                while (!file.empty() && file.back() == ' ') file.pop_back();
                if (file.empty()) {
                    st.optionsMessage = "Give it a name first.";
                    MenuSounds::Error();
                } else {
                    VariantRules::Preset preset;
                    preset.name = st.optionsName;
                    preset.base = st.optionsBase;
                    preset.values = st.optionsValues;
                    const std::filesystem::path path = VariantRules::Directory() / (file + ".json");
                    std::error_code existsError;
                    const bool wouldReplace = std::filesystem::exists(path, existsError) && path != st.optionsFile;
                    if (wouldReplace && st.confirmAction != "replace") {
                        st.confirmAction = "replace";
                        st.optionsMessage = "A mode with this name exists. Press SAVE again to replace it.";
                        MenuSounds::Error();
                    } else if (VariantRules::Save(path, preset)) {
                        NOBLE_INFO("FrontEndMenu: game mode saved: '{}' on '{}' with {} change(s)", preset.name, preset.base, preset.values.size());
                        reloadModes(path);
                        st.optionsOpen = false;
                        MenuSounds::Click();
                    } else {
                        st.optionsMessage = "Couldn't save the game mode.";
                        MenuSounds::Error();
                    }
                }
            }
            if (!st.optionsMessage.empty()) Text(ui, nullptr, 16.0f, ImVec2(px + pw + 24.0f * ui.s, y + 8.0f * ui.s), IM_COL32(255, 190, 120, 255), st.optionsMessage.c_str());
            if (back) { st.optionsOpen = false; MenuSounds::Back(); }
            DrawHints(ui, { { Prompt::Accept, "Select" }, { Prompt::Back, "Back" }, { Prompt::Change, "Change" } });
            break;
        }
        if (st.variantPickerOpen && !forge) {
            // The game type list, in place of the setup screen so nothing behind it reacts to the pad or the mouse.
            const std::vector<MenuVariant>& list = ff ? ctx.ffVariants : ctx.variants;
            int& current = ff ? st.ffVariant : st.variant;
            DrawTitle(ui, ImVec2(left, top), "GAME TYPE", ff ? "FIREFIGHT" : "MULTIPLAYER");
            Text(ui, nullptr, 17.0f, ImVec2(left + 34.0f * ui.s, top + 70.0f * ui.s), kDim,
                 st.variantPickerModded ? "Every game type, on a modded map." : ("Game types " + st.variantPickerMapName + " can host.").c_str());
            const float rowH = 38.0f, listTop = top + 108.0f * ui.s, listW = ui.size.x * 0.40f;
            const int rows = std::max(4, static_cast<int>((ui.origin.y + ui.size.y - 74.0f * ui.s - listTop - 40.0f * ui.s) / (rowH * ui.s)));
            VariantRow shown;
            const MenuVariant* picked = DrawVariantList(ui, st, list, st.variantPickerMap, ff || st.variantPickerModded, ImVec2(left + 34.0f * ui.s, listTop), listW, rowH, rows, true, shown);
            if (!shown.mode.empty()) {
                const float infoX = left + 34.0f * ui.s + listW + 40.0f * ui.s;
                const float infoW = (ui.origin.x + ui.size.x * 0.925f) - infoX;
                ui.dl->AddRectFilled(ImVec2(infoX - 16.0f * ui.s, listTop), ImVec2(infoX + infoW + 16.0f * ui.s, listTop + 190.0f * ui.s), IM_COL32(8, 14, 22, 190), 3.0f * ui.s);
                DrawVariantInfo(ui, ImVec2(infoX, listTop + 14.0f * ui.s), infoW, shown);
            }
            if (picked) {
                current = static_cast<int>(picked - list.data());
                NOBLE_INFO("FrontEndMenu: game type picked: {} ({})", picked->label, picked->path.filename().string());
                st.variantPickerOpen = false;
                MenuSounds::Change();
            } else if (back) {
                st.variantPickerOpen = false;
                MenuSounds::Back();
            }
            DrawHints(ui, { { Prompt::Accept, "Choose" }, { Prompt::Back, "Back" } });
            break;
        }
        DrawTitle(ui, ImVec2(left, top), ff ? "FIREFIGHT" : forge ? "FORGE" : "CUSTOM GAME", ff ? "SURVIVAL" : forge ? "EDIT" : "MULTIPLAYER");
        const MenuMap* baseMaps = ff ? kFirefightMaps : kMaps;
        const int baseCount = ff ? Count(kFirefightMaps) : Count(kMaps);
        const std::vector<const ModEntry*>& modList = ff ? ctx.ffMods : ctx.mpMods;
        int& source = ff ? st.ffSource : forge ? st.forgeSource : st.mpSource;
        float listTop = top + 96.0f * ui.s;
        {
            if (DrawSourceTabs(ui, ImVec2(left + 34.0f * ui.s, top + 70.0f * ui.s), source, static_cast<int>(modList.size()))) {
                st.focusRow = 0;
            }
            listTop = top + 108.0f * ui.s;
        }
        const bool modded = source == 1;
        const ModMode modMode = ff ? ModMode::Firefight : ModMode::Multiplayer;
        const std::string carrier = ff ? "ff60_icecave" : "70_boneyard";
        if (DrawSearchBox(ui, "##map_search", ImVec2(left + 34.0f * ui.s + 240.0f * ui.s, top + 70.0f * ui.s), 140.0f * ui.s, st.mapSearch, sizeof(st.mapSearch), "Search")) st.mapSearchRow = 0;
        if (ImGui::IsItemActive()) st.focusRow = 0;
        const bool searching = st.mapSearch[0] != 0;
        const std::vector<MapEntry> maps = ListedMaps(modded, modList, modMode, baseMaps, baseCount, st.openMods, carrier, st.mapSearch);
        int& realIndex = modded ? (ff ? st.ffModded : forge ? st.forgeModded : st.mpModded) : (ff ? st.ffMap : forge ? st.forgeMap : st.map);
        int& mapIndex = searching ? st.mapSearchRow : realIndex;
        if (const std::string autoMap = AutoPlayMap(); !autoMap.empty() && !modded) {
            for (size_t i = 0; i < maps.size(); ++i) {
                if (maps[i].header) continue;
                if (maps[i].scenario == autoMap) { mapIndex = static_cast<int>(i); break; }
            }
        }
        FixSelection(maps, mapIndex);
        std::vector<ListItem> items = ToItems(maps);
        if (maps.empty()) items.push_back({ searching ? "Nothing matches your search" : ff ? "No Reach Firefight mods installed" : "No Reach multiplayer mods installed", false, false });
        const float listRow = 40.0f;
        const int listRows = std::max(4, static_cast<int>((ui.origin.y + ui.size.y - 74.0f * ui.s - listTop) / (listRow * ui.s)));
        const int hit = DrawList(ui, "maps", ImVec2(left + 34.0f * ui.s, listTop), 380.0f * ui.s, items, mapIndex,
                                 st.focusRow == 0 && !ImGui::GetIO().WantTextInput, listRow, listRows, false);
        if (hit >= 0 && !maps.empty() && maps[hit].header) ToggleMod(st.openMods, modMode, *maps[hit].mod);
        else if (hit >= 0) {
            if (searching) {
                // Land on the choice in the full list; the search clears on the next frame's list.
                const MapEntry chosen = maps[hit];
                if (chosen.mod) st.openMods.insert(OpenKey(modMode, *chosen.mod));
                const std::vector<MapEntry> full = ListedMaps(modded, modList, modMode, baseMaps, baseCount, st.openMods, carrier, "");
                for (size_t i = 0; i < full.size(); ++i) {
                    if (!full[i].header && full[i].scenario == chosen.scenario && full[i].mod == chosen.mod) realIndex = static_cast<int>(i);
                }
                st.mapSearch[0] = 0;
            }
            st.focusRow = 1;
        }
        FixSelection(maps, mapIndex);
        const MapEntry* row = maps.empty() ? nullptr : &maps[mapIndex];
        const MapEntry* map = (row && !row->header) ? row : nullptr;
        const ModEntry* mod = map ? map->mod : nullptr;

        // "-auto-play-variant <text>": a test run can ask for a specific game type (the matrix uses this).
        if (const std::string autoStem = AutoPlayVariantStem(); !autoStem.empty() && !modded) {
            const std::vector<MenuVariant>& list = ff ? ctx.ffVariants : ctx.variants;
            int& v = ff ? st.ffVariant : st.variant;
            std::string needle = autoStem;
            for (char& c : needle) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            // Prefer a file whose stem *starts* with the needle: "slayer" must pick slayer_054, not
            // 2nvasion_slayer_054 (which sorts first and is an Invasion variant).
            int prefix = -1, anywhere = -1;
            for (size_t i = 0; i < list.size(); ++i) {
                std::string stem = list[i].path.stem().string();
                for (char& c : stem) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
                if (stem.rfind(needle, 0) == 0) { prefix = static_cast<int>(i); break; }
                if (anywhere < 0 && stem.find(needle) != std::string::npos) anywhere = static_cast<int>(i);
            }
            const int chosen = prefix >= 0 ? prefix : anywhere;
            if (chosen >= 0) {
                v = chosen;
                static bool s_logged = false;
                if (!s_logged) {
                    s_logged = true;
                    NOBLE_INFO("FrontEndMenu: -auto-play-variant {} -> {}", autoStem, list[chosen].path.filename().string());
                }
            }
        }

        // Right column, top down: banner, game type row, Firefight options + difficulty; the network strip
        // and the action buttons stay anchored at the bottom (RightColumnLayout), so the strip can never
        // cover the difficulty tiles the way it did before.
        const float px = ui.origin.x + ui.size.x * 0.47f, pw = ui.size.x * 0.44f;
        const RightColumnY col = RightColumnLayout(ui);
        const float actionsY = col.actions;
        // Game-type safety: a vanilla map only hosts the types it was built for (Invasion on Sword Base used to
        // crash the engine). Modded maps are never checked - a mod may bring its own setup.
        std::string gameType;
        std::string typeConflict;
        if (!ff && !forge && map && map->mod == nullptr) {
            const std::vector<MenuVariant>& list = ff ? ctx.ffVariants : ctx.variants;
            int& v = ff ? st.ffVariant : st.variant;
            if (!list.empty()) {
                gameType = list[std::clamp(v, 0, static_cast<int>(list.size()) - 1)].type;
            }
            if (!gameType.empty() && !MapGameTypes::Supports(map->scenario, gameType)) {
                // Friendly map names for the "pick ..." part of the message.
                std::string good;
                const std::vector<std::string> allowed = MapGameTypes::RequiredMaps(gameType);
                for (size_t i = 0; i < allowed.size(); ++i) {
                    std::string name = allowed[i];
                    for (const MenuMap& m : kMaps) if (m.scenario == allowed[i]) { name = m.name; break; }
                    if (i) good += (i + 1 == allowed.size()) ? " or " : ", ";
                    good += name;
                }
                typeConflict = std::string(MapGameTypes::Name(gameType)) + " isn't set up on " + map->name +
                               "  Pick " + good + " (modded maps are never restricted).";
                // Say it in the log too: it explains a refused start to whoever reads the log later (and lets the
                // test harness stop waiting for a match that will never start).
                static std::string s_lastRefusal;
                if (s_lastRefusal != (map->scenario + "|" + gameType)) {
                    s_lastRefusal = map->scenario + "|" + gameType;
                    NOBLE_INFO("FrontEndMenu: refused to start {} on {} - {}{}", MapGameTypes::Name(gameType),
                               map->name, map->scenario, " has no setup for it");
                }
            }
        }
        const bool forgeMissing = forge && !std::filesystem::exists(ctx.forgeVariant);
        // Custom Game / Forge get the same row block as Firefight and Campaign: game type, game options and
        // the lobby's player cap, so the column is not banner-then-void (2026-10-30).
        const float rowsH = (forge ? 0.0f : 36.0f * ui.s) + (!ff ? 72.0f * ui.s : 0.0f) + (!ff && !forge ? 36.0f * ui.s : 0.0f) +
                            (ff ? 30.0f * ui.s + 98.0f * ui.s + 8.0f * ui.s : 0.0f) +
                            (forgeMissing ? 26.0f * ui.s : 0.0f) + (typeConflict.empty() ? 0.0f : 42.0f * ui.s);
        const float bannerTop = top + 96.0f * ui.s;
        const char* blurb = row ? row->blurb.c_str() : "";
        const float blurbH = BlurbBlockHeight(ui, pw, blurb);
        const float bannerH = HeroBannerHeight(ui, pw, col.content, rowsH, blurbH, bannerTop);
        const std::string kicker = mod ? "MOD: " + Upper(mod->title)
                                 : std::string(ff ? "HALO: REACH \xC2\xB7 FIREFIGHT" : forge ? "HALO: REACH \xC2\xB7 FORGE" : "HALO: REACH \xC2\xB7 MULTIPLAYER");
        float y = HeroBanner(ui, ImVec2(px, bannerTop), pw, bannerH, row ? row->scenario : "", kicker,
                             row ? (row->header ? row->mod->title.c_str() : row->name.c_str()) : "", blurb,
                             row ? row->imageLarge : std::filesystem::path());
        int focusRows = 1;
        if (!forge) {
            const std::vector<MenuVariant>& list = ff ? ctx.ffVariants : ctx.variants;
            int& v = ff ? st.ffVariant : st.variant;
            const char* label = list.empty() ? "Default" : list[std::clamp(v, 0, static_cast<int>(list.size()) - 1)].label.c_str();
            const int dv = OptionRow(ui, "variant", ImVec2(px, y), pw, "GAME TYPE", label, st.focusRow == focusRows);
            // A, or a click on the row's label or value, opens the whole list; the arrows still step through it.
            const float arrowW = 28.0f * ui.s, valueX = px + pw * 0.48f, rowHeight = 32.0f * ui.s;
            ImGui::SetCursorScreenPos(ImVec2(px, y));
            bool openList = ImGui::InvisibleButton("##variant_label", ImVec2(pw * 0.48f, rowHeight));
            ImGui::SetCursorScreenPos(ImVec2(valueX + arrowW, y));
            openList = ImGui::InvisibleButton("##variant_value", ImVec2(pw * 0.52f - arrowW * 2.0f, rowHeight)) || openList;
            if (st.focusRow == focusRows && hit < 0 && AcceptPressed()) openList = true;
            if (openList && !list.empty()) {
                st.variantPickerOpen = true;
                st.variantPickerModded = modded;
                st.variantPickerMap = map ? map->scenario : std::string();
                st.variantPickerMapName = map ? map->name : std::string();
                BeginVariantList(st, list, st.variantPickerMap, modded || ff, &list[std::clamp(v, 0, static_cast<int>(list.size()) - 1)]);
                MenuSounds::Click();
            }
            if (!list.empty()) {
                const int count = static_cast<int>(list.size());
                // Vanilla maps only offer the game types they were built for, so Invasion simply is not in Sword
                // Base's list; modded maps keep every variant (players asked for that, and a mod may bring its own).
                auto allowed = [&](int index) {
                    if (modded || !map) return true;
                    const std::string& type = list[index].type;
                    return type.empty() || MapGameTypes::Supports(map->scenario, type);
                };
                if (dv) {
                    int next = v;
                    for (int step = 0; step < count; ++step) {
                        next = (next + dv + count) % count;
                        if (allowed(next)) break;
                    }
                    v = next;
                } else if (!allowed(v)) {
                    // The map changed under a game type it cannot host: move to the first one it can.
                    for (int step = 1; step <= count; ++step) {
                        const int candidate = (v + step) % count;
                        if (allowed(candidate)) { v = candidate; break; }
                    }
                }
            }
            y += 36.0f * ui.s;
            ++focusRows;
        }
        if (!typeConflict.empty()) {
            ui.dl->AddRectFilled(ImVec2(px, y), ImVec2(px + pw, y + 38.0f * ui.s), IM_COL32(90, 26, 26, 150));
            Text(ui, nullptr, 14.5f, ImVec2(px + 10.0f * ui.s, y + 4.0f * ui.s), IM_COL32(255, 190, 180, 255),
                 typeConflict.c_str(), pw - 20.0f * ui.s);
            y += 42.0f * ui.s;
        }
        if (!ff) {
            // Custom Game / Forge: the same row block the other setup screens have, so the column is not
            // banner-then-void. PLAYERS is real and feeds HOST LOBBY / HOST FORGE.
            if (forge) {
                ui.dl->AddRectFilled(ImVec2(px, y), ImVec2(px + pw, y + 32.0f * ui.s), IM_COL32(8, 12, 18, 85));
                Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(px + 10.0f * ui.s, y + 4.0f * ui.s), kDisabled, "GAME OPTIONS \xC2\xB7 DEFAULT");
                const char* later = "Coming later";
                Text(ui, nullptr, 15.0f, ImVec2(px + pw - TextWidth(nullptr, 15.0f * ui.s, later) - 10.0f * ui.s, y + 6.0f * ui.s), kDisabled, later);
            } else {
                const std::vector<MenuVariant>& list = ctx.variants;
                const int optionsChange = OptionRow(ui, "game_options", ImVec2(px, y), pw, "GAME OPTIONS", "EDIT", st.focusRow == focusRows);
                ImGui::SetCursorScreenPos(ImVec2(px, y));
                const bool clicked = ImGui::InvisibleButton("##game_options_open", ImVec2(pw * 0.48f, 32.0f * ui.s));
                if ((clicked || optionsChange != 0 || (st.focusRow == focusRows && hit < 0 && AcceptPressed())) && !list.empty()) {
                    OpenGameOptions(ctx, list[std::clamp(st.variant, 0, static_cast<int>(list.size()) - 1)]);
                }
                ++focusRows;
            }
            y += 36.0f * ui.s;
            if (!forge) {
                // PLAYLIST: left and right choose one; A or a click opens the editor, on the chosen one or a new one.
                const bool chosen = st.playlistChoice >= 0 && st.playlistChoice < static_cast<int>(st.playlists.size());
                const int count = static_cast<int>(st.playlists.size());
                const int playlistStep = OptionRow(ui, "playlist", ImVec2(px, y), pw, "PLAYLIST", chosen ? st.playlists[st.playlistChoice].name.c_str() : "NONE", st.focusRow == focusRows);
                if (playlistStep != 0) st.playlistChoice = ((st.playlistChoice + 1 + playlistStep + count + 1) % (count + 1)) - 1;
                ImGui::SetCursorScreenPos(ImVec2(px, y));
                const bool openEditor = ImGui::InvisibleButton("##playlist_open", ImVec2(pw * 0.48f, 32.0f * ui.s));
                if (openEditor || (st.focusRow == focusRows && hit < 0 && AcceptPressed())) {
                    st.playlistEdit = chosen ? st.playlists[st.playlistChoice] : Playlists::Playlist{};
                    std::snprintf(st.playlistName, sizeof(st.playlistName), "%s", chosen ? st.playlistEdit.name.c_str() : "My Playlist");
                    st.playlistEditRow = 0;
                    st.playlistMessage.clear();
                    st.confirmAction.clear();
                    st.playlistEditOpen = true;
                    MenuSounds::Click();
                }
                ++focusRows;
                y += 36.0f * ui.s;
            }
            static const int kCaps[] = { 4, 8, 12, 16, 24, 32, 48, 64, 96, 128 };
            constexpr int kNumCaps = static_cast<int>(sizeof(kCaps) / sizeof(kCaps[0]));
            int capIdx = 3; // default 16
            for (int ci = 0; ci < kNumCaps; ++ci) {
                if (kCaps[ci] >= st.sessionMaxPlayers) {
                    capIdx = ci;
                    break;
                }
            }
            char capText[32];
            std::snprintf(capText, sizeof(capText), "%d PLAYERS", kCaps[capIdx]);
            const int dc = OptionRow(ui, "players", ImVec2(px, y), pw, "MAX PLAYERS", capText, st.focusRow == focusRows);
            if (dc) {
                capIdx = (capIdx + dc + kNumCaps) % kNumCaps;
                st.sessionMaxPlayers = kCaps[capIdx];
            }
            y += 36.0f * ui.s;
            ++focusRows;
        }
        if (ff) {
            static const int kFfCaps[] = { 4, 8, 12, 16 };
            constexpr int kNumFfCaps = 4;
            int ffCapIdx = 3; // default 16
            for (int ci = 0; ci < kNumFfCaps; ++ci) {
                if (kFfCaps[ci] >= st.ffMaxPlayers) {
                    ffCapIdx = ci;
                    break;
                }
            }
            char ffCapText[32];
            std::snprintf(ffCapText, sizeof(ffCapText), "%d PLAYERS", kFfCaps[ffCapIdx]);
            const int dcFf = OptionRow(ui, "ff_players", ImVec2(px, y), pw, "MAX PLAYERS", ffCapText, st.focusRow == focusRows);
            if (dcFf) {
                ffCapIdx = (ffCapIdx + dcFf + kNumFfCaps) % kNumFfCaps;
                st.ffMaxPlayers = kFfCaps[ffCapIdx];
            }
            y += 36.0f * ui.s;
            ++focusRows;

            // The same rules editor as Custom Game. Firefight's own wave and skull options aren't mapped yet.
            const int ffOptionsChange = OptionRow(ui, "ff_options", ImVec2(px, y), pw, "GAME OPTIONS", "EDIT", st.focusRow == focusRows);
            ImGui::SetCursorScreenPos(ImVec2(px, y));
            const bool ffClicked = ImGui::InvisibleButton("##ff_options_open", ImVec2(pw * 0.48f, 30.0f * ui.s));
            if ((ffClicked || ffOptionsChange != 0 || (st.focusRow == focusRows && hit < 0 && AcceptPressed())) && !ctx.ffVariants.empty()) {
                OpenGameOptions(ctx, ctx.ffVariants[std::clamp(st.ffVariant, 0, static_cast<int>(ctx.ffVariants.size()) - 1)]);
            }
            ++focusRows;
            y += 36.0f * ui.s;
        }
        if (ff) {
            y += DifficultyTiles(ui, "ffdiff", ImVec2(px, y), pw, st.difficulty, st.focusRow == focusRows) + 8.0f * ui.s;
            ++focusRows;
        }
        if (forgeMissing) {
            Text(ui, nullptr, 16.0f, ImVec2(px + 10.0f * ui.s, y), IM_COL32(255, 190, 120, 255),
                 "MCC's Forge game type (00_basic_editing_054.bin) wasn't found.");
            y += 26.0f * ui.s;
        }
        const float lobbyY = col.strip;
        DrawLobbyControls(ui, ImVec2(px, lobbyY), pw, st);

        const bool canStart = map && map->enabled && typeConflict.empty();
        const float bw = (pw - 10.0f * ui.s) * 0.5f;
        std::string sub;
        if (!map) sub = "Pick a map";
        else if (ff) sub = "Hold out alone · " + std::string(kDifficultyNames[st.difficulty]);
        else if (forge) sub = map->name + " · Forge";
        else {
            const std::vector<MenuVariant>& list = ctx.variants;
            sub = map->name + " · " + (list.empty() ? std::string("Default") : list[std::clamp(st.variant, 0, static_cast<int>(list.size()) - 1)].label);
        }

        const char* soloLabel = ff ? "PLAY SOLO" : forge ? "SOLO FORGE" : "PLAY LOCAL";
        const char* hostLabel = ff ? "HOST CO-OP" : forge ? "HOST FORGE" : "HOST LOBBY";
        const char* hostSub = st.lobbyNetMode == 1 ? (st.lobbyPrivacy == 0 ? "Public Online P2P" : "Private Online P2P") : "Local LAN Lobby";

        const bool autoPlay = AutoPlayNow();
        const bool startSolo = ((BigAction(ui, "start", ImVec2(px, actionsY), bw, soloLabel, sub.c_str(), true, canStart, st.focusRow == focusRows) ||
                                 (st.focusRow == focusRows && AcceptPressed())) && canStart) || (autoPlay && canStart);
        // The host button is one row past the solo one, so a controller can reach it (it used to be mouse-only).
        const int hostRow = focusRows + 1;
        const bool startHost = (BigAction(ui, "host", ImVec2(px + bw + 10.0f * ui.s, actionsY), pw - bw - 10.0f * ui.s, hostLabel, hostSub, true, canStart, st.focusRow == hostRow) ||
                                (st.focusRow == hostRow && AcceptPressed())) && canStart;

        if (startHost) {
            std::string varPath, varName;
            if (ff) {
                if (!ctx.ffVariants.empty()) {
                    varPath = ctx.ffVariants[st.ffVariant].path.string();
                    varName = ctx.ffVariants[st.ffVariant].label;
                } else {
                    varName = "Firefight Standard";
                }
            } else if (forge) {
                varPath = ctx.forgeVariant.string();
                varName = "Basic Editing";
            } else {
                if (!ctx.variants.empty()) {
                    varPath = ctx.variants[st.variant].path.string();
                    varName = ctx.variants[st.variant].label;
                } else {
                    varName = "Slayer";
                }
            }
            const std::string mode = ff ? "firefight" : (forge ? "forge" : "multiplayer");
            const std::string lobbyScenario = map ? (map->launchScenario.empty() ? map->scenario : map->launchScenario)
                                                  : std::string("20_sword_slayer");
            SetupHostLobby(st, lobbyScenario, mode, varPath, varName, ff ? st.difficulty : 1, 0,
                           ff ? st.ffMaxPlayers : st.sessionMaxPlayers, map ? map->mod : nullptr, map ? map->mapOverride : std::filesystem::path(),
                           map ? map->name : std::string(), map ? map->imageLoading : std::filesystem::path(),
                           map ? map->imageThumbnail : std::filesystem::path());
            // A chosen playlist starts the lobby on its first entry.
            if (!ff && !forge && st.playlistChoice >= 0 && st.playlistChoice < static_cast<int>(st.playlists.size())) {
                const Playlists::Playlist& playlist = st.playlists[st.playlistChoice];
                st.playlistPosition = playlist.shuffle ? static_cast<int>(std::random_device{}() % playlist.entries.size()) : 0;
                ResolvedEntry resolved;
                if (ResolveEntry(ctx, playlist.entries[st.playlistPosition], playlist.randomType, resolved)) ApplyEntryToLobby(st, resolved);
            }
        } else if (startSolo) {
            choice.play = true;
            choice.host = false;
            choice.map = map->launchScenario.empty() ? map->scenario : map->launchScenario;
            choice.mapOverride = map->mapOverride;
            choice.mapTitle = map->name;
            choice.mapImage = map->imageLoading;
            choice.mapThumb = map->imageThumbnail;
            if (mod) { choice.modFolder = mod->folder; choice.modTitle = mod->title; }
            if (ff) {
                choice.mode = "firefight";
                choice.difficulty = st.difficulty;
                if (!ctx.ffVariants.empty()) choice.variantPath = ctx.ffVariants[st.ffVariant].path.string();
            } else {
                choice.mode = forge ? "forge" : "multiplayer";
                if (forge) choice.variantPath = ctx.forgeVariant.string();
                else if (!ctx.variants.empty()) choice.variantPath = ctx.variants[st.variant].path.string();
            }
            done = true;
        }
        if (st.focusRow > 0 && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_Tab) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown))) st.focusRow = std::min(hostRow, st.focusRow + 1);
        if (st.focusRow > 0 && (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickUp))) st.focusRow = std::max(1, st.focusRow - 1);
        if (back) {
            if (st.focusRow > 0) st.focusRow = 0;
            else goBack(Screen::Main);
        }
        if (forge) DrawHints(ui, { { Prompt::Accept, "Select" }, { Prompt::Back, "Back" }, { Prompt::Tabs, "Vanilla / Modded" } });
        else DrawHints(ui, { { Prompt::Accept, "Select" }, { Prompt::Back, "Back" }, { Prompt::Change, "Change" }, { Prompt::Tabs, "Vanilla / Modded" } });
        break;
    }

    case Screen::Armory: {
        // MCC Halo: Reach "CUSTOMIZATION / APPEARANCE" header + nameplate + live in-engine 3D Spartan/Elite
        ArmoryStage::Draw(ui, device, context, top + 64.0f * ui.s);
        Text(ui, NobleStyle::SmallFont(), 15.0f, ImVec2(left, top - 8.0f * ui.s), kAccent, "CUSTOMIZATION");
        Text(ui, NobleStyle::HeroFont(), 44.0f, ImVec2(left, top + 6.0f * ui.s), kWhite, "APPEARANCE");

        // Top-right: the same player row as MY PARTY on the main menu.
        {
            const auto& prof = UIManager::GetArmorCustomizer().GetProfile();
            const float rowW = 400.0f * ui.s;
            PlayerRow(ui, ImVec2(ui.origin.x + ui.size.x * 0.925f - rowW, top + 8.0f * ui.s), rowW, Career::RankIndex(Career::Get().credits),
                      prof.emblemIndex, prof.nameplateIndex, 0, PlayerServiceTag(settings), settings.playerName,
                      AuthClient::Get().IsDeveloper(), false, prof.emblemBackgroundIndex, UIManager::GetArmorCustomizer().EmblemColorsNow());
        }

        // Thin horizontal rule with active "HALO: REACH" underline
        const float ruleY = top + 64.0f * ui.s;
        ui.dl->AddLine(ImVec2(left, ruleY), ImVec2(ui.origin.x + ui.size.x * 0.925f, ruleY), IM_COL32(140, 180, 220, 55), 1.0f);
        const char* gameTab = "HALO: REACH";
        const float tabW = TextWidth(NobleStyle::TitleFont(), 18.0f * ui.s, gameTab);
        const float tabX = ui.origin.x + (ui.size.x - tabW) * 0.5f;
        Text(ui, NobleStyle::TitleFont(), 18.0f, ImVec2(tabX, ruleY - 24.0f * ui.s), kWhite, gameTab);
        ui.dl->AddLine(ImVec2(tabX - 8.0f * ui.s, ruleY), ImVec2(tabX + tabW + 8.0f * ui.s, ruleY), kAccent, 2.5f * ui.s);

        ImGui::SetCursorScreenPos(ImVec2(left, ruleY + 12.0f * ui.s));
        ImGui::BeginChild("##armory_panel", ImVec2(ui.size.x * 0.90f, ui.origin.y + ui.size.y - (ruleY + 12.0f * ui.s) - 62.0f * ui.s),
                          ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
        UIManager::GetArmorCustomizer().Render(device, context);
        ImGui::EndChild();

        if (back && !ImGui::GetIO().WantTextInput) {
            goBack(Screen::Main);
        }
        DrawHints(ui, { { Prompt::Back, "Back" }, { Prompt::Change, "Change Piece" }, { Prompt::Tabs, "Rotate Character (Q/E or Drag)" } });
        break;
    }

    case Screen::Servers:
    case Screen::Settings:
    case Screen::Career:
    case Screen::Mods: {
        const char* title = st.screen == Screen::Servers ? "SERVER BROWSER" : st.screen == Screen::Settings ? "SETTINGS"
                          : st.screen == Screen::Career ? "SERVICE RECORD" : "MODS";
        // These screens use the whole screen, like Settings.
        const bool full = true;
        const float px = full ? ui.origin.x + 24.0f * ui.s : left;
        const float py = full ? ui.origin.y + 86.0f * ui.s : top + 84.0f * ui.s;
        if (full) {
            ui.dl->AddRectFilled(ui.origin, ImVec2(ui.origin.x + ui.size.x, ui.origin.y + ui.size.y), IM_COL32(4, 8, 12, 215));
            DrawTitle(ui, ImVec2(px, ui.origin.y + 10.0f * ui.s), title);
        } else {
            DrawTitle(ui, ImVec2(left, top), title);
        }
        ImGui::SetCursorScreenPos(ImVec2(px, py));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.05f, 0.07f, full ? 0.60f : 0.82f));
        const float panelW = full ? (ui.size.x - 48.0f * ui.s) : (ui.origin.x + ui.size.x * 0.925f - px);
        const float panelH = ui.origin.y + ui.size.y - py - 62.0f * ui.s;
        ImGui::BeginChild("##panel", ImVec2(panelW, panelH),
                          ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
        if (st.screen == Screen::Servers) {
            UIManager::GetServerBrowser().Render();
        } else if (st.screen == Screen::Settings) {
            ctx.settingsPanel.Render();
        } else if (st.screen == Screen::Career) {
            ctx.careerPanel.Render();
        } else {
            if (!ctx.modsScanned) {
                const auto steamapps = ctx.reachDir.parent_path().parent_path().parent_path();
                wchar_t exe[MAX_PATH] = {};
                GetModuleFileNameW(nullptr, exe, MAX_PATH);
                UIManager::GetWorkshopManager().RefreshMods(steamapps / "workshop" / "content" / "976730",
                                                            std::filesystem::path(exe).parent_path() / "data" / "mods");
                ctx.modsScanned = true;
            }
            UIManager::GetWorkshopManager().Render();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        // Esc with unsaved changes discards them (the Settings prompt says so) and stays on the page; B always goes back.
        const bool escDiscards = ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ctx.settingsPanel.IsCapturingKey() &&
                                 !ImGui::GetIO().WantTextInput && ctx.settingsPanel.DiscardOnEscape();
        if (back && !escDiscards && !ImGui::GetIO().WantTextInput && !ctx.settingsPanel.IsCapturingKey()) {
            // The server browser is a top-level main-menu screen, so Back returns to the main menu (it used
            // to drop the player into Custom Game).
            goBack(Screen::Main);
        }
        if (st.screen == Screen::Settings) DrawHints(ui, { { Prompt::Accept, "Select" }, { Prompt::Back, "Back" }, { Prompt::Change, "Change" }, { Prompt::Tabs, "Category" } }, {}, 0.01875f);   // the full-width panel's edge
        else if (st.screen == Screen::Career) DrawHints(ui, { { Prompt::Back, "Back" }, { Prompt::Tabs, "Switch Tab" } }, {}, 0.01875f);
        else if (st.screen == Screen::Servers) DrawHints(ui, { { Prompt::Accept, "Join" }, { Prompt::Back, "Back" }, { Prompt::Tabs, "Mode Filter" }, { Prompt::Change, "Pick Filter" } }, { { InputPrompts::Button::Back, "CLICK", "Toggle Filter" } }, 0.01875f);
        else DrawHints(ui, { { Prompt::Back, "Back" } }, {}, 0.01875f);
        break;
    }

    case Screen::PreGameLobby: {
        DrawPreGameLobby(ctx, ui, left, top, choice, done, back);
        break;
    }

    case Screen::Social: {
        DrawSocialScreen(ctx, ui, left, top, choice, done, back);
        break;
    }
    }

    // Friend invites reach the player on any screen (Settings > Online services can turn this off).
    DrawInviteToast(ui, st);

    // The Workshop-mod gate sits above every screen: the Server browser stays behind it until the
    // player downloads the lobby's mod or cancels.
    ModDownloadPrompt::Get().Render();

    ImGui::End();
}

} // namespace

bool FrontEndMenu::ShouldShow() {
    bool hasMap = false;
    bool menuRequested = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i + 1 < argc; ++i) {
            const std::wstring arg = argv[i];
            const std::wstring value = Lower(argv[i + 1]);
            if (arg == L"-map") {
                hasMap = true;
                if (value == L"mainmenu" || value == L"menu") menuRequested = true;
            } else if (arg == L"-mode" && (value == L"mainmenu" || value == L"menu")) {
                menuRequested = true;
            }
        }
        LocalFree(argv);
    }
    return !hasMap || menuRequested;
}

void FrontEndMenu::Initialize(const std::filesystem::path& reachDir) {
    OverlayContext& ctx = GetOverlayCtx();
    ctx.reachDir = reachDir;
    // UiArt::Init() has run by now (main.cpp initialises the art before the window is shown). A plate
    // retired or hidden in this build must not survive in the saved appearance; do it before the
    // first PlayerRow draws, so the main menu never shows a plate the Armory would refuse.
    UIManager::GetArmorCustomizer().ValidateNameplate();
    ctx.variants = ScanVariants(reachDir, false);
    ctx.ffVariants = ScanVariants(reachDir, true);
    ctx.forgeVariant = reachDir / "game_variants" / kForgeVariant;

    RefreshModCatalog(ctx);

    ctx.st = MenuState{};
    ctx.st.variant = FindVariant(ctx.variants, L"slayer_054.bin");
    ctx.st.ffVariant = FindVariant(ctx.ffVariants, L"hr_ff_standard_1set.bin");
    ctx.st.skulls = Settings::Get().Snapshot().campaignSkulls;

    ReachAudio::SetMusicVolume(Settings::Get().Snapshot().volumeMusic);
    ReachAudio::PlayMenuMusic(Settings::Get().Snapshot().menuMusicTrack);

    int startTab = 0;
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i + 1 < argc; ++i) {
            if (std::wstring(argv[i]) == L"-menu-screen") ctx.st.screen = ParseScreenArg(argv[i + 1]);
            if (std::wstring(argv[i]) == L"-menu-capture") ctx.captureFrame = _wtoi64(argv[i + 1]);
            if (std::wstring(argv[i]) == L"-menu-tab") startTab = _wtoi(argv[i + 1]);
        }
        if (argv) LocalFree(argv);
    }
    // Test-only: "-lobby-demo host|guest [-lobby-demo-players N] [-lobby-demo-red N] [-lobby-demo-ffa 1] [-lobby-demo-mode campaign|firefight|forge]" opens
    // the pre-game lobby with a made-up roster and no network, so a capture can show every state on one PC.
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::wstring demo, demoMode, demoMap;
        int demoPlayers = 1;
        int demoRed = -1;
        bool demoFfa = false;
        for (int i = 1; argv && i + 1 < argc; ++i) {
            if (_wcsicmp(argv[i], L"-lobby-demo") == 0) demo = Lower(argv[i + 1]);
            if (_wcsicmp(argv[i], L"-lobby-demo-players") == 0) demoPlayers = std::clamp(_wtoi(argv[i + 1]), 0, 128);
            if (_wcsicmp(argv[i], L"-lobby-demo-mode") == 0) demoMode = Lower(argv[i + 1]);
            if (_wcsicmp(argv[i], L"-lobby-demo-map") == 0) demoMap = argv[i + 1];
            if (_wcsicmp(argv[i], L"-lobby-demo-red") == 0) demoRed = _wtoi(argv[i + 1]);   // that many red, the rest blue
            if (_wcsicmp(argv[i], L"-lobby-demo-ffa") == 0) demoFfa = _wtoi(argv[i + 1]) != 0;
        }
        if (argv) LocalFree(argv);
        if (!demo.empty()) {
            MenuState& st = ctx.st;
            st.screen = Screen::PreGameLobby;
            st.isLobbyHost = demo == L"host";
            st.lobbyHostEndpoint = (st.isLobbyHost || demoPlayers == 0) ? "" : "demo";
            if (demoMode == L"campaign") { st.lobbyMode = "campaign"; st.lobbyMap = kMissions[0].scenario; st.lobbyMaxPlayers = 16; }
            else if (demoMode == L"firefight") { st.lobbyMode = "firefight"; st.lobbyMap = kFirefightMaps[0].scenario; st.lobbyVariantName = "Firefight"; }
            else if (demoMode == L"forge") { st.lobbyMode = "forge"; }
            if (const wchar_t* line = wcsstr(GetCommandLineW(), L"-lobby-demo-unknown-mode")) { (void)line; st.lobbyVariantName = "Someone's Own Mode"; }
            st.lobbyMaxPlayers = std::max(st.lobbyMaxPlayers, demoPlayers);
            if (!st.isLobbyHost && demoPlayers > 0) st.lobbyRosterSelection = demoPlayers > 1 ? 1 : 0;
            for (int i = 0; i < demoPlayers; ++i) {
                LobbyPlayer p;
                p.peerId = static_cast<uint64_t>(i + 1);
                p.isHost = i == 0;
                p.username = i == 0 ? "Noble Six" : "Spartan " + std::to_string(i + 1);
                p.serviceTag = "N" + std::to_string(i + 10);
                if (i > 0) {   // other players carry their own rank, emblem, background and nameplate, as the roster does
                    p.rankIndex = i % 30;
                    p.emblemId = (i * 7) % 60 + 1;
                    p.emblemBackgroundId = (i * 3) % 40 + 1;
                    p.nameplateId = i % 20 + 1;
                }
                p.team = demoFfa ? 0 : demoRed >= 0 ? (i < demoRed ? 1 : 2) : demoPlayers > 2 ? (i % 2) + 1 : 0;
                st.lobbyRoster.push_back(p);
            }
            if (!demoMap.empty()) {
                std::string scenario;
                for (const wchar_t c : demoMap) {
                    if (c < 128) scenario.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
                }
                const ModEntry* foundMod = nullptr;
                const ModMap* foundMap = nullptr;
                int foundKind = -1;
                for (const ModEntry& mod : ctx.mods) {
                    for (int kind = 0; kind < 3 && !foundMap; ++kind) {
                        const std::vector<ModMap>& list = kind == 0 ? mod.campaign
                                                           : kind == 1 ? mod.firefight
                                                                       : mod.multiplayer;
                        for (const ModMap& m : list) {
                            if (m.scenario == scenario && m.hasMapFile) {
                                foundMod = &mod;
                                foundMap = &m;
                                foundKind = kind;
                                break;
                            }
                        }
                    }
                    if (foundMap) break;
                }
                if (foundMap) {
                    st.lobbyMode = foundKind == 0 ? "campaign"
                                   : foundKind == 1 ? "firefight"
                                   : demoMode == L"forge" ? "forge" : "multiplayer";
                    const std::string carrier = foundKind == 0 ? "m10" : foundKind == 1 ? "ff60_icecave" : "70_boneyard";
                    st.lobbyMap = CarrierForScenario(foundMap->scenario, carrier);
                    st.lobbyMapOverride = foundMod->folder / "maps" / (foundMap->scenario + ".map");
                    st.lobbyMapTitle = foundMap->title;
                    st.lobbyMapImage = foundMap->imageLarge.empty() ? foundMap->imageThumbnail : foundMap->imageLarge;
                    st.lobbyMapThumb = foundMap->imageThumbnail;
                    st.lobbyModFolder = foundMod->folder;
                    st.lobbyModId = foundMod->workshopId;
                    st.lobbyModTitle = foundMod->title;
                    NOBLE_INFO("FrontEndMenu: -lobby-demo-map {} -> lobby '{}' via '{}'", scenario,
                               foundMap->title, st.lobbyMap);
                } else {
                    NOBLE_WARN("FrontEndMenu: -lobby-demo-map {} not found in any installed mod", scenario);
                }
            }
            NOBLE_INFO("FrontEndMenu: -lobby-demo host={} players={} mode={}", st.isLobbyHost, demoPlayers, st.lobbyMode);
        }
    }
    // Test-only: open the "Workshop mod required" dialog on a fake missing item so -menu-capture can
    // photograph it without a second machine hosting a modded lobby.
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i < argc; ++i) {
            if (_wcsicmp(argv[i], L"-mod-prompt-demo") == 0) {
                ModDownloadPrompt::Get().Show("9999999999", "CUSTOM MAP PACK", {}, true);
            }
            // Test-only: a real Workshop id, so the Steam Web API picture / download size can be captured.
            if (_wcsicmp(argv[i], L"-mod-prompt-id") == 0 && i + 1 < argc) {
                std::string id;
                for (const wchar_t* c = argv[i + 1]; c && *c; ++c) id.push_back(static_cast<char>(*c));
                ModDownloadPrompt::Get().Show(id, "Workshop mod", {}, false);
            }
        }
        if (argv) LocalFree(argv);
    }
    // Test-only: "-open-mods" expands every Workshop mod heading, so -auto-play-map / -menu-capture can
    // reach the modded map rows without a click.
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        bool openAll = false;
        for (int i = 1; argv && i < argc; ++i) {
            if (_wcsicmp(argv[i], L"-open-mods") == 0) openAll = true;
        }
        if (argv) LocalFree(argv);
        if (openAll) {
            for (const ModEntry& mod : ctx.mods) {
                ctx.st.openMods.insert(OpenKey(ModMode::Campaign, mod));
                ctx.st.openMods.insert(OpenKey(ModMode::Firefight, mod));
                ctx.st.openMods.insert(OpenKey(ModMode::Multiplayer, mod));
            }
            NOBLE_INFO("FrontEndMenu: -open-mods expanded {} mod heading(s)", ctx.mods.size());
        }
    }
    ctx.settingsPanel.SetTab(startTab);
    ctx.careerPanel.SetTab(startTab);
    // -menu-tab also picks the VANILLA / MODDED source tab of the setup screens, so a test run can capture
    // the modded lists (long workshop names and descriptions) without clicking.
    switch (ctx.st.screen) {
    case Screen::Campaign:   ctx.st.campaignSource = startTab ? 1 : 0; break;
    case Screen::Firefight:  ctx.st.ffSource = startTab ? 1 : 0; break;
    case Screen::CustomGame: ctx.st.mpSource = startTab ? 1 : 0; break;
    case Screen::Forge:      ctx.st.forgeSource = startTab ? 1 : 0; break;
    default: break;
    }
    ctx.modsScanned = false;
    ctx.totalFrames = 0;
    ctx.sceneReadyFrames = 0;
    ctx.closed = false;
    ctx.initialized = true;

    // Menu background: pick and upload the texture here, on the engine thread that owns the device. The
    // fallback draw path (HoldFrame, main thread) must never create textures - it used to hang there.
    UiArt::WarmMenuBackground();

    if (ctx.st.screen == Screen::Armory) {
        UIManager::GetArmorCustomizer().SyncToReachArmory();
    }

    // Server Browser Join Pre-Game Lobby Handler. A lobby whose map comes from a Steam Workshop mod is
    // checked BEFORE joining: a player without the item gets Download & Join / Cancel, and only starts the
    // lobby join once the mod is in the Workshop cache. (The old behaviour walked straight into a lobby
    // whose map the guest could not load.)
    auto enterLobby = [&ctx](const ServerEntry& s, const std::string& modId, const std::string& modTitle,
                             const std::filesystem::path& modFolder) {
        ctx.st.isLobbyHost = false;
        ctx.st.lobbySessionId = s.id;
        ctx.st.lobbyHostEndpoint = s.hostAddress + ":" + std::to_string(s.port);
        ctx.st.lobbyMap = s.mapName.empty() ? "20_sword_slayer" : s.mapName;
        ctx.st.lobbyMode = s.gameMode.empty() ? "multiplayer" : s.gameMode;
        ctx.st.lobbyVariantName = s.variantName.empty() ? "Default" : s.variantName;
        ctx.st.lobbyMaxPlayers = s.maxPlayers;
        ctx.st.lobbyModId = modId;
        ctx.st.lobbyModTitle = modTitle;
        ctx.st.lobbyModFolder = modFolder;
        ctx.st.lobbyRoster.clear();
        ctx.st.lobbyLaunchPending = false;
        {   // a launch or roster left over from an earlier lobby doesn't apply to this one
            std::lock_guard<std::mutex> lock(s_lobbyEventMutex);
            s_lobbyEvents = PendingLobbyEvents{};
        }
        ctx.st.lastLobbyPoll = ImGui::GetTime();
        NOBLE_INFO("ServerBrowser join: lobby '{}' ({}:{}) mod='{}' folder='{}'", s.name, s.hostAddress, s.port,
                   modId, modFolder.string());

        std::string pName = AuthClient::Get().IsLoggedIn() ? AuthClient::Get().GetUsername() : Settings::Get().Snapshot().playerName;
        if (pName.empty()) pName = "Guest";
        uint64_t fallbackId = (MasterServerClient::GetLobbyMachineId() & 0x7FFFFFFFULL);
        if (fallbackId < 2ULL) fallbackId = 2ULL;
        uint64_t pId = AuthClient::Get().IsLoggedIn() ? static_cast<uint64_t>(AuthClient::Get().GetUserId()) : fallbackId;
        std::string sTag = Settings::Get().Snapshot().serviceTag.empty() ? "117" : Settings::Get().Snapshot().serviceTag;

        LobbyState state = MasterServerClient::Get().JoinLobbySync(
            s.id, pId, pName, AuthClient::Get().IsDeveloper(), sTag,
            Career::RankIndex(Career::Get().credits),
            UIManager::GetArmorCustomizer().GetProfile().emblemIndex,
            UIManager::GetArmorCustomizer().GetProfile().emblemBackgroundIndex,
            UIManager::GetArmorCustomizer().GetProfile().nameplateIndex, 0,
            11774, s.hostAddress, s.port);
        if (state.ok) {
            if (!state.sessionId.empty()) ctx.st.lobbySessionId = state.sessionId;
            ctx.st.lobbyRoster = state.roster;
            ctx.st.lobbyMap = state.map;
            ctx.st.lobbyMode = state.mode;
            ctx.st.lobbyVariantName = state.variant;
            ctx.st.lobbyDifficulty = state.difficulty;
            ctx.st.lobbySkulls = state.skulls;
            if (!state.modTitle.empty()) ctx.st.lobbyModTitle = state.modTitle;
            if (state.isLan && !state.hostLanIp.empty()) {
                ctx.st.lobbyHostEndpoint = state.hostLanIp + ":" + std::to_string(state.hostPort);
            }
        } else {
            LobbyPlayer hostP;
            hostP.peerId = 1;
            hostP.username = s.name;
            hostP.isHost = true;
            hostP.ping = s.ping >= 0 ? s.ping : 25;
            ctx.st.lobbyRoster.push_back(hostP);

            LobbyPlayer me;
            me.peerId = pId;
            me.username = pName;
            me.serviceTag = sTag;
            me.isHost = false;
            me.isDev = AuthClient::Get().IsDeveloper();
            me.ping = s.ping >= 0 ? s.ping : 25;
            ctx.st.lobbyRoster.push_back(me);
        }

        ctx.st.screen = Screen::PreGameLobby;
    };

    RefreshPlaylists(ctx);
    // "-lobby-return": a match that just ended came back to its lobby, in this new process. Each match is its own process, so
    // the lobby's lasting part is the server's session: the host registers the same address and port again, which keeps
    // the session id and roster, and guests join it again once the host is back (PauseMenu's post-game panel).
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::map<std::wstring, std::wstring> back;
        for (int i = 1; argv && i + 1 < argc; ++i) {
            if (_wcsnicmp(argv[i], L"-lobby-", 7) == 0) back[Lower(argv[i])] = argv[i + 1];
        }
        if (argv) LocalFree(argv);
        const auto text = [&](const wchar_t* key) {
            const auto it = back.find(key);
            if (it == back.end()) return std::string();
            char narrow[1024] = {};
            WideCharToMultiByte(CP_UTF8, 0, it->second.c_str(), -1, narrow, sizeof(narrow) - 1, nullptr, nullptr);
            return std::string(narrow);
        };
        const auto path = [&](const wchar_t* key) {
            const auto it = back.find(key);
            return it == back.end() ? std::filesystem::path() : std::filesystem::path(it->second);
        };
        const std::string role = text(L"-lobby-return");
        if (!role.empty()) {
            const std::filesystem::path modFolder = path(L"-lobby-mod-folder");
            NOBLE_INFO("FrontEndMenu: back from a match as {} of session '{}'", role, text(L"-lobby-session"));
            if (role == "host") {
                const std::filesystem::path variant = path(L"-lobby-variant");
                std::string varName = text(L"-lobby-variant-name");
                for (const auto* list : { &ctx.variants, &ctx.ffVariants }) {
                    for (const MenuVariant& v : *list) if (v.path == variant) varName = v.label;
                }
                const ModEntry* mod = nullptr;
                for (const ModEntry& m : ctx.mods) if (!modFolder.empty() && m.folder == modFolder) mod = &m;
                ctx.st.lobbyNetMode = text(L"-lobby-net") == "lan" ? 0 : 1;
                ctx.st.lobbyPrivacy = text(L"-lobby-privacy") == "private" ? 1 : 0;
                const std::string maxPlayers = text(L"-lobby-max-players");
                const std::string difficulty = text(L"-lobby-difficulty");
                const std::string skulls = text(L"-lobby-skulls");
                SetupHostLobby(ctx.st, text(L"-lobby-map"), text(L"-lobby-mode"), variant.string(), varName,
                               difficulty.empty() ? 1 : std::atoi(difficulty.c_str()), skulls.empty() ? 0 : std::strtoull(skulls.c_str(), nullptr, 10),
                               maxPlayers.empty() ? 16 : std::atoi(maxPlayers.c_str()), mod, path(L"-lobby-map-override"), text(L"-lobby-map-title"),
                               path(L"-lobby-map-image"), path(L"-lobby-map-thumb"),
                               text(L"-lobby-session"), text(L"-lobby-session-token"));
                // The playlist carries on: NEXT IN ROTATION moves to its next entry, RETURN TO LOBBY stays on this one.
                const std::filesystem::path listFile = path(L"-lobby-playlist");
                for (size_t i = 0; i < ctx.st.playlists.size(); ++i) {
                    if (!listFile.empty() && ctx.st.playlists[i].file == listFile) ctx.st.playlistChoice = static_cast<int>(i);
                }
                if (ctx.st.playlistChoice >= 0) {
                    const Playlists::Playlist& playlist = ctx.st.playlists[ctx.st.playlistChoice];
                    const std::string position = text(L"-lobby-playlist-pos");
                    int at = position.empty() ? 0 : std::atoi(position.c_str());
                    if (text(L"-lobby-advance") == "1" && playlist.autoStart > 0) ctx.st.autoStartAt = playlist.autoStart + 2.0;   // ImGui time starts at 0 here
                    if (text(L"-lobby-advance") == "1" && !playlist.entries.empty()) {
                        const int n = static_cast<int>(playlist.entries.size());
                        if (playlist.shuffle && n > 1) {
                            const int was = at;
                            while (at == was) at = static_cast<int>(std::random_device{}() % n);
                        } else {
                            at = (at + 1) % n;
                        }
                        ResolvedEntry resolved;
                        if (ResolveEntry(ctx, playlist.entries[at], playlist.randomType, resolved)) ApplyEntryToLobby(ctx.st, resolved);
                    }
                    ctx.st.playlistPosition = at;
                }
            } else {
                ServerEntry s;
                s.id = text(L"-lobby-session");
                const std::string endpoint = text(L"-lobby-endpoint");
                const size_t colon = endpoint.rfind(':');
                s.hostAddress = endpoint.substr(0, colon);
                s.port = static_cast<uint16_t>(colon == std::string::npos ? 11774 : std::atoi(endpoint.c_str() + colon + 1));
                s.mapName = text(L"-lobby-map");
                s.gameMode = text(L"-lobby-mode");
                s.maxPlayers = 16;
                enterLobby(s, std::string(), std::string(), modFolder);
            }
        }
    }

    UIManager::GetServerBrowser().SetJoinLobbyHandler([&ctx, enterLobby](const ServerEntry& s) {
        std::string modId = s.modId;
        std::string modTitle = s.modTitle;
        // The list refreshes every two seconds; if the host picked its mod after the last refresh, ask the
        // lobby endpoint once before letting the player in.
        if (modId.empty() && s.isMod && !s.id.empty() && MasterServerClient::Get().Enabled()) {
            const LobbyState peek = MasterServerClient::Get().FetchLobbyStateSync(s.id);
            if (peek.ok) {
                modId = peek.modId;
                modTitle = peek.modTitle;
            }
        }

        std::filesystem::path modFolder;
        if (!modId.empty()) {
            if (const ModEntry* installed = ModCatalog::FindByWorkshopId(ctx.mods, modId)) {
                modFolder = installed->folder;
                if (modTitle.empty()) modTitle = installed->title;
            }
            if (modFolder.empty()) modFolder = SteamWorkshop::FindInstalledFolder(modId);
            if (modFolder.empty()) {
                ModDownloadPrompt::Get().Show(modId, modTitle,
                    [&ctx, s, enterLobby, modId, modTitle](const std::filesystem::path& downloaded) {
                        RefreshModCatalog(ctx);   // the new item belongs in the setup screens and the Mods list
                        enterLobby(s, modId, modTitle, downloaded);
                    });
                return;
            }
        }
        enterLobby(s, modId, modTitle, modFolder);
    });

    // Real-time WebSocket Lobby Callbacks
    // These run on the WebSocket relay thread: they only queue the event (see PendingLobbyEvents).
    NetworkTransport::Get().SetLobbyLaunchCallback([](const std::string& lMap, const std::string& lMode,
                                                      const std::string& lVar, int lDiff, uint64_t lSkulls,
                                                      const std::string& hIp, uint16_t hPort, const std::string& sId) {
        NOBLE_INFO("FrontEndMenu: Received WebSocket real-time lobby launch event! (map: {}, mode: {})", lMap, lMode);
        std::lock_guard<std::mutex> lock(s_lobbyEventMutex);
        if (s_lobbyEvents.launch) return;   // the host's launch can arrive twice
        s_lobbyEvents.launch = true;
        s_lobbyEvents.map = lMap;
        s_lobbyEvents.mode = lMode;
        s_lobbyEvents.variant = lVar;
        s_lobbyEvents.difficulty = lDiff;
        s_lobbyEvents.skulls = lSkulls;
        if (!hIp.empty()) s_lobbyEvents.hostEndpoint = hIp + ":" + std::to_string(hPort);
        s_lobbyEvents.sessionId = sId;
    });

    NetworkTransport::Get().SetLobbyClosedCallback([](const std::string& reason) {
        NOBLE_INFO("FrontEndMenu: Received lobby_closed event! (reason: {})", reason);
        std::lock_guard<std::mutex> lock(s_lobbyEventMutex);
        s_lobbyEvents.closed = true;
        s_lobbyEvents.closeReason = reason;
    });

    NetworkTransport::Get().SetLobbyRosterCallback([](const std::string& rosterJson) {
        try {
            auto j = nlohmann::json::parse(rosterJson);
            if (j.contains("roster") && j["roster"].is_array()) {
                std::vector<LobbyPlayer> updated;
                for (const auto& item : j["roster"]) {
                    LobbyPlayer p;
                    p.peerId = item.value("peerId", 1ULL);
                    p.machineId = item.value("machineId", "");
                    p.username = item.value("username", "Spartan");
                    p.serviceTag = item.value("serviceTag", "117");
                    p.rankIndex = item.value("rankIndex", -1);
                    p.emblemId = item.value("emblemId", 0);
                    p.emblemBackgroundId = item.value("emblemBackgroundId", 0);
                    p.nameplateId = item.value("nameplateId", 0);
                    p.team = item.value("team", 0);
                    p.isHost = item.value("isHost", false);
                    p.isDev = item.value("isDev", false);
                    p.ping = item.value("ping", 0);
                    try {
                        const uint64_t machine = p.machineId.empty() ? 0 : std::stoull(p.machineId, nullptr, 0);
                        if (machine != 0) SocialOverlay::SetPlayerName(machine, p.username);
                        if (machine != 0 && machine == NetworkTransport::Get().GetRelayMachineId()) {
                            VoiceChat::SetLocalTeam(p.team);
                        }
                    } catch (...) {}
                    updated.push_back(p);
                }
                std::lock_guard<std::mutex> lock(s_lobbyEventMutex);
                s_lobbyEvents.roster = true;
                s_lobbyEvents.rosterPlayers = std::move(updated);
            }
        } catch (...) {}
    });

    UpdateSceneForScreen(ctx.st.screen);

    NOBLE_INFO("FrontEndMenu: initialized in-engine 3D menu overlay ({} game variants, {} Firefight variants)",
               ctx.variants.size(), ctx.ffVariants.size());
}

void FrontEndMenu::RenderOverlay(IDXGISwapChain* swapChain) {
    if (!swapChain) return;
    s_overlaySwapChain.store(swapChain);
    s_lastEngineOverlayFrame.store(GetTickCount64());
    std::lock_guard<std::mutex> lock(s_overlayMutex);
    RenderOverlayImpl(swapChain, false);
}

bool FrontEndMenu::HoldFrame() {
    OverlayContext& ctx = GetOverlayCtx();
    if (!ctx.initialized || ctx.closed || !ReachArmory::IsMenuMode()) return false;
    const ULONGLONG last = s_lastEngineOverlayFrame.load();
    IDXGISwapChain* swapChain = s_overlaySwapChain.load();
    if (!swapChain || last == 0 || GetTickCount64() - last < 1000) return false;
    static bool s_logged = false;
    if (!s_logged) {
        s_logged = true;
        NOBLE_INFO("FrontEndMenu: engine stopped drawing the menu; Noble draws it itself");
    }
    {
        std::lock_guard<std::mutex> lock(s_overlayMutex);
        RenderOverlayImpl(swapChain, true);
        FrameCapture::OnFrame(nullptr, 0);   // keeps -quit-after working in test runs
    }
    swapChain->Present(1, 0);
    return true;
}

void FrontEndMenu::RenderOverlayImpl(IDXGISwapChain* swapChain, bool clear) {
    OverlayContext& ctx = GetOverlayCtx();
    if (!ctx.initialized || ctx.closed || !swapChain || !ImGui::GetCurrentContext()) return;

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(&desc)) || !desc.OutputWindow) return;
    const HWND hwnd = desc.OutputWindow;

    ID3D11Device* device = nullptr;
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))) || !device) return;
    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);

    ID3D11Texture2D* backBuffer = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    if (SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer))) && backBuffer) {
        device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    }

    if (context && rtv) {
        D3D11_TEXTURE2D_DESC bbDesc{};
        backBuffer->GetDesc(&bbDesc);

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.DisplaySize = ImVec2(static_cast<float>(bbDesc.Width), static_cast<float>(bbDesc.Height));

        static LARGE_INTEGER s_freq{}, s_last{};
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        if (s_freq.QuadPart == 0) QueryPerformanceFrequency(&s_freq);
        float dt = s_last.QuadPart ? static_cast<float>(now.QuadPart - s_last.QuadPart) / static_cast<float>(s_freq.QuadPart) : 1.0f / 60.0f;
        if (dt <= 0.0f || dt > 0.25f) dt = 1.0f / 60.0f;
        s_last = now;
        io.DeltaTime = dt;

        ArmoryStage::s_frameRtv = clear ? nullptr : rtv;
        FeedEngineThreadInputs(hwnd, io);
        if (clear) {
            // No engine frame underneath: start from the menu's dark backdrop colour.
            const float black[4] = { 0.02f, 0.03f, 0.05f, 1.0f };
            context->ClearRenderTargetView(rtv, black);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        // ImGui_ImplWin32_NewFrame() overwrites io.DisplaySize with the window's client size. The menu
        // process renders a larger back buffer than its window (menu supersampling, see main.cpp) and
        // Windows downscales it; laying the UI out in back-buffer pixels keeps it pixel-aligned and
        // crisp too. A match process is 1:1. FeedEngineThreadInputs() above already scaled the mouse.
        io.DisplaySize = ImVec2(static_cast<float>(bbDesc.Width), static_cast<float>(bbDesc.Height));
        {
            RECT client{};
            GetClientRect(hwnd, &client);
            ArmoryStage::s_windowScale = client.bottom > 0
                ? static_cast<float>(bbDesc.Height) / static_cast<float>(client.bottom)
                : 1.0f;
            static LONG s_lastClientW = -1, s_lastClientH = -1;
            if (client.right != s_lastClientW || client.bottom != s_lastClientH) {
                s_lastClientW = client.right;
                s_lastClientH = client.bottom;
                if (client.right > 0 && client.bottom > 0 &&
                    (client.right != static_cast<LONG>(bbDesc.Width) || client.bottom != static_cast<LONG>(bbDesc.Height))) {
                    NOBLE_INFO("FrontEndMenu: window client {}x{}, menu back buffer {}x{} ({:.2f}x supersample)",
                               client.right, client.bottom, bbDesc.Width, bbDesc.Height, ArmoryStage::s_windowScale);
                }
            }
        }
        // The Win32 backend queues client-pixel mouse positions from WM_MOUSEMOVE; this is the last
        // mouse event before NewFrame, so a back-buffer position wins and hover/click lands on the
        // control actually under the cursor (window and back buffer are different sizes now).
        {
            RECT client{};
            GetClientRect(hwnd, &client);
            POINT cursor{};
            if (GetForegroundWindow() == hwnd && client.right > 0 && client.bottom > 0 &&
                GetCursorPos(&cursor) && ScreenToClient(hwnd, &cursor)) {
                const float sx = io.DisplaySize.x / static_cast<float>(client.right);
                const float sy = io.DisplaySize.y / static_cast<float>(client.bottom);
                io.AddMousePosEvent(static_cast<float>(cursor.x) * sx, static_cast<float>(cursor.y) * sy);
            } else {
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            }
        }
        UiPad::Feed(io);
        SocialOverlay::Poll(ctx.st.screen == Screen::PreGameLobby);
        // Text drawn by plain ImGui widgets (search boxes, descriptions, help lines, input fields) used the
        // fonts' fixed 15-18 px and turned tiny on a native 4K back buffer. Scale it like every custom-drawn
        // label (back-buffer height / 900). Fonts are rasterised at 2.5x density, so this stays crisp.
        io.FontGlobalScale = NobleUi::Scale();
        ImGui::NewFrame();
        if (SocialOverlay::IsTyping()) io.ClearInputKeys();   // the line takes the keyboard, not the screen under it
        InputPrompts::Update();
    // Master mute / mute in background: Noble's own audio only (menu music and menu sounds). Read twice a second
    // rather than every frame, and applied here so it also covers the pause menu and the fallback draw path.
    {
        static ULONGLONG s_nextMuteCheck = 0;
        const ULONGLONG muteNow = GetTickCount64();
        if (muteNow >= s_nextMuteCheck) {
            s_nextMuteCheck = muteNow + 500;
            const SettingsData audio = Settings::Get().Snapshot();
            bool muted = audio.masterMute || s_menuMusicMuted;
            bool bgMuted = false;
            if (!muted && audio.muteInBackground) {
                // "Active" means the foreground window belongs to this process: that covers the menu window and
                // the engine's, without having to know which one is up.
                const HWND foreground = GetForegroundWindow();
                DWORD ownerPid = 0;
                if (foreground) GetWindowThreadProcessId(foreground, &ownerPid);
                bgMuted = foreground != nullptr && ownerPid != GetCurrentProcessId();
                if (bgMuted) muted = true;
            }
            ReachAudio::SetMuted(muted);
            ProcessAudio::ApplyVolume(audio.volumeGame, muted);
        }
    }

        FrontEndChoice choice;
        bool done = false;
        try {
            DrawFrontEndContents(ctx, device, context, choice, done);
            SocialOverlay::Draw(8.0f, 196.0f, 900.0f);   // voice above the title, chat over the Friends panel
        } catch (const std::exception& e) {
            NOBLE_ERROR("FrontEndMenu: exception in menu loop: {}", e.what());
        } catch (...) {
            NOBLE_ERROR("FrontEndMenu: unknown exception in menu loop");
        }

        ImGui::Render();

        ID3D11RenderTargetView* oldRtv = nullptr;
        ID3D11DepthStencilView* oldDsv = nullptr;
        context->OMGetRenderTargets(1, &oldRtv, &oldDsv);
        context->OMSetRenderTargets(1, &rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        context->OMSetRenderTargets(1, &oldRtv, oldDsv);
        if (oldRtv) oldRtv->Release();
        if (oldDsv) oldDsv->Release();

        ++ctx.totalFrames;
        if (ReachArmory::IsSceneReady()) {
            ++ctx.sceneReadyFrames;
        }

        if (ctx.captureFrame > 0) {
            const bool readyToCapture = (ctx.sceneReadyFrames >= ctx.captureFrame) || (ctx.totalFrames >= ctx.captureFrame + 480);
            if (readyToCapture) {
                FrameCapture::SaveNow(swapChain, ctx.totalFrames);
                NOBLE_INFO("FrontEndMenu: -menu-capture done at frame {} (sceneReadyFrames={}), closing",
                           ctx.totalFrames, ctx.sceneReadyFrames);
                ctx.closed = true;
                ReachAudio::StopMusic();
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            }
        }

        if (done && !ctx.closed) {
            ctx.closed = true;
            ReachAudio::StopMusic();
            if (choice.play) {
                // Remember what was launched, for the \"Continue\" row on the main menu (spec 4.1). A lobby join
                // (connectAddress) is not a local game, so it does not overwrite this.
                // CONTINUE is a campaign feature (MCC's Continue = the campaign mission you were on), so only a
                // campaign launch updates it; Firefight, Forge, multiplayer and lobby joins leave it alone.
                if (choice.connectAddress.empty() && !choice.map.empty() && choice.mode == "campaign") {
                    SettingsData d = Settings::Get().Snapshot();
                    d.lastPlayedMode = choice.mode;
                    d.lastPlayedMap = choice.map;
                    d.lastPlayedVariant = choice.variantPath;
                    d.lastPlayedDifficulty = choice.difficulty;
                    d.lastPlayedSkulls = choice.skulls;
                    d.lastPlayedMod = choice.modFolder.empty() ? std::string() : choice.modFolder.string();
                    Settings::Get().Update(d);
                }
                std::wstring args = L"-map \"" + ToWide(choice.map) + L"\" -mode \"" + ToWide(choice.mode) + L"\"";
                // Forward the test-only capture hooks, so a harness that opened the menu can still photograph the
                // match (the child is a fresh process with its own command line).
                for (const wchar_t* flag : { L"-pause-at", L"-scoreboard-at", L"-scoreboard-demo", L"-scoreboard-demo-players", L"-postgame-role", L"-pad-script", L"-quit-after", L"-fake-pad", L"-fake-pad-amount", L"-capture-at" }) {
                    int argc = 0;
                    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
                    for (int a = 1; argv && a + 1 < argc; ++a) {
                        if (_wcsicmp(argv[a], flag) == 0) {
                            args += L" ";
                            args += flag;
                            args += L" ";
                            args += argv[a + 1];
                            break;
                        }
                    }
                    if (argv) LocalFree(argv);
                }
                args += L" -difficulty " + std::to_wstring(choice.difficulty);
                // The match process only keeps this for the way back: post-game's NEXT IN ROTATION.
                if (choice.host && ctx.st.playlistChoice >= 0 && ctx.st.playlistChoice < static_cast<int>(ctx.st.playlists.size())) {
                    args += L" -lobby-playlist \"" + ctx.st.playlists[ctx.st.playlistChoice].file.wstring() + L"\"";
                    args += L" -lobby-playlist-pos " + std::to_wstring(ctx.st.playlistPosition);
                    if (ctx.st.playlists[ctx.st.playlistChoice].autoStart > 0) args += L" -lobby-playlist-auto " + std::to_wstring(ctx.st.playlists[ctx.st.playlistChoice].autoStart);
                }
                if (!choice.variantPath.empty()) args += L" -variant \"" + ToWide(choice.variantPath) + L"\"";
                if (choice.skulls != 0) args += L" -skulls " + std::to_wstring(choice.skulls);
                if (!choice.modFolder.empty()) args += L" -mod \"" + choice.modFolder.wstring() + L"\"";
                if (!choice.mapOverride.empty()) args += L" -map-override \"" + choice.mapOverride.wstring() + L"\"";
                if (!choice.mapTitle.empty()) args += L" -map-title \"" + ToWide(choice.mapTitle) + L"\"";
                if (!choice.mapImage.empty()) args += L" -map-image \"" + choice.mapImage.wstring() + L"\"";
                if (!choice.mapThumb.empty()) args += L" -map-thumb \"" + choice.mapThumb.wstring() + L"\"";
                if (choice.host) {
                    args += L" -host";
                    if (choice.hostOnline) args += L" -online";
                    else args += L" -lan";
                    if (choice.hostPublic) args += L" -public";
                    else args += L" -private";
                    args += L" -max-players " + std::to_wstring(choice.hostMaxPlayers);
                    if (choice.expectedPeers > 0) {
                        args += L" -wait-peers " + std::to_wstring(choice.expectedPeers);
                    }
                    if (!choice.sessionId.empty()) {
                        args += L" -session-id \"" + ToWide(choice.sessionId) + L"\"";
                        if (!choice.sessionToken.empty()) {
                            args += L" -session-token \"" + ToWide(choice.sessionToken) + L"\"";
                        }
                    }
                } else if (!choice.connectAddress.empty()) {
                    // A guest launched from the in-engine menu (pre-game lobby): the match process joins the host.
                    // Without this the relaunch started an unconnected local match (main.cpp's in-process path for
                    // the Noble-drawn menu reads choice.connectAddress itself).
                    args += L" -connect \"" + ToWide(choice.connectAddress) + L"\"";
                    if (!choice.sessionId.empty()) args += L" -session-id \"" + ToWide(choice.sessionId) + L"\"";
                    if (!choice.sessionToken.empty()) args += L" -session-token \"" + ToWide(choice.sessionToken) + L"\"";
                }
                NOBLE_INFO("FrontEndMenu: launching match via Relaunch ({} {})", choice.mode, choice.map);
                ShowWindow(hwnd, SW_HIDE); UpdateWindow(hwnd);
                Relaunch::HandOff(args);  // ends this process; returns only if the match could not start
            } else {
                NOBLE_INFO("FrontEndMenu: quit from menu");
            }
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
    }

    if (rtv) rtv->Release();
    if (backBuffer) backBuffer->Release();
    if (context) context->Release();
    device->Release();
}

FrontEndChoice FrontEndMenu::Run(Window& window, D3D11Renderer& renderer, const std::filesystem::path& reachDir) {
    Initialize(reachDir);
    OverlayContext& ctx = GetOverlayCtx();
    FrontEndChoice choice;
    bool done = false;

    window.SetCustomWndProc([](HWND h, UINT m, WPARAM w, LPARAM l) { return UIManager::HandleWndProc(h, m, w, l); });

    UIManager::GetServerBrowser().SetDirectJoinHandler([&choice, &ctx, &done](const std::string& map, const std::string& mode, const std::string& addr, const std::string& sessId) {
        choice.play = true;
        choice.host = false;
        choice.map = map;
        choice.mode = mode;
        choice.connectAddress = addr;
        choice.sessionId = sessId;
        if (mode == "forge" && std::filesystem::exists(ctx.forgeVariant)) {
            choice.variantPath = ctx.forgeVariant.string();
        }
        done = true;
    });

    while (!done && window.ProcessMessages()) {
        renderer.BeginFrame();
        UIManager::BeginFrame();
        ++ctx.totalFrames;

        DrawFrontEndContents(ctx, renderer.GetDevice(), renderer.GetContext(), choice, done);

        UIManager::EndFrame();
        if (ctx.captureFrame && ctx.totalFrames == ctx.captureFrame) {
            FrameCapture::SaveNow(renderer.GetSwapChain(), ctx.totalFrames);
            choice.play = false;
            done = true;
        }
        renderer.EndFrame(true);
    }

    ReachAudio::StopMusic();
    UIManager::GetServerBrowser().SetDirectJoinHandler(nullptr);
    UIManager::GetServerBrowser().SetJoinLobbyHandler(nullptr);
    window.SetCustomWndProc(nullptr);
    return choice;
}

} // namespace noble
