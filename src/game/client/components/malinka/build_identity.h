#ifndef GAME_CLIENT_COMPONENTS_MALINKA_BUILD_IDENTITY_H
#define GAME_CLIENT_COMPONENTS_MALINKA_BUILD_IDENTITY_H

#define MALINKA_VERSION "0.1.0"
#define MALINKA_UPSTREAM_COMMIT "cf192e3fb8590dbe2e4f66a191aa6616beff6975"
#define MALINKA_UPDATER_MARKER "BestClient-Malinka:updater-api=1"
#define MALINKA_BUILD_MARKER "BestClient-Malinka:api=1;version=" MALINKA_VERSION ";upstream=" MALINKA_UPSTREAM_COMMIT

namespace Malinka::Build
{
inline constexpr const char *ID = "BestClient-Malinka";
inline constexpr const char *REPOSITORY = "twin15388-beep/best-v2";
inline constexpr const char *RELEASES_URL = "https://api.github.com/repos/twin15388-beep/best-v2/releases?per_page=10";
inline constexpr const char *LATEST_URL = "https://github.com/twin15388-beep/best-v2/releases";
inline constexpr const char *DOWNLOAD_PREFIX = "https://github.com/twin15388-beep/best-v2/releases/download/malinka-v";
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
inline constexpr const char *PLATFORM = "windows-x86_64";
#elif defined(__linux__) && defined(__x86_64__)
inline constexpr const char *PLATFORM = "linux-x86_64";
#elif defined(__linux__) && defined(__aarch64__)
inline constexpr const char *PLATFORM = "linux-aarch64";
#else
inline constexpr const char *PLATFORM = "unsupported";
#endif
inline constexpr const char *MARKER_PREFIX = "BestClient-Malinka:api=1;version=";
}

#endif
