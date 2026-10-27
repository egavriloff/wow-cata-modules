// Compile the real implementation against test doubles; never starts a server.
#include "../src/npc_teleport.cpp"
#include <iostream>
#include <limits>
#include <stdexcept>

// Synthetic paid destination exercises costs absent from the free shipped table.
Data const Tele[] = {
    {1, 0, GOSSIP_ICON_TAXI, "paid", 0xffffffffu, 20, 2, 0, 1, 2, 3, 0},
    {1, 2, GOSSIP_ICON_TAXI, "submenu", 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, GOSSIP_ICON_TAXI, "back", 0, 0, 0, 0, 0, 0, 0, 0}
};
uint32 const TELEPORT_COUNT = 3;

void Require(bool condition)
{
    if (!condition)
        throw std::runtime_error("Gossip regression check failed");
}

int main()
{
    Creature npc;
    NpcTeleport::AI ai(&npc);
    auto fresh = [&]()
    {
        sMapStore = {};
        Map::denied = 0;
        MapManager::valid = TerrainMgr::exists = true;
        Player player;
        player.npc = &npc;
        player.money = int64(0xffffffffu) + 10;
        ai.GossipHello(&player);
        return player;
    };
    // Player contains a self-pointer, reset it after the value is returned.
    auto initialize = [&](Player& player) { player.PlayerTalkClass = &player.talk; };
    int checks = 0;
    auto rejected = [&](auto mutate)
    {
        auto player = fresh(); initialize(player);
        mutate(player);
        ai.GossipSelect(&player, 1, 0);
        Require(player.calls == 0 && player.debited == 0);
        ++checks;
    };
    rejected([](Player& p) { p.talk.menu.items.clear(); });
    rejected([](Player& p) { p.talk.menu.items.at(0).OptionType = std::numeric_limits<uint32>::max(); });
    rejected([](Player& p) { p.talk.menu.items.at(0).Sender = 9; });
    rejected([](Player& p) { p.talk.menu.id = 2; });
    rejected([](Player& p) { p.talk.menu.items.at(0).OptionType = 2; });
    rejected([](Player& p) { p.talk.interaction.SourceGuid = 99; });
    rejected([](Player& p) { p.interact = false; });
    rejected([](Player& p) { p.team = HORDE; });
    rejected([](Player& p) { p.level = 19; });
    rejected([](Player& p) { p.combat = true; });
    rejected([](Player& p) { p.alive = false; });
    rejected([](Player& p) { p.inWorld = false; });
    rejected([](Player& p) { p.teleporting = true; });
    rejected([](Player& p) { p.flight = true; });
    rejected([](Player& p) { p.vehicle = true; });
    rejected([](Player&) { sMapStore.exists = false; });
    rejected([](Player&) { sMapStore.entry.expansion = 4; });
    rejected([](Player&) { sMapStore.entry.battleground = true; });
    rejected([](Player&) { MapManager::valid = false; });
    rejected([](Player&) { TerrainMgr::exists = false; });
    rejected([](Player&) { Map::denied = 1; });
    rejected([](Player& p) { p.money = 0xfffffffeu; });
    {
        auto p = fresh(); initialize(p); p.accepted = false;
        ai.GossipSelect(&p, 1, 0);
        Require(p.calls == 1 && p.debited == 0); ++checks;
    }
    {
        auto p = fresh(); initialize(p);
        ai.GossipSelect(&p, 1, 0);
        Require(p.calls == 1 && p.money == 10 && p.debited == -int64(0xffffffffu));
        ai.GossipSelect(&p, 1, 0);
        Require(p.calls == 1 && p.money == 10); ++checks;
    }
    {
        auto p = fresh(); initialize(p);
        ai.GossipSelect(&p, 1, 1);
        Require(p.talk.menu.id == 2 && p.calls == 0 && p.debited == 0);
        ai.GossipSelect(&p, 2, 0);
        Require(p.talk.menu.id == 1 && p.calls == 0 && p.debited == 0); ++checks;
    }
    {
        auto p = fresh(); initialize(p);
        ai.GossipSelect(&p, 1, std::numeric_limits<uint32>::max());
        Require(p.calls == 0 && p.debited == 0); ++checks;
    }
    Addmod_npc_teleportScripts();
    Require(CreatureScript::registered == std::vector<std::string>{"npc_teleport"}); ++checks;
    std::cout << checks << " gossip/payment/registration scenarios passed\n";
}
