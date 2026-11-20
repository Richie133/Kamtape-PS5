// KamTape PS5 - kt::http::get on the console: PacBrew's libcurl with the
// console stand-ins in console_curl.c (no signals, certificate list,
// non-blocking sockets, DNS). Runs on worker threads.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/http.hpp"

#include "console_curl.h"
#include "platform/ps5/system.hpp"

#include <curl/curl.h>

namespace hui::kt::http
{

namespace
{

void ensure_global()
{
    static const bool ready = [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        return true;
    }();
    (void)ready;
}

struct Sink
{
    std::string *body;
    std::size_t limit;
};

std::size_t on_write(char *data, std::size_t size, std::size_t count, void *user)
{
    Sink *sink = static_cast<Sink *>(user);
    const std::size_t bytes = size * count;
    const std::size_t used = sink->body->size();
    const std::size_t room = sink->limit > used ? sink->limit - used : 0;
    sink->body->append(data, bytes < room ? bytes : room);
    return bytes;
}

} // namespace

Response get(const std::string &url, std::size_t max_bytes)
{
    ensure_global();
    Response response;
    CURL *easy = curl_easy_init();
    if (easy == nullptr)
    {
        response.error = "no network library";
        return response;
    }
    console_curl_setup(easy);
    Sink sink{&response.body, max_bytes};
    char message[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT, 25L);
    curl_easy_setopt(easy, CURLOPT_USERAGENT, "KamTapePS5/0.2.0 (PlayStation 5 homebrew; github.com/Richie133/Kamtape-PS5)");
    curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, on_write);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, message);
    const CURLcode code = curl_easy_perform(easy);
    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &response.status);
    if (code != CURLE_OK)
    {
        response.error = message[0] ? message : curl_easy_strerror(code);
        sys::log("[KT] http %s: %s", url.c_str(), response.error.c_str());
    }
    else if (response.status < 200 || response.status >= 300)
    {
        response.error = "HTTP " + std::to_string(response.status);
    }
    curl_easy_cleanup(easy);
    return response;
}

} // namespace hui::kt::http
