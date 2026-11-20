// KamTape PS5 - the app's settings as a small JSON file.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/settings.hpp"

#include "core/save_file.hpp"
#include "kt/json.hpp"

#include <algorithm>

namespace hui::kt
{

Settings Settings::load(const std::string &path)
{
    Settings s;
    std::string text;
    JsonValue root;
    if (!save::read_file(path, &text, 64u << 10) || !parse_json(text, root) || !root.is_object())
        return s;
    if (root["openXlOnStart"].is_bool())
        s.open_xl_on_start = root["openXlOnStart"].as_bool();
    if (root["sounds"].is_number())
        s.sounds = std::clamp(static_cast<int>(root["sounds"].as_number()), 0, 3);
    if (root["recent"].is_array())
    {
        for (const JsonValue &item : root["recent"].items())
        {
            if (item.is_string() && !item.as_string().empty() && item.as_string().size() < 1024 &&
                static_cast<int>(s.recent.size()) < kMaxRecent)
                s.recent.push_back(item.as_string());
        }
    }
    return s;
}

void Settings::save(const std::string &path) const
{
    std::string out = "{\"openXlOnStart\":";
    out += open_xl_on_start ? "true" : "false";
    out += ",\"sounds\":";
    out += std::to_string(sounds);
    out += ",\"recent\":[";
    for (std::size_t i = 0; i < recent.size(); ++i)
    {
        if (i)
            out += ',';
        append_json_string(out, recent[i]);
    }
    out += "]}\n";
    const std::size_t slash = path.rfind('/');
    if (slash != std::string::npos)
        save::ensure_directory(path.substr(0, slash));
    save::write_atomic(path, out);
}

void Settings::remember(const std::string &url)
{
    recent.erase(std::remove(recent.begin(), recent.end(), url), recent.end());
    recent.insert(recent.begin(), url);
    if (static_cast<int>(recent.size()) > kMaxRecent)
        recent.resize(kMaxRecent);
}

float Settings::sound_volume() const
{
    static const float kLevels[] = {0.0f, 0.35f, 0.7f, 1.0f};
    return kLevels[std::clamp(sounds, 0, 3)];
}

} // namespace hui::kt
