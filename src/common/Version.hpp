#pragma once

#include <string_view>

namespace noble {

constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 9;
constexpr int VERSION_PATCH = 16;
constexpr int VERSION_REVISION = 36;
#ifdef NOBLE_DEV_BUILD
constexpr std::string_view VERSION_STRING = "0.9.16.36-dev (Fixed Maximize Window, 16:9 UI Framing, Authentic Armory Cameras & Colors)";
#else
constexpr std::string_view VERSION_STRING = "0.9.16.36 (Fixed Maximize Window, 16:9 UI Framing, Authentic Armory Cameras & Colors)";
#endif
constexpr std::string_view APP_NAME = "Halo: Reach Community Edition";

} // namespace noble
