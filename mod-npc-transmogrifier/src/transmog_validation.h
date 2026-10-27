#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace TransmogValidation
{
// UINT32_MAX is an unaffordable sentinel, never an overflowed/free price.
inline std::uint32_t Cost(std::uint64_t base, double scale, std::int32_t fixed)
{
    constexpr auto invalid = std::numeric_limits<std::uint32_t>::max();
    double price = double(base) * scale + double(fixed);
    if (!std::isfinite(scale) || scale < 0 || fixed < 0 || !std::isfinite(price) || price >= invalid)
        return invalid;
    return static_cast<std::uint32_t>(price);
}

struct Appearance { std::uint8_t slot; std::uint32_t entry; };

inline bool Parse(std::string const& text, std::vector<Appearance>& result)
{
    result.clear();
    if (text.size() > 512)
        return false;
    std::istringstream input(text);
    bool seen[19] = {};
    while (input >> std::ws && !input.eof())
    {
        std::uint64_t slot, entry;
        if (!(input >> slot >> entry) || slot >= 19 || seen[slot] || !entry ||
            entry > std::numeric_limits<std::uint32_t>::max())
            return false;
        seen[slot] = true;
        result.push_back({static_cast<std::uint8_t>(slot), static_cast<std::uint32_t>(entry)});
    }
    return !result.empty();
}

inline bool Name(std::string const& name)
{
    if (name.empty() || name.size() > 48)
        return false;
    for (unsigned char c : name)
        if (c < 32 || c == 127 || c == '|')
            return false;
    return true;
}
}
