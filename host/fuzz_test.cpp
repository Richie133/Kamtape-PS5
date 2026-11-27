// KamTape PS5 - throws random and mangled input at the parsers that read
// network data (XML, API answers, addresses, JSON). Build it with
// -fsanitize=address,undefined: it passes when nothing crashes or trips them.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "kt/address.hpp"
#include "kt/api.hpp"
#include "kt/json.hpp"
#include "kt/release.hpp"
#include "kt/xml.hpp"

#include <cstdio>
#include <random>
#include <string>

int main(int argc, char **argv)
{
    const std::string dir = argc > 1 ? argv[1] : "host/fixtures";
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 20000;
    std::vector<std::string> seeds;
    for (const char *name : {"featured.xml", "details.xml", "fail.xml", "by_tag.xml", "releases.json", "releases.json"})
    {
        std::string text;
        hui::save::read_file(dir + "/" + name, &text);
        seeds.push_back(text);
    }
    seeds.push_back("{\"a\":[1,2,{\"b\":\"c\\u00e9\"}],\"d\":null}");
    std::mt19937 rng(2005);
    const char alphabet[] = "<>/&;#x=\"' \n?!-[]CDATAabc0123456789:.%\\{}";
    for (int round = 0; round < rounds; ++round)
    {
        std::string s = seeds[rng() % seeds.size()];
        const int edits = 1 + static_cast<int>(rng() % 12);
        for (int e = 0; e < edits && !s.empty(); ++e)
        {
            const std::size_t at = rng() % s.size();
            switch (rng() % 4)
            {
            case 0:
                s[at] = alphabet[rng() % (sizeof(alphabet) - 1)];
                break;
            case 1:
                s.erase(at, 1 + rng() % 16);
                break;
            case 2:
                s.insert(at, 1, static_cast<char>(rng() % 256));
                break;
            default:
                s = s.substr(0, at); // cut short
                break;
            }
        }
        (void)hui::kt::parse_xml(s);
        std::vector<hui::kt::Video> videos;
        (void)hui::kt::parse_video_list(s, &videos);
        hui::kt::VideoDetails details;
        (void)hui::kt::parse_video_details(s, "x", &details);
        hui::kt::JsonValue json;
        (void)hui::kt::parse_json(s, json);
        std::string why;
        (void)hui::kt::pick_release(s, "01.000.001", &why);
        (void)hui::kt::readable_notes(s);
        (void)hui::kt::sha256_in(s);
        (void)hui::kt::content_version_in(s);
        (void)hui::kt::normalize_address(s.substr(0, 300));
        (void)hui::kt::short_address(s.substr(0, 300));
        for (const auto &v : videos)
            (void)hui::kt::format_date(v.upload_time), (void)hui::kt::format_length(v.length_seconds),
                (void)hui::kt::format_count(v.view_count);
    }
    // deep nesting
    std::string deep(100000, '<');
    (void)hui::kt::parse_xml(deep);
    std::string nested;
    for (int i = 0; i < 5000; ++i)
        nested += "<a>";
    (void)hui::kt::parse_xml(nested);
    std::string arrays(100000, '[');
    hui::kt::JsonValue json;
    (void)hui::kt::parse_json(arrays, json);
    std::printf("fuzzed %d inputs without a crash\n", rounds);
    return 0;
}
