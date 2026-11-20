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
#include "kt/services.hpp"
#include "ui/fonts.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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
    {"right", 0.8f, "03-full-site"},
    {"type:watch?v=dQw4w9WgXcQ", 1.0f, "04-go-to-page"},
    {"type:kamtape.com/browse", 4.5f, nullptr},
    {"down", 0.2f, nullptr},
    {"down", 0.2f, nullptr},
    {"down", 0.8f, "05-recent-pages"},
    {"type:youtube.com/watch?v=abc", 1.0f, "06-other-site"},
    {"options", 4.5f, "07-settings"},
    {"down", 0.3f, nullptr},
    {"left", 0.8f, "08-settings-sounds"},
    {"circle", 1.2f, "09-back-home"},
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
                     output + "/settings.json");

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
        if (what.rfind("type:", 0) == 0)
        {
            setenv("KT_KEYBOARD_TEXT", what.c_str() + 5, 1);
            what = "triangle";
        }
        app.update(what.empty() ? hui::InputFrame{} : press(what), kDt);
        const int frames = static_cast<int>(step.settle * 60.0f);
        for (int i = 0; i < frames; ++i)
            app.update(hui::InputFrame{}, kDt);
        if (step.picture != nullptr)
            all = picture(step.picture) && all;
    }
    // skip static destructors: the audio thread may still be mixing
    std::fflush(stderr);
    std::_Exit(all ? 0 : 1);
}
