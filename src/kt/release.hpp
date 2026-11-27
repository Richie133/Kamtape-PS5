// KamTape PS5 - reading the app's GitHub releases: which one is newer and installable.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>

namespace hui::kt
{

constexpr const char *kTitleId = "PPSA52005";
constexpr const char *kAppName = "KamTape for PS5";
constexpr const char *kReleasesApi = "https://api.github.com/repos/Richie133/Kamtape-PS5/releases?per_page=10";
constexpr const char *kReleasesPage = "https://github.com/Richie133/Kamtape-PS5/releases";
constexpr const char *kReleaseZip = "PPSA52005.zip";

struct Release
{
    std::string tag;             // "v0.4.0-beta"
    std::string version;         // "0.4.0 beta", for display
    std::string content_version; // "01.000.004"
    std::string notes;           // what's new, plain text
    std::string page;            // the release on github.com
    std::string zip_url;
    std::string sha256; // lowercase hex
    std::uint64_t size = 0;
};

// The newest release in a GitHub API answer (a JSON array of releases) that
// is newer than installed (a contentVersion) and can be installed. Empty tag:
// none; why says what was wrong when the answer couldn't be read.
Release pick_release(const std::string &json, const std::string &installed, std::string *why);
// The pieces, for tests.
std::string content_version_in(const std::string &notes); // "contentVersion: `01.000.004`"
std::string sha256_in(const std::string &notes);          // "`PPSA52005.zip` sha256: `<hex>`"
std::string readable_notes(const std::string &body);      // without those lines and markdown marks
std::string display_version(const std::string &tag);      // "v0.4.0-beta" -> "0.4.0 beta"
// titleId and contentVersion from a param.json; false when unreadable.
bool read_param(const std::string &path, std::string *title, std::string *content_version);

} // namespace hui::kt
