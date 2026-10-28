// Native API doubles for executing the actual controller without databases.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>
using uint8 = uint8_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using SpellSchoolMask = uint32;
inline constexpr uint32 PROC_HIT_ABSORB = 1;
enum DamageEffectType : uint8 { DIRECT_DAMAGE, SPELL_DIRECT_DAMAGE, DOT, HEAL, NODAMAGE, SELF_DAMAGE };
enum { RAID_DIFFICULTY_25MAN_HEROIC=3, DUNGEON_DIFFICULTY_HEROIC=1, CREATURE_ELITE_NORMAL=0, CREATURE_ELITE_WORLDBOSS=3 };
enum { UNIT_MOD_HEALTH, UNIT_MOD_ARMOR, UNIT_MOD_MANA, BASE_PCT, POWER_MANA=0 };
enum { SPELL_EFFECT_INSTAKILL, SPELL_EFFECT_HEAL_PCT, SPELL_EFFECT_ENVIRONMENTAL_DAMAGE, SPELL_EFFECT_DAMAGE_FROM_MAX_HEALTH_PCT,
    SPELL_AURA_PERIODIC_DAMAGE_PERCENT, SPELL_AURA_DAMAGE_SHIELD, SPELL_AURA_PROC_TRIGGER_DAMAGE, SPELL_AURA_OBS_MOD_HEALTH };
