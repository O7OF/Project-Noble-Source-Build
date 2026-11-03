#include "engine/KeyBindings.hpp"
#include "Settings.hpp"
#include "DeviceIdentity.hpp"
#include "Logger.hpp"

#include <windows.h>
#include <shellapi.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>

namespace noble {

namespace {

std::filesystem::path ExeDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

template <typename T>
void Read(const nlohmann::json& j, const char* key, T& out) {
    auto it = j.find(key);
    if (it == j.end()) return;
    try {
        out = it->get<T>();
    } catch (...) {
        // keep the default for a value of the wrong type
    }
}

void Clamp(SettingsData& d) {
    d.width = std::clamp(d.width, 640, 7680);
    d.height = std::clamp(d.height, 360, 4320);
    d.fieldOfView = std::clamp(d.fieldOfView, 70.0f, 120.0f);  // slider range (SETTINGS > VIDEO)
    d.firstPersonFovMod = std::clamp(d.firstPersonFovMod, -1.20f, 1.20f);
    d.vehicleFirstPersonFovMod = std::clamp(d.vehicleFirstPersonFovMod, -1.20f, 1.20f);
    d.vehicleFirstPersonOffsetX = std::clamp(d.vehicleFirstPersonOffsetX, -1.00f, 1.00f);
    d.vehicleFirstPersonOffsetY = std::clamp(d.vehicleFirstPersonOffsetY, -1.00f, 1.00f);
    d.thirdPersonZoom = std::clamp(d.thirdPersonZoom, 0.10f, 1.00f);
    d.thirdPersonOffsetX = std::clamp(d.thirdPersonOffsetX, -1.00f, 1.00f);
    d.thirdPersonOffsetY = std::clamp(d.thirdPersonOffsetY, -1.00f, 1.00f);
    constexpr int kFpsLimits[] = { 0, 30, 60, 120, 144, 165, 200 };
    if (std::find(std::begin(kFpsLimits), std::end(kFpsLimits), d.fpsLimit) == std::end(kFpsLimits)) {
        d.fpsLimit = 0;
    }
    d.mouseSensitivity = std::clamp(d.mouseSensitivity, 0.1f, 10.0f);
    d.mouseSensitivityY = std::clamp(d.mouseSensitivityY, 0.25f, 4.0f);
    d.volumeGame = std::clamp(d.volumeGame, 0.0f, 1.0f);
    d.volumeMusic = std::clamp(d.volumeMusic, 0.0f, 1.0f);
    d.volumeOther = std::clamp(d.volumeOther, 0.0f, 1.0f);
    d.voiceVolume = std::clamp(d.voiceVolume, 0.0f, 2.0f);
    if (d.voiceBitrateKbps != 16 && d.voiceBitrateKbps != 24) d.voiceBitrateKbps = 20;
    d.voiceChannel = std::clamp(d.voiceChannel, 0, 2);
    auto trimName = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    };
    trimName(d.playerName);
    if (d.playerName.empty() || d.playerName == "Player") d.playerName = "NobleSix";
    if (d.playerName.size() > 20) d.playerName.resize(20);
    {
        // Service tag: Reach allows up to 4 characters, letters and digits, shown in upper case.
        std::string tag;
        for (char c : d.serviceTag) {
            if (tag.size() == 4) break;
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) tag.push_back(c);
        }
        d.serviceTag = tag.empty() ? "NOBL" : tag;
    }
    trimName(d.masterServerUrl);
    while (!d.masterServerUrl.empty() && d.masterServerUrl.back() == '/') d.masterServerUrl.pop_back();
    if (d.masterServerUrl.empty() || d.masterServerUrl == "https://outbreak-z-test-server.onrender.com") {
        d.masterServerUrl = "https://servertestdeploy-jfxb.onrender.com";
    }
    if (d.favorites.size() > 50) d.favorites.resize(50);
    d.wheelUpAction = std::clamp(d.wheelUpAction, -1, 65);
    d.wheelDownAction = std::clamp(d.wheelDownAction, -1, 65);
    d.padVibrationStrength = std::clamp(d.padVibrationStrength, 0.0f, 1.0f);
    d.padLookSpeedX = std::clamp(d.padLookSpeedX, 1.0f, 10.0f);
    d.padLookSpeedY = std::clamp(d.padLookSpeedY, 1.0f, 10.0f);
    d.padLeftDeadZone = std::clamp(d.padLeftDeadZone, 0.0f, 0.5f);
    d.padRightDeadZone = std::clamp(d.padRightDeadZone, 0.0f, 0.5f);
    for (auto it = d.keyOverrides.begin(); it != d.keyOverrides.end();) {
        // A key of 0 is deliberate: the player unbound that action.
        if (it->first < 0 || it->first >= KeyBindings::kActionCount || it->second < 0 || it->second >= 256) it = d.keyOverrides.erase(it);
        else ++it;
    }
}

} // namespace

