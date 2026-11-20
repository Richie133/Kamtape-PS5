// KamTape PS5 - the app's screens.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/app.hpp"

#include "core/tween.hpp"
#include "kt/address.hpp"
#include "kt/audio.hpp"
#include "kt/services.hpp"
#include "platform/ps5/system.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace hui::kt
{

namespace
{

using gfx::Align;
using gfx::Color;
using gfx::Rect;

const Color kBgTop = Color::rgb(0x1c1f29);
const Color kBgBottom = Color::rgb(0x0b0c10);
const Color kPanel = Color::rgb(0x191c24);
const Color kPanelHi = Color::rgb(0x232733);
const Color kLine = Color::rgb(0x2e3341);
const Color kBadge = Color::rgb(0x272b37);
const Color kText = Color::rgb(0xf4efe6);
const Color kMuted = Color::rgb(0xa6aab6);
const Color kDim = Color::rgb(0x6f7383);
const Color kAccent = Color::rgb(0x6fdc3a); // KamTape green
const Color kOnAccent = Color::rgb(0x0f2108);
const Color kAmber = Color::rgb(0xffb547);
const Color kCream = Color::rgb(0xf1e8d6);
const Color kCreamShade = Color::rgb(0xd6c8ae);
const Color kHeroTop = Color::rgb(0x1e3a18);
const Color kHeroBottom = Color::rgb(0x0f1a10);
const Color kHeroLine = Color::rgb(0x31592a);
const Color kClear{0.0f, 0.0f, 0.0f, 0.0f};

constexpr Rect kHero{96.0f, 220.0f, 1040.0f, 520.0f};
constexpr float kTileX = 1176.0f;
constexpr float kTileW = 648.0f;
constexpr float kTileH = 160.0f;
constexpr float kTileGap = 20.0f;
constexpr float kRecentY = 822.0f;
constexpr float kRecentH = 60.0f;
constexpr float kRecentMaxText = 360.0f;
constexpr float kRight = 1824.0f;

constexpr Rect kSettingsPanel{336.0f, 206.0f, 1248.0f, 680.0f};
constexpr int kSettingsRows = 3;

Rect settings_row(int i)
{
    return {kSettingsPanel.x + 40.0f, kSettingsPanel.y + 112.0f + static_cast<float>(i) * 96.0f,
            kSettingsPanel.w - 80.0f, 80.0f};
}

const char *const kSoundNames[] = {"Off", "Low", "Medium", "High"};

bool is_shape(const char *which)
{
    const std::string w = which;
    return w == "cross" || w == "circle" || w == "triangle" || w == "square";
}

// Distance between [a0, a1] and [b0, b1]; 0 when they overlap.
float gap(float a0, float a1, float b0, float b1)
{
    if (b1 < a0)
        return a0 - b1;
    if (b0 > a1)
        return b0 - a1;
    return 0.0f;
}

} // namespace

App::App(const Fonts &fonts, std::uint32_t logo, float logo_aspect, std::string settings_path)
    : fonts_(fonts), logo_(logo), logo_aspect_(logo_aspect), settings_path_(std::move(settings_path))
{
    settings_ = Settings::load(settings_path_);
    audio::set_volume(0.0f, settings_.sound_volume());
    layout_home();
}

void App::save()
{
    settings_.save(settings_path_);
}

void App::sound_move() const
{
    audio::play(audio::Sfx::move);
}

void App::show_status(std::string text, bool error)
{
    status_ = std::move(text);
    status_error_ = error;
    status_time_ = time_;
}

// ---- home -------------------------------------------------------------------

void App::layout_home()
{
    items_.clear();
    items_.push_back({Kind::xl, -1, kHero});
    const Kind tiles[] = {Kind::site, Kind::go_to, Kind::settings};
    for (int i = 0; i < 3; ++i)
        items_.push_back({tiles[i], -1, {kTileX, kHero.y + static_cast<float>(i) * (kTileH + kTileGap), kTileW, kTileH}});
    float x = kHero.x;
    for (int i = 0; i < static_cast<int>(settings_.recent.size()); ++i)
    {
        const std::string label = fonts_.regular.font->fit(short_address(settings_.recent[i]), 24.0f, kRecentMaxText);
        const float w = fonts_.regular.measure(label, 24.0f) + 56.0f;
        if (x + w > kRight)
            break;
        items_.push_back({Kind::recent, i, {x, kRecentY, w, kRecentH}});
        x += w + 16.0f;
    }
    focus_ = std::clamp(focus_, 0, static_cast<int>(items_.size()) - 1);
}

int App::neighbour(int from, Direction direction) const
{
    if (direction == Direction::none)
        return -1;
    // From KamTape XL to the right column: back to the tile last used there.
    if (items_[from].kind == Kind::xl && direction == Direction::right)
        return last_tile_;
    const Rect &a = items_[from].rect;
    const bool horizontal = direction == Direction::left || direction == Direction::right;
    int best = -1;
    float best_score = 1e30f;
    for (int i = 0; i < static_cast<int>(items_.size()); ++i)
    {
        if (i == from)
            continue;
        const Rect &b = items_[i].rect;
        float primary = 0.0f;
        switch (direction)
        {
        case Direction::right:
            primary = b.x - (a.x + a.w);
            break;
        case Direction::left:
            primary = a.x - (b.x + b.w);
            break;
        case Direction::down:
            primary = b.y - (a.y + a.h);
            break;
        case Direction::up:
            primary = a.y - (b.y + b.h);
            break;
        case Direction::none:
            break;
        }
        if (primary < -1.0f)
            continue; // not on that side
        const float across = horizontal ? gap(a.y, a.y + a.h, b.y, b.y + b.h) : gap(a.x, a.x + a.w, b.x, b.x + b.w);
        const float centre = horizontal ? std::fabs(b.cy() - a.cy()) : std::fabs(b.cx() - a.cx());
        const float score = std::max(primary, 0.0f) + across * 3.0f + centre * 0.01f;
        if (score < best_score)
        {
            best_score = score;
            best = i;
        }
    }
    return best;
}

void App::open(const std::string &url, const char *what)
{
    if (time_ - last_open_ < 1.0f)
        return; // one press, one browser
    last_open_ = time_;
    if (services::open_browser(url))
    {
        audio::play(audio::Sfx::open);
        show_status(std::string("Opening ") + what + " in the PS5 browser...", false);
    }
    else
    {
        audio::play(audio::Sfx::error);
        show_status("The PS5 browser didn't open. Try again in a moment.", true);
    }
}

void App::go_to_page()
{
    if (services::keyboard_open("Go to a KamTape page", "kamtape.com/"))
    {
        keyboard_ = true;
        audio::play(audio::Sfx::select);
    }
    else
    {
        audio::play(audio::Sfx::error);
        show_status("The system keyboard didn't open.", true);
    }
}

void App::finish_keyboard(const std::string &text)
{
    const Address address = normalize_address(text);
    switch (address.error)
    {
    case AddressError::empty:
        return;
    case AddressError::other_site:
        audio::play(audio::Sfx::error);
        show_status("Only KamTape pages open here.", true);
        return;
    case AddressError::spaces:
        audio::play(audio::Sfx::error);
        show_status("Type an address, like kamtape.com/xl/", true);
        return;
    case AddressError::none:
        break;
    }
    settings_.remember(address.url);
    save();
    layout_home();
    const std::string label = short_address(address.url);
    open(address.url, fonts_.regular.font->fit(label, 24.0f, 700.0f).c_str());
}

void App::activate_home(const Item &item)
{
    switch (item.kind)
    {
    case Kind::xl:
        open(kXlUrl, "KamTape XL");
        break;
    case Kind::site:
        open(kSiteUrl, "KamTape");
        break;
    case Kind::go_to:
        go_to_page();
        break;
    case Kind::settings:
        audio::play(audio::Sfx::select);
        screen_ = Screen::settings;
        settings_focus_ = 0;
        screen_time_ = 0.0f;
        focus_snapped_ = false;
        break;
    case Kind::recent:
    {
        if (item.recent < 0 || item.recent >= static_cast<int>(settings_.recent.size()))
            break;
        const std::string url = settings_.recent[item.recent];
        settings_.remember(url);
        save();
        layout_home();
        focus_ = 4; // the page just opened moves to the front of the row
        open(url, fonts_.regular.font->fit(short_address(url), 24.0f, 700.0f).c_str());
        break;
    }
    }
}

// ---- settings -----------------------------------------------------------------

void App::update_settings(const InputFrame &input)
{
    if (input.is_pressed(Action::back) || input.is_pressed(Action::menu))
    {
        audio::play(audio::Sfx::back);
        screen_ = Screen::home;
        screen_time_ = 0.0f;
        focus_snapped_ = false;
        return;
    }
    if (input.nav == Direction::up || input.nav == Direction::down)
    {
        const int next = std::clamp(settings_focus_ + (input.nav == Direction::down ? 1 : -1), 0, kSettingsRows - 1);
        if (next != settings_focus_)
        {
            settings_focus_ = next;
            sound_move();
        }
        return;
    }
    const bool confirm = input.is_pressed(Action::confirm);
    const bool left = input.nav == Direction::left;
    const bool right = input.nav == Direction::right;
    switch (settings_focus_)
    {
    case 0:
        if (confirm || left || right)
        {
            settings_.open_xl_on_start = !settings_.open_xl_on_start;
            save();
            audio::play(audio::Sfx::select);
        }
        break;
    case 1:
    {
        int level = settings_.sounds;
        if (left)
            level = std::max(level - 1, 0);
        else if (right)
            level = std::min(level + 1, 3);
        else if (confirm)
            level = (level + 1) % 4;
        if (level != settings_.sounds)
        {
            settings_.sounds = level;
            audio::set_volume(0.0f, settings_.sound_volume());
            save();
            audio::play(audio::Sfx::select); // at the new level
        }
        break;
    }
    case 2:
        if (confirm)
        {
            if (settings_.recent.empty())
            {
                audio::play(audio::Sfx::error);
                show_status("There are no recent pages to clear.", false);
            }
            else
            {
                settings_.recent.clear();
                save();
                layout_home();
                audio::play(audio::Sfx::select);
                show_status("Recent pages cleared.", false);
            }
        }
        break;
    default:
        break;
    }
}

// ---- frame --------------------------------------------------------------------

void App::update(const InputFrame &input, float dt)
{
    time_ += dt;
    screen_time_ += dt;

    if (keyboard_)
    {
        std::string text;
        switch (services::keyboard_poll(&text))
        {
        case services::KeyboardState::open:
            return; // the keyboard has the controller
        case services::KeyboardState::done:
            keyboard_ = false;
            finish_keyboard(text);
            break;
        case services::KeyboardState::cancelled:
        case services::KeyboardState::idle:
            keyboard_ = false;
            break;
        }
    }
    else if (!auto_open_done_ && time_ >= 1.2f)
    {
        auto_open_done_ = true;
        if (settings_.open_xl_on_start)
            open(kXlUrl, "KamTape XL");
    }

    if (screen_ == Screen::settings)
    {
        if (!keyboard_)
            update_settings(input);
    }
    else if (!keyboard_)
    {
        if (input.nav != Direction::none)
        {
            const int next = neighbour(focus_, input.nav);
            if (next >= 0 && next != focus_)
            {
                focus_ = next;
                const Kind kind = items_[next].kind;
                if (kind == Kind::site || kind == Kind::go_to || kind == Kind::settings)
                    last_tile_ = next;
                sound_move();
            }
        }
        if (input.is_pressed(Action::confirm))
            activate_home(items_[focus_]);
        else if (input.is_pressed(Action::north))
            go_to_page();
        else if (input.is_pressed(Action::menu))
            activate_home({Kind::settings, -1, {}});
    }

    // the focus ring glides to its target
    const Rect target = screen_ == Screen::home ? items_[focus_].rect : settings_row(settings_focus_);
    if (!focus_snapped_)
    {
        focus_rect_ = target;
        focus_snapped_ = true;
    }
    else
    {
        const float k = 1.0f - std::exp(-18.0f * dt);
        focus_rect_.x += (target.x - focus_rect_.x) * k;
        focus_rect_.y += (target.y - focus_rect_.y) * k;
        focus_rect_.w += (target.w - focus_rect_.w) * k;
        focus_rect_.h += (target.h - focus_rect_.h) * k;
    }
}

bool App::idle() const
{
    const Rect target = screen_ == Screen::home ? items_[focus_].rect : settings_row(settings_focus_);
    const bool settled = std::fabs(target.x - focus_rect_.x) < 0.5f && std::fabs(target.y - focus_rect_.y) < 0.5f &&
                         std::fabs(target.w - focus_rect_.w) < 0.5f;
    return time_ > 1.6f && screen_time_ > 0.9f && settled;
}

// ---- drawing ------------------------------------------------------------------

float App::draw_button(gfx::DrawList &list, const char *which, float x, float cy) const
{
    if (is_shape(which))
    {
        const float s = 32.0f;
        const float c = x + s / 2.0f;
        const std::string w = which;
        list.circle(c, cy, s / 2.0f, kPanelHi);
        if (w == "cross")
        {
            list.line(c - 6, cy - 6, c + 6, cy + 6, 2.6f, kText);
            list.line(c - 6, cy + 6, c + 6, cy - 6, 2.6f, kText);
        }
        else if (w == "circle")
        {
            list.ring(c, cy, 7.0f, 2.6f, kText);
        }
        else if (w == "triangle")
        {
            list.line(c, cy - 7, c - 7, cy + 5.5f, 2.6f, kText);
            list.line(c - 7, cy + 5.5f, c + 7, cy + 5.5f, 2.6f, kText);
            list.line(c + 7, cy + 5.5f, c, cy - 7, 2.6f, kText);
        }
        else
        {
            list.bordered_rect({c - 6, cy - 6, 12, 12}, 1, kClear, 2.6f, kText);
        }
        return s;
    }
    const float tw = fonts_.bold.measure(which, 16.0f, 1.0f);
    list.bordered_rect({x, cy - 15, tw + 20, 30}, 8, kPanelHi, 1.0f, kLine);
    ui::text(list, fonts_.bold, which, x + 10, cy + 6, 16.0f, kText, Align::left, 1.0f);
    return tw + 20;
}

float App::draw_hint(gfx::DrawList &list, const char *which, const char *label, float x, float cy) const
{
    x += draw_button(list, which, x, cy) + 10.0f;
    ui::text(list, fonts_.regular, label, x, cy + 8, 24.0f, kMuted);
    return x + fonts_.regular.measure(label, 24.0f) + 34.0f;
}

void App::draw_background(gfx::DrawList &list) const
{
    list.gradient_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0, kBgTop, kBgBottom);
    // a warm light behind the header and a cool one at the bottom right
    list.glow({-200, -260, 900, 520}, 260, 380, kAccent.with_alpha(0.10f));
    list.glow({1500, 900, 600, 400}, 200, 360, Color::rgb(0x3a6bff, 0.06f));
    // faint scanlines, like an old set
    for (float y = 0; y < gfx::kVirtualHeight; y += 6.0f)
        list.rounded_rect({0, y, gfx::kVirtualWidth, 2}, 0, Color{0, 0, 0, 0.10f});
}

