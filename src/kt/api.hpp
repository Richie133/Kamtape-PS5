// KamTape PS5 - the KamTape developer API (kamtape.com/dev): request URLs and
// what the answers hold.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// KamTape's API follows YouTube's 2005 REST API method for method
// (kamtape.videos.list_featured, list_by_tag, list_by_user, get_details).
// Requests are GETs to kApiBase with method=..., dev_id=... and the method's
// own parameters; answers are XML, a <video_list> or <video_details>, usually
// inside <ut_response status="ok">. A failure is
// <ut_response status="fail"><error><code/><description/></error>.

#pragma once

#include <string>
#include <vector>

namespace hui::kt
{

// The REST endpoint and the developer ID sent with every call (kt/config.hpp),
// unless <directory>/api.json replaces them; api_configure reads that file.
void api_configure(const std::string &directory);
std::string api_base();
std::string api_dev_id();

struct Video
{
    std::string id;
    std::string author;
    std::string title;
    std::string description;
    std::string tags; // space-separated
    std::string url;  // the watch page
    std::string thumbnail_url;
    int length_seconds = 0;
    double rating_avg = 0.0;
    int rating_count = 0;
    long long view_count = 0;
    long long upload_time = 0; // UNIX time
    int comment_count = 0;
};

struct Comment
{
    std::string author;
    std::string text;
    long long time = 0;
};

struct VideoDetails
{
    Video video; // id is the one it was asked for
    long long update_time = 0;
    std::vector<Comment> comments;
    std::vector<std::string> channels;
};

struct ApiError
{
    bool failed = false;
    std::string message; // for the screen
};

std::string url_featured();
std::string url_by_tag(const std::string &tag, int page, int per_page);
std::string url_by_user(const std::string &user);
std::string url_details(const std::string &video_id);

// Reads an answer. On failure the error says why (the API's own description
// when it sent one).
ApiError parse_video_list(const std::string &xml, std::vector<Video> *videos);
ApiError parse_video_details(const std::string &xml, const std::string &video_id, VideoDetails *details);

// The watch page of a video: its url, or one made from its id.
std::string watch_url(const Video &video);
std::vector<std::string> split_tags(const std::string &tags);

// "2:05", "1:02:05".
std::string format_length(int seconds);
// "1,234,567".
std::string format_count(long long value);
// "Jul 14, 2005" (UTC).
std::string format_date(long long unix_time);

} // namespace hui::kt
