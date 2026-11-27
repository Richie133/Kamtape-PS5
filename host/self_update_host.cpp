// KamTape PS5 - the self-update kit's console layer on a PC: the "download"
// is host/fixtures/update.zip and the helper is a stand-in that speaks the
// same protocol (ready, progress, staged, applying) without touching files.
// KT_UPDATE_NO_LOADER=1: no payload loader answers, as on a console without one.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "third_party/update_check/self_update.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace
{

struct FakeHelper
{
    std::mutex lock;
    std::string out;       // what the helper says next
    bool archive_done = false;
    bool apply = false;
    bool closed = false;
    int stage = 0;
};

FakeHelper g_helper;

long helper_send(void *, const void *data, std::size_t size)
{
    std::lock_guard<std::mutex> guard(g_helper.lock);
    const auto *bytes = static_cast<const unsigned char *>(data);
    if (size == 4 && bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0)
        g_helper.archive_done = true; // the end mark
    if (size == 6 && std::memcmp(data, "apply\n", 6) == 0)
        g_helper.apply = true;
    return static_cast<long>(size);
}

long helper_receive(void *, void *data, std::size_t size)
{
    for (int waited = 0; waited < 6000; ++waited)
    {
        {
            std::lock_guard<std::mutex> guard(g_helper.lock);
            if (g_helper.closed)
                return 0;
            if (g_helper.out.empty())
            {
                // the next thing the helper would say, when its time has come
                if (g_helper.stage == 0)
                {
                    g_helper.out = "ready\n";
                    g_helper.stage = 1;
                }
                else if (g_helper.stage == 1 && g_helper.archive_done)
                {
                    g_helper.out = "p 150000 300000\n";
                    g_helper.stage = 2;
                }
                else if (g_helper.stage == 2)
                {
                    g_helper.out = "p 300000 300000\n";
                    g_helper.stage = 3;
                }
                else if (g_helper.stage == 3)
                {
                    g_helper.out = "staged\n";
                    g_helper.stage = 4;
                }
                else if (g_helper.stage == 4 && g_helper.apply)
                {
                    g_helper.out = "applying\n";
                    g_helper.stage = 5;
                }
            }
            if (!g_helper.out.empty())
            {
                const std::size_t n = size < g_helper.out.size() ? size : g_helper.out.size();
                std::memcpy(data, g_helper.out.data(), n);
                g_helper.out.erase(0, n);
                return static_cast<long>(n);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(g_helper.stage >= 2 ? 100 : 10));
    }
    return 0; // silence: the helper is gone
}

void helper_close(void *)
{
    std::lock_guard<std::mutex> guard(g_helper.lock);
    g_helper.closed = true;
}

int host_fetch(void *, const char *, char *, std::size_t, std::size_t *length, int *status)
{
    *length = 0;
    *status = 0;
    return -1;
}

int host_download(void *, const char *url, std::uint64_t limit, self_update_sink sink, void *sink_user)
{
    if (!self_update_url_allowed(url, 0))
        return -1;
    const char *dir = std::getenv("KT_FIXTURES");
    std::string zip;
    if (dir == nullptr || !hui::save::read_file(std::string(dir) + "/update.zip", &zip, 64u << 20) || zip.size() > limit)
        return -1;
    const std::size_t piece = 16384;
    for (std::size_t at = 0; at < zip.size(); at += piece)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25)); // a slow connection, so progress shows
        const std::size_t n = zip.size() - at < piece ? zip.size() - at : piece;
        if (sink(sink_user, zip.data() + at, n) != 1)
            return -1;
    }
    return 0;
}

int host_verify(void *, const unsigned char *, const unsigned char *, const void *, std::size_t)
{
    return 0;
}

int host_open_helper(void *, self_update_channel *channel)
{
    if (std::getenv("KT_UPDATE_NO_LOADER") != nullptr)
        return 0;
    std::lock_guard<std::mutex> guard(g_helper.lock);
    g_helper.out.clear();
    g_helper.archive_done = g_helper.apply = g_helper.closed = false;
    g_helper.stage = 0;
    channel->send = helper_send;
    channel->receive = helper_receive;
    channel->close = helper_close;
    channel->user = nullptr;
    return 1;
}

int host_load_sequence(void *, std::uint64_t *)
{
    return 0;
}

int host_save_sequence(void *, std::uint64_t)
{
    return 0;
}

std::uint64_t host_now_ms(void *)
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

} // namespace

extern "C" const self_update_platform *self_update_console(void)
{
    static const self_update_platform platform = {host_fetch,         host_download,      host_verify, host_open_helper,
                                                  host_load_sequence, host_save_sequence, host_now_ms, nullptr};
    return &platform;
}
