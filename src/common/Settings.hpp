#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace noble {

// Player settings, saved as JSON in <exe folder>\data\userdata\settings.json.
// Read once at start-up; the SETTINGS tab and the pause menu change them and call Save().
// Values that end up in the engine's profile (slot 34) are applied when the profile is built and,
// where safe, again while the game runs (see ReachHost ApplySettingsToProfile).
struct SettingsData {
    // Display
    int width = 1280;
    int height = 720;
    bool borderless = true;        // borderless window covering the monitor (default for new installs)
    bool exclusiveFullscreen = false; // DXGI exclusive full screen; implies borderless, drops out while unfocused
    float fieldOfView = 78.0f;     // horizontal degrees; 78 is Halo: Reach's own default (haloreach+0xB42C10)
    float firstPersonFovMod = 0.0f; // -1.20..1.20 (-120 %..+120 %): 1st-person on-foot FOV modifier (in < 0, out > 0)
    float vehicleFirstPersonFovMod = 0.0f; // -1.20..1.20 (-120 %..+120 %): 1st-person in-vehicle FOV modifier (in < 0, out > 0)
    float vehicleFirstPersonOffsetX = 0.0f; // -1.00..1.00: 1st-person in-vehicle horizontal camera offset (left < 0, right > 0)
    float vehicleFirstPersonOffsetY = 0.0f; // -1.00..1.00: 1st-person in-vehicle vertical camera offset (down < 0, up > 0)
    float thirdPersonZoom = 0.50f; // 0.10..1.00 (10 %..100 %): how much the third-person camera zooms in on right-click aim
    float thirdPersonOffsetX = 0.0f; // -1.00..1.00: third-person camera horizontal offset (left < 0, right > 0)
    float thirdPersonOffsetY = 0.0f; // -1.00..1.00: third-person camera vertical offset (down < 0, up > 0)
    bool verticalSync = false;     // Noble default: off (Tyler)
    int fpsLimit = 0;              // 0 = uncapped
    bool showFps = false;
    bool throttleUnfocused = true;   // drop to 30 FPS when Noble is not the active window
    float brightness = 1.0f;    // 0.7..1.4, applied as a gamma exponent by the post-process (1.0 = untouched)
    bool fxaa = false;          // post-process anti-aliasing in Noble's own pass (Noble's device can do this)

    // Controls
    float mouseSensitivity = 1.0f; // multiplier on Noble's base mouse scale (0.0008 per pixel); horizontal
    float mouseSensitivityY = 1.0f; // vertical look speed relative to horizontal (1 = same)
    float mouseAcceleration = 0.0f; // 0 = off: extra turn speed for fast flicks (0..1)
    float mouseSmoothing = 0.0f;    // 0 = off: evens out jitter, at the cost of a little latency (0..0.9)
    bool invertLook = false;
    std::map<int, int> keyOverrides; // profile action index -> virtual-key code
    int wheelUpAction = 4;         // action the mouse wheel's up notches trigger (-1 = nothing); 4 = change weapon
    int wheelDownAction = 4;       // same for down notches (engine key 0xAE; up is 0xAF)
    bool toggleCrouch = false;     // true: Crouch key toggles crouch on/off; false: hold Crouch key to crouch
    bool padVibration = true;      // controller rumble (host slot 39 -> XInputSetState)
    float padLookSpeedX = 2.0f;    // controller look speed 1.00..10.00; 2.00 = Reach's stock speed (see InputTuning)
    float padLookSpeedY = 2.0f;    // vertical
    // Keyboard keys or mouse buttons treated as controller paddles 1-6 (virtual-key codes, 0 = none). Tools like Steam Input
    // can send a key for each paddle. Default F1-F6, which no keyboard action uses (F5 also toggles third person, see ReachHost).
    std::array<int, 6> padPaddleKeys = { 0x70, 0x71, 0x72, 0x73, 0x74, 0x75 };
    bool padInvertLook = false;
    int padLayout = 0;             // Halo: Reach button layout (engine/PadLayouts.hpp); 0 = Default
    int padStickLayout = 0;        // 0 = Default (left moves, right looks), 1 = swapped. One "preset" row with padLayout
    bool padToggleZoom = true;     // false: zoom only while the button is held
    bool padToggleCrouch = false;  // true: press crouch button to stay crouched; false: hold crouch button
    // Per-action controller bindings, overriding the button layout: action key -> input (PadLayouts::InputBit), -1 = none.
    std::map<std::string, int> padBindings;    // Noble flips the right stick's y before the engine sees it
    float padLeftDeadZone = 0.0f;  // extra radial dead zone Noble applies (0..0.5), on top of the engine's own
    float padRightDeadZone = 0.0f;
    float padVibrationStrength = 1.0f; // 0..1