void App::draw_header(gfx::DrawList &list) const
{
    const float t = tween::cubic_out(time_ / 0.6f);
    list.push_opacity(t);
    list.push_transform(1.0f, 0, 0, 0, (1.0f - t) * -16.0f);
    // the KamTape logo on its own white card
    const Rect card{96, 52, 340, 128};
    list.shadow(card, 26, 22, Color{0, 0, 0, 0.5f});
    list.rounded_rect(card, 26, Color::rgb(0xffffff));
    if (logo_ != 0)
    {
        const float h = 100.0f;
        const float w = h * logo_aspect_;
        list.image(logo_, {card.cx() - w / 2, card.cy() - h / 2, w, h}, gfx::kFullUv, Color{1, 1, 1, 1});
    }
    else
    {
        ui::text(list, fonts_.bold, "KamTape", card.cx(), card.cy() + 22, 64, Color::rgb(0x1b1b1b), Align::center);
    }
    const float x = card.x + card.w + 36;
    ui::text(list, fonts_.bold, "for PlayStation 5", x, 110, 44, kText);
    const float bw = fonts_.bold.measure("BETA", 16, 2) + 24;
    list.rounded_rect({x, 130, bw, 32}, 16, kAmber);
    ui::text(list, fonts_.bold, "BETA", x + 12, 152, 16, Color::rgb(0x2a1a00), Align::left, 2);
    ui::text(list, fonts_.regular, "Unofficial app, not made by KamTape", x + bw + 16, 155, 24, kMuted);
    list.pop_transform();
    list.pop_opacity();
}

