// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <charconv>
#include <compare>
#include <optional>
#include <string_view>

namespace Jinra {

struct VersionNumber {
    std::array<unsigned, 3> core{};
    std::optional<unsigned> preview;

    auto operator<=>(const VersionNumber& other) const {
        if (const auto order = core <=> other.core; order != 0)
            return order;
        if (!preview && !other.preview)
            return std::strong_ordering::equal;
        if (!preview)
            return std::strong_ordering::greater;
        if (!other.preview)
            return std::strong_ordering::less;
        return *preview <=> *other.preview;
    }
    bool operator==(const VersionNumber&) const = default;

    static std::optional<VersionNumber> Parse(std::string_view text) {
        if (text.starts_with('v'))
            text.remove_prefix(1);
        const auto number = [](std::string_view part) -> std::optional<unsigned> {
            if (part.empty() || (part.size() > 1 && part.front() == '0'))
                return {};
            unsigned value{};
            const auto result = std::from_chars(part.data(), part.data() + part.size(), value);
            if (result.ec != std::errc{} || result.ptr != part.data() + part.size())
                return {};
            return value;
        };
        VersionNumber version;
        if (const auto dash = text.find('-'); dash != std::string_view::npos) {
            auto suffix = text.substr(dash);
            if (!suffix.starts_with("-preview."))
                return {};
            version.preview = number(suffix.substr(9));
            if (!version.preview)
                return {};
            text = text.substr(0, dash);
        }
        for (std::size_t i = 0; i < version.core.size(); ++i) {
            const auto dot = text.find('.');
            if ((i < 2) != (dot != std::string_view::npos))
                return {};
            const auto part = number(text.substr(0, dot));
            if (!part)
                return {};
            version.core[i] = *part;
            if (dot != std::string_view::npos)
                text.remove_prefix(dot + 1);
        }
        return version;
    }
};

} // namespace Jinra
