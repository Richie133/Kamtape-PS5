// KamTape PS5 - turns what the user typed into a KamTape address.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The app only opens KamTape pages. It takes full links ("https://www.kamtape.com/xl/"),
// links without the scheme ("kamtape.com/xl/") and bare paths ("xl/", "/xl/"), and
// refuses other sites and text with spaces (there is no search here).

#pragma once

#include <string>
#include <string_view>

namespace hui::kt
{

constexpr const char *kOrigin = "https://www.kamtape.com";
constexpr const char *kXlUrl = "https://www.kamtape.com/xl/";
constexpr const char *kSiteUrl = "https://www.kamtape.com/";

enum class AddressError
{
    none,
    empty,
    other_site, // a link to somewhere that isn't KamTape
    spaces,     // words, not an address
};

struct Address
{
    AddressError error = AddressError::none;
    std::string url; // https://www.kamtape.com/... when error is none
};

Address normalize_address(std::string_view typed);

// "https://www.kamtape.com/xl/" -> "kamtape.com/xl/", for labels.
std::string short_address(std::string_view url);

} // namespace hui::kt