void App::draw_tv(gfx::DrawList &list, float x, float y, float s) const
{
    const auto R = [&](float rx, float ry, float rw, float rh) { return Rect{x + rx * s, y + ry * s, rw * s, rh * s}; };
    // antennas
    list.line(x + 190 * s, y + 46 * s, x + 118 * s, y - 34 * s, 7 * s, kCreamShade);
    list.line(x + 190 * s, y + 46 * s, x + 276 * s, y - 44 * s, 7 * s, kCreamShade);
    list.circle(x + 118 * s, y - 34 * s, 10 * s, kCream);
    list.circle(x + 276 * s, y - 44 * s, 10 * s, kCream);
    // feet and body
    list.rounded_rect(R(56, 290, 44, 26), 8 * s, kCreamShade);
    list.rounded_rect(R(280, 290, 44, 26), 8 * s, kCreamShade);
    list.shadow(R(0, 40, 380, 260), 44 * s, 30 * s, Color{0, 0, 0, 0.55f});
    list.gradient_rect(R(0, 40, 380, 260), 44 * s, kCream, kCreamShade);
    // screen
    const Rect screen = R(28, 68, 252, 204);
    list.gradient_rect(screen, 34 * s, Color::rgb(0x262a36), Color::rgb(0x0f1116));
    list.gradient_rect({screen.x + 10 * s, screen.y + 8 * s, screen.w - 20 * s, screen.h * 0.4f}, 28 * s,
                       Color{1, 1, 1, 0.08f}, Color{1, 1, 1, 0.0f});
    const float pulse = 0.85f + 0.15f * std::sin(time_ * 2.4f);
    list.glow({screen.cx() - 70 * s, screen.cy() - 40 * s, 140 * s, 80 * s}, 40 * s, 40 * s,
              kAccent.with_alpha(0.25f * pulse));
    ui::text(list, fonts_.bold, "XL", screen.cx(), screen.cy() + 40 * s, 112 * s, kAccent, Align::center);
    // knobs and speaker
    list.circle(x + 330 * s, y + 118 * s, 20 * s, Color::rgb(0x3b404d));
    list.ring(x + 330 * s, y + 118 * s, 20 * s, 3 * s, Color::rgb(0x9a8f7a));
    list.line(x + 330 * s, y + 118 * s, x + 330 * s, y + 104 * s, 4 * s, kCream);
    list.circle(x + 330 * s, y + 176 * s, 15 * s, Color::rgb(0x3b404d));
    for (int i = 0; i < 4; ++i)
        list.line(x + 312 * s, y + (220 + i * 14) * s, x + 348 * s, y + (220 + i * 14) * s, 4 * s,
                  Color::rgb(0xa89c84));
}

