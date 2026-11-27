// KamTape PS5 - tests for reading GitHub releases for the updater (make test).
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "kt/release.hpp"

#include <cstdio>
#include <string>

namespace
{

int failures = 0;

void check(bool ok, const char *what)
{
    if (!ok)
    {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

} // namespace

int main(int argc, char **argv)
{
    using namespace hui::kt;
    const std::string root = argc > 1 ? argv[1] : ".";
    std::string json;
    hui::save::read_file(root + "/host/fixtures/releases.json", &json);
    const std::string sha(64, 'a');

    check(content_version_in("x\ncontentVersion: `01.000.004`\n") == "01.000.004", "contentVersion line");
    check(content_version_in("contentVersion: 1.0") == "", "not a content version");
    check(content_version_in("contentVersion 01.000.0045") == "", "digits after the version");
    check(sha256_in("`PPSA52005.zip` sha256: `" + sha + "`") == sha, "sha256 line");
    check(sha256_in("`PPSA52005.zip` sha256: `" + sha.substr(1) + "`") == "", "short sha256");
    check(display_version("v0.4.0-beta") == "0.4.0 beta" && display_version("1.2") == "1.2", "display version");
    const std::string notes = readable_notes("## What's new\r\n\n- **Bold** `code`\n* star\n\n\n`PPSA52005.zip` sha256: `" +
                                             sha + "`\ncontentVersion: `01.000.004`\n");
    check(notes == "What's new\n\n- Bold code\n- star", "readable notes");

    std::string why;
    Release r = pick_release(json, "01.000.004", &why);
    check(r.tag == "v0.9.9-beta" && r.content_version == "01.000.099" && r.version == "0.9.9 beta", "newer release");
    check(r.size > 0 && r.sha256.size() == 64 && r.zip_url.rfind("https://github.com/", 0) == 0, "its zip");
    check(r.notes.find("sha256") == std::string::npos && r.notes.find("What's new in 0.9.9") == 0, "its notes");
    why.clear();
    r = pick_release(json, "01.000.099", &why);
    check(r.tag.empty() && why.empty(), "up to date");
    r = pick_release(json, "01.000.100", &why);
    check(r.tag.empty() && why.empty(), "newer than every release");

    // the digest from the notes when GitHub has none; drafts, other hosts and broken answers are skipped
    const std::string body = "contentVersion: `01.000.010`\\n`PPSA52005.zip` sha256: `" + sha + "`";
    const std::string asset = "{\"name\":\"PPSA52005.zip\",\"size\":5,\"browser_download_url\":"
                              "\"https://github.com/Richie133/Kamtape-PS5/releases/download/v1/PPSA52005.zip\"}";
    r = pick_release("[{\"tag_name\":\"v1\",\"draft\":false,\"body\":\"" + body + "\",\"assets\":[" + asset + "]}]",
                     "01.000.004", &why);
    check(r.tag == "v1" && r.sha256 == sha, "digest from the notes");
    r = pick_release("[{\"tag_name\":\"v1\",\"draft\":true,\"body\":\"" + body + "\",\"assets\":[" + asset + "]}]",
                     "01.000.004", &why);
    check(r.tag.empty(), "drafts are skipped");
    std::string elsewhere = asset;
    elsewhere.replace(elsewhere.find("https://github.com"), 18, "https://evil.example");
    r = pick_release("[{\"tag_name\":\"v1\",\"body\":\"" + body + "\",\"assets\":[" + elsewhere + "]}]", "01.000.004",
                     &why);
    check(r.tag.empty(), "zips not on github.com are skipped");
    why.clear();
    r = pick_release("{\"message\":\"API rate limit exceeded\"}", "01.000.004", &why);
    check(r.tag.empty() && !why.empty(), "a GitHub error answer");

    std::string title, version;
    check(read_param(root + "/sce_sys/param.json", &title, &version) && title == "PPSA52005" && version.size() == 10,
          "the app's param.json");

    std::printf(failures ? "%d failure(s)\n" : "all release tests passed\n", failures);
    return failures ? 1 : 0;
}
