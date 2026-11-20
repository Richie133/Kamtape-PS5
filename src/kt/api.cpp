// KamTape PS5 - the KamTape developer API: request URLs and answers.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/api.hpp"

#include "core/save_file.hpp"
#include "kt/config.hpp"
#include "kt/http.hpp"
#include "kt/json.hpp"
#include "kt/xml.hpp"

#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace hui::kt
{

namespace
{

std::mutex g_config_lock;
std::string g_base = config::kApiBase;
std::string g_dev_id = config::kDevId;

std::string request(const char *method, const std::string &extra)
{
    std::string url = api_base();
    url += url.find('?') == std::string::npos ? '?' : '&';
    url += "dev_id=" + http::escape(api_dev_id());
    url += "&method=";
    url += method;
    url += extra;
    return url;
}

long long to_ll(const std::string &s)
{
    return s.empty() ? 0 : std::strtoll(s.c_str(), nullptr, 10);
}

double to_double(const std::string &s)
{
    // no strtod: the console's libc lacks the locale functions behind it
    double value = 0.0;
    double scale = 0.0;
    bool negative = false;
    for (char c : s)
    {
        if (c == '-' && value == 0.0 && scale == 0.0)
            negative = true;
        else if (c == '.' && scale == 0.0)
            scale = 1.0;
        else if (c >= '0' && c <= '9')
        {
            if (scale == 0.0)
                value = value * 10.0 + (c - '0');
            else
                value += (c - '0') * (scale *= 0.1);
        }
        else
            break;
    }
    return negative ? -value : value;
}

void read_video(const XmlNode &n, Video *v)
{
    v->id = n.child_text("id");
    v->author = n.child_text("author");
    v->title = n.child_text("title");
    v->description = n.child_text("description");
    v->tags = n.child_text("tags");
    v->url = n.child_text("url");
    v->thumbnail_url = n.child_text("thumbnail_url");
    v->length_seconds = static_cast<int>(to_ll(n.child_text("length_seconds")));
    v->rating_avg = to_double(n.child_text("rating_avg"));
    v->rating_count = static_cast<int>(to_ll(n.child_text("rating_count")));
    v->view_count = to_ll(n.child_text("view_count"));
    v->upload_time = to_ll(n.child_text("upload_time"));
    v->comment_count = static_cast<int>(to_ll(n.child_text("comment_count")));
}

// The <video_list> or <video_details> in an answer, unwrapped from
// <ut_response>; error says why when there is none.
const XmlNode *payload(const XmlNode *root, const char *wanted, ApiError *error)
{
    if (root == nullptr)
    {
        error->failed = true;
        error->message = "KamTape sent something that isn't the API's XML.";
        return nullptr;
    }
    if (root->name == wanted)
        return root;
    if (root->name == "ut_response")
    {
        if (root->attribute("status") != "ok")
        {
            error->failed = true;
            const XmlNode *e = root->child("error");
            const std::string description = e ? e->child_text("description") : std::string();
            const std::string code = e ? e->child_text("code") : std::string();
            error->message = description.empty() ? "KamTape's API refused the request." : "KamTape: " + description;
            if (!code.empty())
                error->message += " (code " + code + ")";
            return nullptr;
        }
        if (const XmlNode *inner = root->child(wanted))
            return inner;
    }
    error->failed = true;
    error->message = "KamTape's answer had no " + std::string(wanted) + ".";
    return nullptr;
}

} // namespace

void api_configure(const std::string &directory)
{
    std::string text;
    JsonValue root;
    if (!save::read_file(directory + "/api.json", &text, 16u << 10) || !parse_json(text, root) || !root.is_object())
        return;
    std::lock_guard<std::mutex> lock(g_config_lock);
    if (root["base"].is_string() && !root["base"].as_string().empty())
        g_base = root["base"].as_string();
    if (root["devId"].is_string())
        g_dev_id = root["devId"].as_string();
}

std::string api_base()
{
    std::lock_guard<std::mutex> lock(g_config_lock);
    return g_base;
}

std::string api_dev_id()
{
    std::lock_guard<std::mutex> lock(g_config_lock);
    return g_dev_id;
}

std::string url_featured()
{
    return request("kamtape.videos.list_featured", "");
}

std::string url_by_tag(const std::string &tag, int page, int per_page)
{
    return request("kamtape.videos.list_by_tag", "&tag=" + http::escape(tag) + "&page=" + std::to_string(page) +
                                                     "&per_page=" + std::to_string(per_page));
}

std::string url_by_user(const std::string &user)
{
    return request("kamtape.videos.list_by_user", "&user=" + http::escape(user));
}

std::string url_details(const std::string &video_id)
{
    return request("kamtape.videos.get_details", "&video_id=" + http::escape(video_id));
}

ApiError parse_video_list(const std::string &xml, std::vector<Video> *videos)
{
    ApiError error;
    const auto root = parse_xml(xml);
    const XmlNode *list = payload(root.get(), "video_list", &error);
    if (list == nullptr)
        return error;
    for (const auto &child : list->children)
    {
        if (child->name != "video")
            continue;
        Video v;
        read_video(*child, &v);
        if (!v.id.empty() || !v.url.empty())
            videos->push_back(std::move(v));
    }
    return error;
}

ApiError parse_video_details(const std::string &xml, const std::string &video_id, VideoDetails *details)
{
    ApiError error;
    const auto root = parse_xml(xml);
    const XmlNode *d = payload(root.get(), "video_details", &error);
    if (d == nullptr)
        return error;
    read_video(*d, &details->video);
    details->video.id = video_id;
    details->update_time = to_ll(d->child_text("update_time"));
    if (const XmlNode *comments = d->child("comment_list"))
    {
        for (const auto &c : comments->children)
        {
            if (c->name != "comment")
                continue;
            details->comments.push_back({c->child_text("author"), c->child_text("text"), to_ll(c->child_text("time"))});
        }
    }
    if (const XmlNode *channels = d->child("channel_list"))
    {
        for (const auto &c : channels->children)
        {
            if (c->name != "channel")
                continue;
            std::string name = c->text;
            while (!name.empty() && (name.back() == ' ' || name.back() == '\n' || name.back() == '\t'))
                name.pop_back();
            std::size_t b = 0;
            while (b < name.size() && (name[b] == ' ' || name[b] == '\n' || name[b] == '\t'))
                ++b;
            details->channels.push_back(name.substr(b));
        }
    }
    return error;
}

std::string watch_url(const Video &video)
{
    if (!video.url.empty())
        return video.url;
    return "https://www.kamtape.com/watch?v=" + http::escape(video.id);
}

std::vector<std::string> split_tags(const std::string &tags)
{
    std::vector<std::string> out;
    std::string word;
    for (char c : tags + " ")
    {
        if (c == ' ' || c == ',' || c == '\t' || c == '\n')
        {
            if (!word.empty())
                out.push_back(word);
            word.clear();
        }
        else
        {
            word += c;
        }
    }
    return out;
}

std::string format_length(int seconds)
{
    if (seconds < 0)
        seconds = 0;
    char buf[32];
    if (seconds >= 3600)
        std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60);
    else
        std::snprintf(buf, sizeof(buf), "%d:%02d", seconds / 60, seconds % 60);
    return buf;
}

std::string format_count(long long value)
{
    if (value < 0)
        value = 0;
    std::string digits = std::to_string(value);
    std::string out;
    const int n = static_cast<int>(digits.size());
    for (int i = 0; i < n; ++i)
    {
        out += digits[static_cast<std::size_t>(i)];
        if ((n - i - 1) % 3 == 0 && i != n - 1)
            out += ',';
    }
    return out;
}

std::string format_date(long long unix_time)
{
    if (unix_time <= 0)
        return {};
    // days to civil date (Howard Hinnant's algorithm)
    long long z = unix_time / 86400 + 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = yoe + era * 400;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    const long long d = doy - (153 * mp + 2) / 5 + 1;
    const long long m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2)
        ++y;
    static const char *const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s %lld, %lld", kMonths[m - 1], d, y);
    return buf;
}

} // namespace hui::kt
