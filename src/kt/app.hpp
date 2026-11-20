// KamTape PS5 - the app's screens: a home screen that opens KamTape XL (the
// TV interface), the full site or any KamTape page in the PS5's web browser,
// a row of recently opened pages, and settings.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/input.hpp"
#include "gfx/draw_list.hpp"
#include "kt/settings.hpp"
#include "ui/fonts.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace hui::kt
{

constexpr const char *kVersion = "0.1.0 beta";
constexpr const char *kSourceUrl = "github.com/Richie133/Kamtape-PS5";

struct Fonts
{
    ui::FontRef regular;
    ui::FontRef bold;
};

class App
{
  public:
    // logo: GL texture of the KamTape wordmark (0 writes the name instead),
    // logo_aspect its width / height.
    App(const Fonts &fonts, std::uint32_t logo, float logo_aspect, std::string settings_path);

    void update(const InputFrame &input, float dt);
    void draw(gfx::DrawList &list) const;
    // Nothing is moving: the intro and the focus have settled.
    bool idle() const;

    enum class Screen
    {
        home,
        settings,
    };
    Screen screen() const { return screen_; }

  private:
    enum class Kind
    {
        xl,
        site,
        go_to,
        settings,
        recent,
    };
    struct Item
    {
        Kind kind = Kind::xl;
        int recent = -1; // index into settings_.recent
        gfx::Rect rect;
    };

    void layout_home();
    int neighbour(int from, Direction direction) const;
    void activate_home(const Item &item);
    void update_settings(const InputFrame &input);
    void open(const std::string &url, const char *what);
    void go_to_page();
    void finish_keyboard(const std::string &text);
    void show_status(std::string text, bool error);
    void sound_move() const;
    void save();

    void draw_background(gfx::DrawList &list) const;
    void draw_header(gfx::DrawList &list) const;
    void draw_home(gfx::DrawList &list) const;
    void draw_settings(gfx::DrawList &list) const;
    void draw_footer(gfx::DrawList &list) const;
    void draw_status(gfx::DrawList &list) const;
    void draw_focus(gfx::DrawList &list, const gfx::Rect &r, float radius) const;
    void draw_tv(gfx::DrawList &list, float x, float y, float scale) const;
    void draw_tile_icon(gfx::DrawList &list, Kind kind, float cx, float cy) const;
    // A controller button: "cross", "circle", "triangle", "square" or a label such as "OPTIONS".
    float draw_button(gfx::DrawList &list, const char *which, float x, float cy) const;
    float draw_hint(gfx::DrawList &list, const char *which, const char *label, float x, float cy) const;

    Fonts fonts_;
    std::uint32_t logo_ = 0;
    float logo_aspect_ = 1.0f;
    std::string settings_path_;
    Settings settings_;

    Screen screen_ = Screen::home;
    std::vector<Item> items_;
    int focus_ = 0;
    int last_tile_ = 1;       // the right-hand tile to come back to from KamTape XL
    int settings_focus_ = 0;  // row on the settings screen
    gfx::Rect focus_rect_;    // drawn focus, chasing the focused item
    bool focus_snapped_ = false;

    float time_ = 0.0f;         // since the app started
    float screen_time_ = 0.0f;  // since the current screen opened
    bool auto_open_done_ = false;
    float last_open_ = -10.0f;  // when the browser was last asked to open
    bool keyboard_ = false;     // the system keyboard is up

    std::string status_;
    bool status_error_ = false;
    float status_time_ = -10.0f;
};

} // namespace hui::kt