void App::draw_tile_icon(gfx::DrawList &list, Kind kind, float cx, float cy) const
{
    list.circle(cx, cy, 48, kBadge);
    switch (kind)
    {
    case Kind::site:
        list.bordered_rect({cx - 27, cy - 21, 54, 42}, 7, kClear, 4, kText);
        list.line(cx - 25, cy - 9, cx + 25, cy - 9, 3, kText);
        list.circle(cx - 18, cy - 15, 2.5f, kAccent);
        list.circle(cx - 11, cy - 15, 2.5f, kAmber);
        list.circle(cx - 4, cy - 15, 2.5f, kMuted);
        list.line(cx - 15, cy + 2, cx + 15, cy + 2, 3, kMuted);
        list.line(cx - 15, cy + 10, cx + 6, cy + 10, 3, kMuted);
        break;
    case Kind::go_to:
        list.line(cx - 22, cy, cx + 8, cy, 6, kText);
        list.triangle({cx + 2, cy - 15, 26, 30}, kText, 0.0f, 1.5708f);
        break;
    case Kind::settings:
        for (int i = 0; i < 8; ++i)
        {
            const float a = static_cast<float>(i) * 0.785398f;
            list.rotated_rect({cx + 22 * std::sin(a) - 5, cy - 22 * std::cos(a) - 6, 10, 12}, 2, a, kText);
        }
        list.circle(cx, cy, 19, kText);
        list.circle(cx, cy, 8, kBadge);
        break;
    default:
        break;
    }
}

