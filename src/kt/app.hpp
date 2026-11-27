// KamTape PS5 - the app's screens: a home screen that opens KamTape XL (the
// TV interface) in the PS5's web browser; featured videos and tag search
// through the KamTape API, with thumbnails and a details page for each video;
// "Go to a page" for any KamTape address; recent pages; settings.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/input.hpp"
#include "gfx/draw_list.hpp"
#include "kt/api.hpp"
#include "kt/api_client.hpp"
#include "kt/hid.hpp"
#include "kt/osk.hpp"
#include "kt/settings.hpp"
#include "kt/thumbs.hpp"
#include "kt/updater.hpp"
#include "ui/fonts.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace hui::kt
{

constexpr const char *kVersion = "0.4.0 beta";
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
    // logo_aspect its width / height. settings_dir holds settings.json (and
    // an optional api.json, see kt/config.hpp). param_path: the running app's
    // sce_sys/param.json, for the update check.
    App(const Fonts &fonts, std::uint32_t logo, float logo_aspect, std::string settings_dir,
        std::string param_path);
    ~App();

    // usb: what a USB keyboard typed this frame (empty when there is none).
    void update(const InputFrame &input, const hid::Frame &usb, float dt);
    // The USB keyboard starts once, guarded: begin_usb() says whether to try
    // (not when it's off in Settings, or when the last try crashed the app);
    // end_usb() after hid::open returns.
    bool begin_usb();
    void end_usb(bool opened);
    void draw(gfx::DrawList &list) const;
    // Nothing is moving or loading: the intro and the focus have settled.
    bool idle() const;

    enum class Screen
    {
        home,
        settings,
        videos,
        details,
    };
    Screen screen() const;

  private:
    enum class Kind
    {
        xl,
        featured,
        search,
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
    enum class ListKind
    {
        featured,
        tag,
        user,
    };
    enum class Keyboard
    {
        none,
        go_to,
        search,
    };
    struct Choice
    {
        enum class Type
        {
            watch,
            author,
            tag,
        } type = Type::watch;
        std::string value;
        gfx::Rect rect;
    };
    // A screen above home: a list of videos or one video's details.
    struct Page
    {
        Screen screen = Screen::videos;
        // the list
        ListKind list = ListKind::featured;
        std::string arg; // the tag or user
        std::string title;
        std::vector<Video> videos;
        int next_page = 1;
        bool more = false;
        int ticket = 0; // the request on its way, 0 for none
        std::string error;     // the list couldn't load (nothing to show)
        float retry_at = 0.0f; // loading more failed: not again before this time
        int focus = 0;
        int top_row = 0;
        float scroll = 0.0f; // drawn top row, chasing top_row
        // the details
        VideoDetails details;
        bool details_loaded = false;
        int action = 0;
        float time = 0.0f; // since it opened
    };

    // home
    void layout_home();
    int neighbour(const std::vector<gfx::Rect> &rects, int from, Direction direction) const;
    void activate_home(const Item &item);
    void open_settings();
    void update_settings(const InputFrame &input);
    // pages
    void push_list(ListKind kind, const std::string &arg);
    void push_details(const Video &video);
    void pop_page();
    void load_more(Page &page);
    void collect(Page &page);
    void update_videos(Page &page, const InputFrame &input);
    void update_details(Page &page, const InputFrame &input);
    float details_shift(const Page &page) const;
    std::vector<Choice> details_choices(const Page &page) const;
    // shared
    void open(const std::string &url, const std::string &what);
    void open_keyboard(Keyboard purpose, const std::string &start = std::string());
    // A USB keyboard drives the screens like a controller when the on-screen keyboard is closed.
    void usb_navigation(const hid::Frame &usb, InputFrame &input);
    void finish_keyboard(const std::string &text);
    void show_status(std::string text, bool error);
    void sound_move() const;
    void save();
    gfx::Rect focus_target() const;
    // updates
    void check_updates(bool asked);
    void watch_updates();
    void update_dialog(const InputFrame &input);
    void draw_update_dialog(gfx::DrawList &list) const;
    std::vector<std::string> dialog_buttons() const;

    // drawing
    void draw_background(gfx::DrawList &list) const;
    void draw_header(gfx::DrawList &list) const;
    void draw_home(gfx::DrawList &list) const;
    void draw_settings(gfx::DrawList &list) const;
    void draw_videos(gfx::DrawList &list, const Page &page) const;
    void draw_details(gfx::DrawList &list, const Page &page) const;
    void draw_footer(gfx::DrawList &list) const;
    void draw_status(gfx::DrawList &list) const;
    void draw_focus(gfx::DrawList &list, const gfx::Rect &r, float radius) const;
    void draw_tv(gfx::DrawList &list, float x, float y, float scale) const;
    void draw_tile_icon(gfx::DrawList &list, Kind kind, float cx, float cy) const;
    void draw_thumbnail(gfx::DrawList &list, const Video &video, const gfx::Rect &r, float radius) const;
    void draw_spinner(gfx::DrawList &list, float cx, float cy, float radius) const;
    void draw_stars(gfx::DrawList &list, double rating, float x, float cy, float size) const;
    float draw_chip(gfx::DrawList &list, const gfx::Rect &r, const std::string &label, bool primary) const;
    // A controller button: "cross", "circle", "triangle", "square" or a label such as "OPTIONS".
    float draw_button(gfx::DrawList &list, const char *which, float x, float cy) const;
    float draw_hint(gfx::DrawList &list, const char *which, const char *label, float x, float cy) const;

    Fonts fonts_;
    std::uint32_t logo_ = 0;
    float logo_aspect_ = 1.0f;
    std::string settings_dir_;
    Settings settings_;
    std::unique_ptr<Thumbs> thumbs_;
    std::unique_ptr<ApiClient> api_;
    std::unique_ptr<Updater> updater_;

    Screen screen_ = Screen::home; // home or settings, under any pages
    std::vector<Page> pages_;
    std::vector<Item> items_;
    int focus_ = 0;
    int last_tile_ = 1;      // the right-hand tile to come back to from KamTape XL
    int settings_focus_ = 0; // row on the settings screen
    gfx::Rect focus_rect_;   // drawn focus, chasing the focused item
    bool focus_snapped_ = false;

    float time_ = 0.0f;        // since the app started
    float screen_time_ = 0.0f; // since home or settings opened
    bool auto_open_done_ = false;
    float last_open_ = -10.0f; // when the browser was last asked to open
    Keyboard keyboard_ = Keyboard::none; // what's being typed for, by either keyboard
    OnScreenKeyboard osk_;

    // updates: the dialog, what it last showed, and when the app should close after applying
    bool dialog_ = false;
    bool dialog_dismissed_ = false; // "Later": not again this launch
    bool check_asked_ = false;      // the user asked: say "up to date" too
    bool update_checked_ = false;   // the check at start-up is done
    int dialog_button_ = 0;
    float dialog_time_ = 0.0f;
    Updater::State update_state_ = Updater::State::idle;
    Release update_release_;
    self_update_status install_{};
    float quit_at_ = -1.0f;

    std::string status_;
    bool status_error_ = false;
    float status_time_ = -10.0f;
};

} // namespace hui::kt
