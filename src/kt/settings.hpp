// KamTape PS5 - the app's settings, kept in the title's own storage.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

namespace hui::kt
{

struct Settings
{
    static constexpr int kMaxRecent = 5;

    bool open_xl_on_start = true;    // straight to KamTape XL when the app opens
    int sounds = 2;                  // interface sounds: 0 off, 1 low, 2 medium, 3 high
    bool system_keyboard = false;    // the PS5's keyboard instead of the app's own
    bool usb_input = true;           // read a USB keyboard plugged into the PS5
    bool usb_trying = false;         // set while the USB keyboard starts: still set means it crashed
    std::vector<std::string> recent;   // pages opened with "Go to a page", newest first
    std::vector<std::string> searches; // tags searched for, newest first

    static Settings load(const std::string &path);
    void save(const std::string &path) const;

    // Puts url first, without duplicates, keeping at most kMaxRecent.
    void remember(const std::string &url);
    void remember_search(const std::string &tag);
    float sound_volume() const;
};

} // namespace hui::kt