Settings& Settings::Get() {
    static Settings instance;
    return instance;
}

std::filesystem::path Settings::FilePath() const {
    return ExeDir() / "data" / "userdata" / "settings.json";
}

void Settings::Load() {
    DeviceIdentityStore::MigrateNewestFile("userdata/settings.json");
    SettingsData d;
    const auto path = FilePath();
    std::ifstream in(path);
    const bool existed = in.is_open();
    if (in) {
        try {
            const nlohmann::json j = nlohmann::json::parse(in);
            d.firstStartDone = j.value("firstStartDone", true);
            Read(j, "width", d.width);
            Read(j, "height", d.height);
            Read(j, "borderless", d.borderless);
            Read(j, "exclusiveFullscreen", d.exclusiveFullscreen);
            if (d.exclusiveFullscreen) d.borderless = true;
            Read(j, "fieldOfView", d.fieldOfView);
            Read(j, "firstPersonFovMod", d.firstPersonFovMod);
            Read(j, "vehicleFirstPersonFovMod", d.vehicleFirstPersonFovMod);
            Read(j, "vehicleFirstPersonOffsetX", d.vehicleFirstPersonOffsetX);
            Read(j, "vehicleFirstPersonOffsetY", d.vehicleFirstPersonOffsetY);
            Read(j, "thirdPersonZoom", d.thirdPersonZoom);
            Read(j, "thirdPersonOffsetX", d.thirdPersonOffsetX);
            Read(j, "thirdPersonOffsetY", d.thirdPersonOffsetY);
            Read(j, "verticalSync", d.verticalSync);
            Read(j, "fpsLimit", d.fpsLimit);
            Read(j, "showFps", d.showFps);
            Read(j, "throttleUnfocused", d.throttleUnfocused);
            Read(j, "brightness", d.brightness);
            Read(j, "fxaa", d.fxaa);
            Read(j, "mouseSensitivity", d.mouseSensitivity);
            Read(j, "mouseSensitivityY", d.mouseSensitivityY);
            Read(j, "mouseAcceleration", d.mouseAcceleration);
            Read(j, "mouseSmoothing", d.mouseSmoothing);
            Read(j, "invertLook", d.invertLook);
            Read(j, "wheelUpAction", d.wheelUpAction);
            Read(j, "wheelDownAction", d.wheelDownAction);
            Read(j, "toggleCrouch", d.toggleCrouch);
            Read(j, "padVibration", d.padVibration);
            Read(j, "padVibrationStrength", d.padVibrationStrength);
            // Older files saved 1..19; 19 stays the top of the new 1.00..10.00 scale.
            {
                int oldX = 0, oldY = 0;
                Read(j, "padLookX", oldX);
                Read(j, "padLookY", oldY);
                if (oldX > 0) d.padLookSpeedX = (static_cast<float>(oldX) + 1.0f) * 0.1f;
                if (oldY > 0) d.padLookSpeedY = (static_cast<float>(oldY) + 1.0f) * 0.1f;
            }
            // The first 1-10 scale put stock at 10.00; stock is 2.00 now, so those values are scaled to feel the same.
            {
                float oldX = 0.0f, oldY = 0.0f;
                Read(j, "padLookSpeedX", oldX);
                Read(j, "padLookSpeedY", oldY);
                if (oldX > 0.0f) d.padLookSpeedX = oldX * 0.2f;
                if (oldY > 0.0f) d.padLookSpeedY = oldY * 0.2f;
            }
            Read(j, "padLookRateX", d.padLookSpeedX);
            Read(j, "padLookRateY", d.padLookSpeedY);
            Read(j, "padInvertLook", d.padInvertLook);
            Read(j, "padLayout", d.padLayout);
            Read(j, "padStickLayout", d.padStickLayout);
            Read(j, "padToggleZoom", d.padToggleZoom);
            Read(j, "padToggleCrouch", d.padToggleCrouch);
            if (j.contains("padPaddleKeys") && j["padPaddleKeys"].is_array() && j["padPaddleKeys"].size() == d.padPaddleKeys.size()) {
                for (size_t i = 0; i < d.padPaddleKeys.size(); ++i) {
                    if (j["padPaddleKeys"][i].is_number_integer()) d.padPaddleKeys[i] = j["padPaddleKeys"][i].get<int>();
                }
                // The first default was F13-F18; move anyone still on it to F1-F6. A set the player chose stays.
                if (d.padPaddleKeys == std::array<int, 6>{ 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81 }) d.padPaddleKeys = SettingsData{}.padPaddleKeys;
            }
            if (j.contains("padBindings") && j["padBindings"].is_object()) {
                for (const auto& [key, value] : j["padBindings"].items()) {
                    if (value.is_number_integer()) d.padBindings[key] = value.get<int>();
                }
            }
            Read(j, "padLeftDeadZone", d.padLeftDeadZone);
            Read(j, "padRightDeadZone", d.padRightDeadZone);
            Read(j, "volumeGame", d.volumeGame);
            Read(j, "volumeMusic", d.volumeMusic);
            Read(j, "volumeOther", d.volumeOther);
            Read(j, "menuSounds", d.menuSounds);
            Read(j, "masterMute", d.masterMute);
            Read(j, "muteInBackground", d.muteInBackground);
            Read(j, "inviteNotifications", d.inviteNotifications);
            Read(j, "voiceEnabled", d.voiceEnabled);
            Read(j, "voiceInputDevice", d.voiceInputDevice);
            Read(j, "voiceOutputDevice", d.voiceOutputDevice);
            Read(j, "voiceVolume", d.voiceVolume);
            Read(j, "voiceOpenMic", d.voiceOpenMic);
            Read(j, "voiceBitrateKbps", d.voiceBitrateKbps);
            Read(j, "voiceChannel", d.voiceChannel);
            Read(j, "menuMusicTrack", d.menuMusicTrack);
            if (j.contains("menuMusicDisabled") && j["menuMusicDisabled"].is_array()) {
                for (const auto& id : j["menuMusicDisabled"]) if (id.is_string()) d.menuMusicDisabled.push_back(id.get<std::string>());
            }
            Read(j, "subtitles", d.subtitles);
            Read(j, "campaignSkulls", d.campaignSkulls);
            Read(j, "menuBackground", d.menuBackground);
            Read(j, "playerName", d.playerName);
            Read(j, "lastPlayedMode", d.lastPlayedMode);
            Read(j, "lastPlayedMap", d.lastPlayedMap);
            Read(j, "lastPlayedVariant", d.lastPlayedVariant);
            Read(j, "lastPlayedDifficulty", d.lastPlayedDifficulty);
            Read(j, "lastPlayedSkulls", d.lastPlayedSkulls);
            Read(j, "lastPlayedMod", d.lastPlayedMod);
            Read(j, "serviceTag", d.serviceTag);
            Read(j, "masterServerUrl", d.masterServerUrl);
            // Free federation: the West registry merges East and UK server-side. URL cleanup happens
            // in Clamp() so every entry point (file, UI, command line) is normalised the same way.
            Read(j, "favorites", d.favorites);
            Read(j, "accountUsername", d.accountUsername);
            Read(j, "accountToken", d.accountToken);
            Read(j, "isDeveloper", d.isDeveloper);
            Read(j, "accountUserId", d.accountUserId);
            auto keys = j.find("keyOverrides");
            if (keys != j.end() && keys->is_object()) {
                for (auto it = keys->begin(); it != keys->end(); ++it) {
                    try {
                        d.keyOverrides[std::stoi(it.key())] = it.value().get<int>();
                    } catch (...) {
                    }
                }
            }
            NOBLE_INFO("Settings: loaded {}", path.string());
        } catch (const std::exception& e) {
            NOBLE_WARN("Settings: could not read {} ({}); using defaults", path.string(), e.what());
        }
    }

    // Apply per-process command-line identity overrides passed by Relaunch::Start so multiple instances
    // running on the same PC retain their own logged-in account and player name across match relaunches.
    {
        auto narrowUtf8 = [](const wchar_t* w) -> std::string {
            if (!w || !*w) return "";
            const int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
            if (len <= 0) return "";
            std::string s(static_cast<size_t>(len - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), len, nullptr, nullptr);
            return s;
        };
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            for (int i = 1; i + 1 < argc; ++i) {
                const std::wstring arg = argv[i];
                if (arg == L"-player-name") {
                    d.playerName = narrowUtf8(argv[++i]);
                } else if (arg == L"-service-tag") {
                    d.serviceTag = narrowUtf8(argv[++i]);
                } else if (arg == L"-account-user") {
                    d.accountUsername = narrowUtf8(argv[++i]);
                } else if (arg == L"-account-token") {
                    d.accountToken = narrowUtf8(argv[++i]);
                } else if (arg == L"-user-id") {
                    d.accountUserId = _wtoi(argv[++i]);
                } else if (arg == L"-account-dev") {
                    d.isDeveloper = (_wtoi(argv[++i]) != 0);
                }
            }
            LocalFree(argv);
        }
    }

    Clamp(d);
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_data = d;
        m_loaded = true;
    }
    if (!existed) {
        Save();
    }
}