void App::draw_focus(gfx::DrawList &list, const Rect &r, float radius) const
{
    const float pulse = 0.5f + 0.5f * std::sin(time_ * 3.0f);
    list.glow(r, radius, 16.0f + 6.0f * pulse, kAccent.with_alpha(0.28f + 0.1f * pulse));
    list.bordered_rect(r.inset(-5.0f), radius + 5.0f, kClear, 4.0f, kAccent);
}

void App::draw_home(gfx::DrawList &list) const
{
    const auto enter = [&](int i) {
        const float t = tween::stagger(screen_time_, i, 0.07f, 0.5f);
        list.push_opacity(t);
        list.push_transform(1.0f, 0, 0, 0, (1.0f - t) * 36.0f);
    };
    const auto leave = [&] {
        list.pop_transform();
        list.pop_opacity();
    };

    // the focus ring goes under the cards so they sit on its glow
    list.push_opacity(tween::stagger(screen_time_, 0, 0.0f, 0.5f));
    draw_focus(list, focus_rect_, items_[focus_].kind == Kind::recent ? 30.0f : 28.0f);
    list.pop_opacity();

    // KamTape XL
    enter(0);
    {
        const Rect r = kHero;
        list.shadow(r, 28, 30, Color{0, 0, 0, 0.45f});
        list.gradient_rect(r, 28, kHeroTop, kHeroBottom);
        list.bordered_rect(r, 28, kClear, 2, kHeroLine);
        ui::text(list, fonts_.bold, "WATCH ON THE TV", r.x + 60, r.y + 84, 18, Color::rgb(0xb6f19a), Align::left, 3);
        ui::text(list, fonts_.bold, "KamTape XL", r.x + 60, r.y + 150, 66, kText);
        ui::paragraph(list, fonts_.regular,
                      "The TV interface: big pictures and simple controls, made for watching from the couch.",
                      r.x + 60, r.y + 206, 30, 500, 42, Color::rgb(0xd5e2cf), 3);
        ui::text(list, fonts_.regular, "Opens kamtape.com/xl/ in the PS5 browser.", r.x + 60, r.y + 352, 24, kMuted);
        const Rect chip{r.x + 60, r.y + 396, 236, 68};
        list.rounded_rect(chip, 34, kAccent);
        list.circle(chip.x + 40, chip.cy(), 18, kOnAccent.with_alpha(0.18f));
        list.line(chip.x + 33, chip.cy() - 7, chip.x + 47, chip.cy() + 7, 3, kOnAccent);
        list.line(chip.x + 33, chip.cy() + 7, chip.x + 47, chip.cy() - 7, 3, kOnAccent);
        ui::text(list, fonts_.bold, "Watch", chip.x + 72, chip.cy() + 11, 32, kOnAccent);
        draw_tv(list, r.x + 606, r.y + 150, 1.0f);
    }
    leave();

    // the right-hand tiles
    struct Tile
    {
        const char *title;
        const char *detail;
    };
    const Tile tiles[] = {
        {"KamTape", "The whole site, as on a computer."},
        {"Go to a page", "Type the address of any KamTape page."},
        {"Settings", "Start-up, sounds and help."},
    };
    for (int i = 0; i < 3; ++i)
    {
        enter(i + 1);
        const Rect r = items_[i + 1].rect;
        list.shadow(r, 22, 22, Color{0, 0, 0, 0.4f});
        list.rounded_rect(r, 22, focus_ == i + 1 ? kPanelHi : kPanel);
        list.bordered_rect(r, 22, kClear, 1.5f, kLine);
        draw_tile_icon(list, items_[i + 1].kind, r.x + 88, r.cy());
        ui::text(list, fonts_.bold, tiles[i].title, r.x + 164, r.cy() - 4, 38, kText);
        ui::text(list, fonts_.regular, tiles[i].detail, r.x + 164, r.cy() + 36, 24, kMuted);
        leave();
    }

    // recent pages
    enter(4);
    ui::text(list, fonts_.bold, "RECENT PAGES", kHero.x, 800, 18, kDim, Align::left, 3);
    if (settings_.recent.empty())
    {
        ui::text(list, fonts_.regular, "Pages you open with Go to a page show up here.", kHero.x, kRecentY + 40, 24,
                 kDim);
    }
    for (const Item &item : items_)
    {
        if (item.kind != Kind::recent)
            continue;
        const bool focused = &item == &items_[focus_];
        list.rounded_rect(item.rect, 30, focused ? kPanelHi : kPanel);
        list.bordered_rect(item.rect, 30, kClear, 1.5f, kLine);
        const std::string label =
            fonts_.regular.font->fit(short_address(settings_.recent[item.recent]), 24.0f, kRecentMaxText);
        ui::text(list, fonts_.regular, label, item.rect.x + 28, item.rect.cy() + 8, 24, focused ? kText : kMuted);
    }
    leave();
}

