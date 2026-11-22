// KamTape PS5 - application entry point.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Opens the display, controller and audio, then every frame: read input,
// update, draw, present.

#include "core/input.hpp"
#include "core/save_file.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/font.hpp"
#include "gfx/image.hpp"
#include "gfx/renderer.hpp"
#include "kt/app.hpp"
#include "kt/audio.hpp"
#include "kt/hid.hpp"
#include "kt/services.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/system.hpp"
#include "ui/fonts.hpp"

#include <GL/glcorearb.h>

#include <cstdint>
#include <span>
#include <string>

namespace
{

constexpr const char *kAssets = "/app0/assets";
// A title's sandbox has no /data: settings live in its own storage.
constexpr const char *kSettingsDir = "/download0/kamtape";

bool load_font(hui::gfx::Renderer &renderer, const char *name, hui::gfx::Font *font, hui::ui::FontRef *ref)
{
    std::string data;
    const std::string path = std::string(kAssets) + "/fonts/" + name;
    if (!hui::save::read_file(path, &data) || !font->load(data))
    {
        hui::sys::log("[KT] font %s failed: %s", name, font->error().c_str());
        return false;
    }
    ref->font = font;
    ref->texture = renderer.batch().create_font_texture(*font);
    return true;
}

} // namespace

int main()
{
    using namespace hui;
    sys::log("[KT] entry");

    ps5::Display display;
    if (!display.open(1920, 1080))
    {
        sys::log("[KT] fatal: display open failed");
        sys::park();
    }

    gfx::Renderer renderer;
    gfx::Font regular;
    gfx::Font bold;
    kt::Fonts fonts;
    if (!renderer.init() || !load_font(renderer, "atkinson-regular.huifont", &regular, &fonts.regular) ||
        !load_font(renderer, "atkinson-bold.huifont", &bold, &fonts.bold))
    {
        sys::log("[KT] fatal: renderer init failed");
        sys::park();
    }

    int logo_w = 0;
    int logo_h = 0;
    const std::uint32_t logo = gfx::load_png_texture(std::string(kAssets) + "/logo.png", &logo_w, &logo_h);
    sys::log("[KT] logo %ux%d", static_cast<unsigned>(logo_w), logo_h);

    ps5::Pad pad;
    pad.open();
    InputTracker tracker;
    kt::services::init();
    kt::audio::init(kAssets);

    kt::App app(fonts, logo, logo_h > 0 ? static_cast<float>(logo_w) / static_cast<float>(logo_h) : 1.0f,
                kSettingsDir);

    gfx::DrawList list;
    bool usb_open = false;
    PadSample samples[64];
    std::uint64_t frames = 0;
    std::int64_t last = sys::monotonic_us();
    for (;;)
    {
        const std::int64_t now = sys::monotonic_us();
        float dt = frames == 0 ? 1.0f / 60.0f : static_cast<float>(now - last) / 1e6f;
        last = now;
        if (dt > 0.05f)
            dt = 0.05f;
        const std::size_t count = pad.read(samples);
        const InputFrame input =
            tracker.update(std::span<const PadSample>(samples, count), static_cast<std::uint64_t>(now));

        const kt::hid::Frame usb = usb_open ? kt::hid::read() : kt::hid::Frame{};
        app.update(input, usb, dt);

        list.clear();
        app.draw(list);
        renderer.begin();
        renderer.draw(list);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        renderer.present(0, display.width(), display.height());
        if (!display.swap())
        {
            sys::log("[KT] fatal: swap failed frame=%llu error=%s", static_cast<unsigned long long>(frames),
                     ps5::egl_error_name(display.last_error()));
            sys::park();
        }
        if (++frames == 1)
        {
            const bool hidden = sys::hide_splash_screen();
            sys::log("[KT] ready splash_hidden=%d", hidden ? 1 : 0);
        }
        else if (frames == 30 && app.begin_usb())
        {
            // after the first frames, so a crash here can't stop the app from showing
            usb_open = kt::hid::open(pad.user());
            app.end_usb(usb_open);
            sys::log("[KT] usb keyboard %s", usb_open ? "open" : "unavailable");
        }
    }
}
