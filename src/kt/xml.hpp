// KamTape PS5 - a small XML reader for the KamTape API's answers.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Elements, attributes and text only: comments, the XML declaration,
// processing instructions and DOCTYPE are skipped; CDATA becomes text. It is
// forgiving the way the API's own examples need: a bare '&' (as in
// "Odd & Outrageous") stays a '&', and unknown entities are kept as written.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hui::kt
{

struct XmlNode
{
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::string text; // all text directly inside, entities decoded
    std::vector<std::unique_ptr<XmlNode>> children;

    // The first child with that name, or nullptr.
    const XmlNode *child(std::string_view name) const;
    // The trimmed text of the first child with that name, or "".
    std::string child_text(std::string_view name) const;
    std::string attribute(std::string_view name) const;
};

// The document's root element, or nullptr when the text is not XML.
std::unique_ptr<XmlNode> parse_xml(std::string_view text);

} // namespace hui::kt
