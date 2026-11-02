#pragma once

#include "UiArt.hpp"


#include <d3d11.h>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace noble {

enum class PlayerSpecies : int {
    Spartan = 0,
    Elite = 1
};

enum class BodyType : int {
    Male = 0,
    Female = 1
};

struct AppearanceProfile {
    std::string serviceTag = "N06";
    PlayerSpecies species = PlayerSpecies::Spartan; // Preferred Species for in-game spawning
    BodyType bodyType = BodyType::Male;

    // Spartan Color indices (0-29 matching Reach's 30-color palette)
    int primaryColor = 1;    // Steel
    int secondaryColor = 10; // Green
    int accentColor = 14;    // Cyan (the third / detail colour)
    int lightsColor = 10;    // 4th slot: lights / illumination colour
    // Custom picker wheel (Armory preview). appearance.json stores both the exact RGB and the nearest
    // Reach palette index, because the in-game profile only accepts palette item ids.
    bool primaryCustom = false, secondaryCustom = false, accentCustom = false, lightsCustom = false;
    std::array<float, 3> primaryCustomRgb = {0.349f, 0.349f, 0.349f};
    std::array<float, 3> secondaryCustomRgb = {0.267f, 0.545f, 0.314f};
    std::array<float, 3> accentCustomRgb = {0.471f, 0.788f, 0.808f};
    std::array<float, 3> lightsCustomRgb = {0.106f, 0.839f, 0.529f};

    // Dedicated Elite Color indices (separate from Spartan customizations)
    int elitePrimaryColor = 1;    // Steel
    int eliteSecondaryColor = 10; // Green
    int eliteAccentColor = 14;    // Cyan
    int eliteLightsColor = 10;    // 4th slot: lights / illumination colour
    bool elitePrimaryCustom = false, eliteSecondaryCustom = false, eliteAccentCustom = false, eliteLightsCustom = false;
    std::array<float, 3> elitePrimaryCustomRgb = {0.349f, 0.349f, 0.349f};
    std::array<float, 3> eliteSecondaryCustomRgb = {0.267f, 0.545f, 0.314f};
    std::array<float, 3> eliteAccentCustomRgb = {0.471f, 0.788f, 0.808f};
    std::array<float, 3> eliteLightsCustomRgb = {0.106f, 0.839f, 0.529f};

    // Spartan selections (catalog indices)
    int helmetIndex = 0;         // Mark V [B]
    int helmetVariantIndex = 0;  // Base
    int leftShoulderIndex = 0;   // Default
    int rightShoulderIndex = 0;  // Default
    int chestIndex = 0;          // Default
    int wristIndex = 0;          // Default
    int utilityIndex = 0;        // Default / None
    int visorColorIndex = 0;     // Default (Amber/Gold)
    int kneeGuardsIndex = 0;     // Default
    int armorEffectIndex = 0;    // None
    int firefightVoiceIndex = 0; // Noble Six

    // Elite selections (0=Minor, 1=Spec Ops, 2=Ranger, 3=Ultra, 4=Zealot, 5=General, 6=Field Marshal,
    // 7=Officer, 8=Honor Guard, 9=H2A Ranger, 10=Counselor). 8..10 are mixed presets built from pieces.
    int eliteArmorSet = 0;
    int eliteHelmetIndex = 0;
    int eliteLeftShoulderIndex = 0;
    int eliteRightShoulderIndex = 0;
    int eliteChestIndex = 0;
    int eliteArmsIndex = 0;
    int eliteLegsIndex = 0;
    int eliteArmorEffectIndex = 0;
    int nameplateIndex = 1;
    int emblemIndex = 1;            // position in UiArt::GetEmblemCatalog()
    int emblemBackgroundIndex = 0;  // 0 = none, n = UiArt::GetEmblemBackgrounds()[n - 1]
    int emblemPrimaryColor = 2;     // White (Reach's 30-colour palette)
    int emblemSecondaryColor = 2;   // White
    int emblemBackgroundColor = 16; // Cobalt

    // Legacy string fields preserved for backward compatibility with existing appearance.json files
    std::string spartanHelmetGroup;
    std::string spartanChestGroup;
    std::string spartanLeftShoulderGroup;
    std::string spartanRightShoulderGroup;
    std::string spartanWristGroup;
    std::string spartanKneesGroup;
};

struct ReachColorEntry {
    const char* name;
    float r, g, b;
};

class ArmorCustomizer {
public:
    ArmorCustomizer();
    ~ArmorCustomizer() = default;

    void Render(ID3D11Device* device, ID3D11DeviceContext* context);

    bool LoadProfile(const std::filesystem::path& path);
    bool SaveProfile(const std::filesystem::path& path);
    void SetPreferredSpecies(PlayerSpecies species);
    PlayerSpecies GetPreferredSpecies() const { return m_profile.species; }

    void SyncToReachArmory() const;
    void UpdateArmoryCamera() const;
    // Puts a saved nameplate back to the first plate this build may offer when the saved one was
    // retired or is a developer entitlement being loaded by a release build.
    void ValidateNameplate();
    int GetCameraMode() const { return m_cameraMode; }
    // True while the engine camera is the whole-body view (Armory/Full Body, or the default close-up
    // mode on the Species/Body rows). The stage enlarges that view; per-slot close-ups stay as they are.
    bool IsFullBodyCamera() const;

    const AppearanceProfile& GetProfile() const { return m_profile; }
    // The saved emblem colours, for rows that show the local player's emblem.
    UiArt::EmblemColors EmblemColorsNow() const;
    AppearanceProfile& GetProfileMutable() { return m_profile; }

    static std::filesystem::path ResolveDefaultProfilePath();

private:
    void RenderCharacterPreview(ID3D11Device* device, ID3D11DeviceContext* context,
                                float width, float height, bool fullBleed);
    void SyncLegacyFields();
    void MigrateFromLegacyFields();

    AppearanceProfile m_profile;
    // The chosen visor finish. Kept separately from the profile so a transient preview/restore of
    // m_profile cannot silently reset the equipped colour back to Default (index 0).
    int m_visorSelection = -1;
    PlayerSpecies m_previewSpecies = PlayerSpecies::Spartan;
    std::filesystem::path m_profilePath;
    bool m_dirty = false;
    mutable bool m_syncedInitial = false;

    // 0 = Standard Armory 3/4 view ("armory"), 1 = Full Body ("c_full_spartan"), 2 = close-up of the slot being edited
    // ("c_helmet", ...; default, Armory v2). The mouse wheel over the model switches between them.
    int m_cameraMode = 1;
    std::string m_focusScene = "c_full_spartan";   // close-up scene for the focused slot
    bool m_pickerScrollToEquipped = true;

    std::string m_spartanObjError, m_eliteObjError;
    float m_objYaw = -0.32f;      // MCC 3/4 left-facing angle
    bool m_autoRotate = false;

    int m_focusedRow = 0;
    bool m_showColorModal = false;
};

} // namespace noble