inline constexpr unsigned SPELL_ATTR4_IGNORE_DAMAGE_TAKEN_MODIFIERS = 1;
#define TC_LOG_ERROR(...) ((void)0)
#define TC_LOG_INFO(...) ((void)0)
class Player;
class Creature;
struct Entry { unsigned expansion=3; unsigned Expansion() const { return expansion; } };
struct InstanceScript { bool combat=false; bool IsEncounterInProgress() const { return combat; } };
struct Ref { Player* player; Player* GetSource() const { return player; } };
class Map
{
public:
    bool dungeon=true, raid=false, pvp=false, heroic=false;
    unsigned id=1, capacity=5, difficulty=0;
    Entry entry;
    InstanceScript instance;
    std::vector<Ref> players;
    bool IsDungeon() const { return dungeon; }
    bool IsRaid() const { return raid; }
    bool IsHeroic() const { return heroic; }
    bool IsBattlegroundOrArena() const { return pvp; }
    Map* ToInstanceMap() { return dungeon ? this : nullptr; }
    unsigned GetId() const { return id; }
    unsigned GetDifficulty() const { return difficulty; }
    unsigned GetMaxPlayers() const { return capacity; }
    Entry* GetEntry() { return &entry; }
    InstanceScript* GetInstanceScript() { return &instance; }
    auto const& GetPlayers() const { return players; }
};
class Unit
{
public:
    virtual ~Unit() = default;
    Map* map=nullptr;
    bool inWorld=true, combat=false, pvp=false, owned=false, charmed=false;
    unsigned level=85;
    virtual Player* ToPlayer() { return nullptr; }
    virtual Creature* ToCreature() { return nullptr; }
    Map* GetMap() const { return map; }
    Map* FindMap() const { return map; }
    bool IsInWorld() const { return inWorld; }
    bool IsInCombat() const { return combat; }
    bool IsPvP() const { return pvp; }
    bool IsCharmed() const { return charmed; }
    bool IsControlledByPlayer() const { return owned; }
    bool IsCharmedOwnedByPlayerOrPlayer() { return ToPlayer() || owned || charmed; }
    unsigned getLevel() const { return level; }
};
struct WorldSession
{
    Player* player=nullptr;
    bool logout=false;
    Player* GetPlayer() { return player; }
    bool PlayerLogout() { return logout; }
};
class Player : public Unit
{
public:
    bool gm=false;
    void* duel=nullptr;
    WorldSession session{this};
    Player* ToPlayer() override { return this; }
    bool IsGameMaster() const { return gm; }
    WorldSession* GetSession() { return &session; }
    unsigned GetMapId() const { return map->id; }
    unsigned GetInstanceId() const { return 1; }
};
struct CreatureTemplate { unsigned rank=0; };
class Creature : public Unit
{
public:
    bool pet=false, guardian=false, summon=false, totem=false, vehicle=false, trigger=false, boss=false, alive=true;
    unsigned entry=1, owner=0, health=10000, maxHealth=10000, mana=1000, maxMana=1000, resets=0;
    double baseHealth=10000, baseArmor=1000, baseMana=1000, armor=1000;
    std::array<float,3> modifiers{1,1,1};
    CreatureTemplate info;
    Creature* ToCreature() override { return this; }
    bool IsPet() const { return pet; }
    bool IsGuardian() const { return guardian; }
    bool IsSummon() const { return summon; }
    bool IsTotem() const { return totem; }
    bool IsVehicle() const { return vehicle; }
    bool IsTrigger() const { return trigger; }
    bool IsDungeonBoss() const { return boss; }
    bool IsAlive() const { return alive; }
    unsigned GetOwnerOrCreatorGUID() const { return owner; }
    unsigned GetEntry() const { return entry; }
    CreatureTemplate const* GetCreatureTemplate() { return &info; }
    unsigned GetHealth() const { return health; }
    unsigned GetMaxHealth() const { return maxHealth; }
    unsigned GetPower(int) const { return mana; }
    unsigned GetMaxPower(int) const { return maxMana; }
    void SetHealth(unsigned value) { health=std::min(value,maxHealth); }
    void SetPower(int,unsigned value) { mana=std::min(value,maxMana); }
    float GetPctModifierValue(int mod,int) const { return modifiers[mod]; }
    void SetStatPctModifier(int mod,int,float value) { modifiers[mod]=value; }
    void UpdateMaxHealth() { maxHealth=unsigned(baseHealth*modifiers[UNIT_MOD_HEALTH]); }
    void UpdateArmor() { armor=baseArmor*modifiers[UNIT_MOD_ARMOR]; }
    void UpdateMaxPower(int) { maxMana=unsigned(baseMana*modifiers[UNIT_MOD_MANA]); }
    void ResetPlayerDamageReq() { ++resets; }
};
struct SpellInfo
{
    unsigned Id=1;
    std::unordered_set<int> effects, auras;
    unsigned attributes=0;
    bool HasAttribute(unsigned value) const { return attributes & value; }
    bool HasEffect(int value) const { return effects.count(value); }
    bool HasAura(int value) const { return auras.count(value); }
};
struct ConfigMgr
{
    std::map<std::string,std::string> values;
    std::string GetStringDefault(char const* key,char const* fallback) const
    { auto it=values.find(key); return it==values.end()?fallback:it->second; }
    bool GetBoolDefault(char const* key,bool fallback) const
    { return GetStringDefault(key,fallback?"1":"0")=="1"; }
    float GetFloatDefault(char const* key,float fallback) const
    { auto it=values.find(key); return it==values.end()?fallback:std::stof(it->second); }
};
inline ConfigMgr configMgr;
inline ConfigMgr* sConfigMgr=&configMgr;
struct ChatHandler
{
    WorldSession* session=nullptr;
    Creature* selected=nullptr;
    WorldSession* GetSession() { return session; }
    Creature* getSelectedCreature() { return selected; }
    template<class... Args> void PSendSysMessage(char const*,Args...) { }
};
namespace rbac { inline constexpr unsigned RBAC_PERM_COMMAND_SERVER_INFO=1; }
struct ChatCommand
{
    char const* name;
    unsigned permission;
    bool console;
    bool (*handler)(ChatHandler*,char const*);
    char const* help;
};
struct UnitScript
{
    UnitScript(char const*) { }
    virtual ~UnitScript() = default;
    virtual void OnCreatureScalingUpdate(Creature*) { }
    virtual void OnCreatureScalingReset(Creature*) { }
    virtual void OnBeforeMapUpdate(Map*,uint32) { }
    virtual void OnScalingMapDestroy(Map*) { }
    virtual void OnScalingDamage(Unit*,Unit*,uint32&,DamageEffectType,SpellInfo const*) { }
    virtual void OnBeforeHeal(Unit*,Unit*,uint32&,SpellInfo const*) { }
};
struct ScriptMgrDouble
{
    std::function<void(Unit*,Unit*,uint32&,SpellInfo const*)> heal;
    void OnBeforeHeal(Unit* source,Unit* target,uint32& amount,SpellInfo const* spell)
    { if (heal) heal(source,target,amount,spell); }
};
inline ScriptMgrDouble scriptMgr;
inline ScriptMgrDouble* sScriptMgr = &scriptMgr;
struct WorldScript { WorldScript(char const*) { } virtual ~WorldScript()=default; virtual void OnConfigLoad(bool) { } };
struct CommandScript { CommandScript(char const*) { } virtual ~CommandScript()=default; virtual std::vector<ChatCommand> GetCommands() const=0; };
