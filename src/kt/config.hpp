// KamTape PS5 - where the KamTape API lives and the developer ID the app uses.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Both can be changed without rebuilding: a file
// /download0/kamtape/api.json on the console (or the settings folder on a PC)
// with {"base": "...", "devId": "..."} takes their place.

#pragma once

namespace hui::kt::config
{

// The REST interface, as YouTube's 2005 API had it at /api2_rest.
constexpr const char *kApiBase = "http://www.kamtape.com/api2_rest";

// The developer ID from kamtape.com/dev. Empty: the parameter is still sent,
// and the API's answer says whether it needs one.
constexpr const char *kDevId = "";

} // namespace hui::kt::config
