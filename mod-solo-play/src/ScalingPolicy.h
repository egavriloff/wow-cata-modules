// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOLO_PLAY_SCALING_POLICY_H
#define SOLO_PLAY_SCALING_POLICY_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace SoloPlay
{
struct Profile
{
    double health = 1.0, damage = 1.0, armor = 1.0, mana = 1.0;
    double playerDamage = 1.0, playerHealing = 1.15, incomingDamage = 0.9, healing = 1.0;
    double armorExponent = 0.15, manaExponent = 0.15;
    unsigned minPlayers = 1, maxPlayers = 2;
};

struct Limits
{
    double minHealth = 0.15, minDamage = 0.3, minArmor = 0.6, minMana = 0.5;
    double maximum = 1.0;
};

struct Multipliers
{
    double health = 1.0, damage = 1.0, armor = 1.0, mana = 1.0;
    double playerDamage = 1.0, playerHealing = 1.0, incomingDamage = 1.0, healing = 1.0;
};

inline double Valid(double value, double fallback, double minimum, double maximum)
{
    return std::isfinite(value) && value >= minimum && value <= maximum ? value : fallback;
}

inline Multipliers Calculate(unsigned players, unsigned capacity, Profile const& p, Limits const& l,
    double healthExponent, double damageExponent, double contentHealth, double contentDamage,
    double rankHealth = 1.0, double rankDamage = 1.0)
{
    if (!players || players < p.minPlayers || players > p.maxPlayers || players >= capacity)
        return {};
    double ratio = double(players) / capacity;
    Multipliers m;
    m.playerDamage = p.playerDamage;
    m.playerHealing = p.playerHealing;
    m.incomingDamage = p.incomingDamage;
    // Budget player damage into creature HP: HP/playerDamage keeps the same kill-time target.
    m.health = std::clamp(std::pow(ratio, healthExponent) * p.health * contentHealth * rankHealth,
        l.minHealth, l.maximum);
    m.playerDamage = std::min(m.playerDamage, l.maximum / m.health);
    m.health *= m.playerDamage;
    m.healing = std::clamp(m.health * p.healing, l.minHealth, l.maximum);
    m.damage = std::clamp(std::pow(ratio, damageExponent) * p.damage * contentDamage * rankDamage,
        l.minDamage, l.maximum);
    m.armor = std::clamp(std::pow(ratio, p.armorExponent) * p.armor, l.minArmor, l.maximum);
    m.mana = std::clamp(std::pow(ratio, p.manaExponent) * p.mana, l.minMana, l.maximum);
    return m;
}

inline uint32_t Amount(uint32_t amount, double multiplier)
{
    if (!amount)
        return 0;
    multiplier = Valid(multiplier, 1.0, 0.01, 4.0);
    // An identity policy must preserve native uint32 events, including values above INT32_MAX.
    if (multiplier == 1.0)
        return amount;
    return uint32_t(std::clamp(std::round(double(amount) * multiplier), 1.0,
        double(std::numeric_limits<int32_t>::max())));
}

inline uint32_t Preserve(uint32_t current, uint32_t oldMaximum, uint32_t newMaximum, bool alive)
{
    if (!alive || !current)
        return 0;
    return uint32_t(std::clamp(double(current) * newMaximum / std::max(1u, oldMaximum),
        1.0, double(newMaximum)));
}

// Debounce is reset throughout combat. Even disconnect-to-zero cannot change a live pull.
struct RosterGate
{
    unsigned active = 0, pending = 0, elapsed = 0;
    bool Update(unsigned observed, bool combat, unsigned diff, unsigned delay)
    {
        if (combat || observed != pending)
        {
            pending = observed;
            elapsed = 0;
        }
        if (combat)
            return false;
        elapsed = unsigned(std::min(uint64_t(elapsed) + diff, uint64_t(delay)));
        if (elapsed < delay || active == pending)
            return false;
        active = pending;
        return true;
    }
};
}
#endif
