// KamTape PS5 - the app's own on-screen keyboard.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Laid out like Flashpoint PS5's keyboard (numbers, three QWERTY rows, then
// Caps, symbols, Space, Delete, Done), with a text cursor, recent entries
// above the keys and KamTape's colours.

#include "kt/osk.hpp"

#include "kt/audio.hpp"
#include "kt/style.hpp"

#include <algorithm>
#include <cmath>

namespace hui::kt
{

namespace
{

using gfx::Align;
using gfx::Color;
using gfx::Rect;
using namespace style;

const char *const kLetters[3] = {"qwertyuiop", "asdfghjkl-", "zxcvbnm./?"};
const char *const kSymbols[3] = {"!@#$%^&*()", "_=+[]{};:'", "\",<>\\|~`&="};
// the bottom row: Caps, symbols, Space, Delete, Done, in tenths of the width
const float kBottomStart[5] = {0.0f, 1.5f, 3.0f, 6.0f, 8.0f};
const float kBottomWidth[5] = {1.5f, 1.5f, 3.0f, 2.0f, 2.0f};
constexpr int kMaxSuggestions = 5;
constexpr float kKeyH = 72.0f;
constexpr float kRowStep = 82.0f;

// a controller button drawn small, for the hints along the bottom
float hint(gfx::DrawList &list, const ui::FontRef &regular, const ui::FontRef &bold, const char *which,
           const char *label, float x, float cy)
{
    const std::string w = which;
    float used = 0.0f;
    if (w == "cross" || w == "circle" || w == "triangle" || w == "square")
    {
        const float c = x + 14.0f;
        list.circle(c, cy, 14.0f, kPanelHi);
        if (w == "cross")
        {
            list.line(c - 5, cy - 5, c + 5, cy + 5, 2.4f, kText);
            list.line(c - 5, cy + 5, c + 5, cy - 5, 2.4f, kText);
        }
        else if (w == "circle")
        {
            list.ring(c, cy, 6.0f, 2.4f, kText);
        }
        else if (w == "triangle")
        {
            list.line(c, cy - 6, c - 6, cy + 4.5f, 2.4f, kText);
            list.line(c - 6, cy + 4.5f, c + 6, cy + 4.5f, 2.4f, kText);
            list.line(c + 6, cy + 4.5f, c, cy - 6, 2.4f, kText);
        }
        else
        {
            list.bordered_rect({c - 5, cy - 5, 10, 10}, 1, kClear, 2.4f, kText);
        }
        used = 28.0f;
    }
    else
    {
        const float tw = bold.measure(which, 14.0f, 1.0f);
        list.bordered_rect({x, cy - 13, tw + 16, 26}, 7, kPanelHi, 1.0f, kLine);
        ui::text(list, bold, which, x + 8, cy + 5, 14.0f, kText, Align::left, 1.0f);
        used = tw + 16;
    }
    x += used + 8.0f;
    ui::text(list, regular, label, x, cy + 7, 21.0f, kMuted);
    return x + regular.measure(label, 21.0f) + 26.0f;
}

} // namespace

// ---- the keyboard ---------------------------------------------------------------------

void OnScreenKeyboard::open(std::string title, std::string text, Mode mode, std::vector<std::string> suggestions)
{
    open_ = true;
    mode_ = mode;
    title_ = std::move(title);
    field_.set(std::move(text));
    suggestions_ = std::move(suggestions);
    if (suggestions_.size() > kMaxSuggestions)
        suggestions_.resize(kMaxSuggestions);
    row_ = first_key_row() + 1; // on "q"
    col_ = 0;
    caps_ = false;
    symbols_ = false;
    time_ = 0.0f;
    pressed_at_ = -1.0f;
}

int OnScreenKeyboard::cols(int row) const
{
    if (row < first_key_row())
        return static_cast<int>(suggestions_.size());
    return row - first_key_row() == kRows - 1 ? 5 : 10;
}

Rect OnScreenKeyboard::panel() const
{
    const float h = suggestions_.empty() ? 700.0f : 790.0f;
    return {320.0f, 170.0f, 1280.0f, h};
}

Rect OnScreenKeyboard::key_rect(int row, int col) const
{
    const Rect p = panel();
    const float left = p.x + 48.0f;
    const float width = p.w - 96.0f;
    if (row < first_key_row())
    {
        const int n = static_cast<int>(suggestions_.size());
        const float w = std::min(300.0f, (width - static_cast<float>(n - 1) * 12.0f) / static_cast<float>(n));
        return {left + static_cast<float>(col) * (w + 12.0f), p.y + 230.0f, w, 52.0f};
    }
    const int k = row - first_key_row();
    const float y = p.y + (suggestions_.empty() ? 206.0f : 306.0f) + static_cast<float>(k) * kRowStep;
    const float unit = width / 10.0f;
    if (k == kRows - 1)
        return {left + kBottomStart[col] * unit, y, kBottomWidth[col] * unit - 10.0f, kKeyH};
    return {left + static_cast<float>(col) * unit, y, unit - 10.0f, kKeyH};
}

std::string OnScreenKeyboard::key_label(int row, int col) const
{
    if (row < first_key_row())
        return suggestions_[static_cast<std::size_t>(col)];
    const int k = row - first_key_row();
    if (k == 0)
        return std::string(1, "1234567890"[col]);
    if (k == kRows - 1)
    {
        const char *const names[5] = {"Caps", symbols_ ? "abc" : "#+=",
                                      mode_ == Mode::address ? "kamtape.com/" : "Space", "Delete", "Done"};
        return names[col];
    }
    char c = (symbols_ ? kSymbols : kLetters)[k - 1][col];
    if (caps_ && c >= 'a' && c <= 'z')
        c = static_cast<char>(c - 'a' + 'A');
    return std::string(1, c);
}

OnScreenKeyboard::Result OnScreenKeyboard::finish(Result how)
{
    open_ = false;
    audio::play(how == Result::done ? audio::Sfx::select : audio::Sfx::back);
    return how;
}

OnScreenKeyboard::Result OnScreenKeyboard::press(int row, int col)
{
    pressed_at_ = time_;
    if (row < first_key_row())
    {
        field_.set(suggestions_[static_cast<std::size_t>(col)]);
        return finish(Result::done);
    }
    const int k = row - first_key_row();
    if (k == kRows - 1)
    {
        switch (col)
        {
        case 0:
            caps_ = !caps_;
            break;
        case 1:
            symbols_ = !symbols_;
            break;
        case 2:
            field_.insert(mode_ == Mode::address ? "kamtape.com/" : " ");
            break;
        case 3:
            field_.erase_back();
            break;
        default:
            return finish(Result::done);
        }
        audio::play(audio::Sfx::move);
        return Result::none;
    }
    field_.insert(key_label(row, col));
    audio::play(audio::Sfx::move);
    return Result::none;
}

void OnScreenKeyboard::move_focus(Direction direction)
{
    if (direction == Direction::left || direction == Direction::right)
    {
        const int n = cols(row_);
        col_ = (col_ + (direction == Direction::left ? n - 1 : 1)) % n;
    }
    else if (direction == Direction::up || direction == Direction::down)
    {
        // to the key in the next row that sits nearest over (or under) this one
        const int to = (row_ + (direction == Direction::up ? rows() - 1 : 1)) % rows();
        const float cx = key_rect(row_, col_).cx();
        int best = 0;
        for (int col = 1; col < cols(to); ++col)
        {
            if (std::fabs(key_rect(to, col).cx() - cx) < std::fabs(key_rect(to, best).cx() - cx))
                best = col;
        }
        row_ = to;
        col_ = best;
    }
    else
    {
        return;
    }
    audio::play(audio::Sfx::move);
}

OnScreenKeyboard::Result OnScreenKeyboard::update(const InputFrame &input, const hid::Frame &usb, float dt)
{
    if (!open_)
        return Result::none;
    time_ += dt;

    // a USB keyboard types straight into the field, keys and characters in the order they came
    const bool ordered = usb.typed.size() == usb.pressed.size();
    if (!ordered)
    {
        for (char c : usb.text)
        {
            if (c >= 32 && c < 127)
                field_.insert(std::string(1, c));
        }
    }
    for (std::size_t i = 0; i < usb.pressed.size(); ++i)
    {
        const char c = ordered ? usb.typed[i] : 0;
        if (c >= 32 && c < 127)
        {
            field_.insert(std::string(1, c));
            continue;
        }
        switch (usb.pressed[i])
        {
        case hid::key::kEnter:
            return finish(Result::done);
        case hid::key::kEscape:
            return finish(Result::cancelled);
        case hid::key::kBackspace:
            field_.erase_back();
            break;
        case hid::key::kDelete:
            field_.erase_forward();
            break;
        case hid::key::kLeft:
            field_.left();
            break;
        case hid::key::kRight:
            field_.right();
            break;
        case hid::key::kHome:
            field_.home();
            break;
        case hid::key::kEnd:
            field_.end();
            break;
        default:
            break;
        }
    }

    // the controller
    move_focus(input.nav);
    if (input.is_pressed(Action::confirm))
        return press(row_, col_);
    if (input.is_pressed(Action::back))
        return finish(Result::cancelled);
    if (input.is_pressed(Action::menu) || input.is_pressed(Action::jump_next))
        return finish(Result::done);
    if (input.is_pressed(Action::west))
    {
        field_.erase_back();
        audio::play(audio::Sfx::move);
    }
    else if (input.is_pressed(Action::north))
    {
        field_.insert(mode_ == Mode::address ? "/" : " ");
        audio::play(audio::Sfx::move);
    }
    else if (input.is_pressed(Action::page_prev))
    {
        field_.left();
    }
    else if (input.is_pressed(Action::page_next))
    {
        field_.right();
    }
    else if (input.is_pressed(Action::jump_prev))
    {
        field_.home();
    }
    else if (input.is_pressed(Action::l3))
    {
        caps_ = !caps_;
        audio::play(audio::Sfx::move);
    }
    return Result::none;
}

void OnScreenKeyboard::draw(gfx::DrawList &list, const ui::FontRef &regular, const ui::FontRef &bold) const
{
    if (!open_)
        return;
    const float t = std::min(time_ / 0.18f, 1.0f);
    list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0, Color{0, 0, 0, 0.6f * t});
    list.push_opacity(t);
    list.push_transform(1.0f, 0, 0, 0, (1.0f - t) * 24.0f);