bool Settings::Save() {
    nlohmann::json j;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        const SettingsData& d = m_data;
        j["width"] = d.width;
        j["height"] = d.height;
        j["borderless"] = d.borderless;
        j["exclusiveFullscreen"] = d.exclusiveFullscreen;
        j["fieldOfView"] = d.fieldOfView;
        j["firstPersonFovMod"] = d.firstPersonFovMod;
        j["vehicleFirstPersonFovMod"] = d.vehicleFirstPersonFovMod;
        j["vehicleFirstPersonOffsetX"] = d.vehicleFirstPersonOffsetX;
        j["vehicleFirstPersonOffsetY"] = d.vehicleFirstPersonOffsetY;
        j["thirdPersonZoom"] = d.thirdPersonZoom;
        j["thirdPersonOffsetX"] = d.thirdPersonOffsetX;
        j["thirdPersonOffsetY"] = d.thirdPersonOffsetY;
        j["verticalSync"] = d.verticalSync;
        j["fpsLimit"] = d.fpsLimit;
        j["showFps"] = d.showFps;
        j["throttleUnfocused"] = d.throttleUnfocused;
        j["brightness"] = d.brightness;
        j["fxaa"] = d.fxaa;
        j["mouseSensitivity"] = d.mouseSensitivity;
        j["mouseSensitivityY"] = d.mouseSensitivityY;
        j["mouseAcceleration"] = d.mouseAcceleration;
        j["mouseSmoothing"] = d.mouseSmoothing;
        j["invertLook"] = d.invertLook;
        j["wheelUpAction"] = d.wheelUpAction;
        j["wheelDownAction"] = d.wheelDownAction;
        j["toggleCrouch"] = d.toggleCrouch;
        j["padVibration"] = d.padVibration;
        j["padVibrationStrength"] = d.padVibrationStrength;
        j["padLookRateX"] = d.padLookSpeedX;
        j["padLookRateY"] = d.padLookSpeedY;
        j["padInvertLook"] = d.padInvertLook;
        j["padLayout"] = d.padLayout;
        j["padStickLayout"] = d.padStickLayout;
        j["padToggleZoom"] = d.padToggleZoom;
        j["padToggleCrouch"] = d.padToggleCrouch;
        j["padPaddleKeys"] = d.padPaddleKeys;
        if (!d.padBindings.empty()) j["padBindings"] = d.padBindings;
        j["padLeftDeadZone"] = d.padLeftDeadZone;
        j["padRightDeadZone"] = d.padRightDeadZone;
        j["volumeGame"] = d.volumeGame;
        j["volumeMusic"] = d.volumeMusic;
        j["volumeOther"] = d.volumeOther;
        j["menuSounds"] = d.menuSounds;
        j["masterMute"] = d.masterMute;
        j["muteInBackground"] = d.muteInBackground;
        j["inviteNotifications"] = d.inviteNotifications;
    j["firstStartDone"] = d.firstStartDone;
        j["voiceEnabled"] = d.voiceEnabled;
        j["voiceInputDevice"] = d.voiceInputDevice;
        j["voiceOutputDevice"] = d.voiceOutputDevice;
        j["voiceVolume"] = d.voiceVolume;
        j["voiceOpenMic"] = d.voiceOpenMic;
        j["voiceBitrateKbps"] = d.voiceBitrateKbps;
        j["voiceChannel"] = d.voiceChannel;
        j["menuMusicTrack"] = d.menuMusicTrack;
        j["menuMusicDisabled"] = d.menuMusicDisabled;
        j["subtitles"] = d.subtitles;
        j["campaignSkulls"] = d.campaignSkulls;
        j["menuBackground"] = d.menuBackground;
        j["playerName"] = d.playerName;
        j["lastPlayedMode"] = d.lastPlayedMode;
        j["lastPlayedMap"] = d.lastPlayedMap;
        j["lastPlayedVariant"] = d.lastPlayedVariant;
        j["lastPlayedDifficulty"] = d.lastPlayedDifficulty;
        j["lastPlayedSkulls"] = d.lastPlayedSkulls;
        j["lastPlayedMod"] = d.lastPlayedMod;
        j["serviceTag"] = d.serviceTag;
        j["masterServerUrl"] = d.masterServerUrl;
        j["favorites"] = d.favorites;
        j["accountUsername"] = d.accountUsername;
        j["accountToken"] = d.accountToken;
        j["isDeveloper"] = d.isDeveloper;
        j["accountUserId"] = d.accountUserId;
        nlohmann::json keys = nlohmann::json::object();
        for (const auto& [action, vk] : d.keyOverrides) keys[std::to_string(action)] = vk;
        j["keyOverrides"] = keys;
    }
    const auto path = FilePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    const auto tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            NOBLE_WARN("Settings: could not write {}", tmp);
            return false;
        }
        out << j.dump(2);
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        NOBLE_WARN("Settings: could not replace {}: {}", path.string(), ec.message());
        return false;
    }
    DeviceIdentityStore::MirrorSaveFile("userdata/settings.json");
    return true;
}

