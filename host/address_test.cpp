// KamTape PS5 - tests for what "Go to a page" accepts (runs on a PC: make test).
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/address.hpp"
#include "kt/settings.hpp"

#include <cstdio>
#include <string>

namespace
{

int failures = 0;

void expect_url(const char *typed, const char *url)
{
    const hui::kt::Address a = hui::kt::normalize_address(typed);
    if (a.error != hui::kt::AddressError::none || a.url != url)
    {
        std::printf("FAIL %-40s -> %s (error %d), wanted %s\n", typed, a.url.c_str(), static_cast<int>(a.error), url);
        ++failures;
    }
}

void expect_error(const char *typed, hui::kt::AddressError error)
{
    const hui::kt::Address a = hui::kt::normalize_address(typed);
    if (a.error != error)
    {
        std::printf("FAIL %-40s -> error %d (%s), wanted error %d\n", typed, static_cast<int>(a.error), a.url.c_str(),
                    static_cast<int>(error));
        ++failures;
    }
}

} // namespace

int main()
{
    using hui::kt::AddressError;
    expect_url("https://www.kamtape.com/xl/", "https://www.kamtape.com/xl/");
    expect_url("http://kamtape.com/xl/", "https://www.kamtape.com/xl/");
    expect_url("HTTPS://WWW.KamTape.com", "https://www.kamtape.com/");
    expect_url("kamtape.com", "https://www.kamtape.com/");
    expect_url("kamtape.com/", "https://www.kamtape.com/");
    expect_url("  www.kamtape.com/watch?v=abc  ", "https://www.kamtape.com/watch?v=abc");
    expect_url("kamtape.com?x=1", "https://www.kamtape.com/?x=1");
    expect_url("xl/", "https://www.kamtape.com/xl/");
    expect_url("/xl/", "https://www.kamtape.com/xl/");
    expect_url("watch?v=a.b", "https://www.kamtape.com/watch?v=a.b");
    expect_url("user/some.one", "https://www.kamtape.com/user/some.one");

    expect_error("", AddressError::empty);
    expect_error("   ", AddressError::empty);
    expect_error("cute cats", AddressError::spaces);
    expect_error("https://youtube.com/watch?v=x", AddressError::other_site);
    expect_error("youtube.com/watch?v=x", AddressError::other_site);
    expect_error("kamtape.com.evil.example/xl/", AddressError::other_site);
    expect_error("https://evil.example/kamtape.com/", AddressError::other_site);
    expect_error("http://kamtape.com@evil.example/", AddressError::other_site);

    if (hui::kt::short_address("https://www.kamtape.com/xl/") != "kamtape.com/xl/")
    {
        std::printf("FAIL short_address\n");
        ++failures;
    }

    hui::kt::Settings s;
    for (const char *u : {"a", "b", "c", "d", "e", "f", "b"})
        s.remember(u);
    if (s.recent != std::vector<std::string>{"b", "f", "e", "d", "c"})
    {
        std::printf("FAIL Settings::remember\n");
        ++failures;
    }

    std::printf(failures ? "%d failure(s)\n" : "all address tests passed\n", failures);
    return failures ? 1 : 0;
}
