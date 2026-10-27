// Minimal test doubles for exercising the actual script without a running world.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using int64 = std::int64_t;
constexpr uint32 HORDE = 67, ALLIANCE = 469, UNIT_NPC_FLAG_GOSSIP = 1;
constexpr uint32 GOSSIP_SENDER_MAIN = 1, DEFAULT_GOSSIP_MESSAGE = 0xffffff;
enum GossipOptionIcon { GOSSIP_ICON_TAXI = 2, GOSSIP_ICON_TALK = 0, GOSSIP_ICON_TRAINER = 3, GOSSIP_ICON_BATTLE = 9 };
struct Creature;
struct GossipMenuItem { uint32 Sender, OptionType; };
struct GossipMenu
{
    uint32 id = 0;
    std::map<uint32, GossipMenuItem> items;
    GossipMenuItem const* GetItem(uint32 key) const
    {
        auto it = items.find(key);
        return it == items.end() ? nullptr : &it->second;
    }
    uint32 GetMenuId() const { return id; }
    uint32 GetMenuItemCount() const { return uint32(items.size()); }
};
struct Interaction { uint64 SourceGuid = 0; };
struct PlayerMenu
{
    uint32 sentMenus = 0, textId = 0;
    GossipMenu menu;
    Interaction interaction;
    GossipMenu& GetGossipMenu() { return menu; }
    Interaction const& GetInteractionData() const { return interaction; }
};
struct Player
{
    PlayerMenu talk;
    PlayerMenu* PlayerTalkClass = &talk;
    uint32 team = ALLIANCE, level = 85;
    bool alive = true, inWorld = true, combat = false, teleporting = false, flight = false;
    bool vehicle = false, accepted = true, interact = true;
    Creature* npc = nullptr;
    int64 money = 0, debited = 0;
    int calls = 0;
    bool closed = false;
    uint32 getLevel() const { return level; }
    uint32 GetTeam() const { return team; }
    bool IsAlive() const { return alive; }
    bool IsInWorld() const { return inWorld; }
    bool IsInCombat() const { return combat; }
    bool IsBeingTeleported() const { return teleporting; }
    bool IsInFlight() const { return flight; }
    void* GetVehicle() const { return vehicle ? (void*)this : nullptr; }
    Creature* GetNPCIfCanInteractWith(uint64, uint32) const { return interact ? npc : nullptr; }
    void* GetSession() { return this; }
    bool HasEnoughMoney(int64 cost) const { return money >= cost; }
    bool TeleportTo(uint32, float, float, float, float) { ++calls; return accepted; }
    bool ModifyMoney(int64 delta) { debited += delta; money += delta; return true; }
};
struct Creature
{
    uint64 GetGUID() const { return 42; }
    uint32 GetEntry() const { return 900100; }
    uint32 GetScriptId() const { return 17; }
};
struct ObjectMgr
{
    uint32 GetScriptId(char const* name) const { return std::string(name) == "npc_teleport" ? 17 : 0; }
};
inline ObjectMgr objectMgr;
inline ObjectMgr* sObjectMgr = &objectMgr;
#define TC_LOG_DEBUG(...) do { } while (false)
#define TC_LOG_INFO(...) do { } while (false)
#define TC_LOG_ERROR(...) do { } while (false)
struct CreatureAI { virtual ~CreatureAI() = default; };
struct PassiveAI : CreatureAI
{
    Creature* me;
    explicit PassiveAI(Creature* creature) : me(creature) { }
    virtual bool GossipHello(Player*) { return false; }
    virtual bool GossipSelect(Player*, uint32, uint32) { return false; }
};
struct CreatureScript
{
    static inline std::vector<std::string> registered;
    explicit CreatureScript(char const* name) { registered.emplace_back(name); }
    virtual ~CreatureScript() = default;
    virtual CreatureAI* GetAI(Creature*) const = 0;
};
struct ChatHandler
{
    explicit ChatHandler(void*) { }
    void SendSysMessage(char const*) { }
};
struct MapEntry
{
    uint32 expansion = 3;
    bool battleground = false;
    uint32 Expansion() const { return expansion; }
    bool IsBattlegroundOrArena() const { return battleground; }
};
struct MapStore
{
    MapEntry entry;
    bool exists = true;
    MapEntry const* LookupEntry(uint32) const { return exists ? &entry : nullptr; }
};
inline MapStore sMapStore;
struct Map
{
    enum { CAN_ENTER = 0 };
    static inline int denied = 0;
    static int PlayerCannotEnter(uint32, Player*, bool) { return denied; }
};
struct MapManager
{
    static inline bool valid = true;
    static bool IsValidMapCoord(uint32, float, float, float, float) { return valid; }
};
struct TerrainMgr
{
    static inline bool exists = true;
    static bool ExistMapAndVMap(uint32, float, float) { return exists; }
};
inline void ClearGossipMenuFor(Player* player) { player->talk.menu.items.clear(); }
inline void InitGossipMenuFor(Player* player, uint32 id) { player->talk.menu.id = id; }
inline void AddGossipItemFor(Player* player, GossipOptionIcon, std::string const&, uint32 sender,
    uint32 action, std::string const& = "", uint32 = 0, bool = false)
{
    auto& items = player->talk.menu.items;
    items.emplace(uint32(items.size()), GossipMenuItem{sender, action});
}
inline void SendGossipMenuFor(Player* player, uint32 textId, Creature* creature)
{
    ++player->talk.sentMenus;
    player->talk.textId = textId;
    player->talk.interaction.SourceGuid = creature->GetGUID();
}
inline void CloseGossipMenuFor(Player* player) { player->closed = true; }
