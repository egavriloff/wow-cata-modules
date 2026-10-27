/*
 * This file is part of Project SkyFire https://www.projectskyfire.org.
 * See LICENSE.md file for Copyright information (see this module's NOTICE.md).
 * Derived from alexkulya/pandaria_5.4.8, Custom/npc_teleport.cpp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ScriptMgr.h"
#include "PassiveAI.h"
#include "Chat.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Map.h"
#include "MapManager.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "TerrainMgr.h"
#include "teleport_data.h"
#include "teleport_text.h"

namespace
{
bool IsAllowed(Player const* player, Data const& destination)
{
    return player->getLevel() >= destination.level &&
        (destination.faction == 0 ||
        (destination.faction == 1 && player->GetTeam() == HORDE) ||
        (destination.faction == 2 && player->GetTeam() == ALLIANCE));
}

bool HasMenu(uint32 menu)
{
    for (uint32 i = 0; i < TELEPORT_COUNT; ++i)
        if (Tele[i].menu_id == menu)
            return true;
    return false;
}

void ShowMenu(Player* player, Creature* creature, uint32 menu)
{
    ClearGossipMenuFor(player);
    InitGossipMenuFor(player, menu);
    for (uint32 i = 0; i < TELEPORT_COUNT; ++i)
    {
        Data const& destination = Tele[i];
        if (destination.menu_id != menu || !IsAllowed(player, destination))
            continue;

        std::string popup = destination.next_menu_id ? "" :
            std::string(TeleportText::Confirmation) + destination.name + "|r?";
        AddGossipItemFor(player, GossipOptionIcon(destination.icon), destination.name,
            GOSSIP_SENDER_MAIN, i, popup, destination.cost, false);
    }
    // The donor's custom npc_text IDs were not supplied with any text records.
    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature);
}

class NpcTeleport final : public CreatureScript
{
public:
    NpcTeleport() : CreatureScript("npc_teleport") { }

    struct AI final : public PassiveAI
    {
        explicit AI(Creature* creature) : PassiveAI(creature) { }

        bool GossipHello(Player* player) override
        {
            ShowMenu(player, me, 1);
            return true;
        }

        bool GossipSelect(Player* player, uint32 menuId, uint32 gossipListId) override
        {
            auto const& menu = player->PlayerTalkClass->GetGossipMenu();
            GossipMenuItem const* item = menu.GetItem(gossipListId);
            if (!item || menuId != menu.GetMenuId() || item->Sender != GOSSIP_SENDER_MAIN ||
                item->OptionType >= TELEPORT_COUNT ||
                player->PlayerTalkClass->GetInteractionData().SourceGuid != me->GetGUID() ||
                player->GetNPCIfCanInteractWith(me->GetGUID(), UNIT_NPC_FLAG_GOSSIP) != me)
            {
                ClearGossipMenuFor(player);
                CloseGossipMenuFor(player);
                return true;
            }

            Data const& destination = Tele[item->OptionType];
            if (destination.menu_id != menuId || !IsAllowed(player, destination))
            {
                ClearGossipMenuFor(player);
                CloseGossipMenuFor(player);
                return true;
            }

            // Invalidate the selection before navigation or teleportation.
            ClearGossipMenuFor(player);
            if (destination.next_menu_id)
            {
                if (HasMenu(destination.next_menu_id))
                    ShowMenu(player, me, destination.next_menu_id);
                else
                    CloseGossipMenuFor(player);
                return true;
            }

            if (player->IsInCombat())
            {
                ChatHandler(player->GetSession()).SendSysMessage(TeleportText::Combat);
                ShowMenu(player, me, menuId);
                return true;
            }

            MapEntry const* map = sMapStore.LookupEntry(destination.map);
            if (!player->IsAlive() || !player->IsInWorld() || player->IsBeingTeleported() ||
                player->IsInFlight() || player->GetVehicle() || !map || map->Expansion() > 3 ||
                map->IsBattlegroundOrArena() ||
                !MapManager::IsValidMapCoord(destination.map, destination.x, destination.y, destination.z, destination.o) ||
                !TerrainMgr::ExistMapAndVMap(destination.map, destination.x, destination.y) ||
                Map::PlayerCannotEnter(destination.map, player, false) != Map::CAN_ENTER)
            {
                ChatHandler(player->GetSession()).SendSysMessage(TeleportText::Unavailable);
                ShowMenu(player, me, menuId);
                return true;
            }

            // Widen before negation: uint32 costs must never wrap into a credit.
            int64 const cost = static_cast<int64>(destination.cost);
            if (!player->HasEnoughMoney(cost))
            {
                ChatHandler(player->GetSession()).SendSysMessage(TeleportText::Money);
                ShowMenu(player, me, menuId);
                return true;
            }

            CloseGossipMenuFor(player);
            // TeleportTo validates core restrictions (including disabled maps and starting zones).
            // Debit only an accepted request; the donor's shipped destinations are all free.
            if (player->TeleportTo(destination.map, destination.x, destination.y, destination.z, destination.o))
                player->ModifyMoney(-cost);
            else
                ChatHandler(player->GetSession()).SendSysMessage(TeleportText::Unavailable);
            return true;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override { return new AI(creature); }
};
}

void Addmod_npc_teleportScripts()
{
    new NpcTeleport();
}
