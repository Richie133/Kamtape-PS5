// KamTape PS5 - the text being typed on the on-screen keyboard, with a cursor.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/text_field.hpp"

#include <utility>

namespace hui::kt
{

namespace
{

bool is_continuation(unsigned char c)
{
    return (c & 0xc0) == 0x80;
}

} // namespace

void TextField::set(std::string value)
{
    if (value.size() > kMaxBytes)
        value.resize(kMaxBytes);
    text = std::move(value);
    cursor = text.size();
}

void TextField::insert(const std::string &s)
{
    if (text.size() + s.size() > kMaxBytes)
        return;
    text.insert(cursor, s);
    cursor += s.size();
}

void TextField::erase_back()
{
    if (cursor == 0)
        return;
    std::size_t from = cursor - 1;
    while (from > 0 && is_continuation(static_cast<unsigned char>(text[from])))
        --from;
    text.erase(from, cursor - from);
    cursor = from;
}

void TextField::erase_forward()
{
    if (cursor >= text.size())
        return;
    std::size_t to = cursor + 1;
    while (to < text.size() && is_continuation(static_cast<unsigned char>(text[to])))
        ++to;
    text.erase(cursor, to - cursor);
}

void TextField::left()
{
    while (cursor > 0)
    {
        --cursor;
        if (!is_continuation(static_cast<unsigned char>(text[cursor])))
            break;
    }
}

void TextField::right()
{
    while (cursor < text.size())
    {
        ++cursor;
        if (cursor == text.size() || !is_continuation(static_cast<unsigned char>(text[cursor])))
            break;
    }
}

} // namespace hui::kt
