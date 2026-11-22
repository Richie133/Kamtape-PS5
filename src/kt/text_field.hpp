// KamTape PS5 - the text being typed on the on-screen keyboard, with a cursor.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string>

namespace hui::kt
{

// The text being typed and where the cursor is (a byte offset; the keyboard
// only types ASCII, but text handed in may hold UTF-8, which the cursor steps
// over whole).
struct TextField
{
    static constexpr std::size_t kMaxBytes = 200;

    std::string text;
    std::size_t cursor = 0;

    void set(std::string value);
    void insert(const std::string &s);
    void erase_back();    // Backspace
    void erase_forward(); // Delete
    void left();
    void right();
    void home() { cursor = 0; }
    void end() { cursor = text.size(); }
};

} // namespace hui::kt
