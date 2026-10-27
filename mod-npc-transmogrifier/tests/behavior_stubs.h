#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
using uint8=std::uint8_t; using uint32=std::uint32_t; using uint64=std::uint64_t;
using int32=std::int32_t; using int64=std::int64_t;
enum { EQUIPMENT_SLOT_HEAD=0, EQUIPMENT_SLOT_NECK, EQUIPMENT_SLOT_SHOULDERS, EQUIPMENT_SLOT_BODY,
 EQUIPMENT_SLOT_CHEST, EQUIPMENT_SLOT_WAIST, EQUIPMENT_SLOT_LEGS, EQUIPMENT_SLOT_FEET,
 EQUIPMENT_SLOT_WRISTS, EQUIPMENT_SLOT_HANDS, EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_FINGER2,
 EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2, EQUIPMENT_SLOT_BACK, EQUIPMENT_SLOT_MAINHAND,
 EQUIPMENT_SLOT_OFFHAND, EQUIPMENT_SLOT_RANGED, EQUIPMENT_SLOT_TABARD, EQUIPMENT_SLOT_END,
 EQUIPMENT_SLOT_START=0, INVENTORY_SLOT_BAG_0=255, INVENTORY_SLOT_BAG_START=19,
 INVENTORY_SLOT_BAG_END=23, INVENTORY_SLOT_ITEM_START=23, INVENTORY_SLOT_ITEM_END=39 };
enum { ITEM_CLASS_WEAPON=2, ITEM_CLASS_ARMOR=4 };
enum { ITEM_QUALITY_POOR, ITEM_QUALITY_NORMAL, ITEM_QUALITY_UNCOMMON, ITEM_QUALITY_RARE,
 ITEM_QUALITY_EPIC, ITEM_QUALITY_LEGENDARY, ITEM_QUALITY_ARTIFACT, ITEM_QUALITY_HEIRLOOM };
enum { INVTYPE_NON_EQUIP, INVTYPE_HEAD, INVTYPE_NECK, INVTYPE_SHOULDERS, INVTYPE_BODY,
 INVTYPE_CHEST, INVTYPE_WAIST, INVTYPE_LEGS, INVTYPE_FEET, INVTYPE_WRISTS, INVTYPE_HANDS,
 INVTYPE_FINGER, INVTYPE_TRINKET, INVTYPE_WEAPON, INVTYPE_SHIELD, INVTYPE_RANGED,
 INVTYPE_CLOAK, INVTYPE_2HWEAPON, INVTYPE_BAG, INVTYPE_TABARD, INVTYPE_ROBE,
 INVTYPE_WEAPONMAINHAND, INVTYPE_WEAPONOFFHAND, INVTYPE_HOLDABLE, INVTYPE_AMMO,
 INVTYPE_QUIVER, INVTYPE_RELIC };
enum { ITEM_SUBCLASS_WEAPON_BOW=2, ITEM_SUBCLASS_WEAPON_GUN=3,
 ITEM_SUBCLASS_WEAPON_CROSSBOW=18, ITEM_SUBCLASS_WEAPON_FISHING_POLE=20 };
enum { ITEM_FLAG2_FACTION_HORDE=1, ITEM_FLAG2_FACTION_ALLIANCE=2, TEAM_HORDE=1,
 TEAM_ALLIANCE=0, TRANSMOGRIFY_ENCHANTMENT_SLOT=9, ITEM_CHANGED=1,
 BIND_ON_EQUIP=2, BIND_ON_USE=3, GOSSIP_ICON_CHAT=0, GOSSIP_ICON_TALK=1,
 GOSSIP_MAX_MENU_ITEMS=32, DEFAULT_GOSSIP_MESSAGE=68, UNIT_NPC_FLAG_GOSSIP=1 };