    // Audio (engine volumes, 0..1, profile +0xA5C/+0xA60/+0xA64)
    float volumeGame = 1.0f;
    float volumeMusic = 1.0f;
    float volumeOther = 1.0f;
    bool menuSounds = true;
    bool masterMute = false;         // silences Noble's own audio (menu music and menu sounds)
    bool muteInBackground = false;   // ... and again whenever Noble is not the active window
    bool inviteNotifications = true;   // show a JOIN toast when a friend invites you
    std::string menuMusicTrack = "reach:menu";
    // Tracks switched off in the menu-music playlist: "Shuffle all games" skips them (ReachAudio reads this).
    std::vector<std::string> menuMusicDisabled; // ReachAudio::MusicTracks() id, "all" or "off"

    // Voice chat. Device ids are Windows endpoint ids; empty means the system default.
    bool firstStartDone = false;    // the first-start question was answered; a settings file from before it counts as answered
    bool voiceEnabled = true;
    std::string voiceInputDevice;
    std::string voiceOutputDevice;
    float voiceVolume = 1.0f;       // 0..2
    bool voiceOpenMic = false;      // false: push to talk (KeyBindings::kActionPushToTalk, PadLayouts::PushToTalk)
    int voiceBitrateKbps = 20;      // 16, 20 or 24
    int voiceChannel = 0;           // 0 all, 1 team, 2 party (same as all until parties exist)

    // Gameplay
    bool subtitles = true;         // show cinematic/dialogue subtitles (ReachHost subtitle hook)
    uint64_t campaignSkulls = 0;   // MCC skull bits chosen on the SKULLS screen (launch block +0x48)

    // Menus
    std::string menuBackground = "random"; // "random", "noble" (own art), "reach" (from the MCC install)

    // Player / network
    // "Continue" fast path (spec 4.1): the last match this player started, so the main menu can offer it.
    std::string lastPlayedMode;
    std::string lastPlayedMap;
    std::string lastPlayedVariant;
    int lastPlayedDifficulty = 1;
    unsigned long long lastPlayedSkulls = 0;
    std::string lastPlayedMod;          // mod folder, empty when the game was vanilla
    std::string playerName = "NobleSix";
    std::string serviceTag = "NOBL";  // Reach service tag: 1-4 letters/digits, upper case (profile +0x1AC)
    std::string masterServerUrl = "https://outbreak-z-test-server.onrender.com"; // empty = LAN only
    std::vector<std::string> favorites;

    // Account / Auth
    std::string accountUsername = "";
    std::string accountToken = "";
    bool isDeveloper = false;
    int accountUserId = 0; // "address:port" entries saved in the SERVERS tab
};

class Settings {
public:
    static Settings& Get();

    void Load();
    bool Save();

    SettingsData Snapshot() const;
    void Update(const SettingsData& data);   // replace and save

    std::filesystem::path FilePath() const;
    // Key bindings and preferences to a file and back, for moving them to another PC or sharing them. Account details, the
    // server address and this PC's audio devices never travel, so a shared file can't leak a login or redirect anyone.
    bool ExportTo(const std::filesystem::path& file);
    bool ImportFrom(const std::filesystem::path& file);

private:
    Settings() = default;
    mutable std::recursive_mutex m_mutex;
    SettingsData m_data;
    bool m_loaded = false;
};

} // namespace noble
