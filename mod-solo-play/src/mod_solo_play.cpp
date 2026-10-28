// SPDX-License-Identifier: GPL-3.0-or-later
// Conceptual adaptation of SoloCraft and AutoBalance; see ../NOTICE.md.
#include "ScalingPolicy.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DBCStores.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "RBAC.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "WorldSession.h"
#include <array>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace SoloPlay
{
struct Config
{
    bool enabled = false, legacy = true, heroic = true, dungeons = true, raids = true;
    unsigned profile = 0, delay = 5000, levelGap = 10;
    double healthExponent = 0.55, damageExponent = 0.35;
    double dungeonHealth = 1.0, dungeonDamage = 1.0, raidHealth = 1.0, raidDamage = 1.0;
    double heroicHealth = 1.1, heroicDamage = 1.1, bossHealth = 1.2, bossDamage = 1.1;
    double eliteHealth = 1.0, eliteDamage = 1.0;
    Limits limits;
    std::array<Profile, 3> profiles;
    std::unordered_set<uint32> maps, creatures, spells;
    std::unordered_map<uint32, std::pair<double, double>> mapFactors, creatureFactors;
};

double Read(std::string const& key, double fallback, double minimum, double maximum)
{
    double value = sConfigMgr->GetFloatDefault(key.c_str(), float(fallback));
    double validated = Valid(value, fallback, minimum, maximum);
    if (value != validated)
        TC_LOG_ERROR("server.loading", "SoloPlay: invalid %s; using %g", key.c_str(), fallback);
    return validated;
}

std::unordered_set<uint32> Ids(char const* key)
{
    std::unordered_set<uint32> result;
    std::string text = sConfigMgr->GetStringDefault(key, "");
    std::replace(text.begin(), text.end(), ',', ' ');
    std::istringstream input(text);
    std::string token;
    while (input >> token)
    {
        try
        {
            size_t end;
            auto id = std::stoull(token, &end);
            if (end != token.size() || !id || id > UINT32_MAX || token[0] == '-')
                throw std::invalid_argument("id");
            result.insert(uint32(id));
        }
        catch (std::exception const&)
        {
            TC_LOG_ERROR("server.loading", "SoloPlay: ignored invalid ID in %s: %s", key, token.c_str());
        }
    }
    return result;
}

std::unordered_map<uint32, std::pair<double, double>> Overrides(char const* key)
{
    std::unordered_map<uint32, std::pair<double, double>> result;
    std::istringstream input(sConfigMgr->GetStringDefault(key, ""));
    std::string token;
    while (std::getline(input, token, ';'))
    {
        if (token.find_first_not_of(" \t") == std::string::npos)
            continue;
        std::replace(token.begin(), token.end(), ':', ' ');
        std::istringstream row(token);
        uint64 id;
        double hp, damage;
        std::string extra;
        if (!(row >> id >> hp >> damage) || row >> extra || !id || id > UINT32_MAX ||
            Valid(hp, -1, 0.05, 4.0) != hp || Valid(damage, -1, 0.05, 4.0) != damage)
        {
            TC_LOG_ERROR("server.loading", "SoloPlay: ignored invalid override in %s: %s", key, token.c_str());
            continue;
        }
        result[uint32(id)] = {hp, damage};
    }
    return result;
}

std::shared_ptr<Config const> LoadConfig()
{
    auto c = std::make_shared<Config>();
    c->enabled = sConfigMgr->GetBoolDefault("SoloPlay.Enable", false);
    c->legacy = sConfigMgr->GetBoolDefault("SoloPlay.Legacy.Enable", true);
    c->heroic = sConfigMgr->GetBoolDefault("SoloPlay.Heroic.Enable", true);
    c->dungeons = sConfigMgr->GetBoolDefault("SoloPlay.Dungeons.Enable", true);
    c->raids = sConfigMgr->GetBoolDefault("SoloPlay.Raids.Enable", true);
    std::string name = sConfigMgr->GetStringDefault("SoloPlay.Profile", "Balanced");
    if (name == "Solo") c->profile = 1;
    else if (name == "Duo") c->profile = 2;
    else if (name != "Balanced")
    {
        TC_LOG_ERROR("server.loading", "SoloPlay: unknown profile %s; disabling module", name.c_str());
        c->enabled = false;
    }
    c->delay = unsigned(Read("SoloPlay.SettleMilliseconds", 5000, 1000, 60000));
    c->levelGap = unsigned(Read("SoloPlay.Legacy.SkipLevelGap", 10, 1, 85));
    c->healthExponent = Read("SoloPlay.HealthExponent", 0.55, 0.05, 1.0);
    c->damageExponent = Read("SoloPlay.DamageExponent", 0.35, 0.05, 1.0);
    c->limits.maximum = Read("SoloPlay.MaximumMultiplier", 1.0, 0.5, 1.0);
    c->limits.minHealth = Read("SoloPlay.MinimumHealth", 0.15, 0.01, c->limits.maximum);
    c->limits.minDamage = Read("SoloPlay.MinimumDamage", 0.3, 0.01, c->limits.maximum);
    c->limits.minArmor = Read("SoloPlay.MinimumArmor", 0.6, 0.01, c->limits.maximum);
    c->limits.minMana = Read("SoloPlay.MinimumMana", 0.5, 0.01, c->limits.maximum);
    // A lower maximum also bounds defaults, including when an invalid minimum falls back.
    c->limits.minArmor = std::min(c->limits.minArmor, c->limits.maximum);
    for (auto pair : {std::pair{"Dungeon", &c->dungeonHealth}, {"Raid", &c->raidHealth},
        {"Heroic", &c->heroicHealth}, {"Boss", &c->bossHealth}, {"Elite", &c->eliteHealth}})
        *pair.second = Read(std::string("SoloPlay.") + pair.first + ".Health", *pair.second, 0.05, 4.0);
    for (auto pair : {std::pair{"Dungeon", &c->dungeonDamage}, {"Raid", &c->raidDamage},
        {"Heroic", &c->heroicDamage}, {"Boss", &c->bossDamage}, {"Elite", &c->eliteDamage}})
        *pair.second = Read(std::string("SoloPlay.") + pair.first + ".Damage", *pair.second, 0.05, 4.0);
    c->profiles[1].health = 0.9;
    c->profiles[1].damage = 0.9;
    c->profiles[1].playerHealing = 1.35;
    c->profiles[1].incomingDamage = 0.8;
    c->profiles[1].maxPlayers = 1;
    c->profiles[2].playerHealing = 1.1;
    c->profiles[2].incomingDamage = 0.95;
    c->profiles[2].minPlayers = 2;
    std::array<char const*, 3> names = {"Balanced", "Solo", "Duo"};
    for (unsigned i = 0; i < names.size(); ++i)
    {
        Profile& p = c->profiles[i];
        std::string prefix = std::string("SoloPlay.") + names[i] + ".";
        p.health = Read(prefix + "Health", p.health, 0.05, 4.0);
        p.damage = Read(prefix + "Damage", p.damage, 0.05, 4.0);
        p.armor = Read(prefix + "Armor", p.armor, 0.05, 4.0);
        p.mana = Read(prefix + "Mana", p.mana, 0.05, 4.0);
        p.healing = Read(prefix + "Healing", p.healing, 0.05, 4.0);
        p.armorExponent = Read(prefix + "ArmorExponent", p.armorExponent, 0.0, 1.0);
        p.manaExponent = Read(prefix + "ManaExponent", p.manaExponent, 0.0, 1.0);
        p.playerDamage = Read(prefix + "PlayerDamage", p.playerDamage, 1.0, 1.25);
        p.playerHealing = Read(prefix + "PlayerHealing", p.playerHealing, 1.0, 1.5);
        p.incomingDamage = Read(prefix + "IncomingDamage", p.incomingDamage, 0.5, 1.0);
        p.minPlayers = unsigned(Read(prefix + "MinPlayers", p.minPlayers, 1, 2));
        p.maxPlayers = unsigned(Read(prefix + "MaxPlayers", p.maxPlayers, p.minPlayers, 2));
        p.maxPlayers = std::max(p.minPlayers, p.maxPlayers);
    }
    c->maps = Ids("SoloPlay.ExcludedMaps");
    c->creatures = Ids("SoloPlay.ExcludedCreatures");
    c->spells = Ids("SoloPlay.ExcludedSpells");
    c->mapFactors = Overrides("SoloPlay.MapOverrides");
    c->creatureFactors = Overrides("SoloPlay.CreatureOverrides");
    return c;
}

struct Baseline
{
    double health, armor, mana;
    Multipliers multipliers;
    uint64 revision = 0;
};

struct Context
{
    RosterGate roster;
    unsigned capacity = 0, highestLevel = 0, pendingLevel = 0;
    bool combat = false;
    uint64 revision = 1;
    std::shared_ptr<Config const> config;
    std::shared_ptr<Config const> pendingConfig;
    std::unordered_map<Creature*, Baseline> creatures;
};

bool Participant(Player* p)
{
    // Death is intentionally not tested. Link-dead sessions disappear on map removal.
    return p && p->IsInWorld() && !p->IsGameMaster() && p->GetSession() &&
        !p->GetSession()->PlayerLogout();
}

bool Content(Map* map, Config const& c)
{
    if (!map || !map->IsDungeon() || !map->ToInstanceMap() || map->IsBattlegroundOrArena())
        return false;
    if (!c.enabled || c.maps.count(map->GetId()) || (!c.legacy && map->GetEntry()->Expansion() < 3))
        return false;
    if ((map->IsRaid() && !c.raids) || (!map->IsRaid() && !c.dungeons) || (map->IsHeroic() && !c.heroic))
        return false;
    return map->IsRaid() ? map->GetDifficulty() <= RAID_DIFFICULTY_25MAN_HEROIC :
        map->GetDifficulty() <= DUNGEON_DIFFICULTY_HEROIC;
}

bool CreatureEligible(Creature* creature, Context const& context)
{
    Config const& c = *context.config;
    return Content(creature->GetMap(), c) && !creature->IsPet() && !creature->IsGuardian() &&
        !creature->IsSummon() && !creature->IsTotem() && !creature->IsVehicle() &&
        !creature->IsTrigger() && !creature->IsControlledByPlayer() && !creature->IsCharmed() &&
        !creature->GetOwnerOrCreatorGUID() && !c.creatures.count(creature->GetEntry()) &&
        context.highestLevel < creature->getLevel() + c.levelGap;
}

Multipliers Policy(Map* map, Context const& context, Creature* creature)
{
    if (!CreatureEligible(creature, context))
        return {};
    Config const& c = *context.config;
    double hp = map->IsRaid() ? c.raidHealth : c.dungeonHealth;
    double damage = map->IsRaid() ? c.raidDamage : c.dungeonDamage;
    if (map->IsHeroic()) { hp *= c.heroicHealth; damage *= c.heroicDamage; }
    auto apply = [&](auto const& factors, uint32 id)
    {
        auto it = factors.find(id);
        if (it != factors.end()) { hp *= it->second.first; damage *= it->second.second; }
    };
    apply(c.mapFactors, map->GetId());
    apply(c.creatureFactors, creature->GetEntry());
    bool boss = creature->IsDungeonBoss() || creature->GetCreatureTemplate()->rank == CREATURE_ELITE_WORLDBOSS;
    bool elite = creature->GetCreatureTemplate()->rank != CREATURE_ELITE_NORMAL;
    return Calculate(context.roster.active, context.capacity, c.profiles[c.profile], c.limits,
        c.healthExponent, c.damageExponent, hp, damage,
        boss ? c.bossHealth : elite ? c.eliteHealth : 1.0,
        boss ? c.bossDamage : elite ? c.eliteDamage : 1.0);
}

void Apply(Creature* creature, Baseline const& baseline, Multipliers const& m)
{
    uint32 oldHealth = creature->GetHealth(), oldMax = creature->GetMaxHealth();
    uint32 oldMana = creature->GetPower(POWER_MANA), oldMaxMana = creature->GetMaxPower(POWER_MANA);
    creature->SetStatPctModifier(UNIT_MOD_HEALTH, BASE_PCT, float(baseline.health * m.health));
    creature->SetStatPctModifier(UNIT_MOD_ARMOR, BASE_PCT, float(baseline.armor * m.armor));
    creature->SetStatPctModifier(UNIT_MOD_MANA, BASE_PCT, float(baseline.mana * m.mana));
    creature->UpdateMaxHealth();
    creature->UpdateArmor();
    creature->UpdateMaxPower(POWER_MANA);
    creature->SetHealth(Preserve(oldHealth, oldMax, creature->GetMaxHealth(), creature->IsAlive()));
    // Mana may legitimately be zero.
    creature->SetPower(POWER_MANA, oldMaxMana ? uint32(double(oldMana) * creature->GetMaxPower(POWER_MANA) / oldMaxMana) : 0);
    if (!creature->IsInCombat())
        creature->ResetPlayerDamageReq();
}

class Controller
{
    std::mutex mutex;
    std::shared_ptr<Config const> config = std::make_shared<Config>();
    std::unordered_map<Map*, Context> maps;

    Context& Get(Map* map)
    {
        auto [it, inserted] = maps.try_emplace(map);
        Context& context = it->second;
        if (inserted)
        {
            context.config = config;
            context.pendingConfig = config;
            context.capacity = map->ToInstanceMap()->GetMaxPlayers();
        }
        return context;
    }

public:
    void Reload()
    {
        auto next = LoadConfig();
        std::lock_guard<std::mutex> lock(mutex);
        config = next;
        TC_LOG_INFO("server.loading", "SoloPlay: %s; profile=%u; changes wait for safe instance window", config->enabled ? "enabled" : "disabled", config->profile);
    }

    void UpdateMap(Map* map, uint32 diff)
    {
        if (!map->IsDungeon() || !map->ToInstanceMap()) return;
        std::lock_guard<std::mutex> lock(mutex);
        // Avoid retaining new map state while disabled.
        if (!config->enabled && !maps.count(map)) return;
        Context& c = Get(map);
        unsigned players = 0, highest = 0;
        bool combat = false;
        for (auto const& ref : map->GetPlayers())
            if (Player* p = ref.GetSource(); Participant(p))
            {
                ++players;
                highest = std::max(highest, unsigned(p->getLevel()));
                combat = combat || p->IsInCombat();
            }
        if (InstanceScript* instance = map->ToInstanceMap()->GetInstanceScript())
            combat = combat || instance->IsEncounterInProgress();
        for (auto const& entry : c.creatures)
            combat = combat || entry.first->IsInCombat();
        c.combat = combat;
        // Roster, level eligibility and config are committed in one revision.
        if (c.pendingLevel != highest || c.pendingConfig != config)
            c.roster.elapsed = 0;
        c.pendingConfig = config;
        c.pendingLevel = highest;
        bool rosterChanged = c.roster.Update(players, combat, diff, config->delay);
        if (!combat && (rosterChanged || c.roster.elapsed >= config->delay))
        {
            if (rosterChanged || c.highestLevel != highest || c.config != config)
            {
                c.highestLevel = highest;
                c.config = config;
                ++c.revision;
                // Publish one coherent context before a session can start the next pull.
                // Lazy creature updates could otherwise retain an old revision after combat starts.
                for (auto it = c.creatures.begin(); it != c.creatures.end();)
                {
                    Creature* creature = it->first;
                    if (!CreatureEligible(creature, c))
                    {
                        Apply(creature, it->second, {});
                        it = c.creatures.erase(it);
                        continue;
                    }
                    if (creature->IsAlive())
                    {
                        Multipliers m = Policy(map, c, creature);
                        Apply(creature, it->second, m);
                        it->second.multipliers = m;
                        it->second.revision = c.revision;
                    }
                    ++it;
                }
            }
        }
    }

    void UpdateCreature(Creature* creature)
    {
        Map* map = creature->FindMap();
        if (!creature->IsInWorld() || !map || !map->IsDungeon() || !map->ToInstanceMap()) return;
        std::lock_guard<std::mutex> lock(mutex);
        if (!config->enabled && !maps.count(map)) return;
        Context& c = Get(map);
        auto it = c.creatures.find(creature);
        if (it != c.creatures.end() && !CreatureEligible(creature, c))
        {
            Apply(creature, it->second, {});
            c.creatures.erase(it);
            return;
        }
        if (it == c.creatures.end())
        {
            if (!CreatureEligible(creature, c)) return;
            Baseline b{creature->GetPctModifierValue(UNIT_MOD_HEALTH, BASE_PCT),
                creature->GetPctModifierValue(UNIT_MOD_ARMOR, BASE_PCT),
                creature->GetPctModifierValue(UNIT_MOD_MANA, BASE_PCT)};
            it = c.creatures.emplace(creature, b).first;
        }
        if (it->second.revision == c.revision || !creature->IsAlive()) return;
        Multipliers m = Policy(map, c, creature);
        // An already fighting creature only gets its initial context, never a roster revision.
        if (creature->IsInCombat() && it->second.revision) return;
        Apply(creature, it->second, m);
        it->second.multipliers = m;
        it->second.revision = c.revision;
    }

    void ResetCreature(Creature* creature)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto map = maps.find(creature->FindMap());
        if (map == maps.end()) return;
        auto it = map->second.creatures.find(creature);
        if (it == map->second.creatures.end()) return;
        Apply(creature, it->second, {});
        map->second.creatures.erase(it);
    }

    void Destroy(Map* map)
    {
        std::lock_guard<std::mutex> lock(mutex);
        maps.erase(map);
    }

    void Event(Unit* source, Unit* target, uint32& amount, bool healing, DamageEffectType type, SpellInfo const* spell)
    {
        if (!source || !target || (!healing && source == target) || !source->IsInWorld() || !target->IsInWorld() ||
            source->GetMap() != target->GetMap() || (!healing && (type == NODAMAGE || type == SELF_DAMAGE))) return;
        // Owned/charmed units are never treated as PvE creatures, including in duels.
        bool sourcePlayer = source->IsCharmedOwnedByPlayerOrPlayer();
        bool targetPlayer = target->IsCharmedOwnedByPlayerOrPlayer();
        if ((!healing && sourcePlayer && targetPlayer) || (!sourcePlayer && !targetPlayer)) return;
        if (healing && (source->IsPvP() || target->IsPvP() ||
            (source->ToPlayer() && source->ToPlayer()->duel) || (target->ToPlayer() && target->ToPlayer()->duel))) return;
        std::lock_guard<std::mutex> lock(mutex);
        auto map = maps.find(source->GetMap());
        if (map == maps.end()) return;
        Context& c = map->second;
        // TCPP marks damage derived from another hit with this attribute. The parent hit
        // has already been scaled; repeating the multiplier would compound it.
        if (!healing && spell && spell->HasAttribute(SPELL_ATTR4_IGNORE_DAMAGE_TAKEN_MODIFIERS)) return;
        if (spell && (c.config->spells.count(spell->Id) || spell->HasEffect(SPELL_EFFECT_INSTAKILL) ||
            spell->HasEffect(SPELL_EFFECT_HEAL_PCT) || spell->HasEffect(SPELL_EFFECT_ENVIRONMENTAL_DAMAGE) ||
            spell->HasEffect(SPELL_EFFECT_DAMAGE_FROM_MAX_HEALTH_PCT) ||
            spell->HasAura(SPELL_AURA_OBS_MOD_HEALTH) ||
            spell->HasAura(SPELL_AURA_PERIODIC_DAMAGE_PERCENT) || spell->HasAura(SPELL_AURA_DAMAGE_SHIELD) ||
            spell->HasAura(SPELL_AURA_PROC_TRIGGER_DAMAGE))) return;
        if (healing && source->ToPlayer() && target->ToPlayer())
        {
            // Player healing uses the same profile and roster, without scaling either player's stats.
            if (Participant(source->ToPlayer()) && Participant(target->ToPlayer()) && Content(source->GetMap(), *c.config))
            {
                auto m = Calculate(c.roster.active, c.capacity, c.config->profiles[c.config->profile], c.config->limits,
                    c.config->healthExponent, c.config->damageExponent, 1, 1);
                // Avoid empowering trivial content even though no creature participates in this heal event.
                unsigned cap = source->GetMap()->GetEntry()->Expansion() < 1 ? 63 :
                    source->GetMap()->GetEntry()->Expansion() < 2 ? 73 :
                    source->GetMap()->GetEntry()->Expansion() < 3 ? 83 : 88;
                if (c.highestLevel < cap + c.config->levelGap)
                    amount = Amount(amount, m.playerHealing);
            }
            return;
        }
        Creature* creature = sourcePlayer ? target->ToCreature() : source->ToCreature();
        if (!creature || !CreatureEligible(creature, c)) return;
        auto it = c.creatures.find(creature);
        if (it == c.creatures.end() || !it->second.revision) return;
        Multipliers const& m = it->second.multipliers;
        if (healing)
        {
            // NPC-to-NPC healing is handled below separately; player-to-NPC healing is unchanged.
            return;
        }
        Player* player = sourcePlayer ? source->ToPlayer() : target->ToPlayer();
        if (!Participant(player)) return;
        amount = Amount(amount, sourcePlayer ? m.playerDamage : m.damage * m.incomingDamage);
    }

    void Heal(Unit* source, Unit* target, uint32& amount, SpellInfo const* spell)
    {
        if (source && target && source->IsInWorld() && target->IsInWorld() && source->ToCreature() && target->ToCreature() &&
            source->GetMap() == target->GetMap())
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto map = maps.find(source->GetMap());
            if (map == maps.end()) return;
            Context& c = map->second;
            if (!CreatureEligible(source->ToCreature(), c) || !CreatureEligible(target->ToCreature(), c) ||
                (spell && (c.config->spells.count(spell->Id) || spell->HasEffect(SPELL_EFFECT_HEAL_PCT) ||
                    spell->HasAura(SPELL_AURA_OBS_MOD_HEALTH)))) return;
            auto it = c.creatures.find(source->ToCreature());
            // NPC heals follow HP budget, avoiding an unscaled healer undoing all progress.
            if (it != c.creatures.end()) amount = Amount(amount, it->second.multipliers.healing);
            return;
        }
        Event(source, target, amount, true, DIRECT_DAMAGE, spell);
    }

    bool Status(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        std::lock_guard<std::mutex> lock(mutex);
        auto it = player ? maps.find(player->GetMap()) : maps.end();
        if (it == maps.end())
        {
            handler->PSendSysMessage("SoloPlay: enabled=%u; no tracked instance context.", unsigned(config->enabled));
            return true;
        }
        Context const& c = it->second;
        handler->PSendSysMessage("SoloPlay: enabled=%u map=%u instance=%u difficulty=%u profile=%u players=%u pending=%u capacity=%u combat=%u revision=%llu creatures=%u reloadPending=%u",
            unsigned(c.config->enabled), player->GetMapId(), player->GetInstanceId(), unsigned(player->GetMap()->GetDifficulty()),
            c.config->profile, c.roster.active, c.roster.pending, c.capacity, unsigned(c.combat),
            static_cast<unsigned long long>(c.revision), unsigned(c.creatures.size()), unsigned(c.config != config));
        if (Creature* selected = handler->getSelectedCreature())
        {
            auto creature = c.creatures.find(selected);
            if (creature != c.creatures.end())
            {
                auto const& m = creature->second.multipliers;
                handler->PSendSysMessage("SoloPlay: entry=%u HP=%g armor=%g mana=%g damage=%g healing=%g playerDamage=%g playerHealing=%g incoming=%g",
                    selected->GetEntry(), m.health, m.armor, m.mana, m.damage, m.healing, m.playerDamage, m.playerHealing, m.incomingDamage);
            }
        }
        return true;
    }
};

