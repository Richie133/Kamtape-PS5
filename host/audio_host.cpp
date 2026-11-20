// KamTape PS5 - PC stand-in for audio output: keeps time, plays nothing.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/audio.hpp"

#include <chrono>
#include <thread>

namespace hui::kt::audio_out
{

bool open() { return true; }

void write(const short *)
{
    std::this_thread::sleep_for(std::chrono::microseconds(256 * 1000000 / 48000));
}

} // namespace hui::kt::audio_out