void App::draw_settings(gfx::DrawList &list) const
{
    const float t = tween::cubic_out(screen_time_ / 0.4f);
    list.push_opacity(t);
    list.push_transform(1.0f, 0, 0, 0, (1.0f - t) * 30.0f);
    const Rect p = kSettingsPanel;
    list.shadow(p, 30, 40, Color{0, 0, 0, 0.5f});
    list.rounded_rect(p, 30, kPanel);
    list.bordered_rect(p, 30, kClear, 1.5f, kLine);
    ui::text(list, fonts_.bold, "Settings", p.x + 40, p.y + 74, 46, kText);

    draw_focus(list, focus_rect_, 18.0f);
    for (int i = 0; i < kSettingsRows; ++i)
    {
        const Rect r = settings_row(i);
        const bool focused = i == settings_focus_;
        list.rounded_rect(r, 18, focused ? kPanelHi : Color::rgb(0x1e212b));
        const float cy = r.cy();
        const float right = r.x + r.w - 32;
        switch (i)
        {
        case 0:
        {
            ui::text(list, fonts_.bold, "Open KamTape XL when the app starts", r.x + 32, cy + 11, 30, kText);
            const bool on = settings_.open_xl_on_start;
            const Rect sw{right - 92, cy - 22, 92, 44};
            list.rounded_rect(sw, 22, on ? kAccent : Color::rgb(0x3a3f4c));
            list.circle(on ? sw.x + sw.w - 22 : sw.x + 22, cy, 16, kText);
            ui::text(list, fonts_.regular, on ? "On" : "Off", sw.x - 18, cy + 10, 28, kMuted, Align::right);
            break;
        }
        case 1:
        {
            ui::text(list, fonts_.bold, "Interface sounds", r.x + 32, cy + 11, 30, kText);
            const char *name = kSoundNames[std::clamp(settings_.sounds, 0, 3)];
            const float w = 170;
            list.triangle({right - 18, cy - 10, 18, 20}, settings_.sounds < 3 ? kText : kDim, 0.0f, 1.5708f);
            list.triangle({right - w - 18, cy - 10, 18, 20}, settings_.sounds > 0 ? kText : kDim, 0.0f, -1.5708f);
            ui::text(list, fonts_.regular, name, right - 9 - w / 2 - 9, cy + 10, 28, kText, Align::center);
            break;
        }
        case 2:
        {
            ui::text(list, fonts_.bold, "Clear recent pages", r.x + 32, cy + 11, 30, kText);
            const int n = static_cast<int>(settings_.recent.size());
            const std::string value = n == 0 ? "None saved" : (std::to_string(n) + (n == 1 ? " page" : " pages"));
            ui::text(list, fonts_.regular, value, right, cy + 10, 28, kMuted, Align::right);
            break;
        }
        default:
            break;
        }
    }

    float y = settings_row(kSettingsRows - 1).y + 80 + 70;
    ui::text(list, fonts_.bold, "ABOUT", p.x + 40, y, 18, kDim, Align::left, 3);
    y += 44;
    const float w = p.w - 80;
    y = ui::paragraph(list, fonts_.regular,
                      std::string("KamTape for PS5 ") + kVersion +
                          ". An unofficial app, not made or endorsed by KamTape.",
                      p.x + 40, y, 26, w, 38, kText);
    y = ui::paragraph(list, fonts_.regular,
                      "In the browser, move the pointer with the left stick and press Cross to click. "
                      "Close the browser to come back here.",
                      p.x + 40, y + 6, 26, w, 38, kMuted);
    ui::paragraph(list, fonts_.regular,
                  std::string("Source code and licence (GPL-3.0): ") + kSourceUrl, p.x + 40, y + 6, 26, w, 38, kMuted);
    list.pop_transform();
    list.pop_opacity();
}

