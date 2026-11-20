// KamTape PS5 - turns what the user typed into a KamTape address.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/address.hpp"

#include <cctype>

namespace hui::kt
{

namespace
{

bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// value starts with prefix, ignoring ASCII case.
bool starts_with_nocase(std::string_view value, std::string_view prefix)
{
    if (value.size() < prefix.size())
        return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
    {
        if (lower(value[i]) != lower(prefix[i]))
            return false;
    }
    return true;
}

// The host part ends at the first '/', '?', '#' or the end.
std::size_t host_end(std::string_view rest)
{
    std::size_t i = 0;
    while (i < rest.size() && rest[i] != '/' && rest[i] != '?' && rest[i] != '#')
        ++i;
    return i;
}

bool is_kamtape_host(std::string_view host)
{
    // a port or user part is not something people type here: keep it exact
    std::string h;
    for (char c : host)
        h += lower(c);
    return h == "kamtape.com" || h == "www.kamtape.com";
}

std::string with_path(std::string_view path)
{
    std::string url = kOrigin;
    if (path.empty() || (path[0] != '/'))
        url += '/';
    url.append(path.data(), path.size());
    return url;
}

} // namespace

Address normalize_address(std::string_view typed)
{
    Address result;
    std::size_t begin = 0;
    std::size_t end = typed.size();
    while (begin < end && is_space(typed[begin]))
        ++begin;
    while (end > begin && is_space(typed[end - 1]))
        --end;
    std::string_view text = typed.substr(begin, end - begin);
    if (text.empty())
    {
        result.error = AddressError::empty;
        return result;
    }
    for (char c : text)
    {
        if (is_space(c))
        {
            result.error = AddressError::spaces;
            return result;
        }
    }

    bool had_scheme = false;
    for (std::string_view scheme : {std::string_view("https://"), std::string_view("http://")})
    {
        if (starts_with_nocase(text, scheme))
        {
            text.remove_prefix(scheme.size());
            had_scheme = true;
            break;
        }
    }

    const std::size_t split = host_end(text);
    const std::string_view first = text.substr(0, split);
    if (is_kamtape_host(first))
    {
        result.url = with_path(text.substr(split));
        return result;
    }
    // Anything else with a scheme, or whose first part has a dot (a domain),
    // is another site. Bare paths ("xl/", "watch?v=...") have no dot before
    // the first '/', '?' or '#'.
    const bool looks_like_domain = first.find('.') != std::string_view::npos;
    if (had_scheme || looks_like_domain)
    {
        result.error = AddressError::other_site;
        return result;
    }
    result.url = with_path(text);
    return result;
}

std::string short_address(std::string_view url)
{
    std::string_view rest = url;
    for (std::string_view scheme : {std::string_view("https://"), std::string_view("http://")})
    {
        if (starts_with_nocase(rest, scheme))
        {
            rest.remove_prefix(scheme.size());
            break;
        }
    }
    if (starts_with_nocase(rest, "www."))
        rest.remove_prefix(4);
    return std::string(rest);
}

} // namespace hui::kt
