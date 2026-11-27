// KamTape PS5 - reading the app's GitHub releases: which one is newer and installable.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/release.hpp"

#include "core/save_file.hpp"
#include "kt/json.hpp"
#include "third_party/update_check/self_update.h"
#include "third_party/update_check/update_check.h"

#include <cctype>

namespace hui::kt
{

namespace
{

bool is_hex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

std::string lower(std::string s)
{
    for (char &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// The first NN.NNN.NNN at or after `from`, within the same line.
std::string version_after(const std::string &text, std::size_t from)
{
    const std::size_t line_end = text.find('\n', from);
    const std::size_t end = line_end == std::string::npos ? text.size() : line_end;
    for (std::size_t i = from; i + 10 <= end; ++i)
    {
        const std::string candidate = text.substr(i, 10);
        unsigned parts[3];
        if (update_check_version_parse(candidate.c_str(), parts) == 1 &&
            (i + 10 == end || !std::isdigit(static_cast<unsigned char>(text[i + 10]))))
            return candidate;
    }
    return {};
}

bool line_is_ours(const std::string &line)
{
    return line.find("contentVersion") != std::string::npos ||
           line.find(std::string(kReleaseZip) + "` sha256") != std::string::npos ||
           line.find(std::string(kReleaseZip) + " sha256") != std::string::npos;
}

} // namespace

std::string content_version_in(const std::string &notes)
{
    const std::size_t at = notes.find("contentVersion");
    return at == std::string::npos ? std::string() : version_after(notes, at + 14);
}

std::string sha256_in(const std::string &notes)
{
    const std::size_t at = notes.find(std::string(kReleaseZip) + "` sha256:");
    if (at == std::string::npos)
        return {};
    const std::size_t line_end = notes.find('\n', at);
    const std::size_t end = line_end == std::string::npos ? notes.size() : line_end;
    for (std::size_t i = at; i + 64 <= end; ++i)
    {
        std::size_t n = 0;
        while (n < 64 && is_hex(notes[i + n]))
            ++n;
        if (n == 64 && (i + 64 == end || !is_hex(notes[i + 64])) && (i == 0 || !is_hex(notes[i - 1])))
            return lower(notes.substr(i, 64));
    }
    return {};
}

std::string readable_notes(const std::string &body)
{
    std::string out;
    bool blank = false;
    std::size_t start = 0;
    while (start <= body.size())
    {
        std::size_t end = body.find('\n', start);
        if (end == std::string::npos)
            end = body.size();
        std::string line = body.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line_is_ours(line))
            continue;
        // markdown marks the screen doesn't draw
        std::size_t lead = 0;
        while (lead < line.size() && line[lead] == '#')
            ++lead;
        if (lead > 0)
            line = line.substr(line.find_first_not_of(' ', lead) == std::string::npos ? line.size()
                                                                                     : line.find_first_not_of(' ', lead));
        std::string plain;
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            if (line[i] == '`' || (line[i] == '*' && i + 1 < line.size() && line[i + 1] == '*'))
            {
                if (line[i] == '*')
                    ++i;
                continue;
            }
            plain += line[i];
        }
        if (plain.rfind("* ", 0) == 0)
            plain[0] = '-';
        if (plain.empty() && (out.empty() || blank))
            continue; // no blank lines at the top or two in a row
        blank = plain.empty();
        out += plain;
        out += '\n';
        if (out.size() > 4000)
            break;
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == ' '))
        out.pop_back();
    return out;
}

std::string display_version(const std::string &tag)
{
    std::string v = tag;
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V'))
        v.erase(0, 1);
    for (char &c : v)
    {
        if (c == '-')
            c = ' ';
    }
    return v;
}

bool read_param(const std::string &path, std::string *title, std::string *content_version)
{
    std::string text;
    JsonValue root;
    if (!save::read_file(path, &text, 256u << 10) || !parse_json(text, root) || !root.is_object())
        return false;
    *title = root["titleId"].as_string();
    *content_version = root["contentVersion"].as_string();
    unsigned parts[3];
    return !title->empty() && update_check_version_parse(content_version->c_str(), parts) == 1;
}

Release pick_release(const std::string &json, const std::string &installed, std::string *why)
{
    Release best;
    JsonValue root;
    if (!parse_json(json, root) || !root.is_array())
    {
        if (why)
            *why = "GitHub's answer wasn't a list of releases";
        return best;
    }
    for (const JsonValue &r : root.items())
    {
        if (!r.is_object() || r["draft"].as_bool(false))
            continue;
        const std::string body = r["body"].as_string();
        Release candidate;
        candidate.tag = r["tag_name"].as_string();
        candidate.content_version = content_version_in(body);
        int comparable = 0;
        if (candidate.tag.empty() || candidate.content_version.empty() ||
            update_check_version_compare(candidate.content_version.c_str(), installed.c_str(), &comparable) <= 0 ||
            !comparable)
            continue; // not newer, or a release from before the app could update itself
        if (!best.tag.empty() &&
            update_check_version_compare(candidate.content_version.c_str(), best.content_version.c_str(),
                                         &comparable) <= 0)
            continue;
        for (const JsonValue &asset : r["assets"].items())
        {
            if (asset["name"].as_string() != kReleaseZip)
                continue;
            candidate.zip_url = asset["browser_download_url"].as_string();
            const double size = asset["size"].as_number(0.0);
            candidate.size = size > 0 ? static_cast<std::uint64_t>(size) : 0;
            // GitHub's own digest of the file, or the line the release workflow writes
            const std::string digest = asset["digest"].as_string();
            if (digest.rfind("sha256:", 0) == 0 && digest.size() == 71)
                candidate.sha256 = lower(digest.substr(7));
        }
        if (candidate.sha256.empty())
            candidate.sha256 = sha256_in(body);
        if (candidate.zip_url.empty() || candidate.sha256.size() != 64 ||
            !self_update_url_allowed(candidate.zip_url.c_str(), 0))
            continue;
        candidate.version = display_version(candidate.tag);
        candidate.notes = readable_notes(body);
        candidate.page = r["html_url"].as_string();
        if (candidate.page.size() >= sizeof(self_update_offer{}.page))
            candidate.page = kReleasesPage;
        best = std::move(candidate);
    }
    return best;
}

} // namespace hui::kt
