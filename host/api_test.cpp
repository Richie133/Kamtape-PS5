// KamTape PS5 - tests for the XML reader and the API answers (make test).
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "kt/api.hpp"
#include "kt/xml.hpp"

#include <cstdio>
#include <string>

namespace
{

int failures = 0;

void check(bool ok, const char *what)
{
    if (!ok)
    {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

std::string fixture(const char *dir, const char *name)
{
    std::string text;
    hui::save::read_file(std::string(dir) + "/" + name, &text);
    return text;
}

} // namespace

int main(int argc, char **argv)
{
    using namespace hui::kt;
    const char *dir = argc > 1 ? argv[1] : "host/fixtures";

    // the reader
    {
        auto root = parse_xml("<?xml version=\"1.0\"?>\n<!-- c --><a x='1' y=\"two &amp; three\"><b>hi &lt;there&gt; &#65;&#x42;</b>"
                              "<c/><d><![CDATA[<raw>]]></d><e>Odd & Outrageous</e></a>");
        check(root != nullptr, "parses a document");
        if (root)
        {
            check(root->name == "a" && root->attribute("x") == "1", "attributes");
            check(root->attribute("y") == "two & three", "entities in attributes");
            check(root->child_text("b") == "hi <there> AB", "entities in text");
            check(root->child("c") != nullptr && root->child_text("c").empty(), "empty element");
            check(root->child_text("d") == "<raw>", "CDATA");
            check(root->child_text("e") == "Odd & Outrageous", "a bare ampersand");
        }
        check(parse_xml("\xEF\xBB\xBF<a>bom</a>") != nullptr, "a byte-order mark");
        check(parse_xml("<a><b></a>") == nullptr, "mismatched tags fail");
        check(parse_xml("not xml") == nullptr, "text fails");
        check(parse_xml("<a>") == nullptr, "unclosed fails");
    }

    // the API's answers, with and without the <ut_response> wrapper
    {
        std::vector<Video> videos;
        const ApiError e = parse_video_list(fixture(dir, "featured.xml"), &videos);
        check(!e.failed && videos.size() == 12, "featured list");
        if (!videos.empty())
        {
            check(videos[0].id == "vhs01aaaaaa" && videos[0].length_seconds == 132, "video fields");
            check(videos[0].rating_avg > 2.9 && videos[0].rating_avg < 5.1, "rating");
            check(videos[0].thumbnail_url == "http://www.kamtape.com/get_still?video_id=vhs01aaaaaa", "thumbnail url");
        }
        std::vector<Video> bare;
        const ApiError e2 = parse_video_list(
            "<video_list><video><author>a</author><id>x1</id><title>T</title><url>http://www.kamtape.com/watch?v=x1</url>"
            "<view_count>170</view_count></video></video_list>",
            &bare);
        check(!e2.failed && bare.size() == 1 && bare[0].view_count == 170, "bare video_list");
        std::vector<Video> none;
        const ApiError e3 = parse_video_list(fixture(dir, "fail.xml"), &none);
        check(e3.failed && e3.message.find("dev_id") != std::string::npos, "API error message");
        check(parse_video_list("<html>oops</html>", &none).failed, "not the API's XML");
        std::vector<Video> nostatus;
        check(!parse_video_list("<ut_response><video_list><video><id>a</id></video></video_list></ut_response>",
                                &nostatus)
                       .failed &&
                  nostatus.size() == 1,
              "ut_response without a status");
    }
    {
        VideoDetails d;
        const ApiError e = parse_video_details(fixture(dir, "details.xml"), "vhs07gggggg", &d);
        check(!e.failed, "details parse");
        check(d.video.id == "vhs07gggggg" && d.video.author == "dvdlad", "details fields");
        check(d.comments.size() == 2 && d.comments[1].text == "we had the exact same tv & remote!!", "comments");
        check(d.channels.size() == 2 && d.channels[1] == "Odd & Outrageous", "channels");
    }

    // requests and formatting
    check(url_featured().find("method=kamtape.videos.list_featured") != std::string::npos, "featured url");
    check(url_by_tag("cute cats", 2, 20).find("&tag=cute%20cats&page=2&per_page=20") != std::string::npos, "tag url");
    check(url_details("a b").find("&video_id=a%20b") != std::string::npos, "details url");
    check(format_length(16) == "0:16" && format_length(3725) == "1:02:05", "length");
    check(format_count(1234567) == "1,234,567" && format_count(12) == "12", "count");
    check(format_date(1121398533) == "Jul 15, 2005", "date");
    check(split_tags("feature  film documentary").size() == 3, "tags");

    std::printf(failures ? "%d failure(s)\n" : "all API tests passed\n", failures);
    return failures ? 1 : 0;
}
