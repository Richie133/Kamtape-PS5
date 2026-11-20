// KamTape PS5 - URL escaping for query values.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/http.hpp"

namespace hui::kt::http
{

std::string escape(const std::string &value)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : value)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '_' || c == '.' || c == '~')
        {
            out += static_cast<char>(c);
        }
        else
        {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

} // namespace hui::kt::http
