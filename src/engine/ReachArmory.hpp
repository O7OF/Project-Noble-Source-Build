#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace noble {

struct ReachArmorySelection {
    // Preferred species for in-game spawning (0 = Spartan, 1 = Elite)
    bool isElite = false;
    // Species currently previewed on the Armory pedestal (can be toggled independently or with Preferred Species)
    bool previewElite = false;
    bool isFemale = false;

    // Integer indices for in-game profile / appearance table
    int primaryColorIndex = 1;    // 0..29 (Steel)
    int secondaryColorIndex = 10; // 0..29 (Green/Brick)
    int accentColorIndex = 14;    // 0..29 (Cyan; the third/detail colour)
    int lightsColorIndex = 14;    // 0..29 (4th slot: lights / illumination)
    int visorColorIndex = 0;      // 0..4
    int armorEffectIndex = 0;     // 0..6
    int firefightVoiceIndex = 0;  // 0..15
    int eliteSuitIndex = 0;       // 0..10 (the Armory's elite set presets)
    std::string serviceTag = "N06";

    // Spartan permutation string_id names (matching mainmenu_all.string_ids.csv)
    std::string helmet = "default";
    std::string chest = "chest_blank";
    std::string leftShoulder = "left_shoulder_blank";
    std::string rightShoulder = "right_shoulder_blank";
    std::string hands = "default";       // "default" or "robot"
    std::string wrist = "wrist_blank";
    std::string utility = "utility_blank";
    std::string knees = "knees_default";
    std::string armorEffect = "default"; // "default", "birthday_party", "heart_attack", "pestilence", "inclement_weather", "flames", "blue_flames"
    std::string visor = "default";
    std::string voice = "noble_six";

    // Elite suit string_id name ("minor", "spec_ops", "space", "ultra", "zealot", "general", "story_elite", "officer")
    std::string eliteSuit = "minor";
    // Elite per-piece permutation names for the menu map's Elite model regions (elite_helmet, elite_left_shoulder,
    // elite_right_shoulder, elite_chest, elite_forearms, elite_legs).
    std::string eliteHelmet = "minor";
    std::string eliteLeftShoulder = "minor";
    std::string eliteRightShoulder = "minor";
    std::string eliteChest = "minor";
    std::string eliteArms = "default";
    std::string eliteLegs = "minor";
    int eliteArmorEffectIndex = 0;
    std::string eliteArmorEffect = "default";

    // Linear RGB colors [0..1]
    std::array<float, 3> primaryRgb = { 0.100f, 0.100f, 0.100f };
    std::array<float, 3> secondaryRgb = { 0.100f, 0.100f, 0.100f };
    std::array<float, 3> accentRgb = { 0.471f, 0.788f, 0.808f };   // third/detail colour (Cyan)
    std::array<float, 3> lightsRgb = { 0.106f, 0.839f, 0.529f };   // 4th slot: lights / illumination
    std::array<float, 3> visorRgb = { 0.69f, 0.25f, 0.02f };
    bool lightsCustom = false;
};

class ReachArmory {
public:
    static void Initialize(const std::filesystem::path& dataDir);

    static void SetMenuMode(bool menuMode);
    static bool IsMenuMode();

    static void SetTargetScene(const std::string& sceneName);
    static std::string GetTargetScene();

    static void SetSelection(const ReachArmorySelection& sel);
    static ReachArmorySelection GetSelection();

    static void AddYawRotation(float deltaRadians);
    static void SetYawRotation(float radians);
    static float GetYawRotation();

    // Resolves a Blam! string_id name from mainmenu_all.string_ids.csv (returns 0 if not found).
    static uint32_t ResolveStringId(const std::string& name);

    // Called every frame on the engine render thread inside HostSlot1_FrameTick.
    static void Tick();
    static void NotifyEngineTick();

    // True once mainmenu_all.map HS GLOBALS table is active.
    static bool IsSceneReady();

    // True when the engine is actively displaying an Armory pedestal/customization scene.
    static bool IsArmoryScene();
};

} // namespace noble
