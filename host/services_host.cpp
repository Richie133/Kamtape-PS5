// KamTape PS5 - PC stand-ins for the console services (snapshots, tests).
// The keyboard "types" the KT_KEYBOARD_TEXT environment variable.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/services.hpp"

#include <cstdio>
#include <cstdlib>

namespace hui::kt::services
{

namespace
{
bool g_open = false;
int g_frames = 0;
} // namespace

void init()
{
}

bool keyboard_open(const std::string &, const std::string &)
{
    if (std::getenv("KT_NO_IME")) // as on a PS5 where the system keyboard won't load
        return false;
    g_open = true;
    g_frames = 0;
    return true;
}

KeyboardState keyboard_poll(std::string *text)
{
    if (!g_open)
        return KeyboardState::idle;
    if (++g_frames < 3)
        return KeyboardState::open;
    g_open = false;
    const char *typed = std::getenv("KT_KEYBOARD_TEXT");
    if (typed == nullptr)
        return KeyboardState::cancelled;
    *text = typed;
    return KeyboardState::done;
}

bool open_browser(const std::string &url)
{
    std::fprintf(stderr, "[KT] would open the browser at %s\n", url.c_str());
    return true;
}

} // namespace hui::kt::services