SettingsData Settings::Snapshot() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_data;
}

namespace {
void DropPrivateKeys(nlohmann::json& j) {
    for (const char* key : { "accountUsername", "accountToken", "accountUserId", "isDeveloper", "masterServerUrl",
                             "voiceInputDevice", "voiceOutputDevice", "firstStartDone", "lastPlayedMode", "lastPlayedMap" }) {
        j.erase(key);
    }
}
} // namespace

bool Settings::ExportTo(const std::filesystem::path& file) {
    Save();
    try {
        std::ifstream in(FilePath());
        nlohmann::json j = nlohmann::json::parse(in);
        DropPrivateKeys(j);
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << j.dump(2);
        return static_cast<bool>(out);
    } catch (...) {
        return false;
    }
}

bool Settings::ImportFrom(const std::filesystem::path& file) {
    try {
        std::error_code ec;
        if (std::filesystem::file_size(file, ec) > 4 * 1024 * 1024 || ec) return false;
        std::ifstream in(file);
        nlohmann::json incoming = nlohmann::json::parse(in);
        if (!incoming.is_object()) return false;
        DropPrivateKeys(incoming);
        Save();
        std::ifstream current(FilePath());
        nlohmann::json merged = nlohmann::json::parse(current);
        current.close();
        merged.update(incoming);
        {
            std::ofstream out(FilePath(), std::ios::binary | std::ios::trunc);
            out << merged.dump(2);
            if (!out) return false;
        }
        Load();   // Load checks every value's type and range
        Save();   // so the file holds the checked values too
        return true;
    } catch (...) {
        return false;
    }
}

void Settings::Update(const SettingsData& data) {
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_data = data;
        Clamp(m_data);
    }
    Save();
}

} // namespace noble
