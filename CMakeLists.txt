cmake_minimum_required(VERSION 3.20)
if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS_INIT "/machine:x64 /MANIFEST:NO")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "/machine:x64 /MANIFEST:NO")
    set(CMAKE_USE_WIN32_THREADS_INIT 1 CACHE INTERNAL "" FORCE)
    set(CMAKE_USE_PTHREADS_INIT 0 CACHE INTERNAL "" FORCE)
    set(CMAKE_HAVE_PTHREADS_CREATE 0 CACHE INTERNAL "" FORCE)
    set(CMAKE_HAVE_LIBC_PTHREAD 0 CACHE INTERNAL "" FORCE)
    set(CMAKE_THREAD_LIBS_INIT "" CACHE INTERNAL "" FORCE)
    set(Threads_FOUND TRUE CACHE INTERNAL "" FORCE)
endif()
project(HaloReachCommunityEdition VERSION 0.8.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enforce UTF-8 and standard conformance on MSVC
if(MSVC)
    add_compile_options(/utf-8 /permissive- /Zc:__cplusplus /EHsc)
    add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Set output directories
if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<IF:$<CONFIG:>,Release,$<CONFIG>>)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<IF:$<CONFIG:>,Release,$<CONFIG>>)
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

# 1. spdlog (Fast C++ logging)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.0
    GIT_SHALLOW TRUE
)

# 2. nlohmann_json (Modern JSON parser)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

# 3. MinHook (Minimalistic x86/x64 API Hooking Library)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3
    GIT_SHALLOW TRUE
)

# 4. Dear ImGui (Immediate Mode GUI for DirectX 11)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
    GIT_SHALLOW TRUE
)

# 5. Opus (voice chat codec, BSD-3: assets/licenses/opus-LICENSE.txt ships with the game)
FetchContent_Declare(
    opus
    GIT_REPOSITORY https://github.com/xiph/opus.git
    GIT_TAG v1.5.2
    GIT_SHALLOW TRUE
)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(spdlog nlohmann_json opus)
if(MSVC)
    target_compile_options(opus PRIVATE /W0)
endif()

# Setup MinHook as static library
FetchContent_GetProperties(minhook)
if(NOT minhook_POPULATED)
    FetchContent_Populate(minhook)
    add_library(minhook STATIC
        ${minhook_SOURCE_DIR}/src/buffer.c
        ${minhook_SOURCE_DIR}/src/hook.c
        ${minhook_SOURCE_DIR}/src/trampoline.c
        ${minhook_SOURCE_DIR}/src/hde/hde64.c
        ${minhook_SOURCE_DIR}/src/hde/hde32.c
    )
    target_include_directories(minhook PUBLIC
        ${minhook_SOURCE_DIR}/include
        ${minhook_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(minhook PRIVATE /W0)
    endif()
endif()

# Setup Dear ImGui with Win32 and DX11 backends
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    )
    # src/ui/UiPad.cpp is the only path from a controller to ImGui (any XInput slot, front-end and pause menu).
    target_compile_definitions(imgui PRIVATE IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC d3d11)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/engine/PadLayouts.cpp
    src/engine/InputTuning.cpp
    src/common/MapNames.cpp
    src/engine/MapGameTypes.cpp
    src/engine/VariantRules.cpp
    src/engine/Playlists.cpp
    src/engine/MapGameTypesTest.cpp
    src/host/SteamLocator.cpp
    src/host/SteamWorkshop.cpp
    src/host/OwnershipVerifier.cpp
    src/host/Window.cpp
    src/host/D3D11Renderer.cpp
    src/host/Loader.cpp
    src/hooks/SigScanner.cpp
    src/hooks/HookManager.cpp
    src/hooks/LobbyPatches.cpp
    src/hooks/ForgePatches.cpp
    src/hooks/EnginePatches.cpp
    src/vfs/VFS.cpp
    src/engine/EmbeddedBlocks.cpp
    src/engine/BlamEngine.cpp
    src/engine/ReachHost.cpp
    src/engine/ReachArmory.cpp
    src/engine/ElitePack.cpp
    src/engine/CameraFov.cpp
    src/engine/CameraThirdPerson.cpp
    src/engine/FrameCapture.cpp
    src/common/Relaunch.cpp
    src/ui/UiArt.cpp
    src/ui/UIManager.cpp
    src/ui/ServerBrowser.cpp
    src/ui/ArmorCustomizer.cpp
    src/ui/WorkshopManager.cpp
    src/ui/ModDownloadPrompt.cpp
    src/ui/MissionLauncher.cpp
    src/network/UPnP.cpp
    src/network/SessionManager.cpp
    src/audio/VgmAudioHook.cpp
    src/audio/VoiceChat.cpp
    src/audio/ReachAudio.cpp
    src/audio/ProcessAudio.cpp
    src/ui/ModCatalog.cpp
    src/ui/InputPrompts.cpp
    src/ui/UiPad.cpp
    src/ui/LoadScreen.cpp
    src/ui/PauseMenu.cpp
    src/ui/SocialOverlay.cpp
    src/ui/ForgeMenu.cpp
    src/ui/FrontEndMenu.cpp
    src/ui/NobleStyle.cpp
    src/ui/MenuSounds.cpp
    src/ui/SettingsPanel.cpp
    src/ui/NobleUi.cpp
    src/ui/CareerPanel.cpp
    src/common/Settings.cpp
    src/common/Updater.cpp
    src/common/DeviceIdentity.cpp
    src/common/SaveCodec.cpp
    src/engine/KeyBindings.cpp
    src/engine/EmbeddedBlocks.cpp
    src/career/Career.cpp
    src/network/MasterServerClient.cpp
    src/network/SteamAuth.cpp
    src/network/NetworkTransport.cpp
    src/network/AuthClient.cpp
    src/network/SocialClient.cpp
    src/res/noble.rc
)