using HolidayIds=uint32;
inline bool IsHolidayActive(HolidayIds) { return false; }
inline uint32 ItemQualityColors[8]={};
struct ObjectGuid {
 uint32 value=0;
 uint32 GetCounter() const { return value; }
 bool operator==(ObjectGuid const&) const = default;
};
struct ItemTemplate {
 uint32 id=100, cls=ITEM_CLASS_ARMOR, sub=1, quality=ITEM_QUALITY_RARE, inventory=INVTYPE_HEAD;
 uint32 flags=0, race=0, classMask=0, skill=0, rank=0, spell=0, level=0, holiday=0;
 uint32 GetId() const { return id; } uint32 GetClass() const { return cls; }
 uint32 GetSubClass() const { return sub; } uint32 GetQuality() const { return quality; }
 uint32 GetInventoryType() const { return inventory; } uint32 GetFlags2() const { return flags; }
 uint32 GetAllowableClass() const { return classMask; } uint32 GetAllowableRace() const { return race; }
 uint32 GetRequiredSkill() const { return skill; } uint32 GetRequiredSkillRank() const { return rank; }
 uint32 GetRequiredSpell() const { return spell; } uint32 GetRequiredLevel() const { return level; }
 uint32 GetHolidayID() const { return holiday; } uint32 GetBonding() const { return BIND_ON_EQUIP; }
 char const* GetDefaultLocaleName() const { return "Item"; }
};
struct Player;
struct Item {
 ItemTemplate proto; ObjectGuid guid{10}, owner{1}; uint32 appearance=0, price=10000;
 bool trade=false, bound=false, refundable=true, tradeable=true;
 ItemTemplate const* GetTemplate() const { return &proto; }
 uint32 GetEntry() const { return proto.id; } ObjectGuid GetGUID() const { return guid; }
 ObjectGuid GetOwnerGUID() const { return owner; } bool IsInTrade() const { return trade; }
 uint32 GetEnchantmentId(int) const { return appearance; }
 void ClearEnchantment(int) { appearance=0; }
 void SetEnchantment(int,uint32 entry,int,int) { appearance=entry; }
 void SetState(int,Player*) {} void UpdatePlayedTime(Player*) {}
 void SetNotRefundable(Player*) { refundable=false; }
 void ClearSoulboundTradeable(Player*) { tradeable=false; }
 void SetBinding(bool value) { bound=value; }
 uint32 GetSpecialPrice() const { return price; }
 static uint32 GetSpecialPrice(ItemTemplate const*) { return 10000; }
};
struct Bag { uint32 GetBagSize() const { return 4; } };
struct Creature;
struct GossipMenuItem { uint32 Sender, OptionType, BoxMoney; bool coded; };
struct GossipMenu {
 uint32 id=900101; std::vector<GossipMenuItem> items;
 GossipMenuItem const* GetItem(uint32 index) const { return index<items.size()? &items[index]:nullptr; }
 uint32 GetMenuId() const { return id; }
};
struct TalkClass {
 GossipMenu menu; struct { ObjectGuid SourceGuid; } interaction;
 void ClearMenus() { menu.items.clear(); }
 GossipMenu& GetGossipMenu() { return menu; }
 auto const& GetInteractionData() { return interaction; }
 bool IsGossipOptionCoded(uint32 i) { return menu.items.at(i).coded; }
};
struct Session { template<class... T> void SendNotification(T...) {} };
struct Transaction { std::vector<std::string> sql;
 template<class... T> void PAppend(char const* text,T...) { sql.push_back(text); }
};
struct Field { std::string value; uint8 GetUInt8() { return uint8(std::stoi(value)); } std::string GetString() { return value; } };
struct Result { Field fields[3]; Field* Fetch() { return fields; } bool NextRow() { return false; } };
using QueryResult=std::shared_ptr<Result>;
struct Database {
 int commits=0, escapes=0, deletes=0; std::vector<std::string> queries; QueryResult saved;
 template<class... T> QueryResult PQuery(char const* text,T...) { queries.push_back(text); return saved; }
 template<class... T> void DirectPExecute(char const*,T...) { ++deletes; }
 void EscapeString(std::string& s) { ++escapes; for(size_t i=0;(i=s.find('\'',i))!=std::string::npos;i+=2) s.insert(i,"'"); }
 auto BeginTransaction() { return std::make_shared<Transaction>(); }
 void CommitTransaction(std::shared_ptr<Transaction>) { ++commits; }
 void DirectCommitTransaction(std::shared_ptr<Transaction>&) { ++commits; }
};
inline Database CharacterDatabase;
struct Player {
 TalkClass talk; TalkClass* PlayerTalkClass=&talk; Session session;
 uint64 money=100000; uint32 tokens=10; int destroyed=0, saves=0, visible=0;
 bool combat=false, alive=true, closed=false; std::map<uint32,Item*> items; Bag* bag=nullptr;
 ObjectGuid GetGUID() const { return {1}; }
 Item* GetItemByPos(uint8 bag,uint32 slot) { auto i=items.find((uint32(bag)<<16)|slot); return i==items.end()?nullptr:i->second; }
 Bag* GetBagByPos(uint8 slot) { return slot==INVENTORY_SLOT_BAG_START?bag:nullptr; }
 void Put(uint32 slot,Item* item,uint32 bag=255) { items[(bag<<16)|slot]=item; }
 uint32 getClassMask() const { return 1; } uint32 getRaceMask() const { return 1; }
 uint32 GetTeamId() const { return 0; } uint32 GetSkillValue(uint32) const { return 10; }
 bool HasSpell(uint32) const { return false; } uint32 getLevel() const { return 85; }
 bool HasEnoughMoney(uint64 amount) const { return money>=amount; }
 void ModifyMoney(int64 amount) { money+=amount; }
 bool HasItemCount(uint32,uint32 count) { return tokens>=count; }
 void DestroyItemCount(uint32,uint32 count,bool) { tokens-=count; ++destroyed; }
 void SetVisibleItemSlot(uint8,Item*) { ++visible; }
 Session* GetSession() { return &session; }
 void SaveInventoryAndGoldToDB(std::shared_ptr<Transaction>&) { ++saves; }
 Creature* npc=nullptr;
 Creature* GetNPCIfCanInteractWith(ObjectGuid,int) { return npc; }
 bool IsInCombat() const { return combat; } bool IsAlive() const { return alive; }
};
inline void ClearGossipMenuFor(Player* p) { p->talk.ClearMenus(); }
inline void InitGossipMenuFor(Player* p,uint32 id) { p->talk.menu.id=id; }
inline void CloseGossipMenuFor(Player* p) { p->closed=true; }
inline void AddGossipItemFor(Player* p,int,std::string const&,uint32 sender,uint32 action,
 std::string const& = "",uint32 cost=0,bool coded=false) { p->talk.menu.items.push_back({sender,action,cost,coded}); }
