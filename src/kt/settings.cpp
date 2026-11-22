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
    if (root["keyboard"].is_string())
        s.system_keyboard = root["keyboard"].as_string() == "system";
    if (root["usbInput"].is_bool())
        s.usb_input = root["usbInput"].as_bool();
    if (root["usbTrying"].is_bool())
        s.usb_trying = root["usbTrying"].as_bool();
    const auto read_list = [&](const char *key, std::vector<std::string> *out) {
        if (!root[key].is_array())
            return;
        for (const JsonValue &item : root[key].items())
        {
            if (item.is_string() && !item.as_string().empty() && item.as_string().size() < 1024 &&
                static_cast<int>(out->size()) < kMaxRecent)
                out->push_back(item.as_string());
        }
    };
    read_list("recent", &s.recent);
    read_list("searches", &s.searches);
    return s;
}

void Settings::save(const std::string &path) const
{
    std::string out = "{\"openXlOnStart\":";
    out += open_xl_on_start ? "true" : "false";
    out += ",\"sounds\":";
    out += std::to_string(sounds);
    out += ",\"keyboard\":";
    out += system_keyboard ? "\"system\"" : "\"kamtape\"";
    out += ",\"usbInput\":";
    out += usb_input ? "true" : "false";
    out += ",\"usbTrying\":";
    out += usb_trying ? "true" : "false";
    const auto write_list = [&](const char *key, const std::vector<std::string> &list) {
        out += ",\"";
        out += key;
        out += "\":[";
        for (std::size_t i = 0; i < list.size(); ++i)
        {
            if (i)
                out += ',';
            append_json_string(out, list[i]);
        }
        out += ']';
    };
    write_list("recent", recent);
    write_list("searches", searches);
    out += "}\n";
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

void Settings::remember_search(const std::string &tag)
{
    searches.erase(std::remove(searches.begin(), searches.end(), tag), searches.end());
    searches.insert(searches.begin(), tag);
    if (static_cast<int>(searches.size()) > kMaxRecent)
        searches.resize(kMaxRecent);
}

float Settings::sound_volume() const
{
    static const float kLevels[] = {0.0f, 0.35f, 0.7f, 1.0f};
    return kLevels[std::clamp(sounds, 0, 3)];
}

} // namespace hui::kt