set(HEADERS
    src/engine/EmbeddedBlocks.hpp
    src/common/Logger.hpp
    src/common/Types.hpp
    src/common/Version.hpp
    src/host/SteamLocator.hpp
    src/host/SteamWorkshop.hpp
    src/host/OwnershipVerifier.hpp
    src/host/Window.hpp
    src/host/D3D11Renderer.hpp
    src/host/Loader.hpp
    src/hooks/SigScanner.hpp
    src/hooks/HookManager.hpp
    src/hooks/LobbyPatches.hpp
    src/hooks/ForgePatches.hpp
    src/hooks/EnginePatches.hpp
    src/vfs/VFS.hpp
    src/engine/BlamEngine.hpp
    src/engine/ReachHost.hpp
    src/engine/ReachArmory.hpp
    src/engine/ElitePack.hpp
    src/engine/CameraFov.hpp
    src/engine/CameraThirdPerson.hpp
    src/engine/FrameCapture.hpp
    src/common/Relaunch.hpp
    src/ui/UiArt.hpp
    src/ui/UIManager.hpp
    src/ui/ServerBrowser.hpp
    src/ui/ArmorCustomizer.hpp
    src/ui/WorkshopManager.hpp
    src/ui/ModDownloadPrompt.hpp
    src/ui/MissionLauncher.hpp
    src/ui/LoadScreen.hpp
    src/ui/PauseMenu.hpp
    src/ui/SocialOverlay.hpp
    src/ui/FrontEndMenu.hpp
    src/network/UPnP.hpp
    src/network/SessionManager.hpp
    src/audio/VgmAudioHook.hpp
    src/audio/VoiceChat.hpp
    src/audio/ReachAudio.hpp
    src/audio/ProcessAudio.hpp
    src/ui/ModCatalog.hpp
    src/ui/InputPrompts.hpp
    src/ui/NobleStyle.hpp
    src/ui/MenuSounds.hpp
    src/ui/SettingsPanel.hpp
    src/ui/NobleUi.hpp
    src/ui/CareerPanel.hpp
    src/common/Settings.hpp
    src/common/Updater.hpp
    src/engine/KeyBindings.hpp
    src/career/Career.hpp
    src/network/MasterServerClient.hpp
    src/network/NetworkTransport.hpp
    src/network/AuthClient.hpp
    src/network/SocialClient.hpp
)

