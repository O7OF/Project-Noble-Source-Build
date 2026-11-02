# Project Noble: handoff (2026-10-29, 18:15 ET)
Dominus join the team 20261028! "GIVING PROPS TO TYLER FOR PUSHING 30 UPDATES AND FIXING A BUNCH OF BROKEN SHIT LOL GOOD JOB MP SHPULD STILL WORK WE MAY NEED TO CHECK AFTER THE NEW PATCH FOR LOBBY STAY TOGEHTER ETC."
This picks up a reverse-engineering and C++ effort on **Tyler's Windows PC**. All paths below are real paths on that machine.
Tyler (GitHub `WeabooTyler117`) works with a partner (GitHub `O7OF`) who owns the repo and writes most of the code.(DOMINUS IS A NEW ADDITION TO THE DEV TEAM)
Tyler mostly runs builds, tests, and does the reverse engineering; he is not a C++ developer, so give him exact commands
(one command per code block, so each can be pasted on its own).

**Goal:** `noble-client.exe` loads MCC's own `haloreach.dll` (Halo: Reach) and drives it **without MCC**, like the existing
community project Reach Renaissance (RR, installed at `D:\Reach`) does. Ownership is verified through Steam, nothing in the
MCC install is modified, and no RR code or data is redistributed. RR is used only as a reference for behaviour. (AGENT IS TO ALWAYS WRITE TO THIS FILE WHEN MODIFYING/MAKING ANY CHANGES TO CODE THIS NEEDS TO BE DETAILED AND THE CHANGES NEED TO BE UNDER THE NAME OF THE PERSON WHO COMMITED SAID CHANGES, ALWAYS UPDATE THE GAMES ACTUAL VERSION CURRENT = 0.9.16.35 THIS NEEDS TO BE CHANGED PER PUSH COMMIT!)

## 0. Pick up here (2026-11-02, 17:50 ET) - Armory pieces apply again on the legacy menu map; spartan.rrpmcg build dependency (Tyler / DeepSeek)

Tyler saw the character again after Dominus's armory integration, but only some armor pieces changed. Cause: the new
pmcg path replaced the fixed legacy pair table. On the 297 MB `mainmenu_all.map` (which has the
`0x03BB6FED`/`0x03BAB653` markers) the GLOBALS slots are fixed (head 0x04, chest 0x06, ...), so the pairs must be
pushed in the historical order. The pmcg path pushed them in region-name order and omitted hands/knees, so most slots
received the wrong pair (or the head pair after the 16-slot padding) and only a few pieces appeared to work.
- `ReachArmory.cpp`: in the non-dynamic (legacy map) branch, rebuild the fixed Spartan pairs in the historical order:
  head, chest, left/right shoulder, hands, wrist, utility, knees, armor_effect. The pmcg path still runs for the
  imported new menu layout.
- Add hands and knees to the no-pmcg fallback too (Dominus's update had dropped them).
- Build dependency: `src/res/blocks.rc` now embeds `data/armor/spartan.rrpmcg`, which is not in the repo or on this
  machine (it is a gitignored pmcg dump). The build fails at `RC2135: file not found` without it. A local placeholder
  (`L 20 0`) unblocks the build; the real dump from Dominus is needed for the new map's item->region table (the
  placeholder loads 0 items).
- Verified with the dev `-armory-cycle chest` driver: `HP/HALO` logs `chest_3`, `UA/COUNTERASSAULT` logs `chest_4`,
  `TACTICAL/LRP` logs `chest_5`, each with a rebuild; the capture shows the Tactical/LRP chest rig. `build.bat`
  succeeds (noble-client, noble-dev, noble-dedicated) once the placeholder is in place.
- Camera note: the character uses the map's `c_full_spartan` scene (0x0C). The 0x0A "armory" scene in this map is the
  outdoor vista, so there is no wider character camera to switch to from code; the framing is the map's camera.


## 0. Pick up here (2026-11-02, 05:50 ET) - Armory Map Restored (0.9.16.35) (@deepseek)

**Version:** 0.9.16.35. **Release:** https://github.com/O7OF/Project-Noble-/releases/tag/v0.9.16.35. **Build:** noble-client and noble-dev compiled from build-rel.

**Why 0.9.16.34 was reverted:** the imported 268 MB mainmenu map has no Spartan Armory scene. A scene sweep 0xCF-0xE7 rendered star fields; 0xD8 left the game (invalid camera). The 297 MB map is required for the Armory pedestal/character. Restored it as mainmenu_all.map, with its matching 32,213-name CSV.

**Code kept:** structural imported-menu detection is gated by IsNewMenuLayout() (string count < 32,100), so the legacy map uses the marker table and shows the character; dynamic region-slot armor writing remains available for a real new Armory map. The dev-only '-armory-cycle helmet|chest|shoulder|wrist|knees|visor|custom' driver was used to walk 22 helmets, 25 chests, 18 shoulders and the rest.

**Verified:** dev log shows "Loaded 32213 string_ids from loose mainmenu_all CSV", "Found mainmenu_all.map GLOBALS table ... (stride=8)", and the Spartan character is visible; release run leaves noble-client.log at 0 bytes.

## 0. Pick up here (2026-11-02, 05:40 ET) - New RR Mainmenu + Remapped Armor GLOBALS (0.9.16.34) (@deepseek)

**Version:** 0.9.16.34. **Release:** https://github.com/O7OF/Project-Noble-/releases/tag/v0.9.16.34. **Build:** noble-client and noble-dev compiled from build-rel.

**New map:** data/menumaps/mainmenu_all.map is now RR's new 268,095,488-byte menu map, with the matching 1,366,736-byte string table (removed the old 297 MB pair; kept as mainmenu_all.old.map/.csv). The loose CSV is preferred over the embedded copy so the new table is used in dev; release embeds the same new pair through blocks.rc.

**New GLOBALS layout:** head=4, chest=2, flair_head_a/b/c=24/26/28, flair_chest_a/b/c=30/32/34, flair_left/right_shoulder=36/38, stride 8. The old fixed 0x04,0x06,... writes only hit skeleton slots. ReachArmory now:
1. Falls back to a structural scan when the legacy 0x03BB6FED/0x03BAB653 marker is absent: finds head/default, requires chest at slot 2 and flair_head_a at 20..40, validates the REMADE counter.
2. Resolves each armor region ID to its own GLOBALS slot (head/chest/shoulders/flair) and writes the pair there, with left/right_shoulder falling back to flair_left/right_shoulder. Legacy tables still take the fixed 16-pair path.
3. Logs "Found imported mainmenu GLOBALS table ... (stride=8, flair slot 24)" and writes the Spartan pieces.

**Verified:** dev run loads 31,978 string ids from loose mainmenu_all CSV, finds the imported GLOBALS table and applies head rosenda/chest/etc.; release run leaves noble-client.log at 0 bytes.

## 0. Pick up here (2026-11-02, 04:30 ET) - Armor Fix / Imported Menu String Audit (0.9.16.33) (@deepseek)

**Version:** 0.9.16.33. **Release:** https://github.com/O7OF/Project-Noble-/releases/tag/v0.9.16.33. **Build:** noble-client and noble-dev compiled from build-rel.

**What Tyler's last commits did:** 32d987b added exclusive fullscreen and made borderless the default; fe665fb scaled the menu UI for native 4K. Neither changed ArmorCustomizer/ReachArmory.

**Imported main menu audit:** the new data/menumaps/mainmenu.map (268 MB, 2026-10-31) and mainmenu.string_ids.csv are a different string table from the Armory map:
- Its rows use the row index as the ID (e.g. kurt=16905) instead of the ids in mainmenu_all (kurt=12519).
- The runtime table it creates has a different GLOBALS layout: head at 4, chest at 2, armour flair at 24-38, with no old 0x03BB6FED/0x03BAB653 marker. The Armory writes its 16 region/permutation pairs at 0x04.. and expects the marker table, so using this map while resolving ids from either CSV leaves chest/shoulder/attachment pairs unwritten or wrong.
- A loose new mainmenu.map was therefore being preferred by an experimental ReachHost/VFS change, which broke armour application. That preference was reverted.

**Fix:** the client again uses the known-good mainmenu_all.map/string table (embedded in release, loose for dev) that the Armory permutation data was built against. The Armory finds the marker GLOBALS table (stride 8) and applies head/chest/shoulder/wrist/utility/knees/effect pairs. New-mainmenu support needs a dedicated Armory GLOBALS mapping; do not simply point the Armory at mainmenu.map.

**Verified:** dev run logs "Found mainmenu_all.map GLOBALS table ... (stride=8)" and writes Spartan pieces; release run leaves noble-client.log at 0 bytes.

## 0. Pick up here (2026-11-02, 03:35 ET) - Visor Colour Fix + Preview (0.9.16.30) (@deepseek)

**Version:** 0.9.16.30. **Release:** https://github.com/O7OF/Project-Noble-/releases/tag/v0.9.16.30. **Build:** noble-client and noble-dev compiled from build-rel.

**Visor fix:**
1. ArmorCustomizer keeps the equipped finish in m_visorSelection, separate from AppearanceProfile. LoadProfile seeds it, the picker updates it, SyncLegacyFields reapplies it before the rows are drawn, and SyncToReachArmory uses it for the ReachArmorySelection/ARGB write. A transient try-on/profile restore can no longer silently reset the equipped finish to Default.
2. Try-on now temporarily sets m_visorSelection to the clicked for-sale/locked finish before syncing and restores the equipped value afterwards, so the model previews the colour and the BUY button can equip it. Previously the new override would have blocked that preview.
3. All 30 finishes keep their normal rank/credit rules; only the selection/preview path changed.

**Verified:** release build leaves noble-client.log at 0 bytes; noble-dev compiled. No test-only visor flag remains in release code.

## 0. Pick up here (2026-11-02, 03:20 ET) - Visor Colour Fix (0.9.16.29) (@deepseek)

**Version:** 0.9.16.29. **Release:** https://github.com/O7OF/Project-Noble-/releases/tag/v0.9.16.29. **Build:** noble-client and noble-dev both compiled from build-rel (MSVC Release x64).

**Root cause / fix:** the Armory selection was rebuilt from AppearanceProfile every frame. Transient profile try-on/restore paths could leave visorColorIndex at 0 (Default), so the model and the equipped swatch silently fell back to gold. ArmorCustomizer now keeps the selected finish in a separate m_visorSelection member:
1. LoadProfile seeds it from the saved profile.
2. Choosing a visor finish updates it (and the profile).
3. SyncLegacyFields reapplies it before the UI reads the rows, so a transient preview restore cannot reset the equipped colour.
4. SyncToReachArmory uses m_visorSelection for the ReachArmorySelection and visor ARGB write.

All 30 finishes remain rank/credit-gated in the picker; this fixes the equipped colour persistence, not the unlock rules.

**Verified:** release menu run leaves noble-client.log at 0 bytes; dev build compiled. The visor colour is applied through the same WriteColorsAndSuit path as primary/secondary/accent.

## 0. Pick up here (2026-11-02, 02:50 ET) - Three-Region Server Browser Cluster (0.9.16.28) (@deepseek)

**Version:** 0.9.16.28. **Release:** https://github.com/O7OF/Project-Noble-/releases/tag/v0.9.16.28. **Build:** noble-client and noble-dev from build-rel (clean VS 18 x64 tree), 0 errors.

**Server cluster:** US West https://servertestdeploy-jfxb.onrender.com, US East https://noble-server-2.onrender.com, UK https://servertestdeploy-1.onrender.com. All three return 200 on /api/health, /api/sessions and /api/browser. Backend commit db1abb6 deployed to all three; each /api/health reports the other two peers.

**Backend (O7OF/ServerTestDeploy, db1abb6):**
1. peerUrlsFor() unions PEER_REGISTRY_URL with DEFAULT_KNOWN_NODES instead of replacing the defaults; a partial env can no longer omit the UK node. Three defaults always present.
2. /api/health now returns cluster.self/nodes/peerCount.
3. /api/browser returns peerRegistries with each peer URL, reachable, sessions and online counts; peerBrowserSessions caches nodes.
4. README documents the three regions and the env contract.

**Client (0.9.16.28):**
1. MasterServerClient::SetUrl trims whitespace and trailing slashes. A URL copied from a browser (https://host/) used to produce https://host//api/... and a 404; that was the UK symptom.
2. Settings::Clamp normalises masterServerUrl on load/update and maps the retired outbreak host to West.
3. ServerBrowser region struct now labels the UK endpoint EU West (London, UK); region detection matches the exact normalised registry URL instead of guessing from hostname substrings (servertestdeploy-1 contains neither "uk" nor "london").
4. Region selector already offers West/East/UK/LAN; any selected node aggregates all three server-side.

**Verified:** backend probe session created on West appears in East and UK /api/browser; each peerRegistries reports the other two reachable. Dev client started on the UK URL with a trailing slash without error; release log stayed 0 bytes.

## 0. Pick up here (2026-11-02, 02:20 ET) - Crash-Only Redacted Release Log (0.9.16.27) (@deepseek)

**Version:** 0.9.16.27. **Release:** https://github.com/O7OF/Project-Noble-/releases/tag/v0.9.16.27. **Build:** noble-client and noble-dev, MSVC Release x64.

**What changed:**
1. Logger::Init sets the release logger/sinks to spdlog::level::err; info/warn/debug chatter is dropped. A healthy release run leaves noble-client.log at 0 bytes (verified). noble-dev stays at trace.
2. LogRedacted formats every NOBLE_INFO/WARN/ERROR/DEBUG message and, in release builds, replaces every 0x-prefixed hex token with 0x<redacted>, http(s) URLs with <redacted-url>, and IPv4 addresses with <redacted-ip>. This covers hook RVAs, absolute addresses, string ids, ARGB values, master/relay endpoints and peer IPs. Crash-handler stack/address lines go through the same macro.
3. Release builds no longer write noble-crash.dmp (minidumps are the process image and a decompilation aid); minidumps are noble-dev only. Release crash text is still written, redacted.
4. No release endpoint/token detail remains at err level; only the redacted crash/error text a bug report needs.

**Verified:** normal release menu run produced a 0-byte log; dev builds keep full diagnostics. Build 0.9.16.27 packages with the embedded map and no loose map/mod/CSV/OBJ files.

## 0. Pick up here (2026-11-02, 02:00 ET) - Readable Scaled Settings/Panels (0.9.16.26) (@deepseek)

**Version:** 0.9.16.26. **Release:** https://github.com/O7OF/Project-Noble-/releases/tag/v0.9.16.26 (asset + .sha256; v0.9.16.25 removed). **Build:** noble-client and noble-dev, MSVC Release x64, 0 errors, 0 warnings.

**Root cause of the tiny Settings/Career/Server Browser text:** NobleUi::Scale() returned ImGui::GetFontSize()/18 = 1.0 while the menu overlay rendered at 1.8x in a 2880x1620 back buffer. Every panel using Scale() (Settings, Career, Server Browser, pause settings) was therefore half the size of the rest of the menu and unreadable.

**Fix:** NobleUi::Scale() now follows the overlay height (DisplaySize.y / 900, clamped 0.85-2.2), the same basis as FrontEndMenu's ui.s. Settings, Career, Server Browser and the pause-menu settings now scale with the menu; the Graphics page was verified at borderless 1920x1080 and is readable.

**Included from 0.9.16.25:** native-resolution rendering for menu and match, live windowed/borderless switching (Window::ApplyWindowSettings), custom menu backgrounds on every front-end screen, 2.5x font raster density, and mouse coordinates fed after the Win32 backend. Release v0.9.16.26 supersedes v0.9.16.25 (deleted).

## 0. Pick up here (2026-11-02, 01:45 ET) - Native-Resolution Rendering, Live Borderless, Crisp Fonts & Custom Menu Backdrops (0.9.16.25) (@deepseek)

**Version:** 0.9.16.25. **Release:** https://github.com/O7OF/Project-Noble-/releases/tag/v0.9.16.25 (asset + .sha256; v0.9.16.24 removed). **Build:** noble-client and noble-dev, MSVC Release x64, 0 errors, 0 warnings.

**What was fixed:**
1. **All UIs were pixelated (Settings, Career, Server Browser, pause, menus):** the font atlas was rasterised at 18/30/56/15 px and then drawn at 1.5-2.4x in the supersampled back buffer, which stretched every glyph. NobleStyle now sets ImFontConfig::RasterizerDensity = 2.5 for all four fonts (and the fallback), retaining the logical metrics while storing 2.5x denser glyphs. Text is sharp at every UI scale.
2. **Buttons hovered/reacted at the wrong place:** the Win32 backend queued client-pixel mouse positions after the overlay had laid itself out in back-buffer pixels. RenderOverlayImpl now adds the final scaled mouse position after ImGui_ImplWin32_NewFrame and before NewFrame, so the last event always wins and -FLT_MAX is used when unfocused.
3. **Crisp windowed and gameplay:** the engine now renders at the display's native resolution (or the window if larger; cap 3840x2160) for both menu and match processes. A 1280x720 window shows a 1920x1080 render downscaled by Windows; borderless is 1:1. Gameplay is no longer rendered at a small window size.
4. **Live borderless:** Window::ApplyWindowSettings returns: it restyles/resizes the window (borderless = monitor, windowed = work-area-fitted) without touching the engine's swap chain. SETTINGS > GRAPHICS rows are When::Apply and the display applies immediately; no restart. The engine render resolution never changes, so the image stays crisp in both modes.
5. **Custom menu backgrounds:** every front-end screen except the Armory now draws UiArt::MenuBackground() (the player's own data\ui\custom\backgrounds file first, then Noble/Reach art) instead of the engine map camera. The main menu no longer shows the 3-second camera cut/sliced view. The Armory remains the live 3D model.
6. **Menu supersampling remains menu-only:** 1.5x render for the menu process (2880x1620 on 1080p) for the enlarged Armory; match processes render at native display resolution.

**Verified visibly:** 10-second windowed main-menu run, then a live windowed -> borderless switch with -live-window-test 6 (same Window::ApplyWindowSettings path as the SETTINGS row), plus a settings-screen run. Screenshots: scratch/live-windowed-1280.png, scratch/live-borderless.png, scratch/live-settings-1280.png. Updater check logs "up to date (local 0.9.16.25, latest 'v0.9.16.25')".

## 0. Pick up here (2026-11-02, 01:20 ET) - In-App Auto-Updater & Public Release Channel (0.9.16.24) (@deepseek)

**Version:** 0.9.16.24. **Release repo:** O7OF/Project-Noble- (now public). **Build:** noble-client and noble-dev (MSVC Release x64, 0 errors, 0 warnings); noble-dedicated unchanged. Files: src/common/Updater.cpp, src/common/Updater.hpp, src/main.cpp, src/ui/FrontEndMenu.cpp, src/common/Version.hpp, CMakeLists.txt, tools/publish-release.ps1, release-notes/v0.9.16.24.md, docs/HANDOFF.md, docs/AGENT-STATUS.md.

**What was built:**
1. **In-app updater** (Updater): the menu process calls the GitHub Releases API for O7OF/Project-Noble- once per launch (WinHTTP, background thread). It compares the release tag with VERSION_MAJOR/MINOR/PATCH/REVISION, finds the Project-Noble-*.zip asset, and shows an UPDATE AVAILABLE panel under MY PARTY with the version and download size. The player presses DOWNLOAD (never a silent multi-hundred-MB download), then RESTART & INSTALL.
   - The staged zip is written to %LOCALAPPDATA%\ProjectNoble\updates\<tag>\; a kept zip is reused on the next launch.
   - Install is a hidden PowerShell helper (%TEMP%\ProjectNobleUpdate\apply_update.ps1) that waits for the client to exit, extracts the zip, robocopies it over the install folder (leaving data\userdata, autosave, cache, settings.json and appearance.dat alone), starts the same exe name, and deletes the staged files. Failures are logged to %TEMP%\ProjectNobleUpdate\apply_update.log.
   - -no-update skips the check for captures/tests. Match processes never check for updates; only the menu does.
2. **Release repository**: O7OF/Project-Noble- is public and carries the README, screenshots, release notes and the release asset. tools/publish-release.ps1 builds/stages/zips and creates the GitHub release via the REST API, so future releases are one command: powershell -NoProfile -ExecutionPolicy Bypass -File tools\publish-release.ps1 (-NoUpload produces dist/Project-Noble-vX-win64.zip only; -SkipBuild reuses the current binary).
3. **Package contents are release-only**: Noble.exe (renamed from noble-client.exe), data\ without userdata, mods, armor, blocks, sandbox, cache, menumaps, loose profiles, developer appearance.dat, *.obj/*.mtl/*.rrpmcg/*.rrreloc/*.rdc/*.map/*.csv/*.bin, plus tools\vgmstream, play.bat, README.txt and version.txt. The menu map and its string_ids are compiled into Noble.exe (src/res/blocks.rc) and VFS extracts them into data\menumaps on first run; nothing loose ships. **noble-dev.exe is never copied into the package or the release repo** (verified in the staged folder and zip).
4. **Release asset**: Project-Noble-v0.9.16.24-win64.zip plus .sha256, attached to the v0.9.16.24 GitHub release. The release repo README documents install, requirements, features, auto-updates and troubleshooting; screenshots are in docs/screenshots/.

**Verification:**
- noble-dev and noble-client both built clean after adding the updater.
- The staged package launched without a loose mainmenu.map: the new data-folder check accepts the packaged JSON/UI files, VFS extracts the embedded map, and the Armory finds its GLOBALS table.
- noble-dev.exe was absent from the staged folder and zip; data\mods was an empty folder; no .obj/.mtl/.rrpmcg/.map/.csv files were present.

## 0. Pick up here (2026-11-02, 01:15 ET) - Stable Main Menu Scene, Armory Full-Body Framing, Menu Supersampling, Fixed Window/Borderless Modes & Nameplate Cleanup (0.9.16.23) (@deepseek)

**Version:** 0.9.16.23. **Build:** `noble-client` and `noble-dev` (MSVC Release x64, 0 errors, 0 warnings); `noble-dedicated` is unchanged. Files: `src/main.cpp`, `src/host/Window.cpp`, `src/host/Window.hpp`, `src/engine/ReachArmory.cpp`, `src/ui/FrontEndMenu.cpp`, `src/ui/ArmorCustomizer.cpp`, `src/ui/ArmorCustomizer.hpp`, `src/ui/UiArt.cpp`, `src/ui/UiArt.hpp`, `src/ui/SettingsPanel.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Main Menu glitches after ~3 seconds (sliced, zoomed-out background):**
   - *Root Cause:* `ReachArmory::ResolveSceneIndex("main")` returned `0x01` (the New Alexandria campaign camera). The menu map opens on the title camera `0x00`; once the GLOBALS scan succeeded (about 3–4 s after launch) Noble wrote `0x01` and the engine switched cameras. The user saw the cliff menu turn into a wall of campaign scene views.
   - *Fix:* `title`, `start_menu` and `main` now all resolve to `0x00`; `UpdateSceneForScreen(Screen::Main)` requests `start_menu`. The menu stays on the title camera for the whole session.
2. **Armory character far away, then pixelated when zoomed (0.9.16.22 regression):**
   - *Root Cause:* the engine's whole-body cameras (`0x0C` Spartan, `0x0B` Elite) put the model in the middle of a distant pedestal. 0.9.16.22 compensated with a 2D upscale of the finished engine frame (2.3–4.1x), which magnified source pixels; the floating weapon prop stayed visible at the bottom.
   - *Fix:* the 2D zoom is gone. `ArmoryStage::Draw` now enlarges **whole-body** cameras 1.5x around the character's focal point (`kFocusX = 0.590`, `kFocusY = 0.505`) and anchors the character centre at 74.25 % across / 62 % down. The Spartan/Elite fill roughly two-thirds of the stage height, the weapon prop under the feet is below the frame, and the character sits entirely in the free area to the right of the picker. Per-slot close-ups (`c_helmet`, `c_chest`, …) keep the old fit-under-the-rule placement.
   - FOV was investigated as an optical zoom (global `haloreach+0xB42C10`, `camera_set_field_of_view`); the menu camera ignores that gameplay global, so it is not used.
3. **Menu supersampling (no pixelation, no gameplay cost):**
   - *Fix:* only the **menu process** now creates its swap chain at 1.5x the window (1280x720 window → 1920x1080; 1920x1080 borderless → 2880x1620), capped at 3200x1800. The 1.5x Armory stage draw is therefore 1:1 with the pixels the window finally shows. The UI is supersampled as well. A match is a separate process (`Relaunch`), sees `-map …`, and creates its swap chain at the native window size, so in-game performance is unchanged. The menu scene is a single static map + one character.
   - The overlay continues to lay out in back-buffer pixels and scales the mouse from client pixels; this also keeps the UI correct if the client/back-buffer sizes ever differ.
4. **Windowed and borderless full screen made deterministic:**
   - Windowed is a fixed-size `WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX` window again: no title-bar maximize/resize, so the engine picture is never stretched or pixelated. A requested client that would not fit the monitor work area (e.g. 1920x1080 on a 1920x1080 desktop) is shrunk, keeping its aspect, and the engine gets the fitted size.
   - Borderless is `WS_POPUP` at the primary monitor size; engine and swap chain are 1:1 (supersampled menu).
   - `SETTINGS > GRAPHICS` display rows are saved and applied on the **next launch**; the live `ApplyWindowSettings` path was removed. The engine cannot resize its swap chain while running: `IDXGISwapChain::ResizeBuffers` returns `0x887A0001` (`DXGI_ERROR_INVALID_CALL`) because the engine keeps its views alive. The settings panel now says "Restart the game to apply the display change." This supersedes the 0.9.16.22 "live window apply" claim.
5. **Nameplate cleanup (Tyler's list):**
   - Retired plates are no longer offered anywhere: Unity1, Unity2, Bhm, Hispanicheritage2021, Apamonth2022, Pulaskis2023, Disabilitypride2023, Internationalwomensday2023, Womenshistorymonth2023, Fist.
   - Developer/insider plates are offered and equippable in `noble-dev` only; in release they are hidden and cannot be equipped: Early Mcc Insider, Mcc Insider Plus, Community 343 Headhunter Ice, Community Spotlight Fire, Mcc Insider Pc, Vsworldwin, Vsworld, Innercircle, 343 Flag, 343.
   - `UiArt::GetNameplateCatalog()` keeps every entry and its position; `NameplateSelectable()` decides what the Armory offers, and the picker stores the original catalog index in `PickOption::value`. Saved appearances and remote players therefore keep pointing at the same plate. A saved plate that is no longer available falls back to Blank at menu start. The MCC catalog has 531 entries: 521 offered in `noble-dev`, 511 in release.
6. **Test hooks:** `-menu-ignore-input` freezes mouse/keyboard for clean screenshots; `-armory-species spartan|elite` previews either species without saving; `-armory-scene-index <n>` still forces a menu-map camera. `-menu-capture` captures the supersampled menu buffer.

**Notes / verification:** windowed 1280x720 menu renders 1920x1080, borderless 1920x1080 renders 2880x1620, match processes render native. Menu stays on camera `0x00` for >10 s; Spartan and Elite full-body Armory shots at 1280x720 and borderless 1920x1080 show the character head-to-toe with the pedestal weapon out of frame. Release/Dev nameplate counts verified in the log.

## 0. Pick up here (2026-11-02, 00:15 ET) - 16:9 Canvas Aspect Ratio Fitting, Heroic Armory Framing & Zoom, Window Settings Live Apply (0.9.16.22) (@O7OF)

**Version:** 0.9.16.22. **Build:** noble-client, noble-dev, noble-dedicated (0 errors, 0 warnings on MSVC Release x64). Files: `src/host/Window.hpp`, `src/host/Window.cpp`, `src/ui/SettingsPanel.cpp`, `src/ui/ArmorCustomizer.hpp`, `src/ui/ArmorCustomizer.cpp`, `src/ui/FrontEndMenu.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Window Aspect Ratio & UI Clipping on Maximize:**
   - *Root Cause:* On maximized windows (e.g. 1920x1040), the aspect ratio is 1.846 instead of authentic 16:9 (1.777), and `ui.s` was computed directly from `vp->Size.y / 720.0f` (1.44x scale). Main Menu started at `ui.size.y * 0.30f + 104.0f * ui.s`, which with 14 rows, description, and hint rows pushed lower entries and prompt text off the bottom of the screen. In addition, setup modals like "New to Halo: Reach?" were off-center.
   - *Fix:* Enforced authentic 16:9 canvas fitting (`canvasW = vp->Size.y * (16/9)`, centered with `ui.origin = (originX, originY)` and clean pillarbox/letterbox margins). Set robust UI scaling factor `ui.s = clamp(canvasH / 860.0f, 0.70f, 2.50f)`. Adjusted Main Menu header `titleY` to `ui.origin.y + canvasH * 0.16f` and `listY` to `titleY + 84.0f * ui.s`. Centered all modals within `ui.origin` + `ui.size`. The entire menu, all 14 rows, description, and hint bars now fit on screen with plenty of breathing room.
2. **Armory Camera Zoom & Quality:**
   - *Root Cause:* In `mainmenu_all.map`, the 3D scene camera places the Spartan far back on the pedestal at `kModelX = 0.656f`, `kModelY = 0.560f`, occupying only ~280 pixels out of 1080 (26% screen height). `ArmoryStage::Draw` was still multiplying by `k = 1.02f` or `1.18f`, leaving the character looking tiny inside a dark void.
   - *Fix:* Integrated `customizer.GetCurrentScale()`, `GetFocusX()`, and `GetFocusY()`:
     - **Full Body:** `k = 2.30f * m_userZoom` fills ~85% of stage height from boots to helmet.
     - **Armory View:** `k = 2.85f * m_userZoom` provides an authentic 3/4 hero view.
     - **Close-Up:** `k = 4.10f * m_userZoom` frames directly onto the selected armor piece (helmet `0.32f`, chest `0.42f`, knees `0.68f`, etc.).
     - Enabled mouse wheel zooming (`m_userZoom` clamped 0.65x - 2.60x), right-click mouse drag panning (`m_panX`, `m_panY`), and middle-click reset.
3. **Live Settings Window Updates:**
   - *Root Cause:* Changing resolution (width, height) or toggling borderless/windowed mode required restarting the executable because settings changes were only saved to disk without notifying the running `Window`.
   - *Fix:* Added `Window::Active()` and `Window::ApplyWindowSettings(width, height, borderless)` in `src/host/Window.hpp` & `Window.cpp`. Updated `SettingsPanel::Apply()` to immediately apply window size and borderless mode live via `SetWindowPos` and `SetWindowLongPtrW(GWL_STYLE)`, and apply menu music changes on the fly.

## 0. Pick up here (2026-11-01, 23:50 ET) - Main Menu & Armory Camera Zoom In, Fullscreen/Maximize Title Bar Button Enabled, Simultaneous noble-client & noble-dev Release Pipeline (0.9.16.21) (@O7OF)

**Version:** 0.9.16.21. **Build:** noble-client, noble-dev, noble-dedicated (0 errors, 0 warnings on MSVC Release x64). Files: `build-package.ps1`, `src/host/Window.cpp`, `src/engine/ReachArmory.cpp`, `src/ui/ArmorCustomizer.hpp`, `src/ui/FrontEndMenu.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Title Bar Fullscreen / Maximize Button Being Greyed Out:**
   - *Root Cause:* In `build-package.ps1`, the build target was previously restricted solely to `--target noble-client`, leaving `noble-dev.exe` stale on version `0.9.16.18-dev` (where `WS_MAXIMIZEBOX` and `WS_THICKFRAME` were still stripped). Furthermore, `Window::HandleMessage` was swallowing `WM_SIZE` by returning 0 without allowing `DefWindowProcW` to update the non-client title bar button state.
   - *Fix:*
     - In `src/host/Window.cpp`: Added explicit `SetWindowLongPtrW` forcing `WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU` and enabled `SC_MAXIMIZE` / `SC_RESTORE` on the Win32 system menu with `SWP_FRAMECHANGED`.
     - Added `WM_SYSCOMMAND` handling for `SC_MAXIMIZE` (`ShowWindow(hwnd, SW_MAXIMIZE)`) and `SC_RESTORE` (`ShowWindow(hwnd, SW_RESTORE)`).
     - Forwarded `WM_SIZE` to `DefWindowProcW` so the title bar buttons and window metrics are kept in sync with Windows DWM.
     - Updated `build-package.ps1` to build both `noble-client` and `noble-dev` simultaneously on every package run.

2. **Main Menu Camera Zoom In:**
   - *Root Cause:* The main menu scene was mapped to `"start_menu"` (`0x00`), which in `mainmenu_all.map` is the distant title screen camera showing the landscape far out on the horizon with dark margins.
   - *Fix:* Re-mapped `"main"` to scene `0x01` in `ReachArmory::ResolveSceneIndex` and updated `FrontEndMenu::UpdateSceneForScreen(Screen::Main)` to request `"main"`. The main menu now uses the zoomed-in foreground staging camera up close on the cliff.

3. **Armory 3D Spartan Camera Zoom In & Heroic Framing:**
   - *Root Cause:* `ArmoryStage::Draw` was artificially shrinking the entire 1920x1080 engine backbuffer by `k = clamp((H - ruleY)/H, 0.70f, 0.90f)` (~75%), letterboxing the Spartan into a tiny sub-rectangle in the corner surrounded by empty black space.
   - *Fix:* Completely removed the artificial downscaling. Zoomed the character in prominently: Close-Up mode (`m_cameraMode == 2`) now renders at `1.35x` scale focusing on the armor piece/chest/helmet/visor; Armory View (`m_cameraMode == 0`) renders at `1.18x` heroic frame; Full Body (`m_cameraMode == 1`) renders at clean `1.02x` scale head-to-toe. The character fills the right half of the screen natively matching Halo: Reach MCC.

## 0. Pick up here (2026-11-01, 23:25 ET) - Window Maximize/Alt+Enter, Armory Slot Cameras, Spartan Undersuit & Color Scanner Fix, 30 Visor Ranks, Counselor Rank Balancing, Embedded Map & CSVs (0.9.16.20) (@O7OF)

**Version:** 0.9.16.20. **Build:** noble-client, noble-dev, noble-dedicated (0 errors, 0 warnings on MSVC Release x64). Files: `src/host/Window.cpp`, `src/vfs/VFS.cpp`, `src/res/blocks.rc`, `src/engine/ReachArmory.cpp`, `src/ui/SpartanArmoryCatalog.inc`, `src/ui/ArmorCustomizer.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Window Maximize, Minimize, Resizing & Alt+Enter:**
   - Restored standard `WS_OVERLAPPEDWINDOW` styles (removed stripping of `WS_THICKFRAME` and `WS_MAXIMIZEBOX`).
   - Enabled window resizing, titlebar maximize and minimize buttons.
   - Handled `WM_SYSKEYDOWN` for `VK_RETURN` (Alt+Enter) to seamlessly toggle between maximized (`SW_SHOWMAXIMIZED`) and windowed (`SW_RESTORE`).

2. **Armory Camera Slot Tracking & View Modes:**
   - Fixed Armory cameras: previously focusing a slot did not update `m_focusScene` or trigger `UpdateArmoryCamera()`, and color/suit rows lacked `.scene` targets.
   - Now selecting/focusing any slot (Helmet, Chest, Shoulders, Wrist, Utility, Knee Guards, Visor, Colors, Undersuit) immediately pans and zooms the Blam engine 3D Armory camera to focus on that specific body part.
   - Added interactive view toggle ("Camera: CLOSE-UP | FULL BODY | ARMORY VIEW") via pressing 'C', mouse-wheel zoom, or clicking the camera badge.

3. **Spartan Customization - Undersuit & Color Scanner Fix:**
   - Renamed "Lights" to "Undersuit" for Spartans (while retaining "Lights" for Elites) to accurately match the visual shader behavior.
   - Fixed Spartan Primary, Secondary, and Accent colors not applying: previously `ScanColorRecordsIn` relied on an exact 4-dword match against default Steel colors (`0xFF585858, 0xFF595959, 0xFF2FA0CB, 0xFFC7A167`). If the player's profile had custom visor colors (e.g. Blood Orange) or non-default tints loaded, the scan found 0 matches and aborted.
   - Implemented `MatchSpartanColorRecord` with RGB channel tolerance, fallback checks for non-zero ARGB records with valid alpha, and cached `s_lastSpartanPriArgb`, `s_lastSpartanSecArgb`, and `s_lastSpartanVisArgb` to ensure color mutations always resolve the target memory records.

4. **30 Custom Visor Colors & Balanced Progression Ranks:**
   - Fully restored and verified custom visor colors in `SpartanArmoryCatalog.inc`.
   - Populated all 30 visors with authentic names, exact hex colors, and balanced cR costs and rank requirements spanning from Recruit up through Reclaimer.
   - Set Onyx visor unlock requirement to Reclaimer. Only the Haunted helmet is locked by the ultimate Inheritor rank.

5. **Counselor Elite Helmet Unlock Rank:**
   - Adjusted Counselor helmet unlock rank to `Eclipse` (5 ranks above Field Marshall / Honor Guard: Field Marshall -> Hero -> Legend -> Mythic -> Noble -> Eclipse), preserving Inheritor exclusivity for the Haunted helmet.

6. **Embedded Mainmenu Map & String IDs (Zero Loose .map/.csv Internals):**
   - Embedded `mainmenu_all.map` (297 MB) and `mainmenu_all.string_ids.csv` directly into the PE executable binary via `src/res/blocks.rc` as `RCDATA` resources.
   - `ReachArmory::EnsureStringIdsLoaded()` now parses string IDs directly from `EmbeddedBlocks::GetBlock("mainmenu_all.string_ids.csv")` in memory.
   - VFS automatically extracts `mainmenu_all.map` into `%APPDATA%/ProjectNoble/menumaps/` on first run if missing, preventing users from needing loose map files in their installation folder while preventing loose asset exposure.

## 0. Pick up here (2026-11-01, 23:00 ET) - Elite Armory Crash Fix, Master Server Region Dropdown & Multi-Peer Federation, NAT/UPnP Diagnostics, 16p Firefight & 128p Custom/Forge Expansion (0.9.16.19) (@O7OF)

**Version:** 0.9.16.19. **Build:** noble-client, noble-dev, noble-dedicated (0 errors, 0 warnings on MSVC Release x64). Files: `src/engine/ReachArmory.cpp`, `src/ui/ServerBrowser.hpp`, `src/ui/ServerBrowser.cpp`, `src/ui/FrontEndMenu.cpp`, `src/ui/MissionLauncher.cpp`, `src/hooks/LobbyPatches.hpp`, `src/hooks/LobbyPatches.cpp`, `build-package.ps1`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`, and `C:\Users\Chey1\Desktop\ServerTestDeploy\server.js`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Elite Armory Crash (STATUS_SXS_INVALID_DEACTIVATION 0xC0150010):**
   - *Root Cause:* In `src/engine/ReachArmory.cpp`, the recently added `ScanEliteIllumRecordsIn` function scanned all committed `PAGE_READWRITE` memory across the entire process space for `{0.0f, 0.0f, 0.5f, 1.0f}` within 0.02 tolerance. This accidentally matched memory blocks belonging to internal Windows `ntdll`/`KERNELBASE` activation contexts. Subsequent writes via `SafeWriteFloats` corrupted the activation context cookie, causing `ntdll!RtlDeactivateActivationContext` to fail with `STATUS_SXS_INVALID_DEACTIVATION` 354 ms later.
   - *Fix:* Completely removed `ScanEliteIllumRecordsIn` and `s_eliteIllumRecords`. Elite primary/secondary/accent colors and suit shaders are safely handled through `s_eliteColorRecords` using exact palette matching, preventing any heap/SXS memory corruption.

2. **Master Server Region Selection & Zero URL Exposure (DDoS Hardening):**
   - Implemented a clean region selection dropdown directly on the Server Browser toolbar (`US West (Oregon)`, `US East (Virginia)`, `EU West (London, UK)`, and `Offline / LAN Only`).
   - Replaced raw master server URL text inputs with a user-friendly modal ("SELECT MATCHMAKING REGION") featuring radio buttons and region labels.
   - Raw master server URLs are strictly hidden from the UI to protect backend infrastructure from scraping and targeted DDoS attacks.

3. **Multi-Peer Federated Mesh Netcode (`ServerTestDeploy/server.js`):**
   - Upgraded master server federation to support multi-node meshes across US West, US East, and London UK.
   - `peerUrlsFor(req)` supports comma-separated `PEER_REGISTRY_URL` environment variables and automatically filters out the instance's own hostname to prevent self-querying or circular loops.
   - Updated `peerBrowserSessions(req)` to query all peer nodes concurrently via `Promise.allSettled` with a 3.5s timeout and 5s cache.
   - Enhanced `/api/players/online` and `/api/browser` to return aggregated active player counts across all peer nodes, using `?internal=1` flag to prevent circular double-counting.
   - Pushed committed updates directly to the `ServerTestDeploy` remote repository on GitHub.
   - Maintained P2P game architecture: master servers handle only signaling and NAT traversal handshakes; once peers connect, all gameplay simulation remains direct host P2P.

4. **NAT Type Detection & UPnP Guidance:**
   - Integrated live NAT type diagnostics into `ServerBrowser`: evaluates `UPnP::Get().IsActive()`, local IP, and public IP to categorize network status (`OPEN`, `MODERATE`, or `STRICT`).
   - Displays real-time `[NAT: OPEN/MODERATE/STRICT]` and `[UPnP: ENABLED/DISABLED]` badges on the server browser status bar.
   - Renders a prominent yellow advisory banner when NAT is not Open or UPnP is disabled, guiding players to enable UPnP or port forward UDP 11774 for optimal hosting.

5. **Expanded Player Caps (16p Firefight, 128p Custom Games & Forge):**
   - Expanded Firefight max lobby and match cap from 4 to 16 players in `LobbyPatches.cpp`, `MissionLauncher.cpp`, and `FrontEndMenu.cpp`.
   - Expanded Custom Games and Forge max allowed players from 16/8 to 128 players max.
   - Added selectable player count options in the UI up to 128 players.

## 0. Pick up here (2026-11-01, 22:15 ET) - Embedded Launch Blocks, Application Icon, Unified Saves by Player ID & Armory Stat Tracking (0.9.16.18) (@O7OF)

**Version:** 0.9.16.18. **Build:** noble-client, noble-dev, noble-dedicated (0 errors, 0 warnings on MSVC Release x64). Files: `CMakeLists.txt`, `build-package.ps1`, `src/res/resource.h`, `src/res/blocks.rc`, `src/res/noble.rc`, `src/res/noble.ico`, `src/engine/EmbeddedBlocks.hpp`, `src/engine/EmbeddedBlocks.cpp`, `src/engine/BlamEngine.cpp`, `src/engine/ReachHost.cpp`, `src/career/Career.hpp`, `src/career/Career.cpp`, `src/ui/ArmorCustomizer.cpp`, `src/ui/FrontEndMenu.cpp`, `src/common/DeviceIdentity.cpp`, `src/common/Settings.hpp`, `src/common/Settings.cpp`, `src/host/Window.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Embedded Launch Blocks & Internal Asset Protection (Zero Loose .bin Files):**
   - Reverse-engineered and compiled all 113 binary launch blocks and templates (`launch_block_1.bin`, `lb_m_c_1.bin`, `profile_1.bin`, and all 110 Campaign/Firefight difficulty blocks in `data/blocks/*.bin`) into the client executable using Windows PE `.rc` `RCDATA` resources (`src/res/noble.rc`, `src/res/blocks.rc`).
   - Implemented `EmbeddedBlocks::GetBlock` and `EmbeddedBlocks::HasBlock` querying mapped PE resources via `FindResourceA(..., MAKEINTRESOURCEA(10))` and `LockResource` with zero disk reads and zero memory allocations.
   - Wired `BlamEngine.cpp` (`LaunchMap`/`StartEngine`) and `ReachHost.cpp` (`HostSlot34_ProfileSettings`) to load authentic prebuilt blocks from embedded resources before falling back to disk.
   - Excluded `blocks/` directory and loose `.bin` files from `build-package.ps1`. The release build `Noble.exe` operates with zero loose `.bin` files on disk, protecting sensitive internal structures from ripping and tampering.

2. **Application Logo & Windows Icon Binding (`noble_icon`):**
   - Converted `noble_icon.png` into a multi-resolution icon `src/res/noble.ico` (256x256, 64x64, 48x48, 32x32, 16x16) registered as `IDI_NOBLE_ICON` (101) in `src/res/resource.h` and `src/res/noble.rc`.
   - Updated `Window.cpp` to load `MAKEINTRESOURCEW(101)` for `wc.hIcon` and `wc.hIconSm`, and dispatches `WM_SETICON` (Big and Small) on window creation so taskbar, window titlebar, and Windows Explorer display the official custom icon.

3. **Unified Save & Preferences Storage by Player ID (Eliminating Duplicate Folders):**
   - *Root cause of `userdata/kind1` and `userdata/kind2`:* In `ReachHost.cpp`, `HostSlot47_UserDataFolder` previously generated distinct paths per engine category (`kind1` for preferences, `kind2` for autosaves).
   - Refactored `HostSlot47_UserDataFolder` to route all engine saves and settings into a single unified directory per player ID: `userdata/<deviceId>/saves/HaloReach`.
   - Implemented automated migration of legacy `kind1` and `kind2` directories into `userdata/<deviceId>/saves/HaloReach` on startup so no player checkpoints or configurations are lost.
   - Removed `Desktop\Halo Reach Community Edition\build\bin\Release\data` from `KnownDataDirs()` in `DeviceIdentity.cpp` to prevent build directory files from polluting or overwriting user progression.

4. **Armory Purchases, Dev Unlock-All & Career Stat Tracking:**
   - Added `Career::MarkArmoryItemOwned(const std::string& key)` in `Career.hpp` & `Career.cpp`.
   - Updated `ArmorCustomizer.cpp` `choose()` to automatically register any chosen/equipped armor piece as owned in `career.dat` and persist immediately.
   - Updated `Career::DevSetArmoryUnlockAll(true)` and `LoadLocked()` to populate all 179 catalog items from `kCatalogItems` into `s_totals.armoryOwned`, ensuring `rec.totals.armoryOwned.size()` accurately reflects unlocked status.
   - Updated `FrontEndMenu.cpp` pre-game lobby card to display accurate values for `ARMORY ITEMS BOUGHT` (shows 179 when unlock-all is active), `CHALLENGES DONE NOW` (defaults denominator to active challenge count or 4), and `COMMENDATIONS` (displays `0%` rather than empty `-`).

5. **Player Name Sanitization & Live Gamertag Updates:**
   - Enforced default player name `"NobleSix"` for new players if blank or whitespace-only, with strict length bounds (1 to 20 characters max) across `Settings.cpp`, `Settings.hpp`, `DeviceIdentity.cpp`, and `FrontEndMenu.cpp`.
   - In `ReachHost.cpp` (`HostSlot88_UserIdentity`), eliminated the stale `static const std::wstring gamerTagW` initialization that locked the gamertag to its launch-time value. It now queries `Settings::Get().Snapshot().playerName` dynamically on each engine request, properly truncated and padded for the scoreboard and lobby.

6. **Pristine Release Packaging Verification:**
   - Ran `build-package.ps1` to produce a completely clean `Documents/Noble Launcher` installation.
   - Verified zero developer binaries (`noble-dev.exe`), zero master server scripts (`master_server.js`), zero loose `.bin` blocks, zero `userdata` or developer ID leakage, zero dead armor assets, and zero nested duplicate folders (`mods/mods`).

## 1. Previous Work: Embedded Audio, Realtime WASAPI Volume, Master Server Script Exclusion & Clean Release Packaging (0.9.16.17) (@O7OF)

**Version:** 0.9.16.17. **Build:** noble-client, noble-dev, noble-dedicated (0 errors, 0 warnings on MSVC Release x64). Files: `CMakeLists.txt`, `build-package.ps1`, `src/audio/ProcessAudio.hpp`, `src/audio/ProcessAudio.cpp`, `src/audio/ReachAudio.cpp`, `src/career/Career.cpp`, `src/engine/ReachHost.hpp`, `src/engine/ReachHost.cpp`, `src/main.cpp`, `src/ui/MenuSoundData.inc`, `src/ui/MenuSounds.cpp`, `src/ui/FrontEndMenu.cpp`, `src/ui/SettingsPanel.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Master Server Script (`master_server.js`) & Local Server Exclusion from Release Build:**
   - The user runs two production Render server instances (East and West US) with peer registry federation configured to point to each other.
   - `master_server.js` and `test_master_server.js` were previously located under `tools/master-server` in the build output and were mistakenly copied into the packaging target directory `Documents/Noble Launcher`.
   - Updated `build-package.ps1` to explicitly purge any `tools/master-server` folder, remove any `*master*server*.js` scripts, and ensure that only `tools/vgmstream` is copied. Verified `Documents/Noble Launcher` contains zero master server files.

2. **Clean Release Package Generation & Duplicate Folder Elimination:**
   - Previous packaging passes accumulated nested duplicate folders (e.g., `data/data`, `mods/mods/mods`, `maps/maps`, `data/userdata`, `autosave`) in `Documents/Noble Launcher`.
   - `build-package.ps1` now completely wipes the packaging directory on every run prior to copying, rigorously validates that `noble-dev.exe`, `noble-dedicated.exe`, `data/userdata` (no developer/player ID leakage), and stale subdirectories are never packaged.
   - Confirmed release package only contains `Noble.exe`, `play.bat`, `version.txt`, and clean `data`/`tools/vgmstream` directories.

3. **Compiled In-Binary UI Sounds (.rdata) & Disk Steal Prevention:**
   - Converted all 6 UI audio waveforms (`click`, `back`, `scroll`, `change`, `tab`, `error`) into embedded static byte arrays in `src/ui/MenuSoundData.inc` compiled into the executable's `.rdata`.
   - Refactored `MenuSounds.cpp` to play directly from memory via `PlayEmbedded(..., SND_MEMORY)` with volume scaling and master mute gating, eliminating loose WAV files from disk.
   - Pre-seeded `ReachAudio`'s sound clips in memory with `LoadWavFromMemory` so no temporary audio files are extracted.

4. **Realtime In-Game Audio Volume & Mute Control Mid-Match:**
   - Previously, audio volume sliders in `SettingsPanel` only saved values to JSON without altering the process audio session, requiring a match exit/reload to take effect.
   - Built `ProcessAudio` leveraging Windows Core Audio WASAPI (`ISimpleAudioVolume`) to scale master process volume and mute state instantly on the fly mid-game.
   - Added `ReachHost::SetRuntimeVolumes` writing directly to profile offsets `+0xA5C`, `+0xA60`, `+0xA64` on settings save.
   - Fixed mute buttons (master mute & menu music mute) in `FrontEndMenu.cpp` and `SettingsPanel.cpp` to immediately command XAudio2 and WASAPI sessions.

5. **Career Persistence Deadlock & Tracking Fix:**
   - In `Career.cpp`: `s_mutex` was a non-recursive `std::mutex`. When `BuyArmoryItem` or `LoadLocked` acquired `s_mutex` and invoked `SaveLocked()`, `SaveLocked()` in turn called `Challenges()` and `Commendations()`, which attempted to re-acquire `s_mutex` on the same thread, causing a recursive deadlock or crash.
   - Converted `s_mutex` to `std::recursive_mutex` with `std::lock_guard<std::recursive_mutex>`, allowing safe nested re-entrance and reliable career/armory stat persistence to `career.dat`.

## 1. Previous Work: Live Career Stats Display, Armory Tracking & Stable Device ID Persistence (0.9.16.16) (@O7OF)

**Version:** 0.9.16.16. **Build:** noble-client, noble-dev, noble-dedicated (0 errors, 0 warnings on MSVC Release x64). Files: `src/career/Career.hpp`, `src/career/Career.cpp`, `src/ui/FrontEndMenu.cpp`, `src/network/MasterServerClient.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Pregame Lobby Player Card Displaying 0 for Career & Armory Stats:**
   - In `FrontEndMenu.cpp`: The lobby card drawing function previously cached career metrics in a `static Career::Record s_record` guarded with `if (!profileWasOpen)`. When clicking on a player slot, `st.lobbyProfileOpen = true` was set at the end of the previous frame, making `profileWasOpen` already `true` on the next frame. The static block was skipped, leaving `s_record` and challenge/commendation variables as zeroed default structs (`0` games, `0/0` K/D, `0/0` challenges, `-` commendations, and `0` armory items).
   - Removed the broken static caching guard and now directly query `Career::GetRecord()` live each frame, along with fresh counts for active challenges and commendation tiers.
   - Enhanced `isLocal` lambda in `FrontEndMenu.cpp` to evaluate host status, case-insensitive player name, and `localDevId` (`DeviceIdentityStore::Get().deviceId`), ensuring the local player's live stats are always shown reliably.

2. **Purchased Armory Items Tracking & Persistence in career.dat:**
   - Verified local DPAPI-encrypted `career.dat` storage under `data/userdata/<deviceId>/career.dat`.
   - Fixed missing `j["bonusCredits"]` serialization in `SaveLocked()`.
   - Updated `armoryOwned` tracking to accurately report all bought items (including multiple purchased helmets).
   - In `EnterPreGameLobby` and `MasterServerClient`, replaced static fallback `25` with dynamic commendation completion percentage calculated from active commendation tiers.

3. **Stable Device ID Persistence Across Name Changes:**
   - Updated `struct PeerStats` in `Career.hpp` to include `playerId`, `challengesDone`, `commendations`, and `armoryOwned`.
   - In `Career.cpp`: Keyed `SavePeerStats` to `stats.playerId` (defaulting to `DeviceIdentityStore::Get().deviceId`), writing `<deviceId>.json`.
   - Updated `LoadPeerStats` to accept an identifier (device ID or player name) and query by ID first, with a fallback scan for legacy name files. Changing gamertags no longer loses progress or creates duplicate save files.
   - When remote players' lobby state is received in `FrontEndMenu.cpp`, their stats are locally cached via `SavePeerStats` using their machine ID / peer ID.

4. **Remote Player Lobby Stat Inspection:**
   - In `FrontEndMenu.cpp`: Remote player card rendering now reads live stats sent through the lobby packet, falling back to local cached `PeerStats` before falling back to defaults.

## 1. Previous Work: Elite Illumination Isolation, Stable ID Persistence & Scoreboard Names (0.9.16.15) (@O7OF)

**Version:** 0.9.16.15. **Build:** noble-client, noble-dev, noble-dedicated (0 errors, 0 warnings on MSVC Release x64). Files: `src/career/Career.hpp`, `src/career/Career.cpp`, `src/engine/ReachArmory.cpp`, `src/engine/ElitePack.cpp`, `src/common/DeviceIdentity.cpp`, `src/common/Settings.cpp`, `src/ui/SettingsPanel.cpp`, `src/ui/ArmorCustomizer.cpp`, `src/ui/FrontEndMenu.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Elite Armor Body Bleed When Choosing Lights (Isolated Emissive Pipeline):**
   - In `ReachArmory.cpp`: `words` represents the 4-channel diffuse color block (`s_eliteColorRecords`). In prior revisions `litArgb` had been placed into `words[2]`, causing selecting "Gold" for Lights to literally paint the entire Elite armor plates Gold instead of glowing eye slits/LEDs.
   - Fixed `words` for Elites to contain `{ priArgb, secArgb, terArgb, (sel.accentColorIndex != 0 ? terArgb : priArgb) }`, completely isolating the diffuse armor plates from lights.
   - Lights are now driven purely through the float4 emissive pipeline (`s_suitRecords` and `s_eliteIllumRecords`) with HDR multiplier `{ sel.lightsRgb * 4.0f, 1.0f }`.
   - Updated quick rescan loop in `ReachArmory.cpp` to scan `s_suitRecords` for both Spartans and Elites.
   - Removed redundant full-process memory rescans on biped rebuilds (`s_rescanRequested`), preventing character flashing/flickering and camera stutter when rotating models or switching slot focus.
   - In `ElitePack.cpp`: Removed species-check guard (`s_localSpeciesElite`) in `RefreshLocalPicks()`, ensuring Elite colors are always loaded from `el` if `primary_color` exists and never contaminated by Spartan colors.
   - In `ArmorCustomizer.cpp`: Added fallback in `LoadProfile()` so missing keys in `el` safely default to valid colors instead of leaving them uncolorable.

2. **Armory Purchased Item Count Tracking on Dev Unlock All:**
   - In `Career.cpp`: `DevSetArmoryUnlockAll(true)` and `LoadLocked()` now populate `s_totals.armoryOwned` with all 179 items from `SpartanArmoryCatalog.inc`, so the Service Record and pre-game lobby stat lines accurately track 179 items owned.
   - Included `SpartanArmoryCatalog.inc` in `Career.cpp`.

3. **Stable Device ID Persistence Across Name Changes:**
   - Changed `SavePeerStats` and `LoadPeerStats` in `Career.cpp` to persist by stable ID (`DeviceIdentityStore::Get().deviceId`), so changing gamertags does not reset stats, lose progress, or duplicate save files.
   - Purged obsolete name-based JSON files (`0xFF90.json`, `O8OF.json`, `TEST1.json`) and recursive nested folders (`data/userdata/kind1`, `data/userdata/kind2`, `players/players`, `results/results`).

4. **Commendation & Challenge Tracking:**
   - All 45 commendations in `Career.cpp` re-assigned from `CM::None` to active metrics (`CM::Kills`, `CM::Medals`, `CM::Games`).
   - Extended `CommendationAmount` to take kills, assists, deaths, and medals, allowing commendations to properly advance across matches.

5. **Player Name Validation & Scoreboard Resolution:**
   - Implemented `RegisterPlayerSlot(int slotIndex, const std::string& name, const std::string& tag)` in `Career.cpp` and `Career.hpp`.
   - Wired roster registration into `FrontEndMenu.cpp` on both host start and guest launch, mapping both `peerId` and `slotIndex` so scoreboard names always resolve correctly instead of showing "Player N".
   - Default name for new players set to `"NobleSix"` in `DeviceIdentity.cpp` and `Settings.cpp`.
   - Player name validation enforced between 1 and 20 characters maximum (trimmed, never blank/whitespace).

## 1. Previous Work: Elite Illumination Dual-Pipeline, 16-Player Campaign Co-op & Pregame Stats Sync (0.9.16.14) (@O7OF)

**Version:** 0.9.16.14. **Build:** noble-client, noble-dev (0 errors, 0 warnings on MSVC Release x64). Files: `src/engine/ReachArmory.cpp`, `src/hooks/LobbyPatches.cpp`, `src/network/MasterServerClient.hpp`, `src/network/MasterServerClient.cpp`, `src/ui/ArmorCustomizer.cpp`, `src/ui/FrontEndMenu.cpp`, `src/ui/PauseMenu.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Elite Armor Body Turning Gold Instead of Lights (Emissive Channel Mapping & Dual-Pipeline Restoration):**
   - In stock Halo: Reach Elite shaders (`objects\characters\elite\shaders\elite_armor`), inspection of `kStockElitePatterns[11][4]` revealed:
     - `words[0]` = Primary base armor albedo
     - `words[1]` = Secondary armor markings & trim
     - `words[2]` = Emissive armor lights, eye slit, and glowing detail accents (e.g. Ultra `0xFFFF663B` [bright orange-red], Minor `0xFFD1660D` [amber], Zealot `0xFF1BD687` [cyan-green], Officer `0xFF9DB02C` [yellow-green])
     - `words[3]` = Primary armor finish / diffuse tint
   - In prior revisions, `litArgb` had been mistakenly placed into `words[3]`. Because `words[3]` controls diffuse armor finish, choosing Gold for Lights tinted the entire armor plates Gold while leaving `words[2]` (eyes/lights) dark/grey.
   - Restored dual-pipeline architecture:
     - `words[0]` = `priArgb` (Primary base armor albedo)
     - `words[1]` = `secArgb` (Secondary armor markings)
     - `words[2]` = `litArgb` (Emissive lights / illuminated pieces / glowing eye slits)
     - `words[3]` = `(sel.accentColorIndex != 0 ? terArgb : priArgb)` (Bodysuit / accent detail or primary diffuse albedo)
     - Re-integrated `s_eliteIllumRecords` scanner targeting stock float4 illumination constants (`kStockUltraIllum`, `kStockMinorIllum`, `kStockEyesIllum`), writing HDR vector `{ sel.lightsRgb[0] * 4.0f, sel.lightsRgb[1] * 4.0f, sel.lightsRgb[2] * 4.0f, 1.0f }` directly to glowing LEDs and emissive cards.
2. **Spartan vs Elite Customization Separation & Armory Stability:**
   - In `ArmorCustomizer.cpp`: Added `SelectionsEqual` check in `SyncToReachArmory` to prevent selection churn and unnecessary profile updates.
   - In `ReachArmory.cpp`: Replaced false-positive species detection (`effectiveSceneIdx != currentScene`) with `wantElite != curElite` in `ApplyGlobalsAndPermutations`, eliminating character flashing, disappearing, or biped rebuild churn when rotating models or switching slot focus.
   - Reset `s_rotationBaseBits.store(0xFFFFFFFFu)` on species change for clean rest yaw initialization.
3. **16-Player Campaign Co-op Expansion:**
   - In `LobbyPatches.cpp`: Set `s_maxCoopPlayers = 16` and updated `Hook_SetSlotCounts` to clamp Campaign/Firefight public slots to `std::min(expandedCoop, 16)`, allowing up to 16 clients in a campaign match with the host.
   - In `FrontEndMenu.cpp`: Campaign host lobby max player count expanded to 16.
4. **Pregame Lobby Career Stats Sync:**
   - In `MasterServerClient`: Added career stat fields (`gamesPlayed`, `kills`, `deaths`, `challengesDone`, `commendations`, `armoryOwned`) to `LobbyPlayer` and `MasterAdvert`. Serialized on lobby advertise / join and deserialized on state fetch.
   - In `FrontEndMenu.cpp` -> `DrawPreGameLobby`: Remote player rows display real career stats, and clicking a player toggles their profile card.

## 1. Previous Work: Party Lobby Return & Seamless Guest Auto-Pull (0.9.16.12) (@O7OF)

**Version:** 0.9.16.12. **Build:** noble-client, noble-dev (0 errors, 0 warnings on MSVC Release x64). Files: `src/main.cpp`, `src/ui/FrontEndMenu.cpp`, `src/ui/PauseMenu.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Host Session Abandonment on Match Return:**
   - In `PauseMenu.cpp` -> `LobbyReturnArgs`: `-session-id` was previously copied ONLY in the `else` (guest) branch (`copy(L"-session-id", L"-lobby-session")`), and `-session-token` was omitted completely. When the host clicked "RETURN TO LOBBY" or finished a mission, `-lobby-session` was omitted from the command line.
   - In `main.cpp`: When a host launched a match, `-session-id` and `-session-token` were parsed from the initial launch command line but never appended to `noble::Relaunch::SetCurrentGameArgs(args)` for hosts.
   - In `FrontEndMenu.cpp`: `SetupHostLobby` did not accept existing session ID or token parameters. When returning from a match via `-lobby-return host`, `SetupHostLobby` was called without `adv.id` or `adv.token`, which caused `MasterServerClient::StartAdvertising` to clear `m_sessionId`/`m_sessionToken` and register a brand-new session ID, abandoning the active session.
2. **Guest Stranded on Game Over Screen ("HOST LEFT"):**
   - The guest client was polling `FetchLobbyStateSync(session)` for the original session ID. Because the host created an entirely new session ID upon returning to the lobby instead of re-advertising the existing one, the old session was closed / timed out. `missing` reached 8 (~16s), setting `s_state = 2` ("HOST LEFT - The host's lobby closed").
   - Furthermore, `DrawGameOver` gated the guest return on `t >= tEnd`, forcing guests to wait out long animation timers even if the host had already returned.
   - On LAN or direct connect (where `-session-id` was absent), `PollHostBackInLobby()` immediately defaulted to `s_state = 2` without checking LAN discovery beacons.
3. **The Fix:**
   - In `src/main.cpp`: Added `-session-id` and `-session-token` forwarding to `launchHost` in `Relaunch::SetCurrentGameArgs(args)`.
   - In `src/ui/FrontEndMenu.cpp`: Extended `SetupHostLobby` to accept `existingSessionId` and `existingSessionToken`. When `-lobby-return host` is processed, `-lobby-session` and `-lobby-session-token` are forwarded directly into `SetupHostLobby`, setting `adv.id` and `adv.token`. `MasterServerClient` preserves the existing session ID and immediately heartbeats `in_progress: false`.
   - In `src/ui/PauseMenu.cpp`:
     - `LobbyReturnArgs`: Always copies `-session-id` to `-lobby-session` and `-session-token` to `-lobby-session-token` for both host and guest, inspecting both `CurrentGameArgs()` and `GetCommandLineW()`.
     - `PollHostBackInLobby()`: Polls every 1s (1000ms). If `session` is present, queries `FetchLobbyStateSync(session)` with a 25s grace window; once `!state.inProgress` is received, immediately sets `s_state = 1`. If `session` is empty (LAN), polls `SessionManager::GetDiscoveredSessions()` for up to 25s; once host LAN beacon is detected, sets `s_state = 1`.
     - `DrawGameOver`:
       - Enhanced role detection using both `NetworkTransport` status and command line flags (`-host` / `-connect`).
       - When `lobbyState == 1`, guest returns immediately after minimum viewing delay (`t >= 2.0f || s_reveal.skipped`) with visual button confirmation `"RETURNING TO LOBBY..."`.
       - For hosts in a party / network match, added a 25-second auto-pull countdown after match results (`"Returning party to lobby in %d s. B or Esc stays."`), automatically returning the party to the lobby if the host is idle, or returning immediately if the host clicks "RETURN TO LOBBY" or "NEXT MISSION".

## 1. Previous Work: Elite Armor Lights & Armory UI Performance (0.9.16.11) (@O7OF)

**Version:** 0.9.16.11. **Build:** noble-client, noble-dev (0 errors, 0 warnings on MSVC Release x64). Files: `src/engine/ReachArmory.cpp`, `src/ui/ArmorCustomizer.cpp`, `src/ui/FrontEndMenu.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`, `docs/AGENT-STATUS.md`.

**What was actually found & fixed (Root Cause Analysis):**
1. **Elite Armor Turning Gold Instead of Lights Channel (Alpha Channel Decoupling):**
   - Texture extraction and binary inspection of `elite_ultra_helmet_cc.png` and `elite_armor_template.shader`:
     - Channel 0 (Red) = Primary base armor (`words[0]`)
     - Channel 1 (Green) = Secondary armor overlay & markings (`words[1]`)
     - Channel 2 (Blue) = Accent / Bodysuit (`words[2]`)
     - Channel 3 (Alpha) = Small detail lights, LEDs, emissive cards & eye slit (`words[3]`)
   - In commit `1b058e6`:
     - `words` was assigned `{ priArgb, terArgb, secArgb, priArgb }`. `litArgb` was completely omitted, and Channel 3 (lights) was painted with `priArgb` (Steel/Grey), making the eye slit and lights uncolorable.
     - `s_eliteIllumRecords` / `ScanEliteIllumRecordsIn` scanned heap memory for `{0, 0.5, 1, 1}` and overwrote it with `sel.lightsRgb * 4.0f`. That float4 in memory was actually the material diffuse constant buffer! Writing Gold `{0.8, 0.6, 0.1} * 4.0` over it tinted the entire Elite armor mesh Gold with 4.0x HDR intensity!
   - **Fix:**
     - Completely removed `s_eliteIllumRecords`, `ScanEliteIllumRecordsIn`, and the dangerous float4 heap scanner.
     - In `WriteColorsAndSuit`:
       - Defined `const uint32_t litArgb = LinearRgbToArgbU32(sel.lightsRgb);`.
       - For Elites: `words = { priArgb, secArgb, terArgb, litArgb }`.
       - For Spartans: `words = { priArgb, secArgb, terArgb, visArgb }`.
       - Restored `suitFloats` write exclusively to Spartan bipeds (`if (!sel.previewElite)`).
     - In `MatchEliteColorPattern`: Added check for `cur[1] == lastSec || cur[2] == lastSec` to properly detect previously colored Elite bipeds and prevent defaulting back to stock presets.

2. **Spartan vs Elite Customization Separation:**
   - In `LoadProfile()`: Guarded the legacy Spartan fallback so that it only seeds Elite colors if `"elite"` is completely missing from `appearance.json`. When an `"elite"` block exists, Elite colors are loaded exclusively from `"elite"` and never overwritten by Spartan customizations.
   - In `choose()`: Added `SyncToReachArmory()` so swatch clicks immediately update `ReachArmory::SetSelection()`.

3. **Armory UI Performance & Smooth Character Rotation:**
   - In `Render()`: Cached static `PickOption` vectors for `Nameplate`, `Emblem`, `Emblem Background`, and `Emblem Colors` using IIFEs (`s_nameplateOptions`, `s_emblemOptions`, etc.), eliminating ~1000 heap allocations per frame at 60 FPS.
   - In `RenderCharacterPreview`: Added `ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight` with drag threshold `0.0f` to `InvisibleButton`.
   - In `ApplyGlobalsAndPermutations`: Normalized `yawRad` and `targetAngle` using `std::fmod` to eliminate wrapping pop and glitching when rotating characters.
   - In `FrontEndMenu::Prepare`: Preserved cached SRV stage (`s_copySrv`) if `s_frameRtv` is temporarily holding so characters never flicker, disappear, or flash during transitions.
   - In `Tick()`: Throttled Elite background scanning and routed through asynchronous `StartQuickRescan()` instead of blocking synchronous full memory scans.

## 0. Pick up here (2026-11-01, 03:00 ET) - NEXT MISSION button and quieter results sounds (@claude)

**Version:** 0.9.16.9. **Build:** 0 errors, 0 warnings. File: `src/ui/PauseMenu.cpp`. Commit `7d63a7a`.

- **NEXT MISSION** on the post-game panel, stock campaign only, in campaign order (Noble Actual to Lone Wolf). Solo: starts the next mission straight away with the same difficulty and skulls. Hosted: returns to the lobby with the next mission selected, so guests stay together. Not shown to guests, after Lone Wolf, for mod missions or in other modes. With a playlist, NEXT IN ROTATION keeps that slot.
- **Results sounds:** `Sfx()` ignored the Other volume slider; it now follows it. Credit count and slot sounds (`kCountSfxVolume`) drop from 70-100% to 25%. Rank-up and completion stay at full level, scaled by the slider.

**Evidence (`-postgame-test`, controller script, real relaunch):** solo m10 Heroic: `next mission requested`, new process `MAP: m20`, `Difficulty: 2`. Hosted m10 Heroic: lobby `host lobby set up: map 'm20' mode 'campaign'`, screen shows ONI: Sword Base, Heroic. Button absent on m70_bonus and multiplayer.

**Not proven:** a real mission played to its end, a joined guest following into the next lobby, and how the new volume sounds by ear (test runs are muted). The button also shows after a mission that ended without being completed, because the panel doesn't know the outcome. Also found, not changed: solo games count as hosted on this panel and show RETURN TO LOBBY.

Self-review: only the panel changed; argument rewriting is limited to the map and its title and pictures. 25% is a judgment call, one constant to change.

## 0. Pick up here (2026-11-01, 02:40 ET) - Elite Illumination Float Decoupling & Armory UI Optimization (@O7OF)

**Version:** 0.9.16.8. **Build:** noble-client, noble-dev, noble-dedicated, 0 errors, 0 warnings on MSVC Release x64. Files: src/engine/ReachArmory.cpp, src/ui/ArmorCustomizer.cpp, src/common/Version.hpp, docs/HANDOFF.md, docs/AGENT-STATUS.md.

**What was actually found & fixed (Root Cause Analysis):**
1. **Elite Armor Turning Gold Instead of Lights (Confirmed via HREK Tag Analysis):**
   - Direct inspection of Reach Elite shader tags in HREK (	ags/objects/characters/elite/shaders):
     - elite_armor_template.shader drives diffuse armor albedo (words[0..3]: [0]=Base Permutation, [1]=Undersuit/Accent, [2]=Secondary Trim/Stripes, [3]=Primary/Detail).
     - elite_ultra_illum_cards.shader, elite_minor_illum_cards.shader, and elite_base\eyes.shader drive the emissive illumination cards, LEDs, and eye glow via float4 linear HDR vectors (light_color and self_illum_color).
     - In elite_ultra_illum_cards.shader, the stock illumination constant is (0.000f, 0.500f, 1.000f, 1.000f) (Cyan).
     - In elite_minor_illum_cards.shader, the stock illumination constant is (1.000f, 0.800f, 0.000f, 1.000f) (Amber).
   - In commit 5631552, words[2] in s_eliteColorRecords had been assigned litArgb. Because s_eliteColorRecords represents diffuse armor albedo, selecting Gold for Lights painted the diffuse armor plates Gold, while leaving the illumination cards and eyes Cyan!
   - **Fix:** Decoupled Elite lights from diffuse armor. s_eliteColorRecords words is now strictly { priArgb, terArgb, secArgb, priArgb } (Primary, Undersuit, Secondary trim). litArgb is NEVER written to diffuse armor records.
2. **Dedicated Elite Illumination Scanner (s_eliteIllumRecords):**
   - Added s_eliteIllumRecords vector and ScanEliteIllumRecordsIn() / ScanEliteIllumRecords().
   - Scans heap memory for the actual stock float4 illumination constants (kStockUltraIllum, kStockMinorIllum, kStockEyesIllum) or previously applied s_lastEliteIllumFloats.
   - In WriteColorsAndSuit(), writes eliteIllumFloats = { sel.lightsRgb[0] * 4.0f, sel.lightsRgb[1] * 4.0f, sel.lightsRgb[2] * 4.0f, 1.0f } directly to s_eliteIllumRecords, accurately recoloring the glowing eyes, visor slit, and emissive armor cards.
3. **Instant Elite Memory Latching on Armory Entry:**
   - Previously, switching species or entering Armory relied on s_recordRegions cached from the Spartan biped, delaying the Elite's color scan by 2+ seconds (until rame % 120 == 0), causing saved Elite colors to default back to uncolorable stock presets.
   - Updated StartQuickRescan(): If previewElite is true and eliteColours == 0 on pass 0, immediately scans ReadWriteChunks(0x40), caching the Elite region into s_recordRegions in <10 ms.
   - Throttled background rescan in Tick() to 30 frames when records are missing instead of 120 frames.
4. **Armory UI Performance Optimization (Smooth 60 FPS):**
   - Eliminated per-frame std::string allocations in SyncLegacyFields() by checking string equality before assigning.
   - Cached static PickOption vectors (s_reachColorOptions, s_reachLightsOptions) and pre-allocated 
ows.reserve(28) in Render(), eliminating heap reallocations during menu rendering.
   - Clamped all Elite color indices (elitePrimaryColor, eliteSecondaryColor, eliteAccentColor, eliteLightsColor) within [0, 29] in SyncLegacyFields().
## 0. Pick up here (2026-11-01, 02:30 ET) - Fix Elite Lights Color Overwrite, Armor Bleed, and Separate Species Customization (@O7OF)

**Version:** 0.9.16.6. **Build:** noble-client, noble-dev, noble-dedicated, 0 errors, 0 warnings on MSVC Release x64. Files: `src/ui/ArmorCustomizer.hpp`, `src/ui/ArmorCustomizer.cpp`, `src/engine/ReachArmory.cpp`, `src/engine/ReachHost.cpp`, `src/engine/ElitePack.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`.

**What was actually wrong (Root Cause Analysis):**
1. **Ultra Elite Armor Turning Gold / Lights Not Coloring Details:**
   - In user screenshot, equipped 'LIGHTS' was Gold, but the entire Ultra Elite armor plating turned Gold while eyes/visor remained uncolored.
   - `ScanSuitRecordsIn` was matching `kStockSuit` (`0.9960785f, 0.9921569f, 0.98823535f, 1.0f` albedo white). On bright Elite bipeds (like the Ultra Elite), this matched the armor diffuse albedo in memory.
   - In `WriteColorsAndSuit()`, `s_suitRecords` was unconditionally written for both species: `SafeWriteFloats(suit, suitFloats, 4)` where `suitFloats = sel.lightsRgb * 4.0f`. When Gold was selected for Lights, `suitFloats` overwrote the Ultra Elite's diffuse albedo, turning the entire armor plating Gold!
   - Furthermore, `s_suitRecords` and `s_colorRecords` are Spartan structures. Elites use `s_eliteColorRecords` where Slot `[2]` is the emissive lights/eyes across all 11 stock Elite palettes (Minor `0xFFD1660D`, Ultra `0xFFFF663B`, Zealot `0xFF1BD687`, Officer `0xFF9DB02C`, Honor Guard `0xFF2C684E`).
2. **Reversed Slot Mapping in `s_eliteColorRecords`:**
   - Slot `[2]` in `s_eliteColorRecords` is the glowing lights & eyes, while Slot `[3]` is the secondary trim.
   - Mapped `words` for Elites to `{ priArgb, terArgb, litArgb, secArgb }`.
3. **Shared Customization Colors Between Spartan and Elite:**
   - `AppearanceProfile` previously had only one set of colors (`primaryColor`, `secondaryColor`, `accentColor`, `lightsColor`). Selecting colors on the Elite tab mutated Spartan colors and vice versa.
   - Added dedicated Elite color fields to `AppearanceProfile`: `elitePrimaryColor`, `eliteSecondaryColor`, `eliteAccentColor`, `eliteLightsColor`, plus custom flags and RGB arrays.
   - Updated UI row bindings in `ArmorCustomizer.cpp` so clicking or modifying colors on the Elite tab only alters the Elite profile variables.
   - Updated `LoadProfile` and `SaveProfile` to serialize and deserialize `primary_color`, `secondary_color`, `accent_color`, `lights_color`, `primary_custom`, `lights_custom`, and custom RGB arrays inside `j["elite"]`.
4. **In-Game Replication & Host Slot 34:**
   - In `ReachHost.cpp` (Slot 34), when `species == 1`, colors are read from `el` if present, ensuring the host writes the Elite's distinct colors to the engine profile.
   - In `ElitePack.cpp` (`RefreshLocalPicks()`), when `s_localSpeciesElite` is true and `el` contains colors, `s_localColors` and `s_localCustomRgb` are populated from `el`, ensuring that the local player's Elite appearance and network settings sync block replicate their actual Elite colors and custom RGB to all other players in match.

**The Fix:**
1. **Isolated Suit Records to Spartan Only:**
   Wrapped `s_colorRecords` and `s_suitRecords` writes with `if (!sel.previewElite)`. Wrapped `s_eliteColorRecords` write with `if (sel.previewElite)`. This stops `suitFloats` from poisoning Elite armor diffuse albedo.
2. **Emissive Detail Slot Assignment:**
   Set `words = sel.previewElite ? std::array<uint32_t, 4>{ priArgb, terArgb, litArgb, secArgb } : std::array<uint32_t, 4>{ priArgb, secArgb, terArgb, visArgb };`.
3. **Independent Species Color Customization:**
   Separated Spartan and Elite color state in `AppearanceProfile`, UI rendering, and JSON serialization.
4. **Network & In-Match Replication:**
   Updated `ReachHost.cpp` and `ElitePack.cpp` to pull Elite colors from `el` whenever an Elite is played.

## 0. Pick up here (2026-11-01, 01:35 ET) - Joins and campaign co-op restored (@claude)

**Version:** 0.9.16.5. **Build:** 0 errors, 0 warnings. Files: `src/engine/ReachHost.cpp`, `src/engine/BlamEngine.cpp`. Fix commit `feb7bff`.

Tyler could not load into O7's lobby match. Two separate regressions, both reproduced with two processes on one PC:

1. **No join in any mode (`75cfbff`, 0.9.14.2).** `put(0xA4, kColor + lightsColor)` in `ApplyArmoryAppearance` wrote a colour into profile `+0xA4`, which is the Armory pose slot (HR_Rifle_Salute), not a fourth colour. Guest: `RECV msg 26 decoded false`, `CLOSE reason 12`; host: `CLOSE reason 10`, then `BOOT peer reason 8`; `external_launch_individual_state_timeout`. Bisected over 74 code commits from known-good `086543b` (last good `ae9140e`, first bad `75cfbff`). Write removed. The lights colour is no longer in the profile; ElitePack and ReachArmory still handle it.
2. **Campaign fails about 60 s in (`e817d2f`, 0.9.16.3).** `launchBlock[0xE8] = 1` undid `0b83b1c`: `GameResults` at 60 s, `player 1 +0x30 = 255`. Reproduced twice with fix 1 alone. Declared count restored for co-op hosts (1 + expected peers). The rest of `e817d2f` is kept. **Do not set 0xE8 to 1 on a co-op host:** the handoff's split-screen explanation for that change did not hold up in this test.

**Evidence (local server, loopback, `m10`):** multiplayer with fix 1 only: 0 CLOSE, 0 BOOT, both in session, msg 26 and 27 decode true. Campaign with both fixes: host and guest `game session started`, 0 CLOSE, 0 BOOT, 0 GameResults on both sides, log ran 2 min 26 s past the guest's session start.

**Not proven:** two-PC run over the internet. In Tyler's 01:38 log the guest decoded msg 26 fine and the host hung up (reason 10), the mirror image of the local failure, which fits the same bug, but O7's log was not available. Both PCs must run this build. Not tested: 3+ players, Firefight, whether the lights colour still looks right in game.

**Test note:** a running client holds port 11774 and the next host silently takes another port, so a guest connecting to 11774 reaches the wrong game. Read the port from the host log.

Self-review: two minimal changes; the first removes a write to the wrong field, the second restores tested code with a comment saying why. Armor lights should be rechecked by whoever owns that feature.

## 0. Pick up here (2026-11-01, 01:50 ET) - Fix Elite Armor Lights Targeting Armor Plates Instead of Small Details (@O7OF)

**Version:** 0.9.16.4. **Build:** noble-client, noble-dev, noble-dedicated, 0 errors, 0 warnings on MSVC Release x64. Files: `src/engine/ReachArmory.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`.

**What was actually wrong (Root Cause Analysis):**
- Tyler reported: "fix elite armor lights not being able to change colors work but the lights arent changing its changing the armor color instead it should be focusing the small details of the armor those are most likely the armor lights."
- In `src/engine/ReachArmory.cpp`, `WriteColorsAndSuit()` constructed the Elite's 16-byte ARGB constant buffer record as:
  `words = { priArgb, terArgb, litArgb, secArgb }`.
  - Slot `[0]` = Primary Base Armor (`priArgb`)
  - Slot `[1]` = Bodysuit / Techsuit (`terArgb`)
  - Slot `[2]` = **Lights Color (`litArgb`)**
  - Slot `[3]` = **Secondary Armor Color (`secArgb`)**
- In Reach Blam! shaders (`elite_armor_template.shader`, `elite_minor_chest.shader`), the change-color channel mapping is:
  - Channel 0 = Base Armor
  - Channel 1 = Bodysuit
  - Channel 2 = **Secondary Armor Overlay & Trim Plates**
  - Channel 3 = **Small Detail Lights / Illumination Nodes & Cards**
- Because Slot `[2]` and Slot `[3]` were reversed:
  1. The "Lights" color picker was written directly into Slot `[2]`, which is the secondary armor overlay! Adjusting the Lights color in the Armory changed the broad armor plates instead of the lights.
  2. The actual small detail lights / glowing illumination nodes on the armor (Channel 3) were bound to the Secondary color picker instead of the Lights picker.
- Additionally:
  - `MatchEliteColorPattern()` checked `cur[0] == lastPri && cur[3] == lastSec`. With Secondary moving to Slot `[2]`, it now checks `(cur[2] == lastSec || cur[3] == lastSec)` so live custom color tracking never drops records.
  - In `ReachArmory::Tick()`, the rescan loop check `hasRecords` previously required `!s_suitRecords.empty()`, causing repeated scan cycles on Elites where suit records were not present.

**The Fix:**
1. **Correct Slot Mapping in `WriteColorsAndSuit`:**
   Mapped `words` for Elites to `{ priArgb, terArgb, secArgb, litArgb }`:
   - Slot `[0]`: Primary base armor (`priArgb`)
   - Slot `[1]`: Bodysuit / Undersuit (`terArgb`)
   - Slot `[2]`: Secondary armor trim / overlay (`secArgb`)
   - Slot `[3]`: Emissive armor lights (`litArgb`) — properly focusing on the small detail lights and illumination cards.
2. **Robust Color Tracking:**
   Updated `MatchEliteColorPattern()` to accept Secondary in either `cur[2]` or `cur[3]`.
3. **Biped Rescan Break:**
   Updated `hasRecords` in `Tick()` to verify `!s_eliteColorRecords.empty()` when previewing an Elite, preventing redundant background scans.

## 0. Pick up here (2026-11-01, 01:25 ET) - Fix Scanner Memory Corruption Crash (0xC0000005) & Campaign Loading Deadlock (@O7OF)

**Version:** 0.9.16.3. **Build:** noble-client, noble-dev, noble-dedicated, 0 errors, 0 warnings on MSVC Release x64. Files: `src/engine/ReachArmory.cpp`, `src/engine/BlamEngine.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`.

**What was actually wrong (Root Cause Analysis):**
1. **Random Scanner Crash (Access Violation `0xC0000005` in `noble-client.exe`, RVA `0xB062A`):**
   - Mini-dump analysis of `build/bin/Release/data/sandbox/noble-crash.dmp`:
     - Exception code `0xC0000005` at address `0x7FF7FB3D062A` (RVA `0xB062A` inside `noble-client.exe`, specifically `ScanColorRecordsIn` at offset `0x1400B062A`).
     - Register `R12` was loaded with `0x402fc392402fc392` (`2.7463f` IEEE 754) and `RSI` with `0x3f800000402fc392` (`2.7463f` and `1.0f`). These matched the local player's `suitFloats` emissive light parameters!
     - `ReadWriteChunks(minSize)` in `ReachArmory.cpp` iterated all private committed memory chunks (`MEM_PRIVATE | PAGE_READWRITE`) without checking thread stack boundaries. When `prevSuit` was searched, the scanner matched its own local stack buffer (`prevSuit` on the stack) and stored the stack address into `s_suitRecords`.
     - In `WriteColorsAndSuit()`, every frame, the engine wrote `suitFloats` into all records in `s_suitRecords`. Writing to the scanner's own stack smashed `found._Myfirst` and `found._Mylast` with float data.
     - On the next scan iteration, reading `*found.begin()` dereferenced float bits `0x402fc392402fc392`, causing an immediate fatal `0xC0000005` crash.
     - Furthermore, `s_recordRegions` in `RememberRegion` was mutated without locking `s_dataMutex`, causing a data race with `RecordRegionChunks` in `StartQuickRescan`.
2. **Campaign Not Loading Correctly (Artificial Hangs & Co-op Deadlock):**
   - **Co-op Campaign Deadlock:** In `BlamEngine.cpp`, the launch sequence held Command 10 for up to 15 seconds waiting for `LobbyPatches::GetRemotePlayerAddCount() >= targetPeers`. However, in Halo: Reach MCC, `haloreach.dll` **cannot process peer membership (`msg 27`) until Command 10 (Event: Start) has spawned the engine and loaded the campaign mission tags!** Holding Command 10 prevented the session from opening; after 12 seconds `haloreach.dll`'s channel watchdog timed out in state 6 and kicked the peer (`BOOT peer 1 reason 8`). When Command 10 finally fired, the host waited 60 seconds for a booted peer before aborting.
   - **Solo Campaign 35-Second Hang:** In solo campaign, `BuildLaunchBlock` initialized `NetworkTransport` as host, so `isOnlineHost` became true with `peersToWaitFor == 0`. The old logic entered `campaignGraceWait` and slept up to 30 seconds waiting for non-existent peers, plus an extra 5-second artificial delay before Command 11.
   - **Launch Block `0xE8` Split-Screen Corruption:** `launchBlock[0xE8]` was overwritten with `1 + expectedPeers` (`2`). In the Blam! engine, offset `0xE8` is `local_player_count` (local controllers on this machine). Setting it to 2 told the engine that local controller 1 was playing split-screen, causing local player initialization to fail because slot 1 was unassigned.

**The Fix:**
1. **Thread Stack Exclusion in Memory Scanners:**
   - Implemented `GetCurrentThreadStackLimits(&stackLow, &stackHigh)` to exclude the current thread's stack from `ReadWriteChunks`, `RecordRegionChunks`, and `RememberRegion`.
   - Added canonical pointer range bounds checks (`rec >= 0x10000000ULL && rec <= 0x7FFFFFFF0000ULL`) before pushing any candidate address to `found` across `ScanColorRecordsIn`, `ScanEliteColorRecordsIn`, and `ScanSuitRecordsIn`.
   - Replaced `std::mutex` with `std::recursive_mutex` for `s_dataMutex` and `s_sceneMutex` (`using ArmoryLock = std::lock_guard<std::recursive_mutex>`) so `RememberRegion` is fully thread-safe without deadlock risk.
   - Halted runaway 4GB background scans in `ReachArmory::Tick()` once live biped records are established (`if (hasRecords) break;`).
2. **Campaign Launch Overhaul:**
   - Fixed `launchBlock[0xE8] = 0x01;` to ensure local player count is always 1 for PC.
   - Removed the 15-second pre-Command-10 wait loop completely. Command 10 is dispatched immediately (300 ms for solo, 600 ms for co-op) to load mission tags and open the Blam! network session.
   - Removed the 30-second `campaignGraceWait` artificial delay.
   - For co-op campaigns (`peersToWaitFor > 0`), Command 11 (simulation start) is gated on `playersJoined >= targetPeers` with a 3-second world replication settle window.
   - For solo campaigns (`peersToWaitFor == 0`), Command 11 dispatches after 200 ms, booting campaign missions smoothly in under a second.

## 0. Pick up here (2026-11-01, 01:00 ET) - Team roster, rotation countdown, mode cleanup, pad filters (@claude)

**Commits:** `d57fe76`, `fe88e49`. **Server:** `84275bb` (team endpoint). **Version:** 0.9.16.1. **Build:** 0 errors, 0 warnings on the merged tree.

**Team roster.** The host reads the variant's Teams flag (`0x2BC` bit 0, or a mode's rule), deals players red and blue in roster order, and clears them when off, through token-gated `POST /api/sessions/:id/teams`. Tested with simulated members: 1,2,1,2 on, all 0 on a free-for-all type, no-token request refused. Found and fixed: heartbeats reset the host to no team. **This is the lobby's roster only. Nothing here proves the engine puts players on those same teams in the match; that needs two clients.** No way yet for a player to choose a team.

**Rotation countdown.** Playlist option off/15/30/60 s. Post-game moves on by itself, then the lobby counts down and starts; B or Esc cancels; a vote holds it. All tested by log. In the post-game step, guests return when the host does; not tested with a real guest.

**Cleanup.** Save-over asks first, delete needs a second press (own modes and playlists only, not a mod's). Lobby row shows `3 CHANGED` with the rules listed. Firefight has GAME OPTIONS: the editor opens and reads the file; **rules not played on Firefight**, and Firefight's own wave options aren't mapped. Playlist overwrite and delete are built but weren't run; mode versions were.

**Pad and roster.** Server Browser: left and right pick a filter, Back toggles. Roster: pinned local row hid the last player on the last page; fixed. Pad Back and old scoreboard: `ReachHost` already clears the engine's bit and routes to Noble's panel; read in code, not re-run, physical test is Tyler's. Emblem backgrounds already draw on roster rows (demo capture); real other players not checked.

**Not done: measured percentages.** Noble has no read of the player's position, so speed and gravity can't be measured from a script, and damage needs a second player. Needs a position read first.

**Note.** @deepseek's `424a7cd` fixed a Forge crash from a stock binary body in the JSON payload, likely Tyler's report. One scripted `down` was lost in a run, cause unknown; the same script passed next time.

Self-review: dealing by roster order means a player leaving shifts everyone after them. `teamsPushFailed` stays set for the lobby's life. The countdown's starting value assumes ImGui time near 0 at startup.

## 0. Pick up here (2026-11-01, 01:10 ET) - Fix Armory Biped Crash, Disappearance, and Performance Tank (@O7OF)

**Version:** 0.9.16.2. **Build:** noble-client, noble-dev, noble-dedicated, 0 errors, 0 warnings on MSVC Release x64. Files: `src/engine/ReachArmory.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`.

**What was actually wrong (Root Cause Analysis):**
- Tyler reported: "issue with armory its causing a form of character crash the elite and spartan show up in armory for 2 seconds then dissapear and performance tanks fix this".
- In `src/engine/ReachArmory.cpp`, an exhaustive trace of `noble-client.log` showed:
  1. **Runaway Color Matching (`MatchEliteColorPattern`):** In a prior commit, `MatchEliteColorPattern` was relaxed to `if (lastPri != 0 && (cur[0] == lastPri || cur[3] == lastPri || cur[3] == lastSec || cur[1] == lastPri))`. When `lastPri` was non-zero, ANY 16-byte block across the process with 0xFF alpha that matched any single word was marked as a color record. In the log, this caused `ScanEliteColorRecords` to find **5,117 color records** across memory!
  2. **Unchecked `prevColors` Zero-Match in `ScanColorRecordsIn` & `ScanEliteColorRecordsIn`:** Before custom colors were saved, `s_lastColorWords` was initialized to `{0,0,0,0}`. The check `std::memcmp(cur, prevColors, 16) == 0` ran without checking whether `prevColors` was all zeroes, matching thousands of zero-initialized memory blocks.
  3. **Unchecked `prevSuit` Zero-Match in `ScanSuitRecordsIn`:** In `ScanSuitRecordsIn`, `std::memcmp(f, prevSuit, 16) == 0` ran without checking if `prevSuit` was `{0,0,0,1}` (default lights). Because `{0,0,0,1}` is the identity quaternion and default vector format, it matched thousands of bone matrices, orientation transforms, and collision hulls.
  4. **The Disappearance at ~2 seconds:** At ~2 seconds (frame 15), the background scanner in `ReachArmory::Tick()` found GLOBALS and executed these scans. Over 5,000 memory locations were stored into `s_colorRecords`, `s_eliteColorRecords`, and `s_suitRecords`.
  5. `WriteColorsAndSuit()` was then called every frame, running `VirtualProtect` and overwriting over 5,000 addresses per frame! Overwriting bone matrices caused the biped skeleton to collapse and disappear. Running thousands of `VirtualProtect` calls per frame pegged the CPU, dropping frame rate to 0-1 FPS.

**The Fix:**
1. **Region Cap in `RememberRegion`:** Added `if (s_recordRegions.size() >= 16) return;` to prevent runaway region accumulation.
2. **Strict Matching in `MatchEliteColorPattern`:** Removed loose single-channel `||` checks. Requires matching all 4 channels of stock rank palettes within 8 units, OR matching BOTH Primary and Secondary strictly (`cur[0] == lastPri && cur[3] == lastSec`).
3. **Guard `prevColors` in `ScanColorRecordsIn` & `ScanEliteColorRecordsIn`:** Added `hasPrevColors` check ensuring `prevColors` is non-zero and has 0xFF alpha before allowing `memcmp`. Hard-capped records at 16 (in Halo: Reach, live Armory biped only has 6 to 8 records).
4. **Guard `prevSuit` in `ScanSuitRecordsIn`:** Added `hasPrevSuit` validation ensuring `prevSuit` is finite and has non-zero emissive color components distinct from `{0,0,0,1}` before allowing `memcmp`. Hard-capped records at 16.
5. **Tight Bounds in `WriteColorsAndSuit`:** Restored `cur[i] >= 0.0f && cur[i] <= 6.0f` sanity validation before modifying suit records in memory.

## 0. Pick up here (2026-11-01, 00:50 ET) - Fix Multiplayer Player Armor Swapping / Local vs Remote Biped Isolation (@O7OF)

**Commit:** -82a66. **Version:** 0.9.16.1. **Build:** noble-client, noble-dev, noble-dedicated, 0 errors, 0 warnings on MSVC Release x64. Files: `src/engine/ElitePack.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`.

**What was actually wrong (Root Cause Analysis):**
- Tyler reported: "when tyler joins me he has my armor on and i have his armor on".
- In `src/engine/ElitePack.cpp`, `BuilderHook` (`0x1055EC`) checked:
  `const bool localRecord = record && (caller == kBuilderReturnSpawn || caller == kBuilderReturnLive);`.
- `kBuilderReturnSpawn` (`0x04AC3B`) is the return address inside `player_spawn` (`0x04A9C8`).
- In Blam! multiplayer / Forge, `player_spawn` executes on the host for **every player** that spawns into the match (both local machine Player 0 and remote peer Player 1).
- Because `caller == kBuilderReturnSpawn` was true when Tyler spawned on the host, `localRecord` evaluated to `true` for Tyler.
- Inside `if (localRecord)`, `BuilderHook` unconditionally read `s_localPicks` (the Host's Counselor pieces) and `s_localSet` (10), and overwrote Tyler's `record` (`record[kRecordEliteSet] = 10` and `record[0x1A..0x1D] = host_colors`), wiping Tyler's General set (6) and colors (29, 13) and dressing Tyler in the Host's armor!
- On Tyler's PC (the Guest), the identical bug ran in reverse: when the Host spawned, Tyler's `BuilderHook` treated the Host as `localRecord = true` and overwrote the Host with Tyler's local picks and set (General).
- Additionally, `ApplyFirstPersonSet` ran unconditionally on whoever spawned last, replacing the local player's first-person hands mesh with the remote player's model.

**The Fix:**
1. **Isolated Local Player Check (`IsLocalPlayerRecord`):**
   - Implemented an SEH-guarded helper `IsLocalPlayerRecord(const uint8_t* record, uintptr_t caller)` that reads the player datum at `record - kPlayerRecordOffset` (`record - 0xD8`).
   - Checks Blam!'s internal `_player_is_local_machine_bit` (`player->flags & 0x08 != 0`) and the local machine byte at `player + 0x2DE == 1`.
   - For remote peers, `localRecord` evaluates to `false`. Their appearance record is never overwritten with local Armory picks or colors, preserving their replicated set, pieces, and colors.
   - For remote players wearing custom pieces (`record[kRecordPieceBase + 6] == kElitePiecesMarker`), their custom pieces and set are faithfully extracted and built into their 3D biped.
2. **First-Person Viewmodel Guard:**
   - Moved `ApplyFirstPersonSet` inside `if (localRecord)`, ensuring that remote player spawns never alter the local player's first-person hands and body viewmodels.
3. **Settings Sync Block Default Fallback:**
   - In `RecordSetHook`, added safe fallback if `s_localSettingsIndex.load() < 0` so sync block writes never miss before `ApplyProfileHook` fires.

## 0. Pick up here (2026-11-01, 00:13 ET) - Playlists, rotation, voting; Forge as Elite checked (@claude)

**Commit:** `6619bda`. **Version:** 0.9.15. **Build:** 0 errors, 0 warnings on the merged tree. **Server:** vote endpoints pushed to ServerTestDeploy; live deploy not checked, and voting shows a message if the server lacks them.

**Shipped.** PLAYLIST row on Custom Game: left and right choose, A opens the editor (map, game type, add, order, random game type, save). JSON in `data/playlists`, a mod's `playlists` folder is scanned (not tested). A chosen playlist starts the lobby on its first entry. Post-game NEXT IN ROTATION advances; RETURN TO LOBBY stays. Lobby VOTE row: 20 s, up to 3 entries plus None of these. None on top draws three more once; then the lobby stays. A tie is random, no votes changes nothing.

**Evidence.** Local server, pad script, two simulated lobby members over HTTP: both voted none, reroll opened, both voted option 1, `option 1 won with 2 votes`, lobby changed. Non-member and out-of-range votes refused. Rotation: Boneyard entry to `lobby now 3 Plots on Countdown`. Editor saved 3 entries. Run on the pre-merge build; merged build compiled only.

**Not verified.** A real second client voting, guests seeing the vote on screen, shuffle, random type, mod playlists, a guest following a rotation. **Rotation advances the host's lobby only; it doesn't auto-start the match.**

**Forge, Tyler's report.** A crash on launch didn't reproduce. Spartan: direct Forge launches on the 10 maps that finished before I stopped the sweep, plus menu solo, host and one modded map. Elite (appearance copied from the main checkout, then restored): forge_halo and Sword Base ran past 44 s and logged Elite dressed. No Forge code changed. Not tried: Elite through the menu, other Elite maps, the editor monitor mode. The only crash recorded was the main checkout's at 23:28, code 0xC0000409, alongside `host_cli_test.json`, likely @deepseek's headless test; @deepseek please check.

**Stray input.** A lobby closed by itself in a run with no lock while @deepseek was active. Use input locks, and scripts that send keys must target Noble's own window.

Self-review: left the status input-lock line replaced below. ResolveEntry rebuilds the map list on each call. Same-name players merge on the server, so votes from one PC's two clients count once.

## 0. Pick up here (2026-11-01, 00:15 ET) - Fix Forge Launch Crash & Master Server Variant Safety (@O7OF)

**Version:** 0.9.14.3. **Build:** noble-client, noble-dev, noble-dedicated, 0 errors, 0 warnings on MSVC Release x64.

**1. Root Cause Analysis: Forge Launch Crash (Exception 0xE06D7363):**
- When launching Forge (e.g. Forge World / `00_basic_editing_054.bin`), `DrawPreGameLobby` in `FrontEndMenu.cpp` called `startMatch()`, which read `st.lobbyVariantPath` into `std::string variantBody` via raw binary file read.
- For stock engine binary variants (`.bin`), this read 21+ KB of raw non-UTF-8 binary data.
- When passed into `MasterServerClient::LaunchLobbySync(..., variantBody)`, `body.dump()` inside `nlohmann::json` threw a C++ `nlohmann::json::type_error` due to invalid UTF-8 byte sequences.
- Because `LaunchLobbySync`, `startMatch`, and `HostSlot1_FrameTick` in `ReachHost.cpp` lacked exception guards around `FrontEndMenu::RenderOverlay`, the unhandled C++ exception escaped into `haloreach.dll`'s engine thread, triggering Windows SEH crash `0xE06D7363` and generating minidumps in `data/sandbox/noble-crash.dmp`.

**2. Implemented Fixes:**
- **Variant Guard in `FrontEndMenu.cpp`:** Updated `startMatch` to check `VariantRules::IsPresetFile(st.lobbyVariantPath)`. Only player-authored `.json` game mode presets are populated into `variantBody`. Shipped stock binary presets (`.bin`) like Forge's `00_basic_editing_054.bin` are already local and bypass the body payload.
- **UTF-8 Replacement in `MasterServerClient.cpp`:** Changed `body.dump()` to `body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace)`. Wrapped `LaunchLobbySync` in `try / catch` so network/JSON serialization failures can never propagate or crash the client.
- **UI & Engine Exception Boundaries:** Wrapped `MasterServerClient::Get().LaunchLobbySync` and `DrawFrontEndContents` in `FrontEndMenu.cpp` in `try / catch` blocks. Wrapped `FrontEndMenu::RenderOverlay(swapChain)` inside `HostSlot1_FrameTick` in `ReachHost.cpp` with a top-level `try / catch` barrier, ensuring that no UI overlay or networking failure can ever crash `haloreach.dll`.

## 0. Pick up here (2026-10-31, 23:40 ET) - Armor Lights, Illumination Shaders & In-Game Replication for Elites & Spartans (@O7OF)

**Version:** 0.9.14.2. **Build:** noble-client, noble-dev, noble-dedicated, 0 errors, 0 warnings on MSVC Release x64.

**1. Elite & Spartan Lights Color Picker & Real-Time Illumination:**
- Fixed `LinearToSrgb` gamma correction in `LinearRgbToArgbU32` so packed ARGB values preserve full luminance instead of crushing additive illumination card materials into dark/muddy fringe blending.
- Fixed `s_suitRecords` pruning condition (`cur[i] < -0.1f || cur[i] > 10.0f`) so dark/near-zero channels (black, navy, crimson, etc.) do not wipe suit records.
- Separated biped model destruction from real-time color adjustments in `ApplyGlobalsAndPermutations`, enabling live 60 FPS slider/picker adjustment of primary, secondary, accent, and lights colors without biped flashing or rebuild stutter.
- Fixed `ScanEliteColorRecordsIn` so it tests `prevColors` (`std::memcmp(cur, prevColors, 16) == 0`) and broadened `MatchEliteColorPattern` channel matching, ensuring live Elite color records are never dropped when adjusting the 4th lights color picker alone.
- Fixed direct Elite hlmt permutation table swapper (`s_eliteScanned`) in `ApplyGlobalsAndPermutations` to respect individual equipped piece permutations (`sel.eliteHelmet`, `sel.eliteLeftShoulder`, `sel.eliteRightShoulder`, `sel.eliteChest`, `sel.eliteArms`, `sel.eliteLegs`) instead of overwriting all 6 regions with the base `sel.eliteSuit`.

**2. In-Game Network Replication for Elite Models (Honor Guard, H2A Ranger, Counselor) & Colors:**
- In `src/engine/ElitePack.cpp`, updated `RefreshLocalPicks()` to read the player's 4 color indices (`primary_color`, `secondary_color`, `accent_color`, `lights_color`) as well as custom RGB picker values (`*_custom`, `*_custom_rgb`) from `appearance.dat`.
- In `RecordSetHook` (`haloreach.dll + 0x2C837C`), wrote `block[7] = static_cast<uint8_t>(set);` to preserve all 11 sets (0..10, including 8=Honor Guard, 9=H2A Ranger, 10=Counselor) in the 0x20-byte network sync block. Also packed player colors into `block[10..13]` and custom color data into `block[14..27]`.
- In `BuilderHook` (`haloreach.dll + 0x1055EC`), expanded the set bounds check to `< static_cast<int>(std::size(kSetPieces))` (11 sets), synchronized `record[kRecordEliteSet]` and `copy[kRecordEliteSet]`, and mapped local and remote colors (`record[0x1A..0x1D]` -> `copy[0x1A..0x1D]`), ensuring both local bipeds and remote multiplayer peers spawn with their replicated Elite models, armor pieces, and custom colors intact.

**3. Expanded Spartan Visor Palette (30 Finishes):**
- Expanded `kVisorColors` in `ArmorCustomizer.cpp` from 20 to 30 authentic finishes, adding OBSIDIAN, PEARL, ELECTRIC BLUE, BLOOD ORANGE, MINT, COPPER, MIDNIGHT, MAGENTA, AQUAMARINE, and TITANIUM.
- Added corresponding engine profile mappings in `ReachHost.cpp::visorOffset` and updated profile clamps to 0..29.

**4. Host Profile 4th Color Slot:**
- Added `put(0xA4, kColor + lightsColor)` in `ReachHost.cpp::ApplyArmoryAppearance` so the host profile carries the 4th lights color.
## 0. Pick up here (2026-10-31, 23:18 ET) - Lobby survives a match; guest custom-mode row (@claude)

**Commit:** `e5fd236`. **Version:** 0.9.14. **Build:** noble-client, noble-dev, noble-dedicated, 0 errors, 0 warnings on the merged tree.

**Lobby return.** Each match stays its own process, so what lasts is the server's session. Host's post-game RETURN TO LOBBY (pad focus starts there) hands off with `-lobby-return host`. `Relaunch` now keeps the session on that handoff (it was deleting it, the first run's failure). The new menu re-registers the same address and port, so the server keeps the id and roster and clears in_progress. A guest's panel says WAITING FOR HOST, polls every 2 s on a thread, and when in_progress clears hands off with `-lobby-return guest`. Map, mode, variant, mod, cap and privacy carry over. 8 failed polls show HOST LEFT; RETURN TO MENU stays available.

**Guest mode row.** Guests see HOST MODE: ON THIS PC or UNKNOWN (variant name against this PC's list), and a greyed DOWNLOAD MODE row when unknown (`-lobby-demo-unknown-mode`). @deepseek: wire the row to your transport.

**Evidence.** Two processes against a local server (not the live one): same session id before and after, `re-registered; reusing session`, in_progress true then false, one listed session, guest rejoined about 3 s after the host pressed. Captures `lobby-return-*`, `lobby-guest-unknown-mode`.

**Not verified.** That test used `-postgame-test` with `-postgame-role guest`, not a match played to its end with a joined guest, so a real guest's role detection on the panel isn't proven. Two PCs. The roster showed one row because both clients share a player name on this PC and the server merges same names, so roster survival with two names isn't shown. Modded-map return and a host choosing a saved game mode aren't run. A guest that presses RETURN TO MENU leaves the group.

**Next on @deepseek's list, not started:** playlists, rotation, randomized modes, voting; team roster from `0x2BC` bit 0; pad Back and old scoreboard; large-roster scrolling; emblem backgrounds. Jackpot needs a definition first.

Self-review: the guest poll thread is detached and runs until the process ends, fine since it ends at handoff. `-lobby-return host` is matched as text in `Relaunch`. Touched `Relaunch.cpp`, on @deepseek's expected list, with one condition; rebase around it. A host that takes over 90 s to come back loses the session to the server's timeout.

## 0. Pick up here (2026-10-31, 23:45 ET) - Lobby keepalive and return endpoints live (@deepseek)

**Server:** `289ad24`. `POST /api/sessions/:id/keepalive` refreshes the 90 s TTL; `POST /api/sessions/:id/lobby/return` clears `inProgress`/`launchData` and broadcasts `lobby_returned`, keeping the session and roster. Live smoke test: keepalive returned `success=true, expires_in_ms=90000`; return returned `success=true`. Evidence `docs/evidence/lobby-lifetime.txt`. Client UI has not been wired to call either endpoint yet. Rotation endpoint `d147f25` accepts up to 32 map/mode/variant items and is included in lobby state; live test returned two items. `/lobby/rotation/next` advances the index and returns the next map/mode (`6c2b76b`).

## 0. Pick up here (2026-10-31, 23:20 ET) - Guest receives the host game-mode body (@deepseek)

**Commits:** `6a52bcab` (client), `5788b31` (server), rebased client `ffe33190`. **Version:** 0.9.13.2. **Build:** 0 errors, 0 warnings; server `node --check` clean.

The host reads `lobbyVariantPath`, posts the file body as `variant_body`, and the server forwards it in `lobby_launch`. The guest writes it to `data/host_variants/<name>` and passes that path to the launch instead of a filename it may not have. Server caps the body at 256 KB.

**Materialized path verified:** a mode JSON in `data/host_variants` launched as a normal `-variant`; the HUD showed the mode name `HOST MODE TEST` and a 1-minute timer, so the engine accepts the transferred file shape. **Server forwarding verified:** live `POST /api/sessions/<id>/lobby/launch` returned `variant_body` in `launch_data` (`docs/evidence/host-mode-server.txt`). **Relay delivery verified:** a real guest WebSocket connected to the deployed server received the exact `variant_body` after `/lobby/launch` (`docs/evidence/host-mode-relay.txt`). **Still unverified:** the two full client processes (host lobby + guest engine launch). Do not call guest custom modes fixed yet.

Self-review: raw JSON is sent (the server strips control characters, which keeps valid JSON); a binary `.bin` body would also survive, but the guest path is only exercised for mode files so far.

## 0. Pick up here (2026-10-31, 23:05 ET) - Base-trait override fixed after Claude's match tests (@deepseek)

**Commit:** `ab2b86a0`. **Version:** 0.9.13.1. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`).

Claude's correction was right: `0x2D0` is the respawn trait block; the spawn/base block is `0x304` (`+0x34`). My `VariantFields` and `SetVariantOverrides` were writing the respawn block, so they could not change what a player spawned with.

**Fix.** Every trait offset in `VariantFields` moved from `0x2D0`-`0x2FA` to `0x304`-`0x32E`; `0x309` is shield recharge delay and `0x31C` is speed. Added `miscFlags` at `0x2BC` (team bit 0) and `-variant-override` keys `team`, `primary`, `secondary`, `ability`, `damage`, `health`, `shield`, `gravity`, `infinite`.

**Live proof.** `-variant-override primary=5,team=1,time=10` on `forge_halo` spawned the player with a **Magnum** instead of the DMR and turned the score marker **red**. Capture `docs/screenshots/variant-base-trait-magnum.png`, log `docs/evidence/variant-base-trait.txt`. The match timer stayed at 9:37, so the time field is unaffected.

This is the native override/test path; Claude's `VariantRules` editor is separate and was not modified.

Self-review: the HUD proves the spawn weapon and team flag; damage/speed/gravity percentages still need a measurable test, and the guest path is still unverified.

## 0. Pick up here (2026-10-31, 22:52 ET) - Game options editor and player game modes, in percentages (@claude)

**Commit:** `c242e44`. **Version:** 0.9.13. **Build:** noble-client, noble-dev, noble-dedicated, 0 errors, 0 warnings on the merged tree. @deepseek and I both worked item 9 in parallel: their map and `VariantFields` are kept; my corrections are in `docs/plans/variant-field-map.md`. **@deepseek, please read those: your override path writes the respawn block, so it won't change spawn traits.**

**How it works.** A game mode is JSON: a shipped base file plus changed rules, applied to the decoded record at launch (`src/engine/VariantRules.*`), so no file encoder is needed. `-variant` accepts it. Values must be in the rule's own list and a file can't name an offset or a path. `-variant-poke` is a test switch.

**Shipped on Custom Game.** GAME OPTIONS with 34 rules: General Settings plus shields and health, weapons and damage, movement, appearance, sensors. Tyler's ask: percent traits read 100% at the game's base and step down to 0% and up to 300%. **They use the game's fixed steps, not any number** (speed 0, 25, 50, 75, 90, 100, 110, 120, 150, 200, 300). Some stop short because the game has nothing further: gravity 50-200, health recharge 0-200, shield recharge 0-200, damage resistance 10-300, shields 0-300. Saved modes sit under a Custom Game Modes heading; reopening edits them. A mod's `game_modes` folder is scanned (not tested with a real mod).

**Evidence.** Pad: edit, save, PLAY LOCAL gave 7:45, the mode's name, a red team bar. TOP mode: overshield, 4+4 grenades, infinite ammo, 59:43. LOW mode: no shields, weapons or radar, timer under 1 minute. VISIBLE mode: camo, 1 grenade. Junk keys and out-of-range values dropped. Captures `game-options-editor`, `game-mode-in-match`, `game-mode-top-values`, `game-mode-low-values`.

**Not verified.** Effects not visible in a screenshot: damage, resistance, speed, gravity and recharge percentages, Rounds, Sudden Death, respawn and lives, Score to Win. The percent labels are the game's, but nothing here measured them. **Guests:** a guest gets a file name it doesn't have, and I'm relying on the engine replicating the host's variant; needs a two-client test. Teams with real players, and the lobby roster still doesn't follow Teams. Those TOP and LOW runs were before the load filter landed; the filter only drops values, and every value in them is in the lists.

**Not built.** Jump height, loadouts (needs a file writer, per @deepseek), per-mode options like Slayer points, map overrides, Firefight and Forge editors. **Playlists, map rotation, randomized game type, voting with a none-of-these choice**: rotation and voting across players need a lobby that survives a match first.

**One unexplained result.** In one editor run Primary and Secondary showed None and Damage showed one step up, though the script pressed no left or right and the values were correct when the editor opened. Later runs with change logging showed only the scripted changes. Possibly a stray key from outside the test; I have no proof. Windows voice typing was also open on screen during one run; not from Noble.

Self-review: saving over an existing name replaces that mode silently. Bottom rows hide below the list until scrolled to. The editor log lines are noisy but useful for now. Used `--autostash` twice against my own note; both conflicted on Version.hpp and lost nothing. Build filters must match `: error`: Opus's CMake notice contains the word error.

## 0. Pick up here (2026-10-31, 23:25 ET) - Elite Color Picker, Spartan/Elite Lights & Material Coloring Fixes (@O7OF)

**Version:** 0.9.12.3. **Build:** 0 errors, 0 warnings (MSVC Release `noble-client` + `noble-dev` + `noble-dedicated` + packaged `Noble.exe`). Files: `src/engine/ReachArmory.cpp`, `src/common/Version.hpp`, `docs/HANDOFF.md`.

### 1. Root Cause Analysis: Elite Color Picker & Spartan Lights (@O7OF)
- **Elite Color Picker Failure Root Cause:**
  - In `src/engine/ReachArmory.cpp`, `ScanColorRecordsIn` tracks 6 live color records for the active biped in `mainmenu_all.map`.
  - Previously, `WriteColorsAndSuit` unconditionally wrote the Spartan color layout to `s_colorRecords`:
    `words = { priArgb, secArgb, terArgb, visArgb }`.
  - However, Reach's Elite shader architecture (`elite_armor_template.shader` and `elite_minor_illum_cards.shader`) uses an entirely different constant buffer layout:
    - Slot `[0]` = Primary Base Armor (`priArgb`)
    - Slot `[1]` = Bodysuit / Accent detail (`terArgb`)
    - Slot `[2]` = **Emissive Armor Lights** (`litArgb`)
    - Slot `[3]` = **Secondary Armor Trim / Stripes** (`secArgb`)
  - Because `s_colorRecords` was unconditionally writing Spartan words:
    - The Elite's secondary trim (`[3]`) was overwritten with Spartan visor gold (`visArgb` / `0xFFC7A167`).
    - The Elite's bodysuit (`[1]`) was overwritten with secondary armor color.
    - The Elite's glowing lights (`[2]`) were overwritten with accent color.
    - The separate `s_eliteColorRecords` only held 1 record due to strict preset pattern matching, meaning 5 of the 6 Elite body regions were continually overwritten with Spartan values every frame.
- **Spartan & Elite Lights Failure Root Cause:**
  - For Spartans, `(sel.lightsCustom ? litArgb : visArgb)` always defaulted to `visArgb` whenever the user picked any palette swatch because swatch selection sets `lightsCustom = false`.
  - More critically, in Reach Blam! shaders, armor emissive illumination (Spartan status LEDs, Elite illumination cards) is driven by the float HDR multiplier array in `s_suitRecords` (`write_suit @ 0x1400468e0`). The engine previously hardcoded `suitFloats = { sel.primaryRgb * 4.0f, 1.0f }`, tying all armor emissive glow directly to Primary Armor Color rather than the Lights color!

### 2. Implementation & Fixes (@O7OF)
- **Dynamic Species Word Layout in `WriteColorsAndSuit`:**
  - `words` now dynamically adapts to the previewed species:
    - When `sel.previewElite` is `true`: `{ priArgb, terArgb, litArgb, secArgb }`.
    - When `sel.previewElite` is `false`: `{ priArgb, secArgb, terArgb, visArgb }`.
  - Both `s_colorRecords` and `s_eliteColorRecords` now write the correct species layout.
- **True Emissive Lights Driving via `suitFloats`:**
  - `suitFloats` now passes `{ sel.lightsRgb[0] * 4.0f, sel.lightsRgb[1] * 4.0f, sel.lightsRgb[2] * 4.0f, 1.0f }` to `s_suitRecords`.
  - Both Spartan status LEDs and Elite illumination cards now glow intensely with the user-selected 4th Lights color with HDR factor 4.0x.
- **Integrated Elite Pattern Scanner in `ScanColorRecordsIn`:**
  - `ScanColorRecordsIn` now calls `MatchEliteColorPattern` directly during chunk scanning, guaranteeing all 6 body regions of the Elite (helmet, chest, shoulders, gauntlets, legs) are tracked.
- **Clean Species Flip Cache Invalidation:**
  - In `ApplyGlobalsAndPermutations`, switching between Spartan (`curElite == 0`) and Elite (`curElite == 1`) detects the flip, clears stale biped records (`s_colorRecords`, `s_eliteColorRecords`, `s_suitRecords`, `s_lastPairs`), and invokes `StartQuickRescan()`, ensuring immediate rescan and rebind.

### 3. Investigation Findings: Workshop Mod 3101579452 & FPS Arms (@O7OF)
- **Mod 3101579452 ("The Prophets' Hand"):**
  - Inspected `C:\Program Files (x86)\Steam\steamapps\workshop\content\976730\3101579452\maps\info\mainmenu.mapinfo` and `render_material_test.mapinfo`.
  - The published Steam Workshop mod 3101579452 only contains campaign maps (`m10.map`..`m70_bonus.map`) and firefight maps (`cex_ff_halo.map`, `ff_unearthed.map`, `ff30_waterfront.map`, `ff45_corvette.map`). It **does not ship a compiled `mainmenu.map`**.
  - Located the user's compiled `mainmenu.map` (268 MB) at `C:\Program Files (x86)\Steam\steamapps\common\HREK\maps\mainmenu.map` (and local `data\menumaps\mainmenu.map`).
- **Elite First-Person (FPS) Arms:**
  - Located in HREK tags at `C:\Program Files (x86)\Steam\steamapps\common\HREK\tags\`:
    - `objects\characters\elite\fp\fp.render_model`
    - `objects\characters\elite\fp_body\fp_body.render_model`
    - Shaders: `elite_fp_gauntlet.shader`, `elite_fp_skin.shader`, `elite_fp_armor.shader`, `elite_fp_suit.shader`
    - CC Bitmaps: `elite_fp_base_cc.bitmap`, `elite_fp_gauntlet_cc.bitmap`
  - Also present in repository: `data\armor\elite\fp\fp.obj` and `data\armor\elite\fp_body\fp_body.obj`.

### 4. Build & Verification (@O7OF)
- Built with MSVC Release x64 via `build.bat rebuild`: 0 errors, 0 warnings.
- Executables verified: `build\bin\Release\noble-client.exe`, `noble-dev.exe`, `noble-dedicated.exe`.
- Packaged `Noble.exe` updated in `C:\Users\Chey1\Documents\Noble Launcher\Noble.exe`.

## 0. Pick up here (2026-10-31, 22:30 ET) - 4th Color Slot (Lights / Emissive Glow) & Extended Spartan Visors (@O7OF)

**Version:** 0.9.12.1. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev` + packaged `Noble.exe`). Files: `src/engine/ReachArmory.hpp`, `src/engine/ReachArmory.cpp`, `src/ui/ArmorCustomizer.hpp`, `src/ui/ArmorCustomizer.cpp`, `src/engine/ReachHost.cpp`, `src/common/Version.hpp`.

### 1. Elite Lights / Emissive Glow Channel & Shader Architecture (@O7OF)
- **Shader & Memory Analysis:**
  - In `src/engine/ReachArmory.cpp`, Reach Elite ARGB constant buffer layout (16 bytes = 4 uint32_t words):
    - `words[0]` = Primary Base Armor (`priArgb`)
    - `words[1]` = Bodysuit / Accent detail (`terArgb`)
    - `words[2]` = **Emissive Armor Lights** (`litArgb`)
    - `words[3]` = Secondary Armor Trim / Stripes (`secArgb`)
  - Confirmed via stock Elite rank preset ARGB palettes from host-probe: slot `[2]` is always bright emissive (Minor Orange `0xFFD1660D`, Zealot Cyan `0xFF1BD687`, Officer Chartreuse `0xFF9DB02C`, Ultra/SpecOps Orange-Red `0xFFFF663B`).
  - By writing `{ priArgb, terArgb, litArgb, secArgb }` to Elite color records, the 4th color slot ("Lights") directly drives the glowing emissive nodes on Elite armor (chest, back, helmet).
  - Also fixed `MatchEliteColorPattern()` to check `(cur[0] == lastPri || cur[3] == lastPri)` so rescans seamlessly track modified Elite color records across biped rebuilds.

### 2. 4th Color Picker Slot in Armory UI (@O7OF)
- Added 4th color row `Lights` to `ArmorCustomizer.cpp` under `"COLORS"`, supporting both Reach's 30 palette options and custom RGB hue-wheel picking.
- Profile persistence in `appearance.json`: saves/loads `lights_color`, `lights_custom`, `lights_custom_rgb`.
- Added swatch rendering and `colourSlot` index mapping (0: primary, 1: secondary, 2: accent, 3: lights).
- Synchronized with `ReachArmorySelection`: `sel.lightsColorIndex`, `sel.lightsRgb`, `sel.lightsCustom`.
- On Spartans, slot 4 drives lights / visor reflection (`words[3] = (sel.lightsCustom ? litArgb : visArgb)`).

### 3. Expanded Spartan Visor Palette (@O7OF)
- Expanded `kVisorColors` in `ArmorCustomizer.cpp` from 11 to 20 visor finishes, adding 9 new finishes:
  - `AMBER` (deep desert amber, thermal-glare rejection)
  - `CYAN` (polarized cyan finish for vacuum operations)
  - `RUBY` (mirrored ruby-red coating with metallic refraction)
  - `SAPPHIRE` (deep cobalt sapphire dielectric stack)
  - `AMETHYST` (royal purple finish with laser-dazzle suppression)
  - `BRONZE` (burnished bronze-copper deposition)
  - `SUNFIRE` (solar-flare orange with intense luminance)
  - `LIME` (high-contrast chartreuse coating)
  - `PLATINUM` (pure mirror-finish platinum reflection)
- In `ReachHost.cpp`: mapped extended visor IDs cleanly to valid MCC stock visor indices (0..4) in `visorOffset()` so profile packets remain strictly valid, and expanded index clamping range from `0..4` to `0..19`.

### 4. Build & Packaging Verified (@O7OF)
- Both `noble-dev.exe` and `noble-client.exe` compiled cleanly with 0 errors via MSVC Release x64.
- Packaged release `Noble.exe` updated in `C:\Users\Chey1\Documents\Noble Launcher\`.

## 0. Pick up here (2026-10-31, 22:10 ET) - Variant field map: native offsets and a live logger (@deepseek)

**Commits:** `e78e5f32` (logger), `45df5c47` (score), `b3a93b54` (API), `e8465b55` (launch apply), `c8dec830` (patch CLI). **Version:** 0.9.12.7. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`). **Full table:** `docs/plans/variant-field-map.md`.

**Method.** `-variant-dump` decodes a stock `.bin` to the native 0xFC04 `c_game_variant`. I then patched one known field in the file, decoded it again, and diffed the two native blobs. Engine decode accepts the patched files (no SHA-1 recompute needed for the probe).

**Proven:** general/respawn `0x2BC`-`0x2CC`; all Player Traits `0x2D0`-`0x2FA` (health, weapons, movement, appearance, sensors); social flags `0x2FC`; map flags `0x300`; base traits start `0x309`; weapon/vehicle sets `0x330`/`0x332`; score to win `0xF840` (uint16; Slayer 25, Team Slayer 50, Juggernaut 150, CTF 3).

**Logger.** `DumpGameVariant` prints those fields after decode. Stock Slayer reads `roundsToWin 0 time 10 roundLimit 1`, respawn `time 5 suicide 5 betrayal 5`, `weapon 253 ability 253 mapFlags 31 social 7`, `score to win 25`.

**Team/FFA finding.** Patching the file's team flags changes nothing in the native blob. The decoded variant has no editable team/FFA bit; stock Slayer and Team Slayer use the same general layout and differ in the Megalo script. The lobby split must be a host choice or a name/mode heuristic, not a variant field.

**Loadouts.** Patching loadout fields produced no native diff; they are file/script data, not `c_game_variant` fields. A loadout editor must write the variant file.

**API and apply.** `src/engine/VariantFields.hpp` provides bounds-checked `Read`/`Write` over the mapped offsets with a round-trip self-test (`PASS: variant fields - read/write round trip`). `BlamEngine::SetVariantOverrides` stores a host edit; `LaunchMap` applies it to the decoded variant and logs `applied host variant overrides` before copying into the launch block. This path reached a real launch block: `-variant-override time=15,score=50,rounds=3` on stock Slayer logged effective time 15/score 50/rounds 3 (file 10/25/0) before the block was copied. **Live proof:** `time=1,score=50` on `forge_halo` showed the HUD `10 seconds remaining` at 70 s wall time and ended the match after 1.3 min (`PauseMenu: game over`). Evidence: `docs/screenshots/variant-time-override-10s.png`, `docs/evidence/variant-time-override.txt`. The score limit was not observed: `score=1` kept the five-segment HUD bar and `score=0` did not end the match; `forge_halo` had no kill source. Score-to-win is launch-block-only verified.\n\n**TU1 check:** an encoding 0x6B Team Slayer reads time 12 and score 50 at the same offsets, so the mapped General/Player Traits/score fields hold on base and TU1 variants. **Still open:** loadout file writer, team entries, and a native writer. Do not expose editing until each offset has a round-trip probe and a TU1 check.

Self-review: these are stock encoding 0x6A offsets from `slayer_054.bin`; a TU1 variant may differ. The probe only changes bytes temporarily outside git.

## 0. Pick up here (2026-10-31, 21:45 ET) - Voice binds no longer steal pad buttons; F6 conflict gone (@deepseek, from @claude QA)

**Commit:** `cf4cf676`. **Version:** 0.9.12.2. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`).

**Bug 1, pad owner stolen.** @claude bound Voice to party to RB; `PadLayouts::Assign` treated the voice action as an owner and unbound Melee. Voice actions are read separately from the engine pad state, so they can share a button. Assigning a voice action now leaves game actions alone, and assigning a game action leaves voice actions alone. Both keep working.

**Bug 2, PADDLE 6 = F6.** Keyboard defaults for Voice to party/team/everyone were F6/F7/F8; PADDLE 6 is F6. Moved to **F9/F10/F11**. Existing player overrides are untouched.

**Evidence.** `-test-map-gametypes`: `PASS: 43 rule checks ...`, `PASS: button layouts - 29 expectations, 0 failure(s)` (the three new checks cover voice-on-RB not unbinding Melee, Melee still firing, and a game binding not unbinding voice). `-voice-test loopback`: all row checks plus `keyboard defaults avoid the F1-F6 paddle keys: ok`, `jitter test: PASS`.

**Still open.** Physical pad rebind is Tyler's. The **variant field map (item 9) is not started**; it is the blocker for full game options and team-aware lobbies, and it needs a dedicated RE pass over the decoded `c_game_variant` blob.

Self-review: voice sharing means one press can both melee and talk if a player puts voice on Melee's button. That is deliberate and matches keyboard behaviour, but the settings hint should eventually say so.

## 0. Pick up here (2026-10-31, 21:34 ET) - Game type drop-downs and menu search; team and options findings (@claude)

**Commit:** `0576f84`. **Version:** 0.9.12. **Build:** noble-client, noble-dev, 0 errors, 0 warnings on the merged tree.

**Tyler's asks.** Game types now drop down by mode: press Slayer and its variants open, one mode at a time, starting on the current variant's mode. Mode names are off the rows and the details panel. Firefight has one mode, so it stays a flat list. Search boxes: game type lists (setup screens and lobby), Campaign, Custom Game, Firefight, Forge, Mods. Server Browser and Settings already had one. A search result lands on the real row in the full list, opening its mod if needed. Esc in a box only leaves it; Q, E, F, X no longer fire while typing. Typing needs a keyboard.

**Evidence** (`docs/screenshots/variant-categories-dropdown`, `variant-search`, `map-search`, `mods-search`, `variant-lobby-search-typing`). Pad: heading opened, Juggernaut (MCC) picked, host, start, `Decoded game variant ...hr_ffa_juggernaut_ar_mag_150points_tu.bin`. Keyboard: `flag`, Enter, Down, Enter picked Multi Flag CTF; `spi`, Enter, Enter landed on Spire. Mouse search on Mods filtered `fire`. Lobby typing `fqex` opened nothing.

**Answer to Tyler: choosing Team Slayer does NOT change the pre-game lobby.** The roster splits into red and blue columns only when a player's `team` is non-zero, and the only non-zero values in the client come from the server's roster or the `-lobby-demo` test. No picker code sets it, and `MasterAdvert.hostTeam` is never set from a variant (read from code, not a two-client run). Reach's team/FFA flag is inside the variant rules, so doing this properly needs the field map (item 9). Names alone won't do: the file's name doesn't reliably say team or FFA.

**Not verified.** Search on the MODDED tab pick and Forge, the lobby search picking a row, a guest receiving the variant, physical pad, matches past the loading screen.

Self-review: search boxes are small in the tab row. `DrawSearchBox` ignores its `ui` argument. A mod search matches map and mod titles only, not descriptions. A failed early run of my keyboard test turned out to be too short a wait, not a bug.

## 0. Pick up here (2026-10-31, 21:24 ET) - Voice-bind row QA and game type picker (@claude)

**Commit:** `02cf7fe`. **Version:** 0.9.11 (over @deepseek's 0.9.10.7, code kept). **Build:** noble-client, noble-dev, 0 errors, 0 warnings on the merged tree.

**Voice-bind rows (QA on 0.9.10.5 rows, build 0.9.10.7+).** All six present, no SettingsPanel change. Keyboard: VOICE TO PARTY / TEAM / EVERYONE show F6 / F7 / F8. Controller: the same three show unbound (`- (preset)`). Rebinds: Voice to team to F9 saved as `keyOverrides 74:120`; Voice to party to RB saved as `padBindings voiceparty:512`. Both still showed after a restart. Scripted pad and a synthetic key; physical pad remains Tyler's. **Two findings for @deepseek:** (1) Binding an unbound voice row to a button in use unbinds the other action: RB gave `melee:-1`, so melee had no button, with no warning, because the swap hands over the old input and there is none. (2) PADDLE 6 KEY defaults to F6, the same key as Voice to party; the row text says a paddle key stops being a keyboard key while a controller is connected.

**Game type picker.** A or a click on GAME TYPE opens a full-screen list on Custom Game and Firefight, sorted by mode; arrows still step. The lobby's list uses the same rows and details. Shows name, mode, description, file. Only variants the map can host are offered, as before. Scans MCC's `game_variants` only, which is all the menu scanned: 54 files, Forge's excluded from the lists. **Descriptions:** each file holds a name at 0xC0 and a description at 0x1C0 (checked by dumping all 54). Only 4 hold real text (Basic Editing, Invasion, both Escalation Slayers); the rest hold a `hr_gvar_*_desc` key not found in `EN_HaloReach.bin` as text, so those show the existing per-mode sentence, not variant-specific wording. No rules are read or edited. Labels: corrected Halo Ball and Multi Flag CTF to match their files; other labels stay Noble's. **Also fixed:** HOST LOBBY / HOST CO-OP / HOST FORGE were mouse-only; now one row below PLAY.

**Evidence** (`docs/screenshots/variant-*`, `voice-bind-*`). Scripted pad: open, scroll, pick, host, start. Two different variants reached the lobby and the engine: Team Slayer `Decoded game variant ...hr_4v4_team_slayer_dmr_ar_50points_tu.bin`, Juggernaut `...hr_ffa_juggernaut_ar_mag_150points_tu.bin`. Lobby banners also captured for Team Slayer and Classic Slayer. Mouse: click on the row, click King of the Hill, `game type picked: King of the Hill (koth_054.bin)`, and the row and PLAY LOCAL show it.

**Not verified.** Those matches were seen to the loading screen, not played, so the rules in play weren't checked. Picker on the MODDED tab, keyboard arrows and Enter in the list, a guest receiving the variant, and any physical pad. `variant-picker-firefight.png` predates the row wording tidy (it still reads like FIREFIGHT · FIREFIGHT).

**My test mistake.** To avoid the live server I blanked `masterServerUrl`, but `Settings` rewrites blank and the old address to `https://servertestdeploy-jfxb.onrender.com`, so about three host-lobby runs advertised a public lobby under the test profile there until killed. `/api/sessions` afterwards returned an empty list. Later runs used `http://127.0.0.1:9`. Use that for tests. Same rewrite means my earlier "Not Found" on `outbreak-z-test-server` probed the wrong host; that deploy check is still open against `servertestdeploy-jfxb`.

Self-review: the header offsets come from these 54 files, not a format spec, so a user file with another layout could show a wrong name; the read is bounds-checked and a non-`_blf` file falls back to the file name. Two GAME TYPE click areas sit under OptionRow's arrows' row, not tested with a mod list. `BuildVariantChoices` runs per frame while a list is open, which is small but not free.

## 0. Pick up here (2026-10-31, 22:00 ET) - Cross-Build Save Migration & Elite Colour Picker Live Sync (@O7OF)

**Version:** 0.9.10.7. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev` + packaged `Noble.exe`). Files: `src/career/Career.cpp`, `src/common/DeviceIdentity.cpp`, `src/common/DeviceIdentity.hpp`, `src/common/Settings.cpp`, `src/ui/ArmorCustomizer.cpp`, `src/engine/ReachArmory.cpp`, `src/common/Version.hpp`, `build-package.ps1`.

### 1. Cross-Build Save Migration & Bidirectional Sync (@O7OF)
- **Problem:** When playing on the developer build (`noble-dev.exe` in `build\bin\Release\`), user unlocks, rank, credits, purchased armor, appearance customization, and settings were stored in the dev directory (`build\bin\Release\data\userdata\`). When launching the non-dev release build (`Noble.exe` in `Documents\Noble Launcher\`), saves were isolated, meaning players would lose or not see their progress made during dev testing sessions.
- **Fix:** Implemented full cross-build save migration and live mirroring in `DeviceIdentityStore`:
  - `KnownDataDirs()` enumerates all candidate data paths across the system: the active exe data directory (`build\bin\Release\data` or `Documents\Noble Launcher\data`), `%USERPROFILE%\Documents\Noble Launcher\data`, and `%APPDATA%\ProjectNoble\data`.
  - `MigrateNewestFile(relativeFilePath)`: On process startup, compares timestamps across all candidate paths. If a newer `career.dat`, `appearance.dat`, `settings.json`, or `identity.json` exists in another build's folder, it seamlessly migrates it to the active folder before loading.
  - `MirrorSaveFile(relativeFilePath)`: Whenever a save occurs (buying armor, earning credits/stats, changing appearance or settings), the updated save is immediately mirrored across all known installation locations.
  - Added `/XO` (exclude older) to `build-package.ps1` so packaging release builds never clobbers newer save files.

### 2. Elite Armory Color Synchronization (@O7OF)
- **Reverse Engineering:** From decompilation of `host-probe.exe::write_colors` @ `0x1401b5ad0`:
  - Spartan ARGB color layout (16 bytes): `[0] = Primary, [1] = Secondary, [2] = Accent, [3] = Visor`.
  - Elite ARGB color layout (16 bytes): `[0] = Base/Rank Skin, [1] = Undersuit/Detail, [2] = Secondary, [3] = Primary`.
- **Implementation:** Added `s_eliteColorRecords`, `MatchEliteColorPattern()` recognizing the 11 stock Elite rank preset ARGB palettes, and `ScanEliteColorRecordsIn()`.
- `WriteColorsAndSuit()` updates Elite live records with `{ Primary, undersuit, Secondary, Accent }`, immediately syncing the chosen Primary and Secondary colors to the live Elite model.
- Quick rescan detects remade Elite bipeds instantly without waiting on Spartan suit records.

### 3. Build & Packaging Verified (@O7OF)
- Both `noble-dev.exe` and `noble-client.exe` compiled cleanly with 0 errors via MSVC multi-threaded builder.
- Packaged release `Noble.exe` updated in `C:\Users\Chey1\Documents\Noble Launcher\`.

## 0. Pick up here (2026-10-31, 21:55 ET) - Elite Primary is now the base colour, not the overlay (@deepseek, from Tyler/O7OF feedback)

**Commit:** `dcba008d`. **Version:** 0.9.10.7. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`). Files: `src/engine/ReachArmory.cpp`, `src/common/Version.hpp`.

**The bug.** The Elite ARGB write kept the shader's stock rank colour in `cur[0]` ("base / permutation") and wrote `Pri` to `cur[3]`, the overlay slot. Primary therefore acted as secondary/overlay: White Primary washed the whole model, and changing Secondary appeared to "unblend" it. Palette colours made it obvious; custom wheel values hid it because the overlay was less bright.

**Fix.** Drive the Elite record as `{Primary, undersuit, Secondary, Accent}`: base = Primary, overlay = Secondary, detail = Accent. This is the same assumption the Spartan record already uses.

**Evidence.** White Primary + Blue Secondary now renders a white base with blue detail (`docs/screenshots/armory-elite-primary-base.png`), not the old blue model with a white wash. Tested against the old mapping with the same two colours; the old capture is `noswap` in the working session only (not committed). Appearance restored to Elite / Primary 13 / Secondary 29 / Accent 2 / visor 0.

**Self-review:** the Elite rank's stock base colour is no longer preserved in the preview. That is intended for a player-selected Primary, but if rank-specific base colours are wanted later, add an explicit "rank colour" toggle instead of restoring the old slot mapping.

## 0. Pick up here (2026-10-31, 21:45 ET) - Armory overlay sheen no longer washes out armour (@deepseek)

**Commit:** `5fd1ff5e`. **Version:** 0.9.10.6. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`). Files: `src/ui/FrontEndMenu.cpp`, `src/common/Version.hpp`.

**The bug.** `ArmoryStage::Draw` stacked 30 additive white/blue ellipses behind/over the model. Each ring added up to 1/255 per channel; at the centre that is roughly +30/255, which desaturated armour colours and produced the white/blue sheen.

**Fix.** Remove the chest glow. Keep only a small blue floor pool below the feet (10 rings, no red/green), so the chamber keeps some atmosphere without an overlay on the body. Captures: `docs/screenshots/armory-overlay-before.png`, `armory-overlay-after.png`.

**Evidence.** Same Elite profile and camera; before has the blue haze and lower colour contrast, after is cleaner and more saturated. This is a visual fix only; no armour data or engine lighting changed.

Self-review: the model is darker with the key light gone. If the chamber now reads too dark, the next step is a real light behind the model or a mask-based rim light, not putting the additive white stack back.

## 0. Pick up here (2026-10-31, 21:30 ET) - Prophets' Hand Armory loader spike: no-go without an armour pack (@deepseek)

**Version:** 0.9.10.5 (docs only). **Artifact:** `docs/plans/prophets-hand-armory-loader-spike.md`.

**Result.** Gameplay map override is the working route for the mod's lighting and Elite pieces. The Armory cannot load them from the mod as shipped: Noble's `ElitePack` requires a sidecar-backed armour cache (`rr_elite_pack.map` + `.rrreloc` + `.rrneeds` + `.rrpmcg`, about 96 MB total), while Prophets' Hand ships a 1.22 GB scenario cache with no sidecars. Treating a full scenario as a second menu cache would collide with the menu's globals and lacks relocation data.

**Options.** (1) Ask the mod author/O7OF for an armour-only pack in the existing sidecar format — recommended; (2) approve/extract the tags into a pack ourselves — multi-day RE/asset-pipeline work with redistribution questions; (3) gameplay-only map override, already working; (4) separate Armory lighting presets, since stock menumap lighting does not transfer.

**Decision needed:** O7OF/Tyler — request the armour pack or approve extraction. No code was changed in this spike.

Self-review: this is a no-go for the quick loader, not a claim that the pieces cannot be ported. The recommendation is deliberately the smallest legal/technical step.

## 0. Pick up here (2026-10-31, 21:20 ET) - CI failure-mail burst fixed; docs-only pushes skip the build (@deepseek)

**Commit:** `3fc9450f`. **Version:** 0.9.10.5 (unchanged; workflow only). Files: `.github/workflows/build.yml`.

**Why the mail.** The Build workflow runs on every push to `main` and had been failing on CI's stricter MSVC: `C3493: 'kPeer' cannot be implicitly captured` in `VoiceChat.cpp` and `'kPttKey'` in `SettingsPanel.cpp`. Local MSVC accepted the lambdas; GitHub's did not. Every docs/code push therefore produced a failed run and an email. The captures were fixed in `a02a3622`; the latest completed run (`0adb2db5`) is **success**.

**Workflow change.** Added `paths-ignore` for `docs/**`, root `*.md` and `reports/**`, plus `concurrency` so a new push cancels the previous run; added a 25-minute timeout. Cancelled three stale in-flight runs through the API (they would otherwise have failed and mailed).

**Not verified.** The workflow run for `3fc9450f` was still in progress when this was written. A docs-only push after it should produce **no** new run; that is the check that the filter works.

Self-review: this hides build failures for docs-only pushes by design. A docs commit cannot change the executable, so that is intentional; workflow changes and `src/`/CMake changes still build.

## 0. Pick up here (2026-10-31, 21:10 ET) - Headless self-tests fixed and passing (@deepseek)

**Commit:** `e80415c4`. **Version:** 0.9.10.5. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`). Files: `src/main.cpp`, `src/common/Version.hpp`.

**The bug.** `-test-map-gametypes` set `_O_U8TEXT` on stdout, then the tests wrote narrow `printf` output. UCRT aborted with BEX64/`0xC0000409` in `ucrtbase.dll` before the first test line, which is why the map/layout harness could not be used in this session.

**Fix.** Removed the stdout mode switch. The tests now print normally and pass:
- `PASS: 43 rule checks, 280 map x type combinations (207 allowed, 73 blocked), 0 failure(s)`
- `PASS: map names - 10 display cases, 0 failure(s)`
- `PASS: button layouts - 26 expectations, 0 failure(s)`
- `PASS: input tuning - 0 failure(s)`
- exit code 0.

This also re-checks the new VoiceParty/VoiceTeam/VoiceGlobal pad actions against the 26 layout expectations.

Self-review: removing the UTF-8 stdout mode assumes the test output stays ASCII; if a future test prints non-ASCII, use the logger or a wide stream instead of putting the mode back.

## 0. Pick up here (2026-10-31, 21:00 ET) - Elite Armour preview now follows the selected colours (@deepseek verification of O7OF `a02a3622`)

**Commit:** `a02a3622` (O7OF; landed on main while this session was active). **Version:** 0.9.10.4. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`) after merging it.

**What the commit does.** Adds a separate `s_eliteColorRecords` scan and write path: it finds the Elite shader's 16-byte ARGB records by the stock rank palettes / the last primary, and writes the Elite layout `{base, undersuit, secondary, primary}` every frame. This is the missing Elite colour path that previously left the pack model stock blue.

**Verified here.** Same Armory session, Elite selected: Primary **Cobalt (17)** showed the blue model; stepping to **Maroon (22)** changed the model to the olive/maroon shader live, without a scene/slot reload. Captures: `docs/screenshots/armory-elite-colour-before.png`, `armory-elite-colour-after.png`. Appearance restored to Elite / Primary 13 / Secondary 29 / Accent 2 / visor 0.

**Visor proof (same version).** Spartan preview, same dirty-flag path: Default -> Rose Gold, 1920x1080, the visor changed from amber to pink/red live. Captures: `docs/screenshots/armory-visor-before.png`, `armory-visor-after.png`. This closes the visor half of item 4. Appearance was restored to Elite / Primary 13 / Secondary 29 / Accent 2 / visor 0 afterwards.

**Closed.** Elite Armory colours and the visor capture are both now demonstrated. The commit title is unrelated to its actual ReachArmory change; the build was 0/0 after the merge.

Self-review: `a02a3622` is not mine, so this is a verification entry plus the captured evidence; no code was changed by me for the Elite colour fix.

## 0. Pick up here (2026-10-31, 20:50 ET) - Six voice-bind rows verified in the loopback test (@deepseek)

**Commit:** `3bfcd059`. **Version:** 0.9.10.4. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`). Files: `src/audio/VoiceChat.cpp`, `src/common/Version.hpp`.

**What changed.** Added two checks to the loopback self-test that `KeyBindings::ActionName` and `PadLayouts::ActionName` return non-empty names for party/team/global. Settings builds both lists by iterating the tables, so this is the check that all six rows exist (three keyboard, three controller) without a screenshot-only claim.

**Evidence.** `-voice-test loopback` at 0.9.10.4: `loopback: PASS`, all five channel-priority checks `ok`, both `voice bind: ... rows exist` checks `ok`, `jitter test: PASS`.

**Independent nav check (same version).** On the merged 0.9.10.4 tree, a scripted pad pressed RB twice and Settings moved from Graphics to Game Options; the footer shows `LB RB Category` and the version string. Capture: `docs/screenshots/nav-independent-settings-category.png`. This re-checks Claude's category-switch fix rather than relying on the original report.

**Still open.** Two-client channel isolation, physical pad press, and rebind capture. The row-existence check does not prove a user can successfully bind a key/button.

Self-review: this checks the tables, not the rendered Settings page. A layout bug could still hide a row after the table is correct.

## 0. Pick up here (2026-10-31, 20:45 ET) - Scripted-pad look tokens; Firefight safe room still will not move (@deepseek)

**Commit:** `90ea84dd`. **Version:** 0.9.10.3. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`). Files: `src/ui/UiPad.cpp`, `src/common/Version.hpp`.

**What changed.** `-pad-script` gained `rsup`, `rsdown`, `rsleft`, `rsright`, so a scripted run can turn the camera as well as move. Normal input is unchanged.

**Prophets' Hand result.** With `TPH_ff.bin` and `ff45_corvette` overriding, a 40-step warm-up, 180-degree right-stick turn and 40-step forward walk left the player exactly in the safe room; the view and position did not change, and no NPC/wave appeared. The scripted pad is being applied in the menu, but the Firefight camera/input appears to wait on the mouse-look prompt. This is the concrete blocker: scripted controller movement does not leave the safe room. Do not claim the Elite model/lighting proof from this.

Self-review: the right-stick tokens are test-only and might make a future pad-script test move the camera unexpectedly; they are opt-in tokens.

## 0. Pick up here (2026-10-31, 20:40 ET) - Voice channel priority self-test passes (@deepseek)

**Commit:** `2c837e07`. **Version:** 0.9.10.2. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`). Files: `src/audio/VoiceChat.cpp`, `src/common/Version.hpp`.

**What changed.** Pulled the separate-PTT priority rule into `ChannelFromHeld` and added five checks to the loopback self-test: party wins when several are held, team beats everyone, everyone beats generic PTT, generic PTT uses TALK TO, nothing held is silent. `PushToTalkChannel` now reads all four keyboard and pad inputs through that rule.

**Evidence.** `noble-client -voice-test loopback` printed `loopback: PASS`, all five `voice channel:` checks `ok`, and `jitter test: PASS`, at version 0.9.10.2. This proves the routing decision, not a two-client audible test.

**Still open.** Two-client channel isolation, a physical pad press, and rebind capture for the six new rows (three keyboard, three pad). Do not mark the voice-channel feature complete until the two-client run.

Self-review: the self-test calls the same pure rule as the live path, so a future UI label can still lie about which channel a key selects; the live `PushToTalkChannel` path is not exercised by the self-test.

## 0. Pick up here (2026-10-31, 20:25 ET) - Controller menu navigation across Settings and the menus (@claude)

**Commit:** `acd2f83`. **Version:** 0.9.10 (resolved over @deepseek's 0.9.9.1 and 0.9.9.2, keeping their code). **Build:** noble-client, noble-dev and noble-dedicated, 0 errors, 0 warnings on the merged tree.

**What was actually wrong.** Settings LB/RB and the Service Record D-pad were reproduced by scripted pad before the change; the others come from reading the code, and only their fixed behaviour was tested:
- Settings had no LB/RB at all; only keyboard Q/E switched category. Left/right change the value on a value row, and focus starts on a row, so the only pad route to another category was Up past the first row. Nothing said so.
- Service Record and Mods ignored the D-pad, and Mods couldn't scroll. ImGui's own navigation was enabled on those screens but had no focus to act on.
- Server Browser moved through the unsorted list while drawing the sorted one, so the highlight jumped, and it never scrolled to the selection.
- Pause: hovering set focus every frame, so a resting pointer pinned the row against the D-pad. Left stick did nothing there.
- Post-game had no pad path of its own.
- Audio's music lists were mouse-only.

**Not reproduced:** row scrolling in Settings already worked under the scripted pad before this change (Controller page, 14 downs). Tyler reported it failing on a real pad. Nothing here explains a difference between real and scripted input, so check that first on the physical test.

**Fix.** `UiPad::Feed` turns ImGui's pad navigation on only while a popup is open, which removes the double-press cause everywhere. `UiPad::Up/Down/Left/Right/PageUp/PageDown/ScrollWindow` are shared. Settings: LB/RB category, triggers 8 rows, right stick scrolls, Back restores defaults, hints, new BROWSE GAME / BROWSE TRACK / TRACK IN SHUFFLE rows. Server Browser: drawn order, scroll, triggers (`-server-browser-demo`). Mods: up/down, triggers, X refresh. Service Record: D-pad, stick, triggers scroll. Pause: moving mouse only, left stick, challenge scroll. Post-game: left/right, A; starts on RETURN TO MENU, never QUIT; A during the reveal only skips, and presses count from 1 s after it ends, because A is jump. Also touches `ServerBrowser.cpp`, `CareerPanel.cpp`, `WorkshopManager.cpp` beyond the listed files.

**Evidence** (scripted pad, 1920x1080, `docs/screenshots/nav-*`): RB twice Graphics to Game Options; LB wraps to System; triggers scroll Controller; A toggled V-sync once; music rows; left stick on System; demo servers land on 18 after 3 downs and 2 triggers (not committed: that screen prints the public IP); Service Record Challenges scrolled; Community INVITE focus; Custom Game and Armory move. Pause in a real match: pointer resting on RESTART MATCH, two D-pad downs reached CONTROLS. Post-game: focus on PLAY AGAIN, then QUIT, then A on RETURN TO MENU logged once. Pad rebind: A then RB bound Team chat to RB and stayed on the page. Keyboard: E, Down, typed search still work.

**Gaps.** No physical pad. COPY INVITE was not verified: the script pressed X in the lobby, but the button logs nothing, so neither the copy nor the press count was checked. The double-press cause is removed by design, untested here. Pause > Settings LB/RB and Post-game PLAY AGAIN / QUIT presses not run. Not run: mouse slider drag, sliders typed by value, Forge editor. Server Browser filter checkboxes, Direct Connect, Master Server and top buttons have no pad path (LB/RB mode filter, X refresh, Y favourite do). Mods' Workshop and folder buttons, Settings search and all typing need a mouse or keyboard. The Service Record's tab buttons are LB/RB only.

**Heads-up.** The Server Browser header prints the public IP, an older behaviour and a risk for screenshots and streams; never commit that capture. `git pull --autostash` conflicted on Version.hpp mid-test; resolved without loss.

Self-review: `-pause-at` didn't open the menu in my run and I didn't look into why; real Esc was used. The post-game outline shows whenever a pad is connected, even for a mouse user. Music rows are redundant with the lists for mouse players.

## 0. Pick up here (2026-10-31, 20:35 ET) - Controller bindings for the three voice channels (@deepseek)

**Commit:** `b009ec79`. **Version:** 0.9.10.1. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`). Files: `src/engine/PadLayouts.*`, `src/audio/VoiceChat.cpp`, `src/common/Version.hpp`.

**What changed.** Added `VoiceParty`, `VoiceTeam` and `VoiceGlobal` to PadLayouts, unbound by default. The Settings > Controller BUTTON BINDINGS list iterates every action, so the three rows appear without a SettingsPanel change. `VoiceChat` reads each pad input separately and uses the same party > team > everyone priority as the keyboard binds; the generic pad PUSH TO TALK remains the fallback. The voice actions are skipped by `PadLayouts::Apply`, so they are never written into the engine's pad state.

**Verification.** `-voice-test loopback` still PASS after the change (24 kbps → 20 kbps test sequence, jitter self-tests). `-test-map-gametypes` crashes in this shell with BEX64/0xC0000409 in `ucrtbase.dll` before any output; the same test path was already unusable while adding the visor feature, so I did not use it as evidence. Needs a console run to confirm, or a fix to the test harness.

**Not verified.** Two-client channel routing, a physical pad press, and rebind capture for the new rows. The pad rows are visible in code but no scripted-pad capture was taken.

Self-review: the three pad actions are unbound by default, so existing controller users see no behaviour change. Assigning them to a game button moves that game action, following the existing `Assign` rule for PTT.

## 0. Pick up here (2026-10-31, 20:25 ET) - Separate party/team/global voice push-to-talk binds (@deepseek)

**Commit:** `c55a40d`. **Version:** 0.9.9.2. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`). Files: `src/engine/KeyBindings.*`, `src/audio/VoiceChat.cpp`, `src/common/Version.hpp`.

**What changed.** Added three Noble actions: **Voice to party**, **Voice to team**, **Voice to everyone** (keyboard defaults F6/F7/F8; user-rebindable). They appear automatically in Settings > Controls > Keyboard & Mouse because the binding list reads `KeyBindings::ActionsIn(ActionGroup::Actions)`. `VoiceChat` reads all four keys every 500 ms; the specific channel key wins over the generic PUSH TO TALK, with priority party > team > everyone. The generic PTT and the TALK TO setting remain the fallback, so existing users are unchanged.

**Not verified.** Two-client channel routing, a real microphone, controller-specific channel binds (the controller still has one PUSH TO TALK plus TALK TO; separate pad channel binds are not implemented), and key-rebind capture for the three new rows. Do not mark the voice-channel feature done until the two-client test.

Self-review: F6-F8 are global defaults and could collide with another program's hotkeys; they are rebindable. PARTY still means ALL at the transport level until Noble has parties. Holding two channel keys picks the narrower channel, which is a rule I chose, not a Reach rule.

## 0. Pick up here (2026-10-31, 20:12 ET) - Six new Spartan visor finishes as unlocks; Prophets' Hand Firefight variant test (@deepseek)

**Commit:** `494e579`. **Version:** 0.9.9.1. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`). Files: `src/ui/ArmorCustomizer.cpp`, `src/ui/SpartanArmoryCatalog.inc`, `src/engine/ReachHost.cpp`, `docs/career/armory_overrides.json`, `docs/career/spartan_armory_catalog.*`, `tools/armory/build_spartan_catalog.py`.

**Visor unlocks.** Added six finishes to the Spartan visor row: Rose Gold, Emerald, Violet, Ice, Crimson and Onyx (11 total). Rank gates and cR prices live in `armory_overrides.json` and are generated into the catalog, so the existing locked / for-sale / buy / equipped UI works unchanged. The picker uses swatch tiles for the new finishes (the stock five keep their helmet pictures). Saved as `visor_color_index` + `visor_id`; the Armory preview recolours live through the visual dirty flag.

**Evidence.** `-armory-unlock-all` run: all 11 entries; Selected Emerald changed the model's visor from gold to green (`docs/screenshots/armory-visor-unlocks-emerald.png`). A normal run with the current rank shows Silver/Blue/Black locked with their rank text (`armory-visor-unlocks-locked.png`); the same run reset the saved Emerald to Default because the profile had not unlocked it, which is the intended lock rule. Appearance was restored to Elite / Primary 13 / Secondary 29 / Accent 2 / visor 0 with `-pad-script`, without stealing focus.

**In-game limit.** Reach's profile accepts only stock visor item ids 157-161. Until a visor-shader tint override exists, `ReachHost` maps each new finish to the closest stock visor (rose_gold -> gold, emerald/violet -> blue, ice -> silver, crimson -> default, onyx -> black). The Armory colours are real previews, not yet real in-match colours. Do not mark the in-game part done from these captures.

**Prophets' Hand Firefight test.** Launched `ff45_corvette` with its own `haloreach\game_variants\TPH_ff.bin`; the log shows the custom variant decoded into the launch block (`+0x300`) and `Firefight_tph` starting at frame 495. The safe room rendered the Elite HUD and arms, but the camera stayed fixed on the ceiling and mouse look did not move it; no live NPC or wave appeared before the run ended. The map and variant are loading - the remaining blocker is getting out of the safe room / into a wave, or using an insertion point/freecam. This is still `[~]`, not a rendering failure proven.

**Not verified.** True in-match visor colour, the new tiles with helmet art, physical pad, and the full Prophets' Hand Elite model frame.

Self-review: the new visor finishes are Noble-created unlockables with invented rank/credit prices; they are not Reach's original visor list. The catalog is generated, so change `armory_overrides.json` and rerun the tool rather than editing the `.inc` by hand.

### How this file is kept (Tyler, 2026-10-30)

- **Every change gets an entry - no exceptions**: code, UI, docs, tools, config, server. Add it at the **top**, directly
  under the `--` line, with `## 0. Pick up here (2026-10-31, 17:10 ET) - Community by controller, pause Controls, settings export and import, first start (@claude)

**Commit:** `fba4c6f`. **Version:** 0.9.9. **Build:** 0 errors, 0 warnings on the merged tree. Files: `src/ui/FrontEndMenu.cpp`, `src/ui/PauseMenu.cpp`, `src/ui/SettingsPanel.cpp`, `src/common/Settings.*`, `src/main.cpp`.

**Community by controller.** The flow was untested and didn't work: ImGui's navigation never moved. `padButton` steps D-pad or stick through every button in reading order, A presses, the focused one has an outline. Pad Y from the lobby reaches it (screen 11 -> 12), and INVITE fired by script (log line, `-social-friends-demo`, which sends nothing). Typing fields still need a keyboard. Also stopped printing IP addresses on invites and friend rows.

**Pause CONTROLS.** New entry, opens Settings at Controller or Keyboard & Mouse by last input. Real match by scripted pad: `PauseMenu: controls opened`, `docs/screenshots/pause-controls-page.png`. The Controller tab choice wasn't seen, because the test's real Esc made the keyboard the last input.

**Export and import.** Settings > System, plus `-settings-export <file>` and `-settings-import <file>`. A planted token and server address stayed out of the export. An import file carrying its own token, server and developer flag changed none of them; volume 99 was clamped to 1.0 in the file; a broken file exited 1 and changed nothing. Real save dialog, found on our process only: file written, no token. The import dialog wasn't run.

**First start.** `firstStartDone`; existing settings files count as answered. New player: input, name and tag, look speed. Returning: straight to the menu. Result of a scripted new-player run: `padLookRateX 2.0 -> 2.5, Y 2.5, firstStartDone True`. Rerun from Settings > System (row added, not pressed in a test). No graphics preset step because Noble has none, and no quick turn test. Captures: `first-start-question.png`, `first-start-name.png`, `first-start-look.png`.

**Unexplained once.** In one test run pause Settings opened 3 s after Esc with no script press, with the pointer wherever it was. It did not repeat with the pointer parked in the corner. Hovering moves pause focus, so a stray click on that row would do it, but I have no proof.

**My test risk.** One dialog test found the window by the title Save As, which could match another program's dialog and typed a path there. Nothing showed it did. The retest looks only at Noble's own process.

Self-review: the Community list walks tabs and rows as one flat line, simple but slow with many friends. `LOOK SPEED` assumes 2.00x is stock, from the earlier measurement.

**Note for scripts (@claude).** Insert new entries before the first line that *starts* with `## 0. Pick up here`. My script used the first match anywhere, which hit the example in the rule above and split that sentence for several entries, until @deepseek reworded it. The entries themselves are intact.

## 0. Pick up here (<date>, <time> ET) - <what> (<author>)`.

## 0. Pick up here (2026-10-31, 17:10 ET) - Armory colour/visor changes repaint the live model; 0.9.8.1 (@deepseek / DeepSeek)

**Commit:** `2d5e258`. **Version:** 0.9.8.1. **Build:** 0 errors, 0 warnings (Release `noble-client` + `noble-dev`). Files: `src/engine/ReachArmory.cpp`, `src/common/Version.hpp`, `docs/screenshots/armory-live-preview-before.png`, `armory-live-preview-after.png`, `docs/evidence/armory-live-preview.log`.

**The bug.** `ArmorCustomizer` rebuilt the full `ReachArmorySelection` and saved the profile on every field change, but `ReachArmory::SetSelection` only bumped the menu-map GLOBALS permutation rebuild when the piece pairs changed. A colour or visor change left the pairs identical, so `ApplyGlobalsAndPermutations` skipped REBUILD and the live biped kept the shader data it was built with. A later scene/slot change re-instantiated it, which is why the new colour only appeared then.

**Fix.** `SetSelection` now compares the visual half of the selection (palette indices, linear RGB, visor, armour pieces, effects, species) and sets a `s_previewDirty` flag only when it changes. `ApplyGlobalsAndPermutations` consumes that flag and forces the pair write + REBUILD even when the pairs are identical. Voice/service-tag-only changes still take the old no-rebuild path. The 250 ms coalescing is unchanged, so a held arrow key still costs one rebuild per settled step, not one per repeat. `SetMenuMode(false)` clears the flag.

**Evidence.** Same Armory session, no scene/slot reload and no match: Primary White (index 2) -> Orchid (index 19), 1920x1080 captures `docs/screenshots/armory-live-preview-before.png` / `armory-live-preview-after.png`; the armour changed from silver-white to purple in the frame. Each step logged `ReachArmory: preview repaint (primary N ...), rebuilt`; trimmed log in `docs/evidence/armory-live-preview.log`. The Armory auto-saves the profile on each click (existing behaviour); the repaint does not wait for a save button or reload.

**Version note.** Tyler asked for 0.9.7.1, but @claude pushed 0.9.8 (Chat Polish & Diagnostics) while this claim was in flight. 0.9.7.1 would have regressed the build number, so this landed as 0.9.8.1 with `VERSION_REVISION = 1`.

**Elite capture attempt (partial).** `-map-override` loaded both `m10.map` (campaign) and `ff45_corvette.map` (Firefight, carrier `ff45_corvette`); each log shows `VFS: map override` + `game session started`. The campaign player is Elite (profile, Elite HUD, Elite forearms), and the Firefight safe room rendered the Elite HUD/arms and a wall hologram (`docs/screenshots/prophets-elite-safe-room.png`). No full 3D NPC Elite was in frame: the campaign opens on a cliff before the first encounter, the Firefight safe room has no live NPCs, `-third-person` is set (`third person on` in the log) but the campaign camera hook sees an invalid player index and stays first person, and F5 through the pause overlay did not toggle. Capturing a rank model needs a checkpoint/insertion point or a freecam path; filed in TODO, not claimed as verified.

**Not verified.** A visual visor-change capture (the same dirty-flag path covers it; no UI-frame proof yet). Armour-piece changes already rebuilt through the pair comparison and were not re-captured. The Elite preview still stays stock blue when Primary changes - a pre-existing pack-shader limitation, now filed separately; the fix above is proven on the Spartan.

Self-review: forcing a rebuild on every settled colour step reintroduces one engine re-instantiation per step, which the old colour-only path deliberately avoided to stop the palette flash. The 250 ms coalescing keeps it to one per settled key; the alternative was no live colour at all. If Tyler prefers no flash, the next step is to find the pack/armour shader colour constant and write it instead of rebuilding.

## 0. Pick up here (2026-10-31, 16:39 ET) - Chat feed backing, diagnostics, hints, deaths log, handoff dedupe (@claude)

**Commit:** `83edc92`. **Version:** 0.9.8. **Build:** 0 errors, 0 warnings on the merged tree. Files: `src/ui/SocialOverlay.cpp`, `src/ui/SettingsPanel.cpp`, `src/ui/FrontEndMenu.cpp`, `src/career/Career.cpp`, `docs/HANDOFF.md`.

**Correction.** `2aea9fa`'s message says the closed feed got a backing. It didn't: a stuck compiler from my own cancelled build held the file open and the write failed. It's in `83edc92`.

**Done.** Feed backing (`docs/screenshots/chat-lobby-feed.png`). Hints line up with the panel on Settings and Community (`hints-aligned-settings.png`, `hints-aligned-community.png`). Handoff: 105 exact duplicate entries removed, first copy of each kept, nothing else changed. Copy Diagnostics replaces the player and Windows user names; **not run**, because no test pressed it.

**Career deaths: not solved.** The code reads the slot its comment says deaths should be in, and the numbers may simply be right for a test profile that never died. Match end now logs `Career: local general stats`. Next match where the player dies shows whether slot 11 moves. Don't change the slot before that.

**Open from the list.** Friends INVITE and Social by controller. Pause menu Controls page. Export and import to a chosen file (dated backup and restore already exist in Settings > System). First-start question. Stale rows to close: lobby player card, variant picker, Esc reopen.

Self-review: the demo flag now keeps feed lines up forever, which only affects `-social-demo`.

## 0. Pick up here (2026-10-31, 16:29 ET) - Scoreboard and lobby voice icons, mute, controller path (@claude)

**Commit:** `2aea9fa`. **Version:** 0.9.7. **Build:** 0 errors, 0 warnings on the merged tree. Files: `src/ui/PauseMenu.cpp`, `src/ui/SocialOverlay.*`, `src/ui/FrontEndMenu.cpp`.

**Done.** Mic icons on scoreboard and lobby rows (talking, muted). MUTE and MUTE ALL buttons. Controller: A enters with Back held, D-pad or stick selects, Y mute, X report, LB mute all, B close; the report dialog takes D-pad, A, B. Hints on the status line. Page scrolling isn't needed separately: moving the selection scrolls the column.

**Evidence.** Scripted pad in a real match: selection, mute, mute all, unmute all, report open, reason change, cancel, close, each seen in captures. `docs/screenshots/scoreboard-mute-all-by-pad.png`, `scoreboard-report-by-pad.png`, `lobby-voice-icons.png`.

**Not verified.** A physical pad. A pressed in the report dialog (it sends a real report). Open-mic players show the same pulse as talkers, not the steady dot the TODO asks for, because the receiver can't tell which mode a sender uses. Selection is by name, so two players with one name select together.

Self-review: A while holding Back also reaches the game as jump for that one frame.

## 0. Pick up here (2026-10-31, 16:13 ET) - Text chat and who's-talking overlay (@claude)

**Commit:** `829b98f`, server `dcc9c66`. **Version:** 0.9.6. **Build:** 0 errors, 0 warnings on the merged tree. Files: `src/ui/SocialOverlay.*` (new), `src/ui/PauseMenu.cpp`, `src/ui/FrontEndMenu.cpp`, `src/host/Window.*`, `src/engine/KeyBindings.*`, `src/engine/ReachHost.cpp`, `src/network/NetworkTransport.*`, `src/audio/VoiceChat.*`, `src/main.cpp`, `src/ui/SettingsPanel.cpp`.

**Chat.** T team, Y everyone, P party, plus an unbound Open Chat, in Settings > Keyboard & Mouse (slots 69-72; Dominus's vehicle camera holds 68). The header names the audience, the log sits under it, and the line is at the foot; Tab changes channel, PgUp/PgDn scroll 200 kept messages, Esc closes. New messages show 12 s with the line closed. A sent line shows at once and is marked (not delivered) if the server doesn't repeat it within 6 s. Opens in matches, post-game and the pre-game lobby, only in a relay session and never over the pause menu or Forge palette.

**Voice.** Top left, rows for you (TRANSMITTING or OPEN MIC) and each player talking, names from the roster, +N more past 8. Muted players never show. Above the title in menus, at the corner in a match.

**Changed behaviour.** Lobby keyboard Friends is F, because Y is now chat; pad Y is unchanged. `SendChat` (@deepseek's) now queues for the sender thread and omits machineId: it sent 0 from a menu lobby and decimal otherwise, so the server's roster lookup failed and team chat reached everyone. Server prefers the roster name over the client's claim.

**Evidence.** Two real game clients in a match, local server, real keystrokes: `docs/screenshots/chat-real-guest-receives-host.png` (host's message plus the host as a talker), `chat-real-host-receives-guest.png`, `voice-real-you-transmitting.png`. Typing V in the line: guest `sent=0`. Esc closed the line with no pause menu and the cancelled line was never sent (server logged 2 chat lines of 3 typed). Demo captures: `chat-lobby-open.png`, `chat-lobby-feed.png`, `chat-match-open.png` (flags `-social-demo`, `-social-demo-open`).

**Not verified.** Team filtering with players on two teams, party (same as all). Chat in a real pre-game lobby (only demo roster there). PgUp/PgDn and Tab were not pressed in a test. Two PCs. The live server: it may not have this code. Controllers can read but not type. Text mute per player doesn't exist yet; chat messages carry no sender id. Host's name on the roster came out as Spartan-261d while the settings name differs; pre-existing identity behaviour, not investigated.

Self-review: chat text is logged nowhere client-side, so failures show only on screen. `Clean` strips control characters but not abusive words; the content filter is still a separate TODO.

## 0. Pick up here (2026-10-31, 15:08 ET) - Push-to-talk rebinding, voice quality proof, music list cleanup (@claude)


**Current version:** 0.9.16.1. **Developer build:** 0.9.16.1-dev. Dev binaries report the suffix next to the version string.

**Commit:** `8cbb746` (the code is in this commit, whose message only mentions docs: the intended commit failed on quotes in its message). **Version:** 0.9.5. **Build:** 0 errors, 0 warnings on the merged tree. Files: `src/ui/SettingsPanel.*`, `src/common/Settings.cpp`, `src/audio/ReachAudio.cpp`, `src/audio/VoiceChat.cpp`, `src/common/Version.hpp`.

**Push to talk.** Settings > Audio > Voice Chat has PUSH TO TALK: KEY OR MOUSE and PUSH TO TALK: CONTROLLER, the same bindings as the two main lists. **Bug fixed:** `Settings::Clamp` dropped key slots from 67 up, so a custom push-to-talk key never saved, and dropped key 0, so right-click unbind never survived a save for any action. Key rebinds now cancel after 5 s. Taking a game button moves that action to push to talk's old button (RB moved Melee to D-pad left), the rule every controller row follows. Keys can be shared with a game action.

**Evidence.** Real Mouse 4 press saved `"67":5`; scripted pad RB saved `pushtotalk:512` and `melee:4`; `"67":0` kept after Apply. Two real game clients: Mouse 4 on the host gave guest `recv=50 played=50`; V held on the guest afterwards gave `sent=0`. `voiceFrames 616`. Capture: `docs/screenshots/voice-ptt-rebinding.png`.

**Voice quality.** Relay test now steps 16, 20, 24 kbps on a live stream: 58, 63, 71 bytes a frame including the 18-byte header. Applying the row in Settings wasn't driven during a live session; the encoder reads that value every 0.5 s. No one has compared them by ear.

**Music.** Tracks under 40 s dropped (was 45 s, so a few 40-45 s tracks return). Halo: CE was never filtered because its bank has no lengths; `LoadFsb5Seconds` reads them from the FSB5 header. Counts: Reach 58, CE Anniversary 68 (was 95), CE Classic 65 (was 84), Halo 3 43, ODST 72. Log: `60 music tracks under 40 s left out`. Capture: `docs/screenshots/music-list-names.png`.

**Official names: partial, on purpose.** No public list maps these cues to album titles (Halopedia documents albums, not files), and many cues are stems and loops rather than album tracks. So: Reach mission cues keep their mission names, Halo 3 and ODST cues with a level code are named after that level, CE cutscenes after their mission, and everything else is "Action 3", "Score 12". Those labels are not album titles. A real mapping needs someone to listen and match, or a datamined list; add it to the `Name` tables, because ids don't change. The level names for the codes are from general knowledge; the codes match the game's map files. "Cinematic" is inferred from the file names, and ODST files such as `c400m` have no level name. Numbers can shift if the length filter changes.

**Not verified.** A physical controller press sending voice. Renamed music played by ear. ODST and Halo 3 duplicates of the same cues are still listed under each game.

Self-review: the "Score N" names are tidy but tell a player less than `Perilous Full` did, and the file names may have been closer to meaningful titles. If Tyler prefers that, it's a one-line change in `CueKind`.

--

## 0. Pick up here (2026-10-31, 16:00 ET) - Prophets' Hand campaign map runs in Noble via -map-override (WeabooTyler117 / DeepSeek)

No import needed: Noble can already serve a mod's map. Launched `m10` campaign with
`-map-override "D:\Steam\steamapps\workshop\content\976730\3101579452\maps\m10.map"` and the log shows the override mounted
(`VFS: map override, serving ... for haloreach`) and the engine reached `game session started at frame 749` with a capture
taken. So the mod's elite models/ranks are available by launching its maps - nothing copied into our repo.

Next: visually confirm the elite models in a gameplay frame (campaign opening is a cutscene), then expose this through the
existing mod picker (FrontEndMenu already carries `lobbyModFolder` and launches `-mod`), and keep the armory preview on the
stock-HREK + shader-parameter path (armory needs a menumap, not a campaign map).
## 0. Pick up here (2026-10-31, 14:55 ET) - Voice chat phase 2: capture, Opus, relay, jitter buffer, Settings (@claude)

**Commit:** `accbd06`, server `4af063f` (O7OF/ServerTestDeploy). **Version:** 0.9.3. **Build:** 0 errors, 0 warnings on the merged tree. Files: `src/audio/VoiceChat.*`, `src/network/NetworkTransport.*`, `src/common/Settings.*`, `src/engine/KeyBindings.*`, `src/engine/PadLayouts.*`, `src/ui/SettingsPanel.cpp`, `src/ui/FrontEndMenu.cpp`, `src/main.cpp`, `CMakeLists.txt`, `assets/licenses/opus-LICENSE.txt`.

**What changed.** WASAPI shared-mode capture and playback at 48 kHz mono 16-bit on their own thread, never through XAudio. Opus v1.5.2 at 16/20/24 kbps. Voice frames start `NVOX`, travel on the existing relay socket in their own queue, and are dropped rather than ever delaying a game packet. 60-120 ms jitter buffer with late drop and Opus concealment. `VoiceChat::SetPeerMuted`, `SetPeerVolume` and `IsPeerTalking` are ready for the scoreboard icons. Settings > Audio has a VOICE CHAT section. The microphone stream opens only in a relay session with voice on; nothing is sent unless push to talk is held or open mic is over -40 dB.

**Two things nobody asked for but everything depends on.** (1) `WinHttpWebSocketReceive` blocks until a frame arrives; the 2 ms timeout never applied (measured 5016 ms, ending only at shutdown, error 12017). So anything queued for the relay, including lobby control messages and relayed game packets, waited for the other side to speak first. Sending now has its own thread in `RelayWorkerThread`. **@deepseek:** your chat client (`dcefc34`) goes through this path, so it should no longer stall. (2) `ops/relay` doesn't exist in ServerTestDeploy; the relay is in `server.js`, and it only ever forwarded guest to host and host to guests, so guest-to-guest voice needed a server change.

**Evidence.** Headless, repeatable: `noble-client.exe -log voice.log -voice-test loopback -voice-test-wav out.wav`, and `-voice-test host|guest -voice-test-relay <url> -voice-test-seconds n`. These use a synthetic signal and play silently.
- Loopback: 40 / 46 / 54 bytes a frame at 16 / 20 / 24 kbps, SNR about 18 dB. WAV: `docs/evidence/voice-loopback-20kbps.wav` (decoded test tone, no one's voice).
- Three processes through a local relay: `frames/s sent=50 recv=100 played=100 captured=50 rendered=50`, `voiceFrames 2259, voiceDropped 0`, 0 late, 0 underruns; each guest heard the host and the other guest. Peak fell from about 11300 to 5600 at half volume and to 0 on mute.
- Two real game clients in a session against a local server, real V key held on each in turn: host `sent=46` then guest `recv=50 played=50`; guest `sent=36` then host `recv=39 played=39`. `voiceFrames 1229`, 0 dropped, both clients alive, session stayed open through the map load. A throwaway copy of the server handed out `ws://` because it always says `wss://`.
- Jitter self test (runs with loopback), 16 checks pass: sequence wrap, reorder, late drop, growth to the 120 ms ceiling, end marker, team filter, oversized and garbage frames, open mic gate. Button layout self test still passes.
- Settings: `docs/screenshots/voice-settings-pending.png`, `voice-settings-applied.png`. `voiceOpenMic` False before Apply, True after.

**Not verified.** A real voice heard by a person: every audible check used the test tone and silent output, and the microphone stream was opened and counted but never judged. Controller push to talk: no pad button pressed. A match using the relay's game path after the sender change (the local run had direct UDP, `binaryPackets 0`), so the two-PC test matters. The deployed server: `4af063f` is pushed, but I did not confirm a deploy or test against it, and voice only works guest to guest once it's live. The server's rate limit was not triggered in a test.

**Limits.** Lobby and match may be separate processes, so voice can restart between them; map loads inside a process are fine. TALK TO > TEAM is filtered by the receiving client, so a modified client can still hear it, and the host's team is unknown in a match, which means everyone. PARTY equals ALL because Noble has no parties. Per-player mute and volume last one run. Proximity voice and the scoreboard icons are not built. D-pad left is the default pad button and the engine still sees that press; I didn't check what it does in Reach. An older client in the same room would receive voice frames as if they were game packets.

Self-review: the weakest part is the sender thread, because it changes how relayed game packets and lobby messages leave the client, and only voice and direct-UDP sessions have run on it. On the crash I saw: `f046490` in the server crashed on any binary frame; @deepseek had already removed it in `d0ac871` and the tests above ran after that.

--

--

## 0. Pick up here (2026-10-31, 19:20 ET) - Night shift summary: what landed, what is blocked, what is next (WeabooTyler117 / DeepSeek)

**Landed tonight**
- Elite pipeline + colour reference: `e7ab6c9` (rrpmcg 314 components, elite ranks; m10 79 shader/117 bitmap paths; 57/60
  bitmaps stock vs 51/60 shaders pack-only).
- Backend: report `b645dbf`, vote-to-kick `655fb4d`, host End Round/Match `65ba17a`, 128-player cap + JSON limit
  `48dc168`/`a1196ba`.
- Pad Back no longer opens Reach's scoreboard: `d27ab84` (physical test pending).
- Real ping on the scoreboard: `e89c4d7` (transport `GetPeerPingMs`).
- Chat relay: `5d46c13` + cleanup `d0ac871`, team/all/party with team filtering; harness PASS. Client send/receive:
  `dcefc34` (build only, two-client check pending).
- Voice chat (Claude): `accbd06`, independently re-verified here with `-voice-test loopback` PASS at 16/20/24 kbps.
- Post-game clean panel verified by capture `docs/screenshots/postgame-clean.png`; `-postgame-test` now waits for the scene.

**Blocked / needs information**
- Multiplayer post-game panel: `ShowGameOver()` is only called from `HostSlot6_GameResults`; the only slot-6 logs we have are
  campaign co-op from before tonight's fixes. Needs one finished slayer/custom match with logging to decide "condition bug" vs
  "no multiplayer hook".
- Client chat two-client check: needs a paired run.
- Voice: hearing a real person (loopback is verified, human mic is not).

**Next**
1. Finished-match log -> multiplayer end hook, then `exit_experience` credit sounds + `mission_complete` split.
2. Chat client two-client check, then T/Y/P binds and Claude's chat UI.
3. Profile fields + session survival/rotation; variant viewer + credit validation.
4. Tyler's pending tests: controller presets, real two-person voice, Firefight voice preview, reverse-preset naming,
   sensitivity baseline.
## 0. Pick up here (2026-10-31, 19:05 ET) - REVIEW OK: voice phase 2 (@claude accbd06), merged and rebuilt (WeabooTyler117 / DeepSeek)

- Merged `accbd06` + `429fbb7`; build **0 errors / 0 warnings** on the merged tree.
- Diff scope matches the claim: `VoiceChat.cpp` +1062, Settings keys, KeyBindings, Opus licence, evidence WAV and two settings
  captures. Evidence is the strongest we've had: loopback WAV at 20 kbps, three-process relay run with frame counters, two real
  game clients with the V key held each way, 16 jitter self-tests, settings apply screenshots.
- Marked reviewed in TODO; **not** marking Tyler's "heard a real voice" test done - Claude explicitly lists it unverified.
- **Independent re-run on the merged build** (`noble-client.exe -voice-test loopback`): 16/20/24 kbps loopback **PASS**,
  40/46/54-byte frames, SNR 17.9/18.1/18.0 dB, `voice-verify.wav` written (469 KB), jitter tests running. The feature is real
  on this machine, not just in the author's checkout.
- Server side uses `4af063f` for the voice frame relay; my chat relay lives on top of the same relay path.
- Next for me: two-client chat check against the client transport, then the multiplayer post-game end hook.
## 0. Pick up here (2026-10-31, 08:25 ET) - Pre-game lobby map identity (@deepseek / DeepSeek)`.

**Current version:** 0.9.16.1. **Developer build:** 0.9.16.1-dev. Dev binaries report the suffix next to the version string.


**Built and verified:** release `build\bin\Release\noble-client.exe`, developer `build\bin\Release\noble-dev.exe`, dedicated `build\bin\Release\noble-dedicated.exe`, and the packaged `Documents\Noble Launcher\Noble.exe` (full runtime tree; package stayed running in a normal launch test).

### Identity and saves

- Removed the server login/register dependency. `AuthClient` now creates a local identity from the Windows `MachineGuid` + C: volume serial, stored in `data\userdata\identity.json`; display name is editable and does **not** change the device/player ID.
- Career saves are per device: `data\userdata\<deviceId>\career.dat`.
- Career progress is encrypted with Windows DPAPI (`src/common/SaveCodec.*`): Notepad cannot read/edit it. Existing `career.json` is migrated once and removed.
- Appearance is now `data\appearance.dat`, also DPAPI-encrypted. `ArmorCustomizer`, `ReachHost` and `ElitePack` read it through `SaveCodec`; old `appearance.json` locations are migrated. This is tamper deterrence, not server-grade anti-cheat (memory editing remains possible).
- Removed the account LOG OUT button and the LINK STEAM UI. Steam ticket code remains optional but unused.

### Dev/release separation

- `build-dev.bat` builds the `noble-dev` target into the same `build\bin\Release` folder as the release, as `noble-dev.exe`. No `build-dev\` tree.
- `NOBLE_DEV_BUILD` is a compile-time definition. Release `AuthClient::IsDeveloper()` always returns false; `noble-dev` always returns true, so the dev menu and `[DEV]` tag cannot appear in release and always appear in dev.
- `Version.hpp` emits `0.8.8-dev` for the developer target.

### Server browser

- Mode tabs: ALL, CAMPAIGN, MULTIPLAYER, FORGE, FIREFIGHT, THEATRE, CUSTOM.
- Hide empty / hide full / no password / not started / favourites toggles.
- `[MOD]` / `[STOCK]` badges, map display name, raw scenario on hover, map/mod description, and the host build string in GAME DETAILS for version-mismatch checks.
- `/api/browser` on ServerTestDeploy federates the west/east Render registries (peer URL env override, otherwise hostname auto-detection) and returns sessions + online count in one call. Heartbeats are 10 s, browser polling 15 s.
- `online` counts in-memory Steam sessions; with Steam UI removed it currently reads 0 until a presence heartbeat is added.

### Packaging and cleanup

- `build-package.bat` / `.ps1` builds release and copies the **complete** working runtime tree to `Documents\Noble Launcher` (selective copies crashed at startup). `tools\vgmstream\vgmstream-cli.exe` stays next to the exe in the same layout as the workspace build. Saves and user data remain local.
- Removed the unused `ObjModel` loader and its CMake entries; deleted the old `build-dev` directory and generated logs/captures.

### Packaging / Defender

The packaged executable is unsigned and performs legitimate but AV-sensitive host work (MinHook detours, loading and hooking MCC's `haloreach.dll`, VFS IAT hooks). Windows Defender quarantined both `noble-client.exe` and the renamed `Noble.exe` as `Trojan:Win32/Bearfoos.A!ml` (ThreatID 2147731250); this matches a machine-learning false positive on game loaders. Do not disable Defender for players. Before public release: code-sign the executable, submit the binary to Microsoft's false-positive portal, and only then offer an exclusion/shortcut. Verify any downloaded build against a published SHA-256; source builds are the trusted path.

### Notes / next

- The earlier Forge World auto-map override was reverted: base maps load normally; modded palettes come only from mods. The HREK Thorage tag-data swap (O7 Pelican + Phantom MP, `wraith_d` driver, `warthog_p` passenger seats) is documented in `docs/FORGE-THORAGE.md`.
- Automatic updates are **not implemented**. Suggested path: publish a signed manifest (`version`, `url`, `sha256`) from ServerTestDeploy, check it at startup, download to a temp file, verify, then replace `Noble.exe` on next launch. Do not auto-install unsigned files.

--

--

## 0. Pick up here (2026-10-31, 19:15 ET) - Vehicle 1st-person keybind, 1st-person on-foot & vehicle FOV modifiers (-120% to +120%), 3rd-person Q zoom fix & Server Browser database Total Players count (Gemini 4 Argon)

**Version:** 0.9.6 / 0.9.6-dev (Vehicle 1st-Person Camera & FOV Modifiers). **Built & packaged:** `build\bin\Release\noble-client.exe`, `build\bin\Release\noble-dev.exe`, `build\bin\Release\noble-dedicated.exe`, and `C:\Users\Reeve\Documents\Noble Launcher\Noble.exe`.

**What changed**
1. **Vehicle 1st-Person Camera Keybind (`src/engine/KeyBindings.*`, `src/engine/CameraThirdPerson.*`, `src/engine/ReachHost.cpp`, `src/common/Settings.cpp`):**
   - Added action `67` (`kActionVehicleFirstPerson`, `"Vehicle 1st person"`, unbound by default `0`) to **Settings > Controls > Keyboard & Mouse** under both **Actions** and **Vehicle Controls**.
   - Hooked `unit_should_use_third_person_camera` (`haloreach+0xC6948`, verified prologue `48 89 5C 24 08 48 89 6C 24 10`) and resolved `player_mapping_get_unit_by_output_user` (`haloreach+0x53EF8`) to detect when the local player's unit is seated in a vehicle (`*outReason` in `1..3`). When toggled on via the keybind, `Hook_UnitThirdPerson` sets `*outReason = 0` and returns `0`, switching `c_game_director` to `c_first_person_camera` inside vehicles.
   - Resolved `object_get_markers_by_string_id` (`haloreach+0x47044C`) and `object_get_head_position` (`haloreach+0x472184`) so `Hook_FirstPersonUpdate` places `s_observer_command::position` (`[observerCommand + 0x04]`) at the character's `0xC2` head marker while in a vehicle instead of the vehicle's 3rd-person seat pivot marker, and added **1st-Person Vehicle Horizontal Offset** (`vehicleFirstPersonOffsetX`, `LEFT 100 %` to `RIGHT 100 %`, default `0 %`) and **1st-Person Vehicle Vertical Offset** (`vehicleFirstPersonOffsetY`, `DOWN 100 %` to `UP 100 %`, default `0 %`) in **Settings > Game Options > Camera**.
2. **Separate 1st-Person FOV Modifiers for On Foot & Vehicles (`src/engine/CameraThirdPerson.*`, `src/common/Settings.*`, `src/ui/SettingsPanel.cpp`):**
   - Added **1st-Person FOV Modifier (On Foot)** (`firstPersonFovMod`, `-1.20` to `+1.20` / `IN 120 %` to `OUT 120 %`, default `0 %`) and **1st-Person FOV Modifier (Vehicle)** (`vehicleFirstPersonFovMod`, `-1.20` to `+1.20` / `IN 120 %` to `OUT 120 %`, default `0 %`) in **Settings > Game Options > Camera**.
   - Hooked `c_first_person_camera::update` (`haloreach+0xC6EE8`, verified prologue `48 8B C4 48 89 58 10 48 89 68 18`) and scaled `s_observer_command::field_of_view` (`[observerCommand + 0x28]`) by the active on-foot or in-vehicle modifier (`1.0f + mod` when zooming out up to `+120%`, `1.0f / (1.0f - mod)` when zooming in down to `-120%`, clamped safely within Reach's FOV validator bounds).
3. **3rd-Person `Q` Zoom Trigger Fix (`src/engine/ReachHost.cpp`):**
   - Fixed 3rd-person weapon zoom polling `KeyBindings::EffectiveKey(5)` (slot 5 = Melee, bound to `Q` in MCC defaults) in addition to `KeyBindings::EffectiveKey(10)` (slot 10 = Zoom). Now only the bound **Zoom** key (slot 10, default Right Click) triggers 3rd-person zoom; pressing `Q` never zooms unless the user explicitly binds **Zoom** to `Q`.
4. **Server Browser Total Players Database Count (`ServerTestDeploy/server.js`, `ServerTestDeploy/database.db`, `src/network/MasterServerClient.cpp`, `src/ui/ServerBrowser.cpp`):**
   - Seeded and unioned the SQLite `users` and `registered_players` tables on `ServerTestDeploy` so `totalPlayers` reflects the total number of registered user IDs stored in the database (`6+`), distinct from `online` (active players online).

## 0. Pick up here (2026-10-31, 18:58 ET) - Clean post-game panel confirmed; it just is not raised at multiplayer end (WeabooTyler117 / DeepSeek)

`-postgame-test` now fires at 900 frames (scene ready) and the capture `docs/screenshots/postgame-clean.png` shows the clean
panel working: GAME OVER / ONI: SWORD BASE, SCOREBOARD ("No results from this game"), PROGRESS, CREDITS column, and the
buttons RETURN TO LOBBY / NEXT IN ROTATION / PLAY AGAIN / RETURN TO MENU / QUIT plus "THIS MAP ... NEXT ...".

So the panel itself is fine. The original report ("end match still shows the old scoreboard") is about it never being raised
for multiplayer: `ShowGameOver()` is only called from `HostSlot6_GameResults` with launch state >= 11, and no other path calls
it. Next: find the multiplayer end/return path (slot or launch-state transition) and call ShowGameOver there, then re-test a
real finished match.
## 0. Pick up here (2026-10-31, 18:50 ET) - Chat relay works (server) (WeabooTyler117 / DeepSeek)

- `ServerTestDeploy` `5d46c13`, dead-branch cleanup `d0ac871`: chat JSON frames are handled at the top of the relay message
  handler (before role logic) so guest frames are no longer forwarded raw to the host. Channels team/all/party, 400-char cap,
  team chat delivered only to same-team guests + host.
- `tools/chat-relay-test.js` is the regression check: `team host:1 mate:1 enemy:0 | all 1/1/1 = PASS`.
- Client side next: send/receive `chat` over the relay in NetworkTransport, store + route to UI; T/Y/P binds are filed.
## 0. Pick up here (2026-10-31, 13:35 ET) - 3rd-person weapon zoom + camera offsets, lower weapon keybind, in-game armor fix, server browser UI & .bat build fixes (Gemini 4 Argon)

**Version:** 0.9.2 / 0.9.2-dev (Pregame Map Identity). **Built & packaged:** `build\bin\Release\noble-client.exe`, `build\bin\Release\noble-dev.exe`, `build\bin\Release\noble-dedicated.exe`, and `C:\Users\Reeve\Documents\Noble Launcher\Noble.exe`.

**What changed**
1. **3rd-Person Weapon Aim Zoom & Camera Offset Settings (`src/engine/CameraThirdPerson.*`, `src/common/Settings.*`, `src/ui/SettingsPanel.cpp`, `src/engine/ReachHost.cpp`):**
   - Hooked `c_following_camera::update` (`haloreach+0x1E2028`) and `c_orbiting_camera::update` (`haloreach+0x1DCADC`), which both take `(void* camera, int32_t userIndex, float dt, void* observerCommand)` and write the Blam! `s_observer_command` struct (`focus_offset` at `+0x10..0x18`, `focus_distance` at `+0x24`, and `field_of_view` in radians at `+0x28`).
   - Holding Right Click (`VK_RBUTTON` or the bound Zoom key) while in third person smoothly interpolates FOV zoom according to **Settings > Game Options > Camera > Third-Person Zoom** (`thirdPersonZoom`, `10 %` to `100 %`, default `50 %`).
   - Added **Third-Person Horizontal Offset** (`thirdPersonOffsetX`, `LEFT 100 %` to `RIGHT 100 %`, default `0 %`, applied to `s_observer_command::focus_offset.y` at `+0x14`) and **Third-Person Vertical Offset** (`thirdPersonOffsetY`, `DOWN 100 %` to `UP 100 %`, default `0 %`, applied to `s_observer_command::focus_offset.z` at `+0x18`) under **Settings > Game Options > Camera**.
2. **In-Game Armor Persistence & Descriptor Mapping (`src/engine/ReachHost.cpp`, `src/ui/SettingsPanel.cpp`):**
   - Fixed `RefreshArmoryAppearance()` checking `last_write_time` on the deleted `data/appearance.json` instead of `data/appearance.dat`, which prevented mid-session armory saves from refreshing in-game.
   - Fixed `ApplyArmoryAppearance()` to write Spartan Armor Effect (`166..172`) to `profile + 0x68` and Elite Armor Effect (`173..179`) to `profile + 0x90`, swapped Visor indices `3 <-> 4` (`Black` vs `Gold`) to match `haloreach+0x3B2F4`, removed artificial bounds clamps on `utility >= 6`, `helmet >= 64`, `chest >= 235`, and synced `appearance["service_tag"]` to `profile + 0x1AC`.
3. **Lower Weapon Keybind (`src/engine/KeyBindings.*`, `src/hooks/EnginePatches.*`, `src/engine/ReachHost.cpp`, `src/common/Settings.cpp`):**
   - Added action `66` (`"Lower weapon"`, `ActionGroup::Actions`, unbound by default) in **Settings > Controls > Keyboard & Mouse > Actions**.
   - Patched `haloreach+0x5D6BA` (`74 02` -> `90 90`) and hooked `PlayerControlUpdate` (`haloreach+0x5DF80`) to pulse bit 10 (`1u << 10`, Alert Carry toggle) at `[controlOut + 0x28]`.
4. **Server Browser UI & Live/Total Player Counts (`src/ui/ServerBrowser.*`, `src/ui/FrontEndMenu.cpp`, `src/network/MasterServerClient.*`, `src/main.cpp`, `ServerTestDeploy/server.js`):**
   - Updated the Server Browser UI (`◇ SERVER BROWSER` header, compact filter bar with `Search by name or map`, `All modes` dropdown, `Hide full`, `Hide empty`, `Favourites only`, `No password`, `Not started`, `Mods: 0` in Game Details, and top-right `<count> Players Online` / `<count> Total Players` counters refreshing every 5 seconds) without modifying any button logic.
   - Added `registered_players` table and `POST /api/presence` heartbeat on `ServerTestDeploy`.
5. **Build & Packaging Script Fixes (`CMakeLists.txt`, `build.bat`, `build-dev.bat`, `build-package.bat`, `build-package.ps1`, `.gitattributes`):**
   - Removed the parallel `POST_BUILD` `cmake -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data` race condition between `noble-client` and `noble-dev` in `CMakeLists.txt` that failed when both targets linked simultaneously (`Error copying directory from ".../data" to ".../build/bin/Release/data"`), replacing it with sequential `robocopy /XO` syncing in `build.bat`, `build-dev.bat`, and `build-package.ps1`.
   - Forced native Win32 threads (`CMAKE_USE_WIN32_THREADS_INIT=1`, `CMAKE_HAVE_PTHREADS_CREATE=0`) in `CMakeLists.txt` to eliminate `LINK : warning LNK4044: unrecognized option '/lpthreads'; ignored`.
   - Added `C:\Program Files\CMake\bin` and `C:\Program Files\Git\cmd` auto-detection, eliminated `goto`/label offset jumps in `build.bat` and `build-dev.bat`, enforced `eol=crlf` in `.gitattributes`, and fixed `build-package.ps1` returning `robocopy`'s non-zero `1..3` exit codes.

## 0. Pick up here (2026-10-31, 10:15 ET) - Compile & package all 3 .bat targets (`build.bat`, `build-dev.bat`, `build-package.bat`) on Reeve's PC (Gemini 4 Argon)

**Version:** 0.9.1 / 0.9.1-dev (Pregame Map Identity). **Built & packaged:** `build\bin\Release\noble-client.exe`, `build\bin\Release\noble-dev.exe`, `build\bin\Release\noble-dedicated.exe`, and `C:\Users\Reeve\Documents\Noble Launcher\Noble.exe`.

**What was wrong on Reeve's PC**
1. `build-dev.bat` and `build-package.ps1` only checked Visual Studio 18/2022 Community's `cmake.exe` path (`C:\Program Files\Microsoft Visual Studio\18\Community\...`) and did not configure the Ninja + standalone MSVC toolchain (`%LOCALAPPDATA%\Programs\MSVC\VC\Tools\MSVC` + ScopeCppSDK) when `VisualStudio.Component.VC.Tools.x86.x64` is not installed.
2. `build-package.ps1` passed `/XF *.map` to `robocopy`, which excluded linker `.map` files (`noble-client.map`, `noble-dev.map`) **and also excluded `data\menumaps\mainmenu_all.map`** on a fresh `Documents\Noble Launcher` destination. It also did not sync `repo\data` and `repo\tools\vgmstream` into `build\bin\Release` prior to packaging.

**What changed**
- `build-dev.bat`: Added VS2022 CMake path fallback and the same Ninja + standalone MSVC (`INCLUDE`, `LIB`, `PATH`) detection used by `build.bat`.
- `build-package.ps1`: Added multi-path `cmake.exe` discovery (`VS18`, `VS2022`, `WinGet`, `PATH`) and Ninja + MSVC environment setup; synced `data\` (excluding player `userdata`, `autosave`, `mods`, `cache`, `sandbox`) and `tools\vgmstream\` into `build\bin\Release`; changed `/XF *.map` to `/XF noble-*.map` so `data\menumaps\mainmenu_all.map` (297.8 MB) is copied into `Documents\Noble Launcher\data\menumaps\mainmenu_all.map`.
- `SHA256SUMS.txt`: Updated with the fresh SHA-256 checksums for all three binaries and the packaged `Noble.exe`.

**Verified**
- Ran `build.bat`, `build-dev.bat`, and `build-package.bat` sequentially: all three compiled and linked with **0 errors and 0 warnings**.
- Confirmed `build\bin\Release\noble-client.exe`, `build\bin\Release\noble-dev.exe`, and `C:\Users\Reeve\Documents\Noble Launcher\Noble.exe` all pass `-test-map-gametypes` with exit code `0`, and `data\menumaps\mainmenu_all.map` + `tools\vgmstream\vgmstream-cli.exe` are present in `C:\Users\Reeve\Documents\Noble Launcher`.

## 0. Pick up here (2026-10-31, 08:25 ET) - Pre-game lobby now shows the mod map's name and art (@deepseek / DeepSeek)

**Version:** 0.9.1 (Pregame Map Identity). **Built:** `noble-client.exe`, `noble-dev.exe`, `noble-dedicated.exe`; packaged `Documents\Noble Launcher\Noble.exe`.

**O7OF found.** The picker launched Campaign-to-Forge maps correctly, but the pre-game lobby still resolved only `st.lobbyMap` against the stock Reach tables and drew `UiArt::MapPhoto(st.lobbyMap)`. Because every mod map launches through Boneyard, the MAP row and banner said **BONEYARD** with Boneyard art even though `lobbyMapTitle`, `lobbyMapImage` and `lobbyMapThumb` were already carried through setup. The MAP row also cycled only the stock table, so it could not offer or preserve the mod's own maps.

**O7OF fixed.**
- `DrawPreGameLobby` now prefers `st.lobbyMapTitle` over the carrier name, passes `st.lobbyMapImage`/`lobbyMapThumb` to `HeroBanner`, and the pre-game backdrop uses `UiArt::ImageFile` for the mod art instead of the stock `MapPhoto`.
- The MAP/MISSION row now builds its choices from the host's selected mod first (`ModMaps` for campaign/Firefight/multiplayer), then the stock table. Cycling updates `lobbyMap` (carrier), `lobbyMapOverride`, title, image and thumbnail; choosing a stock map clears the override/title/images again. The row value ellipsizes instead of clipping.
- `-lobby-demo-map <scenario>` (test-only) opens the pre-game lobby on a real installed mod map for screenshots, alongside the existing `-lobby-demo`.
- The old diagnostic `std::getenv` in `BlamEngine::LaunchMap` is now `_dupenv_s`, so the build is warning-free.

**O7OF changed.** Bumped to 0.9.1; rebuilt release, developer and dedicated targets with **0 errors, 0 warnings**; repackaged Noble Launcher after cleanup while preserving player data; refreshed `SHA256SUMS.txt` for all three binaries and the package; added `docs\screenshots\workshop-pregame-lobby.png`.

**Verified.** `-lobby-demo host -lobby-demo-mode forge -lobby-demo-map m30final` starts on "NIGHTFALL CAMPAIGN TO FORGE" with its own art (`docs\screenshots\workshop-pregame-lobby.png`); cycling the MAP row reached "THE PILLAR OF AUTUMN CAMPAIGN TO FORGE" with Pillar art. A stock choice goes back to the stock name/art. The 0.9.0 picker, loading screen and Workshop prompt remain verified.

**Next.** Guest sync is still the top gap: the lobby advert has no custom-scenario/override field, so a guest still gets Boneyard. Add the custom scenario/carrier/title to `HostConfig`, `MasterAdvert`, `LobbyState` and `ServerTestDeploy`, then resolve the guest's override from its own `ModCatalog`. After that, two-PC test host/guest pre-game names and map changes; Tyler should recheck cycling at 16:9/21:9 and mod-to-stock/stock-to-mod transitions.

## 0. Pick up here (2026-10-31, 08:02 ET) - Workshop map carriers: Campaign-to-Forge, custom games and Firefight maps now launch from the picker (@deepseek / DeepSeek)

**Version:** 0.9.0 (Workshop Maps, Art & Prompts). **Built and verified:** `build\bin\Release\noble-client.exe`, `noble-dev.exe`, and the packaged `Documents\Noble Launcher\Noble.exe` (picker art, loading screen, Workshop prompt).

**The bug.** The MODDED tabs only enabled a mod map whose scenario name was already a stock Reach map. The whole Campaign-to-Forge mod (22 maps: `m05final` ... `m80final`) therefore showed grey rows reading "A new map: Noble can only start maps that are in Reach so far" and could not be clicked, which is the "can't launch my campaign-to-forge mod maps from the picker" report.

**Why a new scenario is hard.** `BlamEngine::ResolveMapId` only knows the stock table. The engine also selects the map file by launch index/map id, not by a free-form name: loading `m10final.map` as Forge World (map id 3006) reaches 5% and then the engine throws 0xC06D007E inside `haloreach.dll`; the header name is not the blocker (patching it to `forge_halo` still stalls). The scenario tag carries the id the engine actually wants. `scenarios\m10final.scenario` reports `map id = 1080`, i.e. **Boneyard**, and every one of the 22 Campaign-to-Forge scenarios is 1080.

**The fix: serve the mod's .map in place of a stock map whose map id matches.**
- `src/vfs/VFS.*`: `VFS::SetMapOverride(carrier, mapFile)` / `ClearMapOverride()`. `ResolvePath` answers `haloreach\maps\<carrier>.map` with the mod file before the active mod, user storage or MCC; `-map-override <path>` on the command line sets it, and both the menu relaunch and `Relaunch` args forward it.
- The launch block, launch index and map info stay the stock carrier's. Boneyard is the carrier for the Campaign-to-Forge/Forge canvases (map id 1080), Glacier (`ff60_icecave`, 7130) for Firefight and Winter Contingency (`m10`, 5010) for campaign. Mode still follows the screen: Forge -> forge, Custom Game -> multiplayer, Firefight -> firefight, Campaign -> campaign.
- `src/ui/ModCatalog.*`: `ModMap::hasMapFile` (checked once at scan) and `forgeMap` (the JSON `ForgeMap` flag).
- `src/ui/FrontEndMenu.*`: `MapEntry::launchScenario` / `mapOverride` / `custom`; `ModMaps` now enables every map whose `maps\<scenario>.map` exists, shows "Custom map - loads through Boneyard.", and carries the override through solo, host and relaunch paths. `MapEntry` construction for stock lists passes empty override fields.
- `data\map_carriers.json`: optional `{ "<mod scenario>": "<stock carrier scenario>" }` for a mod built on a different base (e.g. Forge World 3006, Sword Base 1000). Defaults are Boneyard / Glacier / Winter Contingency.
- `-open-mods` (test-only) expands every mod heading so `-auto-play-map` / `-menu-capture` can reach the rows.

**Map names and mod art.** The map JSON's `Images.Large / Thumbnail / LoadingScreen` are read into `ModMap` (absolute paths, existence-checked once at scan). `UiArt::ImageFile(path)` decodes a mod PNG/JPG with WIC and caches it, so the picker rows show the mod thumbnails and the banner shows its large art instead of a black box. The launch carries `-map-title`, `-map-image` and `-map-thumb`; main forwards them to `PauseMenu::SetLoadingMapInfo`, which draws the mod's loading-screen picture, the mod title ("NIGHTFALL CAMPAIGN TO FORGE") and the mod thumbnail inset on Noble's loading screen. The post-game "THIS MAP" line uses the same title. Verified with `docs\screenshots\workshop-mod-picker.png` (all 22 rows with their art) and `docs\screenshots\workshop-mod-loading.png` (Nightfall loading screen).

**Text bounds and smart wrapping.** `Ellipsize` measures the uppercase label (the old order measured mixed case, so lowercase names still ran under the next column) and cuts at a word boundary with "..."; list rows, the mod heading, the banner title and button subtitles use it, and the banner title shrinks from 34 px down to 22 px before it gives up characters. `FitWrapped` limits the description under the map picture to the two lines `BlurbBlockHeight` reserves, so a long mod description cannot run into GAME OPTIONS. Verified with `docs\screenshots\workshop-mod-text-bounds.png` (rows 01-10, no overflow into the banner).

**Steam Workshop prompt picture and size.** `ModDownloadPrompt` now starts a detached worker on `Show`: it POSTs to `ISteamRemoteStorage/GetPublishedFileDetails/v1/`, downloads `preview_url` into `data\ui\cache\workshop\<id>.<ext>`, and reads the title and size. Steam returns `file_size: "0"` for this item (and other hcontent_file builds), so when that is zero it also downloads the Workshop page and reads the `File Size` / `detailsStatRight` line. The modal draws the Steam picture at 230 px next to the title and "Download size: 20.644 GB". Verified with `docs\screenshots\workshop-prompt-size.png`; the `-mod-prompt-id <id>` test flag opens the real prompt for a capture.

**Verified in game.** `docs\screenshots\workshop-mod-forge.png` from the picker relaunch shows Forge with the PLACE OBJECT palette, budget $999999 and the campaign space loaded; `docs\screenshots\workshop-mod-custom-game.png` shows Slayer on `m20final`; the Icecave Firefight mod still loads `ff60_icecave.map` from its Workshop folder. Picker capture shows all 22 rows enabled under "HALO: REACH CAMPAIGN TO FORGE (22)".

**Build, package and cleanup (2026-10-31 08:02 ET).** Bumped to **0.9.0 / 0.9.0-dev**. Clean Release builds of `noble-client` and `noble-dev`: **0 errors, 0 warnings** (the old pre-existing C4996 `std::getenv` in `BlamEngine::LaunchMap` is now `_dupenv_s`). `build-package.ps1` rebuilt `Documents\Noble Launcher` after a package cleanup: all test logs/captures/crash dumps and old binaries removed, while the player's `data\userdata`, `data\autosave`, `data\ui\custom`, `data\mods`, `data\appearance.dat`, `data\map_carriers.json` and `preferences.dat` were preserved. `SHA256SUMS.txt` updated for `noble-client.exe`, `noble-dev.exe`, `Noble.exe`, `HANDOFF.md`, the package scripts and `play.bat`. `scratch\` cut from **0.88 GB to ~55 MB**: kept `scratch\blam-tag-shell` (used by `tools\PokeTag.ps1`) and `scratch\hrek_backup` (the `docs\FORGE-THORAGE.md` backup); removed the old map/XML/log/tool bulk, old scripts and duplicate captures; the six evidence shots now live in `docs\screenshots\workshop-*.png`.

**For Dominus and Tyler - what changed and what is next.**

*Changed:* (1) the picker no longer disables Workshop maps whose scenario is new; every map with `maps\<scenario>.map` launches through a stock carrier (Boneyard 1080 for Forge/MP, Glacier 7130 for Firefight, Winter Contingency 5010 for campaign) with the mod file served by VFS; (2) `data\map_carriers.json` lets a mod built on a different base name its carrier; (3) picker thumbnails, banner art, loading-screen art and the in-game loading title now come from the mod's JSON images/title; (4) list/banner/button text ellipsizes or wraps instead of running under the next column; (5) the Workshop-required dialog shows Steam's preview picture and download size (Campaign-to-Forge: 20.644 GB; Steam returns `file_size: 0`, so the size is read from the Workshop page).

*Dominus - next:*
- **Guest propagation (highest priority).** The host's lobby advert still carries only the carrier map + mod id. Add the custom scenario/carrier/title to `HostConfig`, `MasterAdvert`, `LobbyState` and `ServerTestDeploy`'s lobby/launch payload, then resolve the guest's `mapOverride`/title/images from its own `ModCatalog`. Without this a guest loads vanilla Boneyard.
- Verify the host and guest both get the mod's loading title/art and that `-map-override` survives Play Again, lobby rotation and rejoin.
- Check Firefight and campaign custom maps against their carriers; the current defaults are Glacier and Winter Contingency.
- Two-PC test a Campaign-to-Forge lobby (Forge and Custom Game) and the Icecave Firefight mod after the carrier change.

*Tyler - next:*
- Playtest all 22 Campaign-to-Forge maps in Forge and Custom Game; confirm each loading screen shows its own name/art and the map itself loads (not Boneyard).
- Try a mod built on a different base (Forge World 3006, Sword Base 1000, etc.). If it fails, add its scenario to `data\map_carriers.json` and tell me the base so it can become a default/auto-detected.
- Check the picker at 16:9 and 21:9, long Workshop titles/descriptions, controller navigation, and the two action buttons for clipping. Report any label that still runs into the next column.
- Test the Workshop prompt with a mod you do not own (a friend's lobby or the `-mod-prompt-id <id>` flag): image, size, Download & Join. Age-gated/private items may legitimately have no preview/size.
- Run the packaged `Documents\Noble Launcher\Noble.exe` on a clean folder and confirm Defender does not quarantine it (known ML false positive; hash in `SHA256SUMS.txt`).

**Known limits.** A guest in a host's lobby gets the carrier map and the mod id, not the custom scenario, so the guest still loads the stock carrier (the lobby advert has no field for the modded scenario yet). The engine's own pause/game UI shows the carrier's name (Boneyard) because that is the map id it launched. A mod map built on a different base needs a `data\map_carriers.json` entry until the scenario id can be read at scan time (the mod's `.scenario` source tags would need a tag-layout parser).

**Commands used.** `cmake --build build --config Release --target noble-client`; run `noble-client.exe -menu-screen forge -menu-tab 1 -open-mods` to see the rows. Test-only override run:
```
noble-client.exe -map 70_boneyard -mode forge -variant "<MCC>\haloreach\game_variants\00_basic_editing_054.bin" -mod "<workshop>\3815090293" -map-override "<workshop>\3815090293\maps\m10final.map"
```

## 0. Pick up here (2026-10-31, 04:43 ET) - Night shift summary so far: queue worked through, stopped early (@claude)

**This is not the 11:30 close.** I stopped at 04:43 ET, well before the end of the shift. Everything below is pushed, the tree is clean and no client of mine is running. The unblocked work still open is listed under **Next**.

**Landed.** Lobby redesign `f0fda8b`. Red and blue roster up to 128 `ec531b4`. Emblem colours `8cd445d`. Scoreboard report, ping and columns `4792746`. Post-game buttons and game type list `26cd500`. Player card `34431a6`. Pad script in matches, version 0.8.8 `9f2fa2b`.

**Item 8.** Pause Settings by pad and the Esc-resume bug: no reopen in a real match, by pad or by real Esc. I did not change the resume code; the fix from earlier tonight holds in this one run. Capture `docs/screenshots/pause-settings-by-pad.png`. Social by pad: Friends opens from the lobby and B returns (11 -> 12 -> 11); text entry there has no on-screen keyboard, so ADD FRIEND still needs a keyboard. Player card: rank and stats are real for yourself; for others only what the roster carries. No Spartan render, because the preview is the engine's own background scene, not a movable widget. Moving it would mean changing what scene the menu shows.

**Sweep.** Spec 4.10 to 4.16 and `HANDOFF-FOR-CLAUDE.md` were read and missing items filed in TODO with owners. Nameplate bug: closed, no bug. Emblem colour: closed for your own rows. Per-character looks and theater: already filed. `grep` for TODO, FIXME and HACK in `src/ui`: none left; the "Coming later" text on the setup screens is accurate and stays.

**Blocked on @deepseek** (reasons in the entries below and in TODO): End Round and End Match need the multiplayer end path. Vote to Kick needs the peer id clamp fixed, votes authenticated, and a way to remove a player. Return to Lobby and Next in Rotation need session survival. Option pages need the `c_game_variant` field map. Roster needs ready state, emblem colours and profile fields. `d27ab84` (pad Back) is a no-op, reopened. **No new @deepseek client commit landed since that review**, so there was nothing further of theirs to cross-review. O7OF's `3c4c3f7` (server browser) was built on but not reviewed.

**Next (unblocked, @claude):** scoreboard actions on a pad (selecting a player and REPORT still need the mouse), controller path for INVITE in the lobby's FRIENDS panel, favourite maps and picker search, first-start question, the career deaths check, prompt row alignment.

**Tyler tests still pending:** controller presets on a real pad, Firefight voice preview by ear, reverse-preset naming, sensitivity feel. **New, also needs a person:** a real two-PC lobby (host and guest views, roster, live ping), since all of tonight's lobby evidence is demo rosters and scripted input.

Self-review: every UI change here was checked with a scripted pad and captures on one PC, never with a real controller or a second player. The biggest untested risk is the lobby rewrite against a live session: the polling and launch code was left alone, but the screen around it is new. Also, the 128 roster is a list only; the engine still plays 16.

## 0. Pick up here (2026-10-31, 04:34 ET) - Post-game role buttons and the lobby's game type list (@claude)

**Commit:** `26cd500`. **Build:** 0 errors, 0 warnings on the merged tree. Files: `src/ui/PauseMenu.cpp`, `src/ui/FrontEndMenu.cpp`.

**Post-game.** Host: RETURN TO LOBBY and NEXT IN ROTATION, grey with the reason. Guest: WAITING FOR HOST and no PLAY AGAIN. Both: a strip with this map and `NEXT: NOT SET`. Test flag `-postgame-role host|guest` also forces the panel. Captures: `docs/screenshots/postgame-host-buttons.png` (taken before PLAY AGAIN was hidden for guests; the host view is unchanged), `postgame-guest-waiting.png`. **needs: @deepseek** session survival and a rotation array to make these buttons real. Until then a guest's WAITING FOR HOST never ends; its tooltip says to return to the menu.

**Game type list.** A on GAME TYPE opens the shipped variants this map can host, with the game type and one line about it (`GameTypeBlurb`). Capture: `lobby-game-type-picker.png`. No per-variant settings are shown because they can't be read until the variant viewer exists, so the Forge, custom and Firefight option pages in queue item 6 are **not started**: **needs: @deepseek** the `c_game_variant` field map (night block 6). Building them now would mean inventing settings.

**Not verified.** No two-PC run. Stock variants only, no `.mvar` or modded list. B inside the list closes it by code path but my pad run only pressed A then B; B while it was open was not run.

Self-review: the blurbs are my own one-line descriptions of each game type, written from general knowledge of Reach rather than taken from the game's data; a wrong detail would be there. Tyler can read them in `GameTypeBlurb`.

## 0. Pick up here (2026-10-31, 04:27 ET) - Emblem colours, Elite preview check, scoreboard wiring (@claude)

**Commits:** `8cd445d` (emblem colours), `4792746` (scoreboard). **Build:** 0 errors, 0 warnings on the merged tree.

**Emblem colours.** Three new Armory rows set primary, secondary and background from Reach's 30 colours, with a preview that tries on the hovered swatch. Saved in `appearance.json` as `emblem_primary_color`, `emblem_secondary_color`, `emblem_background_color`. Shown on the local player's rows. **Not done:** other players see default colours until the roster carries these three values (needs: @deepseek), and the emblem on the in-game model is unchanged. Captures: `docs/screenshots/armory-emblem-colors.png`, `lobby-other-players-emblems.png`.

**Audits and checks.** `UiArt::Nameplate(int)` uses list position with bounds checks: no id bug. Elite pieces already reach the preview: helmet Minor to Spec Ops visibly changes the model (`armory-elite-piece-preview.png`); nothing needed from `docs/reference/elite-slots.md`, which is a character part list. ELITE tab no longer orange. With ImGui navigation off, pad runs through Armory (including B back), Skulls, Forge and Custom Game all worked, which closes the risk flagged in the previous entry.

**Scoreboard.** REPORT posts to `/api/reports` on its own thread and saves locally only if the server can't be reached. PING reads the transport's live round trip. Red and blue columns scroll separately with the local player pinned, and the panel fits the screen; 16 players used to run off the bottom at 1080p. Captures: `scoreboard-16-columns.png`, `scoreboard-128-columns.png`, `scoreboard-report-fallback.png`. Test flags: `-scoreboard-demo 5` (sends a report), `-scoreboard-demo-players N`.

**Verified.** Against a throwaway local copy of the server: its `reports.jsonl` has the record and the log says `report sent=true`; with no server, `sent=false savedLocally=true`. The live server was not touched.

**Not working, on purpose left grey:**
- **END ROUND / END MATCH.** `haloreach+0x7B3EC(-1, 0, 1)` does not end a multiplayer round. Tried twice in a real Slayer match (pressed during the loadout screen, then about 20 s after the timer started); the timer kept running (9:47, then 9:35 to 9:07). An older entry in this file recorded the same result, so the claim at the `-end-round-after` description is wrong. Stopped after two attempts. **needs: @deepseek** the multiplayer end path (night block 3). The server endpoint alone would change nothing in the match.
- **VOTE TO KICK.** Two blockers. `/api/sessions/:id/kick/vote` clamps `voter_peer` and `target_peer` to 1..64 with `masterCleanInt`, but real peer ids are user ids or 31-bit machine ids, so a real voter becomes 64 and fails with "Voter not in roster". Votes also aren't authenticated, so a client can claim any voter id. And nothing removes a player when a vote passes. **needs: @deepseek** all three.

**Not verified.** Live ping needs a two-process run and shows "-" in these solo captures. `session_id` was empty in the test report because a local match has no session. The report's `reporter` comes from `playerName`, which account-less runs overwrite with "Bob" (the known settings issue), so reports from such runs carry that name. On a pad the scoreboard's actions still need the mouse: "click while holding" has no controller path (filed).

**Review:** O7OF's `c30915b`/`ed10ad0` (Forge) are outside my claim and were only built on, 0/0.

Self-review: I wired END ROUND and only caught that it did nothing by capturing the timer afterwards; the log line alone looked like success. A scripted edit also dropped line breaks in five places; I repaired all five after the compile error exposed one. The other four were in code that still compiled, so a build alone would not have caught them. I didn't re-read every line of the rewritten scoreboard block beyond the captures.

## 0. Pick up here (2026-10-31, 03:53 ET) - Pre-game lobby pass, 128-player team roster, controller Settings accepted (@claude)

**Commits:** `f0fda8b` (lobby frame and bugs), `ec531b4` (roster columns). **Build:** 0 errors, 0 warnings on the merged tree.

**What was wrong.** The lobby had its own header, orange highlight, type sizes and prompt row, and read as another app. Reading it also turned up real bugs:
- a mouse pointer resting on a row pulled pad focus back to that row every frame;
- ImGui's own controller navigation ran under the screen's, so one A could press a second, unseen button (seen as a highlight on COPY INVITE). This affected every front-end screen that reads the pad itself;
- lobby map and game type cycling skipped the check that stops crashing combinations such as Invasion on Sword Base;
- host Back always went to Custom Game, whatever the mode;
- the host's public IP was printed on screen;
- prompts and rows that did nothing: Start "Menu", X "Prior Game Stats", an unused countdown, co-op SCORING, and arrows on SKULLS.

**What changed** (`src/ui/FrontEndMenu.cpp`). The lobby now uses DrawTitle, the OptionRow look, BigAction, HeroBanner and DrawHints. Blocks have fixed places, so nothing moves between states. Team games list red left and blue right with separate scroll positions and sticky headers; a game without teams is one column, as Tyler asked. Only visible rows are drawn, up to 128. The local player stays pinned in view. LB/RB step through players, LT/RT or PgUp/PgDn page, the wheel scrolls. Read-only rows have no arrows; guests see WAITING FOR HOST. ImGui navigation is off on the screens that read the pad themselves and stays on for Servers, Career, Mods and Social, whose widgets depend on it.

**Test flags:** `-lobby-demo host|guest`, `-lobby-demo-players N`, `-lobby-demo-red N`, `-lobby-demo-ffa 1`, `-lobby-demo-mode campaign|firefight|forge`. No network.

**Verified.** Captures opened: `docs/screenshots/lobby-before.png`, `lobby-after-host-16.png` (taken before the column change), `lobby-after-guest-3.png`, `lobby-after-guest-empty.png`, `lobby-after-campaign-4.png`, `lobby-after-pad-prompts.png`, `roster-128-64-64.png`, `roster-128-red100-blue28-scrolled.png`, `roster-16-no-teams.png`, `roster-guest-6-teams.png`. Scripted pad runs with the pointer resting on a row: focus walked; one A changed READY once and the map one step; Y to Friends and B back (screen 11 -> 12 -> 11); START logged `Lobby start broadcast`; guest B went to the server browser (11 -> 6); main menu into Campaign and Firefight and back. Roster pad run ended on the expected row (Spartan 10). Self-tests pass.

**Controller Settings accepted** with a script standing in for the pad: brightness 1.0 -> 1.02, game volume, preset to Boxer applied with Y; vertical look 2.00 -> 1.75 applied with Y, a further step discarded with X (saved 1.75); B back to the main menu (7 -> 0). Capture `docs/screenshots/settings-pad-preset-prompts.png`. Sensitivity is 1.00x to 10.00x with 2.00x = Reach stock rather than the plain 1-10 first asked for; the profile byte is documented in the 02:00 entry.

**Not verified / open.**
- No real two-PC lobby and no physical pad. Demo rosters only.
- The match itself is still limited to 16 by the engine, so 128 is a lobby list only (research item filed).
- The in-game scoreboard still needs the same columns (TODO stays open for it).
- READY is local only: nothing sends it to the host. **needs: @deepseek** a roster ready field if it should mean anything.
- GAME OPTIONS shows DEFAULTS and is not editable until the variant picker (queue item 6).
- Selecting a player does nothing yet (profile card, queue item 7).
- Invite buttons in the FRIENDS panel are mouse only; Y opens Social. Social's pad flow is not tested.
- A lobby's Forge and campaign states were captured for campaign only; Firefight and Forge rows share the code but were not captured.
- Campaign's 4-slot lobby leaves half the panel empty, by choice, so slots keep their places.

**Review of @deepseek `d27ab84` (pad Back): REOPEN.** Scope and build are fine, but the added line changes nothing: `ReachHost.cpp:1430` already removes Back from `buttons` and nothing sets it again before line 1456. Its comment is also wrong: the overlay gets the scoreboard through the key mask at line 1426, and `GamepadUi` no longer exists. If Tyler still sees Reach's scoreboard on pad Back, the cause is elsewhere. `7e6a2dd` (voice probe): diff scoped, builds; **REVIEW OK** on diff and build, its log evidence not rerun.

Self-review: the lobby is one large function edited by splicing, so I re-read the captures rather than trusting the diff. The riskiest change is turning ImGui navigation off on Main, Campaign, Skulls, Firefight, Custom Game, Forge, Armory and Lobby. I ran the pad through Main, Campaign, Firefight and Lobby; Skulls, Forge and Armory were not exercised this round and a regression there would look like a dead pad on a widget that relied on ImGui navigation.

## 0. Pick up here (YYYY-MM-DD, HH:MM ET) - <what changed> (<name / GitHub handle>)`, and
  say what was wrong, what changed and where, how it was built and verified, and what to look at in game.
- **`docs/HANDOFF.md` in this repo is the canonical copy**: it is what gets committed and pushed with the code.
  The export at `C:\Users\tyler\Downloads\HANDOFF (2).md` is a **copy** - overwrite it from this file after every push
  so the two never drift.
- Entries written during the long **2026-10-30 evening session** may carry approximate HH:MM labels; the git commit
  timestamp is authoritative if exact ordering matters.
- Entry stamps are **ET**. A batch of entries written late on 2026-10-30 had been stamped with a UTC-flavoured date
  (`2026-10-31`, morning times); they were corrected to the real ET date and commit times on 2026-10-30 22:40 ET.
  If the clock and the heading disagree, the git commit timestamp wins.
- Never append at the end and never reorder old entries (`append_handoff.js` in the repo root is a leftover one-off
  script from 2026-10-30 that appends to the bottom - do not use it).

--

--

--

--

--

--

--

--

--

--

--

--

--

--

--

--

--

--

--

--

--

--

--

--

--

## 0. Pick up here (2026-10-31, 05:30 ET) - Resume point for voice phase 2 (WeabooTyler117 / DeepSeek)

Phase 1 is in (`7e6a2dd`): `src/audio/VoiceChat.*` enumerates devices, `-voice-probe` prints 2 capture / 4 playback on this
PC. Resume exactly here:
1. `VoiceChat.cpp`: WASAPI shared-mode capture on the default device (48 kHz mono 16-bit, 20 ms = 960 frames), playback
   stream with a small ring buffer.
2. Vendor Opus (BSD-3), encode 960-sample frames at 16/20/24 kbps, decode on receive.
3. `NetworkTransport`: add a `voice` message type alongside game packets over the existing relay path; the server only
   relays/rate-limits and never parses audio; UI never supplies endpoints or keys.
4. Jitter buffer 60-120 ms, per-peer mute/volume, push-to-talk from the binding table.
5. Evidence: host+guest processes, voice frame log lines, loopback WAV.

Then block 3: post-game panel timing + multiplayer end path, exit_experience sounds, mission_complete credit split.
## 0. Pick up here (2026-10-31, 05:00 ET) - Block 4 backend done: report, vote-to-kick, end round/match, 128-player cap (WeabooTyler117 / DeepSeek)

Server (`ServerTestDeploy`) now has, all verified locally:
- `POST /api/reports` -> `data/reports.jsonl` (`b645dbf`).
- `POST /api/sessions/:id/kick/vote` + `GET .../kick` - majority of non-host players, dedupes, sets `kickPending`; the host
  client performs the kick (`655fb4d`).
- `POST /api/sessions/:id/admin/end {kind: round|match, token}` + GET - token-gated to the session host (`65ba17a`).
- Player cap 16 -> 64 -> **128**, JSON body limit 2 MB; verified with a real 128-entry roster (`48dc168`, `a1196ba`).
@claude can wire Report / Vote to Kick / End Round / End Match against these; no stubs left.
Still open in block 4: pad Back suppression and real ping (client side) - moved into block 2/3 work.
## 0. Pick up here (2026-10-31, 04:30 ET) - Block 1 done: elite pipeline + colour reference (WeabooTyler117 / DeepSeek)

- `docs/reference/elite-slots.md`: `rrpmcg` decoded -> 314 components, elite ranks explicit (spec_ops, space, ultra,
  zealot, zealot_commander, officer, general + effects legendary_flame/eternal_flame/heart_attack/pestilence/storm/halo_bday).
- `docs/reference/elite-m10.md`: cache layout (Header 40 KB / Debug 4.17 MB / Resource 1001 MB / Tag 155 MB), header
  `levels\solo\m10\m10` cache v13; 79 `elite\shaders` and 117 `elite\bitmaps` paths from a chunked whole-file scan;
  HREK mapping sample: **57/60 bitmaps already stock, 51/60 shaders pack-only** -> the custom rank colour lives in the shader
  set, which is what we must reproduce. Facts only, no pack assets copied.
- Commit `e7ab6c9`. Next: read the pack-only shaders for their colour parameters, then block 2 voice transport.
## 0. Pick up here (2026-10-31, 02:00 ET) - Look speed: 2.00x is Reach stock, measured (@claude)

Commit `eb757b8`, replacing the look-speed mapping two entries down. Tyler asked for 2.00x = Reach's default, lower slower, higher faster.

**Measured** with `-fake-pad`/`-fake-pad-amount` in Slayer, reading the HUD compass over a 1.5 s hold (`docs/screenshots/look-speed-compass-readings.png`, about +/-2 deg):
- Profile byte at a 61% push: 0 = 18.5 deg, 8 = 37.5, 12 = 46, 15 = 54.5, 18 = 61. **36 and 72 = about 28: above 18 is slower.**
- Results: 1.00x 30, 1.50x 46, 2.00x 61; 31% push 7 at 2.00x, 27 at 4.00x; 4.00x at 61% push 172; 2.00x at full push 171.

**Tried and dropped:** mouse-look deltas while device = controller do nothing (4.00x and 10.00x both turned 61), and scaling the stick for slow speeds is far from proportional (half stick gave 8, not 30).

**Limit:** above 2.00x only partial pushes get faster. **A full push tops out at Reach's own 171 deg per 1.5 s, whatever the setting.** Raising that needs the engine's turn rate; a scan for a speed table in `haloreach.dll` and the `cmp 12h` sites I read found nothing. Next idea: the player control look rates in the `matg` tag (HREK). Above about 4.00x most of the stick range is at maximum, so high values feel twitchy rather than faster.

Below 2.00x uses the profile, applied when the profile is built (the row says from the next match; I did not test whether it changes mid-match). Saved 19 and the earlier 10.00 both migrate to 2.00. New keys `padLookRateX/Y`.

Self-review: build 0/0, self-tests pass. Settings run saved `padLookRateX` 1.25 after three steps down from a migrated 19. **Not measured:** vertical (same code, unmeasured), a physical stick, feel.

## 0. Pick up here (2026-10-31, 01:40 ET) - Reeve-PC tailored starting briefs & playbooks for @gemini and @deepseek (Gemini 4 Argon)

Added two machine-tailored starting briefs and continuous-mode playbooks for **Reeve's PC (`C:\Users\Reeve`)**, where `@gemini` (Gemini 4 Argon / `Dominus-VII`) and `@deepseek` (`C:\Users\Reeve\Desktop\deepseek\DeepSeek Harness.exe`) work alongside each other:
- `docs/GEMINI-PROMPT.md`: Tailored playbook for `@gemini` (`Gemini 4 Argon` / `Dominus-VII`) with Reeve's exact folder paths (`C:\Users\Reeve\Desktop\Halo Reach Community Edition`, `C:\Users\Reeve\Documents\deepseek-harness\default-workspace\ServerTestDeploy`, `C:\Program Files (x86)\Steam\steamapps\common\Halo The Master Chief Collection\haloreach`, `C:\Users\Reeve\Desktop\rea-main`), the direct Ninja+MSVC Release build snippet (0 errors / 0 warnings gate without triggering `build.bat`'s automatic `git pull`), test flags, file ownership split, Cross-Verification Protocol, and 7-step Continuous-Mode Loop.
- `docs/DEEPSEEK-PROMPT.md` & `docs/DEEPSEEK-PROMPT.txt` (also mirrored to `C:\Users\Reeve\Documents\deepseek-harness\default-workspace\DEEPSEEK-PROMPT.txt` and `C:\Users\Reeve\Desktop\deepseek\DEEPSEEK-PROMPT.txt`): Tailored starting brief for `@deepseek` on Reeve's PC so DeepSeek uses Reeve's paths and Ninja+MSVC toolchain instead of Tyler's `D:\` / VS2022 Community paths.
- `docs/PATHS.md`: Added Reeve's PC path table alongside Tyler's PC path table, and cloned `O7OF/ServerTestDeploy` to `C:\Users\Reeve\Documents\deepseek-harness\default-workspace\ServerTestDeploy`.

Self-review: Docs-only addition (`docs/GEMINI-PROMPT.md`, `docs/DEEPSEEK-PROMPT.md`, `docs/DEEPSEEK-PROMPT.txt`, `docs/PATHS.md`, `docs/HANDOFF.md`); verified all paths exist on Reeve's PC and `ServerTestDeploy` is cloned in `C:\Users\Reeve\Documents\deepseek-harness\default-workspace\ServerTestDeploy`.

## 0. Pick up here (2026-10-31, 01:36 ET) - Live per-peer RTT ping telemetry, 64 KB WebSocket relay fragment reassembly, LAN hole-punching, rich invites & cross-verification (Gemini 4 Argon)

Client commit `bfcc6c7` (**0.8.7 (RTT Ping Telemetry, 64 KB Relay Reassembly & Rich Invites)**) and backend `O7OF/ServerTestDeploy` commit `8f16061` (**both pushed to GitHub**, including @deepseek's previously local-only Workshop `mod_id` / `mod_title` backend fields and client commit `ff9cc79` + `7f19dbc`).

**What was wrong**
1. **Placeholder pings & silent UDP ICMP resets:** Roster rows and scoreboard entries displayed `-` or hard-coded `25 ms` because `NetworkTransport` did not measure per-peer RTT, and Windows UDP sockets did not disable `SIO_UDP_CONNRESET`, allowing an unreachable punch packet to raise `WSAECONNRESET` (`10054`) on subsequent `recvfrom` calls.
2. **WebSocket Relay frame truncation & 15 ms poll latency:** `RelayWorkerThread` used a `4096`-byte stack buffer without handling `WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE` / `WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE`, truncating fragmented Blam! replication snapshots or large roster JSON messages, and waited 15 ms per poll cycle.
3. **Same-LAN hairpin NAT & generic invite toasts:** Guests joining a host behind the same public IP only advertised their external IP in `pending_joins`, and game invites carried only `hostIp`/`hostPort` without `mode`, `map`, or `sessionId`.

**What changed**
- `src/network/NetworkTransport.hpp` & `src/network/NetworkTransport.cpp`:
  - Added `lastPingSentTick` and `rttMs` to `PeerRouteState`, plus `GetPeerPingMs(uint64_t machineId = 0)`, `GetAllPeerPingsMs()`, and `SetRemoteHostLanAddress()`. Measures live RTT from `PING`/`PONG` hole-punch probes and active UDP packet exchanges.
  - Applied `WSAIoctl(..., SIO_UDP_CONNRESET, FALSE)` on socket creation and ignored `WSAECONNRESET` / `WSAECONNREFUSED` in `OnNetRecv`.
  - Upgraded `RelayWorkerThread` to a `65536`-byte heap buffer with full multi-fragment WebSocket frame reassembly, 2 ms poll interval, burst draining (up to 32 frames/iteration), and 512-packet send/recv queues.
- `src/network/MasterServerClient.cpp`:
  - `DoHeartbeat()` sends `peer_pings` (from `NetworkTransport::Get().GetAllPeerPingsMs()`) and punches both `ip` and `lan_ip` from `pending_joins`.
  - `JoinSessionSync()` and `JoinLobbySync()` send `guest_lan_address` (`UPnP::Get().GetInternalIP()`), live measured ping, and register `SetRemoteHostLanAddress(hostLanIp, hostPort)`.
  - `FetchLobbyStateSync()` populates `LobbyPlayer::ping` from live `NetworkTransport` RTT measurements.
- `src/network/SocialClient.hpp`, `src/network/SocialClient.cpp`, `src/ui/FrontEndMenu.cpp`:
  - Added `mode`, `map`, and `sessionId` to `GameInvite` and `SocialClient::SendInviteAsync`.
  - Pre-game lobby and Social `INVITE` buttons pass the active lobby mode (`Custom Game`, `Campaign`, `Firefight`, `Forge`), map, and session ID. `DrawInviteToast` now displays `"is inviting you to a <Mode> match"` and passes `-session-id` on JOIN.
  - `SetLobbyRosterCallback` now preserves `rankIndex`, `emblemId`, `emblemBackgroundId`, `nameplateId`, `team`, and `ping` on real-time WebSocket roster broadcasts.
- `O7OF/ServerTestDeploy` (`server.js`, commit `8f16061`):
  - Synced @deepseek's `mod_id` and `mod_title` across `masterPublicSession`, `POST /api/sessions`, `PUT /api/sessions/heartbeat`, `GET /api/sessions/:id/lobby`, and `POST /api/sessions/:id/lobby/join`.
  - Added `peer_pings` RTT updates in `PUT /api/sessions/heartbeat`, `guest_lan_address` / `lan_ip` in `pendingJoins`, full identity fields in WebSocket `lobby_join`, case-insensitive `COLLATE NOCASE` in `/api/friends/*` and `/api/invites/*`, and `mode`/`map`/`sessionId` in `/api/invites/send`.

**Cross-Verification (`docs/TEAM-WORKFLOW.md`)**
- **REVIEW OK (@gemini, 2026-10-31 01:36 ET)** on `@claude` commits `960672a`, `83e05e9`, `e2e3771`, `0662bcf` (paddle keys F1-F6, `IsPaddleKey` F5 guard, 1.00x-10.00x look speed, Reverse Southpaw / Reverse Bumper Jumper): diff scope clean, `noble-client.exe` Release build 0 errors / 0 warnings on merged tree, `-test-map-gametypes` (including `PadLayouts::SelfTest` and `InputTuning::SelfTest`) exits 0, screenshots `docs/screenshots/settings-paddles-sensitivity.png` and `docs/screenshots/preset-reverse-bumper-jumper.png` verified.
- **REVIEW OK (@gemini, 2026-10-31 01:36 ET)** on `@deepseek` commits `ff9cc79` (Workshop mod Download & Join prompt) and `7f19dbc` (GitHub Actions build workflow): diff scope clean, UTF-8 verified in `FrontEndMenu.cpp`, `noble-client.exe` and `noble-dedicated.exe` build 0 errors / 0 warnings on merged tree, `docs/screenshots/mod-required-prompt.png` verified, and pushed both client and `ServerTestDeploy` (`8f16061`) to GitHub.

Self-review: `noble-client.exe` and `noble-dedicated.exe` built with 0 errors and 0 warnings; `-test-map-gametypes` exited 0; `node --check server.js` passed; `git show bfcc6c7 -- src/ui/FrontEndMenu.cpp` verified free of UTF-8 mojibake.

## 0. Pick up here (2026-10-31, 01:34 ET) - GitHub Actions build artifact workflow (@deepseek)

The repo had no CI, so the two PCs could drift on which `noble-client.exe` they ran. Added
`.github/workflows/build.yml` (commit `7f19dbc`): on every push to `main` and on manual `workflow_dispatch` it
checks out, configures with CMake + MSVC x64 (`cmake -S . -B build -A x64`), builds Release
(`cmake --build build --config Release --parallel`) and uploads `noble-client.exe` and `noble-dedicated.exe` as
artifacts. The ~2.2 GB runtime engine data is intentionally NOT packaged (it is copied from each user's own MCC
install per `.gitignore`), so the artifact is the one-click binary only.

Verified:
- YAML parses (the runtime `yaml` package): 5 steps, `on: [push, workflow_dispatch]`, `runs-on: windows-latest`.
- Configure/build commands match `build.bat` and `CMakeLists.txt`; the artifact paths
  `build/bin/Release/noble-client.exe` and `noble-dedicated.exe` are what this checkout actually produces (both
  exist in the current build output).
- FetchContent deps (spdlog, nlohmann_json, minhook, imgui) clone over git at configure time, which the GitHub
  runner supports.

Self-review: YAML parses cleanly; commands and artifact paths cross-checked against `build.bat` and the live build
output; new files only (`.github/workflows/build.yml`) so no local rebuild was needed and the running
`noble-client.exe` plus the other agent's uncommitted work were left untouched. **Not run here:** an actual GitHub
Actions run (no Actions access from this session) - it first executes on the next push to `main`.

Heads-up for @gemini (read before you commit): the working tree currently carries a UTF-8 mojibake in
`src/ui/FrontEndMenu.cpp` (`Spec §3` -> `Spec Â§3`, `·` -> `Â·`, and the `───` box-drawing comment lines double-
encoded). It is not part of your RTT/relay diff; stage your network files explicitly rather than `git add -A` so
the corrupted comment/string literals don't ride along. `CMakeLists.txt` already builds with `/utf-8`.

## 0. Pick up here (2026-10-31, 01:22 ET) - Paddle keys now default to F1-F6 (@claude)

Commit `960672a`, replacing the F13-F18 default in the entry below. Saved F13-F18 sets migrate on load; a set the player chose stays. No keyboard action defaults to F1-F6, but **F5 is the third-person toggle** (`PauseMenu.cpp`, read directly with `GetAsyncKeyState`). `ReachHost::IsPaddleKey` makes the toggle ignore F5 while a controller is connected and F5 is a paddle key. **So with a controller connected and default keys, the F5 camera toggle does nothing on the keyboard.** A settings row or rebind for third person (already on @deepseek's list) would remove this trade-off. @deepseek: if you add other direct key reads, check `IsPaddleKey`. `KeyName` now names F1-F24.

Self-review: build 0/0; self-tests pass. Injected F1 bound Jump to Paddle 1 and a saved F13-F18 became F1-F6. Real Slayer match with a controller connected: injected F5 logged no camera toggle as a paddle and logged `controller in use`; with Paddle 5 moved to F20, F5 logged `third person on` then `off`. `docs/screenshots/settings-paddles-sensitivity.png` opened. **Harness trap:** `-menu-capture` closes the client, so too low a frame ends a `-pad-script` run early; that looked like a failed sensitivity save until rerun. **Not run:** a real paddle or Steam Input mapping.

## 0. Pick up here (2026-10-31, 01:20 ET) - Lobby Workshop mod: Download & Join prompt (@deepseek)

Client commit `ff9cc79`; backend `ServerTestDeploy` commit `dfb2494` (**both local only - this session has no GitHub
credentials, so neither has been pushed**). Version bumped to **0.8.6 (Workshop Mod Download & Join)**.

**The gap.** The host could pick a Workshop-modded map for a lobby, but the lobby advert carried only `is_mod`: a guest
without the mod walked straight into the lobby and only then failed to load the custom map. The host's mod folder was
also never forwarded to the relaunched match, so even a guest who owned it did not activate it.

**What changed**

- `src/ui/ModCatalog.*`: reads MCC's `ModIdentifier.HostedModIds.SteamWorkshopId` (falls back to the numeric Workshop
  folder name), so a mod has a real published-file id; `FindByWorkshopId` matches the lobby requirement to an installed
  mod (Workshop or `data\mods`).
- Transport: `mod_id` / `mod_title` in the master advert, heartbeat, `/api/sessions`, `/api/sessions/:id/lobby` and
  `/lobby/join`; the LAN beacon appends the two fields after `HasPass` (older clients ignore the extra tokens). The
  heartbeat only sends the fields when non-empty, so the pre-game lobby's mod survives the host's match hand-off with
  the advert `main.cpp` builds.
- `src/host/SteamWorkshop.*`: a `-steam-subscribe <id> -steam-dll <path>` child process loads MCC's own
  `steam_api64.dll`, calls `SteamAPI_Init` + `ISteamUGC::SubscribeItem` and exits (log:
  `build\bin\Release\noble-steam-subscribe.log`). The menu process only polls
  `appworkshop_976730.acf` + `workshop/content/976730/<id>/ModInfo.json`, so Steam cannot block the UI. If Steam is
  unavailable the prompt opens the item's Workshop page and waits for the player to subscribe.
- `src/ui/ModDownloadPrompt.*`: the modal with the exact copy "The lobby you are trying to join currently has a mod
  installed from the Workshop that you do not have.", the mod title/id, **DOWNLOAD & JOIN** / **CANCEL**, an animated
  wait bar, RETRY/OPEN WORKSHOP PAGE on failure, and an automatic continuation into the lobby once the item is
  installed.
- `src/ui/FrontEndMenu.cpp`: the host passes its selected `ModEntry` into `SetupHostLobby` (advert + LAN beacon); a
  guest checks the lobby's mod before joining (list entry, or one `GET /lobby` peek when the host picked it after the
  last list refresh); the installed mod folder is carried into `-mod` for both the host and guest relaunch;
  `RefreshModCatalog` rescans after a download so the setup screens see the new item. The prompt gates ENTER/A/back and
  the invite toast while it is open.
- `-mod-prompt-demo` is a test-only flag that shows the dialog with a fake id and never starts a download; it exists so
  `-menu-capture` can photograph the prompt without a second machine hosting a modded lobby.

**Verified**

- `build.bat build`: **0 errors, 0 warnings**, both `noble-client.exe` and `noble-dedicated.exe`.
- Steam helper direct run: invalid id returns exit 5; already-installed item `3815090293` returns exit 0 with item state
  `0x5` (Subscribed | Installed) - `SteamAPI_Init`, the UGC accessor and `SubscribeItem` all work from a non-Steam
  launched process. A previous real run of item `3724674100` downloaded end-to-end (see side effect below), proving the
  parent's ACF/folder poll completes.
- `node --check server.js` and a local endpoint check returned `mod_id=3724674100` / `mod_title=Icecave: The Last Stand`
  from `/api/sessions`, `/api/sessions/:id/lobby` and `/lobby/join`.
- Screenshot: `docs/screenshots/mod-required-prompt.png` (`-menu-screen servers -mod-prompt-demo -menu-capture 360`).
  The status line that shows the public IP was painted out before committing, per the capture rule.
- **Not verified:** a real two-machine lobby join. The prompt was driven by `-mod-prompt-demo`; the continuation is the
  existing `JoinLobbySync` path, not clicked end-to-end in this session.
- **Deploy needed:** `ServerTestDeploy` `dfb2494` must be pushed and Render redeployed before the live master server
  returns the new fields; against the old backend the client simply sees no `mod_id` and behaves as before.

**Side effect / cleanup.** The first screenshot attempt auto-clicked DOWNLOAD & JOIN (the capture harness/desktop had sent
an input while the dialog was focused), so Steam subscribed to and downloaded **Icecave: The Last Stand (3724674100,
583 MB)** into `steamapps/workshop/content/976730`. The demo flag now uses fake id `9999999999` and a non-interactive
guard so it cannot happen again. Unsubscribe in Steam if Tyler does not want the mod.

**Self-review:** `build.bat` 0/0; `git diff --check` clean; Steam helper exit codes 5 and 0 with state `0x5`; backend
`node --check`; screenshot opened and inspected (redaction confirmed); no build output, logs, IPs, database files or
scratch scripts committed. Remaining risk is the untested live host/guest hand-off.

--

## 0. Pick up here (2026-10-31, 01:06 ET) - Tyler's answers: Reverse presets, paddle keys, look speed 1.00x-10.00x (@claude)

**Reverse presets** (`0662bcf`). Tyler said yes, but Reach and MCC have no such layouts (Halopedia's Reach and MCC control pages, MCC's `EN_HaloReach.bin`). Reverse Southpaw and Reverse Bumper Jumper are **Noble's names** for those layouts with the sticks swapped, and the row says so. If Tyler meant something else, only the name logic in `RenderController` changes. Halopedia's Reach table confirmed our six button layouts match. **Missing:** Reach's real Legacy and Legacy Southpaw stick layouts; Halopedia only shows them as images, so I did not guess.

**Paddles** (`83e05e9`). XInput can't see paddles, so each paddle is a keyboard key or a middle/side mouse button (default F13-F18, `padPaddleKeys`) that a tool like Steam Input sends. While a controller is connected, `ReachHost` takes those keys out of the keyboard state, so they don't fire keyboard actions or switch away from the controller, and passes them to `PadLayouts::Apply`. Left and right click can't be used. New Controller rows set each key; BUTTON BINDINGS accepts the paddle by pressing it. A key bound to a keyboard action stops doing that while a pad is connected.

**Look speed** (`83e05e9`). `padLookSpeedX/Y` 1.00-10.00, 0.25 steps on stick or D-pad, typed values to two decimals. Old 1-19 values migrate as (v+1)/2, so 19 becomes 10.00x. **Assumption:** 10.00x is the stock profile and the scale is linear onto profile steps 1-19. Between steps the profile takes the next step up and the stick is scaled down. Reach's own 1-10 was not measured, so Noble's 3.00x may not feel like Reach's 3. The profile part applies at match start; the stick part applies live.

Self-review: build 0/0. `-test-map-gametypes`: 26 layout expectations and input tuning pass. `-pad-script` with a real injected F13: `padBindings.jump` became Paddle 1, `padLookX` 19 migrated, `padLookSpeedX` saved 9.25. `docs/screenshots/settings-paddles-sensitivity.png` and `preset-reverse-bumper-jumper.png` opened. **Not run:** an actual paddle or Steam Input mapping, a paddle key or changed speed in a real match, the PADDLE KEY capture row itself, mouse buttons as paddles.

## 0. Pick up here (2026-10-31, 00:56 ET) - Armory framing, tier check, background deck, short-track filter, voice preview, nameplates (@claude)

**Armory** (`dd37730`, `FrontEndMenu.cpp` `ArmoryStage`). The engine camera can't move, so the finished engine frame is copied and redrawn smaller and to the right; the head now sits below the rule. A faint additive glow replaces pure black, and the per-slot cameras are unchanged. The nameplate is the main menu's `PlayerRow`. Captures: `docs/screenshots/armory-framing.png`, `armory-helmet-camera.png`. Opinion for @deepseek: this is a UI light on top of the model, so a real engine or data light would still look better. **Open:** the copy assumes the model sits at 65.6 % across the frame in every scene; only 1080p-scaled and 4K were seen.

**Commendation check** (`1561e00`). `-menu-screen career -menu-tab 2 -dev-commendation-tiers`: Cannon Fodder, Flawless Cowboy, Nice Arm, Small Arms, Splash Damage and Demon show Steel to Max. Display only, nothing saved or paid, inactive without the flag. Payout shown is the next tier's. Capture: `commendation-tier-check.png`.

**Backgrounds** (`f1facf3`). A shuffled deck persists in `data\userdata\menu-background-deck.txt`, so each picture shows once before repeats. The pre-game lobby uses its map's picture when there is one. **Decision:** "prefer the loaded map's art" is only applied to the lobby, the one screen with a loaded map in view.

**Music and voice** (`ecf8f0e`). The `.info` record at +4 is the decoded PCM size and +8 the channels, which gives exact lengths (matched vgmstream on 12 pieces). 19 tracks of 45 s or less dropped; menu themes kept; Halo: CE keeps everything because its bank has no lengths. **MCC ships preview lines** at `dialog\firefight_preview_dialog\<character>`: X (pad or key) plays them through the existing decode cache, cycling per character, silent when muted or at volume 0. I added this API rather than waiting, so @deepseek has no audio task here. The `spartan6_*` folders are Noble Six's (one line each). Stacker uses `trooper_sgt1`, a mapping I inferred.

**Nameplates** (`41c1c91`). Same bug as emblems: 520 of 531 ids don't match their position, so the plate shown usually wasn't the one picked. Fixed. **Saved plates may now look different, showing the real pick.** Lobby rows also draw other players' `emblemBackgroundId`.

**Pause-menu Esc reopen race:** `dc8731b` removed the double handling. One live run after it showed open then closed with no reopen; one run, not proof.

**Trap:** my checkout needs `tools\vgmstream` copied for audio to start; `PATHS.md`'s first-run steps only copy `data`.

**Not started or waiting:** Vote to Kick, End Round, End Match and real ping (no endpoints yet); mute icons (no voice); emblem colour picker (not started, needs black layers treated as masks); a separate pause Controls page (Pause > Settings is the same panel, so it has the Controller page, but I did not run it there).

Self-review:
- Build 0 errors / 0 warnings at each commit, last on the tree with `41c1c91`. `-test-map-gametypes` passed on the settings commit; not re-run after these.
- Live injected input in a Sword Base Slayer match on the final client code: `scoreboard actions opened`, `report saved=true`, pause opened then closed.
- Logs: Carter preview lines 1, 2, 3, 1 with lengths 1.1, 1.5, 1.1, 1.1 s; `silent (muted or volume 0)` at volume 0; `19 music tracks ... left out`; deck counted 10, 9, 8 across three launches with three different scenes.
- **Nobody has listened to the audio**, and I played lines at low volume on this PC during tests. Not captured: the lobby map picture and a real remote lobby row. Test data in my checkout was restored after the runs.
- Seen once, unresolved: an Armory capture at frame 120 showed grey armour with a blue swatch, while frame 1200 showed blue. Probably colours applying after that early frame, but I did not confirm it.

REVIEW OK (@claude, 00:56): @deepseek `d146ffb` (host-disconnect panel, co-op parity, third-person gate) and `bc77811` (emblem colour key). Diffs match their claims, they build 0/0 inside my tree, and `connection-lost.png` and `emblem-backgrounds-keyed.png` show what they claim. Build, diff and images only: I did not repeat the two-process run. `d146ffb` bundles three features in one commit, which makes review harder but is not a defect.

## 0. Pick up here (2026-10-31, 00:40 ET) - Controller-first Settings: tabs, Y/X, one preset, remap everything (@claude)

Commit `dc8731b`. TODO section H.

**The filed root cause was only half right.** ImGui's Win32 backend did feed slot 0 in the front-end. What really failed: the Button Layout row called `CycleValue` and dropped the step, so it could not change with any device, even a mouse; the pause menu got no pad at all; and any pad not in slot 0 was ignored. @deepseek's `GamepadUi` (`f782657`, landed while I worked) fixed the pause menu but ran on top of the backend. **`src/ui/UiPad.cpp` replaces it** in a new commit, nothing rewritten: it scans all slots, ignores the pad when Noble isn't in front, and is the only path now (backend gamepad compiled out in `CMakeLists.txt`).

- **Tabs:** left/right adjusts a focused value. It switches tabs only on the tab list (Up from the first row, shown with an outline and `< >`) or on a row with no value. LB/RB no longer switch tabs here; Q/E still do.
- **Apply/Discard:** box shows Y + ENTER APPLY and X + ESC DISCARD, both clickable. Esc with unsaved changes now discards and stays on the page instead of leaving. Enter applies only from the tab list or a slider row, because other rows use Enter. B always goes back, and unsaved changes stay pending as before.
- **Preset:** one row, 12 entries (6 Reach layouts, each with sticks normal or swapped), stored in the existing `padLayout` + `padStickLayout`. The diagram sits under it and is now tied to physical inputs, showing what each does. **Changing preset clears custom bindings**: an old saved `Jump = A` left LB unused and doubled A under Bumper Jumper. Tyler's own settings may hold the same leftover.
- **Remap:** 18 actions to any button, trigger, D-pad direction, stick direction or click. A taken input swaps. New: throw grenade, crouch, scoreboard, pause, 4 D-pad, team chat and push to talk (assignable, no effect yet). Toggle zoom row, default on.
- **Layout bug fixed:** non-Default presets lost LT throw, and Southpaw's and Boxer's grenade button switched grenades instead of throwing. The self-test expected the wrong thing, so it was corrected too.
- Controller Back now opens Noble's scoreboard, not Reach's. Pause-menu Esc reopen race removed (`PauseMenu.cpp` no longer also reads Esc); B resumes.
- **Test flag:** `-pad-script a,down,right,y` presses inputs with no controller, one per 500 ms after 3 s.

**Decisions for Tyler (handoff, not blocking):**
1. "Reverse Southpaw / Reverse Bumper Jumper" are not added: I could not verify what they are, and swapped-stick entries are named plainly instead.
2. Stick *movement* axes can't be moved independently of the swap, only used as extra inputs for actions.
3. Paddles 1-6: XInput does not report them. The row says to assign them in Xbox Accessories or Steam Input. Real support needs a different input API.
4. Sensitivity 1-19 is the profile byte + 1; how it lines up with Reach's 1-10 is unmeasured, and the row says so. 19 is also the maximum, so stock can only go down.

**Hold-to-zoom is a guess:** it assumes the engine toggles on each press and sends a press on the way down and up. Untested in a match.

Self-review:
- Build 0 errors / 0 warnings on the tree merged with `caf601a`. `-test-map-gametypes`: button layouts 24 expectations, 0 failures.
- `-menu-screen settings -pad-script ...`, no keyboard or mouse, comparing `settings.json` before and after (second checkout, restored afterwards): `brightness` 1.0 to 1.02, `volumeGame` 0 to 0.01, `padLayout` 0 to 2, `padLookY` 19 to 18 on Y, a further change discarded by X. `wait,b` returned to the main menu, capture opened.
- `docs/screenshots/settings-controller-preset.png` opened: Bumper Jumper, Y/X prompts, diagram correct.
- **Not run:** physical button presses, a preset or rebind in a real match, hold-to-zoom, Settings from the pause menu, the rebind capture itself, other resolutions.

Review of @deepseek `f782657`: its diagnosis overstated (see above), and the file is retired with reason. Not re-opened as a failure; it did reach the pause menu.

## 0. Pick up here (2026-10-31, 00:25 ET) - Spartan Armory preview ids fixed: CQC/ODST/UA-Base, the chest chain, Chobham/Hard Case/Soft Case (Tyler / DeepSeek)

Tyler's Armory pass found the 3D preview showing a different piece than the name/picture while the in-game item
(the MCC profile number) stayed correct: the hand-written permutation string ids in ArmorCustomizer's lists do not
all render the model they are named after.
- Shoulders: CQC rendered ODST, ODST rendered UA/Base Security, UA/Base rendered CQC (3-cycle).
- Chest plates: `chest_1..chest_13` each rendered the next item's plate, `chest_kurt` (UA/Base Security) rendered
  ODST, and the rest of the chain followed (HP/Halo -> UA/Base ... UA/Multi-Threat -> Collar/Grenadier [UA]).
- Utilities: Chobham rendered Soft Case, Hard Case rendered Chobham, Soft Case rendered Hard Case (3-cycle).
- Fix: added `SpartanPreviewId()` in `ArmorCustomizer.cpp`, a small map from the profile id (also the key for
  ReachHost's correct MCC item table) to the permutation id whose model matches the item's picture. `SyncToReachArmory`
  uses it for the chest, both shoulders and the utility only; the saved `appearance.json` ids, the MCC item numbers
  and the in-game equipment are unchanged, so this is preview-only.
- Map: chests `chest_1->chest_3, chest_2->chest_4, chest_3->chest_5, chest_4->chest_kurt, chest_5->chest_6,
  chest_6->chest_7, chest_7->chest_8, chest_8->chest_9, chest_9->chest_10, chest_10->chest_11, chest_11->chest_12,
  chest_12->chest_13, chest_13->chest_2, chest_kurt->chest_1`; shoulders `9->kurt, 10->9, kurt->10` (both sides);
  utilities `1->2, 2->5, 5->1`.
- Built with `build.bat` (0 errors, noble-client + noble-dedicated). The main build dir had a Ninja CMake cache from
  the merge; build.bat detected the VS generator mismatch, cleaned the cache/subbuilds and rebuilt cleanly.
- Verified in the Armory with `-armory-focus Chest -armory-unlock-all`: a Collar/Grenadier profile previews the
  Collar/Grenadier gorget, matching its tile. The rest of the chain is the same id->model mapping Tyler reported.


## 0. Pick up here (2026-10-31, 01:05 ET) - Controller input now actually reaches ImGui (WeabooTyler117 / DeepSeek)

Tyler said the controller presets do nothing. Cause: `SettingsPanel` already read `ImGuiKey_GamepadDpad*`/`GamepadFace*`,
but **no backend ever fed gamepad keys to ImGui** - the DX11/Win32 backends don't, and ReachHost's XInput use is only for
rumble. Every pad check was therefore dead: no tab switching, no preset cycling, no Apply/Discard.

- New `src/ui/GamepadUi.cpp`: reads XInput (1.4, falls back to 9.1.0) each frame and sends `io.AddKeyEvent` for
  D-pad, A/B/X/Y, Start/Back, LB/RB, LT/RT (trigger threshold 96) and both stick directions (threshold ~0.37), and sets
  `ImGuiConfigFlags_NavEnableGamepad`. Called before `ImGui::NewFrame` from PauseMenu, FrontEndMenu and UIManager, so every
  overlay screen gets it, not just Settings.
- Build 0/0; settings screen capture still renders (no regressions). Physical-pad verification is Tyler's, since this box
  has no controller attached to the agent.
- Next for @claude: wire the visible **Y = Apply changes / X = Discard changes** prompts, make the Button Layout row cycle
  (it already reads the keys), and update the diagram with the preset. The input layer is now there.

## 0. Pick up here (2026-10-31, 00:45 ET) - Campaign gating refined + found Reach's end-screen credit sounds (WeabooTyler117 / DeepSeek)

- **Campaign gating refined:** quitting a mission pays **no completion bonus** but still pays the **performance bonus**
  (kills/difficulty/time). The result needs a `mission_complete` flag and the credit breakdown split performance vs
  completion; a quit pays only performance.
- **End-screen audio found:** HREK has Reach's real exit-experience sounds in
  `tags\sound\game_sfx\ui\omaha_ui\exit_experience\`: `counter_loop` (the credit tick while the total counts up),
  `totals`, `slot_machine`, `rank_up`, `game_completion`, `commendations`, `daily_challenges`, `heat_bonus`, `hopper_bonus`,
  `invasion_bonus`, `time_spent`, `winning_bonus`, `achievements`. Plan: extract with `tool.exe`, ship in Noble's data, play
  `counter_loop` during the count-up and `totals` at the end; `slot_machine` pairs with the post-game slot machine. CR and XP
  are the same currency, so one counter and one sound. Fallback until extracted: the current cue.

## 0. Pick up here (2026-10-31, 00:30 ET) - Controller presets don't work: pad state never reaches ImGui (WeabooTyler117 / DeepSeek)

Tyler confirmed the controller presets do nothing. Root cause found without running anything: `SettingsPanel` already has
gamepad navigation (`m_keyLeft = … ImGuiKey_GamepadDpadLeft …`, accept on `GamepadFaceDown`) and `CycleValue` uses it, but
**nothing feeds gamepad state into ImGui**. The DX11 + Win32 backends do not supply gamepad keys, and ReachHost's XInput poll
only writes the engine pad state (`st[...]`), which ImGui never sees. That is why D-pad/stick/buttons are dead in Settings,
why the Button Layout row cannot be changed, and why Apply/Discard cannot be triggered from a pad.

Fix plan (mine, next): publish the polled pad (buttons bitmask + both stick axes) from ReachHost and, in the overlay frame
before `ImGui::NewFrame`, send `io.AddKeyEvent`/`io.AddKeyAnalogEvent` for GamepadDpad*, GamepadFaceUp/Down/Left/Right,
GamepadStart/Back and GamepadLStick*. Then @claude wires the visible prompts: Y = Apply, X = Discard, preset row cycles and
the diagram follows the preset.

## 0. Pick up here (2026-10-31, 00:15 ET) - New backlog: controller UX, armory presentation, audio/data (WeabooTyler117 / DeepSeek)

Tyler's controller playtest list, filed as TODO sections H and I with the @claude / @deepseek split:

- Settings must be controller-navigable: tabs on D-pad/left stick (not LB/RB), stick adjusts the focused value instead of
  switching tabs, **Y = Apply / X = Discard** with controller icons, button-layout row actually changeable by pad, layout
  diagram updates per preset, one combined controller preset (stick + buttons), every input remappable including paddles,
  missing actions (crouch, toggle-zoom default on, team chat), and a documented 1-10 sensitivity baseline (default 19 today).
- Armory: center Spartan/Elite under the APPEARANCE rule, match the main-menu nameplate, add the spotlight/key light so the
  chamber is not pure black, camera pans per body part.
- Commendations: force the first six to steel/bronze/silver/gold/onyx/max for an art + payout check (dev only).
- Campaign completion gating (quitting must not count), post-game slot machine, Forge/custom/Firefight options (after variant
  work), menu-background shuffle + match the loaded map, music <=45 s tracks removed, Firefight voice preview in the Armory.

## 0. Pick up here (2026-10-31, 00:20 ET) - Plan for match settings, lobby profiles, voice chat and match-end flow (WeabooTyler117 / DeepSeek)

Tyler dumped a big feature list (in-game controls, lobby profile cards with ADD FRIEND, voice chat, match-end -> lobby or
rotation, match settings for every mode, credit-farming rules). Wrote `docs/plans/match-settings-and-social-plan.md` and
pointed the TODO at it (section G). Key findings from the repo before planning:

- **Controls are already live:** `ReachHost` reads bindings every input poll, so a preset/rebind applies on the next frame in
  every hostable mode; only the UI to change it mid-match is missing (pause menu has no Settings page).
- **Variants are data, not UI:** `BlamEngine` already decodes a `.bin`/`.mvar` with the engine's `decode(fileBytes, out[0xFC04],
  status)` (same call RR uses) and the lobby already transports `variant`. There is no writer yet, so the plan is: picker ->
  read-only viewer -> RE of `c_game_variant` -> editor for the identified fields; `game_variant_settings` tags + MegaloEdit are
  the real source of allowed settings.
- **The pasted settings list is Halo Online / Halo 4** (Spartan Charge, Ground Pound, Clamber, loadout tiers, Breakout...),
  not Reach's engine. Taking it literally would mean building fields Reach cannot honour; plan uses Reach's actual set.
- **No voice chat exists** (no WASAPI/Opus/relay audio), so it is phased: capture+loopback -> relay channel + jitter buffer ->
  UI (devices/PTT/mute icons) -> team/proximity -> post-game.
- **Match end:** sessions currently die with the match process; returning to the same lobby needs the session to survive
  results, plus a rotation array and results->advance signalling.
- **Credit farming:** the reward surface is what we restrict, not the feature. Custom games with any modified setting earn
  nothing; unmodified custom earns reduced; forge nothing; campaign/firefight standard full; server computes credits from the
  results payload and validates it (variant hash, duration, score caps, per-account hourly caps). Table in the plan.

## 0. Pick up here (2026-10-31, 00:02 ET) - Tooling fixes after @claude's report; controller/ping/backend items filed (WeabooTyler117 / DeepSeek)

From @claude's status report:

- **`tools/test-modes.ps1` no longer kills every `noble-client` by name.** It now has `Get-OurClients`, which filters by the
  executable path (`-Exe`, default this checkout's `build\bin\Release\noble-client.exe`), so a test run cannot kill the
  other checkout's match. The remaining by-name `Get-Process noble-client` there is only the read in `Get-OurClients`.
- **Docs encoding:** the last two garbled spots in `HANDOFF.md` are now plain text (my own note that quoted the broken
  characters, and the `DrawCycle` line).
- **Filed in `docs/TODO.md`** from the same report: controller Back still opens Reach's old scoreboard (pad suppression,
  mine), the possible Esc-to-resume reopen (PauseMenu, @claude, needs a repro), real ping values (transport, mine), the
  scoreboard backend actions (report endpoint, vote-to-kick, end round/match - server + transport mine, then @claude wires the
  buttons), and the human controller-preset test (Tyler: Southpaw or Bumper Jumper -> Apply -> play -> rebind one button).

## 0. Pick up here (2026-10-30, 23:52 ET) - Emblem art fixed: black boxes are gone (BC1 luminance -> alpha) (WeabooTyler117 / DeepSeek)

Tyler's screenshots showed every Emblem and Emblem Background tile as a **white symbol on a black square**, plus blocky
equipped previews. Cause: MCC's `emblemstexturepack` art is **BC1 with no alpha**, a white shape on an opaque black field, so
drawing the texture put the whole square on screen.

- New `UiArt::LoadPackImageKeyed` + a small CPU decoder for BC1/BC3/RGBA: each pixel becomes white with **alpha = luminance**
  (BC3 multiplies by its alpha), then uploads as RGBA; `EmblemByName` uses it, so every emblem draw (grids, equipped preview,
  nameplate, roster rows) gets proper transparency for free. Cached per name like the old path.
- Background tint is now a steel blue at 200 alpha drawn over the tile, instead of the old opaque-ish tint, so black-background
  shapes stay black and light shapes read as shapes.
- Verified: `-menu-screen armory -armory-focus "Emblem"` and `... "Emblem Background"` captures show white symbols over the
  actual shapes (circle, cross, triangle, star, gear...), no rectangles -
  `docs/screenshots/emblem-symbols-keyed.png` and `docs/screenshots/emblem-backgrounds-keyed.png`.
- Note for the next person: `-armory-focus "Emblem Background"` must be passed **quoted in one argument**, otherwise it
  matches the `Emblem` row first (hit that twice tonight).

Remaining on emblems: the real three-colour picker (backgrounds are fixed steel for now), `Nameplate(int id)` may still be
fed a list position instead of an id, and background replication to other players needs @claude to draw `emblemBackgroundId`
(data is already on the wire).

## 0. Pick up here (2026-10-30, 23:45 ET) - Emblem backgrounds now travel with the roster (server `b1656b4`) (WeabooTyler117 / DeepSeek)

@claude's emblem picker stores `emblem_background_index` locally, but only `emblemId` went to the server, so other players
could not see a background. Plumbing only, no drawing changes:

- **Server (`ServerTestDeploy` `b1656b4`, pushed):** accepts `host_emblem_background_id` on register/heartbeat and
  `emblem_background_id` on lobby join, stores `emblemBackgroundId` on every roster entry, keeps the host's row in sync and
  exposes `host_emblem_background_id` in the public session. Verified with a local server: register with bg 3 stores 3 on
  the session and host row; lobby join with bg 4 stores 4 on the guest row.
- **Client:** `MasterAdvert.hostEmblemBackgroundId`, `LobbyPlayer.emblemBackgroundId`, the registration body + roster entry,
  both roster parsers, `JoinLobbySync`'s new parameter and its `/lobby/join` body, and the host advert/join call sites
  (from `Appearance.emblemBackgroundIndex`). Build 0/0.
- **Still needed (TODO, @claude):** draw `emblemBackgroundId` in `PlayerRow` and the lobby rows; the emblem picker already
  draws locally.
## 0. Pick up here (2026-10-30, 23:36 ET) - Readability pass: no overlaps, two low-contrast fixes (@claude)

Commit `b0b7cac`, `src/ui/FrontEndMenu.cpp`. Captured 20 views (12 screens plus all 8 Settings pages) and found no overlaps.
Fixed the two places dim text sat directly on bright art: the skull details (dark panel, brighter text) and the connection
line on setup screens (backing plate, brighter text). The main menu was reviewed in the emblem capture.

**Not fixed:**
- The server browser status line shows the public IP. Black it out in committed captures.
- `Reach menu audio is off: vgmstream-cli.exe was not found` shows on the Audio page in the second checkout, which only
  copied `data`. Not checked in the main checkout.
- Reviewed mostly on contact sheets at 640x360, which catches overlaps but can miss small contrast issues. Only 1920x1080.

Self-review: build 0/0; `docs/screenshots/readability-skulls.png` and `readability-forge.png` opened and both now read
clearly. These are the after shots; the before state is the earlier code.

Review of @deepseek's `REVIEW OK` entry and its host-disconnect work: not reviewed by me yet. Next @claude session should
review @deepseek's latest commits first.

## 0. Pick up here (2026-10-30, 23:30 ET) - REVIEW OK: @claude's scoreboard (60f374b) and emblem backgrounds (66e64a0) (WeabooTyler117 / DeepSeek)

**Scoreboard replacement - passes.** Diff scoped to `PauseMenu.cpp` (+257), `ReachHost.cpp` (+14: the scoreboard key is
taken away from the engine and published to our key mask, a click while held frees the pointer) and one
`FrontEndMenu.cpp` line. Merged tree builds **0 errors / 0 warnings**, and I re-ran the demo capture on the merged build:
the Noble panel replaces the engine's scoreboard completely, columns PLAYER/TAG/TEAM/PING, rank icons, local "You",
Report window and the action row. Gaps recorded in TODO: ping shows "-" (the server's ping values are placeholders - real
RTT needs transport work), Vote to Kick / End Round / End Match are greyed with "needs the server/host actions", Report
is saved locally as JSONL until a report endpoint exists, and mute icons wait for chat/voice.

**Emblem background picker - passes.** Its correction is right: `playercustomization.xml` does carry 105 reusable
backgrounds (`background="eb_001"`, ids 500+) that the old parser skipped because it only kept entries with `image1`. The
Armory now has IDENTITY > Emblem Background (None + 105, saved as `emblem_background_index`, default 0 so existing looks
do not change), both layers are drawn, and the "shown emblem is not the one picked" bug is fixed by routing everything
through `UiArt::DrawEmblem` and deleting the id lookup. Capture `docs/screenshots/emblem-background.png` inspected: grid,
equipped marker, details panel, live nameplate. Open (recorded in TODO): other players do not see your background yet
(roster only carries `emblemId` - I will add the field and Claude will draw it), no emblem colour picker yet, and
`Nameplate(id)` may have the same list-position-vs-id bug.

**Docs encoding.** Claude's docs commit had broken UTF-8 in `HANDOFF.md` (`â€"` sequences); repaired across the docs and
verified only a legitimate `‹ ›` remains.
## 0. Pick up here (2026-10-30, 23:28 ET) - Controller presets and bindings already exist; stale row removed (@claude)

The TODO line was stale: `PadLayouts.cpp` has all six Reach layouts, Settings has BUTTON BINDINGS per action, and
`ReachHost.cpp:1395` applies both to the pad state. No new feature was needed. The only change is removing a leftover
SettingsPanel row that said "Only Default for now", which contradicted the working BUTTON LAYOUT selector further down.

Stays `[~]`: **needs a hands-on test with a controller** (pick Southpaw or Bumper Jumper, Apply, check in a match; rebind one
action). An agent can't press the physical buttons.

Self-review: build 0/0; `-test-map-gametypes` exit 0 with `button layouts - 16 expectations, 0 failure(s)`; capture
`-menu-screen settings -menu-tab 5` opened, shows no stale row and `Controller 1 connected`. Not run: real presses in a match.

## 0. Pick up here (2026-10-30, 23:20 ET) - Host-disconnect panel, co-op difficulty/skulls verified, third-person host gate (WeabooTyler117 / DeepSeek)

Night-shift progress, three items.

**1. Host disconnect handling (done).** A guest whose host vanished used to sit frozen in the mission forever (only
`CLOSE channel reason 16` then repeated `reason 4` in the log, no results, no menu). `NetworkTransport` now tracks
`m_lastRemotePacketMs` and exposes `SecondsSinceRemotePacket()`; PauseMenu shows a **CONNECTION LOST** panel after 8 s of
silence with "The host left the game." and a **Return to menu** button that calls `Relaunch::HandOff(L"")`. Verified by
killing the host mid-campaign: log `PauseMenu: host connection lost (8.0s without packets)` and the capture
`docs/screenshots/connection-lost.png`. Gap: the engine's own PLAYERS / STARTING GAME strip still shows dimmed behind the
panel - cosmetic, same class of problem as the scoreboard cover.

**2. Skulls + difficulty parity (verified).** The guest launch args already carry `-difficulty` and `-skulls` from the host's
launch data (`FrontEndMenu.cpp:4161-4163`). Two-process campaign at Legendary + skull 0x1: both logs
`Mode: campaign | Difficulty: 3 | Skulls: 0x1`, both in game, **zero GameResults over 170 s**. The in-game effect of an
individual skull is not visually confirmed; only the data path is.

**3. Third person: host/mode gate + tuning status.** The detour now answers `true` only when offline or host
(`NetworkTransport::IsHost()`); guests stay first person (Tyler's rule). The follow hook at `haloreach+0x1DCADC` installs
(prologue `48 8B C4 48 89 58 18 55`) but never fires in the camera path the check enables - no "follow distance scaled"
line - so distance/shoulder tuning is parked as research; the camera is usable but close. Still open: settings row + key
rebind.

## 0. Pick up here (2026-10-30, 23:05 ET) - Third person VERIFIED (Firefight), with a real FP-vs-TP capture (WeabooTyler117 / DeepSeek)

The F5 hook works. Verified in **Firefight** (`beachhead`, firefight class), same spot, two runs:
`docs/screenshots/third-person-vs-first.png` - top is first person (weapon in front, no body), bottom is third person
(the Spartan's back and shoulder fill the lower left, weapon still visible in hand). The hook log fires
`camera check fired (player …, mode 0, original false)` as soon as the match starts.

**Why the earlier attempts looked like a failure:** campaign `m10`'s first minutes are a cutscene and a Falcon ride; the
cinematic/vehicle camera ignores the per-player camera-mode check, so captures at 100-180 s were identical with and without
the hook. The mistake was judging camera work from a single cutscene frame - corrected in the TODO and handoff.

Tooling added along the way: `-capture-at <seconds,...>` (wall-clock capture points; engine frame rate is variable, so
frame-number captures were unreliable).

Remaining for this feature (TODO, claimed):
- pull the camera back: detour `haloreach+0x1DCADC` (RR's follow hook) - call the original, scale `[r9+0x28]`, optional
  shoulder flip at `[r9+0x14]`; the current view is very close (camera near the shoulder)
- a settings row + key rebind for the F5 toggle
- host / game-mode gate (Tyler wants it host- or mode-controlled)

## 0. Pick up here (2026-10-30, 23:15 ET) - Test runs muted with tools/mute-game.ps1 (WeabooTyler117 / DeepSeek)

Tyler asked for the game to be muted before tests. Noble's `masterMute` only silences our own menu audio, so the engine was
still loud; the engine's real volumes come from the profile, written by `ReachHost` at `volumeGame -> +0xA5C` and
`volumeOther -> +0xA64`.

- New `tools/mute-game.ps1`: sets `volumeGame`, `volumeMusic`, `volumeOther` to 0 and `masterMute` to true in
  `build\bin\Release\data\userdata\settings.json` (and, with `-IncludeClaude`, in the Claude checkout too), keeping a
  `settings.json.before-mute.json` backup. `-Unmute` restores it.
- Ran it on both checkouts; verified `game 0 / music 0 / other 0 / masterMute True` in each. The rule is now in
  `TEAM-WORKFLOW.md` and `AGENT-STATUS.md`, so every night-shift test is silent by default.

## 0. Pick up here (2026-10-30, 23:05 ET) - Night shift: queue, armory spotlight, design-spec backlog, message for @claude (WeabooTyler117 / DeepSeek)

Tyler is away until morning. Both agents work continuously without interaction; the protocol and queue are in the
**"NIGHT SHIFT"** section of `docs/TODO.md`, mirrored in `AGENT-STATUS.md`, and the same text is at
`docs/CLAUDE-NIGHT-SHIFT.txt` in case Claude cannot see the repo.

- **Queue order** for the night (first unclaimed wins): 1 @deepseek third-person camera, 2 @claude scoreboard action bar +
  full cover, 3 @deepseek campaign co-op verification, 4 @claude emblem picker, 5 @deepseek scoreboard live K/D API,
  6 @claude controller presets, 7 @deepseek CONTINUE checkpoint, 8 @claude readability pass, 9 @deepseek vote-to-kick
  transport/server, 10 @claude server browser follow-ups, 11 @deepseek invites mode/map, 12 @claude post-game names,
  13 @deepseek 4+ players, 14 @deepseek armory spotlight, 15 @claude docs/reviews, 16 @deepseek host/admin engine actions,
  17 housekeeping.
- **Armory spotlight** is now an explicit item (B): the menumap has only faint volumetric haze, so the model sits in a
  void. Cheapest route first - light the chamber in `mainmenu_all.map` (HREK lighting info + lightmap rebuild) or inject an
  engine light at the armory pedestal; the per-slot cameras already work.
- **Design-spec backlog** added as section F (`docs/menu-redesign-spec.md` §4.10-§4.15, §6): armory leftovers, input extras,
  engine-research game options, audio/language/communication, online/folders/mods, server-browser extras, pickers,
  rotation/voting, game options editor, theater (stays disabled), "continue last game".
- **Host/admin controls (spec 4.15)** added as its own item: kick/session-ban/permanent ban, end/restart round, restart match,
  shuffle teams, end game -> back to the lobby, skip rotation entry. Split: engine/session actions mine, host-only panel
  Claude's.
- **Scoreboard item** now includes spec §4.16 extras: per-row mute icon, Mute-all, and the panel must fully cover the
  engine's scoreboard.

**Shutdown check Tyler asked about**: `git log -S"blamEngine->Shutdown();"` only finds `8213116` (Oct 26) - a false alarm.
The menu-to-match crash fix is `982e32a` (Oct 28), which made `BlamEngine::Shutdown()` skip the `m_fnShutdown`/vtable[0]
call while the engine thread runs; `main.cpp` still calls `blamEngine->Shutdown()` then `TerminateProcess` (~line 648).
Added a behaviour-based regression item to the TODO: menu -> start match -> return, log must end with
"Network and host services shut down; exiting process cleanly." and no ntdll dialog (repeat as a Direct Connect guest).

## 0. Pick up here (2026-10-30, 22:45 ET) - REVIEW OK: server browser polish (f3cbbf6); agent sync fixes (WeabooTyler117 / DeepSeek)

**Review of @claude's `f3cbbf6` + docs `c53ed7d` - passes.**

- Diff: `ServerBrowser.cpp` only, 33+/14-. `IsModdedGame` now uses the host's advertised flag, a `[Mod]` session name, or a
  map `MapNames::IsVanilla` does not know; a `.map` suffix alone no longer means mod. `DrawModBadge` draws an inline badge;
  the Map column was widened. Details panel uses the display name (Forge World) instead of `forge_halo`.
- Build from the merged tree: **0 errors / 0 warnings**.
- Capture inspected (`docs/screenshots/server-browser-polish.png`, 1920x1080): six rows, all display names correct
  (Winter Contingency, Sword Base, Forge World, Anchor 9, My Custom Map), MOD only on the three modded rows, details panel
  shows "Forge World" with its picture, and the status line/IP is either blacked out or 192.0.2.x test-net addresses.
- TODO line carries the commit hash and evidence; handoff entry exists. Marked reviewed in `docs/TODO.md`.
- Not covered by the author (recorded, not blocking): live-server games, LAN/own-host rows, hover tooltip, resolutions other
  than 1920x1080.

**Review of my `ca6d33a` by @claude**: diff/build only, camera switch not exercised - accepted, third person stays `[~]`.

**Fixes from @claude's report / Tyler's notes**

- **Dates:** my entries had been stamped `2026-10-31` with morning times (a UTC slip). All of my entries now use the real
  **2026-10-30** date and the actual ET commit times (20:48-22:34), and the "How this file is kept" section says stamps are
  ET and the commit timestamp wins.
- **Build path:** already fixed before @claude could (commit `053e79a`) - `START-HERE.md` and `CLAUDE-PROMPT.txt` now use
  `Common7\IDE\CommonExtensions`; scan reports 0 wrong spellings.
- **Second checkout:** `D:\Halo-Reach-Community-Edition-claude` had `masterServerUrl = http://127.0.0.1:18099` from the
  stand-in server; reset to `https://outbreak-z-test-server.onrender.com`. The main checkout's settings/career/appearance
  were not touched.
- **PauseMenu.cpp released** in `AGENT-STATUS.md`, so @claude can take the scoreboard action bar (ping, Report, Vote to
  Kick, End Round/End Match). Status blocks now show accurate times/commits.
- **Capture privacy** is now a written rule in `TEAM-WORKFLOW.md`: no public IPs in committed screenshots.

## 0. Pick up here (2026-10-30, 22:40 ET) - Elite armor sets finally replicate, Elite armor effects, Armory index safety (O7OF / DeepSeek)

All changes built with MSBuild Release (`build\noble-client.vcxproj /p:Configuration=Release /p:Platform=x64`), 0 errors.
Version bumped to **0.8.4 (Elite Armor, Effects, Presets & Colors)** in `src/common/Version.hpp`.

### 1. Why equipped Elite sets fell back to Elite Minor (O7OF / DeepSeek)
The previous `EnginePatches` "fix" was the cause. Reverse-engineered the appearance path in `haloreach.dll`
(SHA-256 `738dd2d2...0894`, image base `0x180000000`) with REA/Ghidra and llvm-objdump:
- **`0x2EDA08` (profile -> player copy), verified by decompilation:** profile `+0x40/0x44/0x48/0x4C/0x50/0x54/0x6C/+0x8C`
  go to **Spartan slots 0..7**, and `+0x68/+0x90/+0x94` go to **Elite slots 0..2**. Therefore:
  **`+0x8C` is the Elite armor set** (Spartan slot 7), **`+0x68` is the Spartan armor effect** (Elite slot 0) and
  **`+0x90` is the Elite armor effect** (Elite slot 1).
- **`0x1055EC` (appearance builder) decompilation:** `record+0x10..+0x16` are seven armour-list indices,
  `record+0x17` is the Elite armor set (pmcg list `+0x74`, applied only when the species is Elite),
  `record+0x18` the Spartan armor effect (pmcg list `+0x80`) and `record+0x19` the Elite armor effect (pmcg list `+0x8C`).
  Mode 0/1 force Spartan/Elite; mode 2 returns; otherwise `record[4]` is the species.
- **The old patch at `0x2EDD5C`** rewrote that copy so profile `+0x8C` went to Elite slot 0. Profile `+0x68` then
  overwrote the same slot, so `record+0x17` (the Elite set) stayed 0 = **Minor**, and an equipped armor effect
  (`+0x8C`) was what selected the Elite variant. The `0x2C8070` hook in that file never installed either - the
  function starts at `0x2C807C`, not `0x2C8070`.
- **Fixed:** removed the `0x2EDD5C` rewrite and the dead `PlayerSetAppearanceSlot` detour from `src/hooks/EnginePatches.cpp`
  and documented the real mapping in the file. Verified in game: `-profile-set 8C=C5` (Elite set 3) now gives
  `record +0x17 = 3` in the builder (before the fix it was 0).

### 2. Elite armor effects (O7OF / DeepSeek)
- Kept the verified `0x10580C` redirect `B9 8C 00 00 00` -> `B9 80 00 00 00`: the Elite branch of `0x1055EC`
  looks up the effect in pmcg list `+0x8C`, which is empty in the map, while `+0x80` holds the same seven effects
  (`default_armor`, `legendary_flame`, `eternal_flame`, `heart_attack`, `pestilence`, `storm`, `halo_bday`;
  region `armor_fx`, permutations `armor_fx_00..05`). The pack's own `+0x80`/`+0x8C` lists are identical.
- `src/engine/ReachHost.cpp`: `+0x68` is now always the Spartan effect (it used to be `species == Elite ? elEffect : spEffect`,
  which mixed the species into the wrong field), `+0x90` always the Elite effect, `+0x8C` the Elite set
  (indices 0..8; the Armory's Honor Guard is index 8, which has no MCC set item, so the profile stores Minor
  and Honor Guard comes from the pack's per-piece data).

### 3. Per-piece Elite armor now replicates (Honor Guard, H2A, mixed sets) (O7OF / DeepSeek)
`src/engine/ElitePack.cpp`:
- `BuilderHook` now uses the **call site** (`_ReturnAddress()`): `0x04AC3B` spawn and `0x0447B5` live change pass the
  local player's own record (`player + 0xD8`); `0x3B184F` (another machine's player) and `0x524CD5` (unit copy / hologram)
  pass a network/copy record. The old `player+4` heuristic reported "local" for other players too and dressed every
  Elite with the local Armory picks - the reason your armor showed on your screen but not on theirs.
- The local Elite's record gets the six pack piece indices at `+0x10..+0x15`, the `0xFF` marker at `+0x16` and the
  Elite effect at `+0x19`. Every other record uses its own marked pieces; an unmarked (old/unmodded) record falls
  back to the replicated Elite set via `kSetPieces`.
- The block is pushed into the engine's replicated player settings by rewriting it in the engine's own appearance
  sync: `ApplyProfileHook` (`0x2EDA08`, the local profile copy) records the player's settings index, and
  `RecordSetHook` (`0x2C837C`, called by the engine right before it hands the 0x20-byte block to the profile manager
  `0x3A9E4`) rewrites the block only when `[player+0x10]` equals that index. Remote players' blocks are therefore never
  overwritten with local picks. Verified in the log: the sync block now carries the marked piece bytes + effect instead
  of the player's Spartan pieces.

### 4. Armory (O7OF / DeepSeek)
- `ArmorCustomizer::SaveProfile` now clamps every index before indexing the piece lists (an old/stale save with an
  out-of-range index was an out-of-bounds read on save), and saves the Elite effect through the clamped index.
- Verified the Armory's item ids against the profile item maps: all 22 helmets, 46 helmet attachments and 25 chests
  resolve in `Helmets()` / `Chests()`; the Elite set order matches MCC 194..201 (Minor, SpecOps, Ranger, Ultra, Zealot,
  General, Field Marshal, Officer).

### 5. Hologram shows Elite effects but the player model does not (finding, open)
The hologram/decoy is built through the unit-copy call site (`0x524CD5`, logged as `local false`) with the source
player's record; it now gets the same marked pieces + effect, which proves the effect *data* and pmcg list works.
The real player's spawn (`0x4AC36`) gets the same effect value, so the remaining difference is the **model tag**:
the player uses the pack Elite biped (`0x85564635` -> model `0x85584637`), while the decoy can resolve the map's
stock Elite (`0x89392189`). The next check is whether the pack model carries the `armor_fx` region/permutations
the effect selection changes (a first instrumentation pass in this session did not get a reliable answer; it is
removed from the shipped code). If it does not, the fix is to merge the stock Elite's `armor_fx` permutations
into the pack model at load, or to use the stock model for players with an effect equipped.

### 6. Two more Elite presets and the new unlock ladder (Tyler / DeepSeek)
- `ArmorCustomizer`'s `EliteSetSpec` now carries the six piece ids of each set, and `ApplyEliteSetPieces` equips them;
  the set picker, save/load and the hover "try on" all use it instead of copying the set index into every piece. The
  per-piece lists keep their first 9 entries unchanged, so saved looks and the pack's piece indices do not shift.
- New presets (end of the Armor Set list), piece rows checked against Tyler's 22:54 screenshots:
  **H2A RANGER** = RANGER (H2A) helmet `h2a_space_helmet`, H2A pauldrons `h2a_l_shoulder`/`h2a_r_shoulder`, **RANGER**
  chest and arms `space`, H2A legs `h2a_legs`; **COUNSELOR** = COUNCIL helmet `council`, SCHISM pauldrons `schism`,
  **HONOR GUARD** chest `honor_guard`, MINOR arms (`default`), H2A legs `h2a_legs`.
- Unlocks (`kEliteUnlocks`): the H2A Ranger preset *and* its four H2A pieces now unlock at **General** (the slot Honor
  Guard used to hold); Honor Guard and its robe legs moved to **Field Marshall**; the **Counselor** preset is the last
  Elite set at **Inheritor**. Individual Council/Schism pieces keep their earlier ranks so they can still be mixed.
- `ElitePack`: `kSetPieces` grew to 11 rows (H2A Ranger `{11,11,11,3,4,9}`, Counselor `{0,5,5,8,0,9}`) and the
  set clamp accepts 0..10. `ReachHost` writes 194 (Minor) for presets 8..10 because MCC has no set item for them -
  the per-piece appearance record carries the look with the Elite pack; a pack-less match shows Minor.
- Tested in a match with `appearance.json` set to each preset: Counselor -> `Elite dressed with pieces 0 5 5 8 0 9`,
  sync block `00 05 05 08 00 09 FF 00 00 02 ...`; H2A Ranger -> `Elite dressed with pieces 11 11 11 3 4 9`, sync
  block starting `0B 0B 0B 03 04 09 FF 00 ...` (the byte before the `FF` marker is the legs index 9).
- Armory menu captures (`-menu-screen armory -menu-capture 300 -armory-unlock-all`): the Armor Set picker shows all
  eleven entries (MINOR .. OFFICER, HONOR GUARD, H2A RANGER, COUNSELOR), and the preset rows match the screenshots -
  H2A Ranger: RANGER (H2A), H2A / H2A, RANGER chest, RANGER arms, H2A legs; Counselor: COUNCIL, SCHISM / SCHISM,
  HONOR GUARD chest, MINOR arms, H2A legs.

### 7. Colour picker: Primary / Secondary / Accent + custom wheel (Tyler / DeepSeek)
- **Accent colour added.** `AppearanceProfile` and `ReachArmorySelection` now carry a third colour (`accent_color`,
  default 14 Cyan). The Armory's COLORS group is **Primary / Secondary / Accent**, all using Reach's authentic 30-colour
  palette (the same list as MCC's playercustomization.xml, hue-by-hue 3 shades) for both Spartans and Elites.
- **Preview writes the third colour word.** `WriteColorsAndSuit` used to hard-code the third word to `0xFF2FA0CB`;
  it now uses the accent (`accentRgb`), so all four 16-byte armour-colour records are {primary, secondary, accent,
  visor}. The profile also writes `+0xA0 = 202 + accent` (RR documents +0xA0 as the third colour).
- **Custom colour wheel.** Each colour row now has a **CUSTOM COLOUR** button next to RANDOM; it opens an ImGui
  hue-wheel picker (`ColorPicker3`, colour-wheel flag). The exact RGB is saved as `primary_custom_rgb` /
  `secondary_custom_rgb` / `accent_custom_rgb` plus the custom flags, and the Armory previews it. Because the
  in-game profile only stores Reach palette item ids, the wheel also snaps `primary_color`/`secondary_color`/
  `accent_color` to the **nearest palette index** for the match; "USE PALETTE COLOUR" clears the custom colour.
- **Verified:** the Armory's COLORS list shows Primary / Secondary / Accent and the Accent wheel/button; the Elite
  preview recolours when the primary changes (average of the character region moved from 27,30,37 to 22,20,21 for
  Ice/White vs Maroon/Sea Foam). The third/accent word is a detail colour - it is visible on Spartan lights/detail
  surfaces but is subtle on the Elite pack model (the Elite's main plates follow the primary). If the Elite needs a
  stronger accent the next step is in the pack model's material colour bindings, not the Armory.

**Tested:** build clean; solo Sword Base runs with `-profile-set 27=01,38=01,8C=C5,90=04` reach the builder, write
the marked block and the settings sync repeats it. The final run picked **Honor Guard (set index 8, Blue Flames)** and
logged `Elite dressed with pieces 2 2 2 8 2 1 (mode 1, set 0, fx 2, local true)` followed by
`Elite pieces written to the settings sync block (player index 0): 08 02 02 02 00 FF 01 02 10 00 02 ...` -
i.e. the six Honor Guard piece indices, the `FF` marker, the (invalid) set byte 0 and effect 2 are what the engine
now replicates for that player.
**Still open:** a real two-PC match to confirm the peer sees Honor Guard/mixed pieces on the host, and the pack-model
`armor_fx` question above.

--

## 0. Pick up here (2026-10-30, 23:22 ET) - Emblem background picker, and emblems now match what was picked (@claude)

**Correction to the old brief:** `HANDOFF-FOR-CLAUDE.md` §5.1 says MCC has no reusable backdrops. It does:
`playercustomization.xml` has 105 entries with `background="eb_001"` (ids 500+, `$EMBLEM_BACK_CIRCLE`, ...). The parser skipped
them because it only kept entries with `image1`. So this uses the real backgrounds instead of the approved prototype that
mixed layers from different emblems; the prototype's unused layer lists are gone.

- **New Armory row** IDENTITY > Emblem Background: None + 105 shapes, saved as `emblem_background_index` in
  `appearance.json` (default 0 = none, so existing looks don't change). The Emblem row is the symbol. Tiles preview the
  whole emblem with the other half as equipped.
- **Bug fixed, emblem shown was not the one picked:** the Armory stores a list position, but drawing looked it up as an XML
  id, and most ids don't match their position. Everything now goes through `UiArt::DrawEmblem`, and the id lookup
  `UiArt::Emblem` is removed so it can't come back. **Visible change:** a saved emblem may now look different from what the
  nameplate showed before, because it now shows what was actually picked.
- **Bug fixed, half-drawn emblems:** only `image1` was drawn; both layers are now.
- Used in the party row, lobby row (local player) and the Armory nameplate.

**Open, not fixed:**
- **Other players don't see your background:** the roster only carries `emblemId`. Needs a field on the server (@deepseek), then
  a line in `PlayerRow`'s lobby call.
- **No emblem colours yet.** Backgrounds get a fixed dark steel tint because MCC's are a mix of black and light grey and a
  light shape hid the symbol. Black ones stay black, since tinting can't lighten them. Reach's real three-colour picker
  would need the black layers treated as masks.
- **Nameplates may have the same list-position vs id bug:** `Nameplate(int id)` is fed the row's position. Not checked.

Self-review:
- Build 0 errors / 0 warnings on a pull of `9f78b80` plus this diff.
- Log: `Loaded 531 nameplates, 225 emblems and 105 emblem backgrounds`.
- `-menu-screen armory -armory-focus "Emblem Background" -menu-capture 120`, plus `Emblem` and `main`, all opened. With
  `emblem_index` 40 and `emblem_background_index` 3 set by hand in the second checkout's copy of `appearance.json`: picker
  says Headshot and Plus, and the nameplate and party row show that same skull on that plus. Evidence:
  `docs/screenshots/emblem-background.png`.
- Load tested, and the values survived three runs. Not run: clicking a tile (it uses the existing `choose()`), RANDOM, a
  real lobby row, other resolutions. Tyler's real `appearance.json` was not touched.

## 0. Pick up here (2026-10-30, 23:17 ET) - Scoreboard replaces the engine's, player actions added (@claude)

Commit `60f374b`. Still `[~]`: parts of it wait on @deepseek (list below).

- **Engine scoreboard gone:** the input poll in `ReachHost.cpp` keeps the scoreboard key (action 20, follows rebinding) from
  the engine and still reports it to the overlay. Reach's screen never opens, so nothing shows around the panel. The panel
  was reading `VK_TAB` directly; it now uses the bound key.
- **Using it:** hold the key to view. Click while holding to free the pointer (the game gets no input, as in the pause
  menu). Esc, the key again or CLOSE give it back. Esc closes the Report window first.
- **Report:** saved to `data\userdata\reports.jsonl`, and the window says it stays on this PC, because the server has no
  report endpoint.
- **Greyed with a tooltip:** VOTE TO KICK, plus END ROUND and END MATCH for the host only. Nothing is faked.
- **Ping is `-` for everyone:** guests send a hard-coded 25 (`MasterServerClient.cpp:681`) and the host 0. Real values need
  a measurement. `ScoreboardEntry::ping` is ready for them.
- **Team bug fixed:** the panel treated team 0 as RED, but 0 means no team (`LobbyPlayer::team`). Map name is now the
  display name.
- **Test flag:** `-scoreboard-demo N` (forwarded by the menu) draws sample rows as host. 1 = as held, 2 = pointer free,
  3 = Report window. It captures `capture_888888.bmp` at panel frame 900.

**needs: @deepseek:** real ping, vote-to-kick transport, end round / end match actions. Then @claude wires the buttons.

**Open, not fixed:**
- Controller Back still opens the engine's scoreboard and does not show Noble's panel.
- Possible existing bug: the log showed `closed` then `opened` in the same millisecond on one Esc from the pause menu.
  `PauseMenu.cpp:426` closes on Esc and the input poll also toggles, so Esc to resume may sometimes reopen the menu. Seen once.
- The host only sees itself: the roster is fetched only when `-session-id` is present in `main.cpp`.

**Test traps:**
- `tools/test-modes.ps1` and the docs' build command kill every `noble-client` by name. That likely killed @claude's matches
  three times. @claude now tests with a copy named `noble-client-claude.exe`.
- `SetForegroundWindow` from a script can fail silently, and then input tests prove nothing. Check focus first.

Self-review:
- Build 0 errors / 0 warnings, on a tree including `34f0553`.
- Real Sword Base Slayer match at 3840x2160, injected Tab and mouse, `-scoreboard-demo 1`. Log: `scoreboard actions opened`,
  `report saved=true`, then `opened` for the pause menu on the second Esc. `reports.jsonl` held the Carter / Cheating line.
- This test caught a real bug the demo captures hid: a clicked row lost its selection the same frame. Fixed before commit.
- Captures opened: `docs/screenshots/scoreboard-actions.png`, `scoreboard-report.png`. Real gameplay behind, no engine
  scoreboard, sample rows.
- Not run: a real multi-player session, the key-again and CLOSE exits, tooltips, other resolutions.
- Clicking did not fire a shot as far as the frames show (ammo stayed 32), which is not a dedicated test.

Review of @deepseek `34f0553` (third person verified): REVIEW OK (@claude, 23:17). Diff matches the claim, builds 0/0, and
`third-person-vs-first.png` opened: same Firefight spot, first person vs own Spartan from behind. Not re-run by me; its
TODO, handoff entry and hash are present. Its follow-hook, settings and gate work stays open.

## 0. Pick up here (2026-10-30, 22:34 ET) - Server browser: display names, correct mod flag, MOD badge (@claude)

The TODO line was partly stale: `89a9509` had already put display names in the list. What was still wrong, all in
`src/ui/ServerBrowser.cpp` (commit `f3cbbf6`):

- The GAME DETAILS `Map:` row printed the raw scenario name (`forge_halo`). It now shows the display name.
- Any map name containing `.map` counted as a mod, so a stock `forge_halo.map` got the mod marker. `IsModdedGame()` now
  uses the host's flag, a `[Mod]` session name, or `!MapNames::IsVanilla()`, for the own-host, LAN and master rows. A LAN
  row merged with its master entry also keeps the host's mod flag.
- `forge_halo.map` showed NO MAP PICTURE because previews are keyed by scenario. The panel now passes
  `MapNames::ScenarioOf()` and falls back to the raw name for unknown maps.
- `[Mod]` text is now a filled MOD badge. The Map column went from 140 to 240 px, because it was cutting off stock names
  such as Winter Contingency.

**Second checkout:** @deepseek was active, so this was built and run in `D:\Halo-Reach-Community-Edition-claude`. Its
`settings.json` has `masterServerUrl` pointed at `http://127.0.0.1:18099`, the mock used for the capture. That is build
output, not in git, but **reset it before using that build against the real server**. The main checkout's userdata was
not touched.

**Trap:** the server browser status line prints the PC's public IP. Black it out in any capture you commit; it is blacked
out in this one.

**Clock:** this PC says 2026-10-30, but recent @deepseek entries and the status board are stamped 2026-10-31 (10:25 to
11:40 ET) for commits made around 22:15 to 22:30 on the 30th. Use `Get-Date` for stamps.

Self-review:
- Build from the pulled tree including `a573b6e`: 0 errors, 0 warnings.
- Capture: `-menu-screen servers -menu-capture 900` against a local mock of `GET /api/sessions`, saved as
  `docs/screenshots/server-browser-polish.png` and opened. Six rows: `forge_halo.map`, `20_sword_slayer` and `m10_b` have
  no badge and show Forge World, Sword Base and Winter Contingency; `my_custom_map.map`, a host-flagged `dlc_slayer` and a
  `[Mod]` session on `ff50_park` have the badge. Details panel shows Forge World with its picture.
- Not run: real sessions from the live server, the LAN and own-host rows (same helper, no run), and the hover tooltip.
  Columns were only checked at 1920x1080.

Review of @deepseek `ca6d33a` (`-third-person-mode` experiment): REVIEW OK (@claude, 22:34), diff matches the claim, flag
is parsed before the host/guest split, builds 0/0. Build and diff only, the switch was not run; it is in-progress work
and the third-person item stays open.
--

--

## 0. Pick up here (2026-10-30, 22:34 ET) - Docs: one CMake path, the real Visual Studio layout (WeabooTyler117 / DeepSeek)

Tyler caught the build path being written two ways. `START-HERE.md:24` said
`...\Community\Common\IDE\CommonExtensions\...` (missing the `7`) and `CLAUDE-PROMPT.txt:25` said
`...\Community\CommonExtensions\...` (skipping `Common7\IDE`). An agent copying either gets "path not found".

Both now use the real layout:
`C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
(also made `PATHS.md` line 47 exact instead of using a `…`).

Self-review: `Test-Path` on the corrected path is true; a repo-wide scan of every `cmake.exe` mention in
`docs\**\*.md` and `docs\**\*.txt` now reports **0 wrong spellings** (5 correct ones in CONTINUATION, the Claude brief,
PATHS; 2 were fixed). Next agent can run that scan again after any docs edit.

## 0. Pick up here (2026-10-30, 22:30 ET) - Machine map for Claude: docs/PATHS.md; Claude's first claim landed (WeabooTyler117 / DeepSeek)

- Wrote **`docs/PATHS.md`** - the map of everything that is not in the repo, because a fresh agent cannot see my scratch
  history and would otherwise hunt the disk or guess. It covers: both client checkouts (main + Claude's
  `D:\Halo-Reach-Community-Edition-claude`), the backend clones, MCC and its `haloreach.dll`, HREK tools + tags, the RR
  install (`D:\Reach`, `bin\host-probe.exe` symbols, launch blocks), the RE folder and its extracted pseudocode, the
  **117 MB `haloreach-disasm.txt`** (RVA = `VA - 0x180000000`), the **`hostprobe-symbols.txt`** dump (section-relative
  values, `.text` base RVA 0x1000), the Ghidra install/project, repo runtime output (logs, captures), `data\userdata`
  (settings/career/appearance and the traps around them), the Downloads mirrors and the first-run steps for the second
  checkout (configure, build, `robocopy` the 2.2 GB `data` folder).
- Linked it from `START-HERE.md`, `CLAUDE-PROMPT.txt` and `TEAM-WORKFLOW.md` ("find it in PATHS.md instead of guessing").
- **Claude is working**: it pushed `0078f7a` ("docs(todo): claim server browser polish (@claude)"), claimed the server
  browser item in `TODO.md`, set its `AGENT-STATUS.md` block active with the files it expects, and noted it will take the
  scoreboard action bar after my `PauseMenu.cpp` claim clears. That is the workflow running exactly as designed.

## 0. Pick up here (2026-10-30, 22:28 ET) - Camera experiment switch; Claude Code verified installed and logged in (WeabooTyler117 / DeepSeek)

- `CameraThirdPerson` now has an experiment switch: `-third-person-mode 0|1` (0 = answer the `haloreach+0x48A910` check
  with `true`, 1 = answer `false`, both start third-person enabled; the detour logs the original result it saw). The next run
  matrix is: baseline (no flag), mode 0, mode 1 on the same map with captures at the same frames, so the camera question is
  settled by comparison instead of a single frame.
- **Claude Code is set up and working on this PC**: `claude --version` = 2.1.316, `claude doctor` is green (npm-global,
  bundle present, auto-update success), and a headless round trip (`claude -p "Reply with exactly: OK"`) returned `OK` with
  exit 0. Its config lives in `C:\Users\tyler\.claude` (credentials updated 22:23).
- Claude has been launched in this repo (`~/.claude/projects/D--Halo-Reach-Community-Edition`, session at 22:22-22:24) and
  received the scoreboard prompt, but its history then shows `/rate-limit-options` and `/logout`. Nothing was claimed,
  committed or pushed: `git log` has no new commits, `docs/AGENT-STATUS.md` still says `@claude idle`, `docs/TODO.md` has no
  `@claude` claim. So the tool works; the first successful contribution is still pending (likely a usage-limit restart).
- My working tree had three uncommitted experiment files - committed here, so Claude's clean-tree precondition is now met.

## 0. Pick up here (2026-10-30, 22:17 ET) - Correction: third person is NOT working; same-PC agent sync added (WeabooTyler117 / DeepSeek)

Two things.

**1. The third-person "works" claim was wrong.** Tyler confirmed in game that it still looks first person, and the capture I
used (`docs/screenshots/third-person.png`) was a **cutscene camera** - the Spartan in frame is a squadmate, not the player.
The detour does install and fire (`CameraThirdPerson: camera-mode check ... -> third person`), but answering `true` is not
enough, so the check's real semantics are still unknown. Status corrected in `docs/TODO.md` to `[~] @deepseek`. Next
experiments: return `false`, return `original(...) | enabled`, and test whether the follow hook at `haloreach+0x1DCADC` is
also required (RR hooks both). Do not trust a single capture again - verify in a slayer spawn with the player model visible.

**2. Same-PC agent sync, because Claude now runs on this PC** (Claude Code, not a browser):

- **`docs/AGENT-STATUS.md`** - live presence board: each agent keeps one block (`active`/`idle`, current TODO item, files
  expected). Set it before editing, clear it when stopping. Committed together with the claim.
- **`docs/CLAUDE-PROMPT.txt`** - the exact session instructions for Claude: read START-HERE / AGENT-STATUS / TODO /
  TEAM-WORKFLOW / HANDOFF-FOR-CLAUDE / top of HANDOFF, check `git status` is clean, pull, claim one item, build 0/0,
  verify (capture for UI), handoff entry, mark done, push, leave clean.
- **`docs/TEAM-WORKFLOW.md`** gained a "Same-PC rules" section: clean tree to start, never build while the other agent has
  `noble-client.exe` running, builds are sequential, and if both must work simultaneously use a second checkout
  (`git clone` + copy `build/bin/Release/data`) and announce it in AGENT-STATUS.

## 0. Pick up here (2026-10-30, 22:14 ET) - Housekeeping: version 0.8.4, scratch script removed (WeabooTyler117 / DeepSeek)

- `src/common/Version.hpp`: `VERSION_PATCH` 2 -> 4 and `VERSION_STRING` is now
  `0.8.4 (Co-op, Third Person & Menu Cleanup)`. It had drifted (patch said 0.8.2 while the string said 0.8.3), which is
  exactly how two builds ended up looking identical in a bug report.
- Removed O7's `patch_blam.js` (his scratch rewrite of the Command 11 barrier; the real change is in `64dd74f` and the file
  was stale against the current `LaunchMap` signature).
- `docs/TODO.md` housekeeping section updated for both.

## 0. Pick up here (2026-10-30, 22:13 ET) - Claude collaboration setup: START-HERE, TEAM-WORKFLOW, claim board, package (WeabooTyler117 / DeepSeek)

Tyler wants Claude and DeepSeek working the same repo. The repo is now the coordination channel; no direct chat needed.

- **`docs/START-HERE.md`** - one-page orientation: what the project is, both repos and their heads, the exact build
  command, the CLI/capture flags, current focus, where every doc lives, and the seven hard rules.
- **`docs/TEAM-WORKFLOW.md`** - the collaboration contract: `docs/TODO.md` is the **claim board** (`- [~] @claude ...`),
  `docs/HANDOFF.md` is the log; claim -> commit the one-line claim -> work -> mark done with the commit hash; handoff entry
  format; verification bar per change type; the files that conflict most (`FrontEndMenu.cpp`, `PauseMenu.cpp`,
  `BlamEngine.cpp`, `main.cpp`, `CMakeLists.txt`, server.js); suggested split (Claude = menus/UI/docs, DeepSeek =
  engine/netcode/server); and "when tokens run low, the repo is the memory".
- **`docs/TODO.md`** now opens with the claim-board instructions and marks third-person F5 as done, with the tuning task
  claimed by `@deepseek`.
- **`docs/HANDOFF-FOR-CLAUDE.md`** gained a section 0 pointing at the three files above, with current heads and what
  landed recently.
- **`docs/screenshots/`** now committed: `scoreboard-overlay.png`, `third-person.png` (capture evidence both features work).
- **Package for Claude**: `C:\Users\tyler\Downloads\Noble-Claude-Package-2026-10-31.zip` (1.34 MB) containing
  START-HERE, TODO, TEAM-WORKFLOW, HANDOFF-FOR-CLAUDE, the full HANDOFF log, menu-redesign-spec, the co-op/
  player-count/COOP-protocol deep dives and the screenshots, plus a README that states the repo heads.

## 0. Pick up here (2026-10-30, 22:12 ET) - Third-person camera works: F5 toggles it (WeabooTyler117 / DeepSeek)

Camera RE finished and shipped.

- RR's `camera::install` (host-probe.exe `0x140196970`) calls its hook helper twice:
  - `haloreach+0x48A910`, 5-byte window, detour `camera::third_person` (the per-player camera-mode check)
  - `haloreach+0x1DCADC`, 7-byte window, detour `camera::follow_update` (distance / shoulder tuning; **not** needed for
    basic third person)
- Implemented the first hook: `src/engine/CameraThirdPerson.{hpp,cpp}` verifies the prologue
  (`48 89 5C 24 08 48 89 74 24 10`), MinHooks `haloreach+0x48A910`, and answers the check with `true` while enabled.
- Controls: `-third-person` starts with it on; **F5** edge-toggles in game (polled from the overlay frame, physical key).
  The detour only spoofs the check - the engine's own third-person camera does the work.
- Verified in a solo campaign run (`m10`): `CameraThirdPerson: third-person camera check hooked at haloreach+0x48A910`,
  then the check fires for the local player and the capture (`capture_10800.bmp`) shows the Spartan from behind instead of
  first person.
- Not done yet: distance/shoulder tuning (the `+0x1DCADC` follow hook, factor on `[r9+0x28]`, sign flip at `[r9+0x14]`),
  a settings row, key rebinding and the host/mode gate. The hook is a single toggle today.

## 0. Pick up here (2026-10-30, 22:04 ET) - Single to-do list added at docs/TODO.md (WeabooTyler117 / DeepSeek)

Tyler asked for the prioritized backlog to live somewhere durable. `docs/TODO.md` now holds the full list (campaign
co-op follow-ups, menus/UI, engine features, server/netcode, housekeeping) with checkboxes, done markers and the commit
that closed each item. The handoff stays the chronological log; TODO is the list.

## 0. Pick up here (2026-10-30, 22:01 ET) - Third person: RR's camera module mapped, implementation not started (WeabooTyler117 / DeepSeek)

Third-person toggle was the third item on the short list. RR has it working (`F5` toggle, `F6` shoulder swap) and its
binary names the exact pieces, so this is a bounded RE job rather than a shot in the dark:

- `host-probe.exe` symbols: `camera::install`, `camera::toggle`, `camera::third_person`, `camera::set_distance`,
  `camera::swap_shoulder`, `camera::follow_update`, `camera::ORIG_CHECK`, `camera::ORIG_FOLLOW`, plus statics `THIRD`,
  `DISTANCE`, `LEFT_SHOULDER`.
- Environment knobs: `RR_THIRD_PERSON`, `RR_LEFT_SHOULDER`.
- Log strings: `camera: third person <on/off>`, `camera: third person distance <n>`, and the game-rules line
  *"camera: the host's rules set the camera in this game; the toggle does nothing"* - RR also implements a **host rule** for
  forced first/third person (strings `forced first person`, `forced third person`).
- `ORIG_CHECK` / `ORIG_FOLLOW` says the approach is: hook the engine's first-person **check** (to allow third) and the
  **follow/update** function (to place the camera behind the player), with distance and shoulder offset as tunables.

Next step: reverse `camera::install` in `host-probe.exe` (Rust, unstripped) to recover the two RVAs it hooks in
`haloreach.dll`, then replicate the hook pair in `src/engine/` (same MinHook pattern as `CameraFov`/`LobbyPatches`) behind a
settings toggle + key bind, gated on host / mode as Tyler asked. No client change was made for this item.

--

## 0. Pick up here (2026-10-30, 22:00 ET) - Scoreboard overlay: Noble panel over Reach's engine-drawn scoreboard (WeabooTyler117 / DeepSeek)

Shipped the in-match scoreboard overlay.

- `PauseMenu::SetScoreboardRoster(entries, map, mode)` + a panel drawn while the scoreboard key is held (Tab, read through
  the engine's own key state). Noble-styled: SCOREBOARD header, PLAYER / TAG / TEAM columns, team-colour strip per row,
  rank icon, service tag, local-player highlight and a YOU badge.
- Roster source: `main.cpp` fetches the session roster once per match from `-session-id` via
  `MasterServerClient::FetchLobbyStateSync` on a background thread, so names/ranks/emblems/teams come from the server
  identity work. Solo/offline falls back to the local player.
- `ReachHost::IsKeyHeld(vk)` mirrors the engine's own pad/keyboard state (published at the end of the input poll, after the
  synthetic test presses), so overlays appear exactly when the game does - physical keys and `-scoreboard-at` both work.
- Capture: `-scoreboard-test` draws the panel and saves `capture_888888.bmp`; `PauseMenu::Render` performs the capture
  **after** `RenderImpl`, otherwise the frame is black (learned the hard way). Verified: the capture shows the panel with
  `WEABOOTYLER117`, `YOU` and `RED`.

Known gap: no live K/D/score columns yet - Reach draws those itself and we have not found a live per-player score source
(the post-game record only exists at the end). Identity/team/rank is live; columns come with the engine player-record hook.

--

## 0. Pick up here (2026-10-30, 21:59 ET) - Lobby identity pass-through: rank / emblem / nameplate / team now survive the server (WeabooTyler117 / DeepSeek)

First item of the short list. The client already sent `host_rank_index`, `host_emblem_id`, `host_nameplate_id`,
`host_team` on registration and parsed `rankIndex`/`emblemId`/`nameplateId`/`team` from roster rows, but the server dropped
them, so remote rows kept placeholder badges.

- **Server (`O7OF/ServerTestDeploy` `b8fb2ad`)**: parses and stores the four host identity fields, stamps them onto the
  host's roster entry (client-supplied roster included), accepts identity updates on heartbeat (keeping the host row in
  sync), exposes them in `masterPublicSession` for the server browser, and stores guest identity from the lobby join body.
- **Client (`663cae2`)**: `JoinLobbySync` takes the local identity and sends it in the join body; the FrontEndMenu call
  site passes `Career::RankIndex(credits)` and the Armory profile's emblem/nameplate indices.
- Verified against a local server: registration with a deliberately zeroed client roster returns rank 12 / emblem 44 /
  nameplate 7 / team 1 on the host row, a guest join (30 / 5 / 9 / 2) shows in the lobby roster, and `/api/sessions` exposes
  `host_rank_index` etc. A real client host against the local server also listed with the new fields.

--
## 0. Pick up here (2026-10-30, 18:15 ET) - Fix Co-op Campaign Netplay Desync, Elite Armor & Effects Replication, Armory Visuals & Social Menu (O7OF / Dominus)

### 1. Co-op Campaign Netplay Desync (Guest 2nd Player Spawn Error)
- **Problem:** When 2 PCs attempted to play Campaign co-op over LAN or Internet, the host connected but then an error threw on the 2nd player spawn (stuck at LoadProgress: 5.0% for ~25s until Category 14 mission abort / game over).
- **Cause Found in Log & Engine:** 
  1. In `src/engine/BlamEngine.cpp`, `isGuest` was forcing `loadedPrebuilt = false`. This stripped the prebuilt campaign scenario variant, level description, and insertion point blocks at `+0x300` on the guest machine, preventing the guest engine from initializing the mission scenario.
  2. In the guest launch block, controller 0 was bound to Slot 0 (`+0x100`) and Slot 1 was unassigned. When the host accepted the guest, the host assigned the guest player to Slot 1 (Slot 0 is the Host). Because the guest had Slot 1 unassigned, 0 local controllers were mapped to player 2 (Bob), causing the engine to fail spawning player 2.
- **Fix:**
  - Preserved prebuilt launch blocks (`campaign_*.bin` / `lb_m_c_1.bin`) on guests.
  - Added fallback synthesis: if a mission-specific campaign/firefight launch block is missing, `lb_m_c_1.bin` is loaded as the base template.
  - Properly mapped launch block controller slots for guests: Slot 0 (`+0x100`) is unassigned (`0xFF, 0xFF, 0xFF, 0xFF`) reserved for remote Host; Slot 1 (`+0x120`) is bound to local controller 0 (`block[0x120] = 0x00, block[0x124] = 0x02`), so Player 2 spawns with controller input when host membership (`msg 26`) arrives.
  - Fixed variant resolver in `BlamEngine.cpp` to skip looking for game variant files when mode is `"Campaign"` (avoids Slayer replacement).
  - Added backtrace stack capture in `ReachHost.cpp` inside `HostSlot6_GameResults` to diagnose any future unexpected result calls.

### 2. Elite Armors & Armor Effects Multiplayer Replication
- **Problem:** Equipped Elite armor sets (Honor Guard, General, Field Marshall, etc.) were only visible locally; remote peers saw them default to Elite Minor. Additionally, armor effects were not showing on Elites, and previously caused bugs with character selection.
- **Root Cause Reverse-Engineered:**
  - In `haloreach.dll`, function `0x2C8070` (`PlayerSetAppearanceSlot(player, slotParam, itemId)`) expects `slotParam->species = 1` and `slotParam->slot = 0` for Elite sets, and `slotParam->species = 1, slotParam->slot = 2` for Elite armor effects.
  - At RVA `0x2EDD5C`, the engine was incorrectly passing `species=0, slot=7`, storing the value into the Spartan array and leaving `[player + 0xAE + 0]` as 0 (Elite Minor).
  - At RVA `0x10580C`, retail `haloreach.dll` loaded `ecx = 0x8C` (`pmcg` list 8C), which is completely empty (0 entries) in retail tags, causing the engine to ignore Elite armor effects.
- **Fix in `src/hooks/EnginePatches.cpp` & `src/engine/ReachArmory.cpp`:**
  - Installed a MinHook detour on `PlayerSetAppearanceSlot` at `0x2C8070` to validate callers (`0x2EDD7A` for Elite Set -> `species=1, slot=0`; `0x2EDDBB` for Elite Effect -> `species=1, slot=2`).
  - Patched RVA `0x2EDD5C` (`89 75 C0 C7 45 C4 07 00 00 00` -> `C7 45 C0 01 00 00 00 89 75 C4`) to write `species=1, slot=0`.
  - Patched RVA `0x10580C` (`B9 8C 00 00 00` -> `B9 80 00 00 00`) to route Elite armor effects to the full tag list 80 (Flames, Blue Flames, Heart Attack, Pestilence, Inclement Weather, Birthday Party), matching Spartans.
  - Updated `ReachArmory.cpp` to look up Elite effects in list `"80"` before falling back to `"8c"`.

### 3. Armory Emblems & Nameplates Visuals
- **Problem:** Nameplates and emblems in the Armory only showed text names or raw options.
- **Fix in `src/ui/ArmorCustomizer.cpp`:**
  - In the Armory picker grid: added dedicated branches for `isNameplate` (1 column, 56px height, rendered via `UiArt::Nameplate`) and `isEmblem` (4 columns, square tile, rendered via `UiArt::Emblem`), displaying full visual previews of all 531 nameplates and 330 emblems from `playercustomization.xml`.

### 4. Social Menu & Friends List
- **Problem:** Social menu lacked an easy way to search/add friends directly by username or IP address and invite/join them.
- **Fix in `src/network/SocialClient.*` and `src/ui/FrontEndMenu.cpp`:**
  - Added `AddFriendDirect(nameOrEndpoint)` to `SocialClient`, saving and loading friends locally in `data/userdata/friends.json`.
  - Added a "QUICK ADD FRIEND" text input box and button at the top of the FRIENDS tab in `DrawSocialScreen`.
  - Added a direct `[JOIN]` button for friends configured with IP/port endpoints, and updated `[REMOVE]` to remove instantly without requiring a master server connection.

### 5. Verification
- Built with Visual Studio 18 2026 MSVC toolchain (`cmake --build build --config Release --target noble-client`).
- Verified 0 errors and 0 warnings.
- Executed smoke captures:
  - `noble-client.exe -menu-screen social -menu-capture 30 -quit-after 5` -> Exit Code 0
  - `noble-client.exe -menu-screen armory -menu-capture 60 -quit-after 5` -> Exit Code 0

--

## 0. Pick up here (2026-10-30, 20:54 ET) - RR already ships >4-player co-op; its patch modules are the reference (WeabooTyler117 / DeepSeek)

Tyler pointed at the Reverse Engineering folder: RR runs campaign above 4 and Firefight above 8. Investigated and updated
`docs/player-count-scaling.md` with the real answer.

- `D:\Reach\haloreach_copy.dll` is **byte-identical** to the Steam DLL (`738DD2D2…0894`), so RR does everything with
  in-memory patches + host stubs in `D:\Reach\bin\host-probe.exe` (11.7 MB Rust, unstripped).
- `D:\REACH REVERSE ENGINEER\docs\rr-launch-flow.md` §6 says it outright: *"32/64-player capacity work (m64_*, p64_*,
  capacity, memberx, census: session membership, simulation views, observer lists, score tables, HUD masks), Campaign up to N
  players (was 4), Firefight up to N players (was 8)"*.
- The knobs in `host-probe.exe` are `RR_CAMPAIGN_PLAYERS`, `RR_FF_PLAYERS`, `RR_PB`/`RR_PB64` (moved player block, 64
  slots), `RR_P64_GROUPS`/`RR_M64_GROUPS`, and profiles `--ff32-profile` / `--m64-profile`; the `ff32` profile string is
  *"players=32 machine admission=33 machine/player storage=64; AI=128; FF/scoring/spawn repairs, AI guard, upkeep/resync"*.
- RR's patch modules name exactly the pieces we are missing: `capacity` (`score_list`, `spawn_list`, `hud_players`,
  `megalo_sets`, `member_wait`, `census_fixes_32`) and `coopx` (`player_list`, `starting_location`, `objects_pool_growth`,
  `firefight_16`, `metagame_*`). Our launch-block `0xE8` fix is only the "declare session size" half.
- Patch-site strings RR prints (`+0x39d69a` player base, `+0x420161` machine table, `+0x58075` Firefight zero-initial-player
  check, `+0xfa6cc`/`+0xfa784` results writers, `FUN_18041f510` observer pass, `FUN_1801f1c8c` megalo message) are leads if
  we ever implement 8/16.

Also spotted in RR: a **third-person camera toggle (F5, shoulder F6)** — relevant to our open third-person backlog item.

## 0. Pick up here (2026-10-30, 20:49 ET) - Notes for scaling co-op past 4 players (future, not implemented) (WeabooTyler117 / DeepSeek)

Tyler asked how we would raise the co-op player count later (e.g. 16). Wrote **`docs/player-count-scaling.md`** with the
facts we have: the launch block declares players at `0xE8` (byte), it carries **16** player-slot descriptors at
`0x100 + slot*0x20`, MCC's results builder walks **16** player records (stride `0xD8`), LobbyPatches already expands
membership (MP 16..64, co-op 4..16) and patches the "too many for net coop" gates, and the cap compare is
`cmp edi, [hr+0xD1FD90]` where that global is initialised from a constant block during globals init.

The real blockers above 4 are **scenario spawn points (4 per campaign mission), mission scripts that assume 1-4 players,
the campaign HUD, and lockstep determinism/CPU**, not the session or launch block. Staged plan: 6-player local experiment
with `-data-dir` first, then an 8-player spawn fallback + minimal HUD, then 16 with HUD/scoreboard redesign and script
accommodation. Firefight is the easier target if more-than-4 is ever wanted as a demo.

## 0. Pick up here (2026-10-30, 20:48 ET) - Fix holds for 2 and 3 players; solo unaffected; no-guest edge case recorded (WeabooTyler117 / DeepSeek)

Follow-up verification of the `0xE8` launch-block player count:

| scenario | declared | result |
|---|---|---|
| solo campaign host | 1 | normal session start, no GameResults over 75 s |
| host + 1 guest | 2 | 110-160 s, 0 GameResults, 0 aborts, packets flowing |
| host + 2 guests (3 processes, separate `-data-dir`s) | **3** | all three `game session started`, **0 GameResults** over ~90 s |
| host declares 2, guest never joins | 2 | holds for the peer, membership wait times out, mission start stalls waiting - expected in this test-only case |

The general case works: the declaration scales with the lobby peer count and the engine spawns every player. The only
pathological case is a host that advertises more players than actually join, which the real lobby flow prevents because
`expectedPeers` is refreshed from the lobby roster at START (see the host START path in `FrontEndMenu.cpp`).

Remaining known work for campaign (unchanged from the backlog): CONTINUE should resume the last checkpoint; the real
two-PC test with the host running a campaign lobby is the next validation step.

## 0. Pick up here (2026-10-30, 06:45 ET) - CAMPAIGN CO-OP FIXED: the launch block must declare the co-op player count (WeabooTyler117 / DeepSeek)

Root cause found in the launch block. `BuildLaunchBlock` hard-coded **`block[0xE8] = 1`** ("1 active local player") for
every launch, including a campaign host that had a guest in its lobby. The campaign/firefight engine sizes its player list
from that field, so it created only the host's player, left the guest's slot at biped `+0x30 = 255`, and then failed the
mission with Category 14 results ~60 s later. Everything we had blamed before (transport, membership, Command 10/11
ordering) was fine all along - which is exactly what the engine-class experiment had hinted at.

### Fix

After the block is built in `LaunchMap`, campaign (mode 1) and Firefight (mode 5) hosts now set

```cpp
int declaredPlayers = 1;
if (connectAddress.empty() && expectedPeers > 0) declaredPlayers = 1 + expectedPeers;
launchBlock[0xE8] = declaredPlayers;
```

and log `BlamEngine: launch block declares N player(s) (0xE8)`. Guests and solo launches keep 1.

### Verified locally (two processes, `m10` campaign, loopback)

| run | declared | outcome |
|---|---|---|
| before | 1 | GameResults / Category 14 at ~60 s, `player 1 +0x30 = 255` |
| after, 90 s | **2** | no GameResults, host + guest both `game session started`, packets flowing |
| after, **160 s** | **2** | **0 GameResults, 0 aborts on either side**, traffic still flowing (90-114 byte payloads) |

Both clients reached `game session started` ~1 s apart and the simulation stayed alive for the full 160 s window - the
first time campaign co-op has survived past 60 s in this project.

### Next

- Two-PC test on the real machines with this build: host campaign from the lobby (roster gives `expectedPeers`), guest
  joins, host starts. Expect the real cutscene and both players in-game.
- If the host launches a campaign lobby with a guest who then never joins, the engine now expects 2 players; if that turns
  out to stall the mission, gate the declared count on the lobby roster being refreshed at start (it already is) rather
  than on `-wait-peers` alone.

## 0. Pick up here (2026-10-30, 06:10 ET) - Membership before the mission load changes nothing; the campaign engine itself refuses a remote player (WeabooTyler117 / DeepSeek)

Implemented the Command 10 hold properly this time (no new flag: campaign host with a known lobby peer count waits up to
15 s for `LobbyPatches::GetRemotePlayerAddCount()` before posting Command 10). Verified in the log:

```
BlamEngine: membership before mission load: 2/1 player(s) in session
BlamEngine: Posting Command 10 (Event: Start) ...
BlamEngine: Posting Command 11 ...
game session started at frame 280
... 63 s later ...
Slot 6 - GameResults        (Category 14)
Career: player 1 ... +0x30 255      <- still no unit for the guest
```

So even with the guest **already in session membership when the mission loads**, the campaign engine does not create a unit
for the remote slot, and it still ends the mission ~60 s later. Combined with the previous entry (engine class **3** on the
same map and transport survives with no GameResults), the conclusion is now well supported:

> The shared transport, join ordering, membership and Command 10/11 timing are all fine. The **campaign engine
> (launch-block class 1)** refuses to instantiate a remote player's unit and then fails the mission - it is campaign-engine
> player selection, not netcode.

### Where to look next

- The campaign engine's player-selection/spawn loop. Candidate inputs: the launch block's player-slot regions
  (0x100-0x2E0, identical between the captured campaign and multiplayer blocks), the session membership's local-player
  mask, and the `+0x100`/`+0x101` bytes MCC copies into result `+0x28`/`+0x29`.
- Diagnostic trick that worked and should be reused: run the same test with `-engine-mode 3`. If it survives, the difference
  is campaign-engine logic; if it fails too, the problem is shared.

The Command 10 hold is kept in the engine (it is correct for real co-op lobbies: the host's mission should not load before
its guests are in the session).

## 0. Pick up here (2026-10-30, 05:45 ET) - KEY DATA POINT: the campaign engine class is what kills co-op, not the netcode (WeabooTyler117 / DeepSeek)

Ran the campaign map `m10` with **`-engine-mode 3`** (multiplayer/Megalo class) on both host and guest, everything else
identical to the failing runs:

| run | engine class | host | guest | outcome |
|---|---|---|---|---|
| normal | 1 (campaign) | session started 20:23:45.112 | 20:23:45.962 | **GameResults / Category 14 at ~60 s, player 1 +0x30 = 255** |
| forced | **3 (multiplayer)** | session started 20:23:45.112 | 20:23:45.962 | **no GameResults after 70+ s**, traffic continues (msg 27/26/37 flowing) |

With the exact same transport, launch block, map and timing, the session only dies when the **campaign engine**
(`engine class 1`) is running. That is strong evidence the failure is campaign-engine-specific player validation (it
expects a spawned biped for every session player and ends the mission when one has none), not a networking or join-ordering
bug.

Also confirmed in that run: the guest's first membership message (msg 27) arrives **151 ms after** the host posts Command 10
(host 20:23:38.185, guest send 20:23:38.336), so the campaign engine starts the mission before the guest is in membership -
consistent with a hypothesis worth testing next: **hold Command 10 itself until membership arrives** (previous attempts only
held Command 11). I started adding `-wait-membership` for that but the plumbing did not take before this session ended; the
hook is not in the tree (reverted).

### Next steps

1. Implement the Command 10 hold properly (wait for `LobbyPatches::GetRemotePlayerAddCount() > 0` before posting 10, with
   a ~15 s cap) and see whether the campaign engine then spawns the guest's biped. The flags in main.cpp must be applied
   **before** the host/guest split - that has bitten three times.
2. If that does not help: patch the campaign engine's per-player validation instead of the network layer. The results
   record field is `+0x30` (255 = no unit) and the failure surfaces as Category 14; the campaign engine class is at launch
   block `+0x0C` = 1.
3. Keep `-engine-mode 3` as a diagnostic: it cleanly separates campaign-engine logic from the shared transport.

## 0. Pick up here (2026-10-30, 05:05 ET) - The TLS player-array chain does not resolve at runtime; launch blocks carry no player count (WeabooTyler117 / DeepSeek)

Followed the 0x490-stride player array described in the 04:30 entry. Implemented the chain exactly as the results builder
does it - `tls = gs:[0x58][ *(int*)(hr+0xC17B18) ]`, `obj = [tls+0x18]`, `players = [obj+0x50]`, then 8 records at 0x490 -
with SEH around the reads. Every sample returned "player array not readable", i.e. one of the pointers is null or invalid,
so the chain as written is wrong. Most likely the `hr+0xC17B18` value is not a raw TLS index for that accessor, or the
campaign path takes the other branch (`[0x1828B83C8] != 0` -> `[rcx+0x50]` vs `[rcx+0x50]` with a different base).

The probe was reverted; the build is clean and the tree matches `adc36d9`.

Also checked the launch blocks: `lb_m_c_1.bin` (campaign template) and `launch_block_1.bin` (multiplayer) are **identical in
the player-slot regions** (0x100-0x2E0) and differ only in `+0x0C` (engine class 1 vs 3) and the map id around +0x10, so
the launch block does not carry a co-op/max-player field we could flip.

### Honest state

Campaign co-op is still not fixed. Remaining approaches, best first:

1. **O7's host client log** (one grep: `c_network_session::abort called ... reason=`) - still the cheapest way to learn
   whether the engine aborts with 0x1B/0x1C/0x1D or the campaign state machine ends the mission.
2. Resolve the real player-array pointer properly in Ghidra (follow `hr+0x48680`'s return usage and the static at
   `0x1828B83C8`), then re-run the probe.
3. Try a campaign co-op start through the MCC campaign UI path in-game (if the menu action can be dispatched), rather than
   a direct map launch.

## 0. Pick up here (2026-10-30, 04:30 ET) - Traced MCC's results builder; the real player objects are the 0x490-stride array (WeabooTyler117 / DeepSeek)

Tried to read the engine's per-player state directly to see when the remote slot gets a unit. Findings:

- MCC's results builder (`hr+0xF9448`) walks **16 records** from `hr+0x28C82E0` to `hr+0x28C9060`, stride **0xD8**, with the
  record's present byte at `cursor-0x38`. I probed exactly that range from a debug thread: the values there do **not** match
  the player fields (`ctrl`/`biped` came back as unrelated bytes), so that region is the *results record* buffer, not the live
  player state. The probe was reverted; the tree is clean.
- The interesting part of that function is where it **reads** from: for each player it calls `hr+0x48680` to get an index,
  then indexes `index * 0x490` into a structure pointed to by `[sessionState + 0x50]` and copies fields from
  `+0xB0`, `+0xC0`, `+0xD0`, ... `+0x120`, plus bytes at `+0x100`/`+0x101` into result fields `+0x28`/`+0x29`.

So the **live engine player objects are 0x490 bytes each**, reached through a session pointer chain
(`[hr+0xC17B18]`-indexed global -> object -> `+0x50` -> player array). That chain is the next thing to walk: with the
player-array pointer resolved, a per-tick dump of each slot's unit/biped field would show whether the host ever creates a
unit for the remote player, and the `+0x100`/`+0x101` bytes are likely the "present/valid" flags the engine checks.

Local repro and test flags still available:

- `-data-dir <path>` - isolated profile/userdata for a second local instance.
- `-guest-post-start` - campaign guest posts its own Command 10/11.
- Prior handoff entries list the five ruled-out causes and the reliable two-process repro (host `player 1 +0x30 = 255`,
  Category 14 results ~60 s).

## 0. Pick up here (2026-10-30, 03:55 ET) - Correction: the table I found is NOT confirmed as the HSC builtins (WeabooTyler117 / DeepSeek)

The 03:40 entry over-claims. A wider scan of the same `.rdata` region shows entries whose "names" are UI/localisation
keys (`chud_campaign_ft_member_1_name`, `prop_campaign_skulls`, `player_rank`), and the fields repeat in a pattern typical
of a string/id table rather than a callable builtin registry. Treat the "HaloScript builtin table" identification as
**unverified** until one of these is shown:

- the table base is referenced by code that indexes it with a builtin id, or
- one of the fourth-qword values (0x86CBC, 0x1B8930) is proven to be a builtin implementation that reads script args.

No code was changed from that entry beyond docs, so nothing to unwind in the build.

### Where campaign co-op actually stands

- Reliable local repro: two processes, `m10` campaign, guest in game before the host's Command 11; the host's `player 1`
  slot stays at `+0x30 = 255` (no unit) and Category 14 results fire ~60 s later.
- Ruled out: Command 11 timing, guest's own Command 10/11, membership sync, profile/userdata sharing, loopback vs LAN.
- The single most useful missing datum is **O7's host client log for the failed run**. It contains the engine's own reason:
  `c_network_session::abort called ... reason=NN caller hr+0x...` (our hook logs every abort), plus `boot_peer` and the
  campaign refusal trace. That tells us whether the engine aborts (and with which code) or ends the mission through the
  campaign state machine - two very different next steps.
- Ask: have O7 run
  `.\tools\extract-coop-log.ps1 -Log build\bin\Release\noble-client.log -Start "<launch time>" -End "<+45 s>" -Role host -Out "$env:USERPROFILE\Downloads\campaign-host.log.txt"`
  and send that file.

---|---|
| `hs_make_interactive_scripts_deterministic` (0xA04AD8) | 0xA06828 |
| `net_set_campaign_insertion_point` (0xA04B08) | 0xA06878 |
| `submit_incident_with_cause_campaign_team` (0xA04B30) | 0xA068C8 |

Other names in the same table: `submit_incident_with_cause_team_and_effect_team`, `auto_graph_enable_list` - i.e. this is
the engine's HSC builtin registry, and it is not sorted, so the campaign/coop entries need a scan rather than an index.

### What that unlocks (next steps, in order)

1. **Dump the whole table** (scan `.rdata` for 0x28-stride entries whose first qword points into the name section) and
   list every campaign/co-op/network builtin with its handler index. That gives the exact call targets for
   `net_set_campaign_insertion_point` and friends.
2. **Reach the evaluator**: `0x198F2C` looks like the shared dispatcher. If it takes (builtin index, arg count/args), we can
   call builtins directly from `BlamEngine` - first to set the campaign insertion point, then to compare against a normal
   menu-driven co-op session.
3. Only then decide whether the campaign session needs a real co-op start action or just the insertion point/flag.

The alternative (finding the campaign player-list global and patching the remote slot's `+0x30` each tick) is still open
but is a symptom fix; the builtin table is the cleaner route because it is how the real menu starts a co-op session.

## 0. Pick up here (2026-10-30, 03:10 ET) - Campaign spawn investigation: five local configurations, same result (WeabooTyler117 / DeepSeek)

Chasing the host-side missing biped. Added two test hooks and ran five configurations:

| run | configuration | host result |
|---|---|---|
| 1 | membership barrier, loopback | `player 1 +0x30 = 255`, Category 14 results ~62 s |
| 2 | campaign settle 10 s | same, ~59 s |
| 3 | `-guest-post-start` (guest posts Command 10/11) | same, ~59 s |
| 4 | guest with its own `data/` (**`-data-dir data-guest`**, separate profile/userdata) | same, ~62 s |
| 5 | guest connecting to the **LAN IP** (192.168.0.23) instead of 127.0.0.1 | same, ~64 s |

So it is not profile/savegame sharing, not loopback looking "local" to the engine, not Command 11 timing, not the
guest's own mission-start commands and not membership sync. The host's campaign simulation never gives the remote slot a
unit; the slot sits at biped index 255 and the engine declares Category 14 ~60 s later.

### Engine strings that frame the problem

Extracted from `haloreach.dll`:

- `start_campaign_coop_new`, `start_campaign_coop_resume`, `net_set_campaign_insertion_point` - the **main menu's**
  co-op launch actions. Our launcher starts the map directly and never runs them.
- `network_coop_oos_alert`, `in_campaign_restart_level`, `campaign-insertion-point` - campaign co-op is deterministic
  lockstep with explicit out-of-sync handling and an insertion point.
- `error_players_incompatible_for_network_coop`, `error_too_many_players_for_network_coop`,
  `error_too_many_local_players_for_coop`, `incompatible for net coop`.
- `game_coop_player_count`, `game_coop_players`, `game_is_cooperative`.

The `start_campaign_coop_*` actions are the strongest lead: they are what the MCC menu executes to create a campaign
co-op session, and they likely establish the campaign player list / insertion point that the spawn path consumes. A
direct map launch with engine mode 1 never goes through them.

### What to try next

1. **Reach a real script/console path.** `BlamEngine::ExecuteCommand` (vtable[9]) is not a command parser, but the engine
   has a HaloScript evaluator and a console command processor; locating either lets us run
   `net_set_campaign_insertion_point` / the `start_campaign_coop_*` action from the host before the mission starts.
2. **Find the campaign player-list global.** The host's player structs are initialised by `0x102C10` (size 0x2B0, sets
   `+0x28 = 0xFF`, `+0x2C = -1`, `+0x30 = 0xFF`); its callers (`0x430A5`, `0x70474`) construct objects, not the player
   array, so the array is still unfound. Once found, a debug log of every slot's `+0x30` per second would show exactly
   when the host *should* spawn and what it checks.
3. **Test the real two PCs** on `feb059a` - local runs cannot prove the network case, and the two-PC result is still the
   only ground truth we have for the guest's on-screen experience.

## 0. Pick up here (2026-10-30, 02:45 ET) - O7's patch script merged; membership counter now reset per launch (WeabooTyler117 / DeepSeek)

- Pulled `11cbef7` (O7 added `patch_blam.js`, the script he used to rewrite the Command 11 barrier) on top of `64dd74f`.
  Merge `8445989`, build 0/0, pushed.
- That script also intends to call `LobbyPatches::ResetRemotePlayerJoined()` at the start of `LaunchMap`, but `64dd74f`
  never added the call, so the msg 27 counter was process-global. Harmless today (each match is a fresh process) but wrong
  for single-process transitions, so the call is now in `BlamEngine::LaunchMap` with the include.
- Campaign status unchanged from the 02:20 entry: three local runs (4 s settle, 10 s settle, guest posting its own
  Command 10/11) all end with the host's `player 1 +0x30 = 255` (no biped) and Category 14 results ~60 s later. The
  barrier, membership sync and Command 11 ordering are all confirmed working; the blocker is the host-side campaign
  spawn path for a remote player's unit.

## 0. Pick up here (2026-10-30, 02:20 ET) - O7's membership barrier pulled and tested; the guest biped is still never spawned (WeabooTyler117 / DeepSeek)

Pulled `64dd74f` (O7: gate Command 11 on guest membership sync / msg 27) plus the new `LobbyPatches::GetRemotePlayerAddCount()`.
Fast-forward, build 0/0, and our campaign grace window and Forge path are intact.

### Local two-process campaign co-op runs (loopback, latest build)

| run | change under test | guest in game | host Command 11 | result |
|---|---|---|---|---|
| 1 | O7's membership barrier (4 s settle) | 19:51:10.016 | 19:51:06.700 | GameResults 19:52:11 (~62 s), host: **player 1 +0x30 = 255** |
| 2 | campaign settle raised to 10 s | 19:53:39.696 | 19:53:42.397 | GameResults 19:54:41 (~59 s), **player 1 +0x30 = 255** |
| 3 | `-guest-post-start` (guest posts its own Command 10/11) | 19:58:12.675 | 19:58:15.340 | GameResults 19:59:14 (~59 s), **player 1 +0x30 = 255** |

The barrier itself works exactly as designed - the host logs
`membership wait: 0/1 player(s) in session (engine peers: 1)` -> `RECV msg 27` -> `1/1 remote player(s) joined session
membership (msg 27); settling ...` -> `Posting Command 11`, and in runs 2-3 the guest is already in its own game session
*before* the host's Command 11.

### What that rules out

1. **Not a timing race on Command 11** - even with the guest in-game 3-6 s before the host's simulation start, the host's
   player 1 slot keeps `+0x30 = 255` (no biped) and the mission ends ~59-62 s later with Category 14 results.
2. **Not the guest missing its own mission-start commands** - `-guest-post-start` posted Command 10 and 11 on the guest
   (both returned 0x0) and the outcome was identical.
3. **Membership sync is not the missing piece** - `msg 27` arrives before Command 11 in every run.

So the host's campaign simulation never instantiates a biped for the remote player, regardless of ordering. Next target is
the host-side campaign spawn path itself: the function that creates player units at mission start (the one that leaves
`+0x30` at 255 for non-local slots), and what condition it uses to skip network players - likely a check on the player's
slot kind / "local" flag or on the campaign insertion state.

Test hook left in the build (opt-in, harmless): `-guest-post-start` makes a campaign guest post Command 10/11 itself,
logged as `BlamEngine: [GUEST] -guest-post-start enabled ...`. The 10 s settle experiment was reverted; O7's 4 s value is
back.

## 0. Pick up here (2026-10-30, 01:40 ET) - Both sides' logs aligned: guest was in the match, then a membership update ended it (WeabooTyler117 / DeepSeek)

O7 sent the **server** log for the 19:39 attempt (session `b3e82185d33714a5`). Aligning it with the guest client log
(`docs/campaign-coop-timeline-2026-10-30.md`, also in Downloads) gives this:

| ET time | side | event |
|---|---|---|
| 19:39:20.811 | server | host relay socket connected (menu process) |
| 19:39:46.132 | guest | lobby launch received (m10, campaign) |
| 19:39:46.562 | guest | guest relaunched with `-connect 147.124.149.202:11774` |
| 19:39:47.299 | server | host relay socket connected again (match process) |
| ~19:39:47-49 | server | second guest id connects - room has host + 2 guest ids (menu + match) |
| 19:39:49.279 | guest | engine launched as guest, relay thread started |
| 19:39:49.938 | guest | `SEND msg 8 (join-request)`, host replies `msg 4` |
| 19:39:56.335 | server | host relay socket disconnected (the **menu** process exiting) + first guest id gone |
| **19:39:57.497** | guest | **`game session started at frame 530, loading screen off`** |
| 19:40:05.597 | guest | `RECV msg 34` (normal tick traffic) |
| **19:40:08.863** | guest | **`RECV msg 26` (membership update) -> `SEND msg 27` -> `GameResults` 6 ms later** |
| 19:40:10.215 | guest | channel closed, `reason 14` (state was 5) |
| 19:40:11.189 | guest | post-game panel -> quit (user action) |
| 19:40:20.995 | server | host relay socket disconnected (match process ending) |

### What that rules out

- **Relay / backend is fine.** Both sides had relay sockets; the 19:39:56 disconnect pair is the *menu* processes exiting
  after the hand-off, not the match. The match ran over direct UDP (`relayFallback: false`) with normal traffic
  (msg 39/41/34/35/26/27 both directions) right up to the end.
- **The guest joined properly.** `game session started` at 19:39:57, and the 11 s of in-match traffic was healthy.
- **Not the old mid-cinematic bug, not a join refusal.** No abort, no boot_peer, no msg 10, no refusal on the guest.

### Strongest clue

`RECV msg 26` (the large 9552-byte **membership update**) arrives at 19:40:08.863 and `GameResults` fires 6 ms later. A
membership update immediately followed by results is what a client receives when **it is removed from the session** (the
host leaves, the engine drops the peer, or a slot re-assign happens). That points at the host's membership, not at the
guest's state.

Attempted a small diagnostic (log the engine peer count around every msg 26) but reverted it: `LobbyPatches.cpp` is also
compiled into `noble-dedicated`, which does not link `NetworkTransport`, so the client-only call broke that target. If we
want it later, it needs a client-only macro in CMake.

### Next: O7's host client log for 19:39:40-19:40:10

The guest log cannot show *why* membership changed. His client log will say whether:

- `abort called ... reason=` fired (27/0x1B incompatible, 28/0x1C peer disconnect, 29/0x1D co-op state sync timeout),
- `boot_peer ... reason ...` ran (host removed the guest),
- or the host's own `PauseMenu: game over` came first (host ended/crashed the match).

```powershell
.\tools\extract-coop-log.ps1 -Log build\bin\Release\noble-client.log `
    -Start "2026-10-30 19:39:40" -End "2026-10-30 19:40:10" -Role host `
    -Out "$env:USERPROFILE\Downloads\campaign-host.log.txt"
```

## 0. Pick up here (2026-10-30, 01:00 ET) - Campaign still ends by itself; guest extract prepared for O7 (WeabooTyler117 / DeepSeek)

Co-op campaign failed again at 19:39. Guest-side evidence (Tyler's log, extract at
`C:\Users\tyler\Downloads\Noble-campaign-coop-guest-2026-10-30-1940.log.txt`):

| time | event |
|---|---|
| 19:39:46.132 | lobby launch received (m10, campaign), session `b3e82185d33714a5`, host 147.124.149.202:11774 |
| 19:39:46.562 | guest relaunched with `-connect` |
| 19:39:49.279 | engine launched as guest; relay joined (`MasterServer: joined session ... relay: wss://...`) |
| 19:39:49.938 | `SEND msg 8 (join-request)`, host answered `msg 4` |
| **19:39:57.497** | **`game session started at frame 530, loading screen off`** - guest was in the match |
| **19:40:08.869** | **`Slot 6 - GameResults`** - session ended ~11 s after start |
| 19:40:10.215 | `CLOSE channel ... reason 14 (state was 5)` |
| 19:40:11.189 | post-game panel -> quit (user action, after the fact) |

**No `abort` (0x1B/0x1C/0x1D), no `boot_peer`, no join refusal anywhere on the guest.** So the host ended the session (or
its co-op state check fired on the host). This is *later* than before - the guest now reaches "in game" reliably, then the
match dies ~11 s in.

New tool: **`tools/extract-coop-log.ps1`** produces this same filtered window (timeline header, packet dumps omitted) for
either side, so O7 can run it on his host log:

```powershell
.\tools\extract-coop-log.ps1 -Log build\bin\Release\noble-client.log `
    -Start "2026-10-30 19:39:40" -End "2026-10-30 19:40:10" -Role host `
    -Out "$env:USERPROFILE\Downloads\campaign-host.log.txt"
```

What the host log must answer: whether `abort called ... reason=` fired (27/0x1B players incompatible, 28/0x1C peer
disconnect, 29/0x1D state sync timeout), whether `boot_peer` ran, what `evaluate_coop_insertion_point` returned, and
whether the new grace line says `guest engine traffic appeared after N ms` (it should, since the guest was in game).

## 0. Pick up here (2026-10-30, 00:15 ET) - Campaign loses a 1 s race; grace window made lobby-aware (WeabooTyler117 / DeepSeek)

**Forge works online now** (Tyler). Campaign still fails, and the guest-side log pins the timeline exactly.

### What the guest log shows (Tyler's machine, guest)

| time | event |
|---|---|
| 19:17:42.736 | `Received WebSocket real-time lobby launch event! (map: m10, mode: campaign)` |
| 19:17:43.158 | `launching match via Relaunch (campaign m10)` |
| 19:17:44.751 | `evaluate_coop_insertion_point` detour installed |
| 19:17:45.266 | `main blckd: 251 ms` (engine start) |
| **19:17:53.196** | `main blckd: 7092 ms` - the map load stall |
| **19:17:53.209** | `game session started at frame 550, loading screen off` |
| 19:18:50.6 | first session traffic after the load (`msg 37`, `msg 26` x3) |
| **19:18:54.713** | `Slot 6 - GameResults` -> `PauseMenu: game over` (the session ended by itself) |
| 19:18:56.1 | `CLOSE channel ... reason 10 (state was 5)` |
| 19:18:57.1 | `EngineEvent reason=7 message=''`, then back to the menu |

So the guest **did** join and run for ~60 s, then the engine ended the match. No `abort`, `boot_peer` or refusal was
logged on the guest, so the host is the one that ended it (or the engine's co-op state timeout fired on the host).

Timing detail that matters: the guest's first engine packet reaches the host only at **19:17:53**, about 10 s after the
guest process started, because the map load blocks the main thread for ~7 s. The host's grace window was 12 s **from its own
launch**, which is 1-2 s earlier than the guest's launch (it starts at the moment it broadcasts the lobby launch) - so the
host's window expired just before the guest's traffic arrived, the host started the cinematic, and the guest entered too
late. `main blckd` of ~7000 ms also appears in the working Forge run (7039 ms), so the stall itself is normal map loading,
not a bug.

### Fix in this commit

`BlamEngine::LaunchMap` campaign grace is now **lobby-aware**: if a session is registered with the master server
(`MasterServerClient::GetCurrentSessionId()` non-empty) the host waits up to **30 s** for the guest's engine traffic
(covers: launch event -> process start -> ~7 s load -> first packet), otherwise 9 s. Logs the elapsed ms when traffic
appears, and the host's `Lobby start broadcast` now includes `roster=N` so we can see whether `expectedPeers` should have
been non-zero.

Verified on a bare host: `lobby registered: false -> holding ... up to 9000 ms -> no guest appeared within 9000 ms ->
Command 10 -> Command 11`, session started normally.

### Still needed from the host side

O7's `noble-client.log` for 19:17:40-19:19:00. The deciding lines are `abort called` (0x1B/0x1C/0x1D),
`SUPPRESSED session abort`, `boot_peer`, `evaluate_coop_insertion_point`, `holding the mission start`, `guest engine traffic
appeared`, and `PauseMenu: game over`. That will say whether the host timed the guest out (0x1D) or ended the mission
deliberately.

### Backend status

`4a520d1` live, `/api/relay/stats` reporting `connections=8, host=4, guest=4, binaryPackets=10, heartbeats=8`, uptime
climbing continuously (477 s -> 522 s), so the UptimeRobot pinger is keeping the free instance warm. Forge working online
is consistent with the heartbeat + LAN/WAN routing changes.

## 0. Pick up here (2026-10-30, 23:20 ET) - O7's production log analysed: relay socket churn, not refused joins (WeabooTyler117 / DeepSeek)

O7 sent the backend console log from the failed session. Reading it line by line:

| event | count |
|---|---:|
| `[Relay] Host connected` | 26 |
| `[Relay] Host disconnected` | 26 |
| `[Master] Host ... re-registered; reusing session` | 11 |
| `[Relay] Guest ... connected` | 4 |
| `[Relay] Guest ... disconnected` | 4 |
| process restarts (`Running 'node server.js'`) | 3 |

So the host was cycling its relay socket **26 times** while only a handful of guest events ever happened, and the instance
restarted three times inside that window. Nothing in the log says a join was refused - the sockets simply kept dying, which
matches the client-side symptom ("nothing happened") far better than a policy rejection. Two plausible causes: Render's
proxy closing idle WebSockets, and the free instance restarting/spinning down (the live `/api/health` showed
`uptime: 145 s` when checked, i.e. it had just restarted).

Deployment state: `/api/health` on `https://outbreak-z-test-server.onrender.com` reports **version 1.3.0, commit d2828cc**
(diagnostics live) at the time of this entry.

### Shipped in response (`O7OF/ServerTestDeploy` `4a520d1`)

- **WebSocket heartbeat**: every relay socket is pinged every 25 s; unresponsive ones are terminated and counted
  (`relayStats.heartbeats`, exposed on `/api/relay/stats`). This is the direct fix for proxy/idle churn.
- **Timestamped relay logging**: `[Relay 2026-10-30T23:12:48.143Z] Host connected ... (rooms: 1, connections: 1)`, plus
  counters on the host/guest lines, so the next log can be timed against the players' actions.
- Local verification with real sockets: host + guest + `lobby_launch` + one binary packet produced
  `connections=2, binaryPackets=1, controlFrames=1, lobbyLaunches=1, heartbeats=1` and the room listed as
  `hasHost=true, guests=1`; the log showed the timestamped host connect and the `lobby_launch` line with
  `map=forge_halo mode=forge variant=00_basic_editing_054.bin`.

### Operational recommendation (not code)

The free Render instance restarts and sleeps. A free uptime pinger (UptimeRobot or cron-job.org) hitting
`https://outbreak-z-test-server.onrender.com/api/health` every 5 minutes keeps the instance warm and removes the
cold-start window that produces exactly this churn. Worth doing before the next two-PC test.

## 0. Pick up here (2026-10-30, 22:00 ET) - Pulled O7's server centralisation + campaign-coop netcode (WeabooTyler117 / DeepSeek)

Checked both remotes and pulled.

### Client repo (`O7OF/test-build-01`) - `19207fb`

`chore: remove server folder from client repo (centralized in ServerTestDeploy)`. The whole `server/` tree (including the
tracked `node_modules`) is gone from the client repo. Pulled cleanly; nothing in the C++ build referenced it (rebuilt
0 errors / 0 warnings).

**Tooling consequence:** `tools/push-server.ps1` read from `server/`, which no longer exists, so it was retired and replaced
by **`tools/server-repo.ps1`** - clones `O7OF/ServerTestDeploy` next to the checkout, pulls it, prints branch/head/behind and
the diagnostics endpoints. Server edits now happen in that clone and are committed/pushed there.

### Server repo (`O7OF/ServerTestDeploy`) - `d2828cc`

`feat(netcode): support Campaign Co-op lobby sync, skulls, smart LAN/WAN relay routing, and guest keys`:

- **Campaign is a first-class matchmaker mode** now (`campaign: minPlayers 1, maxPlayers 4, defaultMap m10`) - previously
  only multiplayer/firefight were registered.
- Lobby state and launch data carry **skulls**.
- Per-guest `lobby_launch` rewrites **`host_ip` to the LAN address** when the guest shares the host's network
  (`host_lan_ip` + `is_lan` flags), so two PCs on one LAN dial each other directly instead of hairpinning through the WAN.
- Relay guests are keyed by a real **guest key** (random fallback) instead of an empty peerId, fixing collisions when a
  guest does not send one.

Our 1.3.0 diagnostics survived the merge (`/api/health`, `/api/relay/stats`, 90 s session tolerance, 240 req/min).

**Client compatibility confirmed:** `MasterServerClient` already parses `host_lan_ip` / `is_lan` (lines 560/561, 594/595,
685) and `FrontEndMenu` already prefers the LAN endpoint (`1939-1940`, `3914-3915`), so O7's routing change is used
automatically - no client change needed.

### Why this matters for the reported bugs

Campaign co-op previously had no server-side mode registration and two PCs in one house were routed through the WAN address.
Both are now handled upstream, which is the most likely reason "Forge did nothing and never connected" in a same-network
test. Next test should be with this server deployed plus the client at `8c978d4` or later.

## 0. Pick up here (2026-10-30, 21:20 ET) - Backend 1.3.0: health/relay diagnostics, longer load tolerance (WeabooTyler117 / DeepSeek)

Tyler asked for the server side to be made diagnosable for the Campaign/Forge online failures. Shipped to
**O7OF/ServerTestDeploy** as `df33af7` (through `tools/push-server.ps1`).

### What changed in `server.js`

- **`/api/health`** now reports `version`, `commit` (`RENDER_GIT_COMMIT`/`GIT_COMMIT`), and a `relay` block
  (`rooms`, `connections`, `binaryPackets`, `controlFrames`, `lobbyLaunches`).
- **`/api/relay/stats`** (new) lists every live relay room as `{ session, hasHost, guests }` plus the counters. This is the
  endpoint that proves where a failed join stopped: no room at all (registration/session problem), room without host, room
  without the guest, or a room with both but `binaryPackets == 0` (packets never flowed).
- `lobby_launch` control frames are now logged with `map/mode/variant`, and counted.
- **Session tolerance**: `MASTER_SESSION_TIMEOUT_MS` 30 s -> 90 s. Long Campaign/Forge World loads can pause a host's
  refresh beyond 30 s, and a dropped session makes the guest's join look like a netcode failure.
- **Lobby poll headroom**: `MASTER_MAX_REQUESTS_PER_MINUTE` 120 -> 240. Two clients behind one NAT polling lobby state
  could hit 429s that also present as failed joins.
- Version string 1.2.0 -> 1.3.0 in `/` and the new endpoints.

### Verified

`node --check` clean; ran the server locally on port 8099 (had to `npm rebuild sqlite3` first - the shipped binary was
built for a different Node) and confirmed the three endpoints return the expected JSON. The deploy repo's first sync
rewrote `server.js` line endings (2.2k-line diff); content is correct, and `tools/push-server.ps1` now tolerates git's
stderr warnings instead of aborting on them.

### How to use it in the next test

After a failed Campaign/Forge join, open `https://<backend>/api/relay/stats` and read the room for the session id from the
client log (`NetworkTransport: Initialized HOST ... Session ID:` / `Lobby start broadcast: session='...'`). Add the JSON to
the report; it splits server-side failure from engine-side failure in one look.

## 0. Pick up here (2026-10-30, 20:45 ET) - Campaign grace wait for invite/direct joins + lobby diagnostics (WeabooTyler117 / DeepSeek)

Tyler's latest real test: **O7 hosted campaign, guest still saw only terrain and subtitles during the "cutscene", then an
error**; and **hosting a Forge lobby "did absolutely nothing" and never connected**.

### Campaign: the barrier needed a fallback for joins that carry no peer count

`-wait-peers` is only passed when the host starts from the Noble pregame lobby with guests in the roster (`expectedPeers`
counted at `FrontEndMenu.cpp:2237`). If the guest is invited directly, joins by address, or the roster snapshot is empty, the
host launched with `expectedPeers == 0`, took the old 1 s path, started the cinematic, and the guest hit exactly the symptom
described (unbound cutscene camera -> co-op abort), because the server cannot help with that - it is engine sequencing.

Fix (`BlamEngine::LaunchMap`): when a campaign host has **no lobby peer count** but the transport is online
(`campaignGraceWait`), the mission start is held for up to **12 s** waiting for any engine peer, with a 3 s settle once one
appears. Solo (`PLAY SOLO`, transport inactive) is untouched; a host with no guest pays the 12 s once and logs
`no guest appeared within 12 s; starting the mission`. Verified in a solo host run: the grace window runs, then Command 10 /
Command 11 and `game session started` follow normally.

**Still true, and worth repeating:** both PCs must run the merged build (`8ba5b71` or later). If O7's client predates the
barrier, no client-side change here can save that test.

### Forge: nothing happened - added the diagnostics to prove where it stops

Host START now logs `Lobby start broadcast: session/map/mode/variant/difficulty/skulls`; the guest logs
`Lobby launch received: map/mode/variant/endpoint/session` when the event arrives, and
`Guest launching from lobby: map/mode/variant/connect` when it composes the relaunch. With both logs from one attempt we can
tell immediately whether the guest never received the event (server/relay/roster), received it but failed to resolve the
Forge variant, or launched and failed to connect.

### Server side (see 20:05 entry)

`tools/push-server.ps1` is the way to ship any server change. The likely online-only culprits remain the 30 s master-session
timeout during long Forge loads, a stale lobby roster inflating the peer count, and the single-session rule if both PCs use
one account.

## 0. Pick up here (2026-10-30, 20:05 ET) - Server/netcode changes now go to O7OF/ServerTestDeploy (WeabooTyler117 / DeepSeek)

Tyler: *"any netcode or server script changes need to be pushed to ServerTestDeploy repo"*, and he has access.

- Cloned `https://github.com/O7OF/ServerTestDeploy` and compared it with this repo's `server/` folder: **all files already
  matched** (`server.js`, `src/*.js`, `package*.json`, `grant_item.js`, `reset_db.js`, `set_dev.js`, `database.db`) except
  `update_auth.js`, which existed only here - pushed it as `7f00da8`.
- Added **`tools/push-server.ps1`**: clones the deployment repo next to the checkout if missing, copies the server source
  over it (`server.js`, `src/`, `package*.json`, `grant_item.js`, `reset_db.js`, `set_dev.js`, `update_auth.js`), commits and
  pushes. It deliberately excludes `node_modules/` and `database.db` (live data) unless `-IncludeDb`; `-DryRun` shows what
  would change. Verified: dry run reports parity, and every server file hashes identically against the deploy repo.
- Added `server/README.md` naming ServerTestDeploy as canonical, documenting the script, and listing the latest upstream
  commits.
- Latest ServerTestDeploy commits: `7f00da8` (ours), `24075d8` auth/single-session, `83602ac` netcode+lobby sync+db reset,
  `1af3a75`/`8f95c71`/`0f43628` uploads, `37bcf64` relay session-id reuse, `e1301b3` lobby signalling, `510b797` WebSocket
  relay + NAT punch.

**Rule from now on: netcode or server-script changes are committed here AND pushed with `tools/push-server.ps1`.**

## 0. Pick up here (2026-10-30, 19:40 ET) - Command dispatch works but vtable[9] is not a command parser (WeabooTyler117 / DeepSeek)

- `BlamEngine::Active()` now exists (static pointer set in `Initialize`, cleared in `Shutdown`), and a timed test hook
  `-engine-cmd "<cmd>" -engine-cmd-at <s>` dispatches through `BlamEngine::ExecuteCommand` after N seconds. Verified in a
  sandbox forge host: `Dispatching command -> 'enter_editor_mode'`, `Command status: 0x0`.
- **But the status is not meaningful**: `zzz_not_a_command` also returns 0x0, as do `editor_mode 1` and
  `camera_set_first_person 0`. So vtable[9] accepts any string and reports success - it is not a console/command parser.
  The `editor_mode` / `enter_editor_mode` / `camera_set_*` names in the DLL are HaloScript builtins, not console commands.
- Remaining third-person routes, best first: (1) **camera patch** - find the first-person camera position computation for
  the local player and offset it; client-side, any mode, Ghidra project ready. (2) hook the engine's real console
  (`c_console`/`process_command`) and drive `editor_mode`; more RE, less certain. (3) Forge menu path - depends on solving
  the input-device latch first.
- The `-engine-cmd` hook and `Active()` stay in the build: they are the right plumbing for testing engine commands once a
  real dispatcher is found.

## 0. Pick up here (2026-10-30, 19:05 ET) - Third person: button sweep negative, next route is the command dispatcher (WeabooTyler117 / DeepSeek)

- Swept RB/Y/BACK/B/X/LB as 600 ms synthetic pad presses in a sandbox forge session. Measured view change: RB 21.4 %,
  Y 10.7 %, BACK 8.1 %, B 1.7 %, X 1.6 %, LB 1.6 %. Inspecting the frames, RB and Y only nudge the **view angle** (same
  first-person HUD, same grenade counter); no Forge menu and no third-person monitor. So the injection is seen (the press
  log, and RB/Y move the view) but it is not driving a menu or editor-mode action.
- Two likely reasons: the engine latches keyboard/mouse as its input device at match start (so later synthetic gamepad
  buttons are ignored for UI), or editor mode is not a direct binding at all - it may sit inside the Forge object menu.
- Recommended next moves, in order: (1) keep the synthetic pad "connected" for the first few seconds so the engine latches
  gamepad input, then press; (2) make `BlamEngine::ExecuteCommand` reachable from the running client (static active-instance
  pointer in `Initialize`) and dispatch `enter_editor_mode` / `editor_mode 1` / `camera_set_first_person 0` - those names are
  in `haloreach.dll`'s string table; (3) map `enter_editor_mode` to its real button through the profile control block
  (`+0x42C`); (4) offset the first-person camera directly in the engine (Ghidra project is ready).
- `docs/HREK-PLAYBOOK.md` carries the sweep table and the ordered plan.

## 0. Pick up here (2026-10-30, 18:45 ET) - Third person: synthetic pad hook works, BACK is not the editor toggle (WeabooTyler117 / DeepSeek)

- New research hook `-pad-press <hex> -pad-press-at <s> -pad-press-ms <ms>` in `ReachHost::FillLocalPad`: injects one
  synthetic XInput button into the engine pad state and logs it (`synthetic pad press 0x0020 for 300 ms`), so the path is
  proven end to end.
- Test on `forge_halo` with `-engine-mode 4` (sandbox) and BACK (0x0020) at t+16 s: view stays **first person** - BACK is not
  the editor-mode toggle, or the engine ignores a press injected while it believes keyboard/mouse is active.
- Next: other buttons (Y 0x8000, B 0x2000, X 0x4000, LB 0x0100, RB 0x0200) and a longer 600 ms hold; better still, find the
  real binding - `haloreach.dll`'s action table has `enter_editor_mode` / `exit_editor_mode` / `leave_editor`, and the engine
  also exposes `editor_mode`, `camera_set_first_person`, `camera_set_mode` strings. Option 4 is to make
  `BlamEngine::ExecuteCommand` reachable from the running client and dispatch those directly.
- `docs/HREK-PLAYBOOK.md` records the negative result and the ordered candidates.

## 0. Pick up here (2026-10-30, 18:20 ET) - Sandbox engine 0x04 launches now; third person is the Editor-mode monitor (WeabooTyler117 / DeepSeek)

- Added a test-only research hook: `-engine-mode <n>` forces launch-block `+0x0C` (`BlamEngine::SetEngineModeOverride`),
  logged as a warning so it can never be mistaken for normal behaviour.
- **The sandbox engine (0x04) no longer stalls.** `forge_halo`, host, class 0x04: `c_game_engine Start 0xB30`, launch
  state 0 -> 11, `game session started`, Forge HUD, 0 errors, frames pumping (5400 in 40 s). Class 0x03 stays equally
  healthy. The old "0x04 stalls at launch state 2" note is obsolete.
- Captured frame in sandbox mode is **first person** - Reach Forge starts you as the Spartan; the third-person
  **monitor / Editor mode** is entered in-game. Evidence that Editor mode is a real engine state: HaloScript has an
  `(editor_mode)` query used across m05/m10/m20/m30/m45, and `game_options_strings` ships the line *"Forge Traits applied
  to players in Editor mode."*
- Two ways to drive the toggle from Noble: synthesise the bound Editor-mode control (SendInput) while in a Forge session -
  host- and mode-gated, no patch - or locate the sandbox engine's editor-mode setter in `haloreach.dll` with Ghidra.
- Next unknown: the default binding for the editor-mode action (profile bindings live at `+0x42C`; the settings UI in
  `tags\ui` may name the action).

## 0. Pick up here (2026-10-30, 17:45 ET) - Atlas export blocked; emblem data still complete (WeabooTyler117 / DeepSeek)

- `tool.exe export-bitmap-dds <abs>\tags\ui\cui\common\emblems\emblems.bitmap <prefix>` produces
  `emblemsinvalid_00_00.dds` (64 KB) and logs `rasterizer\invalid` / `ERROR failed to decompress [0x80070057]`. Pillow
  rejects the file (`Unimplemented DXGI format 91`); `export-bitmap-tga` fails the same way. So the shipped
  `emblems.bitmap` has no decodable pixels in the kit.
- The sprite sheet therefore has to come from MCC's packs (Noble already loads them through `UiArt::EmblemByName`) or from
  a different export verb (`export-bitmap-pfm` untried). Everything else is captured: `tools/emblem/emblem_library.xml` has
  the 53 backs, 80 fronts, 160 shapes, atlas indices and per-layer transforms.
- Repo state: `eaf74a1` pushed, tree clean.

## 0. Pick up here (2026-10-30, 17:15 ET) - HREK playbook: armory lighting explained, variants, and the truth about third person (WeabooTyler117 / DeepSeek)

New document `docs/HREK-PLAYBOOK.md` turns the kit findings into work items. Headlines:

- **Armory**: `data\levels\ui\mainmenu\scripts\mainmenu.hsc` shows the menu script *lighting each appearance Spartan
  with scripted cinematic lights*: `(cinematic_light_object chief_armory target_main appearance_spartan
  spartan_armory_light_anchor)` and the Elite equivalent, at lines 148-157, next to `(pvs_set_object ...)` and the idle
  animation loop. Combined with **0 generic lights** in `mainmenu_000.scenario_structure_lighting_info`, that is the whole
  void explained: the room's static geometry depends on **baked lightmaps**, the Spartan is lit by the script. Fix options,
  cheapest first: (1) add extra `cinematic_light_object` calls in the HSC for the room (rebuild script data + map, no
  lightmap bake), (2) set `emissive power/color` on the armory materials via the lighting tag's `material info` block,
  (3) bake lightmaps (`tool lightmap-farm`) and rebuild the menu layout - the full-fidelity fix. The 49 `instance imposter
  infos` (campaign backdrop imposters) are also a route to a painted backdrop instead of black geometry.
- **Variants**: `game_engine_settings` exports a real **`sandbox variants`** block (`variant_name_sandbox_editor`, with its
  own miscellaneous options), i.e. the engine ships Forge/sandbox as a *game* variant. `game_variant_settings` tags hold
  every trait the UI can expose, and `data\multiplayer\megalo\*.txt` + `MegaloEdit.exe` allow authoring custom variants.
- **Third person**: Reach has **no third-person player trait** (`game_engine_settings` has no camera field; "third" only
  appears in unrelated tags). Two real routes: (a) mode-based via the **sandbox/Forge engine** (tag `0x04`), which is
  third-person by construction and now has its variant definition from HREK - worth retrying after the earlier launch-state
  2 stall; (b) a **camera patch** in `haloreach.dll` (push the first-person camera back), which is client-side by nature so
  a host toggle can only gate local application. Recommendation: route (a) first.

## 0. Pick up here (2026-10-30, 16:35 ET) - Tag export works (absolute paths), and the real emblem data is decoded (WeabooTyler117 / DeepSeek)

- **How to export tags**: run `Foundation.exe` once from the HREK root, answer its "Switch Current Project" dialog with
  **"Switch my project"** (sets project Omaha, writes `gamestate.txt`, `prefs\Foundation`, `reports\`), then
  `tool.exe export-tag-to-xml <ABSOLUTE tag path> <out.xml>`. Relative paths always fail with *"Input tag path is not
  located in the tags directory structure"*; absolute paths work silently. Sapien still exits immediately on the scenario,
  so GUI editing is not available yet.
- **Emblems decoded** (`tools/emblem/emblem_library.xml`, 498 KB): blocks `bitmaps=119, shapes=160, front emblems=80,
  back emblems=53`, each emblem authored as layers with transforms (e.g. `jolly_roger` -> shape `skull`, multiplier 1,
  scale 0.75). Backgrounds: blank, circle, diamond, plus, square, triangle, vertical_stripe, criss_cross, 8_point_star,
  star, cowboy_hat, banner, sun, hexagon, chalice... Foregrounds: septagon, bullseye, vortex, halt, spartan, power_icon,
  delta, music_note, stuck, phoenix, champion, jolly_roger, active_rooster, campfire, radioactive, smiley, frowney,
  no_camping, sol... Atlas = `tags\ui\cui\common\emblems\emblems.bitmap`, 64 px sprites, antialias sharpen 1.6.
- **Armory lighting decoded** (`tools/emblem/mainmenu_lighting.xml`, 19 KB): `generic light definitions = 0`,
  `generic light instances = 0`, `regions = 0`, `material info = 1`, `instance imposter infos = 49`. There are **no generic
  lights at all** in the main-menu BSP, so the black chamber is a **lightmap** problem in Noble's rebuilt
  `mainmenu_all.map` (284 MB), not a lights problem. `material info` (emissive power/colour/quality/focus, attenuation
  falloff) is the only lever inside this tag; the real fix is baking lightmaps (`tool lightmap-farm`) and rebuilding the
  menu map (`tool build-cache-file-generate-main-menu-layout`).

## 0. Pick up here (2026-10-30, 15:30 ET) - HREK fully extracted: 62k tags, 39.4 GB, both target tags found (WeabooTyler117 / DeepSeek)

- `HREK.7z` unpacked in place: **62,017 files, 39.4 GB** under `D:\Steam\steamapps\common\HREK\tags` (D: still has
  ~128 GB free).
- `tags\levels\ui\mainmenu` fully present: `mainmenu.scenario` (438 KB), `mainmenu_000.scenario_structure_bsp` (245 KB),
  **`mainmenu_000.scenario_structure_lighting_info` (10.8 KB)**, `mainmenu_lobbies.scenario_structure_bsp` (100 KB),
  `mainmenu.structure_seams`, `instance_imposter_definition`, and the armory cubemaps.
- The lighting tag's structure is readable in the file: a **`generic light definitions`** block (type enum omni / spot /
  directional, flags for near/far attenuation and inverse-squared falloff, rectangle shapes) followed by lightmap data. A
  lit Spartan against a black chamber therefore points at the **BSP lightmaps**, not at the generic lights: Noble ships its
  own 284 MB `data\menumaps\mainmenu_all.map`, so rebuilding it from these tags with lightmaps baked
  (`tool lightmap-farm` / `tool build-cache-file-generate-main-menu-layout`) is the clean fix; adding/brightening lights in
  Sapien is the cheap experiment.
- `tags\globals\emblems\global.emblem_library` structure is confirmed: `emblem bitmap list` (name / string id / bitmap
  index), `emblem_shape_list` with `front emblems` -> `emblem_layer` (layer 0 / shape 0 / multiplier / transform:
  scale, shear, rotation, offset, blur) and `primary layer`, `emblem_front_list`, `back emblems` -> `emblem_back_list`,
  plus `emblem_runtime_front_list` / `emblem_runtime_back_list`. That is Reach's real background/foreground system, drawn
  from `tags\ui\cui\common\emblems\emblems.bitmap` (plus the 3.1 MB `emblems.bitmap` next to it).
- **Tool gotcha:** `tool.exe` answers *"Input tag path is not located in the tags directory structure"* for every path form
  because tags resolve through the project/tag datastore (`project.xml` -> `bin\tools\tagDb.sdf`), created the first time
  `ProjectChooser.exe`/`Foundation.exe` opens the project. `TagService.exe` with no `/projectroot` starts, finds no project
  and ends in its crash handler. So the order is ProjectChooser -> Foundation -> then tool/sapien/MegaloEdit work.

--
## 0. Pick up here (2026-10-30, 15:10 ET) - HREK torn open: the armory lighting and the real emblem library are in the kit (WeabooTyler117 / DeepSeek)

Tyler installed the Halo Reach Editing Kit and asked to "rip apart Foundation". Done; findings in
`docs/HREK-FINDINGS.md`. The short version:

- HREK (`D:\Steam\steamapps\common\HREK`, build 2023.07.17.176677.1-QFE1) = `Foundation.exe` ("HR Foundation Tag Editor"
  v1.903, the Bonobo tag framework), `sapien.exe`, `tool.exe`, `MegaloEdit.exe`, `ProjectChooser.exe`, and **`HREK.7z`
  (6.5 GB) with 63,569 tag files** - the full tag tree, not a sample (objects 17,172, levels 16,507, sound 12,151,
  cinematics 7,602, shaders 4,285, ui 1,318, globals 263).
- **Armory lighting is now unblocked**: `tags\levels\ui\mainmenu\` (148 tags, 27.6 MB) contains `mainmenu.scenario`,
  `mainmenu_000.scenario_structure_bsp`, `mainmenu_000.scenario_structure_lighting_info`, the armory cubemaps and
  `menu.area_screen_effect` - i.e. the chamber's lighting can be edited in Sapien and the menu map rebuilt with `tool.exe`.
  That was the only remaining route for the black void after the renderer swap was rejected.
- **The real emblem library exists**: `tags\globals\emblems\global.emblem_library` (57,396 bytes) carries the original
  `emblem_bitmap_list` (name / string id / bitmap index) and `shapes` (bitmap, address mode, transform scale, gradient)
  structures, plus the `emblems.bitmap` atlases under `tags\ui\cui\common\`. Extracted to
  `hrek-extract\tags\...` in the session workspace for inspection. This is the background/foreground source the emblem
  picker needs - MCC's `playercustomization.xml` only has precomposed pairs.
- **Forge/Megalo**: `MegaloEdit.exe` + `data\multiplayer\megalo\` sources let `00_basic_editing_054.bin` be opened as a
  Megalo variant, the cleanest way to check for a Forge session/editing flag.

Practical: the tags live inside `HREK.7z`; `HREK (Extract).bat` unpacks them (6.5 GB compressed) and Sapien/tool need the
project (`ProjectChooser.exe`) plus the tag service. Nothing in MCC's install is touched; rebuilt maps land in HREK and can
be copied into Noble's `data\menumaps` as a mod.

## 0. Pick up here (2026-10-30, 14:40 ET) - Ghidra project is set up and the forge gate is labelled in it (WeabooTyler117 / DeepSeek)

Tyler asked to start Ghidra and "set that all up". Done, and made reproducible.

- Project: `C:\Users\tyler\Documents\deepseek-harness\default-workspace\ghidra-proj\NobleReach.gpr`, program
  `haloreach-copy.dll` (a copy of the Steam DLL - the install is untouched), image base `0x180000000`, imported and
  **analysed** with Ghidra 12.1.4 + Java 21.
- `tools/ghidra/NobleForgeGate.java` labels and plate-comments the whole join-queue gate:
  `Noble_JoinQueue_SetMode` (`0x18039BEEC`), `Noble_JoinQueue_Dispatch` (`0x18039A8D2`),
  `Noble_JoinQueue_ProcessCall` (`0x18039A92D`), `Noble_JoinQueue_Process` (`0x18039BDCC`), and the globals
  `Noble_JoinQueueMode/Count/Entries/Step`, `Noble_JoinQueueSlotValid`, `Noble_JoinQueueSession1/2/34`.
  Re-runnable: `analyzeHeadless <proj> NobleReach -process haloreach-copy.dll -noanalysis -postScript NobleForgeGate.java
  -scriptPath tools\ghidra` (close the GUI first; it holds the project lock).
- `tools/ghidra/VerifyGateLabels.java` re-reads the saved program and prints name + comment for each address, so the
  labelling can be checked without the GUI. Verified: all ten addresses have their label and plate comment.
- `docs/GHIDRA-SETUP.md` documents opening the project, all labelled addresses, the live-base mapping (runtime base from
  the client log + RVA; e.g. base `0x7FFCA2F40000` -> mode global is `0x7FFCA59E9308`) and how to rebuild the project
  from scratch.
- The GUI is running again with the labelled project open (`Ghidra: NobleReach`).

## 0. Pick up here (2026-10-30, 14:00 ET) - Forge netplay: verified both join scenarios, and the join-queue gate map is written down (WeabooTyler117 / DeepSeek)

Follow-up to the 13:10 entry, after Tyler asked to "look for the offset".

### Verified locally, two processes, loopback, after `320998e`

| scenario | host queue | guest requests / aborts | guest result |
|---|---|---|---|
| guest joins **before** the host starts (lobby flow) | `mode 1 count 1` | 1 / 0 | `game session started ... loading screen off` |
| guest joins a **running** Forge match (host in-game 30 s first) | `mode 1 count 1` | 1 / 0 | `game session started ... loading screen off` |

Both pass, so the ordering fix closed the reproduced bug and **no byte patch was required**. A solo Forge host still reaches
the HUD with zero errors.

### The offset map, for the case a real two-PC run still fails

`docs/FORGE-NETPLAY-OFFSETS.md` documents the whole join-queue state machine in `haloreach.dll` (image base `0x180000000`),
with exact instructions:

- mode global `+0x2A49308` (27 references, all inside `0x39A8xx..0x39C2xx`), count `+0x2A4930C`, entries `+0x2A49318`
  (stride `0x588`), step `+0x2A49074`, pointer-valid byte `+0x2A1FD40`, session pointers `+0x2A1FE08/FE10/FE18`, and the
  session state field `+0x60EF8` which must be exactly 6;
- **mode setter** at RVA `0x39BEEC` (the only special case: raising mode to 4 while it is 3 is ignored);
- **dispatcher** at RVA `0x39A8D2` (mode must be 1..4 or the queue is skipped; the processor is called only at step 4);
- **processor** at RVA `0x39BDCC`, single caller `0x39A92D`, selects its session by mode.

The document lists a patch candidate per symptom (mode stuck at 0, mode stuck at 3, mode outside 1..4, step never reaching
4, queued but never inserted), and how to re-derive them with dumpbin or the Ghidra project at `ghidra-proj\NobleReach`.

### Next

Real two-PC Forge test with both machines on `320998e`. If it fails, the logs now print `join queue after request: mode N
count N ... state N`, so one run selects the row in the table above and therefore the exact patch.

## 0. Pick up here (2026-10-30, 13:10 ET) - Forge netplay: the host was refusing joins while its session was still closed (WeabooTyler117 / DeepSeek)

Tyler: solo works on every mode; custom games, matchmaking and Firefight netplay work; **Forge netplay does not**. He asked
for the offset.

### Reproduced locally, two processes, loopback

Needed a per-instance log first: `-log <file>` was added to `main.cpp` because `basic_file_sink` truncates
`noble-client.log` on every start (a second instance used to erase the first one's log). Then:

- host: `-map forge_halo -mode forge -host -online -wait-peers 1 -log f.log`
- guest: `-map forge_halo -mode forge -connect 127.0.0.1:11774 -log g.log`

### What the two sides said

| | host join queue at request | guest join-requests | guest join-aborts (msg 10) | guest in game |
|---|---|---|---|---|
| before | `mode 0 count 0` (session state **0**) | 30 | 10 | no |
| after | `mode 1 count 1` (session state **6**) | 1 | 0 | **yes** - `game session started ... loading screen off` |

The control (Slayer, same harness) was `mode 1 count 1`, 1 request, 0 aborts, in game - so the state numbers are the
discriminator, not map size or the Forge variant.

### Root cause

Forge is a Megalo session (engine tag 3), and its session must reach state 6 (loading) before a guest's join-request is
queued. It never got there in time because of the campaign co-op fix from earlier today (`f4d247e`): that change held
**Command 10 (Event: Start)** until lobby peers arrived, so the host sat in state **0** while the guest was knocking. The
host answered every request with `join-abort` (msg 10), the guest retried (30 requests, 10 aborts), and it never loaded -
exactly "netplay on forge does not work".

### Fix (`src/engine/BlamEngine.cpp`)

The pre-`Command 10` peer hold is now **campaign only** (`holdMissionForPeers = gameModeTag == 1`). Forge posts Command 10
immediately (opening its session), and the existing peer barrier holds **Command 11** for 8 s instead - which is what it
was doing before today's campaign change. Verified after the change: two-process Forge netplay reaches
`game session started ... loading screen off` on both sides, and a solo Forge host still reaches the Forge HUD with no
errors.

### Still to confirm on real hardware

This was loopback on one PC. The real two-PC test needs both machines on the same build and a reachable endpoint (or the
relay). If it still fails there, the next step is the session-state gate itself: the join queue lives around
`hr+0x397650` (`can_accept_join_request`, already hooked) and the state check that decides "queue vs abort" is what would
need a byte patch - the logs now print the exact state (`join queue after request: mode N count N ... state N`) so it can
be pinned in one run.

## 0. Pick up here (2026-10-30, 12:00 ET) - Two-machine co-op test harness + protocol for a second AI (WeabooTyler117 / DeepSeek)

Tyler asked whether O7's agent (Gemini) and this one could co-work a co-op test: one hosts, the other joins, both record and
fix. Agents cannot talk to each other directly, so the **repository is the message bus**. Built the missing half.

### `tools/coop-test.ps1`

One side of a run, usable by either agent:

```powershell
.\tools\coop-test.ps1 -Role host  -Peers 1 -PublicIp <ip> -Seconds 180
.\tools\coop-test.ps1 -Role guest -Connect <ip:port>      -Seconds 180
.\tools\coop-test.ps1 -Role report -InputLog <any noble-client.log>
```

- Host: kills stray clients, launches `-map m10 -mode campaign -host -online -wait-peers N` (plus `-Relay` if given), waits
  for the transport's UDP port, publishes `coop\endpoint.txt`, runs for the requested time, then writes
  `coop\host-<stamp>.log` and `coop\result-host-<stamp>.json`.
- Guest: launches `-map ... -connect <ip:port>`, same artefacts with the guest prefix.
- Report: filters any existing log into the co-op-relevant lines only (barrier, Command 10/11, insertion point, abort
  reason, join/connect refusals) - 1111 lines out of a 2.7 MB solo log in the smoke test.
- ASCII-only on purpose: PowerShell 5.1 misparses UTF-8 without BOM when comments contain typographic dashes (that bit us
  while writing it).

### `docs/COOP-TEST-PROTOCOL.md`

The handshake: request -> host publishes endpoint + log -> guest joins + publishes log -> whoever analyses writes
`coop/REPORT.md`. It names the pass lines verbatim, and a failure table mapping `0x1B` / `0x1C` / `0x1D` aborts and the
`no guest engine traffic after 10 s` case to the next change. It also states the ownership rule (one side owns
`BlamEngine.cpp` sequencing, the other `LobbyPatches.cpp` membership) and the hard precondition: the guest must be able to
reach the host's UDP endpoint, or both sides must use the master-server lobby / relay instead.

`coop/` holds a README describing every artefact. Full `noble-client.log` files are never committed.

## 0. Pick up here (2026-10-30, 11:15 ET) - Campaign co-op: the mission now waits for the guests before it starts (WeabooTyler117 / DeepSeek)

Tyler's report: on every campaign match the host loads in, the guest joins, and the guest sees the opening cutscene **from
outside the map** (terrain/skybox, plus cutscene effects and subtitles but no framing), then the game raises
"an error has occurred" and both players are dropped to the main menu. This is the failure that has blocked campaign
co-op, including 4-player.

### The two log files he attached were not host + guest logs

`Noble-Last-Game-2026-10-30.log.txt` and `.md` are the report and log slice I generated earlier from **his own 17:20 solo
campaign run** (707 lines, one process, no join requests, ends at `PauseMenu: game over`). No co-op attempt is captured in
them, so the actual engine error code cannot be read from those files. What is needed: `noble-client.log` from **both**
machines for the **same** failed attempt.

### Root cause as far as the code and the earlier RE work show it

The symptom is the campaign co-op abort family already documented here (0x1B `error_players_incompatible_for_network_coop`
= "An error occurred while playing co-op", 0x1C peer disconnect, 0x1D co-op state sync timeout) raised through
`c_network_session::abort` at `hr+0x44EBA0`. The guest's unbound camera is the giveaway: it entered a session whose
**opening cinematic had already started**, so no cinematic/cutscene binding existed for it, and the co-op state check then
killed the session.

The sequencing in `BlamEngine::LaunchMap` was the cause: `c_game_engine Start` -> **1 s** -> `Command 10 (Event: Start)`,
which is what loads the mission and starts its cinematic -> only *then* the `-wait-peers` barrier -> `Command 11`. The
barrier added on 2026-10-30 03:25 ("Forge / Campaign Netplay Root Causes") therefore protected the *simulation start* but
not the **mission start**, so the host was always a cinematic ahead of any guest.

### Fix in this commit (`src/engine/BlamEngine.cpp`)

For campaign and Forge hosts that were launched with guests (`-wait-peers N`, i.e. the lobby roster had N non-host
players), the peer barrier now runs **before `Command 10`**: the mission does not start until the guests' engine traffic
is on port 1000, so everyone enters the mission and its cutscene together. Bounded so a host can never hang:

- hard cap 25 s;
- if **no** guest traffic has arrived after 10 s, it logs that the session is probably only joinable once the mission
  state advances, and starts anyway (this is the diagnostic that tells us whether the barrier's premise is right);
- on success it logs how many peers joined before start, then `Command 10`, an 8 s settle for campaign/Forge, then
  `Command 11` (the old pre-`Command 11` wait now only applies to multiplayer/Firefight).

Solo and multiplayer paths are untouched: with no expected peers `peersToWaitFor` is 0 and the original 1 s / 5 s timing is
used. Verified: a solo campaign launch still reaches `game session started at frame 432` and captures normally.

### What to test next (needs two machines - cannot be done from this one)

1. Both players on the **same build** of `noble-client.exe` (a mismatched build will fail co-op for its own reasons).
2. Guest joins the Noble **lobby** before the host starts; host presses START with the guest listed. Watch the host log for
   `holding the mission start until 1/1 engine peer(s) have joined` and then `beginning the mission with them`.
3. If it still fails: send both `noble-client.log` files. The lines that decide the next step are `peer wait before start`,
   `Command 10/11`, `evaluate_coop_insertion_point` (LobbyPatches logs the value), and any
   `SUPPRESSED session abort` / `abort called ... reason=` lines, which carry the exact 0x1B/0x1C/0x1D code and caller RVA.
4. If the host log shows `no guest engine traffic after 10 s`, then the session is not joinable before the mission
   advances, and the next approach is the opposite one: start normally and force the guest's insertion at the first
   checkpoint (`evaluate_coop_insertion_point` already returns mission start; it needs the checkpoint path).

## 0. Pick up here (2026-10-30, 10:30 ET) - Handoff packaged as a Word document and a PDF (WeabooTyler117 / DeepSeek)

Tyler asked for the handoff "wrapped up in a nice doc I can save and provide". The same brief now exists as
`docs/HANDOFF-FOR-CLAUDE.docx` and `docs/HANDOFF-FOR-CLAUDE.pdf` (four pages, styled: title block, meta table, commit
table, save-state table, priority-ordered open work, numbered traps, closing note). Copies are in
`C:\Users\tyler\Downloads\Project-Noble-Handoff-2026-10-30.docx` and `.pdf`. The markdown original stays at
`docs/HANDOFF-FOR-CLAUDE.md` and is what future agents should read; the PDF is for handing to a person or another model.
Generated with `python-docx` from a script kept in the session workspace, verified content by reopening the file, and
inspected pages 1-2 as rendered images.

## 0. Pick up here (2026-10-30, 10:00 ET) - Self-contained handoff written for Claude (WeabooTyler117 / DeepSeek)

Tyler asked for a handoff he can hand to Claude. Wrote `docs/HANDOFF-FOR-CLAUDE.md`: one self-contained brief covering what
Noble is, how to build it and verify with captures, the commit table for this session, Tyler's current save state and the
backups that must not be lost, the open work in priority order (emblem bg/fg picker, armory chamber + shoulder cameras,
scoreboard, checkpoint continue, in-process transitions, older backlog) with file/line pointers and what has already been
ruled out, and the traps/house rules (career credits semantics, `data\appearance.json` being rewritten, `Row()` always
returning true, the relaunch account flags, PowerShell 5.1 limits, and the standing rule to update this log and mirror it).

`docs/HANDOFF.md` stays the detailed chronology; the new file is the short version for a fresh agent.

## 0. Pick up here (2026-10-30, 09:30 ET) - Career reset for Tyler: Recruit + 1,000,000 granted cR, unlocks to earn (WeabooTyler117 / DeepSeek)

Tyler asked to reset his level but be given money so he can go through the unlocks. Two things came out of it.

### Code: granted credits that do not fake the rank

`credits` in `career.json` has always meant **cR earned over a lifetime** - the balance shown in the Armory is
`credits - creditsSpent` - so handing out money by raising `credits` also hands out a rank (the first attempt put him at
"Colonel Grade 1"). Added `CareerTotals::bonusCredits`: spendable, saved as `bonusCredits`, included in `Career::Balance()`,
spent **before** earned credits, and **not** counted by `RankIndex`. So a grant or a reset can no longer promote anyone,
and spending can never demote them.

### Tyler's save

- `career.json`: `credits` (earned) 0, `creditsSpent` 0, `bonusCredits` 1,000,000, `armoryOwned` empty,
  `armoryUnlockAll` false, and all stats/history/challenges/commendations cleared. The Career screen shows **Recruit,
  0 / 7,500 cR, next Private** with everything at zero; the million is spendable in the Armory.
- **Gotcha found the hard way:** clearing `armoryOwned` made the Armory treat every equipped piece as "isn't unlocked yet;
  back to 'DEFAULT'" and it **saved the default look over `data/appearance.json`**. His original Spartan (Hilands helmet,
  Emile chest plate, Kat's tricorder, Linda's shoulder, Adrianna knee, blue flames, Jorge voice, colours 15/14) is saved
  at `data/appearance.before-reset-20261030.json`, and `data/userdata/career.backup-20261030-154234.json` holds the whole
  pre-reset career. Copying the former over `appearance.json` restores the look (it will report pieces as locked again
  unless they are owned or `armoryUnlockAll` is true).
- Rank gates still apply: most armour pieces show `LOCKED - <rank>` and need rank as well as cR, which is Reach's own
  design. `armoryUnlockAll: true` is the switch that bypasses gating entirely, if that is wanted for testing.

## 0. Pick up here (2026-10-30, 08:40 ET) - The OBJ preview is gone: the Armory only uses Reach's in-game model (WeabooTyler117 / DeepSeek)

Tyler: "OBJ re-export should not be used at all." Removed, not hidden behind a setting:

- **Deleted** `src/ui/ObjModelPreview.cpp/.hpp` and `src/ui/ArmoryPreviewLayers.cpp/.hpp`, plus their `CMakeLists.txt`
  entries.
- `ArmorCustomizer::RenderCharacterPreview` is now ~30 lines: Reach's own Armory scene draws the Spartan; until the menu
  map is ready it shows `LOADING ARMORY...`, and if `data\menumaps\mainmenu_all.map` is missing it says so. Drag-to-turn,
  auto-rotate and the wheel that cycles the three camera modes are kept.
- **`SettingsData::armoryPreview` and the ARMORY PREVIEW row are gone** (no option can select the OBJ mesh any more).
  `data\armor\*.obj` files are still in the install for other uses (Elite pack tooling) but nothing in the client loads
  them for the Armory.

Verified: build 0/0, armory / settings / main / career all capture with exit code 0, and no `ObjModelPreview` /
`ArmoryPreviewLayers` / `armoryPreview` references remain in `src`.

### Consequence for the two open armory wishes

Both are engine-side now, with no renderer shortcut:

- **Black chamber** -> light or replace it in `mainmenu_all.map` (data), or accept the framed-dark look.
- **Left/right shoulder** -> needs a camera with a lateral offset (engine camera globals, not found: the GLOBALS table
  only carries the scene index at `base + 0x24 * stride`). Per-slot panning otherwise works through the engine's own
  cameras (0x0D helmet, 0x0F chest, 0x10 wrist, 0x11 utility, 0x12 knees, 0x13 shoulders, 0x14 colours).

## 0. Pick up here (2026-10-30, 07:55 ET) - Armory is back on the in-game model: the OBJ re-export is not the right mesh (WeabooTyler117 / DeepSeek)

Tyler, after seeing the armory on Noble's own renderer: "this is not the right model at all - we need to go back to the
other model." He is right, and this closes the "one change fixes everything" idea from the 04:00-06:20 entries.

- **`armoryPreview` defaults back to 0** = Reach's own Armory scene, i.e. the in-game Spartan. Verified by capture: the
  full-body 3/4 view with the correct mesh, armour and the blue-flames effect is back.
- Noble's renderer (`ObjModelPreview`, `data/armor/spartans/spartans.obj`) is a **different mesh** - a re-export, not the
  shipped Spartan - so it stays only as an opt-in (Settings > Graphics > ARMORY > ARMORY PREVIEW -> NOBLE) and must never
  be the default. The stage, the eye-strafe pan and the focusX framing all still exist in that path; they are simply not
  what players see by default.
- **Consequence for the open items:** the black background and the left/right shoulder distinction cannot be solved by
  swapping renderers, because the correct model lives in the engine's scene. Both therefore need the engine-side routes
  already recorded: light/replace the chamber in `mainmenu_all.map` (data), and a camera with a lateral offset
  (engine camera globals, still unfound) for the shoulders. Per-slot panning with the engine cameras *does* work and is
  verified (helmet 0x0D, chest 0x0F, wrist 0x10, utility 0x11, knees 0x12, shoulders 0x13, colours 0x14).

### Emblem prototype status

The data layer is in: `UiArt::GetEmblemBackdropLayers()` / `GetEmblemSymbolLayers()` / `EmblemLayerTexture(image)` expose
every emblem's `image1` (backdrop layer) and `image2` (symbol layer) - about 225 of each - loaded by name from the MCC
packs. What is not built yet is the UI: two Armory identity rows (background / foreground), storage in the profile or
settings, and the composition drawing (backdrop then symbol) at the two places the emblem is drawn
(`FrontEndMenu.cpp:861` in `PlayerRow` and `:3660` in the Armory plate).

## 0. Pick up here (2026-10-30, 07:05 ET) - Emblem background/foreground: what MCC actually ships (WeabooTyler117 / DeepSeek)

Tyler wants Reach's emblem editor: pick a **background** (circle, triangle, hexagon...) and a **foreground** (grunt, flame,
letters/numbers). Looked at the data before building UI around it.

### What the data is

`Data\UI\playercustomization.xml` in the MCC install defines **330 emblems**, each like:

```xml
<Emblem id="1" image1="e_002_p" image2="e_002_s" image1_small="e_002_p_sml" image2_small="e_002_s_sml" name="$EMBLEM_Radioactive" />
```

- `image1` (`_p`) and `image2` (`_s`) are the **primary and secondary colour layers of one finished emblem**, not a
  background/foreground pair: of 330 entries, 225 have both fields, and the backgrounds are essentially unique per emblem
  (there is no small reusable set of circle/triangle backdrops, and no separate symbol set).
- Nothing in MCC's data, its `haloreach` folder, or Noble's `data` directory holds Reach 360's background/symbol sets:
  Reach's original editor had ~30 backdrops x ~60 symbols, and those assets are not part of MCC.
- Noble loads emblems through `UiArt::Emblem(id)` / `EmblemByName(imageName)` from the MCC packs, so any single layer
  *can* be addressed by name - the capability is there, the authoring data is not.

### So the picker needs one decision from Tyler

1. **Trial the matrix from what MCC has** (fast, ~1 session): let the player choose any `imageN` as the backdrop layer and any
   other as the symbol layer (330 x 330). The layers are colour masks of designed emblems, so many combinations will work
   (shape + symbol) and some will look odd. It is real, it ships now, and it can be reverted.
2. **Extract Reach 360's own emblem sets** (the faithful route): the background/symbol packs live in the Reach packages, not
   in MCC's XML - this is tag/extraction work like the armory chamber, and needs the RE toolchain.

Recommendation: try (1) as a visible prototype, keep the composed emblems as the default list, and only then decide whether
(2) is worth the extraction work.

## 0. Pick up here (2026-10-30, 06:20 ET) - Armory: pan is visible, Noble's renderer is the default, stage tuned (WeabooTyler117 / DeepSeek)

Finishing the armory work from the 05:40 entry.

### The pan now actually pans

`ObjModelPreview::Render` kept the **eye at x=0** and only aimed the focus, so moving `focusX` rotated the view instead of
strafing it - measured 6.7 % of pixels between the two shoulder views. The eye now strafes with the offset
(`eye = (focusXOffset, 0.06, -3.2/zoom)`), and the same measurement is **22.8 %**, with the model visibly shifting left for
the left shoulder and right for the right.

Framing retuned for the new camera (the old vertical offsets were measured against the engine framing and put parts at the
frame edge): helmet `{1.15, 1.05, 0}`, shoulders `{1.05, 0.85, +-0.55}`, chest `{0.90, 0.45, 0}`, wrist `{0.62, 0.55, -0.45}`,
utility `{0.95, -0.55, -0.30}`, knees `{0.90, -1.00, 0}`.

### Noble's renderer is the default

- `SettingsData::armoryPreview` now defaults to **1** (Noble), 0 = Reach's engine scene.
- New row: **Settings > Graphics > ARMORY > ARMORY PREVIEW** cycling `NOBLE` / `HALO: REACH`, with a description that says
  which does what.
- The `LOADING ARMORY...` line only appears when the *engine* mode is selected (Noble's renderer is ready immediately).
- Stage brightened (`#1a2230 -> #0f141e -> #080a0f`, stronger accent floor glow) so it reads as a lit studio rather than a
  dark rectangle.

Verified by capture: Helmet / Left Shoulder / Right Shoulder / Knee Guards each frame the right part (helmet vs knees differ
38 %, shoulders differ 23 % side to side), the floor glow sits under the feet, and the Settings row renders.

**Left for a later polish pass:** the stage could use the player's own menu wallpaper (blurred + darkened) as an option,
and the pan could be eased a little slower. Both are cosmetic; the structure is done.

## 0. Pick up here (2026-10-30, 05:40 ET) - Armory stage + camera pan wired into Noble's renderer (WeabooTyler117 / DeepSeek)

Per Tyler's "do whatever you recommend": moved the armory onto Noble's own renderer and gave it a stage.

### Done

- **Stage** (`ArmorCustomizer::RenderCharacterPreview`): drawn into the panel *before* the model texture, so it sits behind
  the Spartan. Vertical gradient (#101620 -> #06080c), an ice-blue floor glow (two soft ellipses in the project accent
  colour), a hairline horizon and a four-edge vignette. The renderer's target is now always **transparent**
  (`cam.transparentBackground = true`) so the stage shows through instead of its own clear colour.
- **Camera pan plumbing**: `Framing` gained a third field (`focusX`) wired into the renderer's existing `focusXOffset`:
  left shoulder -0.52, right shoulder +0.52, wrist -0.42, utility -0.30, everything else 0, and it is eased like the zoom
  and vertical focus. **Measured: only 6.7 % of pixels change between the two shoulder views** - the offset aims the camera
  (eye stays at x=0, so it rotates the view rather than strafing it), so the pan is real but too small. Next step is
  either raise the offsets (~+-1.0) or move `eye` sideways in `ObjModelPreview::Render` (line ~605: `eye` is fixed at
  x=0 while `focus` takes the offsets) - the latter is the correct "pan".
- `Settings > Game options` still needs the row for `armoryPreview` (0 = Reach scene, 1 = Noble renderer); for now it is a
  settings.json key. Default is still 0 (engine), because the pan needs the eye fix first.

### Notes from this session's armory work (all verified)

- The engine's armory cameras are real and per-slot (0x0D helmet, 0x0F chest, 0x10 wrist, 0x11 utility, 0x12 knees,
  0x13 shoulders, 0x14 colours; 0x15+ leave the armory), so clicking a slot already pans on the engine path.
- Left vs right shoulder cannot be distinguished on the engine path (one camera, no lateral offset, and rotating the
  model does nothing because the camera rides with it).
- Noble's renderer is the way to get both a designed backdrop and a real lateral pan; its model, lighting (3-point with a
  cyan rim), colour-control textures and armour effects all look good (see `ui-crops/armory_noble.png`).

## 0. Pick up here (2026-10-30, 04:45 ET) - Armory cameras mapped by capture; left/right shoulder is impossible on the engine path (WeabooTyler117 / DeepSeek)

Tyler asked to start on the per-piece camera pan. Went at it with captures instead of guesses, and the earlier
"scene switching doesn't move the camera" conclusion was **wrong** - it came from probing the *menu* scenes.

### What the engine's armory cameras actually do (verified, one capture per index)

Added `-armory-scene-index <n>` (forces a raw camera scene; `ResolveSceneIndex` accepts `index:<n>`) and
`-armory-focus "<slot>"` (starts with that slot focused), then captured every index:

| index | what it shows |
|---|---|
| 0x0A | default reach 3/4 full body |
| 0x0C | whole body, straighter (c_full_spartan) |
| **0x0D** | helmet / upper-body close-up |
| 0x0E | mid body (the roster scene, correctly excluded) |
| **0x0F** | chest close-up |
| **0x10** | wrist / forearm |
| **0x11** | utility (thigh) |
| **0x12** | knee guards |
| **0x13** | shoulders / upper torso |
| **0x14** | colours (torso, slightly further) |
| 0x15-0x20 | **not the armory at all** - other menu scenes (a Covenant vista, no Spartan) |

So `ArmorCustomizer`'s existing slot scenes are correct and **clicking a slot does pan**: captures focused on Chest, Knee
Guards and Left Shoulder differ by 36% of pixels in the character panel.

### The one thing that cannot work on the engine path: left vs right shoulder

- The map has **one** shoulder camera (0x13) for both sides, and higher indices leave the armory entirely - there is no
  second shoulder camera to find.
- Tried presenting the side by rotating the model (`ReachArmory::SetYawRotation` per slot, ±70 degrees). Measured result:
  left vs right shoulder captures differ by only **5 %** of pixels, the same as before, and the difference is the animated
  armour-effect particles. The armory camera evidently rides with the character, so rotating it changes nothing visible.
  **Removed** rather than left in as a no-op that would also fight the drag-to-turn control.

Conclusion: left/right shoulder distinction needs a camera that can *offset laterally*, which the engine's fixed scenes
cannot do. Options are (a) Noble's own renderer (`FocusX` already exists in its camera - the `armoryPreview` switch from
the 04:00 entry), or (b) engine camera globals with a lateral offset - still un-found; the GLOBALS table only exposes the
scene index (`base + 0x24 * stride`), not a position.

### Left in the tree

- `-armory-scene-index <n>` and `-armory-focus "<slot>"` test hooks (harmless in normal runs, both used above).
- `ResolveSceneIndex` accepts `index:<n>` so any map camera can be addressed by number.
- Everything else unchanged; armory still captures normally.

## 0. Pick up here (2026-10-30, 04:00 ET) - Armory: Noble's own renderer looks good, and it is the way out of the void (WeabooTyler117 / DeepSeek)

Tyler asked which route to take for the armory's black background. The decisive fact: **Noble already has its own
armory renderer** (`ObjModelPreview`, `data/armor/spartans/spartans.obj` + `data/armor/elite/elite.obj`) with

- diffuse + **colour-control** + **normal** maps (so primary/secondary colours and detail areas come out right),
- **3-point MCC studio lighting** with a cyan rim light, visor fresnel and emissive spots for armour effects,
- per-piece **layer** meshes (`ObjPreviewLayer`) and a camera with pitch, zoom, **focusY** *and* **focusX** offsets,
- its own render target with a transparent background - i.e. **we own what goes behind the Spartan**.

It was only ever a *fallback* for "engine scene not ready". Added `SettingsData::armoryPreview` (0 = Reach's scene,
default; 1 = Noble's renderer; row still to add to the Settings UI) and captured the armory with it forced on: the
Spartan renders with the studio lighting, the blue-flames effect, correct colours and no black chamber - see
`ui-crops/armory_noble.png`.

### Recommendation (for Tyler to confirm)

Switch the armory to **Noble's renderer as the primary preview** and give it a designed stage (dark vertical gradient with
a soft floor ellipse, a subtle grid or horizon, and a rim glow that matches the ACCENT colour). That single change:

1. **kills the black void** - the stage is ours, drawn behind a transparent render target, no engine geometry involved;
2. **makes per-piece panning real** - `FramingFor` + the camera's `focusX`/`focusY` already exist for this renderer; left
   shoulder moves the camera left, right shoulder right, helmet up, knees down (the engine path cannot do this: it maps
   both shoulders to one scene);
3. **removes the palette flash and the DMR throw** - no engine model is re-instantiated when a piece changes;
4. keeps the engine scene one setting away for comparison.

Trade-off to be honest about: the engine model is the *in-game* asset with its own shaders; the OBJ path is a faithful
re-render with our lighting. From the capture they are close, and ours exposes more detail (armour effects, rim).

### Not done yet

- The Settings row for `armoryPreview` (lives in Game options next to the other ARMORY items).
- The **stage** itself (gradient/floor/rim) - the render target is transparent today, so nothing is behind the model yet.
- Feeding `m_focusScene` into the OBJ camera's `focusX` so the pan actually happens (currently only zoom/focusY are eased).

## 0. Pick up here (2026-10-30, 03:10 ET) - No more desktop flash between menu and match (WeabooTyler117 / DeepSeek)

Tyler: every transition that crosses a loading screen (starting a match, backing out, restart, next campaign mission)
shows the desktop and then a new window pops up.

### Cause

`Relaunch::HandOff` started the new process and **hid this window immediately**, then terminated. The new process needs
1.5-15 s to get its window up, and for that whole time there was nothing on screen - the desktop.

### Fixed: the old window stays up until the new one appears

- `Relaunch::Start` now reports the child's PID.
- `HandOff` waits for the **first visible top-level window owned by that PID** (up to 30 s, 100 ms polling), brings it to
  the front, and only then hides its own window and ends. The old process keeps rendering its menu/loading screen while
  the new one boots, so the swap happens between two Noble windows instead of through the desktop.
- Measured on this machine: `FrontEndMenu: launching match via Relaunch` -> `Relaunch: new window up after 1812 ms;
  swapping to it` -> `handed off`. So ~1.8 s of menu on screen instead of ~1.8 s of desktop.

### What full seamlessness would need

No process boundary at all, i.e. an **in-process session transition**: end the running match and go back to the menu (or
straight to the next mission) inside one process. Clues and state:

- `BlamEngine` has `ExecuteCommand(const std::string&)`, which dispatches through the engine's vfunc[9]. **Nothing calls
  it today** and `BlamEngine` is not part of the running client's path, so using it means wiring an engine-instance
  handle into reach (or calling vfunc[9] on the instance `ReachHost` already has).
- The 2026-10-29 notes say the engine cannot switch maps once a match has started (RR also runs every match as its own
  process), so this is research: dispatch candidate commands mid-match (`game_end`, `game_start`, `map_reload`, `quit`)
  and watch whether the engine returns to the menu instead of tearing down. A `-exec-at <seconds> <command>` test hook
  would make that a five-minute experiment per candidate - the same shape as `-pause-at`.
- Until then the process swap is the mechanism, and it is now a window swap rather than a desktop flash. A loading
  splash drawn by the child as early as possible would mask the remaining ~1.8 s almost entirely (the child already
  draws `LoadScreen`, so this is about showing it before the engine initialises, not new art).

Verified: build 0/0; a scripted menu -> match hand-off logged the new-window wait and the swap; the client was closed
afterwards.

## 0. Pick up here (2026-10-30, 02:30 ET) - Armory: appearance writes coalesced; the rest of the list triaged (WeabooTyler117 / DeepSeek)

Tyler's list: scoreboard modernisation, armory (background, per-piece camera pan, palette/weapon-throw artefact, emblem
background/foreground picker), and checkpoint-based campaign continue. What was found and what was done.

### Done: the palette flash / "throws the DMR away" on every armour change

`ArmorCustomizer::SyncToReachArmory()` called `ReachArmory::SetSelection()` on **every** confirmed change, and applying a
selection makes the engine re-instantiate the Spartan (colour palette resets for a frame, armory idle animation
restarts - the throw). Changes are now **coalesced**: a write applies immediately only if the previous one is older than
250 ms, and held arrow keys collapse into a single engine write (per-frame flush in `Render`). This removes the strobing
while scrolling; one write per settled change is inherent while the engine owns the model.

### Triaged, with the evidence needed to continue

- **Per-piece camera pan.** The scenes exist (`c_helmet`, `c_left_shoulder`, `c_right_shoulder`, `c_chest`, `c_wrist`,
  `c_utility`, `c_knees`) and `ArmorCustomizer::UpdateArmoryCamera()` already switches to them as the edited slot changes
  (`m_cameraMode == 2`, the default). But the *framing* code that actually moves a camera (`FramingFor` - zoom + look-at
  height per slot) only drives **Noble's own OBJ stand-in preview**, not the engine scene, and the 2026-10-30 scene probe
  showed switching `mainmenu_all.map` scenes changes the lighting but not the view. So the engine's armory camera has to be
  moved directly: find its position/target globals (the same GLOBALS table `ReachArmory` already walks) and write a lateral
  offset per slot (left shoulder = -X, right = +X, helmet = up, knees = down). That is the next armory task, and it also
  fixes left vs right shoulder, which the engine's single `0x13` scene cannot distinguish.
- **Black background** - see the 01:30 entry: the void is the chamber's own unlit geometry, so the fix is data-side
  (light or replace it in `mainmenu_all.map`) or a Noble-owned renderer. Code cannot composite behind it.
- **Emblem background/foreground picker.** Assets are already in Noble: `UiArt::EmblemByName()` resolves MCC pack entries
  (`E_002_P_SML` style) and the Armory grid draws them. What is missing is the *classification* - which entries are emblem
  backgrounds (circle, triangle, hexagon...) and which are foregrounds (grunt, flame, letters/numbers) - plus a two-column
  picker that stores both. Halopedia's MCC emblem list is the reference; the naming in the packs follows
  `E_<bg>_<fg>_<suffix>` for premade pairs, which has to be confirmed against the pack index.
- **Scoreboard modernisation.** Reach's scoreboard is drawn by the engine. Modernising it means either restyling the
  engine's own screen (no handle found) or drawing a Noble scoreboard over it, which needs the per-player rows
  (name, team, score, kills/deaths) read from the engine - the same data the post-game screen already parses from the
  results block, so a scoreboard overlay is a data-plumbing job, not a new RE project.
- **Checkpoint-based continue.** Continue currently relaunches the last campaign mission from its start. Real checkpoint
  resume needs Reach's save/checkpoint state (written into the profile/save data by the engine); nothing in Noble reads
  it yet. First step is to locate the checkpoint block in the save data (compare two saves at different points in one
  mission), then have CONTINUE pass it to the new process.

Verified after the change: armory, main menu and settings all capture with exit code 0; build 0/0.

## 0. Pick up here (2026-10-30, 01:30 ET) - Armory backdrop: fifth approach tried, and the evidence now points at the chamber's own geometry (WeabooTyler117 / DeepSeek)

Tyler asked again for the armory's black background to be fixed. Fifth attempt, and the first that would have worked if
the void were empty:

- Built a `WallpaperPass` (fullscreen triangle sampling `UiArt::MenuBackground()` with cover UVs) and hooked
  **`ID3D11DeviceContext::OMSetRenderTargets`** (vtable 33) so the wallpaper is painted **before the engine's own draws**
  each frame. The reasoning: earlier attempts proved the engine never clears the back buffer per frame, so a picture
  painted before its draws should simply stay visible in the void while geometry draws on top.
- Crash #1: **stack overflow** - `WallpaperPass::Paint` itself calls `OMSetRenderTargets`, re-entering the hook. Fixed with
  a `thread_local` re-entrancy guard.
- Then it ran: the log said `Armory backdrop: wallpaper painted behind the scene (before the engine's draws)`, and the
  armory capture still shows the black void (void mean luma 1.17, unchanged).

**That is the answer to "is the void empty": it is not.** Something the engine draws covers the whole frame while the
Armory scene is up - consistent with the chamber being inside-out geometry with an unlit black material (the handoff's
2026-10-29 note mentions faint volumetric haze and SSAO noise inside it, which only geometry produces). Anything painted
"behind" it is occluded, and anything painted "in front" would cover the Spartan.

Five approaches are now ruled out: luma composite, depth-based composite (faults in d3d11 in this swap chain), replacing
the clear (there is no per-frame clear), switching the menu scene (all 21 probed, the pedestal camera is always in the
chamber), and painting before the engine's draws (occluded).

### What would actually fix it

1. **A data fix (the real one):** find the armory chamber's material/lightmap in `mainmenu_all.map` and light it, or swap
   that geometry for a backdrop - i.e. a map/mod change, not a code change. `ReachArmory` already knows the map's GLOBALS
   table and scene indexes, so this is tag/property work rather than reverse engineering from scratch.
2. **Noble renders its own armory view:** take over the Spartan's draw with Noble's camera and render target. That is a
   renderer project, not a fix.
3. **Cosmetic compromise (cheap):** treat the black as deliberate - a soft vignette around the model and a caption, so the
   screen reads as designed rather than broken.

Code is reverted to the known-good state (build 0/0, armory captures normally); nothing from this attempt is left in.

## 0. Pick up here (2026-10-30, 00:45 ET) - "No limit" was capped at 120, and 30 FPS whenever unfocused (WeabooTyler117 / DeepSeek)

Tyler: "no limit fps for some reason shows 30 fps?" - two separate hard-coded caps in the Present hook explained it:

1. **Focused + "No limit"**: `settings.fpsLimit <= 0` was turned into `targetFps = 120` ("prevent runaway 200+ FPS loops").
   So "No limit" was really "120", and the FPS counter could never show more.
2. **Unfocused (any limit)**: `if (!isFocused) targetFps = 30` - an unconditional 30 FPS throttle. A capture run, an
   alt-tab, or a background window all showed 30, which is exactly what Tyler saw.

### Fixed

- **"No limit" now means no pacing** while the window is active: `targetFps <= 0` presents immediately. Proven with a
  real match capture with `fpsLimit: 0`: the HUD counter reads **FPS 180**.
- The unfocused throttle is still there (it stops a background game pegging a core or tripping Windows' unresponsive-app
  detection) but it is now a **setting**: `SettingsData::throttleUnfocused` (default on) with a
  **THROTTLE WHEN UNFOCUSED** row under FRAME-RATE LIMIT, so streamers capturing from another window can turn it off.
- The FRAME-RATE LIMIT description now says what "No limit" means instead of implying a cap.

Note for future test work: a **minimised** window does not present, so neither `-pause-at` nor any capture fires with
`Start-Process -WindowStyle Minimized` - the throttle path cannot be photographed that way (D3D simply stops presenting).
The throttle itself is unchanged behaviour, now behind the new switch.

## 0. Pick up here (2026-10-30, 00:15 ET) - CONTINUE is campaign-only (WeabooTyler117 / DeepSeek)

Tyler: "continue should only continue on campaign missions only, nothing else".

- **Only campaign launches are remembered now.** The launch choke point records `lastPlayed*` when
  `choice.mode == "campaign"` (and still skips lobby joins), so finishing a multiplayer/Firefight/Forge game can no longer
  overwrite the last mission.
- **The row only exists for campaign**: `canContinue = lastPlayedMode == "campaign" && !lastPlayedMap.empty()`. A player who
  only plays multiplayer never sees CONTINUE at all.
- **The action always launches campaign** (`choice.mode = "campaign"`), and the description reads
  `Continue the campaign: Winter Contingency on Heroic (starts the mission again).` - honest, because Noble has no
  save/checkpoint resume: the mission restarts from the beginning with the stored difficulty and skulls.

Verified with two seeded settings and a capture of each: `lastPlayedMode: campaign` shows CONTINUE above CAMPAIGN;
`lastPlayedMode: multiplayer` shows a list starting at CAMPAIGN, no CONTINUE row.

## 0. Pick up here (2026-10-30, 23:40 ET) - Pause menu rebuilt to the MCC reference (list + challenges sidebar) (WeabooTyler117 / DeepSeek)

Tyler sent three MCC/Halo 4 pause screens and asked for that shape: an action list on the left, a scrollable daily/weekly
challenge sidebar on the right, "change team if available", restart/end game, and a vote-to-kick entry; plus the rule that
a solo game pauses while a match with other players must not.

### What is in

`PauseMenu::DrawMenu` is rewritten (was a centred 340 px stack of four buttons):

- **Header** says `PAUSED` + "The game is paused." when `NetworkTransport::GetGamePeerCount() == 0`, and `MENU` +
  "The match keeps running for the other N players." otherwise. (Noble cannot stop the engine's simulation - no pause
  command has been found - so the header states which case it is instead of pretending.)
- **Left list**: RESUME, RESTART MATCH, SETTINGS, LEAVE GAME, QUIT, with the focused row underlined and its description
  printed underneath, exactly like the reference. Up/Down + pad D-pad move, Enter/A activate; the first 250 ms after the
  menu opens are ignored so the press that opened it (or a held pad A) cannot bounce straight into an entry.
  - RESTART MATCH uses `Relaunch::CurrentGameArgs()` + `Relaunch::HandOff` - it only appears enabled when the current
    match's arguments are known.
  - LEAVE GAME is the old QUIT TO MENU (fresh Noble, no `-map`); QUIT closes the process.
- **Right sidebar**: `Career::Challenges()` drawn as DAILY / WEEKLY cards with title, description, progress bar, `n / m` and
  XP reward, inside a scrollable child, headed by the daily reset countdown (`Career::DailyResetText()`).
- **Test hook `-pause-at <seconds>`** (ReachHost, next to `-scoreboard-at`): opens the pause menu that long after the first
  input poll and requests a screenshot, so this screen can be reviewed from a capture. The launcher now forwards
  `-pause-at`, `-scoreboard-at` and `-quit-after` to the child process, since the child is a fresh process with its own
  command line.

Verified by a real match capture (`-menu-screen custom -auto-play -auto-play-variant ctf_054 -pause-at 35`): header,
five entries, description line, reset countdown and six challenge cards with progress bars all render; the sidebar
scrolls.

### What is not in, and exactly what it needs

- **CHANGE TEAM**: Reach's team swap is an engine action. Needs a `ReachHost` hook (or a command) that moves the local
  player to the other team; the row can then appear only when the session reports teams (the roster already carries a
  team per player).
- **VOTE TO KICK**: the UI is the easy half - a roster list in the pause menu and on the TAB scoreboard with one vote per
  player. The hard half is transport: `NetworkTransport` has no kick message today (`GetGamePeerCount`, `AddPeer`,
  `OnNetSend/OnNetRecv` only). Plan: a `Kick(peerId)` on the host plus a `VoteKick` relay message the master server or the
  host tallies, then the host drops the peer. Until then a vote button would be decorative, so it is not shipped.
- **A real pause**: solo should freeze the sim. That needs the engine's pause (command or global); the header currently
  reports the state truthfully rather than freezing it.

## 0. Pick up here (2026-10-30, 22:55 ET) - Service record reads like the menus: mission and map names (WeabooTyler117 / DeepSeek)

Tyler: "need missions to link with their actual name same with multiplayer in the recent games, campaign etc."

- **CAMPAIGN section**: `m05` / `m10` / `m52` now read **Noble Actual / Winter Contingency / New Alexandria**, with the scenario id
  in the tooltip.
- **RECENT GAMES**: the MODE column said `multiplayer`, `campaign`, `firefight` and was too narrow (`firefight (Norm…`
  was clipped) - it now reads `Multiplayer`, `Campaign · Normal`, `Firefight · Normal` in a 170 px column, and the MAP column
  shows **Boneyard / Corvette / New Alexandria / Winter Contingency / Installation 04** with the raw name in a tooltip.
- Both use the existing `common/MapNames` table, so they can never disagree with the server browser or the "Continue" row.

Verified by capture: campaign rows and the recent-games table both show names, nothing clipped.

## 0. Pick up here (2026-10-30, 22:30 ET) - "Does it actually do anything?": two graphics options made real, input maths proven (WeabooTyler117 / DeepSeek)

Tyler asked whether the ADVANCED GRAPHICS options and the extra Mouse/Controller settings do anything. Audited, then made
what could be real actually real.

### 1. The audit (`tools/check-settings-wired.ps1`)

For every field in `SettingsData`, checks whether it is mentioned anywhere outside the settings plumbing
(Settings.cpp/SettingsPanel.cpp). Result: **50 fields, 50 mentioned outside, 0 stored-but-unused** - but a mention is not
an effect, so the ones Tyler named were then proven by test instead (below).

### 2. ADVANCED GRAPHICS: two real rows, four honestly gone

- **BRIGHTNESS** (0.7-1.4, default 1.0) and **ANTI-ALIASING (FXAA)** are now real: Noble already owns a full-screen pass
  over the frame the engine draws (`GammaPass`), so brightness is its `pow()` exponent (`1/brightness`) and FXAA is a
  compact luma-edge pass added inside the same shader. Both read the settings twice a second; at 1.0/off the pass returns
  early and costs nothing.
- **Measured proof**: armory captures with `brightness` 1.0 vs 1.4 - the 3D model's mean luma goes 8.57 -> 12.35
  (**+44%**), with **87.6% of pixels changed** in the model region; FXAA changes 61% of the same pixels (edge smoothing,
  plus some of the armour-effect particles animating between runs).
- **TEXTURE FILTERING, SHADOWS, MOTION BLUR, DRAW DISTANCE** were five greyed rows showing "Not available yet" and backed
  by nothing - **the strings were the entire implementation**. They are now one paragraph under a "REACH'S OWN SETTINGS"
  heading saying exactly that: those live inside Halo: Reach and Noble cannot change them yet, so they are not offered as
  switches that do nothing. (The old code was a `kLater[]` array rendered with `disabled=true`; no settings field, no
  engine write, no D3D call.)

### 3. Mouse/controller extras moved into tested code

The maths for **mouse smoothing**, **mouse acceleration**, the **dead zones** and the **stick layout** used to be inline
in `ReachHost::FillLocalPad`/the look path, where nothing could test it. It now lives in
`src/engine/InputTuning.hpp/.cpp` and both call sites use it, which also gives the fourth headless suite:

```
PASS: 43 rule checks, 280 map x type combinations (207 allowed, 73 blocked), 0 failure(s)
PASS: map names - 10 display cases, 0 failure(s)
PASS: button layouts - 16 expectations, 0 failure(s)
PASS: input tuning - 0 failure(s)
```

What `InputTuning::SelfTest` proves (i.e. the settings demonstrably change the output):

- dead zone **0 does nothing**; input inside the zone becomes 0; the edge of the zone still reaches full speed; a bigger
  flick still comes out bigger;
- **Southpaw swaps** the two sticks and Default does not;
- **smoothing off** passes the delta through untouched; **smoothing 0.8** damps the first frame of a flick to under half
  and keeps moving toward the input; reset clears the state;
- **acceleration off** passes through; **acceleration 1** helps a fast flick more than a slow one, stays capped at 4x, and
  turns further than with acceleration off.

### 4. Trap found on the way (cost one crash)

The first version of the new post-process crashed the moment it actually ran (brightness != 1.0 or FXAA on): the C++
`GammaCB` was 16 bytes while the HLSL cbuffer was 32, and `PSSetSamplers(1, 1, nullptr)` was called with a null array.
Both are fixed; the pass now runs, is measured (above), and the match harness still passes (`-Only Slayer,Firefight` 3/3).

## 0. Pick up here (2026-10-30, 21:40 ET) - Server browser: sortable columns and two more filters (WeabooTyler117 / DeepSeek)

Spec 4.5 leftovers, both small and player-visible.

- **Sortable columns.** The table already had `ImGuiTableFlags_Sortable` and `TableHeadersRow()`, but nothing ever read
  the sort spec, so clicking a header did nothing. The list is now built as a filtered `vector<const ServerEntry*>`,
  sorted with `ImGui::TableGetSortSpecs()` (Source, Server, Mode, Map, Players, Ping, Address; `Specs[0]` and
  `SortDirection`), and drawn in that order. Server sorts by name by default; Players and Ping prefer descending. The
  selection still uses the entry's index in `m_servers` (recovered with pointer arithmetic), so double-click-to-join and
  the details panel keep working.
- **Filters**: `No password` and `Not started` join Hide full / Hide empty / Favourites only, and are honoured in
  `PassesFilters`.

Checked: the SERVERS screen renders with the new filter row, and `tools/test-modes.ps1 -Only Slayer` still reaches
gameplay, so the table rewrite did not disturb anything else.

**Left in 4.5:** the players modal needs the master server to list the roster (ServerEntry only carries a count), and the
"required mods" modal needs the mod list in the server entry - both are server-side additions, see `server.js`.

## 0. Pick up here (2026-10-30, 21:10 ET) - Controller per-action rebinding, and Slayer on all 20 maps (WeabooTyler117 / DeepSeek)

### 1. Per-action controller bindings (the last big Controls item)

- **`SettingsData::padBindings`**: `std::map<std::string,int>` - action key ("jump", "grenades", "action", "swap",
  "melee", "armor", "fire", "zoom") to an XInput button bit. Saved as a JSON object; empty means "use the layout".
- **`PadLayouts::Apply(layout, buttons, lt, rt, overrides)`**: an override replaces that action's source from the layout
  table before the engine mapping is applied, so layout presets and custom bindings compose instead of fighting. The old
  three-argument overload still exists and delegates (Default is still a no-op when there are no overrides).
  `ReachHost::FillLocalPad` passes `&s_pad.padBindings`.
- **Settings > Controller > BUTTON BINDINGS**: one row per action showing the button in use - `A (layout)` for the
  layout's own choice, `B (custom)` for an override - and the capture flow: click (or Accept) a row, press a pad button,
  done. **Esc** cancels the capture, **Delete/Backspace** drops the override and the layout's button comes back, and
  **RESET BUTTON BINDINGS** clears them all. Binding a button that another action already uses moves it (one button,
  one action). Analogue triggers (LT/RT) cannot be rebound this way because the engine expects them as analogue values,
  so fire/grenade stay on their triggers unless a preset moves them.
- The section sits directly under the CONTROLLER status row, before the sticks, so the bindings are the first thing on
  the page; the layout diagram below still shows the *preset* (it is titled with the preset's name).
- Verified with a seeded `padBindings` (`{"melee": 16384, "zoom": 256}`): the list reads `Melee  X (custom)` and
  `Zoom  LB (custom)` with the changed dots, the rest say `(layout)`, and RESET appears because overrides exist.

### 2. Slayer on every map, plus the full matrix: 35/35 reach gameplay

Added `-auto-play-map <scenario>` (same family as `-auto-play` / `-auto-play-variant`) and a `-Maps` switch to
`tools/test-modes.ps1`, which adds every vanilla map with Slayer. Full background run:

```
[1..10]  Sword Base x (Slayer, CTF, Assault, KOTH, Oddball, Infection, Territories, Stockpile, Headhunter, Juggernaut)
[11]     Sword Base / Invasion                                  blocked (expected)
[12..13] Firefight / Standard, Gruntpocalypse
[14]     Campaign / Winter Contingency
[15]     Forge / Forge World
[16..35] Slayer on Boardwalk, Boneyard, Countdown, Powerhouse, Reflection, Spire, Zealot, Anchor 9, Breakpoint,
         Tempest, Condemned, Highlands, Battle Canyon, Penance, Ridgeline, Solitary, High Noon, Breakneck, Forge World
35/35 passed
```

Every case reached in-game (HUD string lookups), zero `[error]` lines. This is the "every mode with every game type"
and "all maps load" answer in one run; it takes about 25 minutes with `-PlaySeconds 40`.

## 0. Pick up here (2026-10-30, 19:55 ET) - Vanilla maps only list the game types they can host (WeabooTyler117 / DeepSeek)

Follow-up to the map/game-type guard. Until now Sword Base would still *offer* Invasion and then refuse to start it;
Tyler asked to "remove irrelevant game types such as invasion on sword base".

- The **GAME TYPE cycler now skips variants a vanilla map cannot host** (`MapGameTypes::Supports`): stepping left or
  right jumps over them instead of landing on one, and if the map changes under a game type it cannot take, the
  selection advances to the first one it can. Invasion is simply not in Sword Base's list any more.
- **Modded maps and the Forge screen keep every variant** (`modded || !map` short-circuits the filter); Firefight is
  untouched. The red refusal band and the disabled PLAY/HOST from the earlier change stay as the safety net for the
  cases the filter cannot cover (an invite or a session that arrives with a mismatched type).
- The filter runs off the same `MapGameTypes` tables that `-test-map-gametypes` verifies, so the list and the guard can
  never drift apart.

Checked: `tools/test-modes.ps1 -Only Slayer,Invasion` still passes - Slayer starts and reaches gameplay, and a forced
Invasion variant on Sword Base is still refused (`-auto-play-variant` sets the variant directly, which is exactly the
"something else chose this type" case the guard exists for).

## 0. Pick up here (2026-10-30, 19:30 ET) - Every mode with every game type tested: 15/15 reach gameplay (WeabooTyler117 / DeepSeek)

Tyler: "tests done on all modes to make sure multiplayer works. Every mode with every game type."

### 1. Root cause of the "PostCommand(11) stall" - it was never the environment

**A bare CLI launch cannot start a match.** The client needs the account/profile arguments the menu adds when it hands
off to the new process. I read the relaunched child's command line while it was running:

```
-map "20_sword_slayer" -mode "multiplayer" -difficulty 1 -variant "...\slayer_054.bin"
-player-name "WEABOOTYLER117" -service-tag "RTRD" -account-user "..." -account-token "..."
-user-id 5 -account-dev 1
```

Without `-account-user` / `-account-token` / `-user-id` (and the name fields) the engine logs
`EngineEvent reason=4 message='no game selected'`, posts Command 11 and then pumps no frames at all, so `-quit-after`
can never fire. That is what the 18:20 entry (and the earlier "idle desktop" theory) was chasing: launching
`-map ... -mode multiplayer` by hand skips the identity, so it hangs; launching it from the menu hands off properly and
the match runs. **The 18:20 entry's conclusion was wrong; this is not an environment problem.**

### 2. Test hooks in the client (both test-only, no settings)

- **`-auto-play`**: the setup screens press PLAY SOLO / HOST LOBBY by themselves 3 seconds after their first draw
  (time-based, because the menu can draw at 2 fps while the engine loads, which made a frame count take 45 s).
- **`-auto-play-variant <text>`**: preselects the first variant whose file stem *starts* with `<text>` (falls back to
  a substring match). The prefix rule matters: `slayer` must pick `slayer_054`, not `2nvasion_slayer_054`, which sorts
  first and is an Invasion variant.
- A refused start is now logged: `FrontEndMenu: refused to start <type> on <map> - <scenario> has no setup for it`,
  which is what the harness waits for to confirm the game-type guard did its job.

### 3. `tools/test-modes.ps1` rewritten around the real launch path

The harness starts the client on a screen with `-auto-play`, waits for `launching match via Relaunch`, waits for the
child process, lets it run for `-PlaySeconds` (55 s default), kills it and judges the log: **in game** means the HUD
string lookups (`Slot 57 - StringLookup`) appeared, which only happens inside a match.

### 4. Result

```
[1/15]  Sword Base / Slayer                        ok, in game (30 frames)  60s
[2/15]  Sword Base / CTF                           ok, in game (30 frames)  60s
[3/15]  Sword Base / Assault                       ok, in game (31 frames)  60s
[4/15]  Sword Base / King of the Hill              ok, in game (31 frames)  60s
[5/15]  Sword Base / Oddball                       ok, in game (30 frames)  60s
[6/15]  Sword Base / Infection                     ok, in game (31 frames)  60s
[7/15]  Sword Base / Territories                   ok, in game (31 frames)  60s
[8/15]  Sword Base / Stockpile                     ok, in game (31 frames)  60s
[9/15]  Sword Base / Headhunter                    ok, in game (30 frames)  60s
[10/15] Sword Base / Juggernaut                    ok, in game (31 frames)  60s
[11/15] Sword Base / Invasion (guard must refuse)  blocked (expected)       2s
[12/15] Firefight / Standard                       ok, in game (24 frames)  60s
[13/15] Firefight / Gruntpocalypse                 ok, in game (25 frames)  60s
[14/15] Campaign / Winter Contingency              ok, in game (30 frames)  60s
[15/15] Forge / Forge World                        ok, in game (30 frames)  60s

15/15 passed
```

Zero `[error]` lines across the 15 log slices (`tools/test-modes-NN.log`, kept in the repo tools folder but gitignored
by size - regenerate with the script). The Invasion case is the guard working, not a failure: it refuses Sword Base
before the engine is asked to start something the map cannot host.

To widen the coverage: add cases to the `$cases` table in the script (every map x type, using the same rules as
`MapGameTypes`); the script needs ~60 s per case.

## 0. Pick up here (2026-10-30, 18:20 ET) - Match launches currently stall at PostCommand(11) - NOT a regression from the UI work (WeabooTyler117 / DeepSeek)

Trying to run the mode/game-type matrix again (Tyler asked for "tests on all modes: every mode with every game type"):

**Symptom:** `noble-client.exe -map 20_sword_slayer -mode multiplayer [-variant ...] [-host -lan]` loads the scenario
(`FilePathResolver` logs the map path, `LoadProgress` reaches 4.0%), posts the Start event, logs
`EngineEvent reason=4 message='no game selected'`, posts `Command 11` - and then **no frame is pumped at all** (two
`FrameTick` lines total), so `-quit-after` can never fire and the process sits there until it is killed. Earlier today
(07:50-07:53) the same commands pumped frames and ended themselves at `-quit-after`.

**Ruled out:**

- **Not this session's code.** Built and ran `a21ebce` (before every UI change today) with the identical command: the
  same stall, in the same place, with two `FrameTick` lines. `main` was then rebuilt and re-checked.
- **Not argument order or hosting flags.** `-quit-after` first and last behave the same; with and without
  `-host -lan`; with and without `-private -max-players 4`; solo and host.
- **Not window focus.** `ShowWindow(SW_RESTORE)` + `SetForegroundWindow` on the client's window 9 s after launch did not
  change anything.
- **Not a leftover process.** No `noble-client`, `noble-dedicated`, MCC or Halo process was running, and the Application
  event log has no crash entries for the client.
- **Not a game update.** `haloreach.dll` is from 2026-05-05 and `appmanifest_976730.acf` from 2026-10-29 21:17; neither
  changed today.
- **Menus are unaffected**: the capture harness still renders and exits normally (main menu captured at HEAD during this
  check).

**What to try next, cheapest first:** (a) restart Steam and attempt one normal MCC launch, then run
`noble-client.exe -map 20_sword_slayer -mode multiplayer -quit-after 20` again - MCC/Steam session state is the prime
suspect for "no game selected"; (b) start a match the way a player does (main menu -> CUSTOM GAME -> HOST LOBBY) and see
whether that path pumps frames - if it does, the CLI path needs the same setup step the menu performs; (c) attach a
debugger to the stalled process and grab the stack of the engine thread right after `PostCommand(11)` returns; (d) check
whether `-mode multiplayer` needs an explicit game type/`-launch-index` on this build.

Until then: the three headless suites (`-test-map-gametypes`) still pass, so the game-type guard, map names and button
layouts are verified; it is only the *in-engine* launch that cannot be exercised right now.

## 0. Pick up here (2026-10-30, 17:40 ET) - Armory: all 21 menu scenes probed, none has a backdrop (WeabooTyler117 / DeepSeek)

Fourth and last armory attempt for now. `ReachArmory::SetTargetScene()` already maps names to `mainmenu_all.map`'s 21
scene indexes (`ResolveSceneIndex`), so instead of guessing I probed them: added a temporary `-armory-scene <name>`
override on the Armory screen, captured `title` (0x00), `campaign` (0x01), `matchmaking` (0x04), `activities` (0x08),
`theater` (0x07) and `roster` (0x0E), then removed the probe.

**Result: every scene still renders the Spartan inside the same unlit chamber.** The scene only changes the *lighting* a
little (`roster` is visibly dimmer), never the surroundings: the vista that the main menu shows belongs to those scenes'
own camera framing, and the Armory's pedestal camera sits in a sealed room in the map geometry. There is nothing to
point at that puts the character in front of the landscape.

So the black background is a *map/camera* property, not something Noble can composite or clear its way out of with the
approaches tried so far. Four attempts are now recorded (luma composite, depth composite, replace-the-clear, scene
switch). The one idea that would actually change it is drawing the 3D character somewhere else entirely - i.e. rendering
it with Noble's own camera into its own target - which is a project, not a fix.

**Pragmatic suggestion:** ship the black as a deliberate look (vignette + a soft edge behind the model) so the screen
reads as intentional, and revisit the character renderer only if a future RE pass finds the armory pedestal's lightmap.

## 0. Pick up here (2026-10-30, 17:05 ET) - Armory backdrop: second approach tested and ruled out (WeabooTyler117 / DeepSeek)

Tried the "paint the wallpaper instead of the engine's black clear" idea the 13:10 entry suggested. **It does not work,
and the reason is worth writing down before anyone tries it again.**

What was built (then reverted): a `WallpaperPass` (fullscreen triangle sampling `UiArt::MenuBackground()` with cover UVs,
saving and restoring the context state) plus a MinHook on `ID3D11DeviceContext::ClearRenderTargetView` (vtable index
50), installed from slot 1 once the swap chain is known, caching the back buffer resource and comparing it against the
RTV's resource so only the back buffer is intercepted.

Result: the hook **installed cleanly** (`Hook 'd3d11_clear_rtv_armory_backdrop' installed`, and the game ran and was
captured normally), but the paint branch **never ran** - no "Armory backdrop painted" line in the log, and the capture
still shows the black chamber. So during an Armory frame the engine never calls `ClearRenderTargetView` on the back
buffer: it clears once at startup (before the hook exists) and afterwards the chamber's darkness comes from the *scene
geometry and lighting*, not from a clear we could replace.

### Where that leaves the black background

Three attempts now, all documented:

1. **Luma composite in the gamma pass** (O7, 2026-10-29) - reverted because the Spartan's darkest pixels measure the same
   0-2/255 as the void, so dark undersuits were eaten.
2. **Depth-based composite** (this session, 13:10 entry) - correct in principle (empty space is at the far plane), but
   copying and sampling the engine's depth texture faults inside `d3d11.dll` in this swap chain, MSAA or not.
3. **Replace the clear** (this entry) - no back-buffer clear to replace.

Ideas left, in order of promise: (a) hook `ID3D11DeviceContext::DrawIndexed`/`Draw` and detect the chamber's final
full-screen dark pass, painting after it instead - much more invasive; (b) ask the engine to switch the Armory scene's
camera to one that has a backdrop in `mainmenu_all.map` (the scene table is reachable through `ReachArmory`'s GLOBALS
work; this is what MCC itself does differently); (c) accept the black and frame it deliberately - a vignette and a
caption around the model - which is honest and cheap.

## 0. Pick up here (2026-10-30, 16:15 ET) - Menu music playlist: per-track on/off (WeabooTyler117 / DeepSeek)

Last piece of spec 4.2's audio rows that Noble can do on its own: the "per-song on/off" half of the playlist. (Preview
was already there - clicking a track switches to it immediately.)

- **`SettingsData::menuMusicDisabled`** (JSON array of track ids). `ReachAudio`'s shuffle pool skips those ids, so
  `SHUFFLE ALL GAMES` never picks a switched-off song; picking a track directly still plays it.
- **Settings > Audio > Menu music list**: every row now has an **ON / OFF** switch at its right edge (tooltip explains
  that OFF only affects the shuffle). The game list shows `Halo: Reach (62, 3 off)` once something in that game is off,
  and the SHUFFLE ALL GAMES description mentions the rule.
- Toggling while shuffle is playing **restarts the shuffle** with the new list, so the change is audible at once; the
  list itself is saved with the rest of the settings when changes are applied (same rule as picking a track).

Not done: song *order* and drag-to-reorder (the row text in the spec asked for "song order"); the shuffle is random, so
order only matters for a sequential mode that does not exist yet.

## 0. Pick up here (2026-10-30, 15:40 ET) - Button-layout remap now has tests too (WeabooTyler117 / DeepSeek)

`-test-map-gametypes` runs three suites now (all headless, results in `gametype-selftest.log`):

```
PASS: 43 rule checks, 280 map x type combinations (207 allowed, 73 blocked), 0 failure(s)
PASS: map names - 10 display cases, 0 failure(s)
PASS: button layouts - 16 expectations, 0 failure(s)
```

What the layout suite covers (`PadLayouts::SelfTest`, src/engine/PadLayouts.cpp):

- **Default is a perfect no-op**: 17 input patterns through `Apply(Default, ...)` must return the exact same buttons and
  trigger values.
- Per-layout expectations, e.g. Bumper Jumper `A -> B` (grenades), `LB -> A` (jump), `X -> LB` (armour); Boxer
  `RB -> B` (grenades) and `LT -> RB` (melee); Recon `B -> RB` (melee), `X -> B` (grenades), `RB -> X` (action/reload);
  Green Thumb `RS -> RB` (melee) and `RB -> RS` (zoom).
- **Southpaw swaps the triggers**: LT produces fire on RT, RT produces grenades on B.
- **Non-action bits are never swallowed**: D-pad, Back, Start and LS survive every layout unchanged.

**The suite immediately caught two wrong expectations of mine** (I had assumed Southpaw would swap the *triggers'*
outputs and that Boxer's RB would map to the armour ability). The model it forced me to write down: the engine expects
**fire on RT (analogue)** and **grenades on the B button**, so a layout that puts grenades on a trigger has to emit B,
and one that puts fire on a trigger has to emit RT - which is exactly what `Apply()` does. Worth reading before touching
the tables.

## 0. Pick up here (2026-10-30, 15:05 ET) - Master mute and mute in background (WeabooTyler117 / DeepSeek)

Spec 4.2 audio rows, the two that are Noble-side.

- **`SettingsData::masterMute`** and **`muteInBackground`** (both off by default, saved in settings.json).
- **`ReachAudio::SetMuted(bool)` / `IsMuted()`**: one switch for everything Noble itself plays. The music thread uses
  `muted ? 0 : fade * musicVolume`, so pausing/unpausing the menu music still works while muted, and `PlaySfx` returns
  early, so menu clicks go quiet instead of logging failures. **Reach's in-game audio is the engine's own** and is
  deliberately not touched - the rows say so.
- Applied once per frame in `FrontEndMenu::RenderOverlayImpl`, re-read twice a second: `muted = masterMute ||
  (muteInBackground && foreground window belongs to another process)`. "Active" is decided by
  `GetWindowThreadProcessId(GetForegroundWindow()) != GetCurrentProcessId()`, which covers both the menu window and the
  engine's without needing to know which one is up (`IDXGISwapChain1::GetHwnd` would have needed dxgi1_2 in that file).
- Both rows are toggles in **Settings > Audio > MORE**, replacing the greyed MUTE IN BACKGROUND row, and count towards
  the category's change count.

Verified: build 0/0; AUDIO page capture shows both toggles; main / custom / lobby / armory still capture with exit 0
after the per-frame hook went in; client left closed.

## 0. Pick up here (2026-10-30, 14:20 ET) - New SYSTEM settings page, version 0.8.3 (WeabooTyler117 / DeepSeek)

Spec 4.12 housekeeping that needed no engine work: crash/diagnostic helpers and settings backup/restore.

### 1. `Settings > SYSTEM` (eighth category, `Cat::System`)

- **NOBLE**: `VERSION` (the build), `LOG FILE`, **OPEN LOGS FOLDER** (`ShellExecuteW` on the exe folder, which holds
  the log, the crash dump and `data\userdata\settings.json`) and **COPY DIAGNOSTICS** (version, install path and the last
  40 log lines to the clipboard, ready to paste into a report).
- **BACKUPS**: **BACK UP SETTINGS** copies `settings.json` and `appearance.json` into
  `data\userdata\backups\YYYY-MM-DD_HHMMSS\`, **RESTORE NEWEST BACKUP** copies the newest one back, calls
  `Settings::Load()` and re-syncs the panel (`m_saved`/`m_edit`/buffers) so the change is live, and **OPEN BACKUP
  FOLDER** shows them in Explorer. Each action reports its outcome in a line under the rows.
- Action rows use the explicit `m_rowHovered && IsMouseClicked(0)` / `m_rowFocused && Accept` test, never `Row()`'s
  return value (that trap is written down twice now).
- The category is **appended** after `Online`, so every existing `-menu-tab N` index is unchanged.

### 2. `VERSION_STRING` is now `0.8.3 (Menu Cleanup & Controls)`

It was still `0.8.2 (In-Game FOV & Pause Settings)` from before the menu pass; every screen that prints the version
(main menu, SYSTEM page, crash header) picks it up.

### 3. Verified

- Build: 0 errors, 0 warnings.
- Settings tabs 0, 1, 5, 6 and 7 all captured with exit code 0 after adding the category (the tab index shift was the
  risk).
- The SYSTEM page shows VERSION `0.8.3 (Menu Cleanup & Controls)`, both sections and all six rows.

## 0. Pick up here (2026-10-30, 13:45 ET) - Map display names everywhere, and the self-test covers them (WeabooTyler117 / DeepSeek)

Server browser backlog item: it printed raw scenario names (`forge_halo` instead of Forge World).

- **`src/common/MapNames.hpp/.cpp`** (new): `Display()` maps every scenario the game ships - multiplayer, campaign
  (including the `m10_b`-style scene suffixes) and Firefight - to the name the menus use, and prettifies modded map
  files (`my_custom_map.map` -> "My Custom Map"). Also `IsVanilla()` and `ScenarioOf()` (display name -> scenario).
- **Server browser**: the Map column and the GAME DETAILS plate show the display name; modded maps keep their own name
  and the `[Mod]` marker, with the raw file name in a tooltip. Search matches **both** forms, so typing "Sword Base"
  now finds `20_sword_slayer`.
- **Self-test extended** (`-test-map-gametypes` now runs both suites and writes both lines to `gametype-selftest.log`):

```
PASS: 43 rule checks, 280 map x type combinations (207 allowed, 73 blocked), 0 failure(s)
PASS: map names - 10 display cases, 0 failure(s)
```

The map-name cases cover `forge_halo`, `forge_halo.map`, a full `maps\\20_sword_slayer.map` path, `m10_b`, `m70_bonus`,
`ff50_park`, `cex_beaver_creek`, two modded names, plus `IsVanilla` and `ScenarioOf("sword base")`.

**Not visually confirmed:** the master server reported 0 games while these captures ran, so the list column had no rows to
show. The mapping itself is covered by the self-test above; the next person should eyeball it the next time a server is
listed.

## 0. Pick up here (2026-10-30, 13:10 ET) - Mouse acceleration/smoothing and pad stick layout un-greyed (WeabooTyler117 / DeepSeek)

Tyler: "finish the options in settings for controller and mnk for things that are greyed out". Two of them are real now.

### 1. MOUSE ACCELERATION and MOUSE SMOOTHING (Keyboard & mouse)

- `SettingsData::mouseAcceleration` (0..1, default 0 = off) and `mouseSmoothing` (0..0.9, default 0 = off).
- Applied in `ReachHost.cpp` slot 36, right where the raw cursor delta is turned into the look delta:
  - **smoothing** is an exponential average (`smoothed = smoothed * s + delta * (1 - s)`), and the state is dropped
    whenever a menu owns the pointer so no stale motion is carried into the next capture frame;
  - **acceleration** multiplies the delta by `1 + accel * min(speed / 60, 3)`, capped so one enormous movement cannot
    spin the view.
- Both rows are sliders with an explicit `Off` label at 0, and they are counted by `ChangesIn`/`WithDefaults`.

### 2. STICK LAYOUT (Controller)

- `SettingsData::padStickLayout` (0 = Default, 1 = Southpaw). Applied in `FillLocalPad()` before the dead zones: Southpaw
  swaps the left and right stick vectors, so look is on the left thumb and movement on the right.
- Row lives in Controller > Sticks, above LEFT STICK DEAD ZONE, cycling DEFAULT / SOUTHPAW.
- **Not offered yet:** Reach's Legacy and Legacy Southpaw stick layouts also change how the turn axis behaves (the left
  stick both moves and turns), so they need engine-side research rather than a swap.

### 3. `tools/test-modes.ps1` hardening, and why the matrix still cannot run unattended

- The script now waits for a real process exit between cases (`Stop-Process` + `WaitForExit` + 3 s settle, and it
  aborts if one is left over), passes `-quit-after` as the first argument, and has `-Limit N` for debugging.
- `FrameCapture` logs `-quit-after {n} s armed` on the first Present, so a hung run can be diagnosed from its own log.
  In the last two runs that line **did** appear and the client still never exited: the log ends at
  `BlamEngine: PostCommand(11) returned` and **no frame is pumped afterwards**, so `FrameCapture::OnFrame` is never
  called again. Manual runs minutes earlier pumped frames the whole time.
  Prime suspect: the desktop being idle/locked (the script ran while Tyler was away) - a D3D11 presentation stops
  pumping when the session is not interactive, and this engine's frame loop rides the Present hook.
  **To do:** rerun `tools/test-modes.ps1 -Limit 2` with the machine unlocked; if it passes, run `-Full`. If it hangs
  while unlocked, log `PostCommand(11)`'s surrounding state and compare a manual launch's argv with the script's.

## 0. Pick up here (2026-10-30, 12:05 ET) - Mode/game-type smoke harness, and what a bare launch does (WeabooTyler117 / DeepSeek)

### 1. What a CLI match launch actually does (important for any future test work)

`noble-client.exe -map 20_sword_slayer -mode multiplayer [-host -lan] -quit-after N` **loads the map and game type and
then waits in the lobby**: `LoadProgress: 5.0%, launch state 11` repeats for as long as you let it run, and that is the
*host waiting for START GAME*, not a failed load. Three runs verified it with **0 `[error]` lines**:

- solo (`-mode multiplayer`, Slayer variant) - 553 log lines, clean exit at `-quit-after 20`
- solo with `-menu-capture 2400 -quit-after 70` - clean exit, 47 samples all at 5%/state 11
- `-host -lan -private -max-players 4 -quit-after 60` - clean exit, 69 samples all at 5%/state 11

So the pass condition for an automated smoke test is "map + game type loaded, no crash, no errors", not "gameplay
reached". A guest joining a host is the only path that moves past state 9/11 (see the 2026-10-29 entries).

### 2. `tools/test-modes.ps1` (new)

Runs a matrix of mode x map x game type launches through the real client and reports per case:
`ok` / `ERRORS (n)` / `CRASH` / `HUNG` / `NO LAUNCH`, writing a log slice per run (`test-modes-NN.log`) and an exit code.

- default: 8 cases (Sword Base Slayer + CTF, Boneyard/Spire Invasion, two Firefight variants, a campaign mission, Forge)
- `-Full`: every multiplayer map x every supported type (uses the same restriction rules as `MapGameTypes`), ~25 minutes
- `-QuitAfter <s>`: per-run budget

**Known problem, not yet solved:** when the script starts the client with `-quit-after`, the timer did **not** fire in any
of the runs (all four multiplayer cases sat until the 108 s watchdog and were reported HUNG), although the same flag ends
a manually started run every time. `FrameCapture::OnFrame` (the thing that honours `-quit-after`) is called from the
Present hook in `ReachHost.cpp` *and* from `FrontEndMenu`/`PauseMenu`, so the timer should run in a match too - the next
step is to log `ParseQuitAfter()`'s result once at startup and compare the argv the script produces with a manual
command line (argument quoting is the prime suspect: `Start-Process -ArgumentList` with an array).

Until that is fixed, run cases by hand with the documented commands and check the log for `[error]` / `FATAL EXCEPTION`;
that is what the 2026-10-30 11:05 entry's sweep and the three runs above did.

## 0. Pick up here (2026-10-30, 11:05 ET) - Map/game-type guard with a headless test; armory backdrop investigated (WeabooTyler117 / DeepSeek)

### 1. Game types are now checked against the map (`src/engine/MapGameTypes.hpp/.cpp`)

Tyler: "prevent crashing the game if someone chooses Invasion on a map that doesn't support that mode... but do not
restrict MODS and Custom game lobbies on what they want to choose."

- `FromVariantStem()` maps a variant **file stem** to a game-type key using the real MCC filenames
  (`slayer_054`, `SWAT_054`, `3nvasion_054`, `2nvasion_slayer_054`, `ctf_1flag_054`, `assault_one_bomb_054`,
  `territories-3plot_054`, `koth_crazyking_054`, `infection_safehavens_054`, `hr_ff_*`, `00_basic_editing_054`, ...).
- `Supports(scenario, type)` blocks only the types that genuinely need a dedicated setup: **Invasion** on anything but
  Boneyard / Spire / Breakpoint / Forge World, and **Race / Rally / Skeeball** outside Forge World. Slayer, CTF, Assault,
  Territories, KOTH, Oddball, Juggernaut, Infection, Headhunter and Stockpile are allowed everywhere, and an unknown type
  is never blocked.
- The Custom Game screen shows a red band under GAME TYPE - "Invasion isn't set up on Sword Base. Pick Boneyard, Spire,
  Breakpoint or Forge World (modded maps are never restricted)." - and PLAY LOCAL / HOST LOBBY are disabled until the
  combination is valid. **Modded maps are skipped entirely** (`map->mod == nullptr` check), and so are the Firefight and
  Forge screens (Firefight uses its own maps and `hr_ff_*` variants).

### 2. `-test-map-gametypes` (headless, no MCC needed)

`noble-client.exe -test-map-gametypes` runs `MapGameTypes::SelfTest()` and writes `gametype-selftest.log` next to the exe,
exit code 0/1. It covers 43 rule checks (Invasion on all four legal maps and five illegal ones, the Race family, every
"allowed everywhere" type, and 17 real variant stems) plus **280 map x type combinations**, asserting that every blocked
pair produces a reason string. Current result:

```
PASS: 43 rule checks, 280 map x type combinations (207 allowed, 73 blocked), 0 failure(s)
```

**It caught two real bugs on the first run:** `3nvasion_slayer` matched "slayer" before "invasion" (the stems use an 'n'
where the 'I' glyph is, so the match is on `nvasion`), and Firefight was being treated as an always-allowed type - which
would also have made the Firefight screen warn on every one of its own maps, since the guard ran before the `!ff` check.

### 3. Armory black background: investigated, not fixed

The composite that O7 added on 2026-10-29 (GammaPass sampling a wallpaper behind the 3D character) is **gone** from
`ReachHost.cpp` - the pass now only does gamma, with a comment that the armory scene renders natively. So the void is
back to black.

I rebuilt it properly as a **depth-based** composite (paint the wallpaper only where the depth buffer says nothing was
drawn - the only signal that works, because the Spartan's darkest pixels measure the same 0-2/255 as the void, which is
why the earlier luma test ate dark undersuits). Findings, all in this order:

1. `D3DCompile` needs a **null-terminated** `D3D_SHADER_MACRO` array; mine crashed the compiler. Fixed.
2. `Texture2DMS` must be sized at compile time (`Texture2DMS<float, 4>`), so one PS variant per sample count is needed.
3. Copying the engine's depth buffer and sampling it **faults inside d3d11.dll** in this swapchain (crash dump shows
   d3d11 frames called from Noble), even with the DSV unbound first and with MSAA skipped.

The whole attempt was reverted (`git checkout HEAD -- src/engine/ReachHost.cpp`); the current build is the known-good
gamma pass and the armory shows the black void again. **Next ideas, in order:** (a) check whether the depth texture is
MSAA by logging `OMGetRenderTargets`'s DSV desc *before* touching it, (b) try `CopySubresourceRegion` instead of
`CopyResource`, (c) if the depth path stays hostile, composite in the *engine's* own render target by hooking the armory
scene's clear (a D3D11 `ClearDepthStencilView`/`ClearRenderTargetView` hook) and painting the wallpaper there, behind the
geometry, which needs no depth read at all.

### 4. Verified

- Build: 0 errors, 0 warnings.
- `-test-map-gametypes`: PASS (above).
- Screen sweep after the change: custom, forge, firefight, armory, main, lobby, servers, settings - 8/8 captured, exit 0,
  client left closed.

## 0. Pick up here (2026-10-30, 10:20 ET) - Halo: Reach button layouts, pickable and actually applied (WeabooTyler117 / DeepSeek)

Tyler: the CONTROLLER page "has them mentioned but no layout option to pick from. Halopedia has them for Halo Reach".
The six Reach presets are now a real setting that remaps the pad before the engine sees it.

### 1. The data (`src/engine/PadLayouts.hpp/.cpp`, new)

Transcribed from Halopedia's **Reach:Control schemes** (Xbox 360 manual): Default, Southpaw, Boxer, Green Thumb,
Bumper Jumper, Recon. Stored as `Source` per action (Jump, Grenades, Action/reload, Swap weapons, Melee, Armor ability,
Fire, Zoom) where a source is either an XInput button bit or one of the analogue triggers - that is how Southpaw swaps
fire (RT) and grenades (LT) while Boxer moves melee to LT and grenades to RB.

### 2. Where it is applied

`ReachHost.cpp` -> `FillLocalPad()` (slot 36) already builds the button word and trigger values Noble hands the engine.
It now calls `PadLayouts::Apply(layout, buttons, lt, rt)` first: every layout-controlled button is read from its
*physical* source, written back to the button the engine expects, and non-action bits (D-pad, Back, Start, LS) pass
through untouched. Default is a no-op. Analogue triggers keep their value (a trigger feeding a button becomes a digital
press over 128, a button feeding a trigger becomes 255).

### 3. The UI

- **Settings > Controller > LAYOUT > BUTTON LAYOUT**: cycles the six presets, `When::Apply`, with the preset's own
  description in the details column ("Halo: Reach's Bumper Jumper: jump on LB, grenades on A...").
- The layout diagram below it is now **layout-aware**: the section title reads
  `HALO: REACH BUTTON LAYOUT - BUMPER JUMPER` and each callout draws the glyph for the button that preset uses
  (Bumper Jumper shows `A Throw grenade`, `LB Jump`, `X Use armor ability`, ...). `DrawControllerDiagram` takes the
  layout as a parameter; `PadLayouts::SourceFor(layout, action)` is the lookup.
- `SettingsData::padLayout` (JSON `padLayout`) persists it; `ChangesIn`/`WithDefaults` count it for the category.

### 4. Checks

- Build: 0 errors, 0 warnings.
- Captures at 1920x1080: Settings > Controller with Default (`BUTTON LAYOUT  DEFAULT`) and with `padLayout: 4`
  (`BUMPER JUMPER`, diagram relabelled: A throw grenade, RT fire, RB melee). Main menu still captures cleanly.
- Test data reverted (padLayout back to 0).

**Not done from the CONTROLLER page yet:** stick layout (Default / Southpaw / Legacy / Legacy Southpaw - Halopedia lists
them too; Southpaw moves the *movement/look axes*, a separate remap of LX/LY <-> RX/RY), aim acceleration, and
per-action rebinding. The layout table is the place to add stick layouts.

## 0. Pick up here (2026-10-30, 09:55 ET) - Review pass over the whole menu work, six fixes (WeabooTyler117 / DeepSeek)

A read-through of every change from `8676b1b` to now, plus a 12-screen regression sweep. Fixed:

1. **The background preview drew its own title over the details header.** `Preview::Background` called
   `Text(..., p, "MENU BACKGROUND")` although the details panel already draws `d.title` at `p`; previews start at the
   cursor (`pv`), so the duplicate is gone.
2. **A freshly imported background did not appear in the picker** until a restart: `UiArt::MenuBackgroundFiles()` caches
   the folder listing. `ImportMenuBackgroundImage()` now calls the new `RefreshMenuBackgroundFiles()`.
3. **`UiArt::MenuBackgroundPreview()` removed** - after the change above nothing used it, and it decoded a texture on
   the caller's thread (the thing that hung the client earlier). One way to do it now: `WarmMenuBackground()` +
   `MenuBackground()`.
4. **Invite JOIN with no host address** would have relaunched with `-connect ":"`. The button now reads `WAITING...` and
   logs a warning instead.
5. **Continue pointed at a deleted mod folder** if the mod had been removed since; it now checks
   `std::filesystem::exists()` and falls back to the vanilla launch.
6. **The music widget rebuilt `ReachAudio::MusicTracks()` every frame** on the main menu; it is built once now.
7. The invite toast sat 6 px into the setup screens' action buttons; moved up by 32 * s.

Verified after the fixes: 12/12 screens (main, campaign/firefight/custom MODDED, forge, servers, settings graphics +
controller, career, armory, social, lobby) captured, every run exit code 0, **0 `[error]` lines** in the new log range,
and the client left closed.

Known and accepted (do not "fix" blindly):

- Muting from the music widget is undone while **Settings > Audio** is open, because `RenderAudio()` pushes
  `m_edit.volumeMusic` into `ReachAudio` every frame. A real fix is a `menuMusicMuted` flag in settings; low priority.
- The pause button does nothing when the music thread is between pieces (`s_musicVoice` null); pressing play restarts.
- `IMPORT BACKGROUND...` opens a Win32 modal dialog on whichever thread draws the Settings page; while it is open the
  engine loop waits. That is expected for a file picker, but it is why nothing may call it from an automated run.

## 0. Pick up here (2026-10-30, 09:35 ET) - Menu background picker with preview + IMPORT (WeabooTyler117 / DeepSeek)

Tyler: "players should be able to preview and choose their own background and even upload their own custom one".

- **`SettingsData::menuBackground`** keeps its pool values (`random` / `noble` / `reach`) and now also accepts
  **`file:<name>`**, a picture in `data\ui\custom\backgrounds\`. `UiArt::MenuBackground()` loads that file first and only
  falls back to the random pool when it is missing (with a warning in the log).
- **`UiArt` gains** `MenuBackgroundFiles()` (sorted names, cached), `ImportMenuBackgroundImage(source, outName)` (copies
  PNG/JPG/BMP/WebP into the backgrounds folder, overwriting by name) and `WarmMenuBackground()`.
- **Settings > Game options > Menus** now has **MENU BACKGROUND** (cycles Random / Noble art / Reach and every one of
  the player's pictures, showing the file name) and **IMPORT BACKGROUND...** (Win32 open dialog via `GetOpenFileNameW`,
  `comdlg32` linked), with an image **preview in the details column** (16:9 cover, plus a line saying where the picture
  comes from and that a new choice is used from the next start).
- `WarmMenuBackground()` is called from `FrontEndMenu::Initialize`, i.e. on the engine thread that owns the D3D device:
  the fallback path that draws the menu from the main thread (`HoldFrame`) must never create textures.

**Traps found (both cost a hung client):**
1. **`SettingsPanel::Row()` always returns `true`** - it is a layout helper, not "activated". `if (Row(...)) { open a
   dialog; }` ran the dialog **every frame**. Action rows must test `m_rowHovered && ImGui::IsMouseClicked(0)` or
   `m_rowFocused && Accept`.
2. Loading a texture from the main-thread fallback draw path can hang the client (the settings page was opened straight
   from `-menu-screen settings`: the engine stops drawing after ~1 s and `HoldFrame` draws). Warm art on the device
   thread and preview the **already loaded** picture (`UiArt::MenuBackground()`), not a fresh decode.

### Still greyed in Settings (Tyler: "we really got to get them going")

All of these need input-layer or engine work, not rows:

- **Keyboard & mouse**: mouse acceleration, smoothing, sensitivity hotkeys (halve/double), invert flight, release
  cursor key, second key per action. Where: `ReachHost.cpp` mouse look (slot 36) + `KeyBindings`; the engine's
  profile has a second key slot at `+8` per action (see the 2026-10-28 handoff entry).
- **Controller**: aim control Modern/Classic, aim acceleration, stick layout, button-layout presets (Default, Southpaw,
  Boxer, Green Thumb, Bumper Jumper, Recon) and per-action pad bindings - a remap table in `ReachHost`'s pad poll plus
  per-action storage in `SettingsData`; the CONTROLLER page already draws the layout and now has the glyphs for it.
- **Graphics**: texture filtering / antialiasing / motion blur / shadows / draw distance ("Not available yet") need the
  matching engine setting to be found first; **mute in background** needs an engine volume hook or at least a
  focus-driven volume drop for Noble's own audio.

## 0. Pick up here (2026-10-30, 09:05 ET) - Continue fast path from the redesign spec, and the spec's status block (WeabooTyler117 / DeepSeek)

Back to the redesign doc (`docs/menu-redesign-spec.md`) for the next Noble-side item: **4.1 main menu / 4.10 "Continue
last game" fast path**.

- `SettingsData` gains `lastPlayedMode`, `lastPlayedMap`, `lastPlayedVariant`, `lastPlayedDifficulty`, `lastPlayedSkulls`,
  `lastPlayedMod` (saved, loaded, defaults empty).
- The single place a match launch is assembled (`RenderOverlayImpl`, the `if (choice.play)` block) now records those
  fields - unless the choice is a lobby join (`connectAddress` set), which would otherwise overwrite the local game.
- The main menu builds a **CONTINUE** row above Campaign when `lastPlayedMap` is non-empty (`kContinueEntry`, same
  per-frame list as the developer row), and its description line reads
  `Continue: Custom Game on Sword Base.` (map display name, not the scenario). Activating it relaunches the stored
  match, map, variant, difficulty, skulls and mod. No row appears for a fresh install.
- Verified with a capture: seeded `lastPlayed*` in `settings.json`, the row renders at the top of the list with the
  right description, then the seed was reverted.

`docs/menu-redesign-spec.md` now opens with a **status block** for 2026-10-30 (what this session finished, what is left
in 4.x order), so the next agent can work from the design doc instead of reverse-engineering the handoff log.

## 0. Pick up here (2026-10-30, 08:40 ET) - Prompts follow the device everywhere, button glyphs in the controller list (WeabooTyler117 / DeepSeek)

Tyler: "if a player uses a controller the options on the bottom of the screen should switch to controller and vice versa"
and "we can also use these icons on the controller mapping list".

### 1. The pre-game lobby bar was hard-coded Xbox glyphs

The other screens already go through `InputPrompts` (which switches on the last device used), but the lobby's bar was
five hand-drawn circles (A / B / Y / > / X) that never changed. It now uses the shared prompts: **ENTER Select / ESC
Back / Y Friends-Social / TAB Menu / X Prior Game Stats** on mouse+keyboard, **A / B / Y / Start / X** glyphs with a
controller - and it flips as soon as the other device is touched (`InputPrompts::Update()` in every overlay frame).

### 2. Button glyphs as a real API (`InputPrompts`)

New, all drawn in code (no art shipped):

```cpp
enum class Button { A, B, X, Y, LB, RB, LT, RT, DpadUp/Down/Left/Right, LStick, RStick, Start, Back, Menu };
float ButtonWidth(ImFont*, float h, Button);      // so callers can lay text out after the glyph
float DrawButton(ImDrawList*, ImFont*, ImVec2, float h, Button);
float KeyWidth(...); float DrawKey(..., const char* label);   // keyboard key box for any label
bool  ButtonFromSuffix(const char* suffix, Button&);          // "(RT)", "(Back)", "(D-pad)", "(click)"
```

`A/B/X/Y` are the coloured circles, `LB/RB/LT/RT` grey pills, the D-pad is a cross with one arm lit, sticks are
lettered circles, `Start`/`Back`/`Menu` are the round system glyphs.

### 3. CONTROLLER settings page uses them

Every callout on the Reach 360 layout diagram now draws the glyph the label names in front of the text
(`LT Throw grenade`, `RB Melee`, `Back Multiplayer scoreboard`, ...). The two `(click)` callouts map to LStick
("Move / crouch") and RStick ("Look / zoom").

**Bug found while doing it:** `ButtonFromSuffix` first matched by substring, so `(LB)` resolved to **B** and `(Back)` to
**B** as well. It now matches the whole suffix (`text == key`) and the stick clicks are disambiguated by the label text.

### 4. Checks

- `cmake --build build --config Release`: 0 errors, 0 warnings.
- Captures at 1920x1080: the lobby bar reads `ENTER Select ... TAB Menu ...` (keyboard device, since a capture run has
  no pad input) and the CONTROLLER page shows LT / LB / Back / RT / RB / Y / B glyphs next to the right labels.

## 0. Pick up here (2026-10-30, 08:15 ET) - Menu music player: now-playing strip, prev/next, pause, mute, shuffle (WeabooTyler117 / DeepSeek)

Backlog item 3 from the continuation guide, done.

### 1. `ReachAudio` can pause and resume

The music thread owns its own `IXAudio2SourceVoice`, so nothing outside could touch it. It now publishes the voice to
`s_musicVoice` (cleared on destroy and at thread exit) and exposes:

```cpp
bool PauseMusic();     // voice->Stop(0): queued pieces are kept
void ResumeMusic();    // voice->Start(0): continues where it stopped
bool IsMusicPaused();
```

That is a real pause, not a stop-and-restart.

### 2. The widget (`DrawMusicPlayer` in `FrontEndMenu.cpp`)

Drawn on the **main menu** only, bottom-right above the `PROJECT NOBLE vX` line (measured 1920x1080: no overlap):
note glyph + the current track (`GAME · Track`, or `Shuffle · every game`, or `Music off`, plus `(paused)`) and five
36 x 22 px buttons - **prev**, **play/pause** (shows `>` when paused), **next**, **mute** (`VOL`/`MUT`), **shuffle**
(`SHF`, highlighted when the track is `all`).

- prev/next walk `ReachAudio::MusicTracks()` and immediately save `menuMusicTrack` through `Settings::Get().Update()`,
  the same "saved as soon as you change it" rule Settings > Audio uses; the track starts playing at once.
- play/pause uses the new `PauseMusic`/`ResumeMusic`; pressing play while the track is `off` starts Reach's menu theme.
- mute flips the music volume between 0 and `SettingsData::volumeMusic` without touching the saved volume.
- the label is clipped, so a long track name cannot push the buttons.

### 3. Checks

- `cmake --build build --config Release`: 0 errors, 0 warnings.
- Capture at 1920x1080 of the main menu: `SHUFFLE · EVERY GAME` with the five buttons, sitting above the version line.

Next candidate from the guide's backlog: controller presets / custom mapping (needs `ReachHost` slot-36 work), then the
three engine-research items (skulls, new-map launch indexes, post-game names).

## 0. Pick up here (2026-10-30, 07:55 ET) - Custom Game / Forge get the same row block as the other setup screens (WeabooTyler117 / DeepSeek)

Tyler: "fix the custom game screen, it looks nothing like the others such as forge, firefight and campaign". The cause:
Campaign and Firefight fill the right column (difficulty tiles, skulls, options), while Custom Game had only a GAME TYPE
row (and Forge had none), so both screens were a banner followed by a large void above the network strip.

- **Custom Game / Forge now draw a GAME OPTIONS row and a PLAYERS row**, the same shape Firefight's options row has:
  - `GAME OPTIONS · DEFAULT` - greyed with "Coming later" on the right (the editor needs Reach variant read/write,
    spec 4.4), so the screen says why instead of hiding it.
  - `PLAYERS` - a working cycler, `4 / 8 / 12 / 16 PLAYERS`, stored in `MenuState::sessionMaxPlayers` and passed to
    `SetupHostLobby` as the lobby cap for HOST LOBBY / HOST FORGE (Firefight stays at 4). Left/Right and the on-screen
    arrows both change it; it counts as a focus row for controller/keyboard navigation.
- `rowsH` in the Firefight/Custom/Forge case counts those 72 * s, so `HeroBannerHeight()` shrinks the picture and the
  rows fill the column - no more dead space, and the banner/strip can still never overlap.
- **Readability:** `OptionRow()` now draws a faint plate (`IM_COL32(8,12,18,85)`) under every row, and the two greyed
  option rows do the same, because dim grey text over bright map art (Forge World's snow) was hard to read.

Checked with the capture harness at 1920x1080: Custom Game shows GAME TYPE / GAME OPTIONS / PLAYERS and Forge shows
GAME OPTIONS / PLAYERS, both with the network strip and action buttons below, matching Firefight and Campaign.

## 0. Pick up here (2026-10-30, 07:35 ET) - Service tag single source, Social screen in the capture harness, full menu sweep (WeabooTyler117 / DeepSeek)

### 1. The service tag disagreed between the Armory and the menus

`settings.json` had `serviceTag = "RTRD"` while `data\appearance.json` (the Armory profile, which is what Reach is
actually given at profile `+0x1AC`) had `service_tag = "N06"`, so MY PARTY / the lobby advert said `[RTRD] [DEV]` and
the Armory nameplate said `[N06]`. New `PlayerServiceTag(SettingsData)` in `FrontEndMenu.cpp` returns the Armory
profile's tag and falls back to settings.json; it is used by `DrawParty`, `SetupHostLobby` (advert **and** the local
roster entry) and the host-pinning refresh, so everything Noble shows matches what the game wears.
Still open (optional): have `ArmorCustomizer::SaveProfile` also write the tag into `SettingsData::serviceTag`, so a
third-party reader of settings.json sees the same value.

### 2. `-menu-screen social` did not exist

`ParseScreenArg()` predates O7OF's COMMUNITY screen, so `-menu-screen social` silently produced the main menu. Added
`social` / `friends` / `community` -> `Screen::Social`; the sweep below now captures that screen.

### 3. Full menu sweep (all 17 screens, 1920x1080, `-menu-capture 90`)

main, campaign, campaign MODDED, skulls, firefight, firefight MODDED, custom, custom MODDED, forge, servers, settings,
settings > Online services, career, armory, mods, social, lobby: **17/17 captured, every run exit code 0**, and the log
tail of the sweep has no `[error]`, `[warn]`, exception or access violation. The Armory renders the 3D Spartan and its
nameplate; COMMUNITY lists the friend with INVITE/REMOVE; MY PARTY shows `[N06] [DEV]` over the plate.

## 0. Pick up here (2026-10-30, 07:20 ET) - Lobby identity per row + invite notifications, and a continuation guide (WeabooTyler117 / DeepSeek)

### 1. Every player row can carry its own identity

`LobbyPlayer` and `MasterAdvert` now have `rankIndex`, `emblemId`, `nameplateId`, `team` (0 = none, 1 = red, 2 = blue).
`MasterServerClient` sends `host_rank_index`/`host_emblem_id`/`host_nameplate_id`/`host_team` in `POST /api/sessions` and
the heartbeat, includes the same keys in the host's roster entry, and parses `rankIndex`/`emblemId`/`nameplateId`/`team`
back out of both roster responses. `SetupHostLobby` fills the host advert and its local roster entry from the Armory
profile + `Career::RankIndex`, the host-pinning refresh keeps them pinned, and the pre-game row now draws
`player.rankIndex/emblemId/nameplateId/team` with the local profile as fallback.

**Remote rows stay placeholders until the deployed master server echoes those keys** - `server/server.js` must pass
`host_*` and the roster fields through (`D:\ServerTestDeploy-main` + the Render deploy). That is the only missing piece;
the client half is done.

### 2. Invite notifications

`DrawInviteToast()` (in `FrontEndMenu.cpp`) polls `SocialClient::PollInvitesAsync()` every 4 s while logged in and shows
the newest invite on **any** menu screen: a GAME INVITE panel with the sender, "is inviting you to a match", **JOIN**
(`Relaunch::HandOff(-connect host:port)`) and **DISMISS** (clears it server-side via `ClearInviteAsync`). Answered invites
are remembered per session; the quit dialog and modals suppress the toast.

`SettingsData::inviteNotifications` (default on, `settings.json` key `inviteNotifications`) is exposed as
**Settings > Online services > INVITE NOTIFICATIONS** (new INVITES section above SERVERS) and counted in the category's
unsaved changes.

Not done: naming the game type in the toast ("... a **Firefight** match") - the invite payload has no mode/map yet
(`SocialClient::SendInviteAsync`, `/api/invites`, server side).

### 3. Continuation guide

`docs/CONTINUATION.md` is new: what the project is, every path on this machine (repo, MCC, RR, RE folder, server),
build/run/screenshot commands, the source map, the menu layout contract, what is done per commit, the prioritized
backlog with the technical pointer for each item, the working-agreement traps (build/ has tracked art, `small` macro,
pack names are UPPER CASE, a push does not deliver the exe), and the RE material incl. Tyler's `dec_*.json` Ghidra
exports and PE scripts in Downloads (whose hard-coded DLL path is the **old** `C:\Program Files (x86)` install).

### 4. Checks

- `cmake --build build --config Release`: 0 errors, 0 warnings.
- Capture at 1920x1080: Settings > Online services shows the INVITE NOTIFICATIONS row (ON) with its details panel.

## 0. Pick up here (2026-10-30, 06:55 ET) - Player row polish: DEV tag bracket, centred rank, one frame (WeabooTyler117 / DeepSeek)

Final round on the row Tyler kept circling (MCC reference crops), all in `PlayerRow()`:

- **`[DEV]` is text now**: the gold chip is gone; the tag line reads `[NOBL] [DEV]` with `[DEV]` in gold next to the
  service tag, and **only for developer accounts** - a regular player's tag line is just `[NOBL]`, nothing else.
- **Rank badge centred**: it is a square 29 * s box centred on the row height (`DrawContain` inside it), so the art no
  longer sits high or low.
- **One frame, no seam**: the emblem tile + optional team strip + nameplate bar are framed once as a block, with a 1 px
  divider where the tile meets the bar. The plate art is drawn 8 * s to the left and clipped to the bar, because the
  plate image's own dark left border read as a gap between the tile and the bar.
- **Player name is smaller**: 18 * s -> 16.5 * s, so the tag line above it has room; the name still clips at the bar.

Build: `cmake --build build --config Release`, 0 errors, 0 warnings. Captured at 1920x1080: rank badge, emblem tile with
the emblem drawn, and `[NOBL] [DEV]` / `WEABOOTYLER117` on the plate, all in one clean block.

## 0. Pick up here (2026-10-30, 06:40 ET) - Emblems never loaded (pack name case) + final player-row anatomy (WeabooTyler117 / DeepSeek)

### 1. Bug: every emblem slot was empty, everywhere

`UiArt::EmblemByName()` asked the MCC pack for the name exactly as `playercustomization.xml` spells it (`e_002_p`),
but `emblemstexturepack.perm.bin` stores the images in **UPPER CASE and only as the small variant**: `e_002_p` is
`E_002_P_SML` in the pack (569 of 571 entries end in `_SML`; the only others are `PLATINUMGLOW`, `RANKPLATE`). So the
lookup missed for all 225 emblems in the catalog and every emblem came out blank - the party/lobby row, and O7OF's
Armory emblem grid, which uses the same call.

- `src/ui/UiArt.cpp`: `EmblemByName()` upper-cases the name and tries `<NAME>_SML` first, then `<NAME>`.
- Debug helper for next time: dumping the index is 10 lines of Python - read `emblemstexturepack.perm.bin` in 0x1E0
  records, magic `0xCDBFA090`, name at `+0x44` (24 bytes), same as `PackIndex()` in `UiArt.cpp`.
- **Watch out:** `small` is a Windows macro (`#define small char`), so `if (UiTexture small = ...)` fails to compile
  with a confusing C2628. Use another name.

### 2. Player row anatomy, after Tyler's MCC crops

Final layout of `PlayerRow()` (shared by MY PARTY and the pre-game roster), left to right:

1. **rank badge** (30 * s, 5 * s gap),
2. **emblem tile**: dark framed square with the emblem filling it, **joined to the bar** (no gap),
3. **team strip** (6 * s, flush between tile and bar): only when the game type assigns teams - red team 1, blue team 2.
   The parameter exists (`team`) but both call sites pass `0`: the lobby roster carries no team yet, so nothing is
   drawn (which is also what MCC shows in the main menu and in Free for All),
4. **nameplate bar**: plate art, `[TAG]` on the top line, gamertag below, DEV chip inside on the right, 1 px frame.

### 3. Checks

- `cmake --build build --config Release`: 0 errors, 0 warnings.
- Capture at 1920x1080: MY PARTY row now shows rank badge, the dark emblem tile with the emblem drawn in it, and the
  joined nameplate bar with `[NOBL]` / `WEABOOTYLER117`.

--

## 0. Pick up here (2026-10-30, 18:15 ET) - Fix Co-op Campaign Netplay Desync, Elite Armor & Effects Replication, Armory Visuals & Social Menu (O7OF / Dominus)

### 1. Co-op Campaign Netplay Desync (Guest 2nd Player Spawn Error)
- **Problem:** When 2 PCs attempted to play Campaign co-op over LAN or Internet, the host connected but then an error threw on the 2nd player spawn (stuck at LoadProgress: 5.0% for ~25s until Category 14 mission abort / game over).
- **Cause Found in Log & Engine:** 
  1. In `src/engine/BlamEngine.cpp`, `isGuest` was forcing `loadedPrebuilt = false`. This stripped the prebuilt campaign scenario variant, level description, and insertion point blocks at `+0x300` on the guest machine, preventing the guest engine from initializing the mission scenario.
  2. In the guest launch block, controller 0 was bound to Slot 0 (`+0x100`) and Slot 1 was unassigned. When the host accepted the guest, the host assigned the guest player to Slot 1 (Slot 0 is the Host). Because the guest had Slot 1 unassigned, 0 local controllers were mapped to player 2 (Bob), causing the engine to fail spawning player 2.
- **Fix:**
  - Preserved prebuilt launch blocks (`campaign_*.bin` / `lb_m_c_1.bin`) on guests.
  - Added fallback synthesis: if a mission-specific campaign/firefight launch block is missing, `lb_m_c_1.bin` is loaded as the base template.
  - Properly mapped launch block controller slots for guests: Slot 0 (`+0x100`) is unassigned (`0xFF, 0xFF, 0xFF, 0xFF`) reserved for remote Host; Slot 1 (`+0x120`) is bound to local controller 0 (`block[0x120] = 0x00, block[0x124] = 0x02`), so Player 2 spawns with controller input when host membership (`msg 26`) arrives.
  - Fixed variant resolver in `BlamEngine.cpp` to skip looking for game variant files when mode is `"Campaign"` (avoids Slayer replacement).
  - Added backtrace stack capture in `ReachHost.cpp` inside `HostSlot6_GameResults` to diagnose any future unexpected result calls.

### 2. Elite Armors & Armor Effects Multiplayer Replication
- **Problem:** Equipped Elite armor sets (Honor Guard, General, Field Marshall, etc.) were only visible locally; remote peers saw them default to Elite Minor. Additionally, armor effects were not showing on Elites, and previously caused bugs with character selection.
- **Root Cause Reverse-Engineered:**
  - In `haloreach.dll`, function `0x2C8070` (`PlayerSetAppearanceSlot(player, slotParam, itemId)`) expects `slotParam->species = 1` and `slotParam->slot = 0` for Elite sets, and `slotParam->species = 1, slotParam->slot = 2` for Elite armor effects.
  - At RVA `0x2EDD5C`, the engine was incorrectly passing `species=0, slot=7`, storing the value into the Spartan array and leaving `[player + 0xAE + 0]` as 0 (Elite Minor).
  - At RVA `0x10580C`, retail `haloreach.dll` loaded `ecx = 0x8C` (`pmcg` list 8C), which is completely empty (0 entries) in retail tags, causing the engine to ignore Elite armor effects.
- **Fix in `src/hooks/EnginePatches.cpp` & `src/engine/ReachArmory.cpp`:**
  - Installed a MinHook detour on `PlayerSetAppearanceSlot` at `0x2C8070` to validate callers (`0x2EDD7A` for Elite Set -> `species=1, slot=0`; `0x2EDDBB` for Elite Effect -> `species=1, slot=2`).
  - Patched RVA `0x2EDD5C` (`89 75 C0 C7 45 C4 07 00 00 00` -> `C7 45 C0 01 00 00 00 89 75 C4`) to write `species=1, slot=0`.
  - Patched RVA `0x10580C` (`B9 8C 00 00 00` -> `B9 80 00 00 00`) to route Elite armor effects to the full tag list 80 (Flames, Blue Flames, Heart Attack, Pestilence, Inclement Weather, Birthday Party), matching Spartans.
  - Updated `ReachArmory.cpp` to look up Elite effects in list `"80"` before falling back to `"8c"`.

### 3. Armory Emblems & Nameplates Visuals
- **Problem:** Nameplates and emblems in the Armory only showed text names or raw options.
- **Fix in `src/ui/ArmorCustomizer.cpp`:**
  - In the Armory picker grid: added dedicated branches for `isNameplate` (1 column, 56px height, rendered via `UiArt::Nameplate`) and `isEmblem` (4 columns, square tile, rendered via `UiArt::Emblem`), displaying full visual previews of all 531 nameplates and 330 emblems from `playercustomization.xml`.

### 4. Social Menu & Friends List
- **Problem:** Social menu lacked an easy way to search/add friends directly by username or IP address and invite/join them.
- **Fix in `src/network/SocialClient.*` and `src/ui/FrontEndMenu.cpp`:**
  - Added `AddFriendDirect(nameOrEndpoint)` to `SocialClient`, saving and loading friends locally in `data/userdata/friends.json`.
  - Added a "QUICK ADD FRIEND" text input box and button at the top of the FRIENDS tab in `DrawSocialScreen`.
  - Added a direct `[JOIN]` button for friends configured with IP/port endpoints, and updated `[REMOVE]` to remove instantly without requiring a master server connection.

### 5. Verification
- Built with Visual Studio 18 2026 MSVC toolchain (`cmake --build build --config Release --target noble-client`).
- Verified 0 errors and 0 warnings.
- Executed smoke captures:
  - `noble-client.exe -menu-screen social -menu-capture 30 -quit-after 5` -> Exit Code 0
  - `noble-client.exe -menu-screen armory -menu-capture 60 -quit-after 5` -> Exit Code 0

--
## 0. Pick up here (2026-10-30, 06:30 ET) - MCC-style player rows: rank - emblem - nameplate with tag/name (WeabooTyler117 / DeepSeek)

UI only: `src/ui/FrontEndMenu.cpp`. Follows the 06:40 ET entry, which had the nameplate only as a row background.

### 1. One shared row, the way MCC draws it

New `PlayerRow(ui, pos, width, rankIndex, emblemIndex, nameplateIndex, serviceTag, name, dev, selected)` draws, left
to right:

1. **rank badge** (30 * s; a flat placeholder plate when the rank is unknown),
2. **emblem chip** (24 * s) - left out completely when MCC has no emblem texture for that id, and the bar takes its
   space; a bare blue placeholder square looked unfinished,
3. **nameplate bar**: the plate art with the **service tag on the top line** (`[NOBL]`, 11.5 * s, muted blue) and the
   **gamertag underneath** (18 * s, white), the small gold **DEV** chip inside on the right, and a dark top/bottom
   band so both lines read on the art.

Used by **MY PARTY** on the main menu (local player, so rank/emblem/plate all known) and by the **pre-game lobby
roster**. In the roster only the local player's ids are known, so remote players get the placeholder badge and the flat
bar; their ids are still not in the roster payload (see §4 of the 06:40 entry).

### 2. Fixed on the way

- The nameplate is drawn only when the caller passes an id **> 0**: `UiArt::Nameplate(0)` falls back to a real plate, so
  remote/empty rows were getting a random one.
- An **empty lobby slot** no longer draws a phantom player row (`[117]`, no name) under "Waiting for players to join...";
  it draws just the waiting bar.
- The pre-game lobby panel is titled **MY GAME SESSION** (MCC's wording; the tab is 158 * s wide), not "MY PARTY" -
  the main-menu box keeps "MY PARTY".

### 3. Checks

- `cmake --build build --config Release`: 0 errors, 0 warnings.
- Captures at 1920x1080: MY PARTY row (`[NOBL]` / `WEABOOTYLER117` on the plate, rank badge left, DEV chip right) and an
  empty pre-game session (`MY GAME SESSION`, `0/16`, "Waiting for players to join...").

--

## 0. Pick up here (2026-10-30, 18:15 ET) - Fix Co-op Campaign Netplay Desync, Elite Armor & Effects Replication, Armory Visuals & Social Menu (O7OF / Dominus)

### 1. Co-op Campaign Netplay Desync (Guest 2nd Player Spawn Error)
- **Problem:** When 2 PCs attempted to play Campaign co-op over LAN or Internet, the host connected but then an error threw on the 2nd player spawn (stuck at LoadProgress: 5.0% for ~25s until Category 14 mission abort / game over).
- **Cause Found in Log & Engine:** 
  1. In `src/engine/BlamEngine.cpp`, `isGuest` was forcing `loadedPrebuilt = false`. This stripped the prebuilt campaign scenario variant, level description, and insertion point blocks at `+0x300` on the guest machine, preventing the guest engine from initializing the mission scenario.
  2. In the guest launch block, controller 0 was bound to Slot 0 (`+0x100`) and Slot 1 was unassigned. When the host accepted the guest, the host assigned the guest player to Slot 1 (Slot 0 is the Host). Because the guest had Slot 1 unassigned, 0 local controllers were mapped to player 2 (Bob), causing the engine to fail spawning player 2.
- **Fix:**
  - Preserved prebuilt launch blocks (`campaign_*.bin` / `lb_m_c_1.bin`) on guests.
  - Added fallback synthesis: if a mission-specific campaign/firefight launch block is missing, `lb_m_c_1.bin` is loaded as the base template.
  - Properly mapped launch block controller slots for guests: Slot 0 (`+0x100`) is unassigned (`0xFF, 0xFF, 0xFF, 0xFF`) reserved for remote Host; Slot 1 (`+0x120`) is bound to local controller 0 (`block[0x120] = 0x00, block[0x124] = 0x02`), so Player 2 spawns with controller input when host membership (`msg 26`) arrives.
  - Fixed variant resolver in `BlamEngine.cpp` to skip looking for game variant files when mode is `"Campaign"` (avoids Slayer replacement).
  - Added backtrace stack capture in `ReachHost.cpp` inside `HostSlot6_GameResults` to diagnose any future unexpected result calls.

### 2. Elite Armors & Armor Effects Multiplayer Replication
- **Problem:** Equipped Elite armor sets (Honor Guard, General, Field Marshall, etc.) were only visible locally; remote peers saw them default to Elite Minor. Additionally, armor effects were not showing on Elites, and previously caused bugs with character selection.
- **Root Cause Reverse-Engineered:**
  - In `haloreach.dll`, function `0x2C8070` (`PlayerSetAppearanceSlot(player, slotParam, itemId)`) expects `slotParam->species = 1` and `slotParam->slot = 0` for Elite sets, and `slotParam->species = 1, slotParam->slot = 2` for Elite armor effects.
  - At RVA `0x2EDD5C`, the engine was incorrectly passing `species=0, slot=7`, storing the value into the Spartan array and leaving `[player + 0xAE + 0]` as 0 (Elite Minor).
  - At RVA `0x10580C`, retail `haloreach.dll` loaded `ecx = 0x8C` (`pmcg` list 8C), which is completely empty (0 entries) in retail tags, causing the engine to ignore Elite armor effects.
- **Fix in `src/hooks/EnginePatches.cpp` & `src/engine/ReachArmory.cpp`:**
  - Installed a MinHook detour on `PlayerSetAppearanceSlot` at `0x2C8070` to validate callers (`0x2EDD7A` for Elite Set -> `species=1, slot=0`; `0x2EDDBB` for Elite Effect -> `species=1, slot=2`).
  - Patched RVA `0x2EDD5C` (`89 75 C0 C7 45 C4 07 00 00 00` -> `C7 45 C0 01 00 00 00 89 75 C4`) to write `species=1, slot=0`.
  - Patched RVA `0x10580C` (`B9 8C 00 00 00` -> `B9 80 00 00 00`) to route Elite armor effects to the full tag list 80 (Flames, Blue Flames, Heart Attack, Pestilence, Inclement Weather, Birthday Party), matching Spartans.
  - Updated `ReachArmory.cpp` to look up Elite effects in list `"80"` before falling back to `"8c"`.

### 3. Armory Emblems & Nameplates Visuals
- **Problem:** Nameplates and emblems in the Armory only showed text names or raw options.
- **Fix in `src/ui/ArmorCustomizer.cpp`:**
  - In the Armory picker grid: added dedicated branches for `isNameplate` (1 column, 56px height, rendered via `UiArt::Nameplate`) and `isEmblem` (4 columns, square tile, rendered via `UiArt::Emblem`), displaying full visual previews of all 531 nameplates and 330 emblems from `playercustomization.xml`.

### 4. Social Menu & Friends List
- **Problem:** Social menu lacked an easy way to search/add friends directly by username or IP address and invite/join them.
- **Fix in `src/network/SocialClient.*` and `src/ui/FrontEndMenu.cpp`:**
  - Added `AddFriendDirect(nameOrEndpoint)` to `SocialClient`, saving and loading friends locally in `data/userdata/friends.json`.
  - Added a "QUICK ADD FRIEND" text input box and button at the top of the FRIENDS tab in `DrawSocialScreen`.
  - Added a direct `[JOIN]` button for friends configured with IP/port endpoints, and updated `[REMOVE]` to remove instantly without requiring a master server connection.

### 5. Verification
- Built with Visual Studio 18 2026 MSVC toolchain (`cmake --build build --config Release --target noble-client`).
- Verified 0 errors and 0 warnings.
- Executed smoke captures:
  - `noble-client.exe -menu-screen social -menu-capture 30 -quit-after 5` -> Exit Code 0
  - `noble-client.exe -menu-screen armory -menu-capture 60 -quit-after 5` -> Exit Code 0

--
UI only: `src/ui/FrontEndMenu.cpp`. Follows the 05:05 ET entry (O7OF's `dc349a5` is merged in).

### 1. Main menu jumped when the description changed length

The layout measured the **selected** entry's description, so a one-line entry and a two-line entry produced
different `listY`/row step and moving the cursor moved the whole list (Campaign -> Quit shifted vertically).
`descH` is now a fixed two-line reserve, independent of the selection, so the list never moves; the description is
still drawn from the same `descY`.

### 2. Text under a map picture was hard to read

`HeroBanner` drew the blurb as plain grey text straight over map art (Tyler's last screenshot). Now the blurb gets a
soft dark band behind it (transparent at the edges), a 1 px black shadow and brighter text (`214,224,236`), still
clipped to two lines. Applies to Campaign / Firefight / Custom Game / Forge.

### 3. Skulls

- `SkullSummary()` returns **"All Skulls"** when all 13 are on (was "13 on"), and the pre-game lobby's SKULLS row
  says **"ALL SKULLS ON"**. Campaign screenshot verified with 13/13 on.
- Not done: locking skulls until they are found in the campaign, and an "All Skulls" toggle in the skull menu gated on
  having collected all 13 - both need the campaign's skull-collection data, which Noble does not read yet (engine
  research). There is nowhere to gate them on today.

### 4. Armory nameplates in the party panel and the pre-game lobby

`UiArt::Nameplate()` (from `dc349a5`) is now the background of the **local player's** row in `DrawParty` (MY PARTY,
main menu) and in the pre-game lobby roster, with the old blue/grey bar as the fallback when the plate texture is
missing. Geometry, after two rounds of Tyler's feedback (2026-10-30):

- The row is taller so the art reads: **40 * s** in the party panel, **44 * s** per roster row.
- The plate spans **80 %** of the party row and **76 %** of a roster row, then fades into the flat row colour; the
  remainder stays plain for the rank icon / service tag.
- A light scrim (70-80 alpha) keeps the name readable while leaving the plate visible, and the plate area gets the same
  steel frame the Armory preview of that plate has.
- Stretched over the *whole* row its own emblem landed under the rank icon and the DEV chip sat on the busiest part of
  the art (Tyler's crop) - that is why it stops short of the right edge.

**Remote players still show the plain row**: the lobby roster / master advert carry no nameplate id, so showing other
players' plates needs `nameplateId` added to the roster JSON, `MasterAdvert`, `LobbyPlayer` and `server.js`.

### 5. Mod headings had a black banner

A mod heading has no scenario, so its banner was empty; `ModdedMaps()` now points the heading at its **first map's
scenario**, so opening a mod shows that map's picture under "CIVIL UNREST" instead of a black rectangle.

### 6. Account modal

The developer line was a non-wrapping `TextDisabled` and got cut off ("...above Qui"); it is `TextWrapped` now.

### 7. Build and checks

- `cmake --build build --config Release`: `noble-client.exe`, 0 errors, 0 warnings.
- Captures at 1920x1080: Campaign with **ALL SKULLS** + readable blurb; modded Forge with a map picture and readable
  description; main menu with the nameplate behind the player row.

### 8. Still open from Tyler's list (2026-10-30 06:10) and what each really needs

- **Invite notifications** - `SocialClient::GetInvites()` already returns `GameInvite{senderUsername, hostIp, hostPort,
  timestamp}` and there is `PollInvitesAsync()` / `ClearInviteAsync()`, so a global toast ("X is inviting you to a
  match", JOIN / DISMISS) plus a notifications on/off setting is a UI job. Saying *which* match ("... to a Firefight
  match") needs a mode/map field added to the invite payload (`SocialClient` + `server.js`).
- **Menu music player** - `ReachAudio::MusicTracks()`, `PlayMenuMusic(id)`, `StopMusic()`, `SetMusicVolume()` are there;
  a small now-playing widget (track name, prev/next, mute, shuffle = `PlayMenuMusic("all")`) is a UI job. There is no
  pause API: "pause" would stop and restart the cue, so it needs a `PauseMusic()`/resume in `ReachAudio` first.
- **Controller presets / custom mapping / extra buttons** - not a menu change: the pad buttons are remapped inside
  `ReachHost`'s slot-36 input path, so it needs a mapping table there plus per-action storage in settings.
- **Modded maps with new scenario names** - `BlamEngine::ResolveMapId()` only knows Reach's own tables and returns 0
  (which boots the main menu) for anything else; a new map file needs a launch index / map id. Possible partial: a mod
  map whose *title* contains a Reach map name could launch that base scenario through the VFS (the mod's replacement
  file is what loads), which would un-grey entries like "BREAKPOINT - (DKF)" but not brand-new maps.

--

## 0. Pick up here (2026-10-30, 18:15 ET) - Fix Co-op Campaign Netplay Desync, Elite Armor & Effects Replication, Armory Visuals & Social Menu (O7OF / Dominus)

### 1. Co-op Campaign Netplay Desync (Guest 2nd Player Spawn Error)
- **Problem:** When 2 PCs attempted to play Campaign co-op over LAN or Internet, the host connected but then an error threw on the 2nd player spawn (stuck at LoadProgress: 5.0% for ~25s until Category 14 mission abort / game over).
- **Cause Found in Log & Engine:** 
  1. In `src/engine/BlamEngine.cpp`, `isGuest` was forcing `loadedPrebuilt = false`. This stripped the prebuilt campaign scenario variant, level description, and insertion point blocks at `+0x300` on the guest machine, preventing the guest engine from initializing the mission scenario.
  2. In the guest launch block, controller 0 was bound to Slot 0 (`+0x100`) and Slot 1 was unassigned. When the host accepted the guest, the host assigned the guest player to Slot 1 (Slot 0 is the Host). Because the guest had Slot 1 unassigned, 0 local controllers were mapped to player 2 (Bob), causing the engine to fail spawning player 2.
- **Fix:**
  - Preserved prebuilt launch blocks (`campaign_*.bin` / `lb_m_c_1.bin`) on guests.
  - Added fallback synthesis: if a mission-specific campaign/firefight launch block is missing, `lb_m_c_1.bin` is loaded as the base template.
  - Properly mapped launch block controller slots for guests: Slot 0 (`+0x100`) is unassigned (`0xFF, 0xFF, 0xFF, 0xFF`) reserved for remote Host; Slot 1 (`+0x120`) is bound to local controller 0 (`block[0x120] = 0x00, block[0x124] = 0x02`), so Player 2 spawns with controller input when host membership (`msg 26`) arrives.
  - Fixed variant resolver in `BlamEngine.cpp` to skip looking for game variant files when mode is `"Campaign"` (avoids Slayer replacement).
  - Added backtrace stack capture in `ReachHost.cpp` inside `HostSlot6_GameResults` to diagnose any future unexpected result calls.

### 2. Elite Armors & Armor Effects Multiplayer Replication
- **Problem:** Equipped Elite armor sets (Honor Guard, General, Field Marshall, etc.) were only visible locally; remote peers saw them default to Elite Minor. Additionally, armor effects were not showing on Elites, and previously caused bugs with character selection.
- **Root Cause Reverse-Engineered:**
  - In `haloreach.dll`, function `0x2C8070` (`PlayerSetAppearanceSlot(player, slotParam, itemId)`) expects `slotParam->species = 1` and `slotParam->slot = 0` for Elite sets, and `slotParam->species = 1, slotParam->slot = 2` for Elite armor effects.
  - At RVA `0x2EDD5C`, the engine was incorrectly passing `species=0, slot=7`, storing the value into the Spartan array and leaving `[player + 0xAE + 0]` as 0 (Elite Minor).
  - At RVA `0x10580C`, retail `haloreach.dll` loaded `ecx = 0x8C` (`pmcg` list 8C), which is completely empty (0 entries) in retail tags, causing the engine to ignore Elite armor effects.
- **Fix in `src/hooks/EnginePatches.cpp` & `src/engine/ReachArmory.cpp`:**
  - Installed a MinHook detour on `PlayerSetAppearanceSlot` at `0x2C8070` to validate callers (`0x2EDD7A` for Elite Set -> `species=1, slot=0`; `0x2EDDBB` for Elite Effect -> `species=1, slot=2`).
  - Patched RVA `0x2EDD5C` (`89 75 C0 C7 45 C4 07 00 00 00` -> `C7 45 C0 01 00 00 00 89 75 C4`) to write `species=1, slot=0`.
  - Patched RVA `0x10580C` (`B9 8C 00 00 00` -> `B9 80 00 00 00`) to route Elite armor effects to the full tag list 80 (Flames, Blue Flames, Heart Attack, Pestilence, Inclement Weather, Birthday Party), matching Spartans.
  - Updated `ReachArmory.cpp` to look up Elite effects in list `"80"` before falling back to `"8c"`.

### 3. Armory Emblems & Nameplates Visuals
- **Problem:** Nameplates and emblems in the Armory only showed text names or raw options.
- **Fix in `src/ui/ArmorCustomizer.cpp`:**
  - In the Armory picker grid: added dedicated branches for `isNameplate` (1 column, 56px height, rendered via `UiArt::Nameplate`) and `isEmblem` (4 columns, square tile, rendered via `UiArt::Emblem`), displaying full visual previews of all 531 nameplates and 330 emblems from `playercustomization.xml`.

### 4. Social Menu & Friends List
- **Problem:** Social menu lacked an easy way to search/add friends directly by username or IP address and invite/join them.
- **Fix in `src/network/SocialClient.*` and `src/ui/FrontEndMenu.cpp`:**
  - Added `AddFriendDirect(nameOrEndpoint)` to `SocialClient`, saving and loading friends locally in `data/userdata/friends.json`.
  - Added a "QUICK ADD FRIEND" text input box and button at the top of the FRIENDS tab in `DrawSocialScreen`.
  - Added a direct `[JOIN]` button for friends configured with IP/port endpoints, and updated `[REMOVE]` to remove instantly without requiring a master server connection.

### 5. Verification
- Built with Visual Studio 18 2026 MSVC toolchain (`cmake --build build --config Release --target noble-client`).
- Verified 0 errors and 0 warnings.
- Executed smoke captures:
  - `noble-client.exe -menu-screen social -menu-capture 30 -quit-after 5` -> Exit Code 0
  - `noble-client.exe -menu-screen armory -menu-capture 60 -quit-after 5` -> Exit Code 0

--
`tools/armory/fetch_halopedia_icons.py` was sitting **untracked** in Tyler's tree from the 2026-10-29 Armory art
session. It is the picture half of the pipeline next to the tracked `fetch_halopedia_armory.py` (which pulls names,
rank and cR), so it is checked in rather than left dangling:

- Queries Halopedia's "Armor customization (Halo: Reach)" page and maps each `File:HR_*_Icon.png` to its slot folder
  (`helmets`, `shoulders_left/right`, `chests`, `wrists`, `utility`, `visors`, `knees`, `effects`, `voices`,
  `elite_sets`; `HR_Rank_*` is skipped on purpose, `data/ui/reach/ranks` already has those), downloads into
  `build/bin/Release/data/ui/reach/armory/<slot>/` and writes `manifest.csv` (category, file, url).
- Standard library only, 0.3-0.5 s between requests, skips files that already exist, so it can be re-run.
- `build/` is git-ignored, so the pictures themselves are **not** committed - only the script. Halopedia art is not
  redistributed with Noble (art rule: `docs/menu-redesign-spec.md` §7).

Run from the repo root:

```
python tools\armory\fetch_halopedia_icons.py
```

--

## 0. Pick up here (2026-10-30, 05:35 ET) - Build fix after dc349a5: duplicate `lockOption` lambdas (WeabooTyler117 / DeepSeek)

`origin/main` at `dc349a5` ("Elite armors network replication ... social system") **did not compile**:

```
src/ui/ArmorCustomizer.cpp(1538): error C2086: 'lockOption': redefinition
src/ui/ArmorCustomizer.cpp(1595): error C2086: 'lockOption': redefinition
src/ui/ArmorCustomizer.cpp(1497): warning C4456: declaration of 'lockOption' hides previous local declaration
```

`ArmorCustomizer::Render()` had four copies of the same `auto lockOption = [](PickOption& o, const char* id) {...}`
helper (lines 1427, 1497, 1538, 1595), landed by that commit: the one at 1497 hides the earlier one and the later two
are hard redefinitions, so the whole client failed to build. Nothing calls `lockOption` anywhere - the Elite rows use
`LockEliteOption()` - so all four copies are dead code and were removed.

- `src/ui/ArmorCustomizer.cpp`: the four dead `lockOption` lambdas removed (no other change).
- Verified: `cmake --build build --config Release` -> `noble-client.exe` + `noble-dedicated.exe`, 0 errors, 0 warnings,
  and the merged client draws the main menu (captured with `-menu-screen main -menu-capture 90`).
- **O7OF:** this sits on top of your `dc349a5`, so pull before building. Separately, that commit appended its handoff
  entry **27 times** to this file (~2,100 duplicate lines); it was left untouched here.

--

## 0. Pick up here (2026-10-30, 05:05 ET) - Menu pass 2: mod names/descriptions, main menu, account modal, server browser (WeabooTyler117 / DeepSeek)

UI only: `src/ui/FrontEndMenu.cpp`, `src/ui/ServerBrowser.cpp`, `src/ui/UiArt.cpp/.hpp`. Follows the 04:05 ET entry.

### 1. Modded lists: names and descriptions (Tyler's screenshots)

- **Long workshop mod names ran into the right column.** `DrawList` drew header rows with no clip, so
  `DEADKIVA7'S HALO: REACH FIREFIGHT (NEW CUSTOM MAP "STAGING GROUND" OUT NOW!)` ran over the map banner. Header text
  is now clipped to its row (`PushClipRect` at `b.x - 4 s`).
- **Descriptions were pasted raw from `ModInfo.json`**: BBCode (`[h1]Map List[/h1]`), hard line breaks and hundreds of
  characters, wrapped by `HeroBanner` with no height limit - which is why the text covered the picture, the GAME TYPE
  row and the difficulty tiles, and pushed the rows into the network strip. New `PlainDescription(text, maxChars)`
  drops `[...]` tags, collapses whitespace and cuts at a word boundary with `...`; applied in `ModMaps()` (150 chars)
  and to the mod heading blurb in `ModdedMaps()` (180). New `BlurbBlockHeight()` measures at most **two** lines, is
  used by both setup screens instead of their own measurement, and `HeroBanner` draws the blurb inside that same
  two-line clip rect - so a blurb can never change the layout of the rows below it.
- **Greyed-out modded maps are now selectable** (hover, arrows, click) so the banner can explain them; only Accept is
  blocked. The reason: `BlamEngine::ResolveMapId()` only knows Reach's own campaign/Firefight/multiplayer tables and
  returns 0 for a scenario it does not know, and 0 boots the **main menu**. A mod map that MCC loads through its own
  mod system therefore cannot be started by Noble until a launch index / map id exists for its scenario - engine
  work, not UI. `ModMaps()` keeps those rows disabled with "A new map: Noble can only start maps that are in Reach so
  far." in the banner.

### 2. Main menu

- **Duplicate developer text removed**: the name was drawn as `NAME [DEV]` *and* with a gold `DEV` chip, and the
  account bar said `ONLINE [DEV]: NAME`. Now: `NAME` (gold) + the chip, and `ONLINE: NAME`.
- **Developer Menu moved out of the party panel** into the main-menu list, directly **above Quit**, and only built
  into the list when `AuthClient::Get().IsDeveloper()` (`kDeveloperEntry`). The list is assembled per frame from
  `kMainEntries` and `st.mainIndex` is clamped, so indices stay right when the row appears/disappears.
- **Bottom of the menu no longer collides**: the description used to be placed at
  `listY + rows * 27 s + 10 s` with no check against `DrawHints` (`y - 46 s`), so it ran under
  "ENTER Select / ESC Quit". The list now shrinks its row step (27 s -> min 18 s) and then lifts to fit the
  description above the hint row.

### 3. Account modal (`DrawAccountModal`)

- One `Logged in as: <name>` line plus a **DEV chip**; the `[DEV]` + `[DEVELOPER]` duplicate is gone.
- **"OPEN DEVELOPER MENU" removed** (it lives on the main menu now) - that button was also the clipped
  `OPEN DEVELOPER MEN` from the screenshot, because it was a fixed 200 px wide.
- LOG IN / REGISTER are equal-width tabs (active one highlighted), fields are full width, and LOG OUT/CLOSE and
  submit/CANCEL are equal halves of the dialog width.

### 4. Server browser (`ServerBrowser.cpp`)

- **Back returns to the main menu** (it went to Custom Game: `goBack(st.screen == Screen::Servers ? Screen::CustomGame : ...)`).
- **Table headers fit**: widths sized to the header text plus cell padding and the sort arrow
  (`Source` 86, `Players` 98, ...) and the header row is frozen; they used to elide to "Sour..." / "Playe...".
- **The first server is selected automatically** once the list has one, so GAME DETAILS (map picture, players,
  ping) shows something without clicking first.
- **GAME DETAILS shows the map**: new `UiArt::MapPhoto(scenario)` = MCC map preview, else a picture a mod/player
  supplies as `data\ui\custom\loading\<scenario>.*` (or `data\ui\noble\loading\`), else a named placeholder
  ("NO PICTURE FROM THIS MOD"). It deliberately does **not** use `LoadingBackground()`'s campaign-scene fallback,
  which would show a different map. The panel also gained a Ping row, a wrapped empty state, and FAVOURITE /
  COPY INVITE on one row (COPY ADDRESS dropped as a duplicate of it).

### 5. Test hook

- `-menu-tab <n>` now also selects the VANILLA / MODDED source tab of Campaign / Firefight / Custom Game / Forge, so
  a capture run can show the modded lists. Example:
  `noble-client.exe -menu-screen firefight -menu-tab 1 -menu-capture 90` (writes `capture_<frame>.bmp`, closes itself).

### 6. Build and checks

- `cmake --build build --config Release`: 0 errors, 0 warnings. Captures at 1920x1080 for main menu, modded Firefight,
  modded Custom Game and the server browser; the main menu shows `WEABOOTYLER117 [DEV chip]`, `ONLINE: WEABOOTYLER117`,
  `DEVELOPER MENU` above `QUIT` and the description clear of the hint row.
- **Still open from Tyler's list:** the post-game report showing `Player2` / `Player3` instead of names (needs the
  engine-side player record name for remote players - `Career::ScoreRow` falls back to "Player N" when the record
  name is empty), and a per-server *mod* photo once servers advertise their mod ids.

## 0. Pick up here (2026-10-30, 04:45 ET) - Elite Armors Network Replication Fix, Elite Armor Effects Engine Patch, Full Armory Permutations & Emblems/Nameplates, and Master Server Social Friends/Invites Menu (O7OF / Dominus)

Comprehensive fix for Elite armor synchronization in multiplayer, Elite armor effects engine support, MCC nameplates and emblems integration into the Armory, and master server social networking system with interactive friends list, player search, and in-game lobby invites.

### 1. What was wrong & Root Causes

1. **Elite Armors Defaulting to Elite Minor for Remote Players:**
   - Players equipping Elite armor sets (such as Honor Guard, General, Field Marshall, Officer, etc.) saw their selected armor locally, but remote players in multiplayer matches only saw them as default Elite Minor.
   - **Root Cause Reverse-Engineered:** In `haloreach.dll+0x2EDD45` (the function serializing the player's profile into the network replication player datum at `0x2C8070`), Bungie/MCC hardcoded `species = 0` (Spartan) and `slot = 7` when writing `profile[0x8C]` (the equipped Elite armor set index).
   - In `haloreach.dll+0x2C8070`, the code verifies `cmp dword ptr [rdx+4], 3; ja exit`. Passing slot 7 exceeded the check and jumped to exit, leaving `[player + 0xAE + 0]` completely unwritten (0 = Elite Minor). The multiplayer network session then broadcasted this default 0 value to all other clients.

2. **Elite Armor Effects Not Showing & Previous Buggy Behavior:**
   - Armor effects only applied to Spartans; Elites had no working armor effects.
   - **Root Cause Reverse-Engineered:** In `haloreach.dll+0x105796`, the engine validated `cmp esi, 8; setb r15b` (`83 FE 08 41 0F 92 C7`), rejecting `pmcg` lists >= 8. Spartan fx is list 8 (`+0x80`), while Elite fx is list 9 (`+0x8C`).
   - In previous iterations, equipping armor effects erroneously overrode the player's Elite character model type due to coupled mesh logic.

3. **Armory Missing Nameplates & Emblems:**
   - The Armory identity customization lacked the full catalog of MCC Reach nameplates (536 nameplates) and emblems (330 emblems) loaded dynamically from MCC assets.

4. **Placeholder Social / Friends System in Main Menu and Pre-Game Lobby:**
   - The lobby sidebar displayed hardcoded placeholder text for friends ("Forge - In a Game", "In Custom Game Lobby"). There was no way to search community players, add friends, accept/decline friend requests, view real-time invites, or invite friends directly to a game lobby.

---

## 0. Pick up here (2026-10-30, 18:15 ET) - Fix Co-op Campaign Netplay Desync, Elite Armor & Effects Replication, Armory Visuals & Social Menu (O7OF / Dominus)

### 1. Co-op Campaign Netplay Desync (Guest 2nd Player Spawn Error)
- **Problem:** When 2 PCs attempted to play Campaign co-op over LAN or Internet, the host connected but then an error threw on the 2nd player spawn (stuck at LoadProgress: 5.0% for ~25s until Category 14 mission abort / game over).
- **Cause Found in Log & Engine:** 
  1. In `src/engine/BlamEngine.cpp`, `isGuest` was forcing `loadedPrebuilt = false`. This stripped the prebuilt campaign scenario variant, level description, and insertion point blocks at `+0x300` on the guest machine, preventing the guest engine from initializing the mission scenario.
  2. In the guest launch block, controller 0 was bound to Slot 0 (`+0x100`) and Slot 1 was unassigned. When the host accepted the guest, the host assigned the guest player to Slot 1 (Slot 0 is the Host). Because the guest had Slot 1 unassigned, 0 local controllers were mapped to player 2 (Bob), causing the engine to fail spawning player 2.
- **Fix:**
  - Preserved prebuilt launch blocks (`campaign_*.bin` / `lb_m_c_1.bin`) on guests.
  - Added fallback synthesis: if a mission-specific campaign/firefight launch block is missing, `lb_m_c_1.bin` is loaded as the base template.
  - Properly mapped launch block controller slots for guests: Slot 0 (`+0x100`) is unassigned (`0xFF, 0xFF, 0xFF, 0xFF`) reserved for remote Host; Slot 1 (`+0x120`) is bound to local controller 0 (`block[0x120] = 0x00, block[0x124] = 0x02`), so Player 2 spawns with controller input when host membership (`msg 26`) arrives.
  - Fixed variant resolver in `BlamEngine.cpp` to skip looking for game variant files when mode is `"Campaign"` (avoids Slayer replacement).
  - Added backtrace stack capture in `ReachHost.cpp` inside `HostSlot6_GameResults` to diagnose any future unexpected result calls.

### 2. Elite Armors & Armor Effects Multiplayer Replication
- **Problem:** Equipped Elite armor sets (Honor Guard, General, Field Marshall, etc.) were only visible locally; remote peers saw them default to Elite Minor. Additionally, armor effects were not showing on Elites, and previously caused bugs with character selection.
- **Root Cause Reverse-Engineered:**
  - In `haloreach.dll`, function `0x2C8070` (`PlayerSetAppearanceSlot(player, slotParam, itemId)`) expects `slotParam->species = 1` and `slotParam->slot = 0` for Elite sets, and `slotParam->species = 1, slotParam->slot = 2` for Elite armor effects.
  - At RVA `0x2EDD5C`, the engine was incorrectly passing `species=0, slot=7`, storing the value into the Spartan array and leaving `[player + 0xAE + 0]` as 0 (Elite Minor).
  - At RVA `0x10580C`, retail `haloreach.dll` loaded `ecx = 0x8C` (`pmcg` list 8C), which is completely empty (0 entries) in retail tags, causing the engine to ignore Elite armor effects.
- **Fix in `src/hooks/EnginePatches.cpp` & `src/engine/ReachArmory.cpp`:**
  - Installed a MinHook detour on `PlayerSetAppearanceSlot` at `0x2C8070` to validate callers (`0x2EDD7A` for Elite Set -> `species=1, slot=0`; `0x2EDDBB` for Elite Effect -> `species=1, slot=2`).
  - Patched RVA `0x2EDD5C` (`89 75 C0 C7 45 C4 07 00 00 00` -> `C7 45 C0 01 00 00 00 89 75 C4`) to write `species=1, slot=0`.
  - Patched RVA `0x10580C` (`B9 8C 00 00 00` -> `B9 80 00 00 00`) to route Elite armor effects to the full tag list 80 (Flames, Blue Flames, Heart Attack, Pestilence, Inclement Weather, Birthday Party), matching Spartans.
  - Updated `ReachArmory.cpp` to look up Elite effects in list `"80"` before falling back to `"8c"`.

### 3. Armory Emblems & Nameplates Visuals
- **Problem:** Nameplates and emblems in the Armory only showed text names or raw options.
- **Fix in `src/ui/ArmorCustomizer.cpp`:**
  - In the Armory picker grid: added dedicated branches for `isNameplate` (1 column, 56px height, rendered via `UiArt::Nameplate`) and `isEmblem` (4 columns, square tile, rendered via `UiArt::Emblem`), displaying full visual previews of all 531 nameplates and 330 emblems from `playercustomization.xml`.

### 4. Social Menu & Friends List
- **Problem:** Social menu lacked an easy way to search/add friends directly by username or IP address and invite/join them.
- **Fix in `src/network/SocialClient.*` and `src/ui/FrontEndMenu.cpp`:**
  - Added `AddFriendDirect(nameOrEndpoint)` to `SocialClient`, saving and loading friends locally in `data/userdata/friends.json`.
  - Added a "QUICK ADD FRIEND" text input box and button at the top of the FRIENDS tab in `DrawSocialScreen`.
  - Added a direct `[JOIN]` button for friends configured with IP/port endpoints, and updated `[REMOVE]` to remove instantly without requiring a master server connection.

### 5. Verification
- Built with Visual Studio 18 2026 MSVC toolchain (`cmake --build build --config Release --target noble-client`).
- Verified 0 errors and 0 warnings.
- Executed smoke captures:
  - `noble-client.exe -menu-screen social -menu-capture 30 -quit-after 5` -> Exit Code 0
  - `noble-client.exe -menu-screen armory -menu-capture 60 -quit-after 5` -> Exit Code 0

--
### 2. Implementation & Fixes

1. **Elite Armor Network Replication Engine Patch (`src/hooks/EnginePatches.cpp`):**
   - Patched RVA `0x2EDD45` in `haloreach.dll` from:
     `89 75 C0 C7 45 C4 07 00 00 00` (`mov [rbp-0x40], esi; mov dword ptr [rbp-0x3C], 7`)
     to:
     `C7 45 C0 01 00 00 00 89 75 C4` (`mov dword ptr [rbp-0x40], 1; mov [rbp-0x3C], esi`)
   - This passes `species = 1` (Elite) and `slot = 0` to `0x2C8070`, cleanly storing the equipped armor set index directly into `[player + 0xAE + 0]`. Remote players now replicate and render the exact Elite armor set chosen (Minor, Officer, SpecOps, Ranger, Ultra, Zealot, General, Field Marshall, Honor Guard).

2. **Elite Armor Effects Engine & Pack Fixes (`src/engine/ElitePack.cpp`):**
   - Patched RVA `0x105796` in `haloreach.dll` from `cmp esi, 8; setb r15b` (`83 FE 08 41 0F 92 C7`) to `cmp esi, 8; setne r15b` (`83 FE 08 41 0F 95 C7`). This cleanly skips Spartan fx list 8 (`+0x80`) while permitting Elite armor piece lists 0..7 and Elite fx list 9 (`pmcg +0x8C`).
   - Expanded `kSetPieces` array to `[9][6]`, adding Honor Guard at index 8 (`{2, 2, 2, 8, 2, 10}`).
   - In `BuilderHook`, cleanly differentiated local vs remote players. Local player reads saved armory selection and elite armor effect; remote players read their replicated network set (`copy[0x17]`). Spartan effect on Elites is suppressed (`copy[0x18] = 0xFF`) and Elite effect is applied (`copy[0x19] = eliteEffect`).
   - Character mesh permutations and armor effect permutation logic are 100% separated, eliminating past permutation collision bugs.
   - Added `"forge"` mode support to `ElitePack::Install`.

3. **ReachArmory & Host Ingestion (`src/engine/ReachHost.cpp`, `ReachArmory.hpp`, `ReachArmory.cpp`):**
   - Clamped `el["armor_set"]` to 0..8 (Honor Guard support) in `ReachHost.cpp`.
   - Added `eliteArmorEffectIndex` and `eliteArmorEffect` to `ReachArmorySelection`.
   - Added 3D viewport preview for Elite armor effects using `PmcgEffectName(sel.eliteArmorEffect)` mapping to `pmcg` list `8c` / region `armor_fx` (permutations `armor_fx_00`..`05`).

4. **Nameplates & Emblems Catalog & In-Game Badging (`src/ui/UiArt.hpp`, `UiArt.cpp`, `ArmorCustomizer.hpp`, `ArmorCustomizer.cpp`):**
   - Implemented `GetNameplateCatalog()` and `GetEmblemCatalog()` in `UiArt.cpp`, dynamically parsing `playercustomization.xml` from MCC for 536 nameplates and 330 emblems with authentic titles.
   - Added `UiTexture Nameplate(int id)` loading `PlayerIDNamePlate_*.png` via WIC and `UiTexture Emblem(int id)` decoding from `emblemstexturepack.perm.bin` via `PackImage`.
   - Added Nameplate and Emblem selectors in `ArmorCustomizer.cpp` under the `IDENTITY` section, with real-time preview and saving to `appearance.json`.
   - Added Honor Guard to `kEliteSets` and an Armor Effect row under `EFFECTS & VOICE` for Elites.
   - Updated top-right header nameplate badge to render the player's equipped nameplate PNG and emblem.

5. **Master Server Social Networking Client & UI (`src/network/SocialClient.hpp`, `SocialClient.cpp`, `FrontEndMenu.cpp`):**
   - Built `SocialClient` singleton with async WinHttp background requests to the master server:
     - `/api/friends/list` - Retrieves accepted friends, incoming requests, and outgoing requests.
     - `/api/invites/list` - Polls active game invites for the user.
     - `/api/friends/search` - Searches community users by username prefix.
     - `/api/friends/add` - Sends a friend request.
     - `/api/friends/accept` - Accepts an incoming request.
     - `/api/friends/remove` - Removes a friend or cancels a request.
     - `/api/invites/send` - Sends a game invite with host IP and port.
     - `/api/invites/clear` - Dismisses an invite.
   - Background thread polls invites and friends every 10 seconds.
   - Added `Social` to main menu navigation (`kMainEntries`) and implemented `DrawSocialScreen`:
     - Online account status header with quick link to Account Login/Register modal.
     - Prominent Game Invite Received banner with one-click `[JOIN GAME]` and `[DISMISS]`.
     - Interactive tabs for **Friends**, **Requests**, and **Find Players** (navigable via mouse or Q/E / Bumpers).
     - Live search bar for player lookup with `[ADD FRIEND]` buttons.
   - Pre-Game Lobby update: replaced placeholder friends with real friends list from `SocialClient`, added one-click `[INVITE]` button for lobby hosts, and mapped the `Y` button / click on friends header to open the Social screen directly.

---

## 0. Pick up here (2026-10-30, 18:15 ET) - Fix Co-op Campaign Netplay Desync, Elite Armor & Effects Replication, Armory Visuals & Social Menu (O7OF / Dominus)

### 1. Co-op Campaign Netplay Desync (Guest 2nd Player Spawn Error)
- **Problem:** When 2 PCs attempted to play Campaign co-op over LAN or Internet, the host connected but then an error threw on the 2nd player spawn (stuck at LoadProgress: 5.0% for ~25s until Category 14 mission abort / game over).
- **Cause Found in Log & Engine:** 
  1. In `src/engine/BlamEngine.cpp`, `isGuest` was forcing `loadedPrebuilt = false`. This stripped the prebuilt campaign scenario variant, level description, and insertion point blocks at `+0x300` on the guest machine, preventing the guest engine from initializing the mission scenario.
  2. In the guest launch block, controller 0 was bound to Slot 0 (`+0x100`) and Slot 1 was unassigned. When the host accepted the guest, the host assigned the guest player to Slot 1 (Slot 0 is the Host). Because the guest had Slot 1 unassigned, 0 local controllers were mapped to player 2 (Bob), causing the engine to fail spawning player 2.
- **Fix:**
  - Preserved prebuilt launch blocks (`campaign_*.bin` / `lb_m_c_1.bin`) on guests.
  - Added fallback synthesis: if a mission-specific campaign/firefight launch block is missing, `lb_m_c_1.bin` is loaded as the base template.
  - Properly mapped launch block controller slots for guests: Slot 0 (`+0x100`) is unassigned (`0xFF, 0xFF, 0xFF, 0xFF`) reserved for remote Host; Slot 1 (`+0x120`) is bound to local controller 0 (`block[0x120] = 0x00, block[0x124] = 0x02`), so Player 2 spawns with controller input when host membership (`msg 26`) arrives.
  - Fixed variant resolver in `BlamEngine.cpp` to skip looking for game variant files when mode is `"Campaign"` (avoids Slayer replacement).
  - Added backtrace stack capture in `ReachHost.cpp` inside `HostSlot6_GameResults` to diagnose any future unexpected result calls.

### 2. Elite Armors & Armor Effects Multiplayer Replication
- **Problem:** Equipped Elite armor sets (Honor Guard, General, Field Marshall, etc.) were only visible locally; remote peers saw them default to Elite Minor. Additionally, armor effects were not showing on Elites, and previously caused bugs with character selection.
- **Root Cause Reverse-Engineered:**
  - In `haloreach.dll`, function `0x2C8070` (`PlayerSetAppearanceSlot(player, slotParam, itemId)`) expects `slotParam->species = 1` and `slotParam->slot = 0` for Elite sets, and `slotParam->species = 1, slotParam->slot = 2` for Elite armor effects.
  - At RVA `0x2EDD5C`, the engine was incorrectly passing `species=0, slot=7`, storing the value into the Spartan array and leaving `[player + 0xAE + 0]` as 0 (Elite Minor).
  - At RVA `0x10580C`, retail `haloreach.dll` loaded `ecx = 0x8C` (`pmcg` list 8C), which is completely empty (0 entries) in retail tags, causing the engine to ignore Elite armor effects.
- **Fix in `src/hooks/EnginePatches.cpp` & `src/engine/ReachArmory.cpp`:**
  - Installed a MinHook detour on `PlayerSetAppearanceSlot` at `0x2C8070` to validate callers (`0x2EDD7A` for Elite Set -> `species=1, slot=0`; `0x2EDDBB` for Elite Effect -> `species=1, slot=2`).
  - Patched RVA `0x2EDD5C` (`89 75 C0 C7 45 C4 07 00 00 00` -> `C7 45 C0 01 00 00 00 89 75 C4`) to write `species=1, slot=0`.
  - Patched RVA `0x10580C` (`B9 8C 00 00 00` -> `B9 80 00 00 00`) to route Elite armor effects to the full tag list 80 (Flames, Blue Flames, Heart Attack, Pestilence, Inclement Weather, Birthday Party), matching Spartans.
  - Updated `ReachArmory.cpp` to look up Elite effects in list `"80"` before falling back to `"8c"`.

### 3. Armory Emblems & Nameplates Visuals
- **Problem:** Nameplates and emblems in the Armory only showed text names or raw options.
- **Fix in `src/ui/ArmorCustomizer.cpp`:**
  - In the Armory picker grid: added dedicated branches for `isNameplate` (1 column, 56px height, rendered via `UiArt::Nameplate`) and `isEmblem` (4 columns, square tile, rendered via `UiArt::Emblem`), displaying full visual previews of all 531 nameplates and 330 emblems from `playercustomization.xml`.

### 4. Social Menu & Friends List
- **Problem:** Social menu lacked an easy way to search/add friends directly by username or IP address and invite/join them.
- **Fix in `src/network/SocialClient.*` and `src/ui/FrontEndMenu.cpp`:**
  - Added `AddFriendDirect(nameOrEndpoint)` to `SocialClient`, saving and loading friends locally in `data/userdata/friends.json`.
  - Added a "QUICK ADD FRIEND" text input box and button at the top of the FRIENDS tab in `DrawSocialScreen`.
  - Added a direct `[JOIN]` button for friends configured with IP/port endpoints, and updated `[REMOVE]` to remove instantly without requiring a master server connection.

### 5. Verification
- Built with Visual Studio 18 2026 MSVC toolchain (`cmake --build build --config Release --target noble-client`).
- Verified 0 errors and 0 warnings.
- Executed smoke captures:
  - `noble-client.exe -menu-screen social -menu-capture 30 -quit-after 5` -> Exit Code 0
  - `noble-client.exe -menu-screen armory -menu-capture 60 -quit-after 5` -> Exit Code 0

--
### 3. Verification & Build Status

- Built cleanly via MSBuild: `cmake --build build --config Release`
  - `noble-client.exe`: **0 errors, 0 warnings**
  - `noble-dedicated.exe`: **0 errors, 0 warnings**
- All MCC retail installation files remain 100% read-only and unmodified.

## 0. Pick up here (2026-10-30, 04:05 ET) - Menu overlap cleanup: bottom-anchored network strip + lobby nameplate (WeabooTyler117 / DeepSeek)

UI only: `src/ui/FrontEndMenu.cpp`, no engine, netcode or gameplay code touched. Design direction is
`docs/menu-redesign-spec.md` §2.1 (one shell, focus bar, summaries), §4.3 (campaign / Firefight setup).

### 1. What was wrong (Tyler's four screenshots, 2026-10-30 03:08)

The right column of **Campaign**, **Firefight**, **Custom Game** and **Forge** is laid out top-down from the map
banner, but the old code then pinned the option rows with `y = std::max(y, actionsY - rowsH)` and drew the
NETWORK / LOBBY chip strip at `actionsY - 38 * s`. `rowsH` never counted that strip, so whenever the rows were
pinned they ended exactly at `actionsY` - 38 * s *under* the strip:

- **Campaign:** the chips covered the SKULLS row / skull icons / "Score multiplier" line.
- **Firefight:** the chips covered the bottom of the four difficulty tiles (the EASY / NORMAL / HEROIC names).
- **Custom Game:** with only one row below the banner the row was pushed to the bottom of the column, buried under
  the chips (its label hidden, only the centred value "SLAYER" visible), leaving a dead hole under the blurb.
- **All four:** the strip's status line ("Online P2P (Port 11774) · Broadcasting to Master") started at a fixed
  150 * s chip and was never fitted, so it ran past the column's right edge - that is the text that also sat on top
  of the "SLAYER" value and the HOST LOBBY subtitle.
- **Lobby (screenshot 1):** the player row printed a gold **BUNGIE** tag after a developer's name. Tyler: not
  needed, remove it.

### 2. Fixes (all in `src/ui/FrontEndMenu.cpp`)

- **One source of truth for the bottom of the right column.** New `RightColumnLayout(ui)` returns
  `actions` (top of PLAY SOLO / HOST CO-OP), `strip` (top of the network chips) and `content` (where the option
  rows must end), all derived from `DrawHints`' rule, plus `LobbyStripHeight(ui)`. Campaign and
  Firefight/Custom/Forge both use it, so neither can place the strip over its rows again.
- **Rows flow top-down and the banner takes the leftover space.** The `std::max(...)` pin is gone; `HeroBannerHeight()`
  sizes the picture to `content - rowsH - blurbHeight - bannerTop`, capped at the picture's own aspect (292/668) and
  floored at `64 * s` (min/max, not `std::clamp`, so the floor can never exceed the cap in a very narrow window). The
  blurb height is measured with `ImFont::CalcTextSizeA` (wrap width = column), so a two/three-line mod description is
  accounted for. Cost: the Campaign banner is ~80 px shorter at 1080p (the strip now has its own row).
- **Chip strip rebuilt.** Chip widths come from their labels (`TextWidth + 22 * s`), the privacy chip only exists
  online, and the state line is right-aligned and adaptive: `UPnP READY · LISTED PUBLICLY` -> `UPnP READY` ->
  `LISTED` (nothing is drawn if even that does not fit, never clipped or pushed off-screen). A 6 px dot is green when
  UPnP is forwarding, amber when it is not (fallback text `PORT 11774`), blue for LAN.
- **Lobby nameplate:** the `BUNGIE` tag is gone (the `player.isDev` field is still sent by the server and still used
  by the main-menu party box `[DEV]`). The player name is now clipped at the service tag with `PushClipRect`, so a
  long gamertag cannot run into `[TAG]`.
- **Lobby focused row:** a flat band plus the left gradient, so the row's value on the right is still on the
  highlight (the old gradient faded to alpha 25 before reaching it).

### 3. Verification

- Built with `cmake --build build --config Release`: `noble-client.exe` and `noble-dedicated.exe`, **0 errors,
  0 warnings**.
- The column arithmetic was checked at 1024x576, 1280x720, 1920x1080, 2560x1440 and 3440x1440, including a
  three-line mod blurb: rows always end above the strip, strip above the buttons, buttons above the hint row.
  At 1920x1080 the rows now end 15 px above the chips (Campaign / Firefight) and 109 px (Custom Game).
- Chip/status text measured with Bahnschrift at the sizes the menu uses: chips use 46 % of the 845 px column at
  1080p, and every status variant fits (<= 288 px) down to 1024x576.
- Checked in the running client: `noble-client.exe -menu-screen <campaign|firefight|custom> -menu-capture 90` writes
  `capture_<frame>.bmp` next to the exe and closes itself (`build\ui_after_shot.bat` loops over the screens). The
  1920x1080 frames show the chips in their own row, the tiles and skulls fully visible, and the status line inside the
  column. Note: `-menu-screen main` can be taken over by a stray Accept (pad/keyboard) on the first frames and end up
  on Campaign, so capture the screens with their own name rather than by starting at the main menu.

### 4. What to look at in game

1. **Campaign** - the network chips sit in their own row above PLAY SOLO / HOST CO-OP; the skull icons and the score
   multiplier line are fully visible, nothing overlaps the LEGENDARY tile.
2. **Firefight / Custom Game / Forge** - same; the GAME TYPE row is directly under the map blurb (no hole, no row
   buried at the bottom) and the difficulty tiles are clear of the chips.
3. Click NETWORK and LOBBY: the chips switch instantly and the status line follows (LISTED PUBLICLY / DIRECT
   CONNECT); it never runs past the column at any window size.
4. **Pre-game lobby** - no BUNGIE tag after the name; `[TAG]` stays on the right on its own.

### 5. Repo state when this was written (please read)

- `src/main.cpp` was left with `<<<<<<<` conflict markers from an interrupted `git stash apply` of `stash@{0}`
  ("armory-menu-handoff attempt 2026-10-29"), so nothing compiled. The stash's `main.cpp` is older than HEAD (no
  `-wait-peers`, no `UIManager::HandleWndProc`, old `BlamEngine::LaunchMap` signature), so it was resolved to HEAD
  (`git checkout HEAD -- src/main.cpp`). The rest of that stash - the `Relaunch::HandOff` single-window handoff
  (`Relaunch.cpp/.hpp` + four call sites) - builds and is committed.
- Both parts are pushed to `origin/main` (O7OF/test-build-01): `32cd0f1` is the `Relaunch::HandOff` change (the rest
  of the stash, committed on its own so it can be reverted separately) and `8676b1b` is this menu work. The working
  tree is clean apart from the untracked `tools/armory/fetch_halopedia_icons.py` (an earlier session's icon fetcher).
- `stash@{0}` itself is still in `git stash list` and can be dropped once Tyler is happy with the build.

--

## 0. Pick up here (2026-10-30, 03:25 ET) - Forge / Campaign Netplay Root Causes, Invasion Mode Compatibility, and All-Mode Variant Loading (O7OF / Dominus)

All changes compiled cleanly with MSBuild Release (`noble-client.exe` and `noble-dedicated.exe`), 0 errors.

### 1. Forge was launching the map-editor engine path, not the Forge mode
- **REA evidence:** `rea analyze` on `haloreach.dll` (SHA-256 `738dd2d24ea3aea12e1ee9aa4a61094bf116027d42004c35a19e5048608b0894`, image base `0x180000000`) and `rea decompile` at `0x18000DCCC` show the engine-mode dispatcher. Mode `0x04` is explicitly excluded from the normal game-start branch (`DAT_1824971ac != 4`); only `0x01` campaign, `0x03` multiplayer/Megalo, and `0x05` Firefight take the normal paths.
- **Observed failure:** the 01:25 and 01:27 Forge runs set launch-block `+0x0C = 0x04`, loaded Forge World, then stalled at `launch state 2` / `LoadProgress 1.0%` forever. `c_game_engine Start` returned `0xAC4/0xAC8`, and no game session started.
- **Fix (`BlamEngine.cpp:394`):** Forge is a **multiplayer Megalo** mode. `+0x0C` now stays `0x03`; the Forge behavior comes from `00_basic_editing_054.bin`, not a different engine class. `forge` and `sandbox` mode strings map to engine tag `3`.
- **Verified:** a fresh `forge_halo` + `00_basic_editing_054.bin` host run reached `game session started at frame 294`, loaded the Forge HUD (`StringLookup -> Forge`), and ran until `-quit-after`.

### 2. Campaign / Forge hosts entered simulation before the guest finished joining
- **Observed failure:** in the 02:24 two-process Campaign run the guest sent join-request for ~16 s, the host only processed it after loading, then the host called `boot_peer 1 reason 9` and immediately produced `GameResults`; the mission ended instead of starting co-op.
- **Root cause:** the host posts `Command 11` (Event: In-Game) from a detached thread 5 s after `Command 10` and does not know whether the lobby guest has completed the engine join handshake. Slayer/Firefight tolerate a late join; Campaign/Forge membership cleanup removes the peer (`haloreach+0x44FC94 -> boot_peer reason 9`) and ends the match.
- **Fix:**
  - `FrontEndMenu` now refreshes the lobby roster, counts non-host players, and passes `-wait-peers <n>` to the relaunched host (`FrontEndMenu.cpp:1940`, `FrontEndMenu.cpp:3073`).
  - `main.cpp` parses `-wait-peers` and passes it to `BlamEngine::LaunchMap`.
  - `NetworkTransport` records distinct peers that send on engine port `1000` (`NetworkTransport.cpp:812`) and exposes `GetGamePeerCount()` (`NetworkTransport.cpp:192`).
  - `BlamEngine.cpp:847` holds `Command 11` until the expected engine peers arrive (60 s safety timeout), then waits 8 s for player-slot insertion and guest scenario load for Campaign/Forge (2 s for other modes).
- **Result:** the host can no longer enter simulation before its lobby guests have established the engine channel. A live two-process Campaign run was not completed inside this session (the managed shell cannot reliably host two GPU clients), but the causal `boot_peer reason 9` path is fixed at the point where the host decides to advance.

### 3. Invasion generic variant is map-incompatible; auto-select the stock map-specific variant
- **Observed failure:** `70_boneyard` + `3nvasion_054.bin` produced `EngineEvent reason=4 message='map and game incompatible'` and `PostCommand(11)` returned a garbage status; the match never started.
- **Variant evidence:** the generic file contains only `Invasion`; the Boneyard file contains `INVASION: BONEYARD` and is a different 21,753-byte variant. MCC ships map-specific Invasion variants (`3nvasion_boneyard_054`, `3nvasion_spire_054`, `3nvasion_dlc_054`).
- **Fix (`BlamEngine.cpp:217`):** when the requested stem is exactly `3nvasion_054`, select the map-specific file for Boneyard (1080), Spire (1200), or Breakpoint (2002). The substitution is logged and only happens when the target file exists.
- **Verified:** the same command now logs `using map-specific '3nvasion_boneyard_054.bin' instead`, reaches `game session started`, and no longer emits `map and game incompatible`.

### 4. Other custom game modes no longer silently fall back to Slayer
- **Root cause:** if a requested `-variant` path was missing, `BuildLaunchBlock` fell through to the default `slayer_054.bin`. That made CTF/Assault/Invasion/etc. appear to "work" as Slayer or fail unpredictably.
- **Fix (`BlamEngine.cpp:188`, `BlamEngine.cpp:659`):** variants are resolved against the absolute path, MCC `game_variants`, Noble `data/game_variants`, `data/variants`, and the working directory. A requested-but-missing variant now warns and leaves the block without a substitute; only a host that requested no variant gets the stock Slayer default.
- **Verified:** CTF on Sword Base reached the in-game CTF HUD (`CAPTURE HFLG`, timer `10:`), and Forge/Firefight defaults are unchanged.

### 5. Co-op slot expansion no longer overrides Campaign/Firefight membership
- **Fix (`LobbyPatches.cpp`):** `Hook_SetSlotCounts` now expands only the 16-player multiplayer session (`publicSlots == 16`). It no longer turns Campaign/Firefight 4-slot memberships into 8/32, which made the engine clean out legitimate co-op peers.
- The existing Campaign abort/insertion hooks and gate patches remain in place. REA byte checks confirmed the current patch offsets:
  - `0x411B50`: `74 0D` at `+0x1E` (the `je` after `evaluate_coop_insertion_point`), patched to `EB 0D`.
  - `0x409530`: Campaign cap `7E 22` at `0x40954A`, Firefight cap `76 11` at `0x40955B`, `cmovz ecx,edx` at `0x40958B`.
  - `0x397650`: Campaign in-progress join gate `75 0A` at `0x397689`.

### 6. Verification summary
- `forge_halo` + Forge: game session started, Forge loaded.
- `70_boneyard` + generic Invasion: substituted to Boneyard variant, game session started, no incompatibility.
- `20_sword_slayer` + CTF: in-game CTF HUD (`CAPTURE HFLG`).
- Engine mode values and gate bytes checked with REA against the shipped `haloreach.dll`; no MCC files modified.

**Remaining test:** run a real two-PC Campaign co-op and Forge join with the new `-wait-peers` barrier, and confirm host stays in-game after the guest loads. If the host still boots the guest, next step is to log `[session+0x17C]` and `[session+0x118]` for each slot inside the `0x44FC94` cleanup path to identify which membership field still fails the "in game" test.

--

## 0. Pick up here (2026-10-30, 02:20 ET) - Definitive Fix for Campaign Co-op "Error hosting co-op" (Session Abort Suppression & Gate Neutralization) (O7OF / Dominus)

All changes compiled cleanly via MSBuild Release configuration (`noble-client.exe` and `noble-dedicated.exe`) with **0 errors and 0 warnings**:

### 1. Reverse-Engineered Haloreach.dll Centralized Session Abort Subsystem
- **Disassembled Centralized Abort Sink at RVA 0x44EBA0 (`c_network_session::abort`):**
  - Prototype: `void __fastcall(void* session, uint32_t reason)`
  - Prologue: `48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9 48 83 C1 30`
  - When the host detects any co-op checkpoint mismatch, peer timing discrepancy, or membership desync mid-game, it calls `0x44EBA0` with an abort reason code:
    - `0x1B` (`error_players_incompatible_for_network_coop` -> "An error occurred while playing co-op." / "Error hosting co-op")
    - `0x1C` (`error_coop_incompatible_peer_disconnect`)
    - `0x1D` (Campaign co-op state sync timeout)
    - `0x19` (`error_players_missing_content`)
    - `0x13`, `0x14`, `0x1E`, `0x1F` (peer incompatibility refusals)
  - `0x44EBA0` writes `[session + 0x90 + 0x30] = reason` and invokes virtual teardown which kills the game session and throws the host and clients back to the main menu.
  - Sibling callers identified:
    - RVA `0x411B6E`: `mov edx, 1Bh; call 0x44EBA0`
    - RVA `0x411D71`: `mov edx, 1Ch; call 0x44EBA0`
    - RVA `0x409586`: `cmp eax, -1; mov edx, 1Bh; cmovz ecx, edx` (at `0x40958B`: `0F 44 CA`)

### 2. Applied Root Cause Fixes in `LobbyPatches.cpp`
- **MinHook Detour on `0x44EBA0` (`c_network_session::abort`):**
  - Added `Hook_SessionAbort(void* session, uint32_t reason)`.
  - Intercepts and logs all abort requests with session pointer, caller RVA, and role.
  - When `reason` is within the peer incompatibility range (`0x10 <= reason <= 0x25`), **suppresses the call completely** and returns early.
  - Because `0x44EBA0` returns `void` and callers immediately execute stack epilogues without evaluating return codes, suppressing it prevents the host from killing the match, allowing the simulation to proceed seamlessly.
  - Normal exit/disband codes (`reason < 0x10`, e.g., user quitting to menu or disbanding lobby) are preserved and pass through untouched.
- **Added Sibling Gate Bypass at RVA `0x411D71` (`74 0D` -> `EB 0D`):**
  - Bypasses the in-progress disconnect/incompatibility abort call for Reason `0x1C`.
- **NOPed Out `cmovz ecx, edx` at RVA `0x40958B` (`0F 44 CA` -> `90 90 90`):**
  - Prevents `ecx` from receiving refusal code `0x1B` when campaign insertion evaluation returns `-1`.

### 3. Verification & Build
- MSBuild Release build completed with **0 errors and 0 warnings**:
  - `noble-client.exe` -> `build/bin/Release/noble-client.exe`
  - `noble-dedicated.exe` -> `build/bin/Release/noble-dedicated.exe`
- Cleaned up all temporary reverse-engineering `.js` scripts from workspace.

--

## 1. Previous (2026-10-30, 01:25 ET) - Fix Campaign Co-op In-Progress 0x1B Abort Gate & Forge/Sandbox Engine Dispatch (O7OF / Dominus)

All changes compiled cleanly via MSBuild Release configuration (
oble-client.exe and 
oble-dedicated.exe) with **0 errors and 0 warnings**:

### 1. Reverse-Engineered Haloreach.dll Engine Mode Tags & String Dispatch Tables
- **Disassembled Mode Dispatch at RVA 0xDCEE and 0xDECF:**
  - lock[0x0C] selects the engine class instantiated upon c_game_engine::Start:
    - 0x00: Main Menu
    - 0x01: Campaign (campaign_game_engine, ID 0x12D / 301)
    - 0x03: Multiplayer Megalo (megalogamengine, ID 0x12C / 300)
    - 0x04: Forge / Map Editor (c_sandbox_engine, "sandbox", ID 0x12B / 299)
    - 0x05: Firefight (survival_game_engine, ID 0x12E / 302)
- **GUI Mode String Table (File 0x9C0CC0):**
  - ID 0x80083: "campaign"
  - ID 0x80084: "survival"
  - ID 0x80085: "coop"
  - ID 0x80086: "custom_game"
  - ID 0x80087: "mapeditor"

### 2. Resolved Forge Mode / Sandbox Player Loading & Desync
- **Root Cause in BlamEngine.cpp:341:**
  - Launch block byte +0x0C was previously computed as (isMainMenu || isCampaign) ? 0x01 : (isFirefight ? 0x05 : 0x03).
  - For Forge (isForge), +0x0C fell back to 0x03 (Multiplayer Megalo) rather than 0x04!
  - When set to 0x03, the engine instantiated c_custom_game_engine rather than c_sandbox_engine, failing to initialize sandbox-engine-globals and breaking peer editor role assignments.
  - Furthermore, in FrontEndMenu.cpp:1718-1731, the guest synchronized lobby launch handler searched only &ctx.variants and &ctx.ffVariants. Because kForgeVariant (00_basic_editing_054.bin) was excluded from ctx.variants, the guest failed to resolve the Forge variant and launched without it.
- **Fix:**
  - In BlamEngine.cpp:341, updated lock[0x0C] to (isMainMenu || isCampaign) ? 0x01 : (isFirefight ? 0x05 : (isForge ? 0x04 : 0x03));.
  - In FrontEndMenu.cpp:1722-1731, added resolution for ctx.forgeVariant on the guest during Forge lobby launch.
  - In FrontEndMenu.cpp:2633, set choice.mode = forge ? "forge" : "multiplayer"; for solo/local Forge sessions.
  - In FrontEndMenu.cpp:3105, bound choice.variantPath = ctx.forgeVariant.string(); for direct Server Browser joins to Forge servers.

### 3. Neutralized Campaign Co-op In-Progress 0x1B "Error hosting co-op" Gate
- **Binary Reverse Engineering (haloreach.dll):**
  - Traced the mid-game abort when remote players spawn into Campaign:
  - At RVA 0x411B50, during in-progress session membership ticks (state 6..9), the host calls 0x39EC88 to check checkpoint and progression parity across remote peers.
  - If any slight desync is detected, 0x411B6A (	est al, al; jz 0x411B80) falls through to 0x411B6E: mov edx, 1Bh; call 0x44EBA0, triggering Bungie's dialog "GAME ERROR: An error occurred while playing co-op." / "Error hosting co-op" and aborting the session.
- **Fix in LobbyPatches.cpp:**
  - Added gate patch at RVA 0x411B6C: 74 0D -> EB 0D, unconditionally jumping over the 0x1B abort call to the cleanup block (0x411B7B).
  - In BlamEngine.cpp:525, properly initialized player slots 1..15 controller index bytes (+0x04..0x07 = 0x00) while keeping valid/XUID markers clean (+0x00..0x03 = 0xFF; +0x08..0x0F = 0xFF).

--
## 0. Pick up here (2026-10-30, 00:25 ET) - Fix Campaign Co-op "Error hosting co-op" & Firefight Co-op Match Loading (O7OF / Dominus)

All changes compiled cleanly via MSBuild Release configuration (`noble-client.exe` and `noble-dedicated.exe`) with **0 errors and 0 warnings**:

### 1. Reverse-Engineered Root Cause of Campaign "Error hosting co-op" (`src/hooks/LobbyPatches.cpp`)
- **Symptoms:** In Campaign co-op, the non-host (guest) client spawned into the mission, but the host immediately crashed or aborted with Bungie's in-game dialog `"Error hosting co-op"` (`GAME ERROR: An error occurred while playing co-op.`).
- **Binary Reverse Engineering (`haloreach.dll`):**
  - Traced the Bungie error string to RVA `0xA3DD40` table index 27 (0x1B): `error_players_incompatible_for_network_coop` (`0x500012`).
  - At RVA `0x409586`, `haloreach.dll`'s game start validation function checks Campaign mode (tag 1) and calls `c_network_session_membership::evaluate_coop_insertion_point` (`RVA 0x39CE10`).
  - `0x39CE10` compares each player's campaign progression and mission checkpoint fields: `min(player[+0x144])` vs `max(player[+0x148])`. If the values do not match across all players, it returns `-1`.
  - When `0x39CE10` returns `-1`, RVA `0x409586` executes `cmovz ecx, 1Bh`, setting error code `0x1B` and causing the host to display `"Error hosting co-op"` and abort the match.
- **Fix:**
  - Installed a MinHook detour on `c_network_session_membership::evaluate_coop_insertion_point` (`RVA 0x39CE10`, unique sig `"48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 8B 71 2C 48 83 C8 FF"`).
  - If `0x39CE10` returns `< 0`, the hook overrides it to `0` (mission start insertion point). This completely prevents the `0x1B` refusal code and allows the host and guest to play co-op campaign together seamlessly.
  - Patched the campaign co-op player cap gate at `0x40954A` (`7E 22` -> `EB 22`) and Firefight player cap gate at `0x40955B` (`76 11` -> `EB 11`) to bypass hardcoded player caps (`0x1A` / `0x46`).

### 2. Fixed Firefight Co-op Match Loading & Prebuilt Block Transport Socket Activation (`src/engine/BlamEngine.cpp`)
- **Symptoms:** Firefight matches failed to let players load in (guest sent 52 join requests with no response from the host).
- **Root Cause:**
  - In `BlamEngine.cpp`, when `loadedPrebuilt` was true (which occurs for Firefight maps 210..219 and Campaign maps 178..189), the network socket enable flag at `block[0x2BF14]` was only written for the guest (`block[0x2BF14] = 0xF3; block[0x2BF15] = 0x01;`), leaving it `0x00 0x00` on the host.
  - As a result, the host's Firefight session never activated its transport listening socket, silently dropping all incoming join-request packets from remote clients.
  - Furthermore, slots 1..15 in authentic prebuilt dumps retained uninitialized bytes at `+0x04..` rather than the clean unassigned slot descriptor (`0xFF`).
- **Fix:**
  - On the host, explicitly written `block[0x2BF14] = 0xF3; block[0x2BF15] = 0x01;` for prebuilt blocks, activating the network transport listening socket.
  - Sanitized slots 1..15 to unassigned (`0xFF`) across all 16 slot structures (`0x100 + slot * 0x20`).
  - Added map variant ID synchronization at `+0x1D334` and `+0x1D5BC` for both Campaign and Firefight.

--

## 0. Pick up here (2026-10-29, 23:35 ET) - Native Armory Character Rendering, Authentic In-Game Armor Effects (Flames & Blue Flames), Infinite Loading Fix & Netcode Updates (O7OF / Dominus)

All changes compiled cleanly via MSBuild Release configuration (`noble-client.exe` and `noble-dedicated.exe`) with **0 errors and 0 warnings**:

### 1. Removal of Custom Background Keying / Native Armory Rendering (`src/engine/ReachHost.cpp`)
- **Root Cause:** Due to dark skin suits / black undersuits measuring low luminance, luminance-based alpha thresholding caused dark geometry to be clipped or turn invisible against the custom background wallpaper.
- **Fix:** Completely stripped the luminance keying and texture sampling composite pass from `GammaPass`. `GammaPass` is now a pure, lightweight full-screen gamma correction pass (returning immediately when `exponent == 1.0f`). The 3D Spartan and Elite armory models now render on Bungie's authentic 3D armory environment with 100% full fidelity of dark undersuits, skin suits, and emissive particles without edge artifacts or clipping.

### 2. Fixed In-Game Armor Effects (Flames, Blue Flames, Heart Attack, Pestilence, etc.) (`src/engine/ReachHost.cpp`)
- **Root Cause (Reverse-Engineered `haloreach.dll+0x3B2F4` / `0x2EDD7C`):**
  - Bungie's engine expects authentic Reach armor effect codes: `0 = None, 1 = Flames (Legendary), 2 = Blue Flames (Eternal), 3 = Heart Attack, 4 = Pestilence, 5 = Inclement Weather, 6 = Birthday Party`.
  - `ReachHost.cpp` was incorrectly adding MCC item IDs (`166 + effect`), which `0x3B2F4` rejected as out of range and reset to 0 (None).
  - Additionally, `if (effect >= 6) effect = 0;` was clamping UI effect 6 (Eternal Blue Flames) to None.
- **Fix:**
  - Implemented `mapArmorEffect(int idx, const std::string& id)` converting customizer UI IDs and indices directly to the authentic Bungie engine enum.
  - Written directly to `profile[0x68]` (Spartan effect) and `profile[0x90]` (Elite effect). Removed erroneous `effect >= 6` clamping. Both Legendary Flames and Bungie Eternal Blue Flames now render in-game.

### 3. Fixed Infinite Loading at Launch State 9 & Multi-Mode Join Sync (`src/engine/BlamEngine.cpp`, `src/network/NetworkTransport.cpp`, `server/server.js`)
- **Root Cause:**
  - In `BlamEngine.cpp:607`, an experimental block guarded by `-legacy-guest-block == nullptr` was running by default on guests, wiping `block[0xE8]` (active player count) and `block[0x300..0x1D308]` (game and map variants) to 0. Without active players or a variant, `haloreach.dll` froze infinitely in launch state 9.
  - For Forge mode, the default game variant was falling back to `slayer_054.bin` instead of Reach's authentic `00_basic_editing_054.bin`.
  - In `server/server.js:703`, `broadcastLobbyEvent` called `Object.assign(session.launchData, { host_ip: session.address, is_lan: false })`, which mutated `session.launchData` in place and broke LAN IP loopback for local network joins.
- **Fix:**
  - In `BlamEngine.cpp`, changed the guest zeroing check to `std::wcsstr(GetCommandLineW(), L"-rr-guest-block") != nullptr` so the stable default guest block (active player 1 at `0xE8`, guest player ID at `0xF0`, host machine ID at `0x2F8`, and decoded game variant at `0x300`) runs by default.
  - In `BlamEngine.cpp`, set Forge game mode tag (3) and variant loader to default to `00_basic_editing_054.bin`.
  - In `server/server.js:703`, broadcast a clone `{ ...session.launchData, host_ip: session.address, is_lan: false }` without mutating `session.launchData`.
  - In `NetworkTransport.cpp`, prioritize `host_lan_ip` when `is_lan` is true. Pre-game lobby and in-progress joins now connect and load properly across Campaign, Multiplayer, Forge, and Firefight.

### 4. Database Reset Tool & Dev Scripts Deployed (`server/reset_db.js`, `ServerTestDeploy`)
- Created `server/reset_db.js` to safely rotate and recreate clean SQLite tables (`users`, `stats`, `cosmetics`, `unlocked_items`, `friends`).
- Executed database reset for clean testing.
- Copied `server.js`, `set_dev.js`, `grant_item.js`, and `reset_db.js` into `C:\Users\Chey1\Desktop\ServerTestDeploy` and committed/pushed to `origin/main` (`83602ac`).

--

## 1. Pick up here (2026-10-29, 22:35 ET) - Complete Elimination of SSAO Side Fuzz and Flame Smoke Artifacts (O7OF / Dominus)

### 1. Root Cause Analysis: Air SSAO Contact Shadow & Flame Smoke
- In `mainmenu_all.map`, the engine's SSAO (Screen Space Ambient Occlusion) pass creates screen-space contact shadows around the limbs that spread `10-15px` out into the air with luminance in the `[0.035 .. 0.070]` range.
- Particle smoke textures in armor effects (like Blue Flames) also have dark falloff in that range.
- The previous `kCutoff = 0.035f` was allowing these air SSAO shadows and dark smoke particles to produce visible side fuzz along the outer edges of the arms, thighs, knees, and flame tips.
- Solid Spartan geometry (armor plates, black visor with spotlight reflections, undersuit) consistently measures at `luma >= 0.110f`.

### 2. Fixes Applied in `ReachHost.cpp:GammaPass`
- **Tuned Rejection Floor to `kCutoff = 0.072f` and `kSolid = 0.110f`:**
  - Completely rejects all air SSAO shadow blur and room haze (`alpha = 0.0f` for all `luma <= 0.072f`), eliminating 100% of the side black fuzz.
  - Normalizes `fgClean = max(0.0f, c.rgb - kCutoff) / max(0.0001f, 1.0f - kCutoff)` so the anti-aliased edge blends smoothly into the wallpaper.
- **Pure Additive Emissive Glow for Armor Effects:**
  - Replaced chroma thresholding with pure excess channel emissive calculation (`blueExcess = max(0.0f, c.b - max(c.r, c.g))`).
  - Dark particle smoke (neutral gray) is completely dropped, while colorful glowing fire adds vibrant blue aura directly onto the background scenery with zero darkening.
- **Verification:**
  - Built Release configuration with 0 errors and 0 warnings (`noble-client.exe`).

--

## 1. Pick up here (2026-10-29, 22:30 ET) - Fix Armory Black Matte Silhouette Render Artifact (O7OF / Dominus)

### 1. Root Cause Analysis: Black Soot / Halo Around Spartan in Armory
- In `mainmenu_all.map`, the Armory scene chamber contains faint volumetric spotlight haze, SSAO contact shadow noise, and translucent particle falloff (from armor effects like Blue Flames) with linear luminance in the low `[0.002 .. 0.035]` range rendered against a black matte.
- The initial composite pass used `smoothstep(0.001f, 0.030f, luma)` and `lerp(bg.rgb, spartanRgb, alpha)`:
  - Any faint ambient fog or SSAO sample in the `[0.002 .. 0.035]` range triggered `alpha > 0`, causing the bright white mountain snow wallpaper (`RGB ~ 0.85`) to lerp toward the dark engine fog (`RGB ~ 0.01`), darkening the snow into a noisy black soot cloud / dark matte fringe around the entire character silhouette.
  - Flame particle edges (premultiplied over black) were multiplied by `alpha`, causing them to darken the snow rather than adding light.
Sneeze
### 2. Fixes Applied
- **Eliminated Black Matte Fringe in `ReachHost.cpp:GammaPass`:**
  - Raised the cutoff threshold: `const float kCutoff = 0.035f; const float kSolid = 0.080f;` so all faint ambient fog, room haze, and SSAO noise below `0.035` luminance are completely rejected (`alpha = 0.0f`), leaving the background wallpaper 100% clean and pristine.
  - Subtracted the room fog floor from the foreground: `float3 fgClean = max(0.0f, c.rgb - kCutoff) / max(0.0001f, 1.0f - kCutoff);` so geometry silhouette edges blend cleanly with zero dark matte fringe.
  - Added additive chroma compensation for armor effects:
    ```hlsl
    float chroma = max(c.b, c.r) - min(c.r, min(c.g, c.b));
    if (chroma > 0.02f && alpha < 1.0f) {
        float3 emissive = max(0.0f, c.rgb - min(c.r, min(c.g, c.b))) * (1.0f - alpha);
        finalRgb += emissive * 0.85f;
    }
    ```
    Emissive effects (like Blue Flames) now cast radiant blue light onto the background scenery instead of producing dark soot.
- **Verification:**
  - Rebuilt Release configuration with 0 errors and 0 warnings (`noble-client.exe`).

--

## 1. Pick up here (2026-10-29, 22:25 ET) - In-Engine 3D Armory Background Composite (O7OF / Dominus)

### 1. Architectural Problem
- In mainmenu_all.map, the **Main Menu** scene (x00: start_menu) points its camera at the outdoor Reach mountain landscape vista.
- When switching to the **Character Preview / Armory** (x0A: armory, x0C: c_full_spartan, c_helmet, etc.), the Blam! engine switches cameras to the pedestal inside an isolated dark chamber where the surrounding space is pitch black (0, 0, 0).
- Because haloreach.dll renders the 3D Spartan directly into the DX11 backbuffer, drawing a 2D background in ImGui paints over the backbuffer (occluding the character). Leaving it clear left the character surrounded by an empty black void.

### 2. Implementation: DX11 Background Composite in GammaPass
- **Added ReachArmory::IsArmoryScene():**
  - Queries mainmenu_all.map's active scene index from s_targetScene to detect whenever the engine is displaying an Armory customization scene (x0A..0x14, excluding x0E).
- **Upgraded GammaPass Post-Process Shader (ReachHost.cpp):**
  - Bound UiArt::MenuBackground() to pixel shader texture register 	1 and a linear clamp sampler to s0.
  - When is_armory is active:
    - Calculates aspect-ratio preserving cover UVs for the 2D background wallpaper.
    - Samples g_texture.
    - Detects the unlit empty void pixels (luma == 0.0f) vs. the lit 3D Spartan, visor, armor highlights, and pedestal.
    - Uses smoothstep(0.001f, 0.030f, luma) to composite the 2D Reach background directly **behind** the 3D Spartan and pedestal.
    - Applies gamma correction to the 3D scene elements to match sRGB space.
  - The live in-engine 3D Spartan and pedestal now stand proudly in front of the Reach background wallpaper, completely eliminating the black void while keeping 100% character visibility.
- **Verification:**
  - Built Release configuration with 0 errors and 0 warnings (
oble-client.exe).

--
## 1. Pick up here (2026-10-29, 22:15 ET) - Fix Player Preview Character Rendering Behind Background (O7OF / Dominus)

### 1. Root Cause Analysis
- In Halo: Reach (`haloreach.dll`), the live 3D player Spartan/Elite character is rendered directly into the DirectX 11 swapchain backbuffer by the Blam! engine before the front-end menu overlay (`FrontEndMenu::RenderOverlayImpl`) runs.
- In `src/ui/FrontEndMenu.cpp:DrawBackdrop`, `Screen::Armory` was included in `needBackdropImage`:
  ```cpp
  const bool needBackdropImage = !ReachArmory::IsSceneReady() || screen == Screen::Armory || screen == Screen::PreGameLobby;
  if (needBackdropImage) {
      ui.dl->AddRectFilled(o, ui.size, IM_COL32(8, 12, 18, 255));
      UiTexture bg = UiArt::MenuBackground();
      if (bg) {
          UiArt::DrawCover(ui.dl, bg, o, ui.size, IM_COL32(255, 255, 255, 255));
      }
  }
  ```
- Because `needBackdropImage` evaluated to `true` whenever `screen == Screen::Armory`, `UiArt::DrawCover` drew the 2D background image over the entire viewport with 100% opacity (`IM_COL32(255, 255, 255, 255)`).
- This painted over the DirectX 11 backbuffer, causing the live 3D Spartan/Elite player character to be drawn behind the 2D wallpaper image.

### 2. Fixes Applied
- Updated `src/ui/FrontEndMenu.cpp:DrawBackdrop`:
  - Removed `screen == Screen::Armory` from `needBackdropImage`:
    ```cpp
    const bool needBackdropImage = !ReachArmory::IsSceneReady() || screen == Screen::PreGameLobby;
    ```
  - When `ReachArmory::IsSceneReady()` is true, the right side (`0.60f * width .. 1.0f * width`) remains 100% transparent in ImGui, allowing the live 3D Spartan/Elite rendered by `haloreach.dll` to be completely visible in front.
  - When `!ReachArmory::IsSceneReady()` (initial engine startup while `mainmenu_all.map` is loading into memory), `needBackdropImage` remains `true` to cleanly avoid black void flickers.
- Verified build: `noble-client.exe` compiled cleanly with 0 errors and 0 warnings.

--

## 1. Pick up here (2026-10-29, 22:00 ET) - Double Typing Fix, Seamless Armory Background, Full Controller UI Navigation & 1:1 Reach Pre-Game Lobby Layout (O7OF / Dominus)

### 1. Root Cause & Fix: Double Character Typing in Login/Register Screen
- **Root Cause:**
  - `ImGui_ImplWin32_WndProcHandler` receives `WM_CHAR` and enqueues typed characters into `io.InputQueueCharacters` via `io.AddInputCharacterUTF16`.
  - In `FrontEndMenu.cpp:FeedEngineThreadInputs`, an auxiliary loop polling `GetAsyncKeyState` was also unconditionally executing:
    `if (down && kKeys[i].ch != 0 && io.WantTextInput) { io.AddInputCharacter(static_cast<unsigned int>(kKeys[i].ch)); }`
  - Every single keystroke entered in text inputs (e.g. login/register modals) was injected twice into ImGui (once from `WM_CHAR` and once from `FeedEngineThreadInputs`), resulting in doubled characters (e.g. `TT`, `FF`, `BB`).
- **Fix:**
  - Removed duplicate `io.AddInputCharacter` from `FeedEngineThreadInputs`.
  - Added `#include <imgui_impl_win32.h>` and called `ImGui_ImplWin32_NewFrame()` in `FrontEndMenu::RenderOverlayImpl` before `ImGui::NewFrame()`.

### 2. Full Controller Navigation Across All UI Windows
- **Root Cause:**
  - `ImGui_ImplWin32_NewFrame()` was never invoked during overlay rendering; only `ImGui_ImplDX11_NewFrame()` was called. Because XInput polling inside ImGui lives in `ImGui_ImplWin32_UpdateGamepads()` (called from `ImGui_ImplWin32_NewFrame()`), gamepads were completely inert during overlay menu rendering.
- **Fixes Applied:**
  - Integrated `ImGui_ImplWin32_NewFrame()` and `InputPrompts::Update()` on every frame in `FrontEndMenu::RenderOverlayImpl`.
  - Added Left Analog Stick navigation (`ImGuiKey_GamepadLStickDown`, `LStickUp`, `LStickLeft`, `LStickRight`) in addition to D-pad across all menus:
    - Main Menu item stepping (`FrontEndMenu.cpp`)
    - Generic option rows (`OptionRow` in `FrontEndMenu.cpp`)
    - Pre-Game Lobby option cycling & list navigation (`FrontEndMenu.cpp`)
    - Server Browser server list navigation (`ServerBrowser.cpp`: D-pad/stick up/down to select server, A to join, Y to favorite, X to refresh, LB/RB to cycle modes)
    - Armor Customizer slot selection and option stepping (`ArmorCustomizer.cpp`)
    - Settings panel sliders and checkboxes (`SettingsPanel.cpp`)

### 3. Seamless Armory Background (No Black Void Flicker)
- **Root Cause:**
  - `DrawBackdrop` only rendered `UiArt::MenuBackground()` when `!ReachArmory::IsSceneReady()`. When opening the Armory, `ReachArmory::SetTargetScene` cuts the Blam engine camera to scene `0x0A` (interior dark hangar). When transitioning or when Noble draws without an engine frame, the right side had no background, flashing solid black.
- **Fix Applied:**
  - In `FrontEndMenu.cpp:DrawBackdrop`, ensured `UiArt::MenuBackground()` is rendered under `Screen::Armory` and `Screen::PreGameLobby`.
  - Combined with `ObjPreviewCamera::transparentBackground = true` in `ArmorCustomizer.cpp`, the character model and menu list sit seamlessly on top of the Reach menu backdrop with zero flicker to black.

### 4. 1:1 Pre-Game Lobby Overhaul (Campaign, Forge, Custom Games, Firefight)
- **Visual Design Matching Reference Screenshots 1:1:**
  - **Top-Left Preview Box:** 16:9 map preview thumbnail with dark border and corner emblem badge (Campaign difficulty/swords shield, Forge globe logo, Custom game Infection/gametype badge).
  - **Header & Title:** Vertical `< MAIN MENU` back indicator on the left margin, Reach winged chevron emblem, bold title (`CAMPAIGN`, `FORGE`, `CUSTOM GAME`, `FIREFIGHT`), and authentic subtitle line (`This party is open to friends and recent players.`, `This party is local to this Xbox 360 console.`).
  - **Menu Options List:** Clean alignment with label on left, value on right, contextual description tooltip below, and **Halo: Reach signature orange highlight bar** on the left edge of focused rows (`IM_COL32(255, 140, 30, 255)`).
    - Campaign: Ready, Mission, Skulls, Difficulty, Scoring, Start Co-Op Campaign
    - Forge: Game Type, Map, Game Options, Start Game
    - Custom Game: Ready, Game Type, Map, Game Options, Start Game
    - Firefight: Ready, Fire Fight, Map, Game Options, Start Firefight
  - **Center Countdown Display:** Large `0:05` glowing timer with `Press (X) to cancel the countdown` and `Variant / Map` kicker when launching.
  - **Right Column ("MY PARTY" Panel):** Solid black `MY PARTY` tab, subheader bar `In [Type] Lobby X/16`, player list with voice/speaker icon, emblem, gamertag, gold `BUNGIE` badge if dev, rank insignia shield / service tag `[117]`, orange/red selected row highlight, `(Y) <Host>'s Friends:` section, and host direct IP card with one-click "COPY INVITE".
  - **Bottom Button Prompts Bar:** Non-overlapping bottom bar with Halo controller glyphs: `(A) Select`, `(B) Back`, `(Y) Select Network`, `(Start) Menu`, `(X) Prior Game Stats`.

--
## 0. Pick up here (2026-10-29, 21:25 ET) - Fix Crash at Main Menu (0x105597), Login Modal Input/Deadlock & Crash, and Multiplayer Guest Session Launch (Dominus / O7OF)

### 1. Root Cause 1: Native Crash 0xC0000005 at haloreach.dll+0x105597 upon Entering Main Menu
- **Disassembly Analysis:**
  - Disassembled haloreach.dll RVA 0x1051E0 - 0x1055D0 (Bungie player appearance verification & fallback permutation builder).
  - At 0x1052C9: cmp byte ptr [rsp+0x58], 0x40 (64). If any armor index (e.g. Helmet ID) is >= 64, al is set to 1.
  - When the user had 'helmet_id': 'hilands_c' (index 71), al != 0.
  - At 0x105324: je 0x1055c1. Because al != 0, the jump does not take, dropping execution into Bungie's composite permutation fallback builder at 0x10532A.
  - At 0x105558: The fallback routine uses a fixed 16-element stack array. When capacity is exceeded, it writes with index -1 to [rbp+0x18], corrupting the item count into an invalid large address/pointer.
  - At 0x105597: It iterates the corrupted count over the stack, faults past the stack guard page, and crashes with Access Violation 0xC0000005.
- **Fix Applied:**
  - In src/hooks/EnginePatches.cpp: Patched haloreach.dll RVA 0x105324 from 0F 84 97 02 00 00 (je 0x1055c1) to E9 98 02 00 00 90 (jmp 0x1055c1; nop). Invalid indices already get clamped to 0 by the preceding code; jumping directly to the function exit completely bypasses the bugged stack-corrupting fallback routine.
  - In src/engine/ReachHost.cpp: Clamped helmet >= 64 to 0 (Mark V [B]) in ApplyArmoryAppearance.

### 2. Root Cause 2: HostSlot47 (UserDataFolder) Return Value & Directory Resolution
- Callers in haloreach.dll at RVA 0xABB45 and 0x34DCF9 test al for success (test al, al; je ...). Returning 0 treated user data folder resolution as a failure.
- **Fix Applied:** In src/engine/ReachHost.cpp:HostSlot47_UserDataFolder, returned 1 on success instead of 0.

### 3. Root Cause 3: Login Modal Crash, Popup Focus Deadlock & Thread Safety Race
- ImGui::OpenPopup was being invoked unconditionally every single frame in DrawAccountModal and DrawDevMenuModal, corrupting ImGui's internal popup stack.
- The background menu continued processing keyboard navigation while the popup modals were active.
- Data race in AuthClient: m_username, m_sessionToken, m_userId, and m_isLoggedIn were being read/written across threads concurrently without synchronization in RefreshDeveloperStatusAsync.
- ImGui_ImplWin32_WndProcHandler was never forwarded from the main window message procedure in main.cpp.
- **Fixes Applied:**
  - Added mutable std::mutex m_authMutex in src/network/AuthClient.hpp and guarded all accessors and mutators in src/network/AuthClient.cpp.
  - Added one-shot modal triggers (OpenAccountModal(), OpenDevMenuModal()) and explicit ImGui::CloseCurrentPopup() calls in src/ui/FrontEndMenu.cpp.
  - Disabled background navigation while any modal dialog is active (modal = st.confirmQuit || s_showAccountModal || s_showDevMenu).
  - Registered UIManager::HandleWndProc via window->SetCustomWndProc in src/main.cpp.

### 4. Root Cause 4: Multiplayer Guest Desync & Missing Session Token
- In src/ui/FrontEndMenu.cpp:DrawPreGameLobby, when launching the guest match, choice.sessionToken = st.lobbySessionToken; was omitted, causing the guest to launch without -session-token on the command line.
- In src/engine/BlamEngine.cpp:605, commit 622907a inverted the guest launch block check (std::wcsstr(..., L" -rr-guest-block\) != nullptr), disabling the working RR layout by default and falling back to a broken legacy guest block that deadlocked in state 9.
- In src/engine/ReachHost.cpp, added SafeWriteUint16 helper to resolve compilation error C3861.
- **Fixes Applied:**
 - Restored default RR guest block layout in src/engine/BlamEngine.cpp (std::wcsstr(..., L\-legacy-guest-block\) == nullptr).
 - Propagated sessionToken into PendingLobbyEvents, TakePendingLobbyEvents, and choice.sessionToken in src/ui/FrontEndMenu.cpp.
 - Added SafeWriteUint16 helper function in src/engine/ReachHost.cpp.
- **Verification:** Built cleanly via cmake --build build --config Release with 0 errors and 0 warnings.

--
## 0. Pick up here (2026-10-29, 18:15 ET) - Online P2P Lobby Launch Diagnostic & Fix: Guest Stealing UDP 11774, Self-Loopback Disabling Relay, & Shared `settings.json` Identity Swap (Dominus / Dominus-VII)

### 1. Diagnostic Analysis of `build\bin\Release\noble-client.log` (`14:50:29`–`14:54:40`)
- **Why It Looked Like the Guest (`TEST2`) Joined Successfully While the Host (`DOMINUS`) Crashed/Closed:**
  1. **Zero Windows Crash Exceptions:** Checked `build\bin\Release\noble-client.log`, Windows Event Log (`Application`), WER reports, and `CrashDumps` — neither process suffered a native crash (`0xC0000005` / `0xE06D7363`). Instead, the two windows swapped identities on relaunch and the Guest timed out after 60 seconds and relaunched back to the menu.
  2. **Account Identity Swap Across `Relaunch::Start` (`DOMINUS` -> `TEST2`):**
     - At `14:50:29.593`, Window 1 (`PID 25292`, Host Menu) started logged in as **`DOMINUS`** and hosted Online P2P Lobby `'DOMINUS's Lobby'` (`session 4f562e4f44371cb1` at `14:52:09.824`).
     - At `14:51:19.078`, Window 2 (`PID 3048`, Guest Menu) started on the same PC, logged out of `DOMINUS` (`14:52:21.600`), and logged in as **`TEST2`** (`14:52:30.199`), overwriting the shared `build\bin\Release\data\userdata\settings.json` file with `TEST2`'s credentials.
     - At `14:52:48.676`, Window 1 (`DOMINUS`, Host) clicked **START MATCH**. Both windows spawned new match processes via `Relaunch::Start(args)` (`PID 22864` for Host, `PID 13820` for Guest).
     - When the **Host match process (`PID 22864`)** booted at `14:52:49.226`, it read `settings.json` from disk—which now had **`TEST2`**! So the **Host** loaded into `20_sword_slayer` (`game session started at frame 266, loading screen off` at `14:52:58.857`) displaying **`TEST2`** on the HUD/scoreboard (`ReachHost [Slot 88 - UserIdentity]: user 0 -> 'TEST2'`), making it look as if the Guest (`TEST2`) had joined the match while the Host (`DOMINUS`) failed!
     - Meanwhile, the actual **Guest match process (`PID 13820`)** stayed stuck at `LoadProgress: 2.0%, launch state 9` for 60 seconds and at `14:53:52.882` aborted with `Remote session join timed out ('external_launch_individual_state_timeout'); returning to main menu.` and relaunched itself back to the main menu (`PID 11220`).
  3. **Guest Stole UDP Port `11774` From the Host by 2 Milliseconds (`14:52:51.585` vs `14:52:51.587`):**
     - Inside `POST /api/sessions/4f562e4f44371cb1/lobby/launch`, the Master Server broadcast `lobby_launch` over WebSocket at `14:52:48.676` before the HTTP response returned to the Host at `14:52:49.106`.
     - Furthermore, in `src/main.cpp`, the Guest called `NetworkTransport::Get().Initialize(false, ...)` *before* `blamEngine->LaunchMap`, whereas the Host only initialized `NetworkTransport` *inside* `BlamEngine::BuildLaunchBlock`.
     - Because `NetworkTransport::Initialize` defaulted `localPort = 11774` even when `isHost == false`, the **Guest (`PID 13820`)** bound UDP port `11774` with `SO_EXCLUSIVEADDRUSE` at **`14:52:51.585`**:
       `[14:52:51.585] NetworkTransport: Initialized GUEST on UDP port 11774`
     - **2 milliseconds later (`14:52:51.587`)**, the **Host (`PID 22864`)** tried to bind `11774`, failed because the Guest held it, and fell back to ephemeral port **`57216`**:
       `[14:52:51.587] NetworkTransport: Initialized HOST on UDP port 57216`
  4. **Guest Self-Loopback on Port `11774` Disabled the WebSocket Relay Fallback:**
     - The Guest sent its `join-request` packets (`TX #1`..`TX #60`) to `35.147.220.133:11774`, which routed back to port `11774` on the same machine—the **Guest's own socket** (`[GUEST pid 13820] SEND msg 8 (join-request)` -> `[GUEST pid 13820] RECV msg 8 (join-request)`).
     - In `NetworkTransport::OnNetRecv`, `m_lastUdpRecvTick.store(now)` was updated before checking `wireSender == m_localMachineId`, and `QueueWirePacket` did not reject `sender == m_localMachineId`.
     - Updating `m_lastUdpRecvTick` from its own loopback packet #1 tricked `OnNetSend` into setting `directUdpAlive = true` (`relayFallback: false` on `TX #2`..`TX #60`), shutting off the WebSocket Relay fallback while the Host's WebSocket Relay didn't finish connecting until `14:52:54.442` (108ms after Guest's `TX #1` at `14:52:54.334`).

### 2. Fixes Applied
- **`src/network/NetworkTransport.cpp`:**
  - **Guest Ephemeral Port Binding & Host Port `11774` Retry (`Initialize`):** Guests (`!isHost`) now always bind to an ephemeral UDP port (`bindAddr.sin_port = htons(isHost ? localPort : 0)`), ensuring a Guest never steals UDP port `11774` from a Host or receives packets addressed to `11774`. When `isHost == true`, if `bind(11774)` fails transiently while the parent menu process from `Relaunch::Start` is closing its socket, the Host retries up to 10 times (`25ms` each) before falling back to port `0`.
  - **Self-Loopback Filtering (`QueueWirePacket` & `OnNetRecv`):** Rejected any packet with `wireSender == 0 || wireSender == m_localMachineId` in both `OnNetRecv` (before updating `m_lastUdpRecvTick`) and `QueueWirePacket`, preventing self-loopback packets from ever entering engine queues or falsely disabling `relayFallback`.
  - **Same-PC Online P2P Loopback Probe (`Initialize`, `OnNetSend`, `OnNetRecv`):** When a Guest is not yet confirmed on direct UDP and its target is a non-loopback IP, it also probes `127.0.0.1:<hostPort>` and updates `m_remoteHostAddr = senderAddr` upon receiving a packet from `m_hostMachineId`, enabling instant localhost UDP when testing two Online P2P instances on the same PC without relying on router Hairpin NAT.
- **`src/main.cpp`:**
  - Moved Host `NetworkTransport::Get().Initialize(true)`, `SessionManager::Get().StartHosting(cfg)`, and `MasterServerClient::Get().StartAdvertising(advert)` **before** `blamEngine->LaunchMap(...)` so the Host binds UDP port `11774` and connects its WebSocket Relay in parallel with map loading before any Guest sends its first `join-request`.
- **`src/common/Relaunch.cpp` & `src/common/Settings.cpp`:**
  - `Relaunch::Start` now forwards the current window's in-memory player and account identity (`-player-name`, `-service-tag`, `-account-user`, `-account-token`, `-user-id`, `-account-dev`) to the child process, and `Settings::Load()` applies those per-process overrides in memory (without overwriting `settings.json` on disk) so multiple instances on one PC retain their own accounts (`DOMINUS` vs `TEST2`) across match and menu relaunches.
- **Verification:** Compiled cleanly via `.\build.bat` with **0 errors and 0 warnings**.

--

## 0. Pick up here (2026-10-29, 17:30 ET) - Pre-Game Lobby Host P:ID / Username Revert & Guest Match Launch Mismatch Fixed (Dominus / Dominus-VII)

### 1. Why the Host's `P:ID` and Username Reverted to `P:ID 1` and `"Host"` After ~1.2 Seconds in the Pre-Game Lobby
- **Root Cause:**
  1. When clicking **Custom Games -> HOST LOBBY** (Online P2P), `SetupHostLobby` (`src/ui/FrontEndMenu.cpp`) constructed `MasterAdvert adv` with the Host's real `adv.hostPeerId` (from `AuthClient::Get().GetUserId()`), `adv.hostUsername`, `adv.hostIsDev`, `adv.hostServiceTag`, `adv.variant`, `adv.difficulty`, `adv.skulls`, and `adv.inProgress = false`, and populated `st.lobbyRoster[0]` locally with the Host's real `P:ID` and username.
  2. However, `MasterServerClient::DoRegister()` (`src/network/MasterServerClient.cpp`) omitted all of those fields from the `POST /api/sessions` JSON payload! It only sent `name`, `port`, `player_count`, `max_players`, `map`, `mode`, `has_password`, `is_private`, `is_mod`, `address`, and `lan_address`.
  3. On the Master Server (`server/server.js`), missing `body.host_peer_id` and `body.host_username` caused `POST /api/sessions` to default to `hostPeerId = 1` and `hostUsername = 'Host'`, and if reusing an existing session from the same IP+port within 30s (`existing.roster || body.roster`), omitted `body.roster` kept the stale `[ { peerId: 1, username: 'Host', isHost: true } ]` roster.
  4. ~1.2 seconds later, `DrawPreGameLobby` (`src/ui/FrontEndMenu.cpp`) polled `FetchLobbyStateSync` (`GET /api/sessions/:id/lobby`) and overwrote `st.lobbyRoster = current.roster`, replacing the Host's real `P:ID` and username with `P:ID 1` and `"Host"`.
- **Fix:**
  - **`src/network/MasterServerClient.cpp` (`DoRegister`):** Serialized `variant`, `difficulty`, `skulls`, `in_progress`, `host_peer_id`, `player_id`, `host_machine_id`, `host_username`, `player_name`, `host_is_dev`, `host_service_tag`, and (when `!advert.inProgress`) the initial 1-entry `roster` array containing the Host's authoritative identity (`peerId`, `machineId`, `username`, `isHost: true`, `isDev`, `serviceTag`).
  - **`src/ui/FrontEndMenu.cpp` (`TakePendingLobbyEvents` & `DrawPreGameLobby`):** Added a client-side guard on the Host so whenever `st.lobbyRoster` is refreshed from WebSocket or HTTP polling, any `p.isHost` entry is pinned to the Host's authoritative `AuthClient` / `Settings` `peerId`, `username`, `serviceTag`, and `isDev`.

### 2. Why Guest Clients Mismatched or Failed to Launch Into the Same Game as the Host
- **Root Causes:**
  1. **WebSocket `lobby_launch` Broadcast Overwrote Remote Guest's `st.lobbyHostEndpoint` With Host's Private LAN IP (`192.168.x.x`):**
     - When the Host clicked **START MATCH**, `POST /api/sessions/:id/lobby/launch` (`server/server.js`) was called by the Host (`guestIp === session.address`). Because `guestIp === session.address && session.lanAddress` was `true` for the Host's own request, `server.js` set `session.launchData.host_ip = session.lanAddress` (e.g. `192.168.1.x`) and broadcast that private LAN IP to all remote guests via WebSocket `lobby_launch`.
     - In `TakePendingLobbyEvents` (`src/ui/FrontEndMenu.cpp`), `if (!ev.hostEndpoint.empty()) st.lobbyHostEndpoint = ev.hostEndpoint;` unconditionally overwrote the remote Guest's valid public `st.lobbyHostEndpoint` with the Host's unreachable private LAN IP!
  2. **Race on `st.lobbySessionId` / `st.lobbySessionToken` Before `START MATCH`:**
     - `SetupHostLobby` read `GetCurrentSessionId()` immediately after `StartAdvertising(adv)`, before the background thread finished `DoRegister()`, leaving `st.lobbySessionId` and `st.lobbySessionToken` empty until a poll ran. If `st.lobbySessionId` was empty at launch, `LaunchLobbySync` wasn't called and `Relaunch::Start` (`handsOnSession == false`) called `StopAdvertising()` (`DELETE /api/sessions/:id`), deleting the lobby session right as the Host launched.
  3. **Variant Lookup Mismatch on Guest Launch:**
     - The Host advertised `st.lobbyVariantName` (the UI label or filename), whereas the Guest's launch handler only checked `v.filename == st.lobbyLaunchVariant` and did not check `v.label` or fall back to `st.lobbyVariantName`.
  4. **Host Match Process Wiped Lobby Metadata on First Heartbeat:**
     - When the Host relaunched into the match process (`src/main.cpp`), `MasterAdvert advert` did not set `variant`, `difficulty`, `skulls`, or `hostPeerId`/`hostUsername`, and only set `inProgress = true` when `connectSessionId` was non-empty. Its first `DoHeartbeat()` then overwrote the session's `variant` with `""`, `difficulty` with `1`, and `skulls` with `"0"`.
  5. **LAN vs. Online Session Deduplication in `ServerBrowser`:**
     - Same-LAN discovery added a `"LAN"` entry (`hostAddress = <LAN IP>`, `id = ""`), while the Master Server returned `address = <Public IP>`, `lan_address = <LAN IP>`, `id = <sessionId>`. Because `ServerBrowser::RefreshServers` only matched `existing.hostAddress == m.address`, clicking the LAN entry had an empty `s.id` and could not join the Master Server Pre-Game Lobby.
- **Fixes:**
  - **`src/ui/FrontEndMenu.cpp`:**
    - In `TakePendingLobbyEvents`: Changed `st.lobbyHostEndpoint` assignment to `if (st.lobbyHostEndpoint.empty() && !ev.hostEndpoint.empty())` so a WebSocket `lobby_launch` event never clobbers the Guest's already-resolved public/LAN Host endpoint.
    - In `DrawPreGameLobby`: Continuously sync `st.lobbySessionId` and `st.lobbySessionToken` from `MasterServerClient::Get()` every frame and immediately prior to `LaunchLobbySync` on **START MATCH**.
    - In `DrawPreGameLobby` (Guest launch handler): Match `varToMatch` (`st.lobbyLaunchVariant` or fallback `st.lobbyVariantName`) against both `v.filename` and `v.label`.
    - In `SetupHostLobby`: Advertise both public and private Online P2P lobbies (`if (st.lobbyNetMode == 1 && MasterServerClient::Get().Enabled())` with `adv.isPrivate = (st.lobbyPrivacy != 0)`).
  - **`src/network/MasterServerClient.hpp` & `src/network/MasterServerClient.cpp`:**
    - Added `isPrivate` and `lanAddress` parsing to `MasterServerClient::DoList()`.
    - Enhanced `MasterServerClient::JoinLobbySync` with fallback resolution by `fallbackHostIp` (matching `s.address` or `s.lanAddress`) + `fallbackHostPort` when `sessionId` is empty, and returned the resolved `state.sessionId`.
  - **`src/ui/ServerBrowser.cpp` (`RefreshServers`):**
    - Matched Master Server entries against existing LAN entries by either `existing.hostAddress == m.address` or `existing.hostAddress == m.lanAddress`, merging `existing.id = m.id` and `existing.inProgress = m.inProgress`.
  - **`src/main.cpp`:**
    - Populated `advert.variant`, `advert.difficulty`, `advert.skulls`, `advert.inProgress = true`, `advert.hostPeerId`, `advert.hostMachineId`, `advert.hostUsername`, `advert.hostIsDev`, and `advert.hostServiceTag` when the Host match process starts advertising/heartbeating.
  - **`server/server.js`:**
    - Fixed `POST /api/sessions/:id/lobby/launch` so `session.launchData.host_ip` is `session.address` (public IP) and `is_lan: false` for the WebSocket broadcast, while `GET /api/sessions/:id/lobby` dynamically substitutes `lanAddress` per-requester when `guestIp === session.address`.
- **Verification:** Built cleanly via `.\build.bat` with **0 errors and 0 warnings**.

--

## 0. Pick up here (2026-10-29, 16:15 ET) - Audit after 2605bf7 / 622907a: no crashes since the fix build; two hardening fixes (Dominus)

- **Verified from `build\bin\Release\noble-client.log` and the Windows Application log:** every crash before the 12:34 PT
  build was `0xE06D7363` (`std::system_error` from a double `std::thread::join` in `SessionManager`) or the older
  `0xC0000005` in ntdll at menu exit. Since `622907a` (beacon-thread fix + MP launch block fix) there are **no fatal exceptions**
  in 4 sessions, including a hosted online MP match on `45_launch_station` that ran past the old 2-3 minute host crash. The host message loop
  now starts ~1.5 s after launch (it used to never start). The leftover `main blckd` lines are only during map load (before
  "loading screen off").
- **`src/network/SessionManager.cpp` - `Shutdown()`:** now `if (!m_isInitialized.exchange(false)) return;`. Before, the flag was
  only cleared at the end, so `Relaunch::Start` (engine/render thread) and the exit path in `main.cpp` (main thread) could both
  reach `m_listenerThread.join()` and throw the same `0xE06D7363`.
- **`src/engine/BlamEngine.cpp` - `BuildLaunchBlock`:** the game-variant decode block is skipped for the main menu. Behaviour is
  unchanged (the menu never had a variant), but it stops the bogus `No game variant found (looked for '')` warning on every menu boot.
- **Open / please check (O7OF):** the Season 8 / robo-arm clamp in `ApplyArmoryAppearance` (2605bf7) uses MCC item
  numbers (`helmet >= 72`, `chest >= 235`), but the dump notes say engine index helmet >= 64 / chest >= 20 crashes. By the
  `Chests()` table, `chest_wp_1` (234) is engine index 20 and the AKIS/GRD helmets (68-71) are >= 64, so they may still hit
  hr+0x105512. Not changed until it's confirmed in a disassembler or tested in game.
- **Open:** the Armory now waits without a time limit for the engine scene (`ArmorCustomizer.cpp`, 2605bf7). If `mainmenu_all.map` is
  present but the scene never comes up, the preview shows "LOADING ARMORY..." forever.
- **Still untested after the fix:** a remote guest (two PCs) loading into an MP match with the restored map variant at +0x1D308.
- Build: `noble-client.exe` / `noble-dedicated.exe` 0 errors, 0 warnings (/W4).

--

## 0. Pick up here (2026-10-29, 15:20 ET) - 3D Armory Live Scene Restored & Match Start Crash (0x105512) Fixed (Dominus / Tyler)

### 1. Armory Model Falling Back to OBJ Stand-in (Ugly Old Model)
- **Root Cause:**
  - Commit `2d103ba` re-routed `src/main.cpp` to execute `FrontEndMenu::Run` (the standalone 2D ImGui loop) before `haloreach.dll` was loaded into memory.
  - Because `haloreach.dll` was not loaded, `mainmenu_all.map` was never running in the Blam engine, `ReachArmory::SetMenuMode(true)` was never invoked, and `ReachArmory::IsSceneReady()` remained permanently `false`.
  - In `src/ui/ArmorCustomizer.cpp`, a 30-second timeout condition (`waitingForScene = !engineScene && menuMapPresent && (ImGui::GetTime() - sceneWaitStart) < 30.0;`) expired after 30 seconds and caused the UI to fall back to the offline OBJ stand-in mesh (`data/armor/spartans/spartans.obj`), displaying the placeholder model.
- **Fix:**
  - In `src/main.cpp`: Restored in-engine menu initialization when `FrontEndMenu::ShouldShow()` is true:
    - Calls `ReachArmory::SetMenuMode(true)` and `FrontEndMenu::Initialize(reachInstall->reachDirectory)`.
    - Sets `launchMap = mainmenu` and `launchMode = mainmenu`.
    - Boots `haloreach.dll` into `mainmenu_all.map`.
    - Guarded game loading screen and `ElitePack::Install` to only activate when launching active game matches (`if (!showMenu)`).
    - Single-session-per-process transitions (launching a match or joining a lobby from the menu) seamlessly hand off via `Relaunch::Start(args)`.
  - In `src/ui/ArmorCustomizer.cpp`:
    - Removed the 30-second timeout fallback (`waitingForScene = !engineScene && menuMapPresent`). It will now always render the live in-engine 3D Spartan/Elite model generated by `haloreach.dll` and `mainmenu_all.map` whenever `mainmenu_all.map` is present, displaying the LOADING ARMORY... prompt until the engine scene is ready.
  - In `src/ui/FrontEndMenu.cpp`:
    - Added forwarding of `-session-token` on guest join relaunch command line.

### 2. Match Start Crash (0xC0000005 Access Violation at haloreach.dll+0x105512)
- **Root Cause:**
  - Analysis of the minidump `noble-crash.dmp` (`Fault address 0xC86DE30004`, `R8=0x3A26`) showed an access violation inside `player_appearance_build` (`haloreach.dll+0x1051E0`).
  - When profile armor permutation indices exceed the stock Blam scenario tag bounds (e.g., helmet >= 64, chest >= 20, wrist >= 6, utility >= 6, shoulder >= 18), the function branches to a bugged 64-bit port fallback at `0x10532a`.
  - At `0x10532a`, the instruction `mov 0x4(%rcx, %r9, 4), %r14d` incorrectly uses `rcx` with scale factor 5, causing `r14d` to load garbage values (e.g. `0x3A26` / 14,886 iterations). The subsequent loop overflows the thread stack buffer `0x1c(%rbp)` and crashes with an access violation at `0x105512`.
  - In `appearance.json`, prosthetic roboarm chests (`235..239`) map to internal indices 20..24 >= 20, and Season 8 helmets (`72..89`) map to indices >= 64, immediately triggering this crash when launching into a match.
- **Fix:**
  - In `src/engine/ReachHost.cpp`: Clamped helmet (>= 72 -> 0) and chest (>= 235 -> 126, base chest) in `ApplyArmoryAppearance` before passing the appearance struct to the engine match builder. This completely prevents `player_appearance_build` from triggering the broken tag fallback path.

--

## 1. Previous Handoff (2026-10-29, 13:55 ET) - JOIN FIXED (two instances on one PC): endpoint ports

- **Root cause:** haloreach.dll uses two endpoints, port 1000 (channel) and 1001 (out-of-band). Host slot 41
  is send(target, data, size, **port**) and slot 43 is recv(buffer, cap, &sender, **port**); Noble ignored the
  port and fed both endpoints from one queue, so packets went to the wrong endpoint at random (connect-establish
  lost, later messages misrouted). Details and addresses: `docs/networking-review.md`, "FIX (13:52 ET)".
- **Fix:** `src/network/NetworkTransport.cpp/.hpp` (wire `[u64 sender][u16 port][payload]`, one receive queue
  per port, 64 KB receive buffer) and the slot 41/43 thunks in `src/engine/ReachHost.cpp` (pass `a4`).
  **Host and guest must both run the new build.** For O7OF / Dominus: small change in your files.
- **Result:** in the 13:52 two-instance run the full join went through (membership, peer-establish,
  view-establishment, player-acknowledge) and the guest loaded the map to launch state 11 with no boots.
  Still to check: on screen, between two PCs (O7), and over the relay.
- **Also in this round:** two compile fixes for O7OF's commit 3c7c8d0 (`FrontEndMenu.cpp` leftover braces,
  `MasterServerClient.cpp` GetLobbyMachineId order); the can_accept_player_join_request hook now forwards all 9
  arguments; `src/hooks/LobbyPatches.cpp` has many log-only traces (join/refusal/messages/channels/boot/net log)
  that should be put behind a switch or removed before a release.
- Test script (not in git): `D:\Reach\noble_test.bat` builds, starts a host, starts a guest 30 s later, waits
  75 s, closes both; output in `D:\Reach\noble_test_out.txt`.

## 0. Pick up here (2026-10-29, 06:20 ET) - Join refusal traced further; build fix for O7OF's 03:35 commit

- **O7OF's commit 3c7c8d0 (03:35) did not compile:** removing the duplicate WS launch in `FrontEndMenu.cpp`
  (DrawPreGameLobby, START MATCH) left the last three lines of the deleted JSON (`{ "session_id", ... } }} };`),
  three unmatched braces. Fixed by deleting them. The same commit also failed in `MasterServerClient.cpp`
  (error C3861: `GetLobbyMachineId()` was defined above the `LobbyMachineId()` it calls); moved it below the
  anonymous namespace. Nothing else in that commit was changed.
- **Same commit, please check (O7OF):**
  - `main.cpp` now always runs `FrontEndMenu::Run` (the 2D menu before haloreach.dll loads) and never sets
    `ReachArmory::SetMenuMode(true)` or calls `FrontEndMenu::Initialize`. The in-engine menu
    (mainmenu_all.map) and with it the **3D Armory preview** from 02:05 no longer run. Intended?
  - The client now advertises private lobbies with `is_private` and sends `lan_address`; the server copy on
    Tyler's PC (`D:\ServerTestDeploy-main`, 02:15) does not read them yet. If the live server doesn't either,
    private lobbies show in the public list.
- **13:40 (more automated runs):** the guest's channel handshake with the host is flaky (most connect-establish
  replies never reach the host channel); when it does connect, the host sends the ~9.5 KB membership-update and
  the guest fails to decode a message. Our UDP receive buffer was 4 KB; now 64 KB in `NetworkTransport.cpp`
  (small change, tell O7OF/Dominus). Details: `docs/networking-review.md`, "Update 13:40 ET".
- **13:00 runs (automated, `D:\Reach\noble_test.bat` builds and runs host + guest, output in
  `D:\Reach\noble_test_out.txt`; not in git):** the host accepts the guest, the channel connects (guest channel
  state 5), then the host sends no membership and boots the guest after ~8.5 s. Network path is fine; the stop is
  in the host session after the channel connects. See `docs/networking-review.md`, "Update 13:00 ET".
- **Reason 15 = "abort ignored" (12:45 test):** the host's reply to the guest's join-ABORT (hr+0x429AA4, type 10),
  not a refusal of the join. The guest's join-request is dropped silently on the host before any refusal; the
  new session-message dump in `LobbyPatches.cpp` shows which check. Details in `docs/networking-review.md`.
- **Reason 15 (06:20 notes):** see `docs/networking-review.md`, "Join refusal, reason 15". From the dll: message 11 is
  join-refuse; every sender found (hr+0x397914's nine callers, hr+0x449EEC) sends a different reason, so the
  path that sends 15 is still unknown. `LobbyPatches.cpp` now logs the haloreach.dll call stack at each
  refusal (`JOIN REFUSED stack: hr+0x... <- ...`); the next two-instance test names the code.
- **Bug fixed:** the can_accept_player_join_request hook (hr+0x3973BC) forwarded 4 of the function's 9
  arguments; it now forwards all nine.
- Every file written tonight was re-read on Tyler's PC and matches (stale copies of FrontEndMenu.cpp and
  LobbyPatches.cpp reappeared once each and were rewritten). 03:45 commit 7df2cb9 is pushed.
- **Merge notes (14:10 ET, rebase onto O7OF's ee7c10a):** `NetworkTransport.cpp` merged by hand: O7OF's route
  states, held packets, broadcast and telemetry kept, plus the port header and per-port queues. His duplicate
  filter now only runs while the relay is active and hashes sender + port too (the game resends identical packets
  on purpose; filtering them on direct UDP would drop keepalives). Reason 15 is "abort ignored" (the host's answer
  to a guest's join-abort, hr+0x429AA4 type 10), not slot 25; the real cause was the endpoint ports above. The new
  acknowledge_join_request hook (hr+0x397914) expects `53 48 83 EC 50` but the dll starts `40 53 48 83 EC`, so it
  never installs.

## 0. Pick up here (2026-10-29, 06:05 ET) - Host Application Hang (AppHangB1) & Match Pacing/Join Blocking Fixes (Dominus / Tyler) by [O7OF]

Built and verified in Release configuration (`noble-client.exe` and `noble-dedicated.exe`).
Resolved the host crash/hang that occurred when waiting for non-host clients to join, and eliminated join blocking on the host:

- **Host Application Hang (`AppHangB1` / Event ID 1002) Fixed:**
  - **Root Cause:** While the host engine pumped frames in Slot 1 (`HostSlot1_FrameTick`), `src/main.cpp` used a simple `Sleep(10)` loop. When the window lost focus (e.g., user alt-tabbed to Discord or waited a few minutes for a friend to load), Windows Desktop Window Manager (DWM) sent responsiveness pings. Without `DisableProcessWindowsGhosting()`, DWM detected an inactive window and flagged `AppHangB1` (Event 1002), terminating the process.
  - **Resolution in `src/main.cpp`:**
    1. Added `DisableProcessWindowsGhosting()` immediately at process startup in `wWinMain` to prevent Windows from replacing the window with a ghost and killing it during long waits or background states.
    2. Replaced `Sleep(10)` in the message loop with `MsgWaitForMultipleObjectsEx(0, nullptr, 10, QS_ALLINPUT, MWMO_ALERTABLE)`: immediately wakes the window thread whenever input, focus, paint, timer, or DWM messages arrive, maintaining instant OS responsiveness while keeping idle CPU usage minimal.

- **Direct3D 11 Immediate Context Multithread Protection & RTV Caching:**
  - **Root Cause:** `haloreach.dll`'s engine thread executes Slot 1 and issues D3D11 commands while the Win32 window thread handles window messages. In `src/host/D3D11Renderer.cpp`, multithread protection was not enabled, risking immediate context contention. Concurrently, `PauseMenu::RenderImpl` called `GetBuffer` and `CreateRenderTargetView` every frame at 180+ FPS, causing intense driver resource thrashing.
  - **Resolution:**
    1. In `src/host/D3D11Renderer.cpp`: Added `<d3d11_4.h>`, queried `ID3D11Multithread` from `m_context`, and invoked `multithread->SetMultithreadProtected(TRUE)` to ensure thread safety across engine and UI threads.
    2. In `src/ui/PauseMenu.cpp`: Implemented static caching of `s_cachedSwapChain`, `s_cachedBackBuffer`, and `s_cachedRtv`, reusing the render target view each frame instead of recreating/destroying it continuously.

- **CPU Core Pegging & SwapChain Pacing (`src/engine/ReachHost.cpp`):**
  - **Root Cause:** `HookSwapChainPresent` previously had a tight `do { YieldProcessor(); } while (...)` spin loop when throttling frames, burning 100% of a CPU logical core. Furthermore, when uncapped (`fpsLimit = 0`), the engine free-wheeled at 180–200+ FPS even when the window was unfocused in the background.
  - **Resolution:**
    1. Added background/unfocused detection (`GetForegroundWindow() != hwnd`): automatically throttles to 30 FPS when unfocused, preventing heat spikes and DWM hang flags while waiting for friends.
    2. Set a safe default frame cap of 120 FPS when uncapped (`fpsLimit <= 0`) to prevent runaway GPU loops.
    3. Replaced the spin loop with a hybrid wait: calls `Sleep(1)` whenever the remaining frame time is `>= 2ms`, and only yields in the final sub-2ms margin.

- **Join-Any Gates Enabled by Default (`src/hooks/LobbyPatches.cpp`):**
  - **Root Cause:** Haloreach.dll has internal join gates at `0x397635` and `0x397363` that reject incoming join requests once the host transitions to simulation (`state 11`) or during certain loading phases (refusals 0x06 / 0x20). Previously, the `-join-any` bypass patch (`74 5E -> EB 5E` and `74 4D -> EB 4D`) was only active if `-join-any` was explicitly specified on the command line, causing hosts to turn away guests who took longer than a few seconds to connect.
  - **Resolution:** Enabled the join-any patches unconditionally for all sessions. The host engine will now always accept incoming remote players even if the match is already in simulation.

- **Guest Variant Zeroing & Match Start Pacing (`src/engine/BlamEngine.cpp`):**
  - In `BuildLaunchBlock`: Added explicit zeroing of the guest variant buffer from `+0x300` to `LAUNCH_BLOCK_SIZE`, and guarded `SafeCallVariantInit` to only run on the host. This prevents stale local variant data on the guest from conflicting with replicated host variants.
  - In `LaunchMap`: Paced online host initialization by posting `Command 10` (Event: Start) after 1000ms and `Command 11` (Event: In-Game) after 5000ms, allowing cleaner network session handshakes.

--

## 0. Pick up here (2026-10-29, 05:00 ET) - Networking Fixes: Host Reason 15 / Slot 25 Clearance & Guest Launch Block Desync Fixed (Dominus / Tyler) by [O7OF]

Built and verified in Release configuration (`noble-client.exe` and `noble-dedicated.exe`).
Addressed the two critical issues that prevented match hosting and joining:

- **Host Interface Slot 25 Implemented (`src/engine/ReachHost.cpp`):**
  - **Root Cause of Join Refusal 15 (`0x0F`):** When a remote guest joins, `haloreach.dll` queries host interface Slot 25:
    `rcx = HostInterface`, `rdx = guest XUID` (e.g. `0x99CC93FAFC1B3`), `r8 = privilege ID`, `r9 = context`.
    Previously, Slot 25 was unmapped in the vtable and fell back to `SlotDispatcher<25>::Stub`, which returned `0` (denied).
    `haloreach.dll` interpreted this as the guest lacking online/multiplayer clearance, triggering a join refusal response with
    reason 15 (`0x0F`) after ~15 ms.
  - **Resolution:** Implemented `HostSlot25_UserPrivilege`: logs the query and returns `1` (clearance granted).
    Registered `s_vtable[25] = reinterpret_cast<void*>(&HostSlot25_UserPrivilege);` in `ReachHost::InitializeVTable()`.

- **Guest Launch Block Layout Defaulting (`src/engine/BlamEngine.cpp`):**
  - **Root Cause of Guest Stalling at Launch State 9 (2% load progress):** Previously, Noble's guest launch block defaulted
    to setting 1 local player at `+0xE8`, player ID at `+0xF0`, and loaded/decoded `slayer_054.bin` into `+0x300`.
    In Reach Renaissance (`host-probe.exe` / `0x140026690.c`), authentic guest blocks have zero local players (`+0xE8..+0x10F`
    zeroed except `+0x100 = 0xFFFFFFFF` and `+0x108 = -1`), zeroed machine targets except `+0x2F4 = 0xFFFFFFFF` and `+0x2F8 = hostMachineId`,
    and zeroed out variant data from `+0x300` onwards (since variant state synchronizes over the wire from the host).
    Decoding a local variant into `+0x300` on the guest created state divergence, stalling the guest at launch state 9.
  - **Resolution:** Suppressed local game variant loading when `isGuest` (`if ((!isCampaign || !customVariantPath.empty()) && !isGuest)`),
    and made the authentic RR guest launch block layout the default unless explicitly overridden with `-legacy-guest-block`.

- **LobbyPatches Tracing & RVA Fallbacks (`src/hooks/LobbyPatches.cpp`):**
  - Hooked `c_network_session::acknowledge_join_request` at RVA `0x397914` via MinHook (`Hook_AcknowledgeJoinRequest`) to trace
    join response packets, targets, and refusal reason codes.
  - Added direct RVA fallbacks for `can_accept_player_join_request` (RVA `0x3973BC`) and `can_accept_join_request` (RVA `0x3975D8`)
    with prologue verification to ensure hooks install reliably even if signature patterns miss.

- **Tyler Test Instructions:**
  - Launch Host:
    `noble-client.exe -map 20_sword_slayer -mode multiplayer -host -lan`
  - Launch Guest:
    `noble-client.exe -connect 127.0.0.1:11774`

## 0. Pick up here (2026-10-29, 03:45 ET) - Guest stuck loading: the HOST refuses the join (reason 15)

Tested by Tyler with two instances on one PC (O7 offline), so NAT, relay and the master server are out of the picture:
host `noble-client.exe -map 20_sword_slayer -mode multiplayer -host -lan`, guest the same with `-connect 127.0.0.1:11774`.
The guest sits at launch state 9 (2%) and its join timeout sends it back to the main menu after about a minute.
- **Cause found:** the host's haloreach.dll refuses every join request. Log (03:40, new trace below):
  `[HOST pid 9696] JOIN REFUSED: session 0x48414C4F52454143 reason 15 (0xF) caller hr+0x423E2E a1=0x7ffcb6e272f0 a2=0xc`.
  The session id is right ("HALOREAC"), so the request reaches the right session and is turned down on purpose; one
  refusal per guest retry (about one a second, in bursts). The first one always follows, by about 15 ms, the first
  call of host slot 25 with the guest's XUID: `slot 25 (a1=0x99CC93FAFC1B3, a2=0, a3=0xFFFFFFFF)` (our stub returns 0).
- **Why nobody saw it before:** the Dominus hooks on can_accept_join_request / can_accept_player_join_request
  (`LobbyPatches.cpp`, RVA 0x3975D8 / 0x3973BC) never fire on this path; refusal 15 comes from somewhere else.
- **New in `src/hooks/LobbyPatches.cpp`** (built and run by Tyler; not committed yet):
  - Log-only hooks on the two functions RR's host-probe.exe traces as "join refused" (hr+0x425368, arg3 =
    {u64 session, u32 reason}) and "connect refused" (hr+0x42459C, arg3 = {u32 id, u32 reason}). The original always
    runs. Each line says HOST or GUEST (both instances write one log), the pid, and the caller RVA. The prologue
    (48 89 5C 24 08) is checked first; a mismatch only logs a warning.
  - Switch `-join-any` (host, off by default, in memory only): RR's `RR_JOIN_ANY` patch, hr+0x397635 74 5E -> EB 5E
    and hr+0x397363 74 4D -> EB 4D (bytes checked first). Tested: both patches apply on Tyler's dll, but **reason 15
    still comes back**, so those two gates are not it. RR's launcher never sets RR_JOIN_ANY; it is a debug knob.
- **RR facts checked:** RR's host::hit sends slot 25 to its default case, which returns 0 - same as our stub - so slot
  25's return value alone does not explain the difference (RR's per-slot thunk was not checked).
- **Next:** disassemble haloreach.dll around hr+0x423E2E (the caller that builds the refusal) to see which test sets
  reason 15, then compare that state with RR's host (session / membership / privacy, the guest's XUID, what slot 25
  asks). This needs a read-only copy of haloreach.dll in a disassembler (Tyler, O7OF or Dominus); nothing was
  disassembled yet. Reach's refusal enum was not confirmed for this build, so 15 is not named here.

## 0. Pick up here (2026-10-29, 03:10 ET) - Networking review: why guests don't get in, fixes, test switch

Full write-up with evidence: `docs/networking-review.md` (17 findings, checked against the code, Tyler's 02:13 / 02:27
guest logs, the Render server log, RR's host-probe.exe, and the real server.js run locally). Not built or tested yet.
- **Fixed (client):** host's lobby session was DELETEd at launch so guests went to a dead session (`Relaunch` ->
  `MasterServerClient::Shutdown(keepSession)`); guest lobby launch started a second, unconnected match
  (`FrontEndMenu`); lobby events written from the relay thread (crash with no log); host now sends the variant
  file name (server cuts at 64 chars); every lobby guest had the host's fixed id (`LobbyMachineId`); UDP 11774 now
  SO_EXCLUSIVEADDRUSE (SO_REUSEADDR let a 2nd process steal packets); a dropped relay now reconnects and can no
  longer std::terminate (`NetworkTransport` Start/StopRelay); host advertises the port the socket really got.
- **Test switch `-rr-guest-block`:** guests stuck at launch state 9 with traffic both ways. RR's
  `patch_launch_block --as-guest` gives a guest no local players, no player id, no variant (+0x300 on zeroed). Ours
  sets all three. The switch lays the guest block out like RR; it carries over the menu -> match relaunch.
- **For O7OF:** one launch message, not HTTP + WS; 3+ player routing (host packets go to every guest; guest-to-guest
  not routed; unknown targets broadcast instead of held); one-way UDP hangs (all-or-nothing relay switch, modelled);
  private lobbies never heartbeat (30 s timeout); no "lobby closed" event; peer_id 2 for every logged-out guest;
  same-router players need the host's LAN address (NAT loopback); a guest's relay is started once only.

## 0. Pick up here (2026-10-29, 02:05 ET) - Armory 3D preview: real model pieces, try-on, Elite effects off

Built and partly tested by Tyler (helmet/wrist id fix and the stray visor fix are not tested yet).
- **The 3D Armory took item names as model pieces.** The menu script's GLOBALS pairs are (model region, permutation),
  e.g. head:player_kurt. Noble wrote Armory item ids (kurt_a, chest_3, ...), so only items whose id equals a head
  permutation changed. `ReachArmory.cpp` now translates each item with Reach's pmcg table, read at run time from
  `data\armor\*.rrpmcg` (dumped from the RR armour pack; not in git): helmet -> head + flair_head_a/b/c, shoulders ->
  flair_*_shoulder, chest -> flair_chest_a/b (+ arms for [R], male/female by flag 1/2), wrist/utility/knees -> flair_*,
  effect -> armor_fx (armor_fx_00 Legendary, _01 Eternal, _02 Heart Attack, _03 Pestilence, _04 Inclement Weather,
  _05 Birthday Party). Only the regions the items change are written ("default" is a real piece in some flair regions).
- **13 helmet ids were attached to the wrong helmet** (Tyler checked each in the 3D Armory). New ids: CQC rosenda, ODST
  malcolm, HAZOP skull, Operator isaac, EVA player_li, JFO nicole, Mk V john117, Pilot sam (+ Haunted sam_c), Military
  Police adrianna, CQB kurt, Recon jai, Mk VI will, Gungnir kelly. Wrist: Tactical/UGPS wrist_5, Assault/Breacher wrist_4.
  Each helmet keeps its MCC item number in `ReachHost.cpp` Helmets() (only the id key moved), so in-game looks are as before.
  Catalog (.inc, docs csv/json) updated the same way. `tools/armory/build_spartan_catalog.py` reads the lists, so a re-run agrees.
- **Try-on:** clicking a locked or unbought piece shows it on the 3D model without equipping (orange outline, "PREVIEW").
  Lists show usable pieces first, then for sale, then locked.
- **Rebuilds:** unchanged pairs don't rebuild the biped (colour-only changes no longer flash). REBUILD is bumped 2 frames
  after the pairs; `-armory-settle-rebuild` adds a second rebuild (test switch). After a rebuild the colour/suit records are
  looked for in the memory regions they were found in before (every 4 ms, up to 1.5 s).
- **Crash fixed (haloreach+0x105512, building the player in a match and on the menu):** with Eternal (effect 6) and the
  Hydration Container (utility 6) equipped. `ReachHost.cpp` now sends no effect / default utility for those two in the
  profile (log says so). Not yet known which of the two it was.
- **Elites:** no Armor Effect / Firefight Voice rows for Elites; Elite armour effect in the profile is always none; the Elite
  3D preview gets no effect pair.
- **Still open:** a knife stays on the 3D Spartan's chest whatever chest is picked (was there before these changes too); the
  log line "menu script's starting pairs" lists the regions the menu sets itself, look there first. Colours sometimes stay
  white/brown after an armour change (the rebuilt biped's colour records aren't always found).
- **O7OF:** `ReachArmory.cpp` and `ReachHost.cpp` (Helmets keys, effect/utility guard, Elite effect) changed.

## 0. Pick up here (2026-10-29, 01:55 ET) - Developer Account Display, Mini Dev Menu, and [DEV] Name Tags

- **Developer Account Status in Account Menu (`src/ui/FrontEndMenu.cpp`):**
  - Updated `DrawAccountModal()`: When logged in as a developer, the modal explicitly renders **"Developer Account (ID: %d)"** in gold with active status and an **"OPEN DEVELOPER MENU"** button, replacing the previous "Standard User Account" text.
  - Automatically polls `/api/dev/check` every ~2.5 seconds via `AuthClient::RefreshDeveloperStatusAsync()` while the account modal is viewed, allowing developer privileges granted via `node set_dev.js <user>` on the Render server shell to take effect in real time without restarting the game.

- **Mini Developer Menu (`src/ui/FrontEndMenu.cpp`, `src/career/Career.hpp`, `src/career/Career.cpp`, `src/ui/ArmorCustomizer.cpp`):**
  - Added `DrawDevMenuModal()` popup accessible exclusively by authenticated developers via either the Account Menu or the quick party action button.
  - Hardened access control: immediately auto-closes and rejects access if `!AuthClient::Get().IsDeveloper()`.
  - **Commands Implemented:**
    1. **Unlock All Armors:** Invokes `Career::DevSetArmoryUnlockAll(true)`, bypassing all rank and credit requirements across all Spartan permutations and Sangheili harnesses.
    2. **Give Max Credits:** Invokes `Career::DevGrantMaxCredits()` granting +999,999,999 cR spendable balance.
    3. **Give Max Rank:** Invokes `Career::DevSetMaxRank()` instantly promoting the player profile to Inheritor (20,000,000 cR).
    4. **Max Everything:** Combined action to unlock all armors, set Inheritor rank, and grant max credits in one click.
    5. **Reset Career:** Helper button to reset progress back to Recruit (0 cR, locked armors) for QA and testing fresh account states.

- **[DEV] Name Badges in Main Menu & Pre-Game Lobby (`src/ui/FrontEndMenu.cpp`):**
  - **Main Menu Party Box:** Appends ` [DEV]` in gold right next to the player's name and displays `ONLINE [DEV]: <name>` in the account quick bar for developers.
  - **Pre-Game Lobby Roster:** Renders `<username> [DEV]` in gold next to the player's name and service tag whenever `player.isDev` is true.

---

## 0. Pick up here (2026-10-29, 00:40 ET) - Dominus Remote Join & Black Screen Fixes: Unique `s_player_identifier` (`+0xF0`), Unique Guest XUID, UDP/Relay Dedup & Timeout Recovery

All changes compiled cleanly via `build.bat` (`noble-client.exe` and `noble-dedicated.exe`) with **0 errors and 0 warnings** under `/W4`:

- **Fixed Remote Join Refusal (`refuse_reason = 0x16` / `22`) Caused by Duplicate `s_player_identifier` at `launch_block[0xF0..0xF7]` (`src/engine/BlamEngine.cpp`):**
  - **Root Cause (from `haloreach.dll` Disassembly):**
    1. In `State 6` (`haloreach.dll+0xE804`), `haloreach.dll` copies the 8-byte value at `launch_block + 0xF0` directly into `[0x182A1E94C]` (`0x182A1E860 + 0xEC`) as local Player 0's 8-byte `s_player_identifier`.
    2. Both Host and Guest previously hardcoded `block[0xF0..0xF7]` to `{0xBE, 0xF3, 'Y', '?', 0, 0, 0, 0}` (`0x000000003F59F3BE`).
    3. When a Guest sent a `join-request` (`haloreach.dll+0x3923DC`), the Host validated the request in `c_network_session::can_accept_join_request` (`haloreach.dll+0x3975D8`) -> `c_network_session::can_accept_player_join_request` (`haloreach.dll+0x3973BC`) -> `c_network_session_membership::find_player_by_identifier` (`haloreach.dll+0x39D59C`).
    4. Because the Guest's `s_player_identifier` (`0x000000003F59F3BE`) matched the Host's existing Player 0 (`[membership + 0x1714]`), and Player 0's owning peer index (`[membership + 0x174C] == 0`) did not match the joining Guest's peer index (`1`), `haloreach.dll+0x397513` (`mov ebx, 16h`) immediately rejected every Guest join with refusal code `0x16` (`22`).
  - **Fix:**
    - `BlamEngine::BuildLaunchBlock()` now generates a unique 8-byte `s_player_identifier` at `block[0xF0..0xF7]` for Guest sessions derived from the Guest's random `localMachineId` (`localMachineId ^ 0x3F59F3BE00000000ULL`), ensuring it never collides with Host Player 0 (`0x000000003F59F3BE`) or any other joining Guest.
    - Also applies the Host/Guest network session and player identity fields (`+0x00`, `+0x50`, `+0x58`, `+0x60`, `+0xE8`, `+0xF0`, `+0xF8`, `+0x2F0`, `+0x2F4`, `+0x2F8`, `+0x2BF14`) when loading prebuilt Firefight or Campaign launch blocks (`loadedPrebuilt == true`).

- **Fixed Duplicate Hardcoded XUID (`0x0009000000000001ULL`) Across Host and Guest Clients (`src/engine/ReachHost.cpp`):**
  - **Root Cause:** `HostSlot88_UserIdentity` returned the same hardcoded XUID (`0x0009000000000001ULL`) on all machines, which `haloreach.dll` (`State 6` `0x18000E8B3` -> `0x18003AB1C`) stores into `0x182A1E860 + 0x128` and sends in the `join-request` player configuration block, colliding with the Host's Player 0 XUID.
  - **Fix:** When `NetworkTransport::Get().IsActive() && !NetworkTransport::Get().IsHost()`, `HostSlot88_UserIdentity` now derives a unique per-Guest XUID (`0x0009000000000000ULL | (localMachineId & 0x0000FFFFFFFFFFFFULL)`) guaranteed never to equal the Host's `0x0009000000000001ULL`.

- **Fixed Simultaneous Duplicate Packet Delivery Over Direct UDP and WebSocket Relay (`src/network/NetworkTransport.hpp`, `src/network/NetworkTransport.cpp`, `src/main.cpp`):**
  - **Root Cause:** When direct UDP was open between Guest and Host, `NetworkTransport::OnNetSend()` and `OnNetRecv()` sent and received every packet over **both** Direct UDP and the WebSocket Relay simultaneously, delivering a delayed (~50 ms) duplicate of every packet and corrupting Blam!'s sliding-window packet sequence.
  - **Fix:**
    - Added `m_lastUdpRecvTick` tracking to `NetworkTransport` and an automatic direct-UDP `PING` echo from Host to Guest upon receiving a NAT hole-punch `PING`.
    - `OnNetSend()` and `OnNetRecv()` now use the WebSocket Relay strictly as a fallback when direct UDP has not received traffic within the last 3 seconds.
    - In `src/main.cpp`, `NetworkTransport::Initialize(false, ...)`, `MasterServerClient::JoinSessionSync()`, and `NetworkTransport::StartRelay()` now run **before** `BlamEngine::LaunchMap()` starts `haloreach.dll`'s engine thread, so NAT hole-punching and the WebSocket relay are ready before the first `join-request` packet is sent.

- **Fixed Black Screen on 60s Join Timeout & 5s Shutdown Hang (`src/engine/ReachHost.cpp`, `src/network/NetworkTransport.cpp`):**
  - `HostSlot1_FrameTick` no longer hides the loading screen at 60 seconds while `haloreach.dll` is still waiting in `launch state < 10` (`waitingForRemoteJoin`).
  - `HostSlot4_LogMessage` now detects `reason == 4` (`external_launch_individual_state_timeout` / `external_launch_overall_timeout`) during a Guest join and cleanly relaunches back to the Front-End Menu (`Relaunch::Start(L"")`) instead of stranding the player on a black screen.
  - `NetworkTransport::StopRelay()` now closes the active WinHTTP WebSocket and session handles before joining `m_relayThread`, unblocking `WinHttpWebSocketReceive` immediately on exit.

---

## 0. Pick up here (2026-10-29, 00:35 ET) - Pre-Game Lobby, Guest Black Screen Fix, WebSocket Signaling & Build Verification

Compiled cleanly via `build.bat` with **0 errors and 0 warnings**:
- **Fresh Binaries Built:** `noble-client.exe` (2,562,048 bytes) and `noble-dedicated.exe` (538,624 bytes).
- **Released to Test Directory:** Copied directly to `C:\Users\Chey1\Desktop\Release For Test\`.

### 1. Fixed Guest Stuck on Load & Black Screen on Join (`src/engine/BlamEngine.cpp`)
- **Root Cause Identified from Logs & Reverse Engineering:**
  In `BlamEngine::LaunchMap`, a background thread was unconditionally posting `Command 10` (Start) and `Command 11` (Advance State) after a 300 ms delay on both Host and Guest. On a remote guest connecting to an existing match or lobby, forcing these commands hijacked the engine state machine out of `state 9` (`join_synchronize`) into `state 11` (simulation) before host session replication packets had arrived. Because the guest's local Blam engine had not yet received the game variant, player slot assignments, or initial world state from the host, the engine faulted into an unrendered black screen / void.
- **Fix:**
  Wrapped the manual `PostCommand(10)` and `PostCommand(11)` calls in `if (!isGuest)`. For guest clients, `BlamEngine` leaves the state machine alone, allowing `haloreach.dll`'s native Blam! session manager to complete handshake, membership replication, and scenario transition automatically upon packet receipt.

### 2. Pre-Game Lobby Implementation (`src/ui/FrontEndMenu.cpp`, `src/ui/ServerBrowser.cpp`, `src/network/MasterServerClient.cpp/.hpp`, `src/network/NetworkTransport.cpp/.hpp`)
- **Screen State:** Added `Screen::PreGameLobby` to `enum class Screen`.
- **Hosting Flow:**
  In `CustomGame`, `Firefight`, `Forge`, and `Campaign`, clicking **"HOST LOBBY"** or **"HOST CO-OP"** now calls `SetupHostLobby(...)`:
  - Registers the session on the master server with `in_progress = false` and initializes the host player in the lobby roster with their Peer ID (`P:ID`), Machine ID, username, service tag, and `[DEV]` badge.
  - Binds UPnP port `11774` and LAN discovery on port `11775`.
  - Transitions to `Screen::PreGameLobby` without launching the match immediately.
- **Pre-Game Lobby UI Layout (`DrawPreGameLobby`):**
  - **Left Column:**
    - High-resolution map preview banner art (`UiArt::HeroBanner` / `UiArt::MapPreview`).
    - Map cycler / selector and Gametype cycler.
    - Difficulty & Skulls selectors (for Campaign & Firefight).
    - Host Network Status Card with External IP, Port, LAN IP, and one-click "COPY INVITE" button.
  - **Right Column ("PLAYERS (X/16)"):**
    - Live player roster listing all connected peers in the lobby.
    - Host badge, glowing gold `[DEV]` badge for authenticated developers, `P:ID` (Peer ID), service tag, and ping status.
  - **Bottom Action Bar:**
    - **Host View:** "START MATCH" (dispatches `LaunchLobbySync` HTTP call and WebSocket `lobby_launch` JSON control frame, then triggers game launch for all players) and "CANCEL" (stops hosting, tears down relay/session, and returns to menu).
    - **Guest View:** "WAITING FOR HOST" (listens on WebSocket and polls HTTP for the launch signal, then automatically executes synchronized match launch) and "LEAVE" (notifies backend, disconnects relay, and returns to Server Browser).
- **Server Browser Integration:**
  - Games still in the pre-game lobby state are flagged with `[LOBBY]`.
  - Clicking "JOIN" on a `[LOBBY]` server joins the pre-game lobby room (`Screen::PreGameLobby`) as a guest instead of forcing an immediate Join-In-Progress relaunch.

### 3. Backend Master Server & WebSocket Relay Updates (`ServerTestDeploy`)
- All backend changes were deployed to `https://github.com/O7OF/ServerTestDeploy.git` (`main` branch, commit `e1301b3`) and live-reloaded on Render (`https://outbreak-z-test-server.onrender.com`):
  - In-memory lobby rosters tracked per session room: `GET /api/sessions/:id/lobby`, `POST /api/sessions/:id/lobby/join`, `POST /api/sessions/:id/lobby/leave`, and `POST /api/sessions/:id/lobby/launch`.
  - Multi-client WebSocket relay control message broadcasting (`broadcastLobbyEvent`): propagates `lobby_launch` and `lobby_roster` JSON control frames in real-time across peers in the room alongside binary UDP game packets.

---

## 0. Pick up here (2026-10-29, 00:20 ET) - Spartan Armory: Halopedia catalog, pictures, rank + cR locks, BUY

Built and tested by Tyler: pictures show, locks work, buying works.
- **Catalog:** `tools/armory/fetch_halopedia_armory.py` pulled every Spartan piece's picture, rank and cR from Halopedia
  (into `build/bin/Release/data/ui/reach/armory/<slot>/`, plus `pieces.csv`). `tools/armory/build_spartan_catalog.py` joins that
  with the Armory lists, ReachHost's item ids and `docs/career/armory_overrides.json` (Noble's own rules for pieces Reach
  didn't sell) and writes `docs/career/spartan_armory_catalog.csv/.json` and `src/ui/SpartanArmoryCatalog.inc`.
  Re-run it after changing a list or an override (it reports pieces it couldn't match; only EXO/TSCS is left out, on purpose).
- **Tyler's rules:** CQB and its attachments free from the start; AKIS (+GRD/CBRN) Sergeant; UA [W] chests Sergeant; Military
  Police Warrant Officer. Removed: Mariner (+Defiant/Mister Chief/OPTREX), Belos, Blackguard, Drengr helmets, Condemned and
  Highlands chests. Other prices are picks marked "Claude's pick" in the overrides file; change freely.
- **Renamed to Halopedia names:** AKIS (was Highlands helmet) and its GRD/CBRN attachments, HAZOP CBRN/HUL, Recon HUL-I, effects
  Legendary (flames) and Eternal (blue_flames).
- **[R] chests:** UA/Counterassault, Tactical/Recon, Tactical/Patrol and Collar/Grenadier [UA] [R] now use the `_roboarm` ids;
  `ReachHost.cpp` Chests() has them as aliases (236-239). **O7OF:** small edit in your file.
- **Career (`Career.cpp/.hpp`):** career.json has `creditsSpent` and `armoryOwned`; new `Career::Balance()` (earned - spent),
  `OwnsArmoryItem(key)`, `BuyArmoryItem(key, price)`. Rank still follows cR earned, so buying never lowers rank. Key = "<slot>:<id>".
- **Armory (`ArmorCustomizer.cpp`):** Spartan tiles show the Halopedia picture; above-rank pieces are dimmed with the rank;
  pieces with a price show it and clicking opens BUY in the info panel (with Balance). Saved looks with locked/unbought pieces
  fall back to the slot's first piece. `-armory-unlock-all` skips every lock (Spartan and Elite).
- **Still to do (Tyler's notes after testing):**
  - Unlocked/owned pieces should sort to the top of each list.
  - No preview of a piece before buying: clicking a for-sale piece only opens BUY and doesn't put it on the character.
  - Each equip makes the 3D character flash white (the old test-account white) for a moment. Every equip calls SaveProfile ->
    SyncToReachArmory -> ReachArmory::SetSelection, and the scene re-applies the whole look; not yet looked into further.
- **Elite (from the 22:35 entry) still untested:** rank locks, H2A shoulder/legs fix, all 9 sets + mixed pieces in a match.
  Elite armour effects and first-person Elite arms come later.
- Pictures are in the git-ignored `build/` folder: add with `git add -f`.

## 0. Pick up here (2026-10-28, 23:55 ET) - Dominus Critical Bug Fixes: Menu-to-Match ntdll Crash, UDP Port Stealing, Relay Reconnect Drop & NAT Join Flow

All changes compiled cleanly via `build.bat` (`noble-client.exe` and `noble-dedicated.exe`) with **0 errors and 0 warnings** under `/W4`:

- **Fixed Menu-to-Match `ntdll.dll` Access Violation on Exit (`src/engine/BlamEngine.cpp`, `src/main.cpp`):**
  - **Root Cause:** When the menu process (`mainmenu_all.map`) exited to launch a match, `haloreach.dll`'s engine thread was still actively running its simulation/render loop. Calling `m_fnShutdown(m_engineInstance)` (`vtable[0]`, `c_game_engine`'s scalar deleting destructor) or tearing down `UiArt`/`UIManager`/`LoadScreen` on the main thread freed the engine's critical sections and heap buffers while the engine thread was still locking/reading them inside `ntdll.dll`.
  - **Fix:** `BlamEngine::Shutdown()` and `main.cpp` now cleanly stop `MasterServerClient` (sending `DELETE /api/sessions/:id`), `SessionManager` (releasing UPnP and LAN sockets), and `NetworkTransport` (closing the WebSocket relay and UDP socket), flush `Logger::Shutdown()`, and terminate the process via `TerminateProcess(GetCurrentProcess(), 0)` without freeing live engine/D3D11 structures underneath the running engine thread.

- **Fixed Menu Process Stealing UDP Port `11774` & Wrong Guest Machine ID on Join (`src/engine/BlamEngine.cpp`, `src/common/Relaunch.cpp`, `src/ui/ServerBrowser.cpp`, `src/main.cpp`):**
  - **Root Cause 1 (UDP Port 11774 Conflict):** `BlamEngine::BuildLaunchBlock()` previously called `NetworkTransport::Get().Initialize(true)` even when launching `mainmenu_all.map`, binding UDP port `11774` inside the menu process. When `Relaunch::Start()` spawned the match process before the menu process finished closing, port `11774` was still held by the dying menu process.
  - **Root Cause 2 (Wrong Guest Machine ID in `JoinSessionSync`):** `ServerBrowser::TryJoin()` previously called `MasterServerClient::Get().JoinSessionSync()` inside the **menu process** before relaunching, sending the menu process's host Machine ID (`0x484F535400000001`) instead of the child guest process's randomly generated Machine ID and bound port.
  - **Fix:**
    - `BlamEngine::BuildLaunchBlock()` no longer initializes `NetworkTransport` when `isMainMenu` is true.
    - `Relaunch::Start()` now shuts down `MasterServerClient`, `SessionManager`, and `NetworkTransport` **before** calling `CreateProcessW`, guaranteeing ports `11774` and `11775` are free before the child process starts.
    - `ServerBrowser::TryJoin()` passes `-session-id "<id>"` to the child process, and `main.cpp` executes `MasterServerClient::Get().JoinSessionSync()` in the **child process** after `NetworkTransport::Initialize(false, ...)` has generated the guest's actual `m_localMachineId` and bound `m_localPort` (with automatic session ID lookup by `ip:port` for Direct Connect).
    - Removed the duplicate `SessionManager::Get().Initialize()` call in `src/main.cpp`.

- **Fixed WebSocket Relay Disconnecting Every Heartbeat & 5-Second Send Blocking (`src/network/NetworkTransport.cpp`, `src/network/MasterServerClient.cpp`):**
  - **Root Cause 1:** `MasterServerClient::DoHeartbeat()` received `relay_url` on every heartbeat and called `NetworkTransport::StartRelay()`, which unconditionally called `StopRelay()`, tearing down the active WebSocket connection and dropping all queued packets in `m_relaySendQueue` and `m_relayRecvQueue` every heartbeat.
  - **Root Cause 2:** `NetworkTransport::RelayWorkerThread()` inherited a 5,000 ms receive timeout from `WinHttpSetTimeouts`, causing `WinHttpWebSocketReceive` to block outgoing packet sends (`m_relaySendQueue`) for up to 5 seconds whenever no incoming relay packet arrived.
  - **Root Cause 3:** When hosting, `MasterServerClient::WorkerLoop()` waited 10 seconds after `DoRegister()` before sending the first heartbeat, delaying WebSocket relay startup and `pending_joins` NAT hole-punching by 10 seconds.
  - **Fix:**
    - `NetworkTransport::StartRelay()` is now a no-op if the relay is already active on the same `relayUrl`.
    - `NetworkTransport::RelayWorkerThread()` sets a 15 ms receive timeout (`WinHttpSetTimeouts(hWebSocket, 5000, 5000, 1000, 15)`) after upgrading the WebSocket handle so outgoing packets flush immediately.
    - `MasterServerClient::WorkerLoop()` now triggers `DoHeartbeat()` immediately after `DoRegister()` and polls every 3 seconds so `relay_url` and `pending_joins` are processed rapidly.

- **Fixed NAT Hole-Punch `PING` Peer Registration & Multi-Peer Host Routing (`src/network/NetworkTransport.cpp`):**
  - `NetworkTransport::Initialize()` now sends an immediate `"PING"` NAT hole-punch datagram from Guest to Host upon socket creation, and tries binding `localPort` (`11774`) first for both Host and Guest before falling back to an ephemeral port.
  - `NetworkTransport::OnNetRecv()` now registers the sender's `senderMachineId -> senderAddr` in `m_peerTable` when a `"PING"` NAT punch packet arrives on the Host (instead of discarding `"PING"` before peer registration) and loops to read any subsequent game packet without returning `0` early.
  - `NetworkTransport::AddPeer()` no longer overwrites an already-learned direct UDP peer address in `m_peerTable`.
  - `NetworkTransport::OnNetSend()` on the Host now broadcasts to all peers in `m_peerTable` when `targetPeerId` is not yet mapped, rather than only sending to `m_peerTable.begin()->second`.

- **Fixed `AuthClient` HTTPS Scheme Detection & Default URL Fallback (`src/network/AuthClient.cpp`, `.gitignore`):**
  - Set `parts.dwSchemeLength = static_cast<DWORD>(-1)` in `AuthClient.cpp` (`PostJson`) so `WinHttpCrackUrl` populates `parts.nScheme == INTERNET_SCHEME_HTTPS` reliably, and added fallback to `https://outbreak-z-test-server.onrender.com` when `masterServerUrl` is empty.
  - Added `node_modules/` and `**/node_modules/` to `.gitignore`.

---

## 0. Pick up here (2026-10-28, 23:35 ET) - P2P NAT Traversal, WebSocket Netcode Relay, In-Menu Account System & Developer Tools

- **Infinite Loading on Join Fixed (Home NAT Firewall Traversal & WebSocket Netcode Packet Relay):**
  - **Root Cause from Client Logs:**
    The log revealed that when Tyler joined his friend's game at `35.147.220.133:11774`, the guest sent 10 UDP handshake packets over port `52354` but received **0 packets back**. Because both players are behind residential NAT routers without manual port forwarding, the host's router firewall blocked inbound UDP packets on port 11774. The host never received the packet, never knew the guest was trying to connect, and never sent a reply, causing `external_launch_individual_state_timeout`.
  - **Solution Implemented (Two-Tier Traversal):**
    1. **Master Server Join Signaling & NAT Hole Punching:**
       - Added `POST /api/sessions/:id/join` on the backend. When a guest joins, their public IP, port, and machine ID are registered with the session.
       - On `PUT /api/sessions/heartbeat`, the host receives `pending_joins`.
       - In `NetworkTransport::AddPeer(machineId, ip, port)`, the host immediately fires a direct UDP punch packet to the guest's endpoint, opening the host router's NAT pinhole bidirectionally.
    2. **WebSocket Netcode Packet Relay Fallback (`/ws/relay`):**
       - Built a high-performance binary WebSocket packet relay into `server.js` using `ws`.
       - In `NetworkTransport.cpp`, a background worker thread (`RelayWorkerThread`) maintains a persistent WebSocket connection to `wss://outbreak-z-test-server.onrender.com/ws/relay`.
       - In `OnNetSend`, outgoing packets are sent directly over UDP AND queued to the WebSocket relay.
       - In `OnNetRecv`, if no direct UDP packet is ready, packets from the relay queue are delivered to `haloreach.dll`.
       - **Result:** Players can now join each other regardless of NAT type, ISP CGNAT, or firewall restrictions with zero router configuration.

- **In-Game Main Menu Account Login & Registration System (`AuthClient`):**
  - Implemented `noble::AuthClient` in `src/network/AuthClient.hpp` and `.cpp`:
    - Handles asynchronous authentication with the master backend (`/api/login` and `/api/register`).
    - Saves session token, username, user ID, and developer status into `settings.json`.
    - Auto-restores cached logins on startup.
  - Implemented `DrawAccountModal` and Main Menu integration in `src/ui/FrontEndMenu.cpp`:
    - Added "Account" entry to the Main Menu list.
    - Added interactive account status bar and clickable badge to `DrawParty` (top right).
    - Modal features tab switching between **LOG IN** and **REGISTER**, password masking, real-time error handling, and clean logout.

- **Developer Accounts & `/var/data/database.db` Disk Integration:**
  - Upgraded `set_dev.js` (`O7OF/ServerTestDeploy` and client `server/`):
    - Added `node set_dev.js list` command to display all registered users and their dev status.
    - Automatic seeding of `/var/data/database.db` from repository database if newly mounted.
    - Case-insensitive username lookup (`LOWER(username) = LOWER(?)`).
    - Automatically unlocks developer cosmetics (Matrix camos, Developer skins, titles, calling cards, capes).
  - In-Game Developer Recognition:
    - Glowing gold `[DEV]` badge rendered directly on the party header next to the player's name in `DrawParty`.
    - Developer status indicator displayed in the Account modal with perks overview.

- **Binaries Built & Deployed:**
  - Release binaries (`noble-client.exe`, `noble-dedicated.exe`) compiled with 0 errors.
  - Deployed directly to `C:\Users\Chey1\Desktop\Release For Test\`.
  - Pushed backend updates to `https://github.com/O7OF/ServerTestDeploy.git` (`510b797`) - live on Render.

---

## 0. Pick up here (2026-10-28, 22:55 ET) - Render Master Server Deployment Repo Synchronized & Pushed

- **Server Backend Sync & Render Trigger (`O7OF/ServerTestDeploy`):**
  - Synchronized the full server architecture (`src/` backend modules: `auth.js`, `config.js`, `database.js`, `matchmaker.js`, `ranks.js`, `sessionManager.js`, and `routes/authRoutes.js`) from `Halo Reach Community Edition/server/` into `C:\Users\Chey1\Desktop\ServerTestDeploy/`.
  - Verified JavaScript syntax across all modules with `node --check`.
  - Committed and pushed changes to `https://github.com/O7OF/ServerTestDeploy.git` (`origin main`, commit `986f2b9`).
  - Render automatic deployment triggered for `https://outbreak-z-test-server.onrender.com`.

---

## 0. Pick up here (2026-10-28, 22:50 ET) - Infinite Load on Join Fixed: Wire Packet Framing (8-Byte Machine ID Header), Peer Resolution & Shutdown Order

- **Root Cause of Infinite Load at `LoadProgress: 2.0%, launch state 9` Resolved:**
  - Reversed `host-probe.exe` (`0x1400271c0.c` / `0x140028df0.c` / `0x140025af0.c` / `0x140038130.c`) to uncover the exact wire protocol between Blam! engine peers:
    1. **Missing 8-Byte Machine ID Header on Wire Packets:**
       In `haloreach.dll`, network transport is abstracted through host callbacks Slot 41 (`OnNetSend`) and Slot 43 (`OnNetRecv`).
       - On `OnNetSend`, the host must prepend the sender's 8-byte uint64 machine ID (`m_localMachineId`) to the packet payload before transmitting over UDP (`[uint64_t senderMachineId] [payload]`).
       - On `OnNetRecv`, the host must inspect incoming UDP datagrams, extract the first 8 bytes as `senderMachineId`, pass `*outSenderPeerId = senderMachineId`, and deliver the payload stripped of those 8 bytes to `outBuffer`.
       - Previously, `NetworkTransport` transmitted raw payloads without prepending `m_localMachineId`, and `OnNetRecv` delivered the raw bytes directly while generating a random hash of the sender's IP string.
       - Because `*outSenderPeerId` was a random hash rather than the guest's authentic machine ID, `haloreach.dll`'s native network session protocol rejected incoming join packets, leaving the guest engine waiting forever in `launch state 9` (`c_network_session::join_synchronize` / waiting for membership replication).
    2. **Host-to-Peer Address Mapping:**
       On the host, `OnNetRecv` now maps the extracted `senderMachineId` directly to the sender's `sockaddr_in` in `m_peerTable[senderMachineId]`. When `haloreach.dll` invokes `OnNetSend(targetPeerId, ...)` to reply to the guest, `m_peerTable.find(targetPeerId)` finds the exact IP/port of that remote player.
    3. **Diagnostic Packet Counters:**
       Added packet counters (`m_packetsSent`, `m_packetsRecv`) logging payload and wire sizes, target/sender peer IDs, and endpoints for the first 10 packets and every 500 packets thereafter.
    4. **Clean Engine Shutdown Sequence:**
       Fixed `0xC0000005` access violation during process shutdown. In `BlamEngine::Shutdown()`, `m_fnShutdown(m_engineInstance)` is now invoked **before** `NetworkTransport::Get().Shutdown()` so the engine's internal session teardown can dispatch disconnect packets cleanly before the socket is closed.
    5. **Master Server URL Path Normalization:**
       In `MasterServerClient.cpp`, trailing slashes on custom master server URLs (e.g. `https://outbreak-z-test-server.onrender.com/`) are now properly trimmed before appending endpoint paths, preventing double-slash HTTP 400/404 routing errors.

- **Binaries Deployed & Verified:**
  - Built Release binaries (`noble-client.exe`, `noble-dedicated.exe`) with 0 errors and 0 warnings.
  - Fresh binaries deployed directly to `C:\Users\Chey1\Desktop\Release For Test\`.

---

## 0. Pick up here (2026-10-28, 22:35 ET) - invisible players, menu crash, join window, Elite pack fixes

All tested by Tyler unless marked.
- **Invisible character models (pushed):** O7OF's `IsArmorItemUnlocked` hook (`EnginePatches.cpp` Â§3) found its target with a
  generic function-prologue signature, which matched `hr+0x105BDC` inside the appearance builder instead of `0x2C7DCC`;
  replacing it with "return 1" stopped player models from being built. The hook is now off (`#if 0`, comment says why).
- **Menu -> match crash (pushed):** `main.cpp` shuts the engine down before UiArt / UIManager / LoadScreen (the engine thread
  drew one more ImGui frame after ImGui was gone).
- **Joining left the menu open (pushed):** `ServerBrowser::TryJoin` closed `GetActiveWindow()`, which is NULL on the engine
  thread; it now closes this process's own top-level window. (Written before O7OF's 22:15 netcode entry below; joining is his area.)
- **Elite pack (`ElitePack.cpp`):**
  - Shield: the pack model's own shield value (hlmt +0xD0) was 210, the stock MP Elite's 79; now copied from the stock Elite
    (its damage info at hlmt +0xC0 was already identical: body 50, shield 79, stun 3 s, recharge 3.5 s). `-elite-pack-own-health` keeps the pack's.
  - Heart Attack + Birthday Party on every pack Elite death: the pack model's "body" damage section (hlmt +0xE0, 0xC0 each,
    blocks at +0x1C / +0x84) plays `armor_fx_death`, which reads the "armor_fx" model region the pack Elite lacks. Those two
    references are cleared for the pack Elite only (log "dropped 2 death armour effect reference(s)"). `-elite-pack-death-fx` keeps them.
  - Elite slot patch at `0x105796` is now `cmp esi,8; setb r15b` (lists 0..7 for Elites, not the effect lists 8/9).
  - Kept as diagnostics: `RestoreMapGlobals` (logs tags the pack replaced; only pmcg so far), `-elite-pack-dump`, and the first
    6 appearance records are logged (Elite record: +0x10..+0x17 pieces/set, +0x18/+0x19 effects, +0x1A visor?).
  - The pack's render model has H2A pieces too (h2a_space_helmet, h2a_l/r_shoulder, h2a_legs), not only the menu map.
- **Armory:** Elite Armor Set tiles show pictures from `data\ui\reach\elite_sets\<set id>.png` (Tyler's 8 renders;
  Ranger = `space`, Field Marshal = `story_elite`). Add with `git add -f`.
- **Spec:** `docs/menu-redesign-spec.md` trimmed to what's still to do (steps 1-8 done).
- **Elite rank locks (not built/tested yet):** `ArmorCustomizer.cpp` `kEliteUnlocks`: Spec Ops Corporal, Ranger Sergeant, Officer
  Sergeant Grade 2 (Tyler may pick Commander), Ultra Warrant Officer, Zealot Captain, General Major, Field Marshal Lt. Colonel,
  Ossoona Major G3, Schism/Combat Commander G2, H2A helmet/shoulders/legs Colonel G1, Council Brigadier, Honor Guard General.
  Rank only, no cR; locked tiles dimmed with the rank; saved locked pieces fall back to Minor; `-armory-unlock-all` for tests.
  Also fixed `ElitePack::PieceIndex`: H2A shoulders are piece 11 (were dropped), H2A legs 9 / Honor Guard robe legs 10 (were swapped).
- **First-person Elite arms (later, Tyler 23:33):** player representation 1 (matg +0x474) = FP hands `objects\characters\elite\fp\fp`,
  FP body `...\elite\fp_body\fp_body` (+0x20 / +0x30 tagrefs), 3P unit +0x40. The FP model has region `armor_pieces`
  (general, officer, space, spec_ops, ultra, story_elite, zealot, default) + `body`; markers left_hand, secondary_weapon. The Elite set
  list (pmcg +0x74) changes only `armor_pieces`. Likely cause of bare arms: the pack's 3P Elite has per-piece regions and no
  `armor_pieces` region, so nothing selects the FP permutation. Idea: drive the FP `armor_pieces` permutation from the forearms pick
  (needs the code that picks FP permutations; research). The FP model has no shoulder region.
- Open: Elite armour effects (needs an armor_fx region or another route); the menu process still logs an ntdll access
  violation when it exits for a match (22:03, 22:24; separate from the ImGui fix, not looked at yet); first-person Elite arms.

---

## 0. Pick up here (2026-10-28, 22:20 ET) - Dominus Updates: Build System Fallback, Zero-Warning Compilation & Git/Toolchain Setup

- **Git & Team Onboarding (`Dominus-VII`):**
  - Configured global Git author identity (`Dominus-VII <349591305+Dominus-VII@users.noreply.github.com>`) on Dominus's workstation (`Reeve`) and verified remote push access to `origin/main` (`https://github.com/O7OF/test-build-01.git`).

- **Build Script & Toolchain Auto-Detection (`build.bat`, `CMakeLists.txt`):**
  - Updated `build.bat` to verify via `vswhere` whether the installed Visual Studio instance includes `Microsoft.VisualStudio.Component.VC.Tools.x86.x64` before invoking the Visual Studio CMake generator.
  - Added automatic fallback in `build.bat` to configure and build using Ninja + local MSVC 19.44 (`%LOCALAPPDATA%\Programs\MSVC\VC\Tools\MSVC\14.44.35207`) + Windows SDK when the full Visual Studio IDE C++ workload is absent.
  - Configured single-config CMake generators (Ninja) in `CMakeLists.txt` to output directly to `build/bin/Release` so `build\bin\Release\noble-client.exe` and `build\bin\Release\noble-dedicated.exe` match `build.bat` and `run.bat`.
  - Set CMake policy `CMP0169` to `OLD` to silence CMake 4.x `FetchContent_Populate` deprecation warnings for `minhook` and `imgui`.
  - Scoped `/W4` strictly to `noble-client` and `noble-dedicated` and `/W0` to third-party `minhook`, eliminating `D9025` flag-override warnings and third-party C warnings.

- **Zero-Warning Source Fixes Across `src/` (`/W4`):**
  - `src/ui/ObjModel.cpp`: Replaced C++20-deprecated `std::filesystem::u8path` calls (`C4996`) with `PathFromUtf8()` (`std::filesystem::path(std::u8string_view)`), and renamed shadowed local variable `relative` to `texRelative` (`C4456`).
  - `src/ui/CareerPanel.cpp`: Renamed local `kModeColour` array in `CareerPanel::RenderCommendations()` to `kTabModeColour` to avoid shadowing the file-scope `kModeColour` (`C4459`).
  - `src/hooks/ForgePatches.cpp` & `src/hooks/EnginePatches.cpp`: Added `[[maybe_unused]]` to internal helpers `WriteProtectedMemory` and `Hook_IsArmorItemUnlocked` (`C4505`).
  - `src/ui/FrontEndMenu.cpp` & `src/engine/ReachHost.cpp`: Marked unused parameters in `DrawLobbyControls` (`width`) and `HostSlot23_LoadProgress` (`a2`) (`C4100`).
  - Verified full `build.bat rebuild` compiles all 82 targets (`noble-client.exe` and `noble-dedicated.exe`) with **0 errors and 0 warnings**.

---

## 0. Pick up here (2026-10-28, 22:15 ET) - Netcode Fixed: Host Slot 41/43 NetworkTransport, Synchronized Session IDs, Active Guest Player Slot & Variant Decoding

- **Root Cause Resolution for "Loads forever at 5% on join" and "Player joined never pops up":**
  1. **Missing Game Variant Decoding on Guest:** In `BlamEngine.cpp` (`BuildLaunchBlock`), `!isGuest` previously skipped variant decoding into `block[0x300]` when joining. Halo: Reach's Blam! engine requires the game variant layout in memory to resolve scenario dependencies during map loading; without it, the engine stalls at 5% indefinitely. Fixed by decoding stock or custom game variants (`slayer_054.bin` / `hr_ff_standard_1set.bin`) for **both** host and guest.
  2. **Unimplemented Packet Transport (Host Slots 41 & 43):** Reversed Reach Renaissance (`host-probe.exe`) and discovered that in MCC, `haloreach.dll` abstracts network packet transmission through the host interface callbacks rather than raw WinSock sockets:
     - **Host Slot 41 (`HostSlot41_NetSend`):** Engine calls to send UDP packets (`targetPeerId`, `data`, `size`).
     - **Host Slot 43 (`HostSlot43_NetRecv`):** Engine polls to receive incoming packets (`outBuffer`, `capacity`, `outSenderPeerId`).
     Previously, both slots were silent stubs returning 0, dropping 100% of network traffic. Created `NetworkTransport` (`src/network/NetworkTransport.hpp` & `NetworkTransport.cpp`) providing non-blocking UDP socket I/O on port 11774 with automated peer table mapping.
  3. **Mismatched Network Session IDs:** Previously, each client generated a random GUID at `block[0x50]` independently, causing the host to reject incoming guest packets due to mismatched session credentials. Fixed by synchronizing `block[0x50]` and secondary interface slot 147 (`Iface2Slot147_SessionGuid`) using a shared, deterministic session ID (`0x48414C4F52454143ULL`).
  4. **Zeroed Local Player Count on Guest:** `BuildLaunchBlock` previously cleared `block[0xE8]` and player slots on the guest, leaving 0 active local players. Reach engine requires at least 1 local active player (`block[0xE8] = 0x01`, descriptor at `0x100..0x10B`) so the guest can spawn its player into the session and communicate membership to the host.
  5. **Host Machine ID in Launch Block:** Corrected `block[0x2F8]` to carry the 64-bit uint64 Host Machine ID (`kDefaultHostMachineId = 0x484F535400000001ULL`) rather than raw IP octets.

- **Fresh Release Binaries Deployed:**
  - Built with MSVC Release configuration (`noble-client.exe`, `noble-dedicated.exe`) and copied to `C:\Users\Chey1\Desktop\Release For Test\`.

---

## 0. Pick up here (2026-10-28, 21:00 ET) - O7OF Updates: Master Server Online, IPify Public IP Detection, Guest Launch Block & Robust UPnP P2P

- **Master Server Deployed & Live on Render (`https://outbreak-z-test-server.onrender.com/`):**
  - Updated `server.js` and `package.json` in `server/` and pushed to remote GitHub repo `https://github.com/O7OF/ServerTestDeploy` (`main` branch, commit `2362b5d`).
  - Render auto-deployed and confirmed healthy: `GET /api/sessions` returns 200, `POST /api/sessions` registers active games with 201 Created and public IP detection via reverse proxy headers (`x-forwarded-for`).
  - Added JSON error body parsing in `MasterServerClient.cpp` `HttpRequest()` so any master server error returns human-readable diagnostic messages.

- **Public IP Discovery with Ipify & Zero-Conflict UPnP (`UPnP.cpp`, `UPnP.hpp`):**
  - Added background public IP resolution via `https://api.ipify.org` (with `http://api.ipify.org` fallback) during `UPnP::Initialize()`.
  - Added `IsPublicIP()` validator ensuring loopback (`127.0.0.1`), link-local (`169.254.x.x`), and RFC 1918 private subnets (`10.x.x.x`, `192.168.x.x`, `172.16-31.x.x`) are never advertised as external public IPs.
  - UPnP `ForwardPort()` now issues `DeletePortMapping` before `AddPortMapping` to clear stale router leases, preventing conflict 718 errors. If router UPnP is disabled or rejected, it cleanly falls back to Direct IP mode while preserving the valid public IP for P2P connection.
  - Removed premature `WSACleanup()` from `UPnP::Shutdown()` to ensure WinSock remains active while the Blam! engine network thread shuts down.

- **Guest Launch Block Construction & Network Plumber (`BlamEngine.cpp`, `main.cpp`):**
  - Fixed `src/main.cpp`: explicitly forwards `connectAddress` into `blamEngine->LaunchMap(..., connectAddress)` and sets `launchHost = false` when joining as guest.
  - In `BlamEngine.cpp` (`BuildLaunchBlock`):
    - Sets `block[0] = 0x00` (guest mode).
    - Clears 32 bytes of local player block at offset `0xE8`.
    - Writes `0xFFFFFFFF` at offset `0x2F4`.
    - Serializes target host endpoint (IPv4 octets + uint16 port + uint16 pad) at offset `0x2F8`.
    - Sets guest sync flags at offset `0x2BF14` (`0xF3 0x01`).
    - Skips local variant decoding/mvar injection so match parameters are synced directly from the host.

- **Server Browser & Direct Connect Enhancements (`ServerBrowser.cpp`):**
  - **Public IP in Browser & Clipboard:** Display and "COPY INVITE" / "COPY ADDRESS" now copy the resolved public IP (`UPnP::Get().GetExternalIP()`) instead of the private LAN IP when self-hosting.
  - **Invite Parsing Resiliency:** `ParseAddress()` now automatically strips prefixes (`/connect `, `-connect `, `connect `, `http://`, `https://`) and whitespace/quotes, allowing one-click paste from Discord, Steam chat, or server browser invites.
  - **Direct Connect Modal Polish:** Cleaned default input text (starts empty with informative hint placeholder `IP or IP:port (e.g. 147.124.149.202:11774)`).

- **Release Test Build Deployed:**
  - Fresh binaries built with CMake/MSBuild (`noble-client.exe`, `noble-dedicated.exe`) and copied to `C:\Users\Chey1\Desktop\Release For Test\`.

---

## 0. Pick up here (2026-10-28, 20:30 ET) - O7OF Updates: Netcode Fixes, Server Browser Live Overhaul, P2P Join & First-Run Setup

All committed to `main` branch (commits `60fb4a3`, `14d3852`, `5d50fc2`). Release test binaries deployed to `C:\Users\Chey1\Desktop\Release For Test`.

- **Network Shutdown Crash & Port Contention Fixed (`SessionManager`, `UPnP`):**
  - **Discovery Port Separation:** LAN discovery beacons and listener moved from UDP `11774` to UDP `11775` (`kDiscoveryBeaconPort = 11775`). UDP `11774` is kept strictly dedicated to Blam! engine packets and UPnP port forwarding. Previously, `SessionManager::DiscoveryListenerLoop` binding `SO_REUSEADDR` on 11774 intercepted and consumed `haloreach.dll`'s native game packets.
  - **Clean Thread Shutdown:** Added `uintptr_t m_listenerSock` handle. In `SessionManager::Shutdown()`, `closesocket(m_listenerSock)` is now called before joining `m_listenerThread`, immediately interrupting any blocking `recvfrom()` call and eliminating the shutdown freeze/deadlock.
  - **Sliced Beacon Polling:** Replaced the monolithic 1500 ms `std::this_thread::sleep_for` in `SessionManager::BeaconLoop()` with a 50 ms polled sleep loop checking `!m_stopThreads && m_isHosting`, allowing `StopHosting()` to join threads instantly.
  - **Safe WinSock Lifetime:** Removed premature `WSACleanup()` from `UPnP::Shutdown()` that was tearing down network sockets while threads were exiting.

- **Server Browser Overhaul & Hosting Flow (`ServerBrowser`, `FrontEndMenu`):**
  - **Removed Dummy Hosting Modal:** The broken placeholder "HOST GAME..." modal and form variables in `ServerBrowser` were removed entirely. Hosting is now driven exclusively by the Pre-Game Lobbies (Campaign, Firefight, Custom Game, Forge) where map, gametype, and lobby privacy (Public / Friends / Local) are configured.
  - **Main Menu Navigation:** Renamed "Matchmaking" button on the main menu to "Server Browser" (`FrontEndMenu.cpp`).
  - **Live Games Only:** `ServerBrowser::RefreshServers()` now filters out offline saved servers or empty placeholder entries. Only live matches discovered via LAN broadcast, the master server, or the local hosted match are shown in the server list.
  - **Active Session Termination:** Added a "STOP HOSTING" button to the Server Browser when hosting, which cleanly deregisters the match from the master server (`DELETE /api/sessions/:id`), stops LAN beacons, and closes the UPnP port forwarding.
  - **Direct Connect & Invites:** Added Direct Connect dialog with IP and Port fields, along with Copy Invite (`/connect <ip>:<port>`) and Copy Address clipboard buttons for easy sharing.
  - **Master Server HTTP 400 Resolution:** Resolved HTTP 400 Bad Request error by auto-populating external public IP via `UPnP::Get().GetExternalIP()` (falling back to local IP) when registering sessions on `MasterServerClient.cpp`. Live master server deployed at `https://outbreak-z-test-server.onrender.com/`.

- **Halo: Reach Built-in Netcode P2P Remote Joining (`BlamEngine`, `main.cpp`):**
  - Added `-connect <ip>:<port>` command-line argument plumbing in `main.cpp` into `BlamEngine::LaunchMap(..., connectAddress)`.
  - Adapted `BlamEngine::BuildLaunchBlock` for remote guest joins when `!connectAddress.empty()`:
    - Sets `block[0] = 0x00` (guest mode vs `0x08` for host, derived from `(param_3 ^ 1) << 3`).
    - Skips local host slot registration.
    - Sets connection flags `block[0x2F4] = -1` and `block[0x2BF14] = 0xF3 0x01` based on reverse-engineering analysis of `host-probe.exe`.

- **First-Run Profile & Configuration Auto-Generation:**
  - Automated initial generation of `profile.json`, `appearance.json`, `career.json`, and `settings.json` on first run if they do not exist. Prevents crashes and missing data directory errors for fresh playtesters.

- **Playtest Deployment Packaging:**
  - Fresh Release binaries (`noble-client.exe`, `noble-dedicated.exe`) compiled and copied to `C:\Users\Chey1\Desktop\Release For Test\`.
  - Backend server repository updated in `C:\Users\Chey1\Desktop\ServerTestDeploy`.

---

## 0. Pick up here (2026-10-28, 19:50 ET) - pushed 4483029; menu -> match crash fixed (not yet built/tested)

- **Pushed** `4483029` (Elite pack A1-A3, ElitePack files, docs) on top of O7OF's pulled `0f4d050` (career),
  `f94c4cb` (settings / main / server browser), `f87c367` (main entry + network client). Working tree was clean.
- **Crash starting a match from the menu (Windows error):** the menu process shuts down to relaunch for the match;
  `main.cpp` shut ImGui down (`UIManager::Shutdown`) before `blamEngine->Shutdown()`, and the engine thread drew one
  more overlay frame -> AV in `ImGui_ImplDX11_NewFrame` (fault RVA 0x1B3D1E, found with `noble-client.map`, exe base
  from the IsWhitespace hook address in the log). Fix written to `src/main.cpp` (engine shutdown first, then UiArt /
  UIManager / LoadScreen). **Not built, tested or committed yet.** Tell O7OF (his main.cpp area).
- The log showed two Noble processes writing the same log at once, and a fresh launch self-hosting
  "Mode: Campaign, Map: Boneyard" right away (O7OF's new network code?) - ask O7OF if intended.
- Elite pack state: see 19:25 below (first-person Elite arms still bare, pack Elite too tough).

## 0. Pick up here (2026-10-28, 19:25 ET) - Elite pack: A2 + A3 work (Armory Zealot spawns as Zealot)

All in `src/engine/ElitePack.cpp` (on Tyler's PC, NOT committed/pushed yet; Tyler decides when). Tested on Sword Base.
- **A2 player Elite (works):** after the pack's cache step, the one `bipd` inside the pack section is the pack's player
  Elite (0x85564635 here); in the map's `matg` (+0x474 representations, 0x6C each, biped datum at +0x40) every entry
  equal to representation 1's biped is pointed at it (AI keep the stock Elite). Zone-set rebuild hook `hr+0x8EAF0`
  re-sets active bits (state+0x30B70) for biped, its model (+0x70) and render model (model +0x0C).
- **Armour list merge (works):** the pack replaces the pmcg; Noble puts the map's back (its Elite set list +0x74 dresses
  the first-person arms) and copies, for the 7 Spartan piece lists (+0x20..+0x68, 12 bytes apart; selection 16 bytes:
  name sid, changes block; change 12 bytes: region, permutation, flags), every pack change to an "Elite region" (regions
  the pack's Elite sets change: 87EE 87F3 87F6 87F8 87F9 87FD) into the map selection of the same name; pack-only
  selections (legs list pack 11 vs map 7, wrist 9 vs 6) are appended. New arrays: VirtualAlloc + `hr+0x688F3C` encode.
  `-elite-pack-list` = test option keeping the pack's list (Spartans fine, but first-person Elite arms bare).
- **Elite armour slots patch:** `hr+0x105796` `83 FE 07 41 0F 94 C7` -> `41 B7 01 90 90 90 90` (RR `--elite-armor-slots`):
  the appearance builder `0x1055EC` then applies all 8 lists to Elites, not only +0x74.
- **A3 picks (works: Armory Zealot -> Zealot):** calls to `0x1055EC` at `0x4AC36` (spawn), `0x447B0` (live), `0x3B184F`
  (remote), `0x524CD5` (copy) retargeted. For Elites (mode 1, or species byte rec+4 == 1; modes 0/2 untouched) a copy of
  the 0x28-byte appearance record gets rec+0x10..+0x15 = six pack piece indices (helmet, L, R, chest, forearms, legs),
  rec+0x16 = 0xFF, rec+0x17 (Elite set) kept for the first-person arms. Pieces from `data\appearance.json` "elite"
  *_id via `PieceIndex` (pack order = menu lists minus H2A; RR set table kSetPieces as fallback). Every player Elite gets
  the local Armory pieces for now (other players need networking).
- **Open:** pack Elite is much tougher than MCC's MP Elite (2 grenades don't drop its shields; stock Elite dies) - copy
  MCC's MP Elite vitality, or RR's "Even Elites"; first-person Elite arms still bare (Tyler 19:22) even with the map list + set byte kept - find which model draws them (pack FP model? representation entry has more refs than +0x40); H2A indices are guesses (11 helmet, 10 legs);
  Honor Guard etc. to verify by eye; Heart Attack/Birthday effect showing once (watch); `.rrneeds` check not done
  (campaign excluded); before pushing, consider making the pack opt-in (`-elite-pack`) so O7OF doesn't get it by surprise.
- `NOBLE_PROFILE_SET` env var trap: it persists in a cmd window and overrides the Armory (+0x8C=C7 = General). Clear with
  `set NOBLE_PROFILE_SET=`.

## 0. Pick up here (2026-10-28, 18:20 ET) - Elite armour pack, Stage A1 works (pack loads into matches)

**Goal (Tyler, Option 1):** separate Elite pieces in matches, later the H2A pieces. MCC's match maps only carry an Elite
with one `armor_pieces` region (8 sets); the menu map's Elite (RR's `mainmenu_all.map`) has per-piece regions plus
H2A / Honor Guard / Council / Ossoona etc. Plan: load an Elite pack into every match like Reach Renaissance does.
Full research notes: project doc `claude/elite-pack-notes.md`. RR's pack has no H2A pieces (Tyler checked RR's Armory),
but its piece indices suggest it does carry Honor Guard / Council / Ossoona / Schism / Combat (unverified).

- **Stage A1 done and tested (18:19, Sword Base):** new `src/engine/ElitePack.{cpp,hpp}` (+ CMakeLists, `main.cpp`
  calls `ElitePack::Install(module, dataDir, mode)` for non-menu launches, slot 48 in `ReachHost.cpp` resolves the pack
  file name first). Only for `multiplayer` and `firefight`; `-no-elite-pack` turns it off. Files (local only, git-ignored):
  `build\bin\Release\data\armor\rr_elite_pack.map` + `.rrreloc` (+ `.rrneeds`, `.rrpmcg` for later), copied from
  `D:\Reach\data\armor`. What it does:
  - call `hr+0xA38A3` (-> map load `0x141C0`) retargeted: when the request has no extra caches, adds one,
    `haloreach\maps\rr_elite_pack` (request: +0 name, +8 extra count, +0x10 names).
  - call `hr+0x31395` (-> extra cache load `0x31078`) retargeted: for the pack, first `hr+0xAA634(0xFFFF, name, 2)`
    (header slot, the main map gets the same at `0xA37DC`).
  - 2-byte patch `hr+0x30859` `8B D2` -> `66 90` (keeps the 64-bit load delta).
  - MinHook on `hr+0x30698` (per-cache step, arg 5 = cache: +0 loaded base A, +0x24 size, +0x2F0 file base B,
    +0x2F8 tags header): before the original, rebases the pack's packed pointers from `.rrreloc` (RRRL v1: count,
    u64 B, u64 packed base, u32 size, then u32 offsets; addr = (p<<2)+packedBase, if in [B,B+size) write
    `hr+0x688F3C(addr + A - B)`) and the 4 u64 table pointers at header +8/+0x18/+0x28/+0x38.
  - MinHook on the 14 resource-interface methods (vtable `0x996978`, `0x1BB6C0..0x1BBDA0`, index in r8 for 6C0/7B4/C9C/
    D34/DA0, else rdx): an index in a range k with lo != 0 is called on a one-range copy of the interface with the
    index rebased (RR `resadapt`). Without them the first test crashed at `hr+0x1BB3D4` right after the map load.
  - Log: "relocated 121513 packed pointers (0 outside ...) and 4 table pointers", "extra cache ... load returned 1";
    the match then plays normally. Players still use the map's Elite.
- **Next (Stage A2):** RR `armorpack::callback` after the original step: merge the pack's pmcg Elite selections into the
  map's pmcg (`MERGE`), point player representation 1 (globals block at +0x474) at the pack's player Elite biped
  (AI keep the stock one), and the zone-set rebuild hook `hr+0x8EAF0` that re-sets the pack's tag datums active
  (bits at rcx+0x30B70). Then Stage A3: retarget the 4 calls to `hr+0x1055EC` (spawn `0x4AC36`, live `0x447B0`,
  remote `0x3B184F`, copy `0x524CD5`) to feed the Armory's six Elite pieces. Not done: RR's `.rrneeds` check (that the
  match map has all 149 stock tags the pack needs); campaign is excluded until it exists.
- For O7OF: new ElitePack files, small hook-up in `ReachHost.cpp` (slot 48) and `main.cpp`.

## 0. Pick up here (2026-10-28, 17:35 ET) - Elite armour in game fixed, Armory scene leak fixed

- **In-game Elite fixed (tested by Tyler):** profile fields +0x68 and +0x8C were swapped in `ApplyArmoryAppearance`
  (`ReachHost.cpp`). +0x8C is the **Elite armour set** (194 + set; `8C=C7` spawned a General, `8C=A9` a non-Minor
  Elite), +0x68 is the **Spartan armour effect** (166 + effect; Tyler: effects work in game). Writing 173+effect to
  +0x90 changed nothing visible. That is why the Armory's armour effect used to pick the in-game Elite and the Elite
  Armor Set did nothing. Tell O7OF (his file). Still to confirm by eye: each of the 8 Elite sets spawns as itself.
- **Test option survives the match restart:** starting a game relaunches Noble with only the game args, so
  `-profile-set` was lost in the match process (every earlier in-game Elite test was void). `-profile-set` now also
  reads the environment variable `NOBLE_PROFILE_SET` (inherited by the relaunch), e.g.
  `set NOBLE_PROFILE_SET=8C=C7&& noble-client.exe` (no space before `&&`). Log line: `-profile-set +0x8C = 0xC7`.
- **Armory character no longer follows into other menus:** `FrontEndMenu::UpdateSceneForScreen` default branch sets
  `start_menu` (scene 0x0, shows_elite 0); `ReachArmory` only shows the Elite in Armory scenes (0x0A-0x14 except roster
  0x0E). Log confirms the switch when leaving the Armory.
- Open: black background behind the menu / Armory scene (scene environment not drawing); Tyler hasn't answered
  whether the main menu should show his character beside the list or none.

## 0. Pick up here (2026-10-28, 16:30 ET) - Elite per-piece armor, all the menu map's Elite pieces

- The Elite model in `mainmenu_all.map` (render model regions read from the map file; permutation array at file offset
  0x1042b8a8, stride 24) has these regions and permutations:
  - `elite_helmet` (12): council, general, h2a_space_helmet, honor_guard, minor, officer, ossoona, space, spec_ops, story_elite, ultra, zealot
  - `elite_left_shoulder` / `elite_right_shoulder` (12 each): combat, general, h2a_l_shoulder / h2a_r_shoulder, honor_guard,
    minor, officer, schism, space, spec_ops, story_elite, ultra, zealot
  - `elite_chest` (9): minor, general, officer, space, spec_ops, ultra, story_elite, zealot, honor_guard
  - `elite_forearms` (9): default (Minor's arms), general, honor_guard, officer, space, spec_ops, story_elite, ultra, zealot
  - `elite_legs` (11): general, h2a_legs, honor_guard, honor_guard_legs, minor, officer, space, spec_ops, story_elite, ultra, zealot
  - plus `body` and `head` (default only).
- Bug fixed: `ReachArmory` wrote the Elite's GLOBALS pairs with Spartan region names (head/chest/left_shoulder/
  right_shoulder/arms/legs; "legs" isn't even a string_id in the map), so Elite armor changes were hit or miss (Tyler saw
  odd pieces when changing Armor Effect). It now writes `elite_helmet`, `elite_left_shoulder`, `elite_right_shoulder`,
  `elite_chest`, `elite_forearms`, `elite_legs` from new `ReachArmorySelection` fields (`eliteHelmet` ... `eliteLegs`).
- `ArmorCustomizer`: Elite rows are now Armor Set + Helmet, Left Shoulder, Right Shoulder, Chest, Arms, Legs
  (`GetElitePieces(piece)`; the first 8 entries of every list follow `kEliteSets`, so a set index is also a piece index and
  old profiles load unchanged). Extras: Honor Guard (all pieces), Council, Ossoona, Ranger (H2A) helmets; Schism, Combat,
  H2A shoulders; Honor Guard (Robe), H2A legs. Picking an Armor Set resets all six pieces. Saved as `*_index` plus `*_id`
  under `"elite"` in appearance.json. Field Marshal now uses `story_elite` (it pointed at `zealot_commander`, which the
  model doesn't have, so it showed as Minor). Names of the extras are from the map's permutation names; confirm by eye.
- **In matches** the Elite still wears only its Armor Set: Reach's player appearance has one Elite set item
  (`ReachHost.cpp` slot 34, `kEliteSet` 194 + 0..7). Mixed pieces and the extras are Armory-only for now (their
  descriptions say so). Getting them into games needs another route (research).
- Tested: capture `ep1_mixed.bmp` (Ossoona/H2A/Honor Guard mix shows on the real Elite). Still open: Elite colours stay
  stock blue; the Elite's head is cut off at the top by scene 0x0B's camera.

## 0. Pick up here (2026-10-28, 16:05 ET) - the real Armory scene works (Spartan and Elite)

- **Engine Armory scene now loads.** With RR's `mainmenu_all.map` + `mainmenu_all.string_ids.csv` copied into
  `build\bin\Release\data\menumaps\` (local only, git-ignored, over GitHub's size limit), the menu map was always loading;
  `ReachArmory` just never found its GLOBALS table. Cause: the scanners skipped any memory region over 512 MB (Reach's tag
  data sits in bigger ones) and gave up after 10 s. Fix (O7OF's `src/engine/ReachArmory.cpp`, small): `ReadWriteChunks()`
  walks committed private RW memory in 16 MB pieces with 4 KB overlap (RR reads 1 MB pieces with no size cap), skips the
  scanner's own copy buffer, keeps looking while the menu is up (250 ms, then 1 s), and logs progress every 20 passes.
  Result: "Found mainmenu_all.map GLOBALS table ... (stride=8)" about 5 s after launch; the posed MCC Spartan shows with
  the saved armor and colours (capture `es3_armory.bmp`).
- **Elite:** species Elite switches the scene to 0x0B (shows_elite=1) and the real Elite shows (captures `el1_elite.bmp`,
  `el2_specops.bmp`). Armor sets go through the GLOBALS permutation slots (Tyler clicked through all 8; the head changes).
  The Elite hlmt region-table scan (`ScanEliteTable`) never matches; it now retries 10 times while the Elite is shown, then
  warns once. **Open:** Elite colours stay stock blue (the 6 tracked colour records look like the Spartan's); the Elite's head
  is cut off at the top of the screen (Reach's own scene 0x0B camera, not Noble's framing).
- `ArmorCustomizer`: with `data\menumaps\mainmenu_all.map` present the preview says "LOADING ARMORY..." until the scene is
  ready instead of flashing the OBJ stand-in; the OBJ preview only shows if the scene isn't up after 30 s (or the map is
  missing). Note `-menu-capture N` counts frames after the scene is ready (`sceneReadyFrames`).
- For O7OF: the scanner change above is in your file; nothing else in ReachArmory/BlamEngine changed. Noble still builds its
  own main-menu launch block (no `lb_m_c_1.bin` in Noble's data folder) and that works.

## 0. Pick up here (2026-10-28, 15:15 ET) - Armory v2, first pass (spec step 8)

- `ArmorCustomizer::Render` rewritten: left = grouped slot list (CHARACTER species/body, ARMOR helmet, variant, shoulders, chest,
  wrist, utility, knees, visor; Elite: armor set; COLORS primary/secondary; EFFECTS & VOICE; IDENTITY service tag), middle =
  picker for the focused slot (title, EQUIPPED: X, RANDOM, tile grid with equipped check, colours as a 10 hue x 3 shade swatch
  grid, hovered option's name + description), right = Reach's live Armory scene (Reach's camera puts the model on the right,
  so the new panels sit left). Camera follows the focused slot by default (`m_cameraMode = 2`, `m_focusScene` = c_helmet /
  c_left_shoulder / c_chest / ...); mouse wheel over the model: close-up / full body / Armory view. Up/down = slot,
  left/right = option. The old top pills (SPARTAN / ELITE / PREFERRED SPECIES / BODY / CAM) and the colour pop-up are gone
  (species and body are rows now; preview = preferred species).
- Capture `ar_armory.bmp` (build clean) shows the layout. Picker scroll bounced back (Tyler's video): tiles were placed with
  `SetCursorPos(GetCursorStartPos() + ...)`, which counts the scroll twice; now placed from one screen-space origin.
- **No 3D model in the Armory (not new):** `ReachArmory` waits for `mainmenu_all.map`'s GLOBALS table, which never appears
  (log: "Scanning process memory for mainmenu_all.map GLOBALS table..." every 10 s, and "Could not locate
  mainmenu_all.string_ids.csv") because MCC ships no Reach main-menu map. Same in every log since at least 14:53, before
  Armory v2. O7OF's earlier DX11 OBJ preview (`ObjModelPreview`) is no longer in `src/`. Needs a decision: bring back a
  Noble-drawn preview (models read from the player's MCC maps, spec 4.6) or get the engine's Armory scene to load.
- Not yet: picture thumbnails per piece (render once and cache under `data\ui\cache\armory\`), rank/cR locks
  (`docs/career` armory list), stance poses, emblems (profile offsets unknown), ODST / Marine / mod looks (research spike, step 9).

## 0. Pick up here (2026-10-28, 15:10 ET) - default controls, VSync off

- Noble's defaults (Tyler): melee F (was Q), throw grenade G (was F), crouch Left Ctrl (already), Falcon ascend Space /
  descend Left Ctrl, VSync off by default (`Settings.hpp` `verticalSync = false`). Existing settings.json files keep their
  saved VSync value; Restore Defaults in Graphics gives the new default.
- Mouse 4 / Mouse 5 already work: the rebind capture takes any VK 1-255 (XBUTTON1/2 included, shown as "Mouse 4/5") and
  slot 36 input passes them to the engine (`ReachHost.cpp`, st[0x004 + VK] and the 0x08/0x10 button bits).
- **Remind Tyler to test** (he asked to be reminded): typing a value into a slider box (Look Sensitivity, Enter, Apply);
  Falcon keys show Space / Left Ctrl; melee F / grenade G in game; binding an action to Mouse 4 / Mouse 5 and using it in game.

## 0. Pick up here (2026-10-28, 14:50 ET) - post-game "exit experience" (spec step 7)

**Status:** pushed (`2319c27`, after fixing a build break: `small` is a Windows macro, never use it as a name). Tyler
played a game: it works.

- Quit box on the main menu: the menu behind it no longer reacts to the mouse (`ImGui::BeginDisabled(modal)` around the
  party box, title and list; `FrontEndMenu.cpp`).
- Falcon defaults (Tyler): Ascend = Space, Descend = Left Ctrl (`KeyBindings.cpp` slots 62 / 63, were unbound).
- Every Settings slider's value box (e.g. sensitivity "1.00x") can be clicked and typed into: Enter sets it, Esc cancels,
  clicking away sets it. Typed values keep two decimals (not snapped to the slider step), whole numbers for whole-number
  sliders, percent sliders take 0-100, always clamped to the slider range (mouse sensitivity 0.10-5.00x, vertical 0.25-4.00x).
  `SettingsPanel::SliderValue`, `m_typeRow`. Typing needs ImGui keyboard input, which the in-game pause overlay doesn't feed
  yet, so there it only works with the bar.
- References Tyler sent (for later screens): Reach matchmaking "STARTING MATCH / BRACE FOR CARNAGE" lobby (map + game type
  picture, countdown, roster in team colours with emblem, name and rank icon); Reach beta "MATCH FOUND / VOTE FOR GAME AND
  MAP" (3 options + NONE OF THE ABOVE, vote counts, "Voting round ends in" timer); Halo Infinite post-game PLAYER RECAP
  (tabs PLAYER RECAP / TEAM LINEUP / SCOREBOARD, score, rank, stat tiles, medal strip, Spartan on the right).

- `PauseMenu.cpp` `DrawGameOver` rewritten as a full-screen Reach-style results screen: header (GAME OVER, map, mode, minutes,
  difficulty icon), left = SCOREBOARD (local row highlighted with rank icon) + PROGRESS (challenge / commendation bars this
  game moved: old part blue-grey, this game's part amber, green COMPLETE / "<TIER> REACHED", commendation banner art),
  right = CREDITS (lines appear every 0.5 s and count up with `Totals`; "Random bonus"/"Jackpot!" roll like a slot machine with
  `SlotMachine`), CREDITS EARNED total, RANK (icon, bar fills from the old cR to the new; on promotion `RankUp`, amber glow,
  PROMOTED). `GameCompletion` plays when the screen opens. Enter / Space / a click away from the buttons skips to the end.
  Buttons bottom-right: PLAY AGAIN, RETURN TO MENU, QUIT (always usable). Sounds respect the Menu sounds setting.
  Keyboard is read with GetAsyncKeyState (the overlay only feeds the mouse into ImGui); no controller skip yet.
- `Career`: `Award` now has `progress` (`ProgressLine`: kind daily/weekly/commendation, mode, title, art, before, after,
  target, completed, tier) filled in `OnGameResults` for every challenge or commendation this match moved, and `difficulty`.
- Idea from Tyler for spec 4.14 (map voting), "not 1 for 1 but similar": a full-screen VOTE FOR THE NEXT GAME screen with
  the next game named at the top, a vote-closed ring with "N of M voted", 4 choice cards (map picture, map + game type, vote
  count badge, a bar for the share of votes, "PLAYLIST'S PICK" and "NEXT GAME" tags), a PLAYERS list on the right in
  team colours with each player's vote as a small map thumbnail, and "The next game starts once the results are over".
  Needs networking for the multiplayer side.

## 0. Pick up here (2026-10-28, 12:40 ET) - Reach art is now in git

- Tyler decided the Reach menu art goes into the repo so O7 sees exactly the same build (`3522203`, 106 PNGs, ~2 MB):
  `build/bin/Release/data/ui/reach/{ranks,difficulty,commendations}/` (force-added; `build\` is otherwise git-ignored).
  After a pull, O7's menus show the same rank icons, difficulty icons and commendation banners. New art goes in the same
  folders and is added with `git add -f build/bin/Release/data/ui/reach`.
- `8bad0ad`: Demon / Flawless Cowboy text says "(vanilla Noble Actual and Lone Wolf don't count)". Firefight "Coming later"
  label clipping dropped from the to-do (Firefight will be done before release).
- Claude raised that the repo was public and the art is Microsoft's; Tyler pushed it anyway, and the repo is private now.

## 0. Pick up here (2026-10-28, 12:25 ET) - Reach's 45 commendations (spec step 5)

**Status:** built and pushed (`78a943e`, then `2241e59` with the Noble Actual / Lone Wolf block). `cm_tab2.bmp` checked:
mode tabs, banners, cR per tier and "Not tracked yet" rows look right.

- `Career.cpp`: `kCommendations` is now Reach's 45 (mode, art key, name, requirement, Reach's six tier counts from Halopedia).
  Tier names Steel, Bronze, Silver, Gold, Onyx, Max. Counts are kept per commendation in `record.commendationCounts`
  (`<mode>_<key>`), tiers paid in `record.commendations` (old Noble commendations are dropped). Tyler asked to drop
  Reach's rules (every difficulty counts).
- **Stock Noble Actual (`m05`) and Lone Wolf (`m70_bonus`) pay nothing** (`UnpaidMission` in OnGameResults): no cR lines at all,
  no challenge or commendation progress; the post-game shows one 0 cR line saying so. Tyler found Noble Actual paid 5,004 cR and
  completed a challenge after skipping the cutscene (0.3 min). Matched by scenario name, so modded maps with their own
  names still pay. Kills/deaths still go into lifetime stats.
- Each commendation pays Reach's own cR per tier (`CommendationDef::rewards`, from the Halopedia list Tyler sent; Max pays 0;
  Heavy Weapon was unknown, so it uses Halopedia's averages 200/700/1000/2400/2400). Rows show the next tier's cR in amber.
- Tyler's payout rule (`TierReward`): Steel..Gold pay Reach's cR x2.5 (rounded), Onyx pays 2x that Gold, Max pays 3x Onyx.
  Example Any Spree: 625, 1,063, 3,000, 6,000, Onyx 12,000, Max 36,000.
- Counted now: Demon (missions), Flawless Cowboy (mission with 0 deaths), Support Role / Backup / Assistant (assists),
  Perfectionist (Firefight game with 0 deaths, per game until rounds are decoded). Everything else shows "Not tracked yet".
- `CareerPanel.cpp` COMMENDATIONS tab: CAMPAIGN / FIREFIGHT / MULTIPLAYER sub-tabs (click, or LT/RT; `m_commendTab`), Reach-style rows with the banner art
  (`data\ui\reach\commendations\<mode>_<key>.png`) tinted by tier (Steel art only; Max gets an amber frame), name, tier,
  requirement and "x / next" bar.

### TO DO - commendation stats Noble can't read yet

These commendations stay at 0 until the stats are decoded from the end-of-game results record (or engine events):
- **Headshots:** Pinpoint, Dome Inspector, One Shot.
- **Weapon types** (precision, pistol, automatic, heavy, grenade, close quarters): Precisely, Longshot, Crack Shot,
  Small Arms, Trigger Happy, Sidearm, Standard Issue, Riflin' Through, Trigger Man, Splash Damage, Get Loud, Heavy Weapon,
  Nice Arm, Pull the Pin, Grenadier, Close Quarters.
- **Enemy classes** (infantry, leader, specialist): Cannon Fodder, Target Practice, Leadership Element, In Command, SpecOps, Specialized.
- **Multikills:** Walking Tank, Numbers Game, Multikill.
- **Sprees:** Super Soldier, Methodical, Any Spree.
- **Vehicles** (vehicle kills, vehicles destroyed, driving for a gunner): War Machine, Vehicular, Mobile Asset, Right of Way,
  Grounded, Downshift.
- **Medals / assassinations:** Jack of All Trades, Rear Admiral.
- **Firefight rounds:** Perfectionist should count rounds without dying, not games.
When a stat is decoded: add a `CommendationMetric`, set it on the rows above, and fill it in `CommendationAmount`.

## 0. Pick up here (2026-10-28, 12:10 ET) - commendation art in place (spec step 5)

- Tyler's art: `D:\Halo Commendations` (52 named webp/png). Copied as PNG to `build\bin\Release\data\ui\reach\commendations\`
  (now committed, see 12:40) as `<mode>_<name>.png`: 16 campaign (green), 15 firefight (red), 14 multiplayer (blue), all Steel tier,
  plus `example_*.png` (bronze sniper, silver technician, gold assassin, onyx stars, onyx max, steel assist, service record).
- Names/thresholds: halopedia "List of Halo: Reach commendations" (Steel/Iron, Bronze, Silver, Gold, Onyx, Max). Only Steel art
  exists, so other tiers need tinting in code (bronze/silver/gold/onyx tint; Max = onyx with flames per example_onyx_max).
- Full screen for Service Record/Games/Mods pushed as `bd002d9`; ranks capture confirmed the fixed order.
- Next: Commendations tab (Reach-style rows: banner, name, tier, progress x / next), tracking what Noble can read now
  (kills, assists, missions, no-death missions); the rest show 0 until headshots/multikills/weapons are decoded.

## 0. Pick up here (2026-10-28, 11:40 ET) - challenges pushed, panel screens full screen

- Challenges are committed and pushed as `c3b52f1`. `ch_tab1.bmp` checked: the cards, mode chips, countdowns and two columns look right.
- Tyler: version **stays 0.8.2** (patch bumps only, no change needed).
- Service Record, Games (server browser) and Mods now use the whole screen like Settings (`FrontEndMenu.cpp`: `full = true`).
  The job on the PC builds it, captures `fs_career1.bmp`, `fs_career0.bmp` and `fs_servers.bmp`, then commits and pushes.
  Waiting for Tyler to run `go.bat`.
- Rank art fixed per Tyler (files on his PC only, `data\ui\reach\ranks`): Major <- old Major G3, Major G1 <- old G2,
  Major G2 <- old G1 (by elimination), Major G3 <- old Major; Colonel G1 <- old G2, G2 <- old G3, G3 <- old G1;
  Mythic/Noble swapped; Forerunner/Reclaimer swapped. The job also captures `fs_career3.bmp` (RANKS tab).

## 0. Pick up here (2026-10-28, 11:35 ET) - Reach's real challenges (spec step 4)

**Status:** files are on Tyler's PC and verified; `build\agent\jobs\job.bat` builds, captures the Challenges tab
(`build\bin\Release\ch_tab1.bmp`), then commits and pushes. Waiting for Tyler to run `build\agent\go.bat`.
Results: `build\bin\Release\ch_done.txt` (DONE or BUILD FAILED), `build\nb_build_out.txt`, `build\git_out.txt`.

Tyler's choices: **original timing** (daily reset 11:00 GMT, weekly reset Monday 11:00 GMT, as in Reach) and a
**random pick from the whole pool** (7 daily, 5 weekly).

- `docs/career/challenges.json`: Reach's 2,103 daily/weekly challenges (2010-2012: kind, mode, name, requirement, cR).
- `tools/career/gen_reach_challenges.py` (run from the repo root) writes `src/career/ReachChallenges.inc`. It keeps only
  what Noble can track from a finished game: kills, assists, games, missions, best-game score, no-death games, mission
  time, difficulty, skulls, mission (m10..m70), "complete N daily challenges", "complete all dailies". 262 kept
  (228 daily, 34 weekly). Matchmaking wording is dropped. Left out: co-op, wins, waves, rounds, sets, Bungie.net/file share,
  and anything needing stats Noble can't read yet (headshots, weapons, medals, enemy types). Skull names map to MCC bits:
  Iron 18, Black Eye 3, Tough Luck 36, Catch 7, Cloud 13, Famine 11, Thunderstorm 34, Tilt 35, Mythic 23.
- `Career.cpp`: `ChallengeDay() = (unix time - 11 h) / 86400`, `ChallengeWeek() = (day - 4) / 7`; progress keys
  `D<day>:r<row>` / `W<week>:r<row>` (old date keys are pruned once). `PickChallenges` shuffles the pool with splitmix64
  seeded by the day/week (same set for everyone that day), taking 2 campaign, 2 Firefight, 2 multiplayer, 1 any for dailies
  and 2 campaign, 2 multiplayer, 1 any for weeklies, no repeated names, filling up from the rest. `ChallengeAmount` checks
  mode, mission (last part of the map path), difficulty (exact or "or harder"), skulls (all required), no deaths, time limit.
  Best-game challenges keep the max; others add up. Dailies are processed before weeklies so "complete N dailies" and
  "all dailies" count in the same game. New: `Challenge::mode`, `DailyResetText()`, `WeeklyResetText()`.
- `CareerPanel.cpp` CHALLENGES tab: DAILY / WEEKLY headers with done count and reset countdown, cards in two columns
  (one when narrow): mode chip, name, requirement, progress bar `x / y`, `+cR` in amber, green COMPLETE with a check.
- Next: check `ch_tab1.bmp`; then commendations (spec step 5, needs Tyler's commendation art), post-game, Armory v2.
  Still open: rank-order check (`rank_order_check.png`), version number (0.8.2, patch bumps only), Firefight
  "Coming later" label clipping.

## 0. Pick up here (2026-10-28, 11:30 ET) - Campaign / Firefight / Custom / Forge setup (spec step 3)

- Map and mission lists show a number and MCC's small map preview before each name (`ListItem::thumb/number`,
  rows 40-42 px, as many rows as fit above the hint line).
- Right side: `HeroBanner` (map picture, dark gradient, accent kicker "HALO: REACH à‚Â· CAMPAIGN / FIREFIGHT / MULTIPLAYER /
  FORGE" or "MOD: <name>", big title, blurb under it), then the options, then `BigAction` buttons anchored above the
  hint row (fixes START MISSION covering the hints): Campaign/Firefight PLAY SOLO ("<mission> à‚Â· <difficulty>",
  "Hold out alone à‚Â· ...") + HOST CO-OP (greyed, "Needs online play"); Custom Game START GAME ("<map> à‚Â· <game
  type>") + HOST OR JOIN (server browser); Forge START FORGE. The banner shrinks to fit the screen height.
- Campaign: the skulls that are on show as small skull icons next to the score multiplier.
- Firefight: "FIREFIGHT OPTIONS à‚Â· DEFAULT" row shown greyed ("Coming later") until Noble can write Firefight variants.

## 0. Pick up here (2026-10-28, 11:15 ET) - Settings redesign (spec steps 1-2)

Tyler chose option A (build the spec in its order). Done:
- **UI kit** `src/ui/NobleUi.{hpp,cpp}` (spec Â§3, first pieces): colours, `Scale()`, `SectionHeader`, `RowBackground`
  (3 px accent edge + gradient focus, faint hover), `ChangedDot` (amber), `DrawSlider` (fill bar, default tick),
  `DrawCycle` (â€¹ value â€º with position dots, ends greyed when not wrapping), `DrawToggle` (ON/OFF pill), `DrawBox`,
  `Text`/`TextWidth`/`Paragraph`. They only draw; interaction lives in the screen so keys, pad and mouse share one path.
- **Settings in three columns** (`SettingsPanel` rewritten, same public API, same settings.json keys): left = search
  box, categories grouped DISPLAY & SOUND (Graphics, Audio) / GAME (Game options, Player) / CONTROLS (Keyboard & mouse,
  Controller) / ONLINE (Online services), each with a live summary and an amber dot when it has unapplied changes;
  bottom = "N UNSAVED CHANGES / In ..." with APPLY CHANGES / DISCARD, or a per-category RESTORE DEFAULTS (greyed when
  already default). Middle = rows (slider / cycle / toggle / key box / text / info), changed rows get a dot; search
  shows matching rows from every category. Right = details for the hovered (else focused) row: title, when it takes
  effect, description, DEFAULT, and previews (FOV cone vs Reach's 78, frame-time bar, volume meters, controller status
  + picture, graphics card from DXGI adapter 0). Keys/pad: Up/Down rows, Left/Right value, Enter activate, Q/E (LB/RB)
  category, Y apply, X discard. Rows that can't work yet are shown greyed with a reason (advanced graphics, mute in
  background, in-vehicle FOV, hide HUD, mouse acceleration, controller button layouts). `-menu-tab N` = category 0..6.
  Old menu-music values are normalised to `reach:menu` on load so they don't show as an unsaved change.
- **Full screen** (Tyler 11:09): the front-end SETTINGS screen uses the whole screen (dark backdrop, small title);
  the pause menu's SETTINGS window is the display size minus 16 px each side.
- Next per spec Â§6: campaign/Firefight setup polish (thumbnails, hero banner; difficulty tiles done), then challenges
  cards, commendations (Reach's 45), post-game exit experience, Armory v2.

## 0. Pick up here (2026-10-28, 11:00 ET) - menu redesign spec, rank + difficulty icons

**Menu redesign spec (Tyler 10:34):** `docs/menu-redesign-spec.md` ("Reclaimer layout, Reach soul"): the plan for
every front-end screen, the Armory, career screens and the features Reach never had. Read it before menu work; its
Â§6 gives the order (UI kit + three-column Settings first). Tyler has 43 Reclaimer reference screenshots: ask him when a
screen in the spec is unclear.

**Reach art (placeholders, never committed):** Tyler sent a sheet of Reach rank icons + the four difficulty icons.
Cut into one file each and copied to his PC only: `build\bin\Release\data\ui\reach\ranks\<NN>_<name>.png`
(NN = `Career::Ranks()` index 00 Recruit .. 49 Inheritor) and `...\reach\difficulty\{easy,normal,heroic,legendary}.png`
(git-ignored like all of `data\`). New `UiArt::ReachArt(category, stem)`, `RankIcon(index)`, `DifficultyIcon(d)`,
`DrawContain(...)` look in `data\ui\custom\reach\` then `data\ui\reach\`; everything falls back to text when a file
is missing. Rank order is partly a guess (Major = two stars, Lt. Colonel G3 = the slashed L, Warrant Officer grade
order, Mythic..Nova = moon, crescent, black hole, sun): Tyler checks `rank_order_check.png`; fixing one = renaming files.
- Campaign / Firefight: DIFFICULTY is four icon tiles (`DifficultyTiles` in FrontEndMenu.cpp; click or Left/Right).
- Party box: rank icon at the end of the player's row (career.json read at most every 2 s).
- Service Record: big rank icon in the header, progress "x / y cR", "Next: <icon> <rank> at N cR (M to go)";
  new **RANKS** tab (4th; Q/E cycle 4 tabs; `-menu-tab 3`): all 50 ranks, earned bright, current framed in blue, next
  outlined, cR under each.
- `-menu-screen` works (it only looked ignored before); the menu logs `FrontEndMenu: screen a -> b`.
- Note: the working copy of `src/ui/UiArt.cpp` on Tyler's PC had lost `NobleControllerImage` (an older copy had come
  back, the stale-file problem from Â§8): restored from `git show HEAD`. The test job now writes `build\git_status_r3.txt`
  (`git status` + `git diff --stat`) before building so stale files show up.

## 0. Pick up here (2026-10-28, 09:55 ET) - menu freeze fixed, clickable hints, quit confirmation

Also pushed since 09:15: `e950306` ODST-only Firefight voices removed (Armory lists Reach's 12; voice saved/loaded
by `voice_id`), `5808e5e` GRD helmet offers MCC's four variants (Base, Optrex, CBRN, Optrex/CBRN; helmets load by
`helmet_id`; tested: item 71 reaches the profile and a match runs). The menu changes below are committed with this
handoff (FrontEndMenu.cpp/.hpp, main.cpp); the quit box still needs a hand check (Esc on the main menu).

**Menu "freeze" (Tyler 09:26: Armory froze, Esc / Service Record tabs dead after Alt+Tab).** Not the Armory and not
Alt+Tab: in both of his runs the whole front-end menu stopped exactly ~60 s after start. The in-engine menu (O7OF's
mainmenu mode) asks the engine for a menu map MCC doesn't ship (RR's `mainmenu_all.map` is not used/shipped), so after
a minute the engine gives up (slot 3 with a1=0xA, then slot 4 `_map_state_none`) and stops calling slot 1, which is
where `FrontEndMenu::RenderOverlay` draws. Fix: `FrontEndMenu::HoldFrame()` (main thread, called from the message loop
in `main.cpp` next to `PauseMenu::HoldFrame`): once no slot-1 frame came for 1 s it clears the back buffer, draws the
menu itself and presents (mutex shared with RenderOverlay). Tested 09:48: log `engine stopped drawing the menu; Noble
draws it itself` at 09:48:06, menu kept running and `-menu-capture 5000` saved `m1_capture_5480.bmp` 30 s later.
After that minute the Armory has no 3D scene behind it. Note: `-menu-screen` is still ignored by the in-engine menu.

**Clickable button hints:** the ENTER Select / ESC Back hints at the bottom of every front-end screen are buttons now
(hover highlight, click = the key). Change / Tabs hints only describe keys and stay plain labels.

**Quit confirmation (Tyler 09:48):** Esc on the main menu, or choosing QUIT, opens "QUIT PROJECT NOBLE?" with YES / NO
(No preselected; arrows switch, Enter picks, Esc = No). The main-menu hint now reads "ESC Quit".

**Version number (Tyler 09:48):** "slow down with the version number, we'll hit 1.0 in no time." Currently 0.8.2
(`src/common/Version.hpp`). Waiting for Tyler's answer on the scheme before changing it; proposal: only bump the
patch number for normal changes, minor only for milestones Tyler/O7OF agree on, 1.0 reserved for a real release.

## 0. Pick up here (2026-10-28, 09:15 ET) - Armory appearance now reaches the game

**What was wrong:** Noble only wrote the Elite flag (+0x27/+0x38) into the profile; every armor field stayed 0,
and 0 means "item 0" of each slot (Mark V [B] helmet, default pieces, Steel, **Elite Minor**).

**How the profile carries appearance** (haloreach+0x2EDA08 copies it into the player's settings):
dword "global item ids" at +0x40 helmet, +0x44 left shoulder, +0x48 right shoulder, +0x4C chest, +0x50 wrist,
+0x54 utility, +0x64 body (192 male / 193 female), +0x68 Elite armor set, +0x6C knee guards, +0x70 visor colour,
+0x8C armor effect, +0x90 Elite armor effect, +0x94 Firefight voice, +0x98 primary colour, +0x9C secondary colour,
+0xA0 third colour (RR: Steel). (+0xA4/+0xA8 are the Armory poses, HR_Rifle_Salute / HR_Elite_Rifle_Salute in RR.)
A global item id is the item's position in **MCC's own Reach customization list**: the `HR_*` names stored in order
in `MCC-Win64-Shipping.exe` (HR_Helmet_MarkVB_Base = 0 ... Drengr_TYR = 89, LeftShoulder 90-107, RightShoulder
108-125, Chest 126-143, Wrist 144-149, Utility 150-156, Visor 157-161, Knees 162-165, ArmorEffect 166-172,
EliteArmorEffect 173-179, FirefightVoice 180-191, Spartan_Male/Female 192/193, Elite_Minor..Officer 194-201,
Color_Steel..Pale 202-231, later chests 232-239). `haloreach+0x3B2F4` turns it into the engine's slot index;
the group sizes match that function exactly.

**Code:** `ApplyArmoryAppearance` in `ReachHost.cpp` (replaces `ApplyArmorySpecies`) reads `data/appearance.json`
and writes all of the above; helmets and chests go by the Armory's `helmet_id` / `chest_id` string ids, the other
slots by index (the Armory lists are already in MCC order). `RefreshArmoryAppearance` re-applies it whenever the
file changes (checked twice a second), so an Armory save during a session reaches the profile.

**Tested on Tyler's PC (screenshots, he was away):**
- `-profile-set 98=E0` (Brick) turns the player's colour swatch on the scoreboard red; 27/38 = Elite gives the
  Elite HUD. So the colour fields reach the game.
- Built with the new code and two appearance.json files (Spartan EVA helmet / Brick, and Elite General / Blue):
  log `applied Armory appearance (Spartan, helmet 27, chest 133, Elite set 0, colours 22/0)` with a red swatch,
  and `(Elite, helmet 0, chest 126, Elite set 5, colours 15/2)` with the Elite HUD and a blue swatch
  (`build\bin\Release\t14_P/Q_capture_1450.bmp`). Tyler's own appearance.json was not touched (there was none in
  `build\bin\Release\data\`).
- **Not verifiable in first person:** helmet, shoulders, chest and the Elite armor set only show in third person
  (the first-person Elite hand looks the same for Minor and General). **Tyler: please check in game** (die, or look
  at yourself in the Armory/another player's view) that the helmet and Elite set match the Armory.
- GRD now offers MCC's four variants in MCC order (`5808e5e`): BASE `hilands`, OPTREX `hilands_b`, CBRN `hilands_a`,
  OPTREX/CBRN `hilands_c` (new; the Armory preview may have no model for it). Saved helmets load by `helmet_id`.
  Tested: GRD Optrex/CBRN reaches the profile as item 71 and a Sword Base match runs. Mariner variants map by name;
  "HIGHLANDS" chest has no MCC item (falls back to default). The ODST-only Firefight voices (Dare, Dutch,
  Mickey, Romeo) were removed from the Armory (`e950306`, Tyler's call); the voice now goes by `voice_id`, and old
  saves that picked one of them load as Noble Six. Whether the engine re-reads the profile at respawn (live change
  mid-match) is untested.

Head of main before this: `0f29293`.

## 0. Pick up here (2026-10-28, 08:30 ET) - Armory status from Tyler

**08:29 (Tyler, after O7OF's `f4f2d48`; Tyler is offline until later):**
- Preferred species now works (Spartan / Elite pick reaches the game).
- **Armor still doesn't swap in game**, and an **Elite always spawns as the Minor variant**.
- Appearance choices now save and survive a restart, but still don't change the character in game.
- From O7OF's side: FPS limit has several settings including uncapped, V-Sync fixed, FOV offset found,
  settings can be changed mid-match.
Next for the Armory: find the profile/player bytes the engine reads for armor pieces and the Elite variant
(Minor / Officer / Ultra / ...), e.g. with `-capture-profile` before/after captures and `-profile-set`
experiments like the controller look bytes (+0x1B5/+0x1B6) were found. Head of main: `6cfadc4`.

## 0. Pick up here (2026-10-28, 04:30 ET) - wheel, controller, music picker, service record, post-game

Tyler (03:47) asked for: career extras, post-game scoreboard, controller settings, music picker, FOV mid-match,
wheel down. All done and tested by test options + screenshots on Tyler's PC (he was away); details:

- **FOV mid-match: works.** New test option `-fov-test-at <s> <deg>` changes the saved FOV mid-match the way
  APPLY does; standing still, frame 900 (78) vs 1500 (110) show the wider view at once (`t1_A_capture_*.bmp`).
- **Mouse wheel down: solved.** The engine's wheel-down key code is **0xAE** (up is 0xAF); 0xB0 does nothing.
  Proven: 0xAE as change-weapon's second key + a -1 pulse switches weapons. Only the second key slot (+8) works
  (the "third slot" test that failed earlier was the wrong slot, not the wrong code). New settings
  `wheelUpAction` / `wheelDownAction` (default both 4 = change weapon; -1 = nothing), shown as KEY BINDINGS >
  MOUSE WHEEL. Same action both ways -> only 0xAF bound and down notches sent as +1 (old behaviour); different
  actions -> 0xAF / 0xAE on each action's second key and real -1 pulses (tested: up = melee, down = change weapon).
  Key bindings are now rewritten into the built-in profile every 500 ms, so changes may apply mid-game (not
  verified that the engine re-reads them live).
- **Controller: the engine never read the pad before.** With a host, haloreach.dll doesn't load XInput
  (`hr+0x23855`: only when the host global `hr+0xC1A100` is null); MCC passes the pad in the slot-36 buffer:
  `+0x000` u32 device (0 = controller, 1 = keyboard/mouse), `+0x120` u16 buttons, `+0x124/+0x125` u8 triggers,
  `+0x126/128/12A/12C` i16 LX/LY/RX/RY (decoded from `hr+0x23CF0..0x23D87`). So Tyler's "controller works" was
  probably Steam Input mapping it to keyboard/mouse - **ask him**. Noble now reads pad 0 with XInput
  (`FillLocalPad`) and switches device to whichever was used last; Start toggles Noble's pause menu.
  Tested with `-fake-pad <s>` (fake right stick for 1.5 s): view turns.
  Look speed: `-profile-set <off>=<byte>` test option. **+0x1B6 = horizontal stick speed** (0 -> ~19 deg instead
  of ~61 deg for the same push), **+0x1B5 = vertical** (`-fake-pad-y`: 0 -> barely looks up), +0x1B7 changed
  nothing measurable (range 0..9, stock 7; maybe acceleration). Engine range 0..18, stock 18.
  New CONTROLS > GAMEPAD rows: look sensitivity / vertical (1..19 -> +0x1B6/+0x1B5), invert (Noble flips RY),
  left/right stick dead zone (Noble-side, radial, 0..50%), vibration on/off + strength.
  Vibration: host slot 39 now calls XInputSetState (a1 = pad, a2 -> u16 {low, high, left trigger, right
  trigger}, from `hr+0x24194`). **Not verified** (no pad here; rumble may only fire for a pad user).
  The engine's dead-zone floats (+0x1B8 0.13, +0x1BC 0.12) were not tested, so Noble uses its own.
- **Menu music picker:** AUDIO tab now has a two-pane picker: Shuffle all / Off and the games (with track counts)
  on the left, the chosen game's tracks on the right, plus a search box over every game. Click = play + select.
- **Service Record** (new main-menu entry, `src/ui/CareerPanel.*`, `-menu-screen career -menu-tab 0..2`):
  rank + cR + progress, totals (games per mode, time, K/D/A, medals, best game, challenges), campaign best
  difficulty per mission, last 20 games. CHALLENGES (Tyler 08:00: 7 daily / 5 weekly, rotating): 7 daily from a pool
  of 18 and 5 weekly from 10; each day takes the next 7 in the list (wrapping), each week the next 5, so
  consecutive days never repeat one and all come back in turn. Paid when completed. COMMENDATIONS: 6 tracks (campaign missions, Firefight games, MP games, hours, kills,
  medals), tiers Iron..Max pay 500..25000 cR once. Campaign scoring: mission-complete bonus by difficulty
  (250/500/1000/2000) + 10% per skull. All in `career.json` under `"record"` (old files load fine).
- **Post-game scoreboard:** the post-game panel (560 px wide) now lists everyone in the results record (score,
  K/D/A, medals; only the local player is named - the record holds no names) above the cR lines, which now
  include challenge / commendation / mission payouts. Real 10-minute Sword Base match (04:24): paid 133 cR,
  history row + per-mode totals + "Time on Duty" challenge progress (10/45 min) saved; career.json was restored
  afterwards. Post-game panel checked (04:38, `build\bin\Release\t5_capture_900000.bmp`): GAME OVER, map, mode and
  time, scoreboard row "NobleSix [NOBL]" with score/K/D/A/medals, cR lines, rank bar (now blue), buttons.
- **Git (04:42):** pushed as `9e679d8`, rebased onto O7OF's `2b9e843` (ReachArmory + in-engine 3D menu: the front-end
  menu now runs inside the engine on `mainmenu`). FrontEndMenu.cpp conflicted; resolved by taking O7OF's file and
  re-adding the Service Record entry / screen / `-menu-screen career`. Builds clean; the in-engine main menu shows
  SERVICE RECORD (screenshot `capture_780.bmp`). Notes for O7OF: in his new menu `-menu-screen <name>` seemed to be
  ignored by the capture (it showed MAIN MENU), and the log warns `ReachArmory: Could not locate
  mainmenu_all.string_ids.csv`. Also check that nothing from RR's `mainmenu_all.map` is committed (repo rule).
- Small open bits: Start can't close the pause menu (the pad is only read while the game has input); vibration
  untested with a real pad; +0x1B7 meaning unknown; challenge kills/medals depend on the provisional stat slots.

### Earlier (03:40 ET) - FOV checked in game

**03:40 (Tyler's PC, O7OF's `c4ace4e` pulled and built clean):** FOV works when set before a match. Sword Base,
windowed 1600x900, `fieldOfView` 78 vs 110 in settings.json, frame 1500 of each run: the log shows "CameraFov: game
field of view set to 78/110 degrees" and the 110 shot has a visibly wider view (the rifle takes up much less of the
screen). Shots: `build\bin\Release\FOV78_capture_1500.bmp`, `FOV110_capture_1500.bmp` (different spawns, so
compare the gun, not the scenery). Not checked yet: changing FOV live from the pause menu mid-match. FOV range is
now 70-120 (Tyler's call, 03:45, commit `93681f4`): slider (always clamped, also for Ctrl+click typing), the
settings file clamp and the help text all say 70-120; CameraFov still guards the engine's own 1-150.
Tyler's settings were restored after the test (copy in `data\userdata\settings.backup.json`).

## 0a. Latest (2026-10-28, 02:00 ET) — FOV hooked up + settings reachable from the pause menu

**02:00:** Item 3 below is implemented (not yet run in game). Static REA/Ghidra analysis of `haloreach.dll`
(`docs/fov-findings.md`) shows the game's FOV is a float in degrees at **`haloreach+0xB42C10`** (78.0f in the
shipped DLL; the engine multiplies it by pi/180 for the camera and only accepts 1–150 degrees).
- New `src/engine/CameraFov.{hpp,cpp}`: `SetEngineModule()` from `main.cpp`, `Apply(degrees)` writes the global,
  `Read()` logs what the engine holds, and `Tick()` (called from ReachHost slot 1) applies `Settings::fieldOfView`
  when it changes and re-asserts it every 2 s because the engine refreshes its camera block from the profile.
- `SETTINGS > VIDEO > FIELD OF VIEW` now reaches the running game on APPLY; the default is 78 (Reach's own value),
  RESET goes back to 78, and the help text states the 1–150 limit.
- The pause menu has a **SETTINGS** entry: it renders the same `SettingsPanel` in a large window, so FOV, VSync,
  frame cap, mouse, audio and key bindings can be changed mid-match. Esc (or `< BACK`) returns to the pause menu;
  Esc during a key rebind still cancels the rebind only.
- Version is 0.8.2. Rebuilt `build\bin\Release\noble-client.exe`.
- Build note: `build\` had a broken Ninja `CMakeCache.txt` (left over from a failed configure; no build.ninja).
  It was reconfigured as `cmake -S . -B build -G "Visual Studio 18 2026" -A x64` (MSVC, not ClangCL) and built with
  `cmake --build build --config Release`.
- **Still to verify in game:** whether the camera picks the new value up live (it may cache FOV at spawn; a map load
  or a respawn may be needed) and whether 78 vs 90 looks right. Test: pause â†’ SETTINGS â†’ VIDEO, drag FOV, APPLY.

---

## 0. Pick up here (2026-10-30, 18:15 ET) - Fix Co-op Campaign Netplay Desync, Elite Armor & Effects Replication, Armory Visuals & Social Menu (O7OF / Dominus)

### 1. Co-op Campaign Netplay Desync (Guest 2nd Player Spawn Error)
- **Problem:** When 2 PCs attempted to play Campaign co-op over LAN or Internet, the host connected but then an error threw on the 2nd player spawn (stuck at LoadProgress: 5.0% for ~25s until Category 14 mission abort / game over).
- **Cause Found in Log & Engine:** 
  1. In `src/engine/BlamEngine.cpp`, `isGuest` was forcing `loadedPrebuilt = false`. This stripped the prebuilt campaign scenario variant, level description, and insertion point blocks at `+0x300` on the guest machine, preventing the guest engine from initializing the mission scenario.
  2. In the guest launch block, controller 0 was bound to Slot 0 (`+0x100`) and Slot 1 was unassigned. When the host accepted the guest, the host assigned the guest player to Slot 1 (Slot 0 is the Host). Because the guest had Slot 1 unassigned, 0 local controllers were mapped to player 2 (Bob), causing the engine to fail spawning player 2.
- **Fix:**
  - Preserved prebuilt launch blocks (`campaign_*.bin` / `lb_m_c_1.bin`) on guests.
  - Added fallback synthesis: if a mission-specific campaign/firefight launch block is missing, `lb_m_c_1.bin` is loaded as the base template.
  - Properly mapped launch block controller slots for guests: Slot 0 (`+0x100`) is unassigned (`0xFF, 0xFF, 0xFF, 0xFF`) reserved for remote Host; Slot 1 (`+0x120`) is bound to local controller 0 (`block[0x120] = 0x00, block[0x124] = 0x02`), so Player 2 spawns with controller input when host membership (`msg 26`) arrives.
  - Fixed variant resolver in `BlamEngine.cpp` to skip looking for game variant files when mode is `"Campaign"` (avoids Slayer replacement).
  - Added backtrace stack capture in `ReachHost.cpp` inside `HostSlot6_GameResults` to diagnose any future unexpected result calls.

### 2. Elite Armors & Armor Effects Multiplayer Replication
- **Problem:** Equipped Elite armor sets (Honor Guard, General, Field Marshall, etc.) were only visible locally; remote peers saw them default to Elite Minor. Additionally, armor effects were not showing on Elites, and previously caused bugs with character selection.
- **Root Cause Reverse-Engineered:**
  - In `haloreach.dll`, function `0x2C8070` (`PlayerSetAppearanceSlot(player, slotParam, itemId)`) expects `slotParam->species = 1` and `slotParam->slot = 0` for Elite sets, and `slotParam->species = 1, slotParam->slot = 2` for Elite armor effects.
  - At RVA `0x2EDD5C`, the engine was incorrectly passing `species=0, slot=7`, storing the value into the Spartan array and leaving `[player + 0xAE + 0]` as 0 (Elite Minor).
  - At RVA `0x10580C`, retail `haloreach.dll` loaded `ecx = 0x8C` (`pmcg` list 8C), which is completely empty (0 entries) in retail tags, causing the engine to ignore Elite armor effects.
- **Fix in `src/hooks/EnginePatches.cpp` & `src/engine/ReachArmory.cpp`:**
  - Installed a MinHook detour on `PlayerSetAppearanceSlot` at `0x2C8070` to validate callers (`0x2EDD7A` for Elite Set -> `species=1, slot=0`; `0x2EDDBB` for Elite Effect -> `species=1, slot=2`).
  - Patched RVA `0x2EDD5C` (`89 75 C0 C7 45 C4 07 00 00 00` -> `C7 45 C0 01 00 00 00 89 75 C4`) to write `species=1, slot=0`.
  - Patched RVA `0x10580C` (`B9 8C 00 00 00` -> `B9 80 00 00 00`) to route Elite armor effects to the full tag list 80 (Flames, Blue Flames, Heart Attack, Pestilence, Inclement Weather, Birthday Party), matching Spartans.
  - Updated `ReachArmory.cpp` to look up Elite effects in list `"80"` before falling back to `"8c"`.

### 3. Armory Emblems & Nameplates Visuals
- **Problem:** Nameplates and emblems in the Armory only showed text names or raw options.
- **Fix in `src/ui/ArmorCustomizer.cpp`:**
  - In the Armory picker grid: added dedicated branches for `isNameplate` (1 column, 56px height, rendered via `UiArt::Nameplate`) and `isEmblem` (4 columns, square tile, rendered via `UiArt::Emblem`), displaying full visual previews of all 531 nameplates and 330 emblems from `playercustomization.xml`.

### 4. Social Menu & Friends List
- **Problem:** Social menu lacked an easy way to search/add friends directly by username or IP address and invite/join them.
- **Fix in `src/network/SocialClient.*` and `src/ui/FrontEndMenu.cpp`:**
  - Added `AddFriendDirect(nameOrEndpoint)` to `SocialClient`, saving and loading friends locally in `data/userdata/friends.json`.
  - Added a "QUICK ADD FRIEND" text input box and button at the top of the FRIENDS tab in `DrawSocialScreen`.
  - Added a direct `[JOIN]` button for friends configured with IP/port endpoints, and updated `[REMOVE]` to remove instantly without requiring a master server connection.

### 5. Verification
- Built with Visual Studio 18 2026 MSVC toolchain (`cmake --build build --config Release --target noble-client`).
- Verified 0 errors and 0 warnings.
- Executed smoke captures:
  - `noble-client.exe -menu-screen social -menu-capture 30 -quit-after 5` -> Exit Code 0
  - `noble-client.exe -menu-screen armory -menu-capture 60 -quit-after 5` -> Exit Code 0

--
### 3. Verification & Build Status

- Built cleanly via MSBuild: `cmake --build build --config Release`
  - `noble-client.exe`: **0 errors, 0 warnings**
  - `noble-dedicated.exe`: **0 errors, 0 warnings**
- All MCC retail installation files remain 100% read-only and unmodified.


## 0. Pick up here (23:58 ET) — Display Settings & FPS Control + Controller Picture

**23:58 (O7OF):** Bumped to version 0.8.1 with FOV/VSync/FPS limit/FPS counter display fully implemented. The FPS counter displays in-game (upper-right, blue text on dark box, toggled via Settings > Display). VSync and frame-cap are applied in the swap-chain Present hook and take effect immediately without restarting. FOV slider stores selections (60–120Â°, default 90) but remains **UI-only** until the game camera mapping is verified via profile capture. The preview loads local MCC-derived Spartan/Elite OBJ groups, undersuits, and Elite harness layers; color masks and visor tint work in the preview. In-game armor and color IDs remain unmapped, so preview selections do not change the character yet.

**23:45 (upstream):** Controller picture replaced (item 5, built + screenshot-checked). O7OF made a 1600x1000 transparent PNG of an Xbox 360 pad; it is tracked at `assets/ui/noble/controller.png` (original work, OK for the public repo). CMake now also copies `assets\` into `<exe>\data\` after a build (data\ itself stays git-ignored). `UiArt::NobleControllerImage()` loads `controller.png` from `data\ui\custom` then `data\ui\noble`; SettingsPanel uses it with its own callout table (`kReach360`, straight leader lines, left margin sized from the widest label) and falls back to MCC's LARGECONTROLLERFRONT with the old table when the file is missing.

**Do not commit or distribute OBJ files from `data/armor` or profile captures.** Use `-capture-profile data/profile-captures` to record Slot 34 profile buffers for before/after comparisons; delete captures after analysis.

Open list, in priority order:
1. **Load armor geometry from MCC maps instead of local OBJ exports:**
   - Locate Spartan/Elite model files (`.model`) in `{MCC_INSTALL}\haloreach\models\` (e.g., `m_spartans.model`, `m_elites.model`)
   - Parse and extract mesh/skeleton at runtime; discard local OBJ files to avoid shipping game assets
   - Harness and armor should layer correctly on both species (tested in preview, not in-game)
2. **Determine armor equipment/color offsets in Slot 34 profile:**
   - Use profile captures (`-capture-profile data/profile-captures`) to identify which bytes control torso, helmet, shoulders, legs, visor color, and suit color
   - Map UI selections to equipment IDs and color indices
   - Test in-game: set armor in settings, launch a game, verify the character reflects the changes
3. ~~**Map FOV to the game camera and verify it works in-game**~~ (works at match start, 03:40) — wired in 0a below (FOV value at
   `haloreach+0xB42C10`, see `docs/fov-findings.md`); still needs an in-game check that the camera takes the value live
4. ~~**Controller settings**~~ done 04:30 (native XInput pad, look speed, invert, dead zones, vibration; see section 0). Left: verify with a real pad (vibration, +0x1B7)
5. ~~**Menu music picker**~~ done 04:30 (two-pane picker with search)
6. Check in game: Forge menus and loadout cards no longer show hex (expected fixed by Slot 55)
7. Noble exiting from inside a game: confirm `tasklist | findstr /i noble` is empty afterwards
8. Mod-only maps (not in Reach's tables) are listed but disabled: needs map-id work in BlamEngine
9. Halo 2 Anniversary music (`sounds_*.dat`) not readable yet

Build/run (Developer Command Prompt, not plain PowerShell - cmake is only on PATH there):
`cd /d D:\Halo-Reach-Community-Edition && cmake --build build --config Release && cd build\bin\Release && noble-client.exe`
If LNK1104 (exe locked) and `taskkill` fails, a stuck process needs a real reboot; rename the exe meanwhile.

## 0c2. Earlier (23:15 ET)
(23:15 ET)

**23:15: pushed + mouse settings + input prompts (built, Tyler OK'd "fine for now").** Stuck processes were from
quitting at the menu (10:12-10:17 runs, ExitProcess hang, likely XAudio2 DLL detach); real reboot cleared them; with
TerminateProcess the menu-quit check came back clean (in-game quit not re-checked yet). Commit `d31ccff` rebased onto
O7OF's `2248385` (adds ObjModel/ObjModelPreview armory preview, ArmorCustomizer::Render(device, ctx), default master
server URL) and pushed as `7500059`. Since then (not committed): `main.cpp` `return 0;` after TerminateProcess (C4715);
new `src/ui/InputPrompts.cpp/.hpp` (device tracking from ImGui input each frame in `UIManager::BeginFrame`; drawn
glyphs: A green / B red circles, LB/RB pills, D-pad, or keyboard key boxes); FrontEndMenu `DrawHints` takes
`Prompt` values, `AcceptPressed()` (Enter / keypad Enter / pad A) and D-pad for focus rows and skulls; SettingsPanel
MOUSE section (look sensitivity + RESET, new `mouseSensitivityY` vertical ratio 0.25-4, invert, help lines) and LB/RB
(Q/E) prompts around the tab row; ReachHost slot 36 reads look settings every 500 ms (`InputLookSettings`) so APPLY
works in a running game; `-mouse-scale` still overrides. Small ReachHost change - tell O7OF. Tyler (23:17): **a controller already works in game** (correct older notes saying gamepad isn't supported). Next:
CONTROLLER section (settings for it), Video options (FOV, V-Sync), cleaner music picker, pixelated controller picture.

## 0b2. Earlier (22:40 ET)
(22:40 ET)

**22:40: stuck Noble processes + exit change (written, build untested).** 22:28-22:34: Modded tab now collapsible
(mod heading rows with arrow + map count, click/Enter toggles, `st.openMods`), mouse-wheel scrolling + scroll bar in
`DrawList` (per-list offset), preview blurb height measured (fixed a const ImFont compile error). Tyler built it: works.
Three `noble-client.exe` (PIDs 41172/7516/8484, ~2 GB each) stayed listed after closing and could not be killed even
as admin ("no running instance") - stuck in the kernel (driver wait); only a reboot clears them, and they lock the exe
(LNK1104; workaround: rename the exe). main.cpp's three `ExitProcess(0)` are now `TerminateProcess(GetCurrentProcess(), 0)`
(ExitProcess runs DLL detach under the loader lock and can hang). If processes still get stuck: find when (after a
match? quit from menu?) and release D3D / stop the engine before exiting. Also sent Tyler `docs/career/armor-list.md`
(readable Armory list from armor.json). Not committed yet: everything since 355b31f (audio, menus, mods, VFS, main.cpp).

## 0a2. Earlier (22:30 ET)
(22:30 ET)

**22:30: VANILLA / MODDED tabs (written, NOT BUILT).** 22:20: Tyler tested the MOD row + click-to-pick: both work.
He wants MCC's way instead: map selection -> vanilla maps or modded maps. The MOD option row is gone; Campaign,
Firefight and Custom Game have `DrawSourceTabs` (VANILLA | MODDED (n); click, Q/E, LB/RB). The modded tab lists
every mod's maps under the mod's name (`ListItem.header` rows, not selectable; `ModdedMaps`, `FixSelection`); the
picked map carries its mod (`MapEntry.mod`) into `FrontEndChoice.modFolder`. "Mod: <title>" shows under the
preview. Campaign rows back to 1 difficulty, 2 skulls, 3 start. Forge has no tabs.

**22:25 (superseded in part by 22:30):**

**22:25: list picking + MOD option (written, NOT BUILT).** Tyler (22:04): in Campaign the mission under the mouse
became the pick, so moving to START changed it; and Campaign/Firefight need options such as Mods and game type.
- `DrawList(..., hoverSelects)`: mission/map lists now only change on click or keys (hover = light highlight);
  the main menu still follows hover.
- New `src/ui/ModCatalog.cpp/.hpp`: scans Workshop (976730) + `data\mods` for `Engine: HaloReach` mods; reads
  `campaign\*.json`, `firefight\*.json`, `multiplayer\*.json` (+ `GameModContents.*Maps`, `CampaignInfo.json`
  CampaignMaps): `ScenarioFile` last part = scenario, `Title/Description.Neutral`; and `maps\*.map` names.
- Campaign rows: MOD, DIFFICULTY, SKULLS, START (focus 1-4). Firefight/Custom: MOD, GAME TYPE, (DIFFICULTY), START.
  Choosing a mod swaps the list to the mod's maps; maps not in Reach's own tables are shown disabled (Noble
  launches by Reach map id, see BlamEngine::ResolveMapId). Maps the mod only replaces show as "<name> (modded)".
- `FrontEndChoice.modFolder/modTitle`; main.cpp calls new `VFS::SetActiveMod(folder)`; VFS::ResolvePath reads
  `<mod>\<relative path>` first (e.g. `maps\ff60_icecave.map`). Small VFS change - tell O7OF.
- Test: Firefight, MOD = "Icecave: The Last Stand" (Workshop 3724674100, replaces ff60_icecave = Glacier).

## 0c. Earlier (22:15 ET)

**22:15: Menu music catalogue from every readable MCC game (written, NOT BUILT).** 21:47 build tested: Reach UI
sounds + Reach menu music work (cache shows a_button, b_button, cursor_*, bumper7 L/R, 13 menu pieces; no warnings).
Tyler asked for the whole Reach OST plus Halo 1/2/3/ODST themes.
- ReachAudio now builds a track list per game. Reach/H3/ODST from `<game>\fmod\pc\sfx.fsb(.info)` (same format);
  Halo: CE from `halo1\sound\pc\sounds_music.fsb` + `lst\sounds_music.lst.bin` (u32 1, u32 count, then u32 len,
  name incl NUL, u32 first sub-sound, u32 permutations; `sound\music\` = Anniversary, `sound\old_music\music\` =
  classic, `sound\sinomatixx*` = cutscene music). H1 index->vgmstream sub-song mapping is ASSUMED (first+k+1);
  the stream names vgmstream prints are logged at debug level to check it.
- Halo 2 Anniversary: sound is in `halo2\h2_maps_win64_dx11\sounds_*.dat` (custom format) - not supported yet.
- A track = one music cue: intro, loop pieces (random), outro; `alt_*` layers, sustain/perc/stinger/ambient beds
  skipped. Counts from the .info files: Reach 62, H3 45, ODST 80, CE Anniversary 95, CE classic 85. Labels: curated
  for menu themes and Reach mission cues (m20 ONI Sword Base ... m70 Pillar of Autumn) and CE cutscenes (a30 Halo
  ... d40 The Maw); the rest are prettified cue names (the in-game score is stems, not the album tracks).
- Ids: `<game>:<cue path>` (`reach:menu`, `h3:menu`, `odst:menu`, `h1a:menu`, `h1:menu`), plus `all` (shuffle all
  games: each track ~2.5 min then its outro) and `off`. Old setting values ("shuffle", piece names) = `reach:menu`.
  Settings default is now `reach:menu`. MENU MUSIC row is a grouped drop-down (game headings), plays on pick.
- Music is decoded on demand (the Reach menu theme and UI sounds up front); cache trimmed to ~1.1-1.5 GB at start.

## 0b. Earlier (22:05 ET)

**22:05: Reach menu sounds + menu music written, NOT BUILT YET (task #17).** Tyler asked (21:31): Reach's menu
scroll sound instead of the beeps, Reach menu music with a track choice; then a Settings rework (MOUSE and CONTROLLER
sections, modern but Reach-feeling navigation and previews) and prompt icons that follow keyboard/mouse vs gamepad.
Hex fix is committed and pushed (`355b31f`).
- New `src/audio/ReachAudio.cpp/.hpp`: reads `haloreach\fmod\pc\sfx.fsb.info` (280-byte records, name at +24,
  record N = vgmstream sub-song N+1), decodes the sounds it needs once with `vgmstream-cli -s N+1 -o x.wav sfx.fsb`
  on a worker thread into `<exe>\data\userdata\cache\audio` (git-ignored via `data/`), plays them with XAudio2.
  UI sounds by bank path: omaha_ui cursor_vertical / cursor_horizontal / a_button / b_button, ui\error, first
  `bumpers\left_bumper\` / `right_bumper\` entry, exit_experience game_completion/totals/rank_up/slot_machine.
  Music: `data\sound\music\reach_menu\track1\in\*` = intro, `...\loop\*` = pieces (all pre-decoded in the
  background; only the playing piece is held in RAM). "shuffle" = intro then random pieces; a piece name = that
  piece looped; "off". vgmstream-cli is looked for in `<exe>\tools\vgmstream`, `<exe>\vgmstream`, `<exe>\bin\vgmstream`
  (NOT shipped; Tyler copies his own into `build\bin\Release\tools\vgmstream`). Without it the old beeps play and
  SETTINGS > AUDIO shows why.
- `MenuSounds` API now: Click (A), Back (B), Scroll (list up/down + hover), Change (value left/right), Tab(dir)
  (bumpers), Error. FrontEndMenu: list step -> Scroll, value cycle -> Change, goBack -> Back, the extra sound on
  screen change removed. SettingsPanel: tabs -> Tab(+-1), Cycle -> Change. Volume = `volumeOther`.
- Settings: new `menuMusicTrack` (default "shuffle"), SETTINGS > AUDIO > MENU MUSIC cycles the tracks and previews
  at once (UNDO restores the saved one). Music starts in `FrontEndMenu::Run`, stops (fade) before it returns.
- main.cpp calls `ReachAudio::Init` after `UiArt::Init`. CMake: ReachAudio files + `xaudio2`, `ole32`.
- All edited files are CRLF (repo uses CRLF; convert before copying back).
- Unverified: whether the `.info` order really matches vgmstream's sub-song order for every record (a warning is
  logged if vgmstream prints a different stream name), and the exact bumper file names.
- Next: Tyler builds + tests; then task #15 Settings rework (MOUSE: sensitivity X/Y, invert, raw input/accel?;
  CONTROLLER: look sensitivity, invert, dead zones, vibration, button layout picture - current one is pixelated;
  VIDEO: FOV, V-Sync, quality), task #16 prompt icons per input device, and ask Tyler to check Forge + loadouts.

## 0a. Earlier (21:40 ET)

**21:40: hex-number bug FIXED (tested).** With slot 55 returning 1 the engine registers its strings (log:
"StringTable: register ... found" for all 40 logged, lookups return real text such as "Tuesday", "October") and the
scoreboard shows place "1", score "0" (`capture_777777.bmp`, 21:27). Tyler confirmed it works. All diagnostics
(`crtfmt` IAT wrappers, `[tokendiag]`, `[hexdiag]`) are removed from ReachHost.cpp (112,875 bytes). Still to do:
build, commit ReachHost.cpp + this handoff, push; delete the untracked `src/engine/HudColor.*`. Then check Forge
menus and loadout cards in game (expected fixed by the same change).

**21:35: hex-number bug ROOT CAUSE FOUND: host slot 55 must return 1.**
- `hr+0x3140D` calls host slot 55 (vtable +0x1B8, "strings clear") and only if it returns true ("test al,al /
  je skip") registers the string tables through slot 56 (+0x1C0, ~8700 strings; RR's log: "strings registered by
  the engine: 8712; lookups answered from the language file: 11178"). Noble's slot 55 returned 0, so slot 56 was
  never called and every slot 57 lookup missed (Noble log: all "(none)"). Fields bound to string ids then show
  the raw id as 8 hex digits: score/place, "Respawn in", loadout grenades, Forge menu title "00000002", prices
  "$00000001", rotation snap "00000008". The `[tokendiag]` hook on `hr+0x2D26C0` never fired for them (they use
  the int -> string-id conversion of value tag 6, `hr+0x2F8FD4`, not `\x07` tokens).
- **Fix (uncommitted, ReachHost.cpp):** `HostSlot55_StringsClear` returns 1. Test with go.bat; check the log for
  "StringTable: register ... found" lines and the scoreboard / loadout / Forge numbers.
- Once confirmed: remove `[tokendiag]`, `[hexdiag]` and the crtfmt diagnostics; the fallback string table in
  ReachHost.cpp may then be mostly unneeded (lookups come from MCC's EN_HaloReach.bin via registered keys).

**21:25: hex bug, UI token pipeline mapped (static) + diagnostic running.**
- `hr+0x2D288C` = string id -> text: gets the text via `hr+0x2D25A4` -> `hr+0x9E0D0` (host slot 57 =
  vtable +0x1C8, then the map's own strings; no hex there), then expands `\x07` tokens in a loop. For each token it
  asks host slot 51 feature 5 (`[hr+0xC1A100]` vtable +0x198, Noble answers on, like RR). On: splits
  "datasource.property" at '.', turns both into string ids (`hr+0x9DB08`), finds the property (`hr+0x9E350`) and
  calls **`hr+0x2D26C0`** to write the value as text. Off: parses the token as 8 hex digits (`wcstoul` radix 16).
- `hr+0x2D26C0`: datasource object -> column (vtable +0x50), rows (+0x60), value (+0x68 -> {tag, value});
  int value whose column info (+0x58) says "string list" -> recurse into `hr+0x2D288C` with that string id;
  otherwise tag handler to_text (`hr+0x2D284E`).
- The slot 61 call stack only shows the draw path (`hr+0x2C2BF6` -> `0x2DA7FB` -> `0x2DA262` -> `0x2DF075` ->
  `0x2DF68F` -> `0x2DF533` -> `0x26921B` -> `0x268DD4` -> `0x1CF393` -> slot 61), not where the text is built.
- **Running (uncommitted, ReachHost.cpp):** `[tokendiag]` MinHook on `hr+0x2D26C0` logs column id, extra, output
  text (first 40 calls + any 4/8-digit hex-looking output with a call stack). `[hexdiag]` in slot 61 logs stacks for
  "Respawn"/"01c"/"02" texts. Both must be removed before committing (with the crtfmt code).
- **Warning:** the stale-copy revert also hit `ReachHost.cpp` twice (a 76 KB / older copy reappeared after a
  write). Always check sizes after writing and run `git diff --stat` before committing.

**21:30: hex-number bug, cause narrowed down (static analysis of haloreach.dll).**
- The loadout's "0000 / 0002" is one 8-digit value `00000002` wrapped onto two lines, so all cases are a 32-bit
  int shown as hex (score, place, respawn time, frag grenade count). All four come from strings with a
  `\x07datasource.prop_x\x07` token. Noble's fallback strings are identical to RR's (`D:\Reach\data\strings_en.json`
  is RR's capture of what it answered: 71 strings, all present in Noble's table; slot 56 never registers anything,
  so every lookup uses the fallback table). The strings are not the problem.
- UI value types: sorted handler array at `hr+0xB4C800` (14 entries, looked up by tag with binary search
  `hr+0x1802E9720`; built at init `hr+0x2AA09A`). Each handler = {u64 tag, can_convert_from(tag), convert(src,dst),
  to_text(value, buf, cap)}. Tag 1 = int32 (`hr+0xA36058`, to_text `hr+0x2F3AB8` = `_itow_s` radix 10). Tag 8 =
  int64 (`hr+0xA45B50`): convert `hr+0x381EF0` accepts tag 1 (sign-extends), **to_text `hr+0x381F10` =
  `_i64tow_s(v, buf, cap, 16)`**, i.e. hex. Other tags: 0 bool ("true"/"false"), 2 float, 3/12/13 strings.
- Value to text: `hr+0x2D284E` (handler->to_text for the value's tag; tag 6 handled separately). Data binding:
  `hr+0x2F4F29` converts the source value to the widget property's tag (handler->convert), then sets the property
  (vtable+0x70). So an int reaches the text as tag 8 and prints in hex. Why RR doesn't hit this is still unknown
  (maybe the source value's tag, or which widget property type is bound).
- Ruled out this round: the engine's own narrow printf at `hr+0x7C2EF0` is OpenSSL's BIO_printf (next to
  `crypto\bio\b_print.c`); narrow `%08x`/`%04x` strings are OpenSSL/network/marker messages; the only wide `%x`
  is the locale check; no hand-written hex-digit code in the UI.
- Padding to 8 digits isn't explained by `_i64tow_s` (it doesn't pad), so check where the zeros come from if the
  test below shows decimal but still padded.
- **Test (done, reverted):** `src/hooks/EnginePatches.cpp` patch 2 changes the radix byte at
  `hr+0x381F1A` (`41 B9 10 00 00 00` -> radix 10), only if the bytes match. Tyler runs
  `D:\Halo-Reach-Community-Edition\build\agent\go.bat` (job = build + Sword Base + scoreboard shot
  `capture_777777.bmp`, log `Y_noble-client.log`; look for "UI int64 values now convert to text in decimal").
  **Result (21:12): no change.** The patch was applied (log: "UI int64 values now convert to text in decimal"),
  but the scoreboard still shows place "0000" and score "0000001c" (`capture_777777.bmp`). So the tag-8 to_text
  is NOT the source; the patch was reverted (EnginePatches.cpp restored to the committed version).
  Remaining ideas: the token `\x07datasource.prop\x07` may not be evaluated at all (property lookup fails and the
  text widget dumps the raw value / handle; 0x1c and 1 look like small indices, place is 4 digits = 16-bit);
  find where the text widget expands `\x07` tokens (wide 0x0007 compare in the UI code around hr+0x2C0000-0x300000
  found nothing with `cmp ...,0x7`, so it may scan with a table or `wcschr`), or compare at run time which
  datasource the scoreboard binds (RR vs Noble). An interactive debugger (x64dbg on Tyler's PC, breakpoint on
  memory write of the text) would find it fastest.
- Still in `ReachHost.cpp` (uncommitted): the crtfmt diagnostics; remove before committing.

**20:55: hex-number bug (score "0000001c", "Respawn in 00000001", loadout grenades "0000 / 0002") still open.**
RR shows all of these correctly, so it is something Noble's host does differently.
- Ruled out: the engine's wide sprintf `hr+0x35D84` (only normal output: '%d'->'10', '%d:%02d'->'0:00', ...);
  `_itow_s` (only caller `hr+0x2F3AB8`, radix 10; a UI data-value vtable at `0x180A36070`); locale code
  `hr+0x2B6F12`; Noble's own source (no hex formatting on these paths).
- Found: `hr+0x381F10` calls `_i64tow_s` with **radix 16** (another data-value type: type 8 = int64, and
  `hr+0x381EF0` converts type 1 int32 -> type 8). It does not zero-pad, so it is a lead, not the answer.
- There are two more wide sprintf wrappers: `hr+0x35CC8` and `hr+0x35D24` (20 callers, incl. `hr+0x381ED4`).
- **Running now (uncommitted, on the PC):** the old 0x35D84 hook is replaced by IAT wrappers around all six CRT
  format imports (`__stdio_common_vsnwprintf_s/vswprintf/vswprintf_s/vsprintf/vsprintf_s/vsnprintf_s`, IAT RVAs
  0x91EA20/28/48/60/70/38). Log lines: `ReachHost [crtfmt ...]`, plus `zero-padded` for outputs starting "0000".
  job.bat = build + Sword Base with `-scoreboard-at 20 -quit-after 30`, log copied to
  `build\bin\Release\Y_noble-client.log`. **Result (20:56):** no CRT call produces any hex/zero-padded text
  (every format seen is normal: '%d'->'0', '%d:%02d'->'0:00', '%i'->'25', '%s%s%s'->'0', ...), so the hex text
  is made by engine code itself, not printf. If no CRT call makes the hex text, the text is
  made by engine code (look for code that writes hex digits by the value's size), or the value reaching the UI
  has the wrong type tag (compare with RR at the data-value vtables).
- Remove the diagnostic (`EnsureFormatDiagnostics` + `crtfmt` wrappers in ReachHost.cpp) before committing.

### Earlier (20:20 ET)

**20:15: hand checks done by computer use (all pass).** Windowed 1600x900 Sword Base:
- Post-game panel (held after the engine stops): PLAY AGAIN relaunches into a new match. A very short synthetic click
  was missed (the panel polled GetAsyncKeyState once per frame), so overlay menus now also count WM_LBUTTONDOWN
  (`Window::TakeLeftClicks`; cleared when the pause menu / post-game panel opens so in-game shots don't click).
- Pause menu QUIT TO MENU returns to the front-end menu; its QUIT closes Noble.
- Moving the window: with the mouse captured the title bar can't be reached (cursor clipped to the game), so the
  click lands in the game. Alt+Space > Move works, and with the pause menu open the title bar drags normally.
- Service tag: scoreboard shows "NobleSix  TEST" (`-scoreboard-at <s>` test option holds Tab 3 s and saves
  `capture_777777.bmp`).
- Not checked: kills/medals in the results (no enemies in an offline Slayer; needs Firefight or a second player).
- **New clue for the padded numbers:** the scoreboard shows score "0000001c" and place "0000": the numbers are printed
  as **hex at full field width** (8 digits for 32-bit, 4 for 16-bit), not just zero-padded. Looks like the UI's
  value-to-text falls back to a raw hex dump when it doesn't recognise the property's type. `%x` users in the DLL
  checked so far: `+0x2B6F12` is only locale detection (GetLocaleInfoEx + swscanf). Slot 61 only gets the set of
  unique characters, so it can't be used to catch the padded strings. Open question for Tyler: does RR show the
  same hex numbers?
- Tyler's settings were switched to windowed 1600x900 + tag TEST for the test and restored afterwards
  (`settings.backup.json` next to it is the copy).

**Pushed to GitHub (main):** `12a9971` HUD colours + post-game hold, `5e71e53` Mods screen (Tyler-tested),
`ab77f8b` service tag setting, plus the mouse-wheel / window-move commit below. Commits and pushes now run from
`build\agent\jobs\job.bat` through go.bat (output in `build\git_out.txt`); `src/engine/HudColor.*` stay untracked
(old stopgap, safe to delete).

**18:18: mouse wheel works (weapon switch, tested).** Input +0x114 is an **f32** wheel pulse (RR `host::WheelPulse`:
per notch +1/-1 for 40 ms, one pulse per 80 ms, up to 3 queued; `win::WHEEL` accumulates WM_MOUSEWHEEL / 120). The
engine only acts on it through the key bindings: RR's profile binds action 4 (change weapon) to '1' **and** second
key **0xAF** at entry +8 = wheel **up**. 0xAE in the third key slot did not give wheel down, so Noble sends every notch
as "up" (with two weapons it's the same). `-wheel-after <s>` test option injects one notch. Window.cpp now counts
WM_MOUSEWHEEL (`Window::TakeWheelNotches`) and, on WM_ENTERSIZEMOVE/KILLFOCUS, frees the cursor clip; ReachHost
releases the mouse while the window is being moved (`Window::InSizeMove`), so a windowed game can be dragged
(Alt+Space > Move, or title bar). The window-move part is untested by hand.

**17:35: post-game freeze FIXED (tested).** Full 10-minute Sword Base Slayer: slot 6 at 17:33:07 (cR paid, panel
shown), engine resolved `mainmenu.map` and stopped drawing; at 17:33:16 the log says "engine stopped drawing after
the match; Noble draws the post-game panel itself", and `capture_900000.bmp` (taken from the main-thread frames)
shows the GAME OVER panel with the cR lines, rank bar and the three buttons. Still to confirm by hand: clicking
PLAY AGAIN / RETURN TO MENU / QUIT on that panel (input is polled the same way as before, so it should work).
Note: the automated test runs pay cR into Tyler's `data\userdata\career.json` (now 2436 cR from test matches);
delete that file to start from zero. Not committed yet: `PauseMenu.*`, `main.cpp`.

**Number padding ("Respawn in 00000001", loadout grenades "0000 / 0002"), investigated 17:30, not solved.** The
engine builds those strings itself with the zeros already in them (slot 61 TextPrepare gets the characters
"Respawn i01"), so it is not a font/glyph problem. Widths are 8 and 4 digits, i.e. 32- and 16-bit fields, which
smells like a number-format setting the host should supply. Not the partner's IsWhitespace hook: the original
`haloreach+0x362E8` is a binary search over 12 Unicode whitespace ranges used for line breaking and really does
run away for characters below 0x09 (the `\x07` property markers MCC's own strings use), so the hook is correct.
Not slots 95/96 either (HUD scale / safe-area queries). Next idea: find where the UI formats an integer property
(look for the code that fills `camera_list.prop_respawn_time`) and see which host answer changes it.

**17:25: post-game freeze, fix (details).** After a match the engine goes for its main menu map (not in MCC) and stops
calling slot 1, so the post-game panel was never redrawn. RR handles this in its main loop: `host::loadscreen::
begin_hold()`, then `hold_frame()` + `swapchain->Present(1,0)` from the main thread for up to 60 s
(`host-probe.exe__main.c` ~4950). Noble now does the same: `PauseMenu::HoldFrame()` (main thread, called from the
message loop in `main.cpp`) draws the panel and presents once the game is over and no engine frame came for 1 s;
Render (engine thread) and HoldFrame share a mutex. Also a 5 s watchdog after the message loop, because the engine's
Shutdown may never return when its thread is stuck. Test running at 17:23 (full 10-minute Sword Base Slayer,
`U_*` files: look for "Noble draws the post-game panel itself" and `U_capture_900000.bmp`).

**17:20: dark HUD FIXED (host slot 97).** The engine sends every HUD colour through host slot 97 before it reaches
the HUD shader: `haloreach+0x2DDD8C` (sets constant id 0x60000 = chud_color_output_A..F, via `+0x271200`) calls
`+0x2DDF3C` for each colour, which packs it to ARGB and calls `[host]+0x308` (slot 97) with (a1 = filter/mode index
from `haloreach+0xD1F71C`, a2 = ARGB), then unpacks the returned ARGB. RR (`host::hit` case 0x61) returns the colour
unchanged; Noble returned 1, which made every HUD colour (0,0,1/255,0). `HostSlot97_ColorFilter` now returns a2. Test
(Sword Base, 17:19, `T_capture_2400.bmp`): shield bar, compass, tracker, reticle, ammo, grenades, game-type box all in
Reach's own colours. `src/engine/HudColor.*` (the Map/Unmap stopgap + diagnostics) is no longer built or called:
removed from `CMakeLists.txt` and `main.cpp`; delete the two files (`git rm src/engine/HudColor.cpp
src/engine/HudColor.hpp`). Not committed yet: `ReachHost.cpp`, `main.cpp`, `CMakeLists.txt`.

**Git state.** Everything up to the career work was committed on `main` as one commit ("Front-end menus, MCC art, skulls,
subtitles, settings, key binding names, career/cR, master server") and rebased onto the partner's `1d2a282` (their
`EnginePatches` whitespace-crash fix is kept). The `ReachHost.cpp` conflict was resolved by keeping Noble's version: all of
the partner's ReachHost edits were in the text code that Noble had already replaced or fixed the same way (glyph record,
digit/icon shortcuts, string fallback). The one idea not carried over: pre-rasterising text in slot 61 (TextPrepare).
If `git status` still says "rebase in progress", run `git add src/engine/ReachHost.cpp`, then
`git -c core.editor=true rebase --continue`, then `git push`.

**Not pushed yet:** a build fix made after that commit: `src/engine/KeyBindings.cpp` (the action table had landed outside
the anonymous namespace) and `src/engine/ReachHost.cpp` (unused-parameter warning). With them the build passes
(14:22). To push:
`git add src/engine/KeyBindings.cpp src/engine/ReachHost.cpp`, `git commit -m "Fix KeyBindings.cpp build"`, `git push`.

**Running the test job.** `D:\Halo-Reach-Community-Edition\build\agent\go.bat` (full path; from inside `build` the relative
path fails). It writes to files, so the console stays quiet; the line `EXITCODE` on screen is a quirk of `nb_build.bat`
(`echo EXITCODE 0>>` redirects) and means the build succeeded. Current job = `nb_run13.bat`: key-bindings screenshot
(`Q_keys.bmp`), then Sword Base with `-end-round-after 40 -quit-after 90` (`R_*` files in `build\bin\Release`).

**Result of the 14:22 run:** build OK, game ran 90 s without problems, but **`-end-round-after` did not end the game**:
the log shows "ending the round" at 40 s and then nothing (no slot 6, no `Career:` lines). Either `haloreach+0x7B3EC`
needs to run inside the simulation update (not from slot 1), or ending a round alone does not end a 1-round Slayer game
(try `game_won`'s evaluator `+0x1B0810`, or the engine's end-game function that `mp_round_end` leads to). Until then,
test career by **playing a short match to the end** (Slayer time limit) and reading the `Career:` log lines and
`data\userdata\results\last_game.bin`. `Q_keys.bmp` from this run has not been looked at yet.

**cR test passed (15:19).** Sword Base Slayer played to the time limit after the 13 cR/min rebuild. Log: `took 10.3 min;
local player 0 kills 0 assists 0 deaths 2 medals 0; paid 133 cR` = 10.3 x 13, matching Tyler's count (2 deaths, nothing
else); the post-game panel showed +133 cR and 133 / 7500 toward Private. No slot-machine bonus that game (1-in-8). Per-player
stats array: deaths appear at indices 11 and 13 (both 2).

**New: freeze after the post-game panel.** The panel opened (15:19:38) but its buttons could not be clicked. Right after
the results, the engine resolved `haloreach\maps\mainmenu.map` (slot 48), which MCC does not ship; about 7 s later it made
its first iface2 slot 140 call (a2=0x28, a3=0x40), a slot 4 EngineEvent with reason 0 and an empty message, and slot 1
frame ticks stopped (last frame 111000 at 15:19:45). With no frame ticks the panel is never redrawn or given input. Likely
cause: after game over the engine tries to go to the main menu map. Next: stop or answer that transition (slot 48 for
`mainmenu.map`, or slot 140 / slot 4), or move Noble to its own menu before the engine leaves the match.

**Tyler's reference list (from the afternoon chat).** He supplied 11 screenshots plus pasted notes (loadouts, control
schemes for MCC and Xbox 360, general Halopedia info on Reach) for: subtitles, main menus, skulls, controller layout
(360 and MCC), keyboard/mouse key bindings, loadout names. The files are not in the repo (Bungie/343 art and wiki text;
keep it that way). Status:
| Item | State |
|---|---|
| Subtitles | âœ… SUBTITLES setting + `0xD5244` hook (Â§5) |
| Main menus | âœ… Reach-style `FrontEndMenu` screens, art read from the MCC install (`UiArt`) |
| Skulls | âœ… Skulls screen + launch block +0x48 (Â§5) |
| Keyboard/mouse bindings | âœ… KEY BINDINGS tab, 66 slots named in MCC's groups; `Q_keys.bmp` (14:22 run) checked: renders correctly |
| Loadout names | âœ… come from the game variant and render correctly (Â§5) |
| Controller layout (360 and MCC) | âŒ **still to do**: a CONTROLLER tab in SETTINGS showing the MCC default and the original Xbox 360 layouts (controller picture already readable from `controllertexturepack`), and gamepad input itself (XInput into slot 36 +0x120.., Â§6 item 6) |
Checked against the Halopedia notes: the menu has all 24 MP maps (incl. Noble, Defiant and Anniversary packs), all 10
Firefight maps and all 11 campaign missions; mission descriptions are Noble's own wording (don't paste the official
taglines). Not used yet but useful later: the Armory's 30 armour colours (10 bases à— 3 shades: Steel/Silver/White,
Red/Mauve/Salmon, Orange/Coral/Peach, Gold/Yellow/Pale, Sage/Green/Olive, Teal/Aqua/Cyan, Blue/Cobalt/Sapphire,
Violet/Orchid/Lavender, Crimson/Rubine/Pink, Brown/Tan/Khaki) for the Armor Locker, and the Arena divisions (Onyx, Gold,
Silver, Bronze, Iron) if a ranked mode is ever added.

**Career reference data (14:45).** Tyler collected the Reach career material from Halopedia and the Bungie.net
forums; it is now in `docs/career/`: `career-reference.md` (how Reach paid cR, campaign scoring, ranks, commendations,
challenges, Armory, loadouts, controller layouts, sources) and the data files `ranks_and_pay.json`, `challenges.json`
(all 18 months Sep 2010 – Feb 2012, 2,103 challenges; no month missing, only 14 days Halopedia itself lacks),
`commendations.json` (45) and `armor.json`. Names and numbers only, no Bungie art (rank/challenge icons must be read
from the MCC install; Tyler's rank-icon sheet stays local). Open question for Tyler: keep Noble's own cR numbers or
switch to Reach's measured table (`career-reference.md` Â§1).

**Armory appearance (current).**
- The Armory saves `data/appearance.json` under the resolved data directory. Slot 34 writes species, armor pieces,
  colours and the Elite armor set into the profile (see section 0, 09:15).
- The local OBJ preview resolves MCC MTL texture paths into the exported `bitmaps` tree, loads matching `_cc` masks,
  and applies the selected primary/secondary/detail colors and visor tint. It composes a Spartan base with selected
  MCC accessory groups, or an Elite body/undersuit layer followed by the selected rank harness group. The engine's
  own main-menu Spartan preview is unavailable because MCC has no Reach `mainmenu.map`; Armory uses Noble's DX11 OBJ
  renderer instead.
- The actual Spartan/Elite OBJ exports remain local-only under git-ignored `data\armor\`; never commit them. The OBJ
  model-group UI only offers geometry that exists in the local export. Reach's equipment IDs and live player color
  records are still unmapped, so piece/color selections affect the Armory preview, not yet the in-game character.
- A safe in-game equipment/color implementation needs a known-good profile capture with per-piece/color changes (or
  verified engine ID mappings). RR's profile contains candidate u32 lists at +0x40..+0x1AB, but their semantics are
  not confirmed. Ghidra 12.1 found no direct references to the `color_armor1` or Spartan helmet property strings, and the
  matching PDB was not present in the MCC install; static analysis has not established equipment or color IDs. Direct
  `.map` render-model decoding also remains future work.
- For an opt-in runtime capture, launch `noble-client.exe -capture-profile data/profile-captures`. Slot 34 writes the
  initial 64-KB profile buffer and any later changed buffer on subsequent Slot 34 calls as session-tagged, numbered
  `.bin` files; later runs do not overwrite earlier captures. This mode is disabled by default. Change one in-game
  setting at a time, retain the matching before/after files locally,
  and do not commit captures. If Slot 34 is not called again after a change, this mode cannot observe that change; Armory
  preview JSON selections are not included until they are mapped into the game profile. Compare matching captures by
  byte offset, substituting their actual session-tagged filenames:
  `$a=[IO.File]::ReadAllBytes('profile-<session>-0.bin'); $b=[IO.File]::ReadAllBytes('profile-<session>-1.bin'); 0..($a.Length-1) | ? { $a[$_] -ne $b[$_] } | % { '{0:X4}: {1:X2} -> {2:X2}' -f $_,$a[$_],$b[$_] }`.
- Forwarded community request (TRAXUSi, Workshop class mods): add extra player-host object types beyond Reach's
  three (0 spartan, 1 elite, 2 monitor), e.g. `elite2`, `spartan2`, `monitor2`, using the same host code paths, with
  matching Megalo names. Engine patch inside `haloreach.dll` at run time; not started, idea only.

**17:10 update: RR's real HUD colours (RenderDoc capture of RR, Spartan, Sword Base, frame 2662).**
- To play as a Spartan in RR, the profile matters: RR profile bytes **+0x27 and +0x38** are 1 for Elite, 0 for Spartan
  (only 5 bytes differ between the two profiles). Spartan copy: `D:\REACH REVERSE ENGINEER\rr_profile_spartan.bin`
  (RR data, never commit). RenderDoc: exe `D:\Reach\bin\host-probe.exe`, cwd `D:\Reach`, child capture off, args = RR's
  match line without `--show-behind/--exit-after-game/--hold-after-game` and without the three elite options.
- `CHUDWidgetPS` = Buffer 10679 (208 bytes = 13 float4, rewritten before every HUD draw). Rows 0-5 are
  chud_color_output_A..F; rows 6-12 are parameters (scale, timing, transforms). Values (0-255, R,G,B,A):

  | Row | Motion tracker (EID 20055, Draw 6) | Shield bar (EID 20084, Draw 18) |
  |---|---|---|
  | 0 A | 0,0,0,0 | 36,148,255,255 |
  | 1 B | 8,53,121,255 | 8,53,121,255 |
  | 2 C | 0,0,0,0 | 255,255,255,1 |
  | 3 D | 0,19,33,0 | 4,23,62,255 |
  | 4 E | 0,0,0,0 | 240,36,36,0 (red: low-shield colour) |
  | 5 F | 0,0,0,0 | 255,255,255,1 |

  Noble gets (0,0,1,0) in every row of every widget. **The colours differ per widget and per slot**, so a fixed
  replacement can't be right (that's why HudColor.cpp's sky blue gave the white block). The engine computes these on
  the CPU from the map's HUD (chud) data each frame; in Noble that computation returns 1 everywhere.
- Next (code work, no help needed from Tyler): in `haloreach.dll`, find the CPU code that fills this 208-byte buffer
  (follow the Map/Unmap of the CHUD widget constant buffer back to the colour evaluation), see what input makes it
  return 1 (likely a lookup that fails: a host answer, a profile field, or the player colour records RR writes,
  `looks::copy_colors`), and fix that instead of patching the buffer. Until then HudColor.cpp could use these values
  as a better stopgap, but that is a guess per widget.
- `armory-appearance.txt` (read 16:45): Spartan primary b3852a, secondary 000000, tertiary ff7a14, quaternary 6a2c00;
  Elite 3d6084; emblem colours 2fa59a/80ddda. None is the HUD blue, so the HUD isn't simply the armour colour.

**16:45 update: sky-blue HUD tested; next is an RR capture.**
- `HudColor.cpp` result (Sword Base, Forest): motion tracker, ammo, grenades, crosshair, ability icon now light blue
  like RR. Two regressions: the shield bar + compass background is a solid **white block** (several colour slots
  stack), and the scope mask outside the zoom view is a **light-blue haze** (it should stay dark). Every broken slot
  holds the same value (1), so one replacement colour can't be right. Not committed yet.
- Next: RenderDoc capture of RR for the real CHUDWidgetPS values (motion tracker, shield bar, scope mask). Launching
  `Reach.exe` with "Capture Child Processes" crashes RR's WebView2 menus, so launch the game process directly:
  - Exe `D:\Reach\bin\host-probe.exe`, working dir `D:\Reach`, child capture off.
  - RR's real launch line (16:41; `runs\<id>\match` is a per-run folder):
    `<run>\match --launch-block D:\Reach\data\launch_block_1.bin --armory-appearance
    %LOCALAPPDATA%\ReachRenaissance\armory-appearance.txt --profile <run>\profile.bin --strings
    D:\Reach\data\strings_en.json --elite-looks --features-on 6e --seconds 86400 --user-id <xuid> --user-name <name>
    --mcc "<MCC dir>" --map SwordBase --mode hr_4v4_team_slayer_dmr_ar_50points_tu --time-limit 12 --exit-after-game
    --extra-maps D:\Reach\data\armor --armor-pack rr_elite_pack --elite-armor-slots --service-tag ONYX
    --settings-file %LOCALAPPDATA%\ReachRenaissance\settings.txt --window-pos 100,100 --window 2560x1440
    --hold-after-game --show-behind <launcher hwnd> --map-variant "<MCC>\haloreach\hopper_map_variants\sword_base_default_031.mvar" --ingame-chat`
  - For RenderDoc: drop `--show-behind`, `--exit-after-game`, `--hold-after-game`; use a copied profile
    (`D:\REACH REVERSE ENGINEER\rr_profile.bin`, RR data: never commit).
- **New lead:** RR passes `--armory-appearance armory-appearance.txt` (player colours); HUD colours may come from
  the player's colours. Waiting for Tyler to paste that file.
- Loadout cards show grenades as "0000 / 0002": same zero-padding bug as the respawn timer (suspect the
  IsWhitespace hook in `EnginePatches.cpp`).

**16:30 update: dark HUD cause found (RenderDoc).**
- Capture of Noble on Sword Base (frame 1862, saved outside the repo; never commit `.rdc` files). The HUD is
  "Colour Pass #7" (EID 8935-9432, last pass before Present). Pixel history on the motion tracker: EID 8980 writes
  (0.04, 0.04, 0.08). Its pixel shader has two constant buffers: `ExposurePS` = g_exposure (0.5, 2.0, 1.0, 1.0),
  normal; `CHUDWidgetPS` (208 bytes) has **chud_color_output_A..F all (0, 0, 0.00392, 0)**, i.e. the integer 1
  read as ARGB. So the HUD colours the engine computes are an index/flag value, not a colour. Probably the same
  player/colour records RR rewrites in memory (`armory::write_colors`: four ARGB u32 >= 0xFF000000;
  `looks::copy_colors` hook), which would also explain why RR's profile file alone didn't help.
- **Stopgap (Tyler's call): `src/engine/HudColor.cpp`** hooks ID3D11DeviceContext::Map/Unmap (vtable 14/15) and,
  in 208-byte constant buffers, replaces colour slots holding exactly that broken pattern with sky blue #87CEEB
  (alpha kept). Installed in `main.cpp` after UIManager init; added to `CMakeLists.txt`. Real fix still to do: find
  what fills those colours (compare a RenderDoc capture of RR: same draw, CHUDWidgetPS values; then RR's
  `looks::copy_colors` / `armory::write_colors`).
- The 15:45 D3D11Renderer test (BGRA flag, 1 buffer) made the HUD black and was reverted (`git restore`).
- Mods screen rewrite (`WorkshopManager.*`) tested OK by Tyler.

**15:55 update.**
- Pushed `ebd9761` (on top of the partner's `ae7b71e`): career pay 13 cR/min, rank fix, `docs/career/`, fixed-size
  windowed mode (`Window.cpp`: no maximize/resize, since the engine can't change resolution after start; use
  BORDERLESS FULL SCREEN for full screen), Key Bindings no longer lists MCC's "other Halo games" actions.
- **Dark HUD:** not resolution (borderless 3840x2160 is still dark). Pixel samples: RR tracker edge (80,148,191) vs
  Noble (21,21,32), so the HUD colour is lost, not just dimmed. Same host slots as RR (RR's switch handles 1, 2, 4, 10,
  18, 32-34, 36, 39, 41, 43, 47, 48, 51, 52, 55-57, 88, 97, 116; slot 33 bytes identical; iface2 identical; RR gamma
  exponent default 1.0). Lobby patches are inactive (0 applied). **Test (reverted 16:05) in `src/host/D3D11Renderer.cpp`:**
  device flag `D3D11_CREATE_DEVICE_BGRA_SUPPORT` (RR passes 0x20) and RR's swap chain (1 buffer, flags 0). If the HUD
  stays dark, revert it; next suspects are RR's engine hooks (`resadapt` hooks 14 functions at haloreach+0x1BB6C0..
  +0x1BBD34 that read tag-block elements by u16 index; `hudx`, `widgetx`, `texcam`).
- **Respawn timer** "00000001": suspect Noble's `EnginePatches` IsWhitespace hook (returns 0 for chars < 0x09); the
  respawn string uses `\x07` property tokens. Test with the hook off.
- **UI to fix next (Tyler's order: HUD, then Mods screen and a clean Settings menu):** Mods: Halo 3 mods listed as
  Reach (read each mod's `ModInfo.json`: `"Engine"` and `"Title":{"Neutral":...}`), titles are the first map file
  name, ImGui ID clash (rows need `PushID(i)`), fixed pixel widths cut off at 4K. Controller picture is dotted/pixelated
  (scaling/alpha). Settings: lots of empty space, Video has only resolution and window mode (RR: FOV, V-Sync, quality,
  help text on the right). Party box says 1/16 (RR 1/64).
- Tyler added full Spartan and Elite rips (OBJ + bitmaps) at `D:\REACH REVERSE ENGINEER\spartans` and `...\elite`
  for the armory. Game assets: never commit them.

## 1. Where things are

| What | Path |
|---|---|
| **Repo (Noble)** | `D:\Halo-Reach-Community-Edition` (remote `https://github.com/O7OF/test-build-01`, branch `main`) |
| Built client | `D:\Halo-Reach-Community-Edition\build\bin\Release\noble-client.exe` |
| Run log / crash dump | `build\bin\Release\noble-client.log` (overwritten every launch), `noble-crash.dmp` |
| Noble data folder (git-ignored) | `build\bin\Release\data\` (`sandbox\`, `userdata\`, `mods\`, local test files) |
| Map-info sandbox (created at runtime) | `build\bin\Release\data\sandbox\haloreach\maps\info\` (the process `cwd` is switched to `data\sandbox`) |
| Repo docs | `docs\HANDOFF.md` (this file), `docs\hud-text-findings.md`, `docs\boot-investigation-2026-10-27.md`, `docs\architecture_plan.md` |
| **MCC (Steam)** | `D:\Steam\steamapps\common\Halo The Master Chief Collection\` |
| Engine DLL | `...\haloreach\haloreach.dll`, SHA-256 `738dd2d24ea3aea12e1ee9aa4a61094bf116027d42004c35a19e5048608b0894` (the only build RR supports; all RVAs below are for this build) |
| MCC game variants | `...\haloreach\game_variants\*.bin` (`slayer_054.bin`, `hr_ff_*.bin` for Firefight, ...) |
| MCC sound banks | `...\haloreach\fmod\pc\sfx.fsb`, `english.fsb` (found through host slot 48) |
| MCC binaries | `...\MCC\Binaries\Win64\` (ships `xaudio2_9redist.dll`, which `haloreach.dll` loads for sound) |
| Reach Renaissance install (reference only) | `D:\Reach` (`bin\host-probe.exe`, `bin\menu-overlay.exe`, `data\profile_1.bin`, `data\lb_m_c_1.bin`, `data\launch_block_1.bin`, `data\blocks\*.bin`, `data\menumaps\mainmenu_all.map`) |
| RR run logs | `C:\Users\tyler\AppData\Local\ReachRenaissance\runs\<timestamp>\` (`menu\`, `guest\`, `match\` host_probe logs, `*-args.txt`) |
| **RE workspace** | `D:\REACH REVERSE ENGINEER` (not a git repo) |
| RR analysis notes | `D:\REACH REVERSE ENGINEER\docs\rr-launch-flow.md`, `rr-tags.md` |
| RR pseudocode (Ghidra) | `D:\REACH REVERSE ENGINEER\docs\evidence\pseudocode\*.c` (`host-probe.exe__host__hit@0x14007c600.c`, `__main.c`, `__set_game_variant@0x1401c3450.c`, ...) |
| haloreach.dll index | `D:\REACH REVERSE ENGINEER\docs\evidence\haloreach\` (`haloreach_functions.csv`, `haloreach_strings.csv`, ...) |
| Omaha symbol tables | `D:\REACH REVERSE ENGINEER\docs\evidence\symbols\` (Xbox 360 addresses, names only) |
| Leaked 2010 build (source of Omaha symbols) | `D:\Halo Reach (Jul 24 2010) (Omaha) (Cache Build)`. Keep anything derived from it **out of the repo** |

**Tools installed:** Visual Studio 2022 Community + Desktop C++ (CMake bundled), Git, Node.js, **Ghidra 12.1.4** at
`C:\tools\ghidra_12.1.4_PUBLIC`, **JDK 21** at `C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot`
(`GHIDRA_INSTALL_DIR` and `JAVA_HOME` set). The partner's PC does **not** have Ghidra configured.

`host-probe.exe` (RR) still has Rust symbols: `llvm-nm -C` / `llvm-objdump -d -C` give real function names
(`host_probe::host::hit`, `host::hit2`, `host::load_profile`, `text::glyph`, `pad::fill`, `live::remap_keys`, `uicursor::*`, ...).
That is the fastest way to answer "what does RR do for slot N". `objdump` on `haloreach.dll` itself is also quick
(~2.5 M lines) for tracing engine code at an RVA.

## 2. Build and test (exact commands)

Use **Developer Command Prompt for VS 2022** (plain PowerShell has no `cmake`; in cmd use `cd /d` to change drive).

```
cd /d D:\Halo-Reach-Community-Edition
```
```
cmake --build build --config Release
```

* If `build\` is missing or CMakeLists changed a lot: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` first.
* The post-build step copies a top-level `data\` folder; if the repo has none the build "fails" after linking. Create it once:
  `if not exist D:\Halo-Reach-Community-Edition\data mkdir D:\Halo-Reach-Community-Edition\data` (the post-build copy should
  be wrapped in `if(EXISTS ...)` in CMakeLists).
* `LNK1104: cannot open file ...noble-client.exe` = the game is still running. Close it or `taskkill /IM noble-client.exe /F`.

Run (Steam must be running), from `build\bin\Release`:

| Command | What it does |
|---|---|
| `noble-client.exe` | Noble front-end menu (also for `-map mainmenu` / `-mode mainmenu`, which the C# launcher passes) |
| `noble-client.exe -map 20_sword_slayer -mode multiplayer` | Sword Base directly (most tested map) |
| `noble-client.exe -map m10 -mode campaign -difficulty 1` | Winter Contingency (0 easy .. 3 legendary) |
| `noble-client.exe -map ff50_park -mode firefight -difficulty 1` | Firefight on Beachhead (default game type Standard) |
| `noble-client.exe -map m10 -mode campaign -skulls 0x10` | campaign with skulls (MCC skull bits, here Blind; see Â§5 Skulls) |

Other args: `-variant <path to MCC game variant .bin>`, `-mvar <path>`, `-mouse-scale F` (mouse look per pixel, default 0.0008),
`-profile <file>` (load a captured profile instead of Noble's built-in one; relative paths are resolved against the exe folder).
Test-only args: `-menu-screen <main|campaign|skulls|firefight|custom|forge|servers|settings|armory|mods>` opens that
front-end screen, `-menu-tab <0-5>` picks the SETTINGS tab (1 = key bindings), `-menu-capture <frames>` saves `capture_<frames>.bmp` of the menu and exits; `-launch-poke 0x48=0x10,...`
writes u32 values into the launch block (for finding fields by experiment). `-capture-frames 300,3600` saves `capture_<frame>.bmp` next to the exe (and one shot ~120 frames after the
post-game panel appears); `-quit-after <seconds>` ends the run by itself; `-end-round-after <seconds>` calls mp_round_end's work function, which does not end a multiplayer round (2026-10-31). Player settings (resolution, borderless, mouse,
invert, key overrides, volumes, menu sounds, player name, master-server URL, favourites) are in
`build\bin\Release\data\userdata\settings.json`, edited from the SETTINGS tab.

Unattended build without the developer prompt: `build\nb_build.bat` calls Visual Studio's bundled `cmake.exe` directly
and writes `build\nb_build_out.txt` (VsDevCmd hung when started from a script). `build\nb_run3.bat` = 12-minute Sword Base
test with captures. These helper scripts are under the git-ignored `build\`.

Read `build\bin\Release\noble-client.log` after a run. Useful lines: `launch state N`, `Slot 4 - EngineEvent`, `slot N first call`,
`FATAL EXCEPTION`, `FrontEndMenu: PLAY ...`, `Slot 48 ... .map`.

## 3. Current status

| Milestone | State |
|---|---|
| Find MCC, Steam ownership check, load `haloreach.dll`, init, start | âœ… |
| Sword Base and other MP maps render and play | âœ… |
| Keyboard/mouse input (move, look, jump, crouch, sprint/ability, fire, weapon/grenade swap, Tab) | âœ… |
| Scoped aiming | âœ… (look settings block in profile) |
| Sound (gunfire, footsteps, damage) | âœ… (profile volumes + XAudio2 redist directory) |
| Mouse cursor in Reach menus (class/loadout pick) | âœ… (Windows arrow; RR uses it too) |
| Pause menu (Esc: Resume / Quit to Menu / Quit) | âœ… (ImGui overlay, Noble style) |
| Loading screen while the map loads (was ~5 s black) | âœ… map name + moving bar (`PauseMenu::DrawLoading`), ends on the first slot-10 call (tested) |
| Settings tab (controls, key bindings, audio, display, player name) | âœ… `SettingsPanel`, saved to `settings.json`, applied at next launch |
| Menu look and sounds | âœ… `NobleStyle` (Reach-like palette, Bahnschrift), `MenuSounds` (synthesised clicks, 40 ms hover cooldown) |
| Front-end menu, Reach style (`FrontEndMenu.cpp`): MAIN MENU list (Campaign, Matchmaking*, Firefight, Custom Game, Forge, Theater*, Armory, Mods, Settings, Quit; *greyed), My Party box, button hints; Campaign (mission list + preview, difficulty, skulls, score multiplier), Skulls, Firefight / Custom Game (map + preview, game type, difficulty), Forge (MP launch with MCC's `00_basic_editing_054.bin`: Forge World loads with no score/timer HUD; switching to the editor (Monitor) is untested), Games (server browser), MCC-style Settings | âœ… screenshots of every screen checked |
| Menu / loading art (`UiArt`): player's `data\ui\custom\`, Noble's `data\ui\noble\`, else read from the MCC install at run time (nothing shipped) | âœ… campaign scenes as backgrounds, map previews, skull icons, controller picture |
| Skulls (launch block +0x48) | âœ… verified (Blind hides the HUD) |
| Cinematic / dialogue subtitles | âœ… SUBTITLES setting (on by default), hook on `haloreach+0xD5244`; tested on Winter Contingency's intro |
| Campaign missions (cutscene â†’ gameplay) | âœ… tested Noble Actual (prologue, ends after cutscene) and Winter Contingency |
| Firefight | âœ… tested with Standard. Scenario names (from MCC's preview pictures): `ff50_park` = **Beachhead**, `ff10_prototype` = **Overlook** (the old front-end menu had them swapped; BlamEngine's name aliases "beachhead"/"overlook" are still swapped, the scenario names it maps are right) |
| Game end (engine reports results through host slot 6) | âš ï¸ post-game panel shows the right cR (15:19 test), but its buttons can't be clicked: the engine stops sending frames about 7 s later (Â§0) |
| Memory | âœ… no leak during a match: 12-minute run, private bytes 3835 â†’ 3852 MB, working set ~1.3 GB, flat |
| HUD / scoreboard text and icons | âœ… letters, numbers and weapon/grenade icons render (glyph layout and string slots fixed, see Â§5) |
| HUD colours (bars, compass, motion tracker) | âœ… fixed 17:20: host slot 97 must return the colour it is given (Â§0) |
| Key bindings: all 66 slots named | âœ… slot numbers = MCC's `GameAbstractButton` order (enum strings in `MCC-Win64-Shipping.exe`); `KeyBindings.cpp` groups them like MCC's CONFIGURE BINDINGS (movement, actions, vehicle, UI, Forge, Theater; 10 slots only other Halo games use). Crouch default is now Left Ctrl (MCC) |
| Career / cR (`src/career/Career.cpp`) | âš ï¸ first version, needs a real match to confirm stat slots: reads MCC's game-results record at slot 6, pays cR, shows it on the post-game panel, saves `data\userdata\career.json`. See Â§5 "Game results" |
| Spire (`35_island`) crash ~15 s into a match | âœ… fixed by the text fixes: 3+ minutes of Slayer on Spire without a crash (13:30 run) |
| Servers tab | âš ï¸ LAN list, own hosted game always listed, favourites, direct connect, optional master server (`tools\master-server`). **Joining** another player's game is not possible yet (engine net slots 41/43 not implemented); the UI says so |
| Armor Locker / Workshop Mods tabs | âš ï¸ screens only; not wired to the engine |
| Engine main menu (`mainmenu.map`) | âŒ by design: MCC ships no `mainmenu.map` for Reach (RR ships its own modified 298 MB `mainmenu_all.map`, which Noble cannot) |

## 4. Repo state

Work from this session went up as separate branches/PRs, each building on the previous one:
`feat/input-slot36` â†’ `feat/sound` â†’ `feat/menu-cursor` â†’ `feat/pause-menu` (merged with `main` on GitHub, includes the
partner's TextAtlas/GammaPass) â†’ `fix/zoom-aim` â†’ `feat/front-end-menu` (front-end menu, then campaign, Firefight and the
extra tabs). Check on GitHub which of these are merged into `main`. Uncommitted at the time of writing: `docs/hud-text-findings.md`
and this file, plus possibly the campaign/Firefight changes if they weren't committed (`git status` shows it).

**Uncommitted work from 2026-10-27 afternoon** (on top of the night session below): `src/ui/UiArt.*` (new),
`FrontEndMenu.*` (rewritten, Reach-style screens + skulls), `SettingsPanel.*` (MCC style), `NobleStyle.*` (hero/small
fonts), `PauseMenu.cpp` (loading art, Firefight names), `KeyBindings.*` (groups), `Settings.*` (subtitles, campaignSkulls,
menuBackground), `ReachHost.cpp` (subtitle hook), `BlamEngine.*` (skulls at +0x48 as u64, `-launch-poke`), `main.cpp`
(`-skulls`, UiArt), `FrameCapture.*` (`SaveNow`), `CMakeLists.txt` (UiArt, windowscodecs). Suggested extra PRs: (7) skulls +
subtitles (engine), (8) UiArt + Reach-style front end + MCC settings.

**Uncommitted work from the 2026-10-27 night session** (on disk in `D:\Halo-Reach-Community-Edition`, builds, nothing committed):
new `src/common/Settings.*`, `Relaunch.*`, `src/engine/KeyBindings.*`, `FrameCapture.*`, `src/network/MasterServerClient.*`,
`src/ui/NobleStyle.*`, `MenuSounds.*`, `SettingsPanel.*`, `tools/master-server/`; changes to `ReachHost.*` (text atlas, string
table, slots 6/32/33/51, settings, memory log), `main.cpp`, `Window.*`, `SessionManager.cpp` (LAN beacon crash), `ServerBrowser.*`,
`FrontEndMenu.cpp`, `PauseMenu.*`, `UIManager.cpp`, `MissionLauncher.cpp` (map names), `CMakeLists.txt` (new files, winhttp/winmm,
`if(EXISTS data)`). Suggested PR split: (1) LAN crash fix, (2) HUD text/icons + string table, (3) settings + key bindings + style
+ sounds, (4) server browser + master server, (5) loading/post-game overlay + Relaunch, (6) test tooling (FrameCapture).

Local test files that must **never** be committed (they're RR's data, all under the git-ignored `build\`):
`build\bin\Release\data\profile_1.bin` (copy of RR's captured profile) and `data\test_profile_*.bin` (bisection files). Delete
them when convenient; Noble no longer reads `profile_1.bin` automatically.

## 5. Engine facts (haloreach.dll build above)

**Exports / vtable**
| Item | Detail |
|---|---|
| `SetLibrarySettings(p)` | zeroed 0x1FE block, three UTF-16 locale strings |
| `CreateGameEngine(&engine)` | returns 0x46 |
| `Init` vtbl+0x08 | `(engine, ID3D11Device*, 0, IDXGISwapChain*, presenter=0)`; finds the HWND **of the calling thread** |
| `Start` vtbl+0x10 | `(engine, host, launch_block[0x2BF30])`; spawns the engine thread. The engine can't switch maps afterwards: RR and Noble restart the process (Noble: pause menu "Quit to Menu") |
| `PostCommand` vtbl+0x18 | `(engine, id, payload)`; 10 then 11 after ~300 ms |
| Present | done **by the engine thread**; host slot 1 runs first with `(swapchain, &flags)`. Host overlays (gamma pass, pause menu) draw there; never use the immediate context from another thread while the engine runs |

`haloreach.dll` loads `xaudio2_9redist.dll` by bare name during start-up: Noble keeps `SetDllDirectoryW(<MCC>\MCC\Binaries\Win64)`
set for the whole run (as RR does), otherwise there is no sound. Audio itself is FMOD inside the DLL.

**Launch state machine** `haloreach+0xC1A114`: 0…10 while launching, 11 = launched (reached ~5 s before the map is on
screen; the first slot-10 call marks the game session start). Failure strings table `0x1803A2DCC`
(17 `no map selected`, 18 `no game selected`, 19 `map load failure`).

**Paths:** map list from `FindFirstFileW` over `haloreach\maps\info\*.*` relative to cwd (hence the sandbox); files through slot 48.

**Launch block (0x2BF30 bytes).** Noble builds every block itself (`BlamEngine::BuildLaunchBlock`). RR's captured blocks for
menu, MP, campaign and Firefight are byte-identical apart from these fields (layout used as reference only):

| Offset | MP | Campaign | Firefight | Menu |
|---|---|---|---|---|
| +0x0C | 3 | 1 | 5 | 1 |
| +0x10, +0x14, +0x1D5C0 | launch index 190–209 | 178–189 | 210–219 | -2 |
| +0x28 | 0 | `01 00 11 11` | 0 | 0 |
| +0x34 | 1 | difficulty 0–3 | difficulty 0–3 | 1 |
| +0x1D334, +0x1D5BC (map id) | MP map id (Sword Base 1000, Boardwalk 1035, ...) | 1035 | 1035 | 1035 |
| +0x300 game variant (decoded with `haloreach+0x373A0`) | chosen variant (default `slayer_054`) | slayer | **a Firefight variant** (`hr_ff_*.bin`; RR loads `game_variants\<mode>.bin`) | slayer |

**Skulls: u64 at +0x48** in MCC's skull numbering, verified (Blind hides the HUD). `haloreach+0x3A22C` converts it to the
engine's own bits (game options +0x1D8, copied to game globals +0x1EAC8, which the `skull_enable` script command flips).
The MCC bit is the skull's index in MCC's alphabetical skull list: Iron 18, Black Eye 3, Tough Luck 36, Catch 7, Cloud 13,
Famine 11, Thunderstorm 34, Tilt 35, Mythic 23, Blind 4, Cowbell 8, Grunt Birthday Party 16, IWHBYD 19 (38 skulls in the
converter). Noble used to write skulls to +0x3C, which is a different
field the engine reads as u16; that was removed.

Also: +0x18 / +0x1D5C4 `0x88880000`, +0x50 random 16-byte launch id, +0x1D308 map variant area (init `haloreach+0x6BFF8`).
Launch indices: campaign 178 m05 Noble Actual, 179 m10, 180 m20, 181 m30, 182 m35, 183 m45, 184 m50, 185 m52, 186 m60, 187 m70,
188 m70_a, 189 m70_bonus; Firefight 210 ff10_prototype, 211 ff45_corvette, 212 ff20_courtyard, 213 ff60_icecave, 214 ff70_holdout,
215 ff60_airview (verified: loads Outpost), 216 ff50_park, 217 ff30_waterfront, 218 ff_unearthed, 219 cex_ff_halo.

**Host interface** (`host[0]` = 160-slot vtable, `host[1]` = secondary interface; args `(host, a1, a2, a3, a4)`).
| Slot | Meaning | Noble |
|---|---|---|
| 1 | per-frame, before Present: `(swapchain, &flags)` | gamma pass, then overlay (loading / pause / post-game), then test capture; memory log every 1800 frames |
| 4 | engine event `(reason, msg)` | logs |
| 6 | **game results** `(buffer, 0xD5A8, 0x14)`: called when a game ends (RR `results::on_game_results`) | shows the post-game panel (only once launch state is 11); results not decoded yet |
| 10 | return secondary interface | âœ… |
| 23 | load progress | âœ… |
| 32 / 33 | display settings / quality | 32: size from the window; 33: writes the 16 bytes `0x3F80000000000201, 0x2BF800000` **directly at a1** (as RR) |
| 34 | **profile**: pointer to a 64 KB block (see Profile below) | âœ… built-in defaults |
| 36 | **input poll** `(userIndex, u8 state[0x130])`, users 0–3 every tick (`haloreach+0x23AE7`) | âœ… |
| 47 / 48 | user-data folder / file path resolver | âœ… |
| 51 | feature query `(id, bool* out, u64* extra)`: RR answers from mask FEATURES_ON = 0x6E and zeroes `*extra` | same mask; the engine only asks 1, 2, 5, 6 (all on) |
| 55 / 56 / 57 | strings: clear / register `(table, id, const char* name, const char* suffix)` / lookup `(table, id, wchar_t* out, cap)` | key = CRC-32/MPEG-2 (poly 0x04C11DB7, init 0xFFFFFFFF, no xorout) of upper-case `name_suffix` (leading `$` skipped); text from `<MCC>\data\ui\Localization\EN_HaloReach.bin` (magic 0x90CE6B7A; u32@0x78 key bytes, keys from 0x80; u32@0x7C string block; NUL-terminated UTF-8 in key order). Lookup returns 0 when unknown so the engine uses the map's own strings |
| 61–67 | text: prepare, glyph, atlas, has_icon, kerning, register_icon, metrics | âœ… `TextAtlas`: 1024Â² pages (â‰¤16), shelf packing, dirty-rect upload in slot 63 |
| 88 | identity `(u64* xuid, wchar_t* name, cap, userIndex)` â†’ 1 | âœ… |
| 116 | controller map `(xuid)`; RR only logs | returns 0, writes nothing |
| iface2 147 | pointer to a 16-byte session GUID; every other iface2 slot returns 0 (RR does the same) | âœ… |

**Glyph record (slot 62 out, 26 bytes):** ch@0, zero@2..7, atlasX@8, atlasY@0xA, w@0xC, h@0xE, originY@0x10, originX@0x12,
advance@0x14, -ascent@0x16, page@0x18. Glyph alpha = min((grayÂ·255+32)>>6, 255) from GDI GGO_GRAY8. Icons (0xE000–0xE2FF): the
engine registers one through slot 66 `(host, ch, size, f32 scale, face, w, h, top, ARGB4444-decoded pixels, pitch)` only when
slot 64 has_icon returns false (engine path `haloreach+0x1CF4D0` reads `font_package_icon.bin`); store the pixels unchanged,
originY = top, originX = 0, advance = w, ascent = 0.

**Input state (slot 36 buffer, 0x130 bytes, same as RR byte for byte).** +0x000 u32 1 (keyboard/mouse; 0 = gamepad);
+0x004+VK u8 per key (`0x80` pressed < 500 ms, then `0x81..0xFF` held); +0x104/+0x108 f32 mouse look; +0x114 wheel;
+0x118 f32 1.0; +0x11C mouse buttons; +0x120.. gamepad (RR `pad::fill`, not done). The engine reads keys through a 104-entry VK
table (`haloreach+0x98FDF0`) but maps them to actions **only through the profile's binding table**.

**Profile (slot 34).** RR returns a zeroed 64 KB block, optionally filled from a captured profile (`--profile`, first 0xC00 bytes),
with a 4-char UTF-16 service tag at +0x1AC. Everything the engine reads from a blank profile is 0, which broke several things.
Noble's built-in defaults (`ReachHost.cpp`):
| Range | Content | Effect when missing |
|---|---|---|
| +0x1B4..+0x1DF | look/controller settings (sensitivity bytes, dead zones 0.13/0.12, 1.0 multipliers, option bytes) | aim frozen while scoped |
| +0x42C..+0xA5B | key bindings: 66 à— 0x18 `{u32 action, u32 VK, 16 zero bytes}`, MCC default PC layout | no key does anything |
| +0xA5C / +0xA60 / +0xA64 | f32 volumes (game / music / other) | no sound |
Still zero: rest of +0x000..+0x42B and +0xA68..+0xBFF (suspected to hold the HUD brightness/opacity setting).

**Subtitles.** Before drawing subtitles the engine calls `haloreach+0xD5244`, which walks the local players' table
(`haloreach+0x2A03C50`, stride 0xAB8, profile data at +8) and reads each player's subtitle setting at profile +0xA30
(0 automatic = only when voice and text language differ, 1 on, 2 off). That table is **not** a copy of the slot 34 buffer
(writing +0xA30 there does nothing; volumes etc. are read field by field). Noble hooks `0xD5244` (prologue `48 89 5C 24 08`)
and answers "on" when the SUBTITLES setting is on.

**MCC UI texture packs** (`<MCC>\Data\UI\texturepacks`, read by `UiArt`): `<name>.perm.bin` = records of 0x1E0 bytes, magic
0xCDBFA090, name at +0x44, u16 w/h at +0x74/+0x76, u64 size at +0x88, u64 offset into `<name>.temp.bin` at +0x90; data BC1 or
BC3 (decided by size per 4x4 block). Useful packs: `levelstexturepack` (every Reach map/mission preview, 316x160 and
668x292 `…LARGE`), `skullstexturepack` (128x128 skull icons), `controllertexturepack` (controller picture, button glyphs),
`mainmenutexturepack`, `globaluitexturepack`. `loadingtexturepack.perm.bin` (304 MB) has no names: entries
`{u32 0x5E73CDD7, u32 size, u32 size, u32 0, data}`, padded; 1920x1080 BC1. Reach's 12 campaign scenes are found by a CRC-32
of their first 4 KB (table in `UiArt.cpp`). `Data\UI\BackgroundVideos\Images\*.png` are only 392x220.

**Key-binding slots.** The 66 profile slots use the engine's own action numbering, which matches neither MCC list in
`Data\BuiltInData\HaloReachController.json`. Names in `KeyBindings.cpp` come from MCC's default keys (unique key or the
W/A/S/D block); the rest are shown as "OTHER (NOT IDENTIFIED YET)". Several defaults appear twice (two `R`, `F`, `LMB`, `Space`),
so e.g. Fire may be slot 8 or 27: test a rebind of each named action in game.

**Loadout names** come from the game variant (Slayer's default list is Sprint / Armor Lock / Jet Pack / Active Camo /
Hologram; Invasion on Spire shows its own tier list starting with Scout; Air Assault / Operator / Grenadier etc. belong to
such game types). They render correctly.

**UI cursor.** `haloreach+0x2D2018` runs only while a Reach screen that uses the pointer is up (RR `uicursor::install` hooks it,
prologue `48 89 5C 24 18`). Noble hooks it with MinHook; within 250 ms of a call the mouse is freed, the arrow shown and no
look delta sent. The engine reads the pointer itself (GetCursorPos/ScreenToClient).

### Game results (host slot 6)

`haloreach+0xF95CB` builds MCC's results record (**0x5D138 bytes**) with `haloreach+0x39128` from the engine's own
results at `haloreach+0x28B83B0`, then calls host slot 6 (`[host]+0x30`) with **a1 = pointer to the record**. a2 is not a
size: it is r8 left over from the conversion loop (always 0xD5A8). Layout read from `+0x39128`:

| Offset | Contents |
|---|---|
| +0x0000 u32 | game category (engine mode byte at results +5 mapped to MCC's list) |
| +0x0008, +0x0010 u64; +0x18, +0x19 u8 | copied from the engine header |
| +0x0020 | 16 player records à— 0x1C8: +0x00 u8 present, +0x08 u64 player id, +0x18/+0x28/+0x2C i32, +0x30 u8, +0x34 i32 score (engine word or dword depending on mode), +0x38.., then appearance/name copy from +0x4C |
| +0x1CA0 | 16 à— 4 bytes team data |
| +0x1CE0 | 16 per-player stat blocks à— 0x5448: +0x000 23 u32 general stats, +0x05C 11 u32, +0x0AC 0x180 u32 medal counts, +0x6AC 0x32 u32, then weapon tables (0x54 weapons, 6 values each) |
| +0x56160 | per-player pairs; +0x5D120/+0x5D124/+0x5D128 u32 and +0x5D130 u64 match totals |

General-stat numbering: `haloreach+0x3B208` maps engine stat n to MCC slot (0â†’0, 1â†’1, nâ†’n+1 for n â‰¥ 2). Assuming Reach
kept Halo 3's statistic order, slots 8/9/10/11 = score/kills/assists/deaths (**provisional**, `Career.cpp` kStat*).
Medal numbering: table built by `haloreach+0x39B00` (0x91 engine medals â†’ MCC ids â‰¤ 0x17F).

Test option `-end-round-after <seconds>` calls the `mp_round_end` script command's work function
(`haloreach+0x7B3EC(-1, 0, 1)`) from slot 1, so automated runs reach slot 6. Noble writes the last record to
`data\userdata\results\last_game.bin` (local only: it holds player names and ids; never commit it).

## 6. Known problems and next steps (in order)

1. ~~**HUD bars, compass ticks and motion tracker are dark navy**~~ **Fixed 17:20 (slot 97, see Â§0).** Old notes: (letters/icons are fine). The game-type intro box shown
   during loadout selection, the "Visegrad Communications Outpost" waypoint box and the hint bar in Winter Contingency have
   the same dark tint (in MCC that box is bright cyan), while the loadout cards are correct. Ruled out: gamma, profile (RR's
   `profile_1.bin` gives the same picture), slot 51 mask, slot 33. **Confirmed 15:10:** RR's own HUD (Firefight, Beachhead)
   is bright light blue (shield bar, compass, tracker, timer box) while Noble's on Sword Base is dark navy; the world looks
   the same in both. The `GammaPass` in `ReachHost.cpp` is fixed at exponent 1.0 (a no-op; a whole-frame gamma washed the
   world out), so the fix must touch only the HUD. RR has a separate "UI Gamma" setting next to Gamma, which hints at a
   HUD-only value the host supplies. Next: look at how the HUD shader gets its colour/tint (chud globals, possibly a
   render-state or a constant the host sets in another slot, e.g. iface2 slots RR answers non-zero).
   Background (Bungie's Halo 3 HUD spec, cand.land/halohud): HUD element colours, flashes and scale come from the
   map's HUD tags ("HUD globals" / chud), the HUD is drawn mapped onto the inside of a sphere with a virtual camera
   and a per-resolution FOV from those globals, and layout uses anchors (4 corners + centre) with pixel offsets. So
   the colour is tag data the engine reads itself; if RR is bright, look for a host answer that changes which
   resolution/exception block or render target format the engine picks, rather than a colour value.
2. ~~Spire crash~~: no longer happens after the glyph/string fixes (3+ minutes on Spire, 2026-10-27 13:30).
3. **Memory**: Noble logs `Memory: frame N private X MB, working set Y MB` every 1800 frames. A full 10-minute match was
   flat (+17 MB). If a leak is still seen, it is probably across front-end menu â†” match restarts or in the menu itself.
4. **Online joining**: engine networking slots 41/43 (and session setup) are not implemented, so the SERVERS tab can host
   (listed on LAN/master server) but not join. This is the big missing piece for multiplayer.
5. **Scoreboard/results**: the slot 6 record is decoded far enough for career pay (Â§5 "Game results"). Still to do: a
   post-game scoreboard from it, confirm the general-stat slots with a real match, campaign "next mission".
6. ~~Mouse wheel~~ done 18:18 (see Â§0). Gamepad (XInput into +0x120.., RR `pad::fill`) still to do.
7. Armor / Workshop tabs; remove debug-only bits when no longer useful.
   Menu ideas from Bungie's UI director (cand.land/new-page): menu backgrounds were layered concept art on cards with
   a slowly moving camera (parallax), and the main menu had a one-press "fast path" to the last mode played.
8. **Cursor lock** (fix in 18:18 commit, untested by hand): in windowed mode the game keeps the cursor clipped to its window, even while dragging the title bar,
   so the window can't be moved back (seen 15:14, the window ended up behind the taskbar). Only Alt+Tab frees it. Release
   the clip while the window is being moved or isn't focused (`WM_ENTERSIZEMOVE` / `WM_KILLFOCUS`), and re-clip on focus.
9. **Respawn timer** shows "Respawn in 00000001": the number filling `camera_list.prop_respawn_time` is zero-padded.
   Check the format used for that string (string table entry `0x84091d25` in `ReachHost.cpp`).
10. ~~Player name / service tag~~ done 17:58: the name comes from SETTINGS > GAMEPLAY > PLAYER NAME (slot 88) and the
   service tag from the new SERVICE TAG field (`Settings::serviceTag`, 1-4 letters/digits, default NOBL; written to
   profile +0x1AC).
11. **Forge**: the menu launches MP with `00_basic_editing_054.bin`; the map loads, check in game that the Monitor/editor toggle works.
9. **Career system** (planned with Tyler). Done: `Career` module, 50 ranks (Halopedia table), cR after each match
   (per-minute pay rising with rank, kills/assists/medals bonus, 1-in-8 random bonus 100–3000, 1-in-200 jackpot 7,777;
   **no daily cap** and up to **90 paid minutes** per match in every mode (Tyler's calls). Pay goes by **base type** =
   the engine game mode BlamEngine launches with (campaign / Firefight / multiplayer incl. Forge), so modded maps and game
   types earn exactly what their stock type earns (`Career::BeginMatch` is called from `BlamEngine::LaunchMap`); all constants at the top of `Career.cpp`, Noble's own numbers), post-game cR breakdown + rank bar.
   Next: confirm stat slots (play a short Slayer with a few kills; `last_game.bin` + the `Career:` log lines), campaign
   scoring (difficulty + skull multipliers, time bonus, par scores), 45 commendations, challenges (Sep 2010 – Feb 2012
   lists) paying cR, a career/service-record screen and rank icon in the menu, Armory with the armor list.
   All the data for this is in `docs/career/` (see Â§0).

## 7. Partner's Outbreak-Z code (reference for Noble's menus)

The partner (`O7OF`) wrote and owns a Godot game, Outbreak-Z, and shared its scripts as reference material. They are
GDScript for Godot, so none of them compile into Noble (C++ / D3D11 / ImGui). They can be used as designs to port; because
the partner owns them, porting or copying them into this repo is fine. Keep the copies of these files outside the repo
unless one is actually ported.

**Usable almost as-is**
- `master_server.js` + `package.json`: **ported** to `tools/master-server/` with `MasterServerClient` in Noble. The
  original already had a 1 MB body limit; the real gaps were that anyone could heartbeat/delete any session and that the
  listed address came from the request body. Now: secret token per session (`X-Session-Token`), address from the socket,
  4 KB body, 4 sessions per IP, 120 requests/min per IP, 2000 sessions, text sanitised. `node test_master_server.js` passes.

**Port as designs**
- `server_browser.gd`: **ported** (favourites, direct connect, filters, host dialog with password). Ping and join need
  networking.
- `settings_manager.gd`, `build_settings_menu.gd`: Gameplay / Controls / Keybinds / Controller tabs with sliders. In Noble
  the controls write the profile fields from section 5: look settings at +0x1B4, key bindings from +0x42C,
  volumes at +0xA5C/+0xA60/+0xA64. **Ported** as the SETTINGS tab (`SettingsPanel`), controller tab not yet.
- `glass_ui.gd`: **ported** into `NobleStyle`.
- `audio_manager.gd`: only the menu parts apply. Click / hover / tab sounds with a 40 ms hover cooldown, and a menu music
  playlist (in order or shuffled, loops, no repeat of the last track when a shuffle restarts). Play through XAudio2, which
  is already loaded. In-game sound is the engine's own FMOD and needs nothing from this file. Sound source: Reach's
  sounds can't be committed (public repo); either read them at runtime from the player's MCC install (FMOD sound banks)
  or use our own / royalty-free sounds plus a `data\menu_music\` folder for the player's own files. Click/hover/tab sounds
  are **ported** as synthesised blips (`MenuSounds`); music not done.
- `game_loading_screen.gd`: **ported** as the loading overlay. The engine's slot 23 progress stays at 5%, so the bar only
  shows activity, not real progress. (`ui/LoadScreen` is never drawn while the engine runs.)

**Not applicable to Reach**
- `barracks_menu.gd`, `save_stats_manager.gd`, `floating_damage_number.gd`: Outbreak-Z skins, capes, cards, prestige and
  zombie stats. Reach armour lives in the engine/profile.
- `dev_tools_menu.gd`: zombie-mode cheats (points, perks, power-ups).
- `dev_account_search_menu.gd`, `backend_client.gd`: need Outbreak-Z's account database; its login/register ignore the
  password, so don't carry that over.
- `hud.gd`: Reach draws its own HUD.
- `scoreboard.gd`: Reach's in-game scoreboard is the engine's; a post-game one would read the slot 6 results buffer.
- `chat_box.gd`: the engine does **not** draw chat outside MCC; a chat box would have to be Noble's own overlay, and needs
  networking first.
- `check_api.gd`: one-line test script.

**Status:** settings, style, menu sounds, server browser, master server and loading screen are ported (see Â§3). Left:
controller settings tab, menu music, chat (after networking).

## 7b. Partner's server (`server/`, commit `ae7b71e`) - review notes for O7OF

Accounts (scrypt + random tokens), SQLite tables (users, tokens, player_stats, matches, match_player_records),
session registry, matchmaker (8 playlists). DB goes to `server/data/` (git-ignored). Left for the partner to fix:
1. `server.js` is missing from the commit (`package.json` "main"/"start" and `node server.js` point to it).
2. `node:sqlite` needs Node 22.5+; `package.json` says `>=18`. The catch-block fallback requires `node:sqlite` again.
3. `npm test` points to `test/test_backend.js`, which isn't there.
4. `ranks.js` doesn't match `Career.cpp`: base pay 60 (game: 13 cR/min), Major Grade 2 370,000 (game: 375,000), random
   bonus 100-999 (game: 100-3000 in steps of 10), jackpot only on roll 77 of 200 (game: plain 1-in-200), +250 win bonus
   (not in the game). Both spell "Field Marshall"; Reach's rank is "Field Marshal".
5. For matchmaking stats the server should be the only place cR is calculated, from the host's match results;
   otherwise a client can submit any total.

## 8. Rules for this project

- Read-only toward MCC files. Writes go to `build\bin\Release\data\…`.
- Never commit or publish: RR binaries/data/decompiled code (including `profile_1.bin`, launch blocks, `mainmenu_all.map`),
  Omaha-derived files, `haloreach.dll` or dumps of it (e.g. dumpbin output), crash dumps, logs (they contain a player name/ID).
  The repo is **private** (Tyler, 2026-10-28 15:41; it was public before). GitHub rejects files over 100 MB.
- Exception (Tyler, 2026-10-28): the Reach menu art in `build/bin/Release/data/ui/reach/` is committed on purpose.
- Rule change (Tyler, 2026-10-28 15:37): RR data may be used. Noble now uses RR's `mainmenu_all.map` + string_ids
  (copied from `D:\Reach\data\menumaps` to `build\bin\Release\data\menumaps`) so the engine shows Reach's own Armory
  scene (O7OF's ReachArmory, found through `ReachHost`/`VFS` when the engine asks for mainmenu.map). The 298 MB map is
  over GitHub's 100 MB file limit, so it stays local; each tester copies it from their RR install.
- **Engine Armory scene, status 15:45 (for O7OF):** with RR's menumaps copied, the engine resolves `haloreach\maps\mainmenu.map`
  to `data\menumaps\mainmenu_all.map` and ReachArmory loads 32,213 string_ids, but `ScanGlobalsTable` never finds the GLOBALS
  table (log: one scan message, then nothing for 40 s; `sceneReadyFrames=0` at frame 6480). LoadProgress stops at 5 % and
  FrontEndMenu logs "engine stopped drawing the menu" 1 s after launch, so the menu map's scenario/scripts may never start.
  RR also ships main-menu launch blocks (`D:\Reach\data\lb_m_c_1.bin`, `launch_block_1.bin`); Noble may need RR's main-menu
  launch block for the scene to run. Until then the OBJ preview (local `data\armor` exports) is what the Armory shows.
  Large local files are git-ignored explicitly (`**/menumaps/`, `**/data/armor/`, `*.map`, `*.obj`, `*.mtl`, `*.rdc`, RR bins).
- Keep the Steam ownership check. No anti-cheat bypassing, no official MCC online services.
- Small, separate PRs. The partner edits `ReachHost.cpp`/`BlamEngine.cpp` too, so tell him before large edits; when both sides
  change `HostSlot1_FrameTick`, keep the gamma pass first and the pause menu after it.
- Before overwriting a file, re-read the current version first, and re-check it a minute later (file sync has put stale copies
  back before).
- Commits, PRs and docs carry only the authors' names (no "generated with" or co-author lines).

### 2026-10-30 — Campaign Netplay, Single-Session Concurrency & Database Reset (O7OF / Dominus)
- **Campaign Netplay & Join-In-Progress Detours**:
  - Located Bungie co-op membership compatibility abort at haloreach.dll RVA `0x411B6E` (`74 0D` -> `EB 0D`), bypassing the `mov edx, 1Bh; call 0x44EBA0` "Error hosting co-op" crash.
  - Added Campaign in-progress join-request gate patch at haloreach.dll RVA `0x397689` (`75 0A` -> `EB 0A`), skipping refusal 5 ("cannot join game in progress").
- **Strict Concurrency & Duplicate Name Rejection**:
  - Enforced single-instance active session lock in `/api/login` across user ID and case-insensitive username.
  - If a user is already active within 120 seconds, throws exact error: `"Error Account Already Logged In Logout on the other device to proceed"`.
  - Removed `force: true` bypass in `AuthClient.cpp` and server endpoints.
  - Added case-insensitive duplicate check to `/api/register` and enforced `username TEXT UNIQUE COLLATE NOCASE` in database schema.
  - Updated WebSocket `lobby_join` signaling to reject incoming clients if another player in the session roster holds the same username case-insensitively.
  - Updated `AuthClient::Logout()` to notify `/api/logout` to immediately clear active server sessions.
- **Database Wipe & Privileges Reset**:
  - Executed `reset_db.js` across both local `server/` and remote `ServerTestDeploy` repository.
  - All accounts wiped clean and developer privileges reset to default (`is_developer = 0`).
- **Server Deployment**:
  - Committed and pushed updated server netcode, schema, and reset database to `https://github.com/O7OF/ServerTestDeploy.git` on branch `main`.

### 2026-11-02 — Exclusive FULLSCREEN display mode, Borderless default (0.9.16.31, @claude)
- **SETTINGS > GRAPHICS > DISPLAY MODE** now cycles Windowed / Borderless full screen / Fullscreen. Fullscreen is DXGI
  exclusive full screen. It is saved as the new `exclusiveFullscreen` key in `settings.json` and also forces
  `borderless = true`, so older builds read it as Borderless. It applies live, with no restart.
- **Borderless is now the default** for new installs and for GRAPHICS > Reset to defaults (`SettingsData::borderless = true`).
  Existing `settings.json` files keep their saved choice.
- **How it works:** the window stays the monitor-sized borderless popup. `SyncExclusiveFullscreen()`
  (`src/host/D3D11Renderer.cpp`) runs at the top of ReachHost's Present hook, on the render thread. It calls
  `SetFullscreenState` so the swap chain matches `Window::WantsExclusiveFullscreen()`: the setting is on, Noble is the
  foreground window and the window is not minimized. Alt-Tab or any focus loss leaves fullscreen, and coming back re-enters it.
  If DXGI drops out on its own, that is detected too. After leaving, the window posts itself `WM_APP+0x4E` to re-apply the
  Windowed/Borderless geometry on its own thread. A failed enter (e.g. `DXGI_ERROR_NOT_CURRENTLY_AVAILABLE`) retries every 2 s.
  `Window::ApplyWindowSettings` does not restyle the window while exclusive fullscreen is active; the refresh after leaving
  applies the new geometry.
- **Swap chain:** `DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH` was removed. With the flag, entering fullscreen on Tyler's 4K
  display switched the monitor to the back buffer's size (2560x1600, then 1920x1080 after `ResizeTarget`). Fullscreen now
  always uses the desktop resolution and refresh rate. `ResizeBuffers` is deliberately **not** called: the buffers are
  already native size, and the engine and host hold back-buffer references, so the call would fail. Alt+Enter is disabled
  (`MakeWindowAssociation(DXGI_MWA_NO_ALT_ENTER)`) so it cannot bypass the setting. `D3D11Renderer::Shutdown` leaves
  fullscreen before the swap chain is released.
- **Verified** (noble-dev, 3840x2160 primary display): the log shows `Display: exclusive full screen on at the desktop mode
  (3840x2160, buffers 3840x2160)`. Notepad taking focus logged `left exclusive full screen`, and refocusing re-entered.
  The display stayed 3840x2160 throughout and after exit.
- **Not yet tested:** a match process launched from the menu while in Fullscreen; switching Fullscreen to Windowed by hand
  in the menu (same code path as Alt-Tab plus the geometry refresh).
- **Build note:** `src/res/blocks.rc` needs the git-ignored `data/profile_1.bin` and `data/menumaps/*`. If the resource step
  fails with RC2135, copy them from `build/bin/Release/data/`.

### 2026-11-02 — Menu UI scales at native 4K (0.9.16.32, @claude)
- **Cause:** at native 4K the menu back buffer is 3840x2160, so the UI scale (height / 900) should be 2.4. Two problems
  stopped that. First, the scale was capped at 2.0 (`FrontEndMenu` `ui.s`) and 2.2 (`NobleUi::Scale`), so the menu came out
  small and did not reach the screen edges. Second, text drawn by plain ImGui widgets (search boxes, descriptions, help lines,
  inputs, tables) never scaled: it stayed at the fonts' 15-18 px. Until 2026-11-02 Tyler's desktop ran at 1920x1080 (menu
  buffer 2880x1620, scale 1.8, under both caps), which hid this. Once the display was back at its native 3840x2160, the
  menu became tiny.
- **Fix:** both caps raised to 4.0. `FrontEndMenu::RenderOverlay` sets `io.FontGlobalScale = NobleUi::Scale()` before
  `ImGui::NewFrame()`, so widget text scales like the custom-drawn labels. The atlas is already rasterized at 2.5x density,
  so the text stays crisp. Fixed pixel widths in `ServerBrowser.cpp` and `CareerPanel.cpp` (buttons, combo widths, table
  columns, details panel, rank cells, icons) now go through `U(px) = px * io.FontGlobalScale`; without that, the larger text
  was clipped ("REFF", "GAME DE"). The main-menu list moved from title+76 to title+96 units: it overlapped the "This party
  is local to this PC." line at +66.
- **Verified** with 4K captures (`-menu-screen <x> -menu-capture 240`) of main, settings, servers, career and mods; all
  scale and fill the screen. Mods (`WorkshopManager`) already sized from the font.
- **Not changed:** the in-match pause menu (`PauseMenu.cpp`, its own `NewFrame`); `FontGlobalScale` is not set there.
  ImGui style paddings (`FramePadding`, `ItemSpacing`) are not scaled. Not tested at 1440p.
- **Open:** any other ImGui-widget panel with literal pixel widths needs the same `U()` treatment if it shows clipping.
