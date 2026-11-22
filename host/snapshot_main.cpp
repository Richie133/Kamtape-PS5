// KamTape PS5 - headless PC run of the app: scripted input, PNG pictures.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// usage: kt_snapshots <assets dir> <output dir>
//
// Runs the same App code as the console build through Mesa's surfaceless EGL
// (llvmpipe). The browser and the keyboard are the PC stand-ins in
// host/services_host.cpp: the keyboard "types" KT_KEYBOARD_TEXT.

#include "core/input.hpp"
#include "core/save_file.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/font.hpp"
#include "gfx/gl_program.hpp"
#include "gfx/image.hpp"
#include "gfx/renderer.hpp"
#include "kt/app.hpp"
#include "kt/audio.hpp"
#include "kt/hid.hpp"
#include "kt/services.hpp"
#include "ui/fonts.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <cstdlib>
#include <string>
#include <vector>

namespace hui::kt::hid
{
void hid_host_push(const Frame &f);
}

namespace
{

bool open_context()
{
    auto get_platform_display =
        reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    EGLDisplay display = get_platform_display != nullptr
                             ? get_platform_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
                             : eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0, minor = 0;
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor) || !eglBindAPI(EGL_OPENGL_API))
        return false;
    const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION,       4, EGL_CONTEXT_MINOR_VERSION, 5,
                                 EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
    return context != EGL_NO_CONTEXT && eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
}

bool load_font(hui::gfx::Renderer &renderer, const std::string &path, hui::gfx::Font *font, hui::ui::FontRef *ref)
{
    std::string data;
    if (!hui::save::read_file(path, &data) || !font->load(data))
    {
        std::fprintf(stderr, "cannot load font %s\n", path.c_str());
        return false;
    }
    ref->font = font;
    ref->texture = renderer.batch().create_font_texture(*font);
    return true;
}

// A step: a button press (or "type:text" through the keyboard), then `settle`
// seconds, then maybe a picture.
struct Step
{
    const char *press;
    float settle;
    const char *picture;
};

const Step kTour[] = {
    {"", 2.0f, "01-start-opens-xl"},
    {"", 4.0f, "02-home"},
    {"right", 0.8f, "03-featured-tile"},
    {"cross", 0.25f, "04-loading"},
    {"", 2.5f, "05-featured-videos"},
    {"right", 0.15f, nullptr},
    {"right", 0.15f, nullptr},
    {"down", 1.0f, "06-video-focused"},
    {"up", 0.15f, nullptr},
    {"cross", 2.0f, "07-details"},
    {"right", 0.6f, "08-details-author"},
    {"cross", 2.0f, "09-videos-by-author"},
    {"circle", 0.3f, nullptr},
    {"circle", 0.3f, nullptr},
    // the on-screen keyboard: open it, type "mu" with the controller, the rest on a USB keyboard
    {"triangle", 0.6f, "10-keyboard"},
    {"down", 0.1f, nullptr},  // from q to a
    {"down", 0.1f, nullptr},  // z
    {"right", 0.1f, nullptr}, // x
    {"right", 0.1f, nullptr}, // c
    {"right", 0.1f, nullptr}, // v
    {"right", 0.1f, nullptr}, // b
    {"right", 0.1f, nullptr}, // n
    {"right", 0.1f, nullptr}, // m
    {"cross", 0.1f, nullptr},
    {"usb:usic", 0.6f, "11-keyboard-typing"},
    {"options", 2.5f, "12-tag-search"},
    {"circle", 0.3f, nullptr},
    {"circle", 0.6f, nullptr},
    {"triangle", 0.6f, "13-keyboard-recent"},
    {"circle", 0.6f, nullptr},
    {"env:KT_FIXTURE_FAIL=1", 0.0f, nullptr},
    {"up", 0.2f, nullptr},
    {"cross", 2.0f, "14-api-error"},
    {"env:KT_FIXTURE_FAIL=", 0.0f, nullptr},
    {"circle", 0.6f, nullptr},
    {"down", 0.2f, nullptr},
    {"down", 0.2f, nullptr},
    {"cross", 0.6f, "15-keyboard-address"},
    {"usb:watch?v=dQw4w9WgXcQ\n", 4.5f, nullptr},
    {"options", 1.2f, "16-settings"},
    {"circle", 1.2f, "17-back-home"},
};