inline void SendGossipMenuFor(Player* p,int,ObjectGuid guid) { p->talk.interaction.SourceGuid=guid; }
struct Creature { ObjectGuid GetGUID() const { return {500}; } };
struct CreatureAI {
 virtual ~CreatureAI()=default;
 virtual bool GossipHello(Player*) { return false; }
 virtual bool GossipSelect(Player*,uint32,uint32) { return false; }
 virtual bool GossipSelectCode(Player*,uint32,uint32,char const*) { return false; }
};
struct PassiveAI:CreatureAI { Creature* me; explicit PassiveAI(Creature* c):me(c) {} };
inline int registered=0;
struct CreatureScript { explicit CreatureScript(char const*) { ++registered; }
 virtual CreatureAI* GetAI(Creature*) const { return nullptr; } };
struct WorldScript { explicit WorldScript(char const*) { ++registered; } virtual void OnConfigLoad(bool) {} };
struct Config {
 bool GetBoolDefault(char const*,bool v) { return v; } int GetIntDefault(char const*,int v) { return v; }
 float GetFloatDefault(char const*,float v) { return v; }
 std::string GetStringDefault(char const*,char const* v) { return v; }
};
inline Config config; inline Config* sConfigMgr=&config;
struct ObjectMgr {
 std::map<uint32,ItemTemplate> templates;
 ItemTemplate const* GetItemTemplate(uint32 entry) { auto i=templates.find(entry); return i==templates.end()?nullptr:&i->second; }
};
inline ObjectMgr objectMgr; inline ObjectMgr* sObjectMgr=&objectMgr;
inline bool Utf8toWStr(std::string const&,std::wstring&) { return true; }
#define TC_LOG_INFO(...) ((void)0)
