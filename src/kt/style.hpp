// KamTape PS5 - the colours every screen shares.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gfx/draw_list.hpp"

namespace hui::kt::style
{

inline const gfx::Color kBgTop = gfx::Color::rgb(0x1c1f29);
inline const gfx::Color kBgBottom = gfx::Color::rgb(0x0b0c10);
inline const gfx::Color kPanel = gfx::Color::rgb(0x191c24);
inline const gfx::Color kPanelHi = gfx::Color::rgb(0x232733);
inline const gfx::Color kLine = gfx::Color::rgb(0x2e3341);
inline const gfx::Color kBadge = gfx::Color::rgb(0x272b37);
inline const gfx::Color kText = gfx::Color::rgb(0xf4efe6);
inline const gfx::Color kMuted = gfx::Color::rgb(0xa6aab6);
inline const gfx::Color kDim = gfx::Color::rgb(0x6f7383);
inline const gfx::Color kAccent = gfx::Color::rgb(0x6fdc3a); // KamTape green
inline const gfx::Color kAccentText = gfx::Color::rgb(0xb6f19a);
inline const gfx::Color kOnAccent = gfx::Color::rgb(0x0f2108);
inline const gfx::Color kAmber = gfx::Color::rgb(0xffb547);
inline const gfx::Color kError = gfx::Color::rgb(0xff6b5a);
inline const gfx::Color kClear{0.0f, 0.0f, 0.0f, 0.0f};

} // namespace hui::kt::style
