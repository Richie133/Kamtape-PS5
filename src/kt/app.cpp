// KamTape PS5 - the app's screens.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/app.hpp"

#include "core/tween.hpp"
#include "kt/address.hpp"
#include "kt/audio.hpp"
#include "kt/services.hpp"
#include "kt/style.hpp"
#include "platform/ps5/system.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace hui::kt
{

namespace
{

using gfx::Align;
using gfx::Color;
using gfx::Rect;

using namespace style;
const Color kCream = Color::rgb(0xf1e8d6);
const Color kCreamShade = Color::rgb(0xd6c8ae);
const Color kHeroTop = Color::rgb(0x1e3a18);
const Color kHeroBottom = Color::rgb(0x0f1a10);
const Color kHeroLine = Color::rgb(0x31592a);

constexpr float kRight = 1824.0f;

// home
constexpr Rect kHero{96.0f, 220.0f, 1040.0f, 520.0f};
constexpr float kTileX = 1176.0f;
constexpr float kTileW = 648.0f;
constexpr float kTileH = 118.0f;
constexpr float kTileGap = 16.0f;
constexpr float kRecentY = 822.0f;
constexpr float kRecentH = 60.0f;
constexpr float kRecentMaxText = 360.0f;

// settings
constexpr Rect kSettingsPanel{336.0f, 206.0f, 1248.0f, 744.0f};
constexpr int kSettingsRows = 5;

// the video grid
constexpr int kCols = 5;
constexpr float kGridX = 96.0f;
constexpr float kCardGap = 26.0f;
constexpr float kCardW = (1728.0f - (kCols - 1) * kCardGap) / kCols;
constexpr float kThumbH = 214.0f;
constexpr float kCardH = kThumbH + 92.0f;
constexpr float kRowStep = kCardH + 30.0f;
constexpr float kGridTop = 300.0f;
constexpr int kPerPage = 20;

// the details page
constexpr Rect kDetailThumb{96.0f, 220.0f, 640.0f, 480.0f};
constexpr float kDetailX = 784.0f;
constexpr float kDetailW = kRight - kDetailX;

Rect settings_row(int i)
{
    return {kSettingsPanel.x + 40.0f, kSettingsPanel.y + 104.0f + static_cast<float>(i) * 82.0f,
            kSettingsPanel.w - 80.0f, 70.0f};
}

Rect card_thumb(int index, float scroll)
{
    const int row = index / kCols;
    const int col = index % kCols;
    return {kGridX + static_cast<float>(col) * (kCardW + kCardGap),
            kGridTop + (static_cast<float>(row) - scroll) * kRowStep, kCardW, kThumbH};
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

std::string trimmed(const std::string &s)
{
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t'))
        ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t'))
        --e;
    return s.substr(b, e - b);
}

std::string views_label(long long views)
{
    return format_count(views) + (views == 1 ? " view" : " views");
}

} // namespace

App::App(const Fonts &fonts, std::uint32_t logo, float logo_aspect, std::string settings_dir)
    : fonts_(fonts), logo_(logo), logo_aspect_(logo_aspect), settings_dir_(std::move(settings_dir)),
      thumbs_(std::make_unique<Thumbs>()), api_(std::make_unique<ApiClient>())
{
    settings_ = Settings::load(settings_dir_ + "/settings.json");
    api_configure(settings_dir_);
    audio::set_volume(0.0f, settings_.sound_volume());
    layout_home();
}

App::~App() = default;

App::Screen App::screen() const
{
    return pages_.empty() ? screen_ : pages_.back().screen;
}

void App::save()
{
    settings_.save(settings_dir_ + "/settings.json");
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
    const Kind tiles[] = {Kind::featured, Kind::search, Kind::go_to, Kind::settings};
    for (int i = 0; i < 4; ++i)
        items_.push_back(
            {tiles[i], -1, {kTileX, kHero.y + static_cast<float>(i) * (kTileH + kTileGap), kTileW, kTileH}});
    float x = kHero.x;
    for (int i = 0; i < static_cast<int>(settings_.recent.size()); ++i)
    {
        const std::string label = fonts_.regular.font->fit(
            short_address(settings_.recent[static_cast<std::size_t>(i)]), 24.0f, kRecentMaxText);
        const float w = fonts_.regular.measure(label, 24.0f) + 56.0f;
        if (x + w > kRight)
            break;
        items_.push_back({Kind::recent, i, {x, kRecentY, w, kRecentH}});
        x += w + 16.0f;
    }
    focus_ = std::clamp(focus_, 0, static_cast<int>(items_.size()) - 1);
}

int App::neighbour(const std::vector<Rect> &rects, int from, Direction direction) const
{
    if (direction == Direction::none || from < 0 || from >= static_cast<int>(rects.size()))
        return -1;
    const Rect &a = rects[static_cast<std::size_t>(from)];
    const bool horizontal = direction == Direction::left || direction == Direction::right;
    int best = -1;
    float best_score = 1e30f;
    for (int i = 0; i < static_cast<int>(rects.size()); ++i)
    {
        if (i == from)
            continue;
        const Rect &b = rects[static_cast<std::size_t>(i)];
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
        const float across =
            horizontal ? gap(a.y, a.y + a.h, b.y, b.y + b.h) : gap(a.x, a.x + a.w, b.x, b.x + b.w);
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

void App::open(const std::string &url, const std::string &what)
{
    if (time_ - last_open_ < 1.0f)
        return; // one press, one browser
    last_open_ = time_;
    if (services::open_browser(url))
    {
        audio::play(audio::Sfx::open);
        show_status("Opening " + fonts_.regular.font->fit(what, 26.0f, 700.0f) + " in the PS5 browser...", false);
    }
    else
    {
        audio::play(audio::Sfx::error);
        show_status("The PS5 browser didn't open. Try again in a moment.", true);
    }
}

void App::open_keyboard(Keyboard purpose, const std::string &start)
{
    const bool search = purpose == Keyboard::search;
    const std::string title = search ? "Search KamTape by tag" : "Go to a KamTape page";
    const std::string text = search ? start : (start.empty() ? "kamtape.com/" : start);
    keyboard_ = purpose;
    audio::play(audio::Sfx::select);
    if (settings_.system_keyboard && services::keyboard_open(title, text))
        return;
    // the app's own keyboard (also when the system one won't open)
    std::vector<std::string> suggestions;
    if (search)
    {
        suggestions = settings_.searches;
    }
    else
    {
        for (const std::string &url : settings_.recent)
            suggestions.push_back(short_address(url));
    }
    osk_.open(title, text, search ? OnScreenKeyboard::Mode::text : OnScreenKeyboard::Mode::address,
              std::move(suggestions));
}

bool App::begin_usb()
{
    if (settings_.usb_trying)
    {
        // the last start died while the USB keyboard was starting
        settings_.usb_trying = false;
        settings_.usb_input = false;
        save();
        show_status("USB keyboard is off: starting it failed last time. Settings turns it back on.", true);
        return false;
    }
    if (!settings_.usb_input)
        return false;
    settings_.usb_trying = true;
    save();
    return true;
}

void App::end_usb(bool opened)
{
    (void)opened;
    settings_.usb_trying = false;
    save();
}

void App::usb_navigation(const hid::Frame &usb, InputFrame &input)
{
    const auto press = [&](Action a) { input.pressed |= action_bit(a); };
    for (std::uint16_t key : usb.pressed)
    {
        switch (key)
        {
        case hid::key::kUp:
            input.nav = Direction::up;
            break;
        case hid::key::kDown:
            input.nav = Direction::down;
            break;
        case hid::key::kLeft:
            input.nav = Direction::left;
            break;
        case hid::key::kRight:
            input.nav = Direction::right;
            break;
        case hid::key::kEnter:
            press(Action::confirm);
            break;
        case hid::key::kEscape:
        case hid::key::kBackspace:
            press(Action::back);
            break;
        case hid::key::kTab:
            press(Action::menu);
            break;
        default:
            break;
        }
    }
    // typing a letter starts a tag search with it
    const Screen now = screen();
    if ((now == Screen::home || now == Screen::videos) && !usb.text.empty())
    {
        const char c = usb.text.front();
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            open_keyboard(Keyboard::search, std::string(1, c));
    }
}

void App::finish_keyboard(const std::string &text)
{
    const Keyboard purpose = keyboard_;
    keyboard_ = Keyboard::none;
    if (purpose == Keyboard::search)
    {
        const std::string tag = trimmed(text);
        if (!tag.empty())
        {
            settings_.remember_search(tag);
            save();
            push_list(ListKind::tag, tag);
        }
        return;
    }
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
    open(address.url, short_address(address.url));
}

void App::open_settings()
{
    audio::play(audio::Sfx::select);
    screen_ = Screen::settings;
    settings_focus_ = 0;
    screen_time_ = 0.0f;
    focus_snapped_ = false;
}

void App::activate_home(const Item &item)
{
    switch (item.kind)
    {
    case Kind::xl:
        open(kXlUrl, "KamTape XL");
        break;
    case Kind::featured:
        push_list(ListKind::featured, "");
        break;
    case Kind::search:
        open_keyboard(Keyboard::search);
        break;
    case Kind::go_to:
        open_keyboard(Keyboard::go_to);
        break;
    case Kind::settings:
        open_settings();
        break;
    case Kind::recent:
    {
        if (item.recent < 0 || item.recent >= static_cast<int>(settings_.recent.size()))
            break;
        const std::string url = settings_.recent[static_cast<std::size_t>(item.recent)];
        settings_.remember(url);
        save();
        layout_home();
        focus_ = 5; // the page just opened moves to the front of the row
        open(url, short_address(url));
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
        const int next =
            std::clamp(settings_focus_ + (input.nav == Direction::down ? 1 : -1), 0, kSettingsRows - 1);
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
        if (confirm || left || right)
        {
            settings_.system_keyboard = !settings_.system_keyboard;
            save();
            audio::play(audio::Sfx::select);
        }
        break;
    case 2:
        if (confirm || left || right)
        {
            settings_.usb_input = !settings_.usb_input;
            save();
            audio::play(audio::Sfx::select);
            show_status(settings_.usb_input ? "The USB keyboard starts next time the app opens."
                                            : "The USB keyboard is off from the next start.",
                        false);
        }
        break;
    case 3:
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
    case 4:
        if (confirm)
        {
            if (settings_.recent.empty() && settings_.searches.empty())
            {
                audio::play(audio::Sfx::error);
                show_status("There is nothing to clear.", false);
            }
            else
            {
                settings_.recent.clear();
                settings_.searches.clear();
                save();
                layout_home();
                audio::play(audio::Sfx::select);
                show_status("Recent pages and searches cleared.", false);
            }
        }
        break;
    default:
        break;
    }
}

// ---- video lists ----------------------------------------------------------------

void App::push_list(ListKind kind, const std::string &arg)
{
    audio::play(audio::Sfx::select);
    Page page;
    page.screen = Screen::videos;
    page.list = kind;
    page.arg = arg;
    switch (kind)
    {
    case ListKind::featured:
        page.title = "Featured videos";
        break;
    case ListKind::tag:
        page.title = "Videos tagged \"" + arg + "\"";
        break;
    case ListKind::user:
        page.title = "Videos by " + arg;
        break;
    }
    pages_.push_back(std::move(page));
    load_more(pages_.back());
    focus_snapped_ = false;
}

void App::load_more(Page &page)
{
    if (page.ticket != 0)
        return;
    page.error.clear();
    std::string url;
    switch (page.list)
    {
    case ListKind::featured:
        url = url_featured();
        break;
    case ListKind::tag:
        url = url_by_tag(page.arg, page.next_page, kPerPage);
        break;
    case ListKind::user:
        url = url_by_user(page.arg);
        break;
    }
    page.ticket = api_->fetch(url);
}

void App::push_details(const Video &video)
{
    audio::play(audio::Sfx::select);
    Page page;
    page.screen = Screen::details;
    page.details.video = video;
    page.title = video.title;
    if (!video.id.empty())
        page.ticket = api_->fetch(url_details(video.id));
    else
        page.details_loaded = true;
    pages_.push_back(std::move(page));
    focus_snapped_ = false;
}

// Picks up the answer to a page's request, if it has come back.
void App::collect(Page &page)
{
    if (page.ticket == 0)
        return;
    http::Response response;
    if (!api_->take(page.ticket, &response))
        return;
    page.ticket = 0;
    if (!response.ok())
    {
        const std::string why =
            response.status > 0 ? "HTTP " + std::to_string(response.status) : response.error;
        if (page.screen == Screen::details)
        {
            page.details_loaded = true; // keep what the list had
            return;
        }
        // an API error can come with a failing status: say what it says
        std::vector<Video> ignored;
        const ApiError api = parse_video_list(response.body, &ignored);
        page.error = api.failed && api.message.rfind("KamTape: ", 0) == 0
                         ? api.message
                         : "Couldn't reach KamTape (" + why + ").";
        sys::log("[KT] list failed: %s", page.error.c_str());
        return;
    }
    if (page.screen == Screen::details)
    {
        VideoDetails details;
        const ApiError error = parse_video_details(response.body, page.details.video.id, &details);
        if (!error.failed)
        {
            Video &v = page.details.video;
            const Video &d = details.video;
            // the list's fields stay where the details leave them empty (the url is only in lists)
            if (!d.title.empty())
                v.title = d.title;
            if (!d.author.empty())
                v.author = d.author;
            if (!d.description.empty())
                v.description = d.description;
            if (!d.tags.empty())
                v.tags = d.tags;
            if (!d.thumbnail_url.empty() && v.thumbnail_url.empty())
                v.thumbnail_url = d.thumbnail_url;
            if (d.length_seconds > 0)
                v.length_seconds = d.length_seconds;
            if (d.rating_count > 0)
            {
                v.rating_avg = d.rating_avg;
                v.rating_count = d.rating_count;
            }
            if (d.view_count > 0)
                v.view_count = d.view_count;
            if (d.upload_time > 0)
                v.upload_time = d.upload_time;
            page.details.comments = std::move(details.comments);
            page.details.channels = std::move(details.channels);
            page.details.update_time = details.update_time;
            if (v.comment_count < static_cast<int>(page.details.comments.size()))
                v.comment_count = static_cast<int>(page.details.comments.size());
        }
        else
        {
            sys::log("[KT] details failed: %s", error.message.c_str());
        }
        page.details_loaded = true;
        return;
    }
    std::vector<Video> videos;
    const ApiError error = parse_video_list(response.body, &videos);
    if (error.failed)
    {
        page.error = error.message;
        sys::log("[KT] list failed: %s", page.error.c_str());
        return;
    }
    page.more = page.list == ListKind::tag && static_cast<int>(videos.size()) >= kPerPage;
    ++page.next_page;
    for (Video &v : videos)
        page.videos.push_back(std::move(v));
}

void App::update_videos(Page &page, const InputFrame &input)
{
    const int count = static_cast<int>(page.videos.size());
    if (input.is_pressed(Action::back))
    {
        audio::play(audio::Sfx::back);
        pages_.pop_back();
        focus_snapped_ = false;
        return;
    }
    if (input.is_pressed(Action::north))
    {
        open_keyboard(Keyboard::search);
        return;
    }
    if (count == 0)
    {
        if (!page.error.empty() && input.is_pressed(Action::confirm))
        {
            audio::play(audio::Sfx::select);
            load_more(page);
        }
        return;
    }
    int next = page.focus;
    switch (input.nav)
    {
    case Direction::left:
        if (page.focus % kCols > 0)
            next = page.focus - 1;
        break;
    case Direction::right:
        if (page.focus % kCols < kCols - 1 && page.focus + 1 < count)
            next = page.focus + 1;
        break;
    case Direction::up:
        if (page.focus >= kCols)
            next = page.focus - kCols;
        break;
    case Direction::down:
        if (page.focus + kCols < count)
            next = page.focus + kCols;
        else if (page.focus / kCols < (count - 1) / kCols)
            next = count - 1; // a short last row
        break;
    case Direction::none:
        break;
    }
    if (next != page.focus)
    {
        page.focus = next;
        sound_move();
    }
    const int row = page.focus / kCols;
    if (row < page.top_row)
        page.top_row = row;
    else if (row > page.top_row + 1)
        page.top_row = row - 1;
    // near the end of a tag search: ask for the next page
    if (page.more && page.ticket == 0 && page.error.empty() && row >= (count - 1) / kCols - 1)
        load_more(page);

    const Video video = page.videos[static_cast<std::size_t>(page.focus)]; // pages_ may grow below
    if (input.is_pressed(Action::confirm))
        push_details(video);
    else if (input.is_pressed(Action::west))
        open(watch_url(video), "\"" + video.title + "\"");
}

// ---- details ------------------------------------------------------------------------

// How far everything under the title moves up when the title takes one line.
float App::details_shift(const Page &page) const
{
    const Video &v = page.details.video;
    const ui::ParagraphLines title =
        ui::paragraph_lines(fonts_.bold, v.title.empty() ? "Untitled" : v.title, 46.0f, kDetailW, 2);
    return title.lines.size() > 1 ? 0.0f : -56.0f;
}

std::vector<App::Choice> App::details_choices(const Page &page) const
{
    std::vector<Choice> actions;
    const Video &v = page.details.video;
    const float shift = details_shift(page);
    float x = kDetailX;
    const float y = 500.0f + shift;
    {
        const float w = fonts_.bold.measure("Watch", 30.0f) + 104.0f;
        actions.push_back({Choice::Type::watch, watch_url(v), {x, y, w, 64}});
        x += w + 18.0f;
    }
    if (!v.author.empty())
    {
        const std::string label = fonts_.bold.font->fit("More from " + v.author, 26.0f, 420.0f);
        const float w = fonts_.bold.measure(label, 26.0f) + 56.0f;
        actions.push_back({Choice::Type::author, v.author, {x, y, w, 64}});
    }
    // tags: up to two rows
    float tx = kDetailX;
    float ty = 628.0f + shift;
    int row = 0;
    for (const std::string &tag : split_tags(v.tags))
    {
        const std::string label = fonts_.regular.font->fit(tag, 24.0f, 300.0f);
        const float w = fonts_.regular.measure(label, 24.0f) + 44.0f;
        if (tx + w > kRight)
        {
            if (++row == 2)
                break;
            tx = kDetailX;
            ty += 62.0f;
        }
        actions.push_back({Choice::Type::tag, tag, {tx, ty, w, 50}});
        tx += w + 12.0f;
    }
    return actions;
}

void App::update_details(Page &page, const InputFrame &input)
{
    if (input.is_pressed(Action::back))
    {
        audio::play(audio::Sfx::back);
        pages_.pop_back();
        focus_snapped_ = false;
        return;
    }
    const std::vector<Choice> actions = details_choices(page);
    page.action = std::clamp(page.action, 0, std::max(static_cast<int>(actions.size()) - 1, 0));
    if (input.nav != Direction::none)
    {
        std::vector<Rect> rects;
        for (const Choice &a : actions)
            rects.push_back(a.rect);
        const int next = neighbour(rects, page.action, input.nav);
        if (next >= 0 && next != page.action)
        {
            page.action = next;
            sound_move();
        }
    }
    const Video video = page.details.video; // the page may go away below
    if (input.is_pressed(Action::west))
    {
        open(watch_url(video), "\"" + video.title + "\"");
        return;
    }
    if (!input.is_pressed(Action::confirm) || actions.empty())
        return;
    const Choice action = actions[static_cast<std::size_t>(page.action)];
    switch (action.type)
    {
    case Choice::Type::watch:
        open(action.value, "\"" + video.title + "\"");
        break;
    case Choice::Type::author:
        push_list(ListKind::user, action.value);
        break;
    case Choice::Type::tag:
        push_list(ListKind::tag, action.value);
        break;
    }
}

// ---- frame --------------------------------------------------------------------

Rect App::focus_target() const
{
    if (!pages_.empty())
    {
        const Page &page = pages_.back();
        if (page.screen == Screen::videos)
            return page.videos.empty() ? Rect{960, 600, 0, 0} : card_thumb(page.focus, page.scroll);
        const std::vector<Choice> actions = details_choices(page);
        if (actions.empty())
            return {kDetailX, 500, 0, 0};
        const int i = std::clamp(page.action, 0, static_cast<int>(actions.size()) - 1);
        return actions[static_cast<std::size_t>(i)].rect;
    }
    return screen_ == Screen::home ? items_[static_cast<std::size_t>(focus_)].rect : settings_row(settings_focus_);
}

void App::update(const InputFrame &pad_input, const hid::Frame &usb, float dt)
{
    time_ += dt;
    screen_time_ += dt;
    thumbs_->pump();
    for (Page &page : pages_)
    {
        page.time += dt;
        collect(page);
        const float k = 1.0f - std::exp(-14.0f * dt);
        page.scroll += (static_cast<float>(page.top_row) - page.scroll) * k;
        if (std::fabs(page.scroll - static_cast<float>(page.top_row)) < 0.002f)
            page.scroll = static_cast<float>(page.top_row);
    }

    InputFrame input = pad_input;
    bool typing = false;
    if (osk_.is_open())
    {
        typing = true; // the on-screen keyboard has the controller
        switch (osk_.update(input, usb, dt))
        {
        case OnScreenKeyboard::Result::done:
            finish_keyboard(osk_.text());
            break;
        case OnScreenKeyboard::Result::cancelled:
            keyboard_ = Keyboard::none;
            break;
        case OnScreenKeyboard::Result::none:
            break;
        }
    }
    else if (keyboard_ != Keyboard::none)
    {
        std::string text;
        switch (services::keyboard_poll(&text))
        {
        case services::KeyboardState::open:
            typing = true; // the keyboard has the controller
            break;
        case services::KeyboardState::done:
            finish_keyboard(text);
            typing = true; // this frame's buttons were for the keyboard
            break;
        case services::KeyboardState::cancelled:
        case services::KeyboardState::idle:
            keyboard_ = Keyboard::none;
            typing = true;
            break;
        }
    }
    else if (!auto_open_done_ && time_ >= 1.2f)
    {
        auto_open_done_ = true;
        if (settings_.open_xl_on_start)
            open(kXlUrl, "KamTape XL");
    }

    if (!typing && keyboard_ == Keyboard::none)
        usb_navigation(usb, input);
    if (!typing && keyboard_ == Keyboard::none)
    {
        if (!pages_.empty())
        {
            Page &page = pages_.back();
            if (page.screen == Screen::videos)
                update_videos(page, input);
            else
                update_details(page, input);
        }
        else if (screen_ == Screen::settings)
        {
            update_settings(input);
        }
        else
        {
            if (input.nav != Direction::none)
            {
                std::vector<Rect> rects;
                for (const Item &item : items_)
                    rects.push_back(item.rect);
                // from KamTape XL to the right column: back to the tile last used there
                const int next = items_[static_cast<std::size_t>(focus_)].kind == Kind::xl &&
                                         input.nav == Direction::right
                                     ? last_tile_
                                     : neighbour(rects, focus_, input.nav);
                if (next >= 0 && next != focus_)
                {
                    focus_ = next;
                    const Kind kind = items_[static_cast<std::size_t>(next)].kind;
                    if (kind != Kind::xl && kind != Kind::recent)
                        last_tile_ = next;
                    sound_move();
                }
            }
            if (input.is_pressed(Action::confirm))
                activate_home(items_[static_cast<std::size_t>(focus_)]);
            else if (input.is_pressed(Action::north))
                open_keyboard(Keyboard::search);
            else if (input.is_pressed(Action::menu))
                open_settings();
        }
    }

    // the focus ring glides to its target
    const Rect target = focus_target();
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
    const Rect target = focus_target();
    const bool settled = std::fabs(target.x - focus_rect_.x) < 0.5f &&
                         std::fabs(target.y - focus_rect_.y) < 0.5f && std::fabs(target.w - focus_rect_.w) < 0.5f;
    return time_ > 1.6f && screen_time_ > 0.9f && settled && !api_->busy();
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
    // a green light behind the header and a cool one at the bottom right
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
    const auto R = [&](float rx, float ry, float rw, float rh) {
        return Rect{x + rx * s, y + ry * s, rw * s, rh * s};
    };
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
        list.line(x + 312 * s, y + static_cast<float>(220 + i * 14) * s, x + 348 * s,
                  y + static_cast<float>(220 + i * 14) * s, 4 * s, Color::rgb(0xa89c84));
}

void App::draw_tile_icon(gfx::DrawList &list, Kind kind, float cx, float cy) const
{
    list.circle(cx, cy, 40, kBadge);
    switch (kind)
    {
    case Kind::featured:
        list.star(cx, cy + 1, 24, kAmber);
        break;
    case Kind::search:
        list.ring(cx - 5, cy - 5, 15, 5, kText);
        list.line(cx + 6, cy + 6, cx + 19, cy + 19, 6, kText);
        break;
    case Kind::go_to:
        list.line(cx - 19, cy, cx + 6, cy, 5, kText);
        list.triangle({cx + 1, cy - 12, 22, 24}, kText, 0.0f, 1.5708f);
        break;
    case Kind::settings:
        for (int i = 0; i < 8; ++i)
        {
            const float a = static_cast<float>(i) * 0.785398f;
            list.rotated_rect({cx + 19 * std::sin(a) - 4.5f, cy - 19 * std::cos(a) - 5, 9, 10}, 2, a, kText);
        }
        list.circle(cx, cy, 16, kText);
        list.circle(cx, cy, 7, kBadge);
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

void App::draw_spinner(gfx::DrawList &list, float cx, float cy, float radius) const
{
    list.ring(cx, cy, radius, radius * 0.22f, kLine);
    list.arc(cx, cy, radius, radius * 0.22f, time_ * 5.0f, 1.6f, kAccent);
}

void App::draw_stars(gfx::DrawList &list, double rating, float x, float cy, float size) const
{
    for (int i = 0; i < 5; ++i)
    {
        const float cx = x + size * 0.5f + static_cast<float>(i) * (size + 6.0f);
        const double fill = std::clamp(rating - i, 0.0, 1.0);
        list.star(cx, cy, size * 0.5f, kBadge);
        if (fill >= 0.75)
        {
            list.star(cx, cy, size * 0.5f, kAmber);
        }
        else if (fill >= 0.25)
        {
            list.push_clip({cx - size * 0.5f, cy - size * 0.5f, size * 0.5f, size});
            list.star(cx, cy, size * 0.5f, kAmber);
            list.pop_clip();
        }
    }
}

void App::draw_thumbnail(gfx::DrawList &list, const Video &video, const Rect &r, float radius) const
{
    const Texture texture = video.thumbnail_url.empty() ? Texture{} : thumbs_->get(video.thumbnail_url);
    if (texture.id != 0 && texture.width > 0 && texture.height > 0)
    {
        // cover the rectangle, cropping the longer side
        const float ta = static_cast<float>(texture.width) / static_cast<float>(texture.height);
        const float ra = r.w / r.h;
        Rect uv = gfx::kFullUv;
        if (ta > ra)
        {
            uv.w = ra / ta;
            uv.x = (1.0f - uv.w) * 0.5f;
        }
        else
        {
            uv.h = ta / ra;
            uv.y = (1.0f - uv.h) * 0.5f;
        }
        list.image(texture.id, r, uv, Color{1, 1, 1, 1}, radius);
    }
    else
    {
        list.gradient_rect(r, radius, kBadge, kPanel);
        const float s = std::min(r.w, r.h) * 0.16f;
        list.circle(r.cx(), r.cy(), s * 1.25f, Color{1, 1, 1, 0.06f});
        list.triangle({r.cx() - s * 0.4f, r.cy() - s * 0.55f, s * 1.1f, s * 1.1f}, kDim, 0.0f, 1.5708f);
    }
    if (video.length_seconds > 0)
    {
        const std::string len = format_length(video.length_seconds);
        const float w = fonts_.bold.measure(len, 18.0f) + 18.0f;
        list.rounded_rect({r.x + r.w - w - 10, r.y + r.h - 40, w, 30}, 8, Color{0, 0, 0, 0.75f});
        ui::text(list, fonts_.bold, len, r.x + r.w - w / 2 - 10, r.y + r.h - 19, 18.0f, kText, Align::center);
    }
}

float App::draw_chip(gfx::DrawList &list, const Rect &r, const std::string &label, bool primary) const
{
    list.rounded_rect(r, r.h / 2, primary ? kAccent : kPanelHi);
    if (!primary)
        list.bordered_rect(r, r.h / 2, kClear, 1.5f, kLine);
    const float size = r.h >= 60 ? 26.0f : 24.0f;
    ui::text(list, primary ? fonts_.bold : (r.h >= 60 ? fonts_.bold : fonts_.regular), label, r.cx(),
             r.cy() + size * 0.36f, size, primary ? kOnAccent : kText, Align::center);
    return r.w;
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
    const Kind focused_kind = items_[static_cast<std::size_t>(focus_)].kind;
    draw_focus(list, focus_rect_, focused_kind == Kind::recent ? 30.0f : (focused_kind == Kind::xl ? 28.0f : 22.0f));
    list.pop_opacity();

    // KamTape XL
    enter(0);
    {
        const Rect r = kHero;
        list.shadow(r, 28, 30, Color{0, 0, 0, 0.45f});
        list.gradient_rect(r, 28, kHeroTop, kHeroBottom);
        list.bordered_rect(r, 28, kClear, 2, kHeroLine);
        ui::text(list, fonts_.bold, "WATCH ON THE TV", r.x + 60, r.y + 84, 18, kAccentText, Align::left, 3);
        ui::text(list, fonts_.bold, "KamTape XL", r.x + 60, r.y + 150, 66, kText);
        ui::paragraph(list, fonts_.regular,
                      "The TV interface: big pictures and simple controls, made for watching from the couch.",
                      r.x + 60, r.y + 206, 30, 500, 42, Color::rgb(0xd5e2cf), 3);
        ui::text(list, fonts_.regular, "Opens kamtape.com/xl/ in the PS5 browser.", r.x + 60, r.y + 352, 24,
                 kMuted);
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
        {"Featured videos", "Browse KamTape's picks right here."},
        {"Search by tag", "Find videos by tag, like music or cats."},
        {"Go to a page", "Open any KamTape address in the browser."},
        {"Settings", "Start-up, sounds and help."},
    };
    for (int i = 0; i < 4; ++i)
    {
        enter(i + 1);
        const Item &item = items_[static_cast<std::size_t>(i + 1)];
        const Rect r = item.rect;
        list.shadow(r, 22, 22, Color{0, 0, 0, 0.4f});
        list.rounded_rect(r, 22, focus_ == i + 1 ? kPanelHi : kPanel);
        list.bordered_rect(r, 22, kClear, 1.5f, kLine);
        draw_tile_icon(list, item.kind, r.x + 76, r.cy());
        ui::text(list, fonts_.bold, tiles[i].title, r.x + 140, r.cy() - 4, 34, kText);
        ui::text(list, fonts_.regular, tiles[i].detail, r.x + 140, r.cy() + 30, 22, kMuted);
        leave();
    }

    // recent pages
    enter(5);
    ui::text(list, fonts_.bold, "RECENT PAGES", kHero.x, 800, 18, kDim, Align::left, 3);
    if (settings_.recent.empty())
    {
        ui::text(list, fonts_.regular, "Pages you open with Go to a page show up here.", kHero.x, kRecentY + 40, 24,
                 kDim);
    }
    for (std::size_t i = 0; i < items_.size(); ++i)
    {
        const Item &item = items_[i];
        if (item.kind != Kind::recent)
            continue;
        const bool focused = static_cast<int>(i) == focus_;
        list.rounded_rect(item.rect, 30, focused ? kPanelHi : kPanel);
        list.bordered_rect(item.rect, 30, kClear, 1.5f, kLine);
        const std::string label = fonts_.regular.font->fit(
            short_address(settings_.recent[static_cast<std::size_t>(item.recent)]), 24.0f, kRecentMaxText);
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
    const auto toggle = [&](const Rect &r, bool on, const char *on_label, const char *off_label) {
        const float right = r.x + r.w - 32;
        const Rect sw{right - 84, r.cy() - 20, 84, 40};
        list.rounded_rect(sw, 20, on ? kAccent : Color::rgb(0x3a3f4c));
        list.circle(on ? sw.x + sw.w - 20 : sw.x + 20, r.cy(), 14, kText);
        ui::text(list, fonts_.regular, on ? on_label : off_label, sw.x - 18, r.cy() + 10, 26, kMuted, Align::right);
    };
    for (int i = 0; i < kSettingsRows; ++i)
    {
        const Rect r = settings_row(i);
        const bool focused = i == settings_focus_;
        list.rounded_rect(r, 18, focused ? kPanelHi : Color::rgb(0x1e212b));
        const float cy = r.cy();
        const float right = r.x + r.w - 32;
        const auto label = [&](const char *text) { ui::text(list, fonts_.bold, text, r.x + 32, cy + 10, 28, kText); };
        switch (i)
        {
        case 0:
            label("Open KamTape XL when the app starts");
            toggle(r, settings_.open_xl_on_start, "On", "Off");
            break;
        case 1:
        {
            label("Keyboard");
            const char *name = settings_.system_keyboard ? "PS5 system keyboard" : "KamTape keyboard";
            const float w = fonts_.regular.measure(name, 26) + 56;
            list.triangle({right - 16, cy - 9, 16, 18}, kText, 0.0f, 1.5708f);
            list.triangle({right - w - 16, cy - 9, 16, 18}, kText, 0.0f, -1.5708f);
            ui::text(list, fonts_.regular, name, right - 8 - w / 2, cy + 9, 26, kText, Align::center);
            break;
        }
        case 2:
            label("USB keyboard");
            toggle(r, settings_.usb_input, "On", "Off");
            break;
        case 3:
        {
            label("Interface sounds");
            const char *name = kSoundNames[std::clamp(settings_.sounds, 0, 3)];
            const float w = 170;
            list.triangle({right - 16, cy - 9, 16, 18}, settings_.sounds < 3 ? kText : kDim, 0.0f, 1.5708f);
            list.triangle({right - w - 16, cy - 9, 16, 18}, settings_.sounds > 0 ? kText : kDim, 0.0f, -1.5708f);
            ui::text(list, fonts_.regular, name, right - 8 - w / 2 - 8, cy + 9, 26, kText, Align::center);
            break;
        }
        case 4:
        {
            label("Clear recent pages and searches");
            const int n = static_cast<int>(settings_.recent.size() + settings_.searches.size());
            const std::string value = n == 0 ? "None saved" : (std::to_string(n) + " saved");
            ui::text(list, fonts_.regular, value, right, cy + 9, 26, kMuted, Align::right);
            break;
        }
        default:
            break;
        }
    }

    float y = settings_row(kSettingsRows - 1).y + 70 + 58;
    ui::text(list, fonts_.bold, "ABOUT", p.x + 40, y, 18, kDim, Align::left, 3);
    y += 42;
    const float w = p.w - 80;
    y = ui::paragraph(list, fonts_.regular,
                      std::string("KamTape for PS5 ") + kVersion +
                          ". An unofficial app, not made or endorsed by KamTape. Lists and details come from "
                          "KamTape's developer API.",
                      p.x + 40, y, 24, w, 34, kText);
    ui::paragraph(list, fonts_.regular, std::string("Source code and licence (GPL-3.0): ") + kSourceUrl, p.x + 40,
                  y + 4, 24, w, 34, kMuted);
    list.pop_transform();
    list.pop_opacity();
}

void App::draw_videos(gfx::DrawList &list, const Page &page) const
{
    const float t = tween::cubic_out(page.time / 0.35f);
    list.push_opacity(t);
    list.push_transform(1.0f, 0, 0, (1.0f - t) * 40.0f, 0);
    ui::text(list, fonts_.bold, fonts_.bold.font->fit(page.title, 44.0f, 1300.0f), kGridX, 262, 44, kText);
    const int count = static_cast<int>(page.videos.size());
    if (count > 0)
    {
        const std::string label =
            std::to_string(count) + (count == 1 ? " video" : " videos") + (page.more ? " so far" : "");
        ui::text(list, fonts_.regular, label, kRight, 260, 24, kMuted, Align::right);
    }

    if (count == 0)
    {
        if (page.ticket != 0)
        {
            draw_spinner(list, 960, 560, 34);
            ui::text(list, fonts_.regular, "Loading from KamTape...", 960, 650, 28, kMuted, Align::center);
        }
        else if (!page.error.empty())
        {
            const Rect r{460, 430, 1000, 300};
            list.rounded_rect(r, 26, kPanel);
            list.bordered_rect(r, 26, kClear, 1.5f, kLine);
            list.circle(r.cx(), r.y + 64, 26, kError.with_alpha(0.18f));
            list.rounded_rect({r.cx() - 3, r.y + 48, 6, 20}, 3, kError);
            list.circle(r.cx(), r.y + 78, 3.5f, kError);
            ui::paragraph(list, fonts_.regular, page.error, r.cx(), r.y + 140, 28, r.w - 100, 38, kText, 2,
                          Align::center);
            ui::text(list, fonts_.regular, "KamTape XL on the home screen still works.", r.cx(), r.y + 250, 24,
                     kMuted, Align::center);
        }
        else
        {
            ui::text(list, fonts_.regular,
                     page.list == ListKind::tag ? "No videos with that tag." : "No videos here yet.", 960, 560, 30,
                     kMuted, Align::center);
        }
        list.pop_transform();
        list.pop_opacity();
        return;
    }

    list.push_clip({0, 284, gfx::kVirtualWidth, 676});
    const int first = std::max(0, static_cast<int>(page.scroll) * kCols - kCols);
    const int last = std::min(count, (static_cast<int>(page.scroll) + 3) * kCols);
    // the focus ring under the focused card
    draw_focus(list, focus_rect_, 16.0f);
    for (int i = first; i < last; ++i)
    {
        const Video &v = page.videos[static_cast<std::size_t>(i)];
        const Rect thumb = card_thumb(i, page.scroll);
        const bool focused = i == page.focus;
        list.shadow(thumb, 16, 16, Color{0, 0, 0, 0.45f});
        draw_thumbnail(list, v, thumb, 16);
        const std::string title =
            fonts_.bold.font->fit(v.title.empty() ? "Untitled" : v.title, 24.0f, kCardW - 4);
        ui::text(list, fonts_.bold, title, thumb.x + 2, thumb.y + kThumbH + 38, 24, focused ? kAccentText : kText);
        std::string meta = v.author;
        if (v.view_count > 0 || meta.empty())
            meta += (meta.empty() ? "" : "  \xC2\xB7  ") + views_label(v.view_count);
        ui::text(list, fonts_.regular, fonts_.regular.font->fit(meta, 20.0f, kCardW - 4), thumb.x + 2,
                 thumb.y + kThumbH + 70, 20, kMuted);
    }
    list.pop_clip();
    if (page.ticket != 0)
        draw_spinner(list, kRight - 230, 252, 12); // more on the way
    list.pop_transform();
    list.pop_opacity();
}

void App::draw_details(gfx::DrawList &list, const Page &page) const
{
    const float t = tween::cubic_out(page.time / 0.35f);
    list.push_opacity(t);
    list.push_transform(1.0f, 0, 0, (1.0f - t) * 40.0f, 0);
    const Video &v = page.details.video;

    // left: the picture and the description
    list.shadow(kDetailThumb, 22, 30, Color{0, 0, 0, 0.5f});
    draw_thumbnail(list, v, kDetailThumb, 22);
    float y = kDetailThumb.y + kDetailThumb.h + 56;
    ui::text(list, fonts_.bold, "ABOUT THIS VIDEO", kDetailThumb.x, y, 18, kDim, Align::left, 3);
    const std::string description = v.description.empty() ? "No description." : v.description;
    ui::paragraph(list, fonts_.regular, description, kDetailThumb.x, y + 40, 24, kDetailThumb.w, 34,
                  v.description.empty() ? kDim : kText, 5);

    // right: title, author, numbers
    const ui::ParagraphLines title =
        ui::paragraph_lines(fonts_.bold, v.title.empty() ? "Untitled" : v.title, 46.0f, kDetailW, 2);
    ui::paragraph(list, fonts_.bold, title, kDetailX, 266, 46, 56, kText);
    const float shift = details_shift(page);
    if (!v.author.empty())
        ui::text(list, fonts_.bold, "by " + v.author, kDetailX, 376 + shift, 28, kAccentText);
    std::string stats = views_label(v.view_count);
    if (v.upload_time > 0)
        stats += "  \xC2\xB7  Added " + format_date(v.upload_time);
    ui::text(list, fonts_.regular, stats, kDetailX, 420 + shift, 26, kMuted);
    draw_stars(list, v.rating_avg, kDetailX, 456 + shift, 26);
    if (v.rating_count > 0)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.2f  \xC2\xB7  %d rating%s", v.rating_avg, v.rating_count,
                      v.rating_count == 1 ? "" : "s");
        ui::text(list, fonts_.regular, buf, kDetailX + 5 * 32 + 12, 465 + shift, 24, kMuted);
    }
    else
    {
        ui::text(list, fonts_.regular, "Not rated yet", kDetailX + 5 * 32 + 12, 465 + shift, 24, kDim);
    }

    // actions and tags
    const std::vector<Choice> actions = details_choices(page);
    if (!actions.empty())
        draw_focus(list, focus_rect_, focus_rect_.h / 2);
    bool tags_label = false;
    for (const Choice &a : actions)
    {
        switch (a.type)
        {
        case Choice::Type::watch:
        {
            list.rounded_rect(a.rect, a.rect.h / 2, kAccent);
            const float cx = a.rect.x + 40;
            list.circle(cx, a.rect.cy(), 18, kOnAccent.with_alpha(0.18f));
            list.triangle({cx - 6, a.rect.cy() - 9, 18, 18}, kOnAccent, 0.0f, 1.5708f);
            ui::text(list, fonts_.bold, "Watch", a.rect.x + 70, a.rect.cy() + 11, 30, kOnAccent);
            break;
        }
        case Choice::Type::author:
            draw_chip(list, a.rect, fonts_.bold.font->fit("More from " + v.author, 26.0f, 420.0f), false);
            break;
        case Choice::Type::tag:
            if (!tags_label)
            {
                ui::text(list, fonts_.bold, "TAGS", kDetailX, 612 + shift, 18, kDim, Align::left, 3);
                tags_label = true;
            }
            draw_chip(list, a.rect, fonts_.regular.font->fit(a.value, 24.0f, 300.0f), false);
            break;
        }
    }

    // channels and comments
    y = 790.0f + shift;
    if (!page.details.channels.empty())
    {
        std::string channels = "Channels: ";
        for (std::size_t i = 0; i < page.details.channels.size(); ++i)
            channels += (i ? ", " : "") + page.details.channels[i];
        ui::text(list, fonts_.regular, fonts_.regular.font->fit(channels, 22.0f, kDetailW), kDetailX, y - 34, 22,
                 kMuted);
    }
    if (!page.details_loaded)
    {
        draw_spinner(list, kDetailX + 14, y + 10, 12);
        ui::text(list, fonts_.regular, "Loading comments...", kDetailX + 40, y + 18, 24, kMuted);
    }
    else
    {
        const int total = std::max(v.comment_count, static_cast<int>(page.details.comments.size()));
        ui::text(list, fonts_.bold, "COMMENTS (" + std::to_string(total) + ")", kDetailX, y + 14, 18, kDim,
                 Align::left, 3);
        if (page.details.comments.empty())
        {
            ui::text(list, fonts_.regular, total ? "Read them on KamTape." : "No comments yet.", kDetailX, y + 54,
                     24, kDim);
        }
        float cy = y + 56;
        for (std::size_t i = 0; i < page.details.comments.size() && i < 2; ++i)
        {
            const Comment &c = page.details.comments[i];
            std::string who = c.author;
            if (c.time > 0)
                who += "  \xC2\xB7  " + format_date(c.time);
            ui::text(list, fonts_.bold, fonts_.bold.font->fit(who, 22.0f, kDetailW), kDetailX, cy, 22, kAccentText);
            ui::text(list, fonts_.regular, fonts_.regular.font->fit(c.text, 24.0f, kDetailW), kDetailX, cy + 32, 24,
                     kText);
            cy += 74;
        }
    }
    list.pop_transform();
    list.pop_opacity();
}

void App::draw_footer(gfx::DrawList &list) const
{
    list.rounded_rect({96, 966, kRight - 96, 1.5f}, 0, kLine);
    const float cy = 1016;
    float x = 96;
    switch (screen())
    {
    case Screen::home:
        x = draw_hint(list, "cross", "Open", x, cy);
        x = draw_hint(list, "triangle", "Search", x, cy);
        draw_hint(list, "OPTIONS", "Settings", x, cy);
        break;
    case Screen::settings:
        x = draw_hint(list, "cross", settings_focus_ == 4 ? "Clear" : "Change", x, cy);
        draw_hint(list, "circle", "Back", x, cy);
        break;
    case Screen::videos:
    {
        const Page &page = pages_.back();
        if (!page.videos.empty())
        {
            x = draw_hint(list, "cross", "Details", x, cy);
            x = draw_hint(list, "square", "Watch", x, cy);
        }
        else if (!page.error.empty())
        {
            x = draw_hint(list, "cross", "Try again", x, cy);
        }
        x = draw_hint(list, "triangle", "Search", x, cy);
        draw_hint(list, "circle", "Back", x, cy);
        break;
    }
    case Screen::details:
        x = draw_hint(list, "cross", "Select", x, cy);
        x = draw_hint(list, "square", "Watch", x, cy);
        draw_hint(list, "circle", "Back", x, cy);
        break;
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
    list.bordered_rect(r, 28, kClear, 1.5f, status_error_ ? kError : kLine);
    list.circle(r.x + 30, r.cy(), 6, status_error_ ? kError : kAmber);
    ui::text(list, fonts_.regular, text, r.x + 48, r.cy() + 9, 26, kText);
    list.pop_opacity();
}

void App::draw(gfx::DrawList &list) const
{
    draw_background(list);
    draw_header(list);
    if (!pages_.empty())
    {
        const Page &page = pages_.back();
        if (page.screen == Screen::videos)
            draw_videos(list, page);
        else
            draw_details(list, page);
    }
    else if (screen_ == Screen::home)
    {
        draw_home(list);
    }
    else
    {
        draw_settings(list);
    }
    draw_footer(list);
    osk_.draw(list, fonts_.regular, fonts_.bold);
    draw_status(list);
}

} // namespace hui::kt
