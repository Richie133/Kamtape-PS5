// KamTape PS5 - plain HTTP(S) GET for the KamTape API and thumbnails.
// src/platform/ps5/http_curl.cpp on the console (libcurl), host/http_host.cpp
// on a PC. Blocking: call it from a worker thread, never from the frame.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string>

namespace hui::kt::http
{

struct Response
{
    long status = 0;   // HTTP status, 0 when the request never got an answer
    std::string body;  // at most max_bytes
    std::string error; // why it failed, empty on success
    bool ok() const { return error.empty() && status >= 200 && status < 300; }
};

Response get(const std::string &url, std::size_t max_bytes = 2u << 20);

// "a b&c" -> "a%20b%26c", for query values.
std::string escape(const std::string &value);

} // namespace hui::kt::http
