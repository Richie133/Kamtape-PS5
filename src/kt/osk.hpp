// KamTape PS5 - the app's own on-screen keyboard: a QWERTY panel driven by the
// controller (and by a USB keyboard), with a text cursor and a row of recent
// entries to pick from. Used instead of the PS5's system keyboard unless
// Settings asks for that one, and whenever the system keyboard can't open.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/input.hpp"
#include "gfx/draw_list.hpp"
#include "kt/hid.hpp"
#include "kt/text_field.hpp"
#include "ui/fonts.hpp"

#include <string>
#include <vector>

namespace hui::kt
{

class OnScreenKeyboard
{
  public:
    enum class Mode
    {
        text,    // a tag: Space on the bottom row
        address, // a KamTape page: "kamtape.com/" on the bottom row instead
    };
    enum class Result
    {
        none,
        done,      // text() is what was typed (or the picked suggestion)
        cancelled, // closed with Circle or Escape
    };

    // suggestions: recent entries shown above the keys; picking one finishes.
    void open(std::string title, std::string text, Mode mode, std::vector<std::string> suggestions);
    bool is_open() const { return open_; }
    const std::string &text() const { return field_.text; }

    Result update(const InputFrame &input, const hid::Frame &usb, float dt);
    void draw(gfx::DrawList &list, const ui::FontRef &regular, const ui::FontRef &bold) const;

  private:
    static constexpr int kRows = 5; // numbers, three letter rows, the bottom row

    int first_key_row() const { return suggestions_.empty() ? 0 : 1; }
    int rows() const { return kRows + first_key_row(); }
    int cols(int row) const;
    gfx::Rect panel() const;
    gfx::Rect key_rect(int row, int col) const;
    std::string key_label(int row, int col) const;
    Result press(int row, int col);
    Result finish(Result how);
    void move_focus(Direction direction);

    bool open_ = false;
    Mode mode_ = Mode::text;
    std::string title_;
    TextField field_;
    std::vector<std::string> suggestions_;
    int row_ = 0;
    int col_ = 0;
    bool caps_ = false;
    bool symbols_ = false;
    float time_ = 0.0f;
    float pressed_at_ = -1.0f; // the focused key flashes when pressed
};

} // namespace hui::kt