add_executable(noble-client WIN32 ${SOURCES} ${HEADERS})
if(NOBLE_DEV_BUILD)
    target_compile_definitions(noble-client PRIVATE NOBLE_DEV_BUILD=1)
endif()
if(MSVC)
    target_compile_options(noble-client PRIVATE /W4)
endif()

target_include_directories(noble-client PRIVATE
    src
)

target_link_libraries(noble-client PRIVATE
    d3d11
    dxgi
    d3dcompiler
    windowscodecs
    ole32
    comdlg32
    ws2_32
    iphlpapi
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    minhook
    imgui
    opus
    dbghelp
    winhttp
    winmm
    xaudio2
    ole32
    windowscodecs
)

target_link_options(noble-client PRIVATE /MAP)

# Noble's own art (original work, tracked in assets\) goes into the data folder alongside the exe.
# Note: bulk runtime data\ is synced sequentially by build.bat / build-dev.bat / build-package.ps1 using robocopy
# so parallel links of noble-client and noble-dev never race on build/bin/Release/data or overwrite userdata.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
    add_custom_command(TARGET noble-client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:noble-client>/data
    )
endif()

# Developer build: same sources, NOBLE_DEV_BUILD, separate executable in the same output directory.
add_executable(noble-dev WIN32 ${SOURCES} ${HEADERS})
set_target_properties(noble-dev PROPERTIES OUTPUT_NAME "noble-dev")
target_compile_definitions(noble-dev PRIVATE NOBLE_DEV_BUILD=1)
if(MSVC)
    target_compile_options(noble-dev PRIVATE /W4)
endif()
target_include_directories(noble-dev PRIVATE src)
target_link_libraries(noble-dev PRIVATE
    d3d11 dxgi d3dcompiler windowscodecs ole32 comdlg32 ws2_32 iphlpapi
    spdlog::spdlog nlohmann_json::nlohmann_json minhook imgui opus
    dbghelp winhttp winmm xaudio2 ole32 windowscodecs
)
target_link_options(noble-dev PRIVATE /MAP)

# -----------------------------------------------------------------------------
# Headless Dedicated Server Binary (noble-dedicated.exe)
# -----------------------------------------------------------------------------
set(DEDICATED_SOURCES
    src/dedicated/main.cpp
    src/dedicated/DedicatedServer.cpp
    src/host/SteamLocator.cpp
    src/host/OwnershipVerifier.cpp
    src/host/Loader.cpp
    src/hooks/SigScanner.cpp
    src/hooks/HookManager.cpp
    src/hooks/LobbyPatches.cpp
    src/hooks/ForgePatches.cpp
    src/vfs/VFS.cpp
    src/engine/EmbeddedBlocks.cpp
    src/network/UPnP.cpp
    src/network/SessionManager.cpp
)

set(DEDICATED_HEADERS
    src/dedicated/DedicatedServer.hpp
    src/common/Logger.hpp
    src/common/Types.hpp
    src/common/Version.hpp
    src/host/SteamLocator.hpp
    src/host/OwnershipVerifier.hpp
    src/host/Loader.hpp
    src/hooks/SigScanner.hpp
    src/hooks/HookManager.hpp
    src/hooks/LobbyPatches.hpp
    src/hooks/ForgePatches.hpp
    src/vfs/VFS.hpp
    src/network/UPnP.hpp
    src/network/SessionManager.hpp
)

add_executable(noble-dedicated ${DEDICATED_SOURCES} ${DEDICATED_HEADERS})
if(NOBLE_DEV_BUILD)
    target_compile_definitions(noble-dedicated PRIVATE NOBLE_DEV_BUILD=1)
endif()
if(MSVC)
    target_compile_options(noble-dedicated PRIVATE /W4)
endif()

target_include_directories(noble-dedicated PRIVATE
    src
)

target_link_libraries(noble-dedicated PRIVATE
    ws2_32
    iphlpapi
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    minhook
    dbghelp
)
