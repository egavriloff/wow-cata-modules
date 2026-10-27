#include "behavior_stubs.h"
#include "actual_script.cpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <source_location>

int checks=0;
void Check(bool value, std::source_location location=std::source_location::current()) {
 ++checks; if (!value) { std::cerr << "failed check " << checks << " at line " << location.line(); std::exit(1); }
}

struct Fixture {
 Player player; Creature creature; Item target,source;
 npc_transmogrifier::AI ai{&creature};
 Fixture() {
   g_TransmogConfig=TransmogConfig(); CharacterDatabase=Database();
   player.npc=&creature; source.proto.id=200; source.guid.value=20;
   player.Put(0,&target); player.Put(INVENTORY_SLOT_ITEM_START,&source);
   objectMgr.templates[100]=target.proto; objectMgr.templates[200]=source.proto;
   objectMgr.templates[300]=source.proto;
 }
 uint32 Find(uint32 sender,uint32 action) {
   auto const& items=player.talk.menu.items;
   for (uint32 i=0;i<items.size();++i) if (items[i].Sender==sender && items[i].OptionType==action) return i;
   Check(false); return 0;
 }
 void Open() {
   ai.GossipHello(&player);
   ai.GossipSelect(&player,900101,Find(SENDER_SLOT,0));
 }
 void Apply() { ai.GossipSelect(&player,900101,Find(0,20)); }
 void Saved(std::string data) {
   auto row=std::make_shared<Result>(); row->fields[0].value="0";
   row->fields[1].value="Test"; row->fields[2].value=data; CharacterDatabase.saved=row;
   ai.GossipHello(&player);
   ai.GossipSelect(&player,900101,Find(SENDER_SETS,0));
   ai.GossipSelect(&player,900101,Find(SENDER_SET_VIEW,0));
 }
};

