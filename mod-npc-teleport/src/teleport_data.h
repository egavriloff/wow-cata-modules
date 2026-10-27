// Derived from the supplied Project SkyFire npc-teleport module.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Define.h"

#include <string>

struct TeleportStructure {
    uint32 menu_id, next_menu_id;
    uint8 icon;
    std::string name;
    uint32 cost;
    uint8 level, faction;
    uint32 map;
    float x, y, z, o;
};

using Data = TeleportStructure;

extern Data const Tele[];
extern uint32 const TELEPORT_COUNT;
