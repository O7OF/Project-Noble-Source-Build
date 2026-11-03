#include "KeyBindings.hpp"
#include "common/Settings.hpp"

#include <windows.h>
#include <cstdint>

namespace noble {
namespace KeyBindings {

namespace {

// MCC's default PC layout, by action index (moved here from ReachHost.cpp), with Noble's changes (Tyler):
// melee F (MCC: Q), grenade G (MCC: F), Falcon ascend Space / descend Left Ctrl (MCC: unbound).
const uint8_t kDefaultKeys[kActionCount] = {
    /*  0 */ VK_SPACE, '2',        'E',        'R',        '1',        'F',        '4',        'G',
    /*  8 */ VK_LBUTTON, VK_LCONTROL, VK_RBUTTON, 'Z',        'X',        'C',        'C',        'Q',
    /* 16 */ 'W',        'S',      'A',        'D',        VK_TAB,     VK_LCONTROL, VK_SPACE, VK_LSHIFT,
    /* 24 */ VK_RBUTTON, 'R',      'F',        VK_LBUTTON, VK_LSHIFT,  VK_LCONTROL, VK_DELETE, '1',
    /* 32 */ '2',        VK_OEM_3, VK_RBUTTON, VK_NUMPAD4, 'X',        'Z',        'C',        VK_RBUTTON,
    /* 40 */ VK_RIGHT,   VK_LEFT,  VK_RETURN,  'E',        VK_SPACE,   VK_LSHIFT,  VK_NUMPAD5, 'R',
    /* 48 */ 'F',        VK_RBUTTON, VK_RBUTTON, 'E',      'G',        'C',        'X',        'F',
    /* 56 */ '3',        'G',      'L',        VK_UP,      VK_DOWN,    'P',        VK_SPACE,   VK_LCONTROL,
    /* 64 */ 0,          0,        0,          'V',        0,          'Y',        'T',        'P',
    /* 72 */ 0,          VK_F9,     VK_F10,     VK_F11,   // voice channels: F1-F6 are the default paddle keys
    /* 76 */ VK_F5,      0,                               // 3rd-person camera (default F5), Toggle crouch (unbound)
};

// Slot numbers are MCC's GameAbstractButton order (the enum list in MCC-Win64-Shipping.exe, 66 entries
// ending in CountTotal); every default key below matches MCC's default PC layout for Reach. Names and
// groups follow MCC's CONFIGURE BINDINGS screen. Actions other Halo games use are listed last.
struct ActionInfo {
    int slot;
    const char* name;
    ActionGroup group;
};
const ActionInfo kActions[] = {
    // Movement
    { 16, "Move forward", ActionGroup::Movement },
    { 17, "Move backward", ActionGroup::Movement },
    { 18, "Move left", ActionGroup::Movement },
    { 19, "Move right", ActionGroup::Movement },
    { 0, "Jump", ActionGroup::Movement },
    { 9, "Crouch", ActionGroup::Movement },
    { kActionToggleCrouch, "Crouch (toggle)", ActionGroup::Movement },
    // Actions
    { 8, "Fire", ActionGroup::Actions },
    { 10, "Zoom", ActionGroup::Actions },
    { 11, "Zoom in", ActionGroup::Actions },
    { 12, "Zoom out", ActionGroup::Actions },
    { 5, "Melee", ActionGroup::Actions },
    { 3, "Reload", ActionGroup::Actions },
    { 7, "Throw grenade", ActionGroup::Actions },
    { 2, "Action", ActionGroup::Actions },
    { 4, "Change weapon", ActionGroup::Actions },
    { 1, "Switch grenades", ActionGroup::Actions },
    { 65, "Select next grenades", ActionGroup::Actions },
    { 56, "Select previous grenades", ActionGroup::Actions },
    { 23, "Use armor ability", ActionGroup::Actions },
    { 6, "Toggle night vision", ActionGroup::Actions },
    { kActionLowerWeapon, "Lower weapon", ActionGroup::Actions },
    { kActionPushToTalk, "Push to talk", ActionGroup::Actions },
    { kActionVoiceParty, "Voice to party", ActionGroup::Actions },
    { kActionVoiceTeam, "Voice to team", ActionGroup::Actions },
    { kActionVoiceGlobal, "Voice to everyone", ActionGroup::Actions },
    { kActionChatAll, "Chat to everyone", ActionGroup::Actions },
    { kActionChatTeam, "Chat to team", ActionGroup::Actions },
    { kActionChatParty, "Chat to party", ActionGroup::Actions },
    { kActionChatOpen, "Open chat", ActionGroup::Actions },
    { kActionThirdPerson, "3rd person camera", ActionGroup::Actions },
    { kActionVehicleFirstPerson, "Vehicle 1st person", ActionGroup::Actions },
    // Vehicle
    { kActionVehicleFirstPerson, "Vehicle 1st person", ActionGroup::Vehicle },
    { 24, "Vehicle function 1", ActionGroup::Vehicle },
    { 21, "Vehicle function 2", ActionGroup::Vehicle },
    { 22, "Vehicle function 3", ActionGroup::Vehicle },
    { 62, "Ascend (Falcon)", ActionGroup::Vehicle },
    { 63, "Descend (Falcon)", ActionGroup::Vehicle },
    // UI
    { 20, "Toggle scoreboard", ActionGroup::Interface },
    { 58, "Open loadouts menu", ActionGroup::Interface },
    // Forge
    { 25, "Ascend", ActionGroup::Forge },
    { 26, "Descend", ActionGroup::Forge },
    { 27, "Grab / release object", ActionGroup::Forge },
    { 28, "Boost", ActionGroup::Forge },
    { 29, "Precision edit", ActionGroup::Forge },
    { 30, "Delete object", ActionGroup::Forge },
    { 31, "Create object / object properties", ActionGroup::Forge },
    { 32, "Tools", ActionGroup::Forge },
    { 52, "Duplicate", ActionGroup::Forge },
    { 54, "Reset", ActionGroup::Forge },
    { 33, "Switch player mode", ActionGroup::Forge },
    { 34, "Zoom", ActionGroup::Forge },
    { 35, "Rotation mode", ActionGroup::Forge },
    { 51, "Toggle rotation axes", ActionGroup::Forge },
    { 53, "Lock object", ActionGroup::Forge },
    // Theater
    { 36, "Toggle panel", ActionGroup::Theater },
    { 37, "Toggle interface", ActionGroup::Theater },
    { 38, "Toggle first / third person", ActionGroup::Theater },
    { 39, "Camera reset", ActionGroup::Theater },
    { 40, "Jump forward", ActionGroup::Theater },
    { 41, "Jump back", ActionGroup::Theater },
    { 42, "Pause / resume playback", ActionGroup::Theater },
    { 43, "Fast forward", ActionGroup::Theater },
    { 44, "Toggle free camera", ActionGroup::Theater },
    { 45, "Boost", ActionGroup::Theater },
    { 46, "Theater panning", ActionGroup::Theater },
    { 47, "Ascend", ActionGroup::Theater },
    { 48, "Descend", ActionGroup::Theater },
    { 50, "Zoom", ActionGroup::Theater },
    // Used by other Halo games in MCC, not by Reach
    { 13, "Swap weapon (other games)", ActionGroup::Other },
    { 14, "Sprint (other games)", ActionGroup::Other },
    { 15, "Banshee bomb (other games)", ActionGroup::Other },
    { 49, "Dual wield (other games)", ActionGroup::Other },
    { 55, "Reload secondary (other games)", ActionGroup::Other },
    { 57, "Special action (other games)", ActionGroup::Other },
    { 59, "Activate waypoint (other games)", ActionGroup::Other },
    { 60, "Activate waypoint, alternate (other games)", ActionGroup::Other },
    { 61, "Ping navpoints (other games)", ActionGroup::Other },
    { 64, "Flashlight, alternate (other games)", ActionGroup::Other },
};

const ActionInfo* Find(int action) {
    for (const ActionInfo& a : kActions) {
        if (a.slot == action) return &a;
    }
    return nullptr;
}

} // namespace

int DefaultKey(int action) {
    return (action >= 0 && action < kActionCount) ? kDefaultKeys[action] : 0;
}

int EffectiveKey(int action) {
    const SettingsData s = Settings::Get().Snapshot();
    auto it = s.keyOverrides.find(action);
    return it != s.keyOverrides.end() ? it->second : DefaultKey(action);
}

const char* ActionName(int action) {
    const ActionInfo* a = Find(action);
    return a ? a->name : nullptr;
}

ActionGroup GroupOf(int action) {
    const ActionInfo* a = Find(action);
    return a ? a->group : ActionGroup::Other;
}

std::vector<int> ActionsIn(ActionGroup group) {
    std::vector<int> out;
    for (const ActionInfo& a : kActions) {
        if (a.group == group) out.push_back(a.slot);
    }
    return out;
}

const char* GroupName(ActionGroup group) {
    switch (group) {
    case ActionGroup::Movement: return "MOVEMENT";
    case ActionGroup::Actions: return "ACTIONS";
    case ActionGroup::Vehicle: return "VEHICLE CONTROLS";
    case ActionGroup::Interface: return "UI CONTROLS";
    case ActionGroup::Forge: return "FORGE";
    case ActionGroup::Theater: return "THEATER";
    default: return "OTHER HALO GAMES (NOT USED BY REACH)";
    }
}

std::string KeyName(int vk) {
    if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
    switch (vk) {
    case 0: return "-";
    case VK_LBUTTON: return "Left mouse";
    case VK_RBUTTON: return "Right mouse";
    case VK_MBUTTON: return "Middle mouse";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    case VK_LSHIFT: return "Left Shift";
    case VK_RSHIFT: return "Right Shift";
    case VK_LCONTROL: return "Left Ctrl";
    case VK_RCONTROL: return "Right Ctrl";
    case VK_LMENU: return "Left Alt";
    case VK_RMENU: return "Right Alt";
    default: break;
    }
    UINT scan = MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
    LONG lparam = static_cast<LONG>(scan << 16);
    switch (vk) {  // extended keys
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_INSERT: case VK_DELETE:
    case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT: case VK_DIVIDE: case VK_NUMLOCK:
        lparam |= 1 << 24;
        break;
    default: break;
    }
    wchar_t name[64] = {};
    if (scan != 0 && GetKeyNameTextW(lparam, name, 64) > 0) {
        char out[64] = {};
        WideCharToMultiByte(CP_UTF8, 0, name, -1, out, sizeof(out), nullptr, nullptr);
        return out;
    }
    return "Key " + std::to_string(vk);
}

} // namespace KeyBindings
} // namespace noble