int main() {
 using namespace TransmogValidation;
 Check(Cost(10000,1,0)==10000); Check(Cost(10000,0,0)==0);
 Check(Cost(10000,-1,0)==UINT32_MAX); Check(Cost(1,1,-1)==UINT32_MAX);
 Check(Cost(1,std::numeric_limits<double>::quiet_NaN(),0)==UINT32_MAX);
 Check(Cost(UINT64_MAX,1,0)==UINT32_MAX);
 std::vector<Appearance> values;
 Check(Parse("0 200 4 300 ",values) && values.size()==2);
 for (auto text:{"", "0", "0 1 0 2", "19 1", "-1 1", "0 0", "0 4294967296", "0 1 rubbish"}) Check(!Parse(text,values));
 Check(Name("A'B")); Check(!Name("|Hitem:1")); Check(!Name("A\nB"));
 { Fixture f; Check(CanTransmogTemplates(&f.player,&f.target.proto,&f.source.proto));
   f.source.proto.quality=ITEM_QUALITY_LEGENDARY; Check(!CanTransmogTemplates(&f.player,&f.target.proto,&f.source.proto));
   g_TransmogConfig.AllowLegendary=true; Check(CanTransmogTemplates(&f.player,&f.target.proto,&f.source.proto));
   f.source.proto.classMask=2; Check(!SuitableForTransmog(&f.player,&f.source.proto));
   g_TransmogConfig.IgnoreReqClass=true; Check(SuitableForTransmog(&f.player,&f.source.proto));
   f.source.proto.race=2; Check(!SuitableForTransmog(&f.player,&f.source.proto));
   f.source.proto.race=0; f.source.proto.skill=1; f.source.proto.rank=11; Check(!SuitableForTransmog(&f.player,&f.source.proto));
   f.source.proto.skill=0; f.source.proto.spell=1; Check(!SuitableForTransmog(&f.player,&f.source.proto));
   f.source.proto.spell=0; f.source.proto.level=86; Check(!SuitableForTransmog(&f.player,&f.source.proto));
 }
 { Fixture f; f.target.proto.inventory=INVTYPE_SHIELD; f.source.proto.cls=ITEM_CLASS_WEAPON;
   Check(!CanTransmogTemplates(&f.player,&f.target.proto,&f.source.proto)); }
 { Fixture f; f.Open(); f.Apply(); Check(f.target.appearance==200); Check(f.player.money==90000);
   Check(f.source.bound && !f.source.refundable && !f.target.tradeable);
   Check(f.source.owner.value==1 && f.target.owner.value==1); Check(f.player.saves==1);
   auto money=f.player.money; f.ai.GossipSelect(&f.player,900101,0); Check(f.player.money==money); }
 for (int attack=0;attack<9;++attack) {
   Fixture f; f.Open(); auto id=f.Find(0,20);
   if(attack==0) f.source.owner.value=2;
   if(attack==1) f.target.owner.value=2;
   if(attack==2) f.source.trade=true;
   if(attack==3) f.player.money=1;
   if(attack==4) { g_TransmogConfig.RequireToken=true; f.player.tokens=0; }
   if(attack==5) f.player.talk.interaction.SourceGuid.value=999;
   if(attack==6) f.player.combat=true;
   if(attack==7) f.target.price=20000;
   auto money=f.player.money;
   f.ai.GossipSelect(&f.player,attack==8?900102:900101,id);
   Check(f.target.appearance==0 && f.player.money==money && !f.player.destroyed);
 }
 { Fixture f; g_TransmogConfig.RequireToken=true; f.Open(); f.Apply();
   Check(f.target.appearance==200 && f.player.destroyed==1 && f.player.tokens==9); }
 { Fixture f; g_TransmogConfig.RequireToken=true; g_TransmogConfig.TokenEntry=200;
   f.Open(); f.Apply(); Check(f.target.appearance==0 && f.player.destroyed==0); }
 { Fixture f; f.Open(); f.source.proto.quality=ITEM_QUALITY_POOR; f.Apply(); Check(f.player.money==100000); }
 { Fixture f; f.target.appearance=200; f.ai.GossipHello(&f.player);
   f.ai.GossipSelect(&f.player,900101,f.Find(SENDER_REMOVE_ALL,0)); Check(!f.target.appearance && f.player.saves==1); }
 { Fixture f; f.Saved("0 200"); f.ai.GossipSelect(&f.player,900101,f.Find(SENDER_SET_APPLY,0));
   Check(f.target.appearance==200 && f.player.money==100000 && f.player.saves==1); }
 { Fixture f; f.Saved("0 200 4 300"); f.ai.GossipSelect(&f.player,900101,f.Find(SENDER_SET_APPLY,0));
   Check(f.target.appearance==0 && f.player.saves==0); }
 { Fixture f; f.Saved("0 200"); CharacterDatabase.saved->fields[2].value="0 200 0 100";
   f.ai.GossipSelect(&f.player,900101,f.Find(SENDER_SET_APPLY,0)); Check(f.target.appearance==0); }
 { Fixture f; f.target.appearance=200; std::string data; uint64 cost;
   Check(BuildCurrentSet(&f.player,data,cost) && data=="0 200 " && cost==30000); }
 { Fixture f; Bag bag; f.player.bag=&bag;
   f.player.items.erase((255u<<16)|INVENTORY_SLOT_ITEM_START);
   f.player.Put(0,&f.source,INVENTORY_SLOT_BAG_START);
   f.Open(); f.Apply(); Check(f.target.appearance==200); }
 { Fixture f; f.target.appearance=200; f.ai.GossipHello(&f.player);
   f.ai.GossipSelect(&f.player,900101,f.Find(SENDER_SETS,0));
   f.ai.GossipSelect(&f.player,900101,f.Find(SENDER_SET_SAVE_MENU,0));
   f.ai.GossipSelectCode(&f.player,900101,f.Find(SENDER_SET_SAVE_CODE,0),"A'B");
   Check(CharacterDatabase.escapes==1 && f.player.money==70000 && f.player.saves==1); }
 { Fixture f; f.target.appearance=200; f.ai.GossipHello(&f.player);
   f.ai.GossipSelect(&f.player,900101,f.Find(SENDER_SETS,0));
   f.ai.GossipSelect(&f.player,900101,f.Find(SENDER_SET_SAVE_MENU,0));
   f.ai.GossipSelectCode(&f.player,900101,f.Find(SENDER_SET_SAVE_CODE,0),"|Hitem:1");
   Check(f.player.money==100000 && f.player.saves==0); }
 { Fixture f; f.Saved("0 200");
   f.ai.GossipSelect(&f.player,900101,f.Find(SENDER_SET_DELETE,0));
   Check(CharacterDatabase.deletes==1 && f.player.money==100000); }
 { Fixture f; f.ai.GossipHello(&f.player);
   f.ai.GossipSelect(&f.player,900101,999); Check(f.player.closed && f.player.money==100000); }
 Check(std::string(GetSlotName(EQUIPMENT_SLOT_RANGED))=="\xD0\x94\xD0\xB0\xD0\xBB\xD1\x8C\xD0\xBD\xD0\xB8\xD0\xB9 \xD0\xB1\xD0\xBE\xD0\xB9");
 Addmod_npc_transmogrifierScripts(); Check(registered==2);
 std::cout << checks << " behavioral checks passed\n";
}
