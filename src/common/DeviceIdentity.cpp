#include "DeviceIdentity.hpp"
#include "common/Logger.hpp"

#include <nlohmann/json.hpp>
#include <shlobj.h>
#include <windows.h>

#pragma comment(lib, "shell32.lib")

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace noble {
namespace {

std::filesystem::path ExeDataDir() {
  wchar_t exe[MAX_PATH] = {};
  if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0)
    return std::filesystem::path("data") / "userdata";
  return std::filesystem::path(exe).parent_path() / "data" / "userdata";
}

std::string MachineGuid() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0,
                    KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
    return {};
  wchar_t value[128] = {};
  DWORD size = sizeof(value);
  DWORD type = 0;
  const LONG rc = RegQueryValueExW(key, L"MachineGuid", nullptr, &type,
                                   reinterpret_cast<LPBYTE>(value), &size);
  RegCloseKey(key);
  if (rc != ERROR_SUCCESS || type != REG_SZ)
    return {};
  std::wstring wide(value);
  while (!wide.empty() && wide.back() == L'\0')
    wide.pop_back();
  const int n = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                    static_cast<int>(wide.size()), nullptr, 0,
                                    nullptr, nullptr);
  std::string out(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                      out.data(), n, nullptr, nullptr);
  return out;
}

std::string VolumeSerial() {
  wchar_t root[] = L"C:\\";
  DWORD serial = 0;
  if (!GetVolumeInformationW(root, nullptr, 0, &serial, nullptr, nullptr,
                             nullptr, 0))
    return {};
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%08X", serial);
  return buf;
}

std::string MakeDeviceId() {
  const std::string seed =
      MachineGuid() + "|" + VolumeSerial() + "|noble-device-v1";
  // 64-bit FNV-1a, written as 16 hex chars. Stable across runs and machines.
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : seed) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}

std::string DefaultName(const std::string & /*id*/) { return "NobleSix"; }

std::filesystem::path PrimaryDataDir() {
  wchar_t exe[MAX_PATH] = {};
  if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0)
    return std::filesystem::path("data");
  return std::filesystem::path(exe).parent_path() / "data";
}

DeviceIdentity LoadOrCreate() {
  DeviceIdentityStore::MigrateNewestFile("userdata/identity.json");
  const auto path = ExeDataDir() / "identity.json";
  DeviceIdentity id;
  std::ifstream in(path);
  if (in.is_open()) {
    try {
      const auto j = nlohmann::json::parse(in);
      id.deviceId = j.value("deviceId", "");
      id.playerName = j.value("playerName", "");
      id.serviceTag = j.value("serviceTag", "");
      id.developer = j.value("developer", false);
    } catch (...) {
    }
  }
  if (id.deviceId.empty())
    id.deviceId = MakeDeviceId();
  auto trim = [](std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
              return !std::isspace(ch);
            }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
  };
  trim(id.playerName);
  if (id.playerName.empty() || id.playerName == "Player")
    id.playerName = "NobleSix";
  if (id.playerName.size() > 20)
    id.playerName.resize(20);
  if (id.serviceTag.empty())
    id.serviceTag = id.deviceId.substr(0, 4);
  for (char &ch : id.serviceTag)
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  if (id.deviceId.empty())
    id.deviceId = "0000000000000000";
  std::error_code ec;
  std::filesystem::create_directories(ExeDataDir(), ec);
  nlohmann::json j = {{"deviceId", id.deviceId},
                      {"playerName", id.playerName},
                      {"serviceTag", id.serviceTag},
                      {"developer", id.developer}};
  std::ofstream out(path, std::ios::trunc);
  out << j.dump(2);
  out.close();
  DeviceIdentityStore::MirrorSaveFile("userdata/identity.json");
  return id;
}

DeviceIdentity &Mutable() {
  static DeviceIdentity id = LoadOrCreate();
  return id;
}
} // namespace