Controller controller;

class Lifecycle final : public UnitScript
{
public:
    Lifecycle() : UnitScript("solo_play_lifecycle") { }
    void OnCreatureScalingUpdate(Creature* c) override { controller.UpdateCreature(c); }
    void OnCreatureScalingReset(Creature* c) override { controller.ResetCreature(c); }
    void OnBeforeMapUpdate(Map* m, uint32 diff) override { controller.UpdateMap(m, diff); }
    void OnScalingMapDestroy(Map* m) override { controller.Destroy(m); }
    void OnScalingDamage(Unit* s, Unit* t, uint32& n, DamageEffectType type, SpellInfo const* spell) override
    { controller.Event(s, t, n, false, type, spell); }
    void OnBeforeHeal(Unit* s, Unit* t, uint32& n, SpellInfo const* spell) override
    { controller.Heal(s, t, n, spell); }
};

class World final : public WorldScript
{
public:
    World() : WorldScript("solo_play_world") { }
    void OnConfigLoad(bool) override { controller.Reload(); }
};

class Commands final : public CommandScript
{
public:
    Commands() : CommandScript("solo_play_commands") { }
    static bool Status(ChatHandler* handler, char const* args)
    {
        if (args && *args)
        {
            handler->PSendSysMessage("Usage: .soloplay (no subcommands)");
            return false;
        }
        return controller.Status(handler);
    }
    std::vector<ChatCommand> GetCommands() const override
    { return {{"soloplay", rbac::RBAC_PERM_COMMAND_SERVER_INFO, true, &Status, "Show SoloPlay instance and selected creature scaling."}}; }
};
}

void Addmod_solo_playScripts()
{
    new SoloPlay::Lifecycle();
    new SoloPlay::World();
    new SoloPlay::Commands();
}