    const Rect p = panel();
    list.shadow(p, 30, 48, Color{0, 0, 0, 0.6f});
    list.rounded_rect(p, 30, kPanel);
    list.bordered_rect(p, 30, kClear, 1.5f, kLine);
    ui::text(list, bold, title_, p.x + 48, p.y + 62, 34, kText);
    ui::text(list, regular, mode_ == Mode::address ? "Only KamTape pages" : "Letters, numbers, spaces",
             p.x + p.w - 48, p.y + 60, 22, kDim, Align::right);

    // the field, scrolled so the cursor stays in view
    const Rect field{p.x + 48, p.y + 92, p.w - 96, 84};
    list.rounded_rect(field, 18, Color::rgb(0x0f1116));
    list.bordered_rect(field, 18, kClear, 2.5f, kAccent);
    const float room = field.w - 56;
    const float to_cursor = regular.measure(field_.text.substr(0, field_.cursor), 32);
    const float offset = std::max(0.0f, to_cursor - room);
    list.push_clip(field.inset(4));
    const float tx = field.x + 28 - offset;
    if (field_.text.empty())
        ui::text(list, regular, mode_ == Mode::address ? "kamtape.com/..." : "Type a tag", tx, field.cy() + 11, 32,
                 kDim);
    else
        ui::text(list, regular, field_.text, tx, field.cy() + 11, 32, kText);
    if (std::fmod(time_, 1.0f) < 0.6f)
        list.rounded_rect({tx + to_cursor + 1, field.y + 20, 3, field.h - 40}, 1.5f, kAccent);
    list.pop_clip();