namespace DeviceIdentityStore {
const DeviceIdentity &Get() { return Mutable(); }

std::filesystem::path DataDir() { return ExeDataDir(); }

std::filesystem::path UserDir() { return ExeDataDir() / Get().deviceId; }

std::vector<std::filesystem::path> KnownDataDirs() {
  std::vector<std::filesystem::path> dirs;
  auto addDir = [&](const std::filesystem::path &p) {
    if (p.empty())
      return;
    std::error_code ec;
    std::filesystem::path norm = p.lexically_normal();
    for (const auto &existing : dirs) {
      if (existing == norm)
        return;
    }
    dirs.push_back(norm);
  };

  addDir(PrimaryDataDir());

  // Documents\Noble Launcher\data
  wchar_t docs[MAX_PATH] = {};
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, nullptr,
                                 SHGFP_TYPE_CURRENT, docs))) {
    addDir(std::filesystem::path(docs) / "Noble Launcher" / "data");
  }

  // AppData\ProjectNoble\data
  wchar_t appdata[MAX_PATH] = {};
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr,
                                 SHGFP_TYPE_CURRENT, appdata))) {
    addDir(std::filesystem::path(appdata) / "ProjectNoble" / "data");
  }

  // Known data directories are strictly client runtime paths (Documents & AppData)

  return dirs;
}

bool MigrateNewestFile(const std::filesystem::path &relativeFilePath) {
  const auto primary = PrimaryDataDir() / relativeFilePath;
  const auto dirs = KnownDataDirs();

  std::filesystem::path newestPath;
  std::filesystem::file_time_type newestTime{};
  bool foundAny = false;

  std::error_code ec;
  for (const auto &dir : dirs) {
    const auto cand = dir / relativeFilePath;
    if (std::filesystem::is_regular_file(cand, ec)) {
      const auto candTime = std::filesystem::last_write_time(cand, ec);
      if (!ec) {
        if (!foundAny || candTime > newestTime) {
          newestTime = candTime;
          newestPath = cand;
          foundAny = true;
        }
      }
    }
  }

  if (!foundAny)
    return false;

  // If the primary file already exists and is equal or newer than newestPath,
  // nothing to migrate
  if (std::filesystem::is_regular_file(primary, ec)) {
    const auto primaryTime = std::filesystem::last_write_time(primary, ec);
    if (!ec && primaryTime >= newestTime) {
      return false;
    }
  }

  // Copy newestPath to primary
  if (newestPath != primary) {
    if (primary.has_parent_path()) {
      std::filesystem::create_directories(primary.parent_path(), ec);
    }
    std::filesystem::copy_file(
        newestPath, primary, std::filesystem::copy_options::overwrite_existing,
        ec);
    if (!ec) {
      std::filesystem::last_write_time(primary, newestTime, ec);
      NOBLE_INFO("DeviceIdentity: Migrated newest '{}' from '{}' to '{}'",
                 relativeFilePath.string(), newestPath.string(),
                 primary.string());
      return true;
    }
  }
  return false;
}

void MirrorSaveFile(const std::filesystem::path &relativeFilePath) {
  const auto primary = PrimaryDataDir() / relativeFilePath;
  std::error_code ec;
  if (!std::filesystem::is_regular_file(primary, ec))
    return;

  const auto primaryTime = std::filesystem::last_write_time(primary, ec);
  const auto dirs = KnownDataDirs();

  for (const auto &dir : dirs) {
    const auto dest = dir / relativeFilePath;
    if (dest == primary)
      continue;
    // Mirror to dir if the dir exists or parent exists
    if (dir == dirs.back() || std::filesystem::exists(dir, ec)) {
      if (dest.has_parent_path()) {
        std::filesystem::create_directories(dest.parent_path(), ec);
      }
      std::filesystem::copy_file(
          primary, dest, std::filesystem::copy_options::overwrite_existing, ec);
      if (!ec && primaryTime != std::filesystem::file_time_type{}) {
        std::filesystem::last_write_time(dest, primaryTime, ec);
      }
    }
  }
}

void Save(const DeviceIdentity &identity) {
  DeviceIdentity sanitized = identity;
  auto trim = [](std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
              return !std::isspace(ch);
            }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
  };
  trim(sanitized.playerName);
  if (sanitized.playerName.empty() || sanitized.playerName == "Player")
    sanitized.playerName = "NobleSix";
  if (sanitized.playerName.size() > 20)
    sanitized.playerName.resize(20);

  std::error_code ec;
  std::filesystem::create_directories(ExeDataDir(), ec);
  nlohmann::json j = {{"deviceId", sanitized.deviceId},
                      {"playerName", sanitized.playerName},
                      {"serviceTag", sanitized.serviceTag},
                      {"developer", sanitized.developer}};
  std::ofstream out(ExeDataDir() / "identity.json", std::ios::trunc);
  out << j.dump(2);
  out.close();
  MirrorSaveFile("userdata/identity.json");
  Mutable() = sanitized;
}

void Reload() { Mutable() = LoadOrCreate(); }
} // namespace DeviceIdentityStore
} // namespace noble
