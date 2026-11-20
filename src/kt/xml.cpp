// KamTape PS5 - a small XML reader for the KamTape API's answers.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/xml.hpp"

#include <cstdint>

namespace hui::kt
{

namespace
{

constexpr int kMaxDepth = 32;

bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_name_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
           c == '.' || c == ':';
}

std::string trim(std::string_view s)
{
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && is_space(s[b]))
        ++b;
    while (e > b && is_space(s[e - 1]))
        --e;
    return std::string(s.substr(b, e - b));
}

void append_utf8(std::string &out, std::uint32_t cp)
{
    if (cp == 0 || cp > 0x10ffff)
        cp = '?';
    if (cp < 0x80)
    {
        out += static_cast<char>(cp);
    }
    else if (cp < 0x800)
    {
        out += static_cast<char>(0xc0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    }
    else if (cp < 0x10000)
    {
        out += static_cast<char>(0xe0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    }
    else
    {
        out += static_cast<char>(0xf0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    }
}

// Appends raw text with entities decoded.
void append_decoded(std::string &out, std::string_view raw)
{
    for (std::size_t i = 0; i < raw.size();)
    {
        if (raw[i] != '&')
        {
            out += raw[i++];
            continue;
        }
        const std::size_t semi = raw.find(';', i + 1);
        if (semi == std::string_view::npos || semi - i > 10)
        {
            out += raw[i++]; // a bare '&'
            continue;
        }
        const std::string_view entity = raw.substr(i + 1, semi - i - 1);
        bool known = true;
        if (entity == "amp")
            out += '&';
        else if (entity == "lt")
            out += '<';
        else if (entity == "gt")
            out += '>';
        else if (entity == "quot")
            out += '"';
        else if (entity == "apos")
            out += '\'';
        else if (entity.size() > 1 && entity[0] == '#')
        {
            std::uint32_t cp = 0;
            const bool hex = entity[1] == 'x' || entity[1] == 'X';
            bool digits = entity.size() > (hex ? 2u : 1u);
            for (std::size_t k = hex ? 2 : 1; k < entity.size() && digits; ++k)
            {
                const char c = entity[k];
                int v = -1;
                if (c >= '0' && c <= '9')
                    v = c - '0';
                else if (hex && c >= 'a' && c <= 'f')
                    v = c - 'a' + 10;
                else if (hex && c >= 'A' && c <= 'F')
                    v = c - 'A' + 10;
                if (v < 0 || cp > 0x10ffff)
                    digits = false;
                else
                    cp = cp * (hex ? 16 : 10) + static_cast<std::uint32_t>(v);
            }
            if (digits)
                append_utf8(out, cp);
            else
                known = false;
        }
        else
        {
            known = false;
        }
        if (known)
        {
            i = semi + 1;
        }
        else
        {
            out += raw[i++];
        }
    }
}

class Parser
{
  public:
    explicit Parser(std::string_view text) : s_(text) {}

    std::unique_ptr<XmlNode> document()
    {
        skip_misc();
        if (at_ >= s_.size() || s_[at_] != '<')
            return nullptr;
        auto root = element(0);
        return ok_ ? std::move(root) : nullptr;
    }

  private:
    // Skips whitespace, comments, <?...?> and <!DOCTYPE ...> before the root.
    void skip_misc()
    {
        for (;;)
        {
            while (at_ < s_.size() && is_space(s_[at_]))
                ++at_;
            if (s_.compare(at_, 4, "<!--") == 0)
                skip_past("-->");
            else if (s_.compare(at_, 2, "<?") == 0)
                skip_past("?>");
            else if (s_.compare(at_, 2, "<!") == 0)
                skip_past(">");
            else
                return;
        }
    }

    void skip_past(std::string_view end)
    {
        const std::size_t found = s_.find(end, at_);
        at_ = found == std::string_view::npos ? s_.size() : found + end.size();
    }

    std::string name()
    {
        const std::size_t begin = at_;
        while (at_ < s_.size() && is_name_char(s_[at_]))
            ++at_;
        return std::string(s_.substr(begin, at_ - begin));
    }

    std::unique_ptr<XmlNode> element(int depth)
    {
        if (depth > kMaxDepth || at_ >= s_.size() || s_[at_] != '<')
        {
            ok_ = false;
            return nullptr;
        }
        ++at_;
        auto node = std::make_unique<XmlNode>();
        node->name = name();
        if (node->name.empty())
        {
            ok_ = false;
            return nullptr;
        }
        // attributes
        for (;;)
        {
            while (at_ < s_.size() && is_space(s_[at_]))
                ++at_;
            if (at_ >= s_.size())
            {
                ok_ = false;
                return nullptr;
            }
            if (s_[at_] == '/' && at_ + 1 < s_.size() && s_[at_ + 1] == '>')
            {
                at_ += 2;
                return node; // <empty/>
            }
            if (s_[at_] == '>')
            {
                ++at_;
                break;
            }
            std::string key = name();
            if (key.empty())
            {
                ok_ = false;
                return nullptr;
            }
            while (at_ < s_.size() && is_space(s_[at_]))
                ++at_;
            std::string value;
            if (at_ < s_.size() && s_[at_] == '=')
            {
                ++at_;
                while (at_ < s_.size() && is_space(s_[at_]))
                    ++at_;
                if (at_ < s_.size() && (s_[at_] == '"' || s_[at_] == '\''))
                {
                    const char quote = s_[at_++];
                    const std::size_t end = s_.find(quote, at_);
                    if (end == std::string_view::npos)
                    {
                        ok_ = false;
                        return nullptr;
                    }
                    append_decoded(value, s_.substr(at_, end - at_));
                    at_ = end + 1;
                }
            }
            node->attributes.emplace_back(std::move(key), std::move(value));
        }
        // content
        while (at_ < s_.size())
        {
            if (s_[at_] != '<')
            {
                const std::size_t end = s_.find('<', at_);
                const std::size_t stop = end == std::string_view::npos ? s_.size() : end;
                append_decoded(node->text, s_.substr(at_, stop - at_));
                at_ = stop;
                continue;
            }
            if (s_.compare(at_, 4, "<!--") == 0)
            {
                skip_past("-->");
            }
            else if (s_.compare(at_, 9, "<![CDATA[") == 0)
            {
                const std::size_t end = s_.find("]]>", at_ + 9);
                const std::size_t stop = end == std::string_view::npos ? s_.size() : end;
                node->text.append(s_.substr(at_ + 9, stop - at_ - 9));
                at_ = end == std::string_view::npos ? s_.size() : end + 3;
            }
            else if (s_.compare(at_, 2, "<?") == 0)
            {
                skip_past("?>");
            }
            else if (s_.compare(at_, 2, "</") == 0)
            {
                at_ += 2;
                const std::string closing = name();
                while (at_ < s_.size() && s_[at_] != '>')
                    ++at_;
                if (at_ < s_.size())
                    ++at_;
                if (closing != node->name)
                    ok_ = false;
                return node;
            }
            else
            {
                auto child = element(depth + 1);
                if (!ok_)
                    return nullptr;
                node->children.push_back(std::move(child));
            }
        }
        ok_ = false; // ran out before the closing tag
        return nullptr;
    }

    std::string_view s_;
    std::size_t at_ = 0;
    bool ok_ = true;
};

} // namespace

const XmlNode *XmlNode::child(std::string_view wanted) const
{
    for (const auto &c : children)
    {
        if (c->name == wanted)
            return c.get();
    }
    return nullptr;
}

std::string XmlNode::child_text(std::string_view wanted) const
{
    const XmlNode *c = child(wanted);
    return c ? trim(c->text) : std::string();
}

std::string XmlNode::attribute(std::string_view wanted) const
{
    for (const auto &a : attributes)
    {
        if (a.first == wanted)
            return a.second;
    }
    return {};
}

std::unique_ptr<XmlNode> parse_xml(std::string_view text)
{
    return Parser(text).document();
}

} // namespace hui::kt