    if (!suggestions_.empty())
        ui::text(list, bold, "RECENT", p.x + 48, p.y + 218, 16, kDim, Align::left, 3);

    const float flash = pressed_at_ >= 0.0f ? std::max(0.0f, 1.0f - (time_ - pressed_at_) / 0.15f) : 0.0f;
    for (int row = 0; row < rows(); ++row)
    {
        for (int col = 0; col < cols(row); ++col)
        {
            const Rect k = key_rect(row, col);
            const bool focused = row == row_ && col == col_;
            const bool suggestion = row < first_key_row();
            const int kr = row - first_key_row();
            const bool bottom = kr == kRows - 1;
            const bool lit = bottom && ((col == 0 && caps_) || (col == 1 && symbols_));
            const bool done = bottom && col == 4;
            const float radius = suggestion ? k.h / 2 : 12.0f;
            if (focused)
            {
                list.glow(k, radius, 14.0f, kAccent.with_alpha(0.35f));
                list.rounded_rect(k, radius, kAccent);
                if (flash > 0.0f)
                    list.rounded_rect(k, radius, Color{1, 1, 1, 0.35f * flash});
            }
            else
            {
                list.rounded_rect(k, radius, done ? Color::rgb(0x1f3a1a) : kPanelHi);
                list.bordered_rect(k, radius, kClear, lit ? 2.5f : 1.0f, lit || done ? kAccent : kLine);
            }
            std::string label = key_label(row, col);
            const bool word = suggestion || bottom;
            const float size = word ? 22.0f : 30.0f;
            const ui::FontRef &font = suggestion ? regular : bold;
            label = font.font->fit(label, size, k.w - 24);
            const Color colour = focused ? kOnAccent : (done ? kAccentText : kText);
            ui::text(list, font, label, k.cx(), k.cy() + size * 0.36f, size, colour, Align::center);
        }
    }

    // what the buttons do
    float x = p.x + 48;
    const float cy = p.y + p.h - 42;
    x = hint(list, regular, bold, "cross", "Type", x, cy);
    x = hint(list, regular, bold, "square", "Delete", x, cy);
    x = hint(list, regular, bold, "triangle", mode_ == Mode::address ? "/" : "Space", x, cy);
    x = hint(list, regular, bold, "L1", "", x, cy) - 26.0f;
    x = hint(list, regular, bold, "R1", "Cursor", x, cy);
    x = hint(list, regular, bold, "OPTIONS", "Done", x, cy);
    hint(list, regular, bold, "circle", "Cancel", x, cy);

    list.pop_transform();
    list.pop_opacity();
}

} // namespace hui::kt
