#pragma once

#include <string>
#include <vector>

namespace noble {

// Profile key-binding table (profile +0x42C, 66 entries of 0x18 bytes: {u32 action, u32 VK, 16 zero
// bytes}). Defaults are MCC's PC layout; players can override single actions in the SETTINGS tab.
// Slots 66 (kActionLowerWeapon), 67 (kActionPushToTalk), and 68 (kActionVehicleFirstPerson) are
// Noble-managed actions handled in ReachHost / VoiceChat.
namespace KeyBindings {

constexpr int kActionLowerWeapon = 66;
constexpr int kActionPushToTalk = 67;  // Noble-managed too: voice chat reads it, the engine never sees it
constexpr int kActionVehicleFirstPerson = 68;
constexpr int kActionChatAll = 69;    // Chat lines: Noble-managed like push to talk
constexpr int kActionChatTeam = 70;
constexpr int kActionChatParty = 71;
constexpr int kActionChatOpen = 72;   // reopens the last channel; unbound by default
// Voice-chat channel push-to-talk. Separate from kActionPushToTalk, which keeps using the TALK TO setting.
constexpr int kActionVoiceParty = 73;
constexpr int kActionVoiceTeam = 74;
constexpr int kActionVoiceGlobal = 75;
constexpr int kActionThirdPerson = 76;
constexpr int kActionToggleCrouch = 77;
constexpr int kActionCount = 78;

int DefaultKey(int action);            // VK code, 0 = unbound
int EffectiveKey(int action);          // default or the player's override
const char* ActionName(int action);    // nullptr when the action's purpose isn't confirmed

// MCC-style groups for the key-binding screen.
enum class ActionGroup { Movement, Actions, Vehicle, Interface, Forge, Theater, Other };
ActionGroup GroupOf(int action);
std::vector<int> ActionsIn(ActionGroup group);
const char* GroupName(ActionGroup group);
std::string KeyName(int vk);

} // namespace KeyBindings
} // namespace noble