void App::draw_footer(gfx::DrawList &list) const
{
    list.rounded_rect({96, 966, kRight - 96, 1.5f}, 0, kLine);
    const float cy = 1016;
    float x = 96;
    if (screen_ == Screen::home)
    {
        x = draw_hint(list, "cross", "Open", x, cy);
        x = draw_hint(list, "triangle", "Go to a page", x, cy);
        draw_hint(list, "OPTIONS", "Settings", x, cy);
    }
    else
    {
        x = draw_hint(list, "cross", settings_focus_ == 2 ? "Clear" : "Change", x, cy);
        draw_hint(list, "circle", "Back", x, cy);
    }
    ui::text(list, fonts_.regular, std::string("Version ") + kVersion, kRight, cy + 8, 22, kDim, Align::right);
}

void App::draw_status(gfx::DrawList &list) const
{
    const float age = time_ - status_time_;
    if (status_.empty() || age < 0.0f || age > 4.0f)
        return;
    const float alpha = std::min(tween::cubic_out(age / 0.2f), std::clamp((4.0f - age) / 0.5f, 0.0f, 1.0f));
    const std::string text = fonts_.regular.font->fit(status_, 26.0f, 1200.0f);
    const float w = fonts_.regular.measure(text, 26.0f) + 72.0f;
    const Rect r{960.0f - w / 2.0f, 894.0f + (1.0f - alpha) * 12.0f, w, 56.0f};
    list.push_opacity(alpha);
    list.shadow(r, 28, 24, Color{0, 0, 0, 0.5f});
    list.rounded_rect(r, 28, status_error_ ? Color::rgb(0x4a1d1b) : kPanelHi);
    list.bordered_rect(r, 28, kClear, 1.5f, status_error_ ? Color::rgb(0xff6b5a) : kLine);
    list.circle(r.x + 30, r.cy(), 6, status_error_ ? Color::rgb(0xff6b5a) : kAmber);
    ui::text(list, fonts_.regular, text, r.x + 48, r.cy() + 9, 26, kText);
    list.pop_opacity();
}

void App::draw(gfx::DrawList &list) const
{
    draw_background(list);
    draw_header(list);
    if (screen_ == Screen::home)
        draw_home(list);
    else
        draw_settings(list);
    draw_footer(list);
    draw_status(list);
}

} // namespace hui::kt
