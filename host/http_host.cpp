// KamTape PS5 - kt::http::get on a PC: answers from host/fixtures instead of
// the network, so the snapshots and tests show the screens with made-up
// videos. KT_FIXTURES names the folder; KT_FIXTURE_FAIL=1 answers every API
// call with an API error.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/http.hpp"

#include "core/save_file.hpp"

#include <chrono>
#include <cstdlib>
#include <thread>

namespace hui::kt::http
{

Response get(const std::string &url, std::size_t max_bytes)
{
    Response response;
    const char *dir = std::getenv("KT_FIXTURES");
    if (dir == nullptr)
    {
        response.error = "no network in the PC build (set KT_FIXTURES)";
        return response;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150)); // a little like the internet
    std::string file;
    const std::size_t still = url.find("get_still?video_id=");
    if (still != std::string::npos)
        file = "thumbs/" + url.substr(still + 19) + ".jpg";
    else if (url.rfind("https://api.github.com/repos/", 0) == 0)
        file = std::getenv("KT_NO_UPDATE") != nullptr ? "releases-none.json" : "releases.json";
    else if (std::getenv("KT_FIXTURE_FAIL") != nullptr && url.find("method=") != std::string::npos)
        file = "fail.xml";
    else if (url.find("method=kamtape.videos.list_featured") != std::string::npos)
        file = "featured.xml";
    else if (url.find("method=kamtape.videos.list_by_tag") != std::string::npos)
        file = "by_tag.xml";
    else if (url.find("method=kamtape.videos.list_by_user") != std::string::npos)
        file = "by_user.xml";
    else if (url.find("method=kamtape.videos.get_details") != std::string::npos)
        file = "details.xml";
    if (file.empty() || !save::read_file(std::string(dir) + "/" + file, &response.body, max_bytes))
    {
        response.status = 404;
        response.error = "HTTP 404";
        return response;
    }
    response.status = 200;
    return response;
}

} // namespace hui::kt::http