hui::InputFrame press(const std::string &n)
{
    using hui::Action;
    using hui::Direction;
    hui::InputFrame f;
    f.connected = true;
    const auto bit = [&](Action a) { f.pressed |= hui::action_bit(a); };
    if (n == "up")
        f.nav = Direction::up;
    else if (n == "down")
        f.nav = Direction::down;
    else if (n == "left")
        f.nav = Direction::left;
    else if (n == "right")
        f.nav = Direction::right;
    else if (n == "cross")
        bit(Action::confirm);
    else if (n == "circle")
        bit(Action::back);
    else if (n == "triangle")
        bit(Action::north);
    else if (n == "square")
        bit(Action::west);
    else if (n == "l1")
        bit(Action::page_prev);
    else if (n == "r1")
        bit(Action::page_next);
    else if (n == "options")
        bit(Action::menu);
    return f;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: %s <assets dir> <output dir>\n", argv[0]);
        return 2;
    }
    const std::string assets = argv[1];
    const std::string output = argv[2];
    const int width = 1920, height = 1080;
    if (!open_context())
    {
        std::fprintf(stderr, "no EGL context (Mesa surfaceless)\n");
        return 1;
    }
    hui::gfx::set_glsl_prefix("#version 450 core\n");
    hui::gfx::Renderer renderer;
    hui::gfx::Font regular, bold;
    hui::kt::Fonts fonts;
    if (!renderer.init() ||
        !load_font(renderer, assets + "/fonts/atkinson-regular.huifont", &regular, &fonts.regular) ||
        !load_font(renderer, assets + "/fonts/atkinson-bold.huifont", &bold, &fonts.bold))
        return 1;

    GLuint framebuffer = 0, color = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return 1;
    hui::save::ensure_directory(output);
    std::remove((output + "/settings.json").c_str());

    int logo_w = 0, logo_h = 0;
    const std::uint32_t logo = hui::gfx::load_png_texture(assets + "/logo.png", &logo_w, &logo_h);
    hui::kt::services::init();
    hui::kt::audio::init(assets);
    hui::kt::App app(fonts, logo, logo_h > 0 ? static_cast<float>(logo_w) / static_cast<float>(logo_h) : 1.0f,
                     output);

    hui::gfx::DrawList list;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    stbi_flip_vertically_on_write(1);
    constexpr float kDt = 1.0f / 60.0f;
    const auto picture = [&](const std::string &name) {
        list.clear();
        app.draw(list);
        renderer.begin();
        renderer.draw(list);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        renderer.present(framebuffer, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        for (std::size_t i = 3; i < pixels.size(); i += 4)
            pixels[i] = 255;
        const std::string path = output + "/" + name + ".png";
        const bool ok = stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4) != 0;
        std::fprintf(stderr, "wrote %s ok=%d\n", path.c_str(), ok ? 1 : 0);
        return ok;
    };

    bool all = true;
    for (const Step &step : kTour)
    {
        std::string what = step.press;
        if (what.rfind("env:", 0) == 0)
        {
            const std::string kv = what.substr(4);
            const std::size_t eq = kv.find('=');
            if (eq + 1 < kv.size())
                setenv(kv.substr(0, eq).c_str(), kv.substr(eq + 1).c_str(), 1);
            else
                unsetenv(kv.substr(0, eq).c_str());
            continue;
        }
        if (what.rfind("usb:", 0) == 0)
        {
            // typing on a USB keyboard; a trailing "\n" presses Enter
            hui::kt::hid::Frame typed;
            typed.keyboard = true;
            for (char c : what.substr(4))
            {
                if (c == '\n')
                    typed.pressed.push_back(hui::kt::hid::key::kEnter);
                else
                    typed.text.push_back(c);
            }
            hui::kt::hid::hid_host_push(typed);
            what.clear();
        }
        app.update(what.empty() ? hui::InputFrame{} : press(what), hui::kt::hid::read(), kDt);
        const int frames = static_cast<int>(step.settle * 60.0f);
        for (int i = 0; i < frames; ++i)
        {
            // real time passes too, so downloads on the worker threads can finish
            std::this_thread::sleep_for(std::chrono::microseconds(16667));
            app.update(hui::InputFrame{}, hui::kt::hid::read(), kDt);
        }
        if (step.picture != nullptr)
            all = picture(step.picture) && all;
    }
    // skip static destructors: the audio thread may still be mixing
    std::fflush(stderr);
    std::_Exit(all ? 0 : 1);
}
