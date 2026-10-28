#include "behavior_stubs.h"
#include "actual_script.cpp"
#include "actual_heal_info.h"
#include <cstdlib>
#include <limits>

unsigned checks=0;
void CheckResult(bool result, unsigned line) { ++checks; if (!result) { std::fprintf(stderr,"Failed check %u at line %u\n",checks,line); std::exit(1); } }
#define Check(value) CheckResult((value),__LINE__)
void Near(double a,double b,double epsilon=0.001) { Check(std::abs(a-b)<epsilon); }

int main()
{
    using namespace SoloPlay;
    Profile p;
    Limits l;
    auto solo=Calculate(1,5,p,l,.55,.35,1,1);
    auto duo=Calculate(2,5,p,l,.55,.35,1,1);
    Check(solo.health<duo.health && duo.health<1);
    for (auto capacity : {5u,10u,20u,25u,40u})
        for (auto count : {0u,1u,2u,3u,5u,40u})
        {
            auto m=Calculate(count,capacity,p,l,.55,.35,1,1);
            Check(std::isfinite(m.health) && m.health>=l.minHealth && m.health<=1);
            if (!count || count>2 || count>=capacity) Near(m.health,1);
        }
    p.playerDamage=1.25;
    auto budget=Calculate(1,5,p,l,.55,.35,1,1);
    Near(budget.health/budget.playerDamage,solo.health);
    Check(Amount(UINT32_MAX,4)==INT32_MAX);
    Check(Amount(UINT32_MAX,1)==UINT32_MAX);
    Check(Amount(100,std::numeric_limits<double>::quiet_NaN())==100);
    Check(Amount(100,std::numeric_limits<double>::infinity())==100);
    Check(Amount(0,.2)==0);
    Check(Preserve(1,10000,2,true)==1);
    Check(Preserve(0,10000,5000,false)==0);
    Check(Preserve(5000,10000,4000,true)==2000);

    Controller controller;
    Map map, other;
    Player one,two,three,gm;
    one.map=two.map=three.map=gm.map=&map;
    gm.gm=true;
    map.players={{&one},{&gm}};
    Creature mob;
    mob.map=&map;
    controller.UpdateMap(&map,5000);
    controller.UpdateCreature(&mob);
    Check(mob.maxHealth==10000); // disabled
    configMgr.values["SoloPlay.Enable"]="1";
    controller.Reload();
    controller.UpdateMap(&map,5000);
    controller.UpdateCreature(&mob);
    Near(mob.maxHealth,10000*solo.health,1.1);
    auto max=mob.maxHealth;
    mob.health=max/2;
    controller.UpdateCreature(&mob);
    Check(mob.maxHealth==max && mob.health==max/2); // no accumulation or refill
    uint32 damage=1000;
    controller.Event(&mob,&one,damage,false,DIRECT_DAMAGE,nullptr);
    Check(damage==Amount(1000,solo.damage*.9));
    uint32 heal=1000;
    controller.Heal(&one,&one,heal,nullptr);
    Check(heal==1150); // solo self-heal
    scriptMgr.heal=[&](Unit* s,Unit* t,uint32& n,SpellInfo const* spell) { controller.Heal(s,t,n,spell); };
    HealInfo directHeal(&one,&one,1000,nullptr,0);
    directHeal.ApplyScaling();
    directHeal.ApplyScaling();
    Check(directHeal.GetHeal()==1150); // actual HealInfo: no double amplification
    directHeal.AbsorbHeal(900);
    Check(directHeal.GetHeal()==250 && directHeal.GetAbsorb()==900);
    directHeal.SetEffectiveHeal(100);
    Check(directHeal.GetHeal()+directHeal.GetAbsorb()-directHeal.GetEffectiveHeal()==1050);
    HealInfo fullAbsorb(&one,&one,1000,nullptr,0);
    fullAbsorb.ApplyScaling();
    fullAbsorb.AbsorbHeal(2000);
    Check(fullAbsorb.GetHeal()==0 && fullAbsorb.GetAbsorb()==1150);
    ChatHandler handler;
    Check(!Commands::Status(&handler,"status"));
    Check(!Commands::Status(&handler,"mapstat"));
    Check(!Commands::Status(&handler,"creaturestat"));
    Check(Commands::Status(&handler,""));
    Commands commands;
    Check(commands.GetCommands().front().permission==rbac::RBAC_PERM_COMMAND_SERVER_INFO);
    heal=1000;
    one.duel=&one;
    controller.Heal(&one,&one,heal,nullptr);
    Check(heal==1000);
    one.duel=nullptr;
    one.combat=mob.combat=true;
    map.players.push_back({&two});
    controller.UpdateMap(&map,60000);
    controller.UpdateCreature(&mob);
    Check(mob.maxHealth==max); // late join locked
    map.players.clear();
    controller.UpdateMap(&map,60000);
    controller.UpdateCreature(&mob);
    Check(mob.maxHealth==max); // last disconnect locked by creature
    map.players={{&one},{&two}};
    one.combat=mob.combat=false;
    controller.UpdateMap(&map,5000);
    Near(mob.maxHealth,10000*duo.health,1.1); // revision committed before any creature tick
    controller.UpdateCreature(&mob);
    Near(mob.maxHealth,10000*duo.health,1.1);
    Near(double(mob.health)/mob.maxHealth,.5,.002);
    map.players.push_back({&three});
    controller.UpdateMap(&map,5000);
    controller.UpdateCreature(&mob);
    Check(mob.maxHealth==10000); // 3 players native
    map.players={{&one}};
    controller.UpdateMap(&map,5000);
    controller.UpdateCreature(&mob);
    max=mob.maxHealth;
    one.combat=mob.combat=true;
    configMgr.values["SoloPlay.Enable"]="0";
    controller.Reload();
    controller.UpdateMap(&map,60000);
    controller.UpdateCreature(&mob);
    Check(mob.maxHealth==max); // disable pending in combat
    one.combat=mob.combat=false;
    controller.UpdateMap(&map,4999);
    controller.UpdateCreature(&mob);
    Check(mob.maxHealth==max);
    controller.UpdateMap(&map,1);
    controller.UpdateCreature(&mob);
    Check(mob.maxHealth==10000); // safe disable restoration
    configMgr.values["SoloPlay.Enable"]="1";
    controller.Reload();
    controller.UpdateMap(&map,5000);
    controller.UpdateCreature(&mob);
    for (unsigned i=0;i<5;++i)
    {
        controller.ResetCreature(&mob);
        Check(mob.maxHealth==10000);
        controller.UpdateCreature(&mob);
        Near(mob.maxHealth,10000*solo.health,1.1);
    }
    mob.charmed=true;
    controller.UpdateCreature(&mob);
    Check(mob.maxHealth==10000);
    mob.charmed=false;
    for (bool* flag : {&mob.pet,&mob.guardian,&mob.summon,&mob.totem,&mob.vehicle,&mob.trigger,&mob.owned})
    {
        *flag=true;
        controller.UpdateCreature(&mob);
        Check(mob.maxHealth==10000);
        *flag=false;
    }
    controller.UpdateCreature(&mob);
    damage=1000;
    controller.Event(&one,&two,damage,false,SPELL_DIRECT_DAMAGE,nullptr);
    Check(damage==1000); // PvP
    damage=1000;
    controller.Event(&mob,&mob,damage,false,SPELL_DIRECT_DAMAGE,nullptr);
    Check(damage==1000); // reflection/self hit
    SpellInfo percent;
    percent.auras.insert(SPELL_AURA_PERIODIC_DAMAGE_PERCENT);
    damage=1000;
    controller.Event(&mob,&one,damage,false,DOT,&percent);
    Check(damage==1000);
    SpellInfo derived;
    derived.attributes=SPELL_ATTR4_IGNORE_DAMAGE_TAKEN_MODIFIERS;
    damage=1000;
    controller.Event(&mob,&one,damage,false,SPELL_DIRECT_DAMAGE,&derived);
    Check(damage==1000); // damage based on an already scaled hit
    damage=1000;
    controller.Event(&mob,&one,damage,false,NODAMAGE,nullptr);
    Check(damage==1000); // sharing not scaled twice
    damage=1000;
    controller.Event(nullptr,&one,damage,false,DIRECT_DAMAGE,nullptr);
    Check(damage==1000);
    two.map=&other;
    damage=1000;
    controller.Event(&mob,&two,damage,false,DIRECT_DAMAGE,nullptr);
    Check(damage==1000);
    mob.alive=false; mob.health=0;
    controller.ResetCreature(&mob);
    Check(mob.health==0 && mob.maxHealth==10000);
    controller.Destroy(&map);
    mob.alive=true; mob.health=10000;
    map.players={{&one}};
    controller.UpdateMap(&map,5000);
    controller.UpdateCreature(&mob);
    Near(mob.maxHealth,10000*solo.health,1.1); // reused map has no stale record
    Creature healer;
    healer.map=&map;
    controller.UpdateCreature(&healer);
    heal=1000;
    controller.Heal(&healer,&mob,heal,nullptr);
    Check(heal==Amount(1000,solo.health));
    heal=1000;
    SpellInfo percentHeal;
    percentHeal.auras.insert(SPELL_AURA_OBS_MOD_HEALTH);
    controller.Heal(&healer,&mob,heal,&percentHeal);
    Check(heal==1000);
    Map raid;
    raid.raid=raid.heroic=true;
    raid.capacity=25; raid.difficulty=3;
    Player raider;
    raider.map=&raid;
    raid.players={{&raider}};
    Creature raidBoss;
    raidBoss.map=&raid; raidBoss.boss=true;
    controller.UpdateMap(&raid,5000);
    controller.UpdateCreature(&raidBoss);
    auto raidPolicy=Calculate(1,25,Profile{},Limits{},.55,.35,1.1,1.1,1.2,1.1);
    Near(raidBoss.maxHealth,10000*raidPolicy.health,1.1);
    Check(mob.maxHealth!=raidBoss.maxHealth); // independent instance contexts
    controller.ResetCreature(&raidBoss);
    controller.Destroy(&raid);
    Creature unattached;
    controller.ResetCreature(&unattached); // reset before map assignment is safe
    controller.ResetCreature(&mob);
    map.entry.expansion=0; mob.level=60;
    controller.UpdateCreature(&mob);
    Check(mob.maxHealth==10000); // trivial legacy
    configMgr.values["SoloPlay.Profile"]="Invalid";
    Check(!LoadConfig()->enabled);
    configMgr.values["SoloPlay.HealthExponent"]="nan";
    Near(LoadConfig()->healthExponent,.55);
    configMgr.values["SoloPlay.ExcludedCreatures"]="1,broken,-2,4294967296";
    Check(LoadConfig()->creatures.size()==1);
    configMgr.values["SoloPlay.MapOverrides"]="1:0.8:1.2;2:nan:1;3:0:1;4:inf:1";
    Check(LoadConfig()->mapFactors.size()==1);
    configMgr.values.clear();
    configMgr.values["SoloPlay.Enable"]="1";
    configMgr.values["SoloPlay.Profile"]="Solo";
    auto soloConfig=LoadConfig();
    Check(soloConfig->profile==1 && soloConfig->profiles[1].maxPlayers==1);
    Near(Calculate(2,5,soloConfig->profiles[1],l,.55,.35,1,1).health,1);
    Check(Calculate(1,5,soloConfig->profiles[1],l,.55,.35,1,1).incomingDamage<1);
    configMgr.values["SoloPlay.Profile"]="Duo";
    auto duoConfig=LoadConfig();
    Check(duoConfig->profile==2 && duoConfig->profiles[2].minPlayers==2);
    Near(Calculate(1,5,duoConfig->profiles[2],l,.55,.35,1,1).health,1);
    Check(Calculate(2,5,duoConfig->profiles[2],l,.55,.35,1,1).health<1);
    configMgr.values["SoloPlay.MaximumMultiplier"]="0.5";
    configMgr.values["SoloPlay.MinimumArmor"]="nan";
    auto bounds=LoadConfig();
    Check(bounds->limits.minArmor<=bounds->limits.maximum);
    configMgr.values.clear();
    Check(!LoadConfig()->enabled);
    std::printf("%u behavioral checks passed\n",checks);
}
