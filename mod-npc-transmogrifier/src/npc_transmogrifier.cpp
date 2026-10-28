/*
 * This file is part of the Pandaria 5.4.8 Project. See THANKS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details. See COPYING for the full license.
 *
 * Adapted from the supplied SkyFire 5.4.8 NPC transmogrifier module v4,
 * derived from alexkulya/pandaria_5.4.8. See NOTICE.md.
 * TCPP uses native enchantment slot storage, without the SkyFire mirror.
 */

#include "Bag.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameEventMgr.h"
#include "Item.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"
#include "PassiveAI.h"
#include "Creature.h"
#include "Util.h"
#include "transmog_validation.h"

#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
enum GossipSender {
  // Main-menu slot selector.  Source-item entries still use sender == actual slot
  // (< EQUIPMENT_SLOT_END), matching the original Rochet2/Legends routing.
  SENDER_SLOT = EQUIPMENT_SLOT_END,
  SENDER_MAIN = EQUIPMENT_SLOT_END + 1,
  SENDER_REMOVE_ALL = EQUIPMENT_SLOT_END + 2,
  SENDER_REMOVE_ONE = EQUIPMENT_SLOT_END + 3,
  SENDER_SETS = EQUIPMENT_SLOT_END + 4,
  SENDER_SET_VIEW = EQUIPMENT_SLOT_END + 5,
  SENDER_SET_APPLY = EQUIPMENT_SLOT_END + 6,
  SENDER_SET_DELETE = EQUIPMENT_SLOT_END + 7,
  SENDER_SET_SAVE_MENU = EQUIPMENT_SLOT_END + 8,
  SENDER_SET_SAVE_CODE = EQUIPMENT_SLOT_END + 9
};

static uint8 const HARD_MAX_SETS = 10;
// Reserve removal, Back, Close and an optional empty-result message.
static uint32 const MAX_SOURCE_OPTIONS = GOSSIP_MAX_MENU_ITEMS - 4;

struct TransmogConfig {
    bool EnableSets;
    uint8 MaxSets;
    float SetCostModifier;
    int32 SetCopperCost;

    float ScaledCostModifier;
    int32 CopperCost;

    bool RequireToken;
    uint32 TokenEntry;
    uint32 TokenAmount;

    bool AllowPoor;
    bool AllowCommon;
    bool AllowUncommon;
    bool AllowRare;
    bool AllowEpic;
    bool AllowLegendary;
    bool AllowArtifact;
    bool AllowHeirloom;

    bool AllowMixedArmorTypes;
    bool AllowMixedWeaponTypes;
    bool AllowFishingPoles;

    bool IgnoreReqRace;
    bool IgnoreReqClass;
    bool IgnoreReqSkill;
    bool IgnoreReqSpell;
    bool IgnoreReqLevel;
    bool IgnoreReqEvent;
    bool IgnoreReqStats;

    std::set<uint32> Allowed;
    std::set<uint32> NotAllowed;

    TransmogConfig()
        : EnableSets(true), MaxSets(10), SetCostModifier(3.0f), SetCopperCost(0), ScaledCostModifier(1.0f),
          CopperCost(0), RequireToken(false), TokenEntry(49426), TokenAmount(1), AllowPoor(false), AllowCommon(false),
          AllowUncommon(true), AllowRare(true), AllowEpic(true), AllowLegendary(false), AllowArtifact(false),
          AllowHeirloom(true), AllowMixedArmorTypes(false), AllowMixedWeaponTypes(false), AllowFishingPoles(false),
          IgnoreReqRace(false), IgnoreReqClass(false), IgnoreReqSkill(false), IgnoreReqSpell(false),
          IgnoreReqLevel(false), IgnoreReqEvent(false), IgnoreReqStats(false) {}

    static void ParseList(std::string const& value, std::set<uint32>& out) {
      out.clear();
      std::istringstream in(value);
      uint32 entry = 0;
      while (in >> entry)
        out.insert(entry);
    }

    void Load() {
      EnableSets = sConfigMgr->GetBoolDefault("Transmogrification.EnableSets", true);
      int maxSets = sConfigMgr->GetIntDefault("Transmogrification.MaxSets", 10);
      if (maxSets < 0)
        maxSets = 0;
      if (maxSets > HARD_MAX_SETS)
        maxSets = HARD_MAX_SETS;
      MaxSets = uint8(maxSets);
      SetCostModifier = sConfigMgr->GetFloatDefault("Transmogrification.SetCostModifier", 3.0f);
      SetCopperCost = sConfigMgr->GetIntDefault("Transmogrification.SetCopperCost", 0);

      ScaledCostModifier = sConfigMgr->GetFloatDefault("Transmogrification.ScaledCostModifier", 1.0f);
      CopperCost = sConfigMgr->GetIntDefault("Transmogrification.CopperCost", 0);

      RequireToken = sConfigMgr->GetBoolDefault("Transmogrification.RequireToken", false);
      TokenEntry = uint32(std::max(0, sConfigMgr->GetIntDefault("Transmogrification.TokenEntry", 49426)));
      TokenAmount = uint32(std::max(0, sConfigMgr->GetIntDefault("Transmogrification.TokenAmount", 1)));

      AllowPoor = sConfigMgr->GetBoolDefault("Transmogrification.AllowPoor", false);
      AllowCommon = sConfigMgr->GetBoolDefault("Transmogrification.AllowCommon", false);
      AllowUncommon = sConfigMgr->GetBoolDefault("Transmogrification.AllowUncommon", true);
      AllowRare = sConfigMgr->GetBoolDefault("Transmogrification.AllowRare", true);
      AllowEpic = sConfigMgr->GetBoolDefault("Transmogrification.AllowEpic", true);
      AllowLegendary = sConfigMgr->GetBoolDefault("Transmogrification.AllowLegendary", false);
      AllowArtifact = sConfigMgr->GetBoolDefault("Transmogrification.AllowArtifact", false);
      AllowHeirloom = sConfigMgr->GetBoolDefault("Transmogrification.AllowHeirloom", true);

      AllowMixedArmorTypes = sConfigMgr->GetBoolDefault("Transmogrification.AllowMixedArmorTypes", false);
      AllowMixedWeaponTypes = sConfigMgr->GetBoolDefault("Transmogrification.AllowMixedWeaponTypes", false);
      AllowFishingPoles = sConfigMgr->GetBoolDefault("Transmogrification.AllowFishingPoles", false);

      IgnoreReqRace = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqRace", false);
      IgnoreReqClass = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqClass", false);
      IgnoreReqSkill = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqSkill", false);
      IgnoreReqSpell = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqSpell", false);
      IgnoreReqLevel = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqLevel", false);
      IgnoreReqEvent = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqEvent", false);
      IgnoreReqStats = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqStats", false);

      ParseList(sConfigMgr->GetStringDefault("Transmogrification.Allowed", ""), Allowed);
      ParseList(sConfigMgr->GetStringDefault("Transmogrification.NotAllowed", ""), NotAllowed);
    }
};

static TransmogConfig g_TransmogConfig;

static void LoadModuleConfig() {
  g_TransmogConfig.Load();
  TC_LOG_INFO("server.loading", "Transmogrifier: configuration read from worldserver.conf");
}

struct SavedSet {
    uint8 Id;
    std::string Name;
    std::string Data;
};

static char const* GetSlotName(uint8 slot) {
  switch (slot) {
  case EQUIPMENT_SLOT_HEAD:
    return "Голова";
  case EQUIPMENT_SLOT_SHOULDERS:
    return "Плечи";
  case EQUIPMENT_SLOT_BODY:
    return "Рубашка";
  case EQUIPMENT_SLOT_CHEST:
    return "Грудь";
  case EQUIPMENT_SLOT_WAIST:
    return "Пояс";
  case EQUIPMENT_SLOT_LEGS:
    return "Ноги";
  case EQUIPMENT_SLOT_FEET:
    return "Ступни";
  case EQUIPMENT_SLOT_WRISTS:
    return "Запястья";
  case EQUIPMENT_SLOT_HANDS:
    return "Кисти рук";
  case EQUIPMENT_SLOT_BACK:
    return "Спина";
  case EQUIPMENT_SLOT_MAINHAND:
    return "Правая рука";
  case EQUIPMENT_SLOT_OFFHAND:
    return "Левая рука";
  case EQUIPMENT_SLOT_RANGED:
    return "Дальний бой";
  case EQUIPMENT_SLOT_TABARD:
    return "Гербовая накидка";
  default:
    return NULL;
  }
}

static std::string GetSlotIcon(uint8 slot) {
  std::ostringstream ss;
  ss << "|TInterface/PaperDoll/";
  switch (slot) {
  case EQUIPMENT_SLOT_HEAD:
    ss << "UI-PaperDoll-Slot-Head";
    break;
  case EQUIPMENT_SLOT_SHOULDERS:
    ss << "UI-PaperDoll-Slot-Shoulder";
    break;
  case EQUIPMENT_SLOT_BODY:
    ss << "UI-PaperDoll-Slot-Shirt";
    break;
  case EQUIPMENT_SLOT_CHEST:
    ss << "UI-PaperDoll-Slot-Chest";
    break;
  case EQUIPMENT_SLOT_WAIST:
    ss << "UI-PaperDoll-Slot-Waist";
    break;
  case EQUIPMENT_SLOT_LEGS:
    ss << "UI-PaperDoll-Slot-Legs";
    break;
  case EQUIPMENT_SLOT_FEET:
    ss << "UI-PaperDoll-Slot-Feet";
    break;
  case EQUIPMENT_SLOT_WRISTS:
    ss << "UI-PaperDoll-Slot-Wrists";
    break;
  case EQUIPMENT_SLOT_HANDS:
    ss << "UI-PaperDoll-Slot-Hands";
    break;
  case EQUIPMENT_SLOT_BACK:
    ss << "UI-PaperDoll-Slot-Chest";
    break;
  case EQUIPMENT_SLOT_MAINHAND:
    ss << "UI-PaperDoll-Slot-MainHand";
    break;
  case EQUIPMENT_SLOT_OFFHAND:
    ss << "UI-PaperDoll-Slot-SecondaryHand";
    break;
  case EQUIPMENT_SLOT_RANGED:
    ss << "UI-PaperDoll-Slot-Ranged";
    break;
  case EQUIPMENT_SLOT_TABARD:
    ss << "UI-PaperDoll-Slot-Tabard";
    break;
  default:
    ss << "UI-Backpack-EmptySlot";
    break;
  }
  ss << ":16:16|t";
  return ss.str();
}

static std::string GetItemLink(ItemTemplate const* proto) {
  if (!proto)
    return "<unknown item>";

  std::ostringstream ss;
  ss << "|c" << std::hex << ItemQualityColors[proto->GetQuality()] << std::dec << "|Hitem:" << proto->GetId()
     << ":0:0:0:0:0:0:0:0:0|h[" << proto->GetDefaultLocaleName() << "]|h|r";
  return ss.str();
}

static std::string GetItemLink(Item const* item) {
  return item ? GetItemLink(item->GetTemplate()) : "<unknown item>";
}

static uint32 GetTransmogEntry(Item const* item) {
  return item ? item->GetEnchantmentId(TRANSMOGRIFY_ENCHANTMENT_SLOT) : 0;
}

static bool IsRangedWeapon(ItemTemplate const* proto) {
  if (!proto || proto->GetClass() != ITEM_CLASS_WEAPON)
    return false;

  return proto->GetSubClass() == ITEM_SUBCLASS_WEAPON_BOW || proto->GetSubClass() == ITEM_SUBCLASS_WEAPON_GUN ||
         proto->GetSubClass() == ITEM_SUBCLASS_WEAPON_CROSSBOW;
}

static bool IsAllowedQuality(uint32 quality) {
  switch (quality) {
  case ITEM_QUALITY_POOR:
    return g_TransmogConfig.AllowPoor;
  case ITEM_QUALITY_NORMAL:
    return g_TransmogConfig.AllowCommon;
  case ITEM_QUALITY_UNCOMMON:
    return g_TransmogConfig.AllowUncommon;
  case ITEM_QUALITY_RARE:
    return g_TransmogConfig.AllowRare;
  case ITEM_QUALITY_EPIC:
    return g_TransmogConfig.AllowEpic;
  case ITEM_QUALITY_LEGENDARY:
    return g_TransmogConfig.AllowLegendary;
  case ITEM_QUALITY_ARTIFACT:
    return g_TransmogConfig.AllowArtifact;
  case ITEM_QUALITY_HEIRLOOM:
    return g_TransmogConfig.AllowHeirloom;
  default:
    return false;
  }
}

static bool SuitableForTransmog(Player* player, ItemTemplate const* proto) {
  if (!player || !proto)
    return false;

  if (proto->GetClass() != ITEM_CLASS_ARMOR && proto->GetClass() != ITEM_CLASS_WEAPON)
    return false;

  if (g_TransmogConfig.Allowed.find(proto->GetId()) != g_TransmogConfig.Allowed.end())
    return true;

  if (g_TransmogConfig.NotAllowed.find(proto->GetId()) != g_TransmogConfig.NotAllowed.end())
    return false;

  if (!g_TransmogConfig.AllowFishingPoles && proto->GetClass() == ITEM_CLASS_WEAPON &&
      proto->GetSubClass() == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
    return false;

  if (!IsAllowedQuality(proto->GetQuality()))
    return false;

  if ((proto->GetFlags2() & ITEM_FLAG2_FACTION_HORDE) && player->GetTeamId() != TEAM_HORDE)
    return false;
  if ((proto->GetFlags2() & ITEM_FLAG2_FACTION_ALLIANCE) && player->GetTeamId() != TEAM_ALLIANCE)
    return false;

  if (!g_TransmogConfig.IgnoreReqClass && proto->GetAllowableClass() &&
      (proto->GetAllowableClass() & player->getClassMask()) == 0)
    return false;
  if (!g_TransmogConfig.IgnoreReqRace && proto->GetAllowableRace() && (proto->GetAllowableRace() & player->getRaceMask()) == 0)
    return false;

  if (!g_TransmogConfig.IgnoreReqSkill && proto->GetRequiredSkill()) {
    uint32 skill = player->GetSkillValue(proto->GetRequiredSkill());
    if (!skill || skill < proto->GetRequiredSkillRank())
      return false;
  }

  if (!g_TransmogConfig.IgnoreReqSpell && proto->GetRequiredSpell() && !player->HasSpell(proto->GetRequiredSpell()))
    return false;
  if (!g_TransmogConfig.IgnoreReqLevel && proto->GetRequiredLevel() && player->getLevel() < proto->GetRequiredLevel())
    return false;
  if (!g_TransmogConfig.IgnoreReqEvent && proto->GetHolidayID() && !IsHolidayActive((HolidayIds)proto->GetHolidayID()))
    return false;

  return true;
}

// Template-level compatibility is required for saved sets because the source
// item may no longer be in the player's bags when the set is applied.
static bool CanTransmogTemplates(Player* player, ItemTemplate const* target, ItemTemplate const* source) {
  if (!player || !target || !source)
    return false;
  if (target->GetId() == source->GetId())
    return false;
  if (!SuitableForTransmog(player, target) || !SuitableForTransmog(player, source))
    return false;

  if (target->GetInventoryType() == INVTYPE_NON_EQUIP || source->GetInventoryType() == INVTYPE_NON_EQUIP ||
      source->GetInventoryType() == INVTYPE_BAG || source->GetInventoryType() == INVTYPE_RELIC ||
      source->GetInventoryType() == INVTYPE_FINGER || source->GetInventoryType() == INVTYPE_TRINKET ||
      source->GetInventoryType() == INVTYPE_AMMO || source->GetInventoryType() == INVTYPE_QUIVER)
    return false;

  if (source->GetClass() != target->GetClass())
    return false;

  if (source->GetSubClass() != target->GetSubClass() && !(IsRangedWeapon(target) && IsRangedWeapon(source))) {
    if (source->GetClass() == ITEM_CLASS_ARMOR && !g_TransmogConfig.AllowMixedArmorTypes)
      return false;
    if (source->GetClass() == ITEM_CLASS_WEAPON && !g_TransmogConfig.AllowMixedWeaponTypes)
      return false;
  }

  if (source->GetInventoryType() != target->GetInventoryType()) {
    if (source->GetClass() == ITEM_CLASS_WEAPON) {
      bool targetRanged = IsRangedWeapon(target);
      bool sourceRanged = IsRangedWeapon(source);
      if (targetRanged != sourceRanged)
        return false;

      bool targetHand = target->GetInventoryType() == INVTYPE_WEAPON || target->GetInventoryType() == INVTYPE_WEAPONMAINHAND ||
                        target->GetInventoryType() == INVTYPE_WEAPONOFFHAND || target->GetInventoryType() == INVTYPE_2HWEAPON;
      bool sourceHand = source->GetInventoryType() == INVTYPE_WEAPON || source->GetInventoryType() == INVTYPE_WEAPONMAINHAND ||
                        source->GetInventoryType() == INVTYPE_WEAPONOFFHAND || source->GetInventoryType() == INVTYPE_2HWEAPON;
      if (!targetHand || !sourceHand)
        return false;
    } else if (source->GetClass() == ITEM_CLASS_ARMOR) {
      bool chestRobe = (source->GetInventoryType() == INVTYPE_CHEST && target->GetInventoryType() == INVTYPE_ROBE) ||
                       (source->GetInventoryType() == INVTYPE_ROBE && target->GetInventoryType() == INVTYPE_CHEST);
      if (!chestRobe)
        return false;
    }
  }

  return true;
}

static uint32 GetTransmogCost(Item const* target) {
  if (!target)
    return 0;
  return TransmogValidation::Cost(target->GetSpecialPrice(), g_TransmogConfig.ScaledCostModifier,
                                 g_TransmogConfig.CopperCost);
}

static void RemoveTransmog(Player* player, uint8 slot, Item* item) {
  if (!player || !item || item->GetOwnerGUID() != player->GetGUID() || item->IsInTrade())
    return;

  item->ClearEnchantment(TRANSMOGRIFY_ENCHANTMENT_SLOT);
  item->SetState(ITEM_CHANGED, player);
  player->SetVisibleItemSlot(slot, item);

}

static void ApplyEntry(Player* player, uint8 slot, Item* target, uint32 sourceEntry) {
  if (!player || !target || !sourceEntry || target->GetOwnerGUID() != player->GetGUID() || target->IsInTrade())
    return;

  target->SetEnchantment(TRANSMOGRIFY_ENCHANTMENT_SLOT, sourceEntry, 0, 0);
  target->UpdatePlayedTime(player);

  target->SetNotRefundable(player);
  target->ClearSoulboundTradeable(player);
  target->SetState(ITEM_CHANGED, player);
  player->SetVisibleItemSlot(slot, target);

}

static void ApplyFromItem(Player* player, uint8 slot, Item* target, Item* source) {
  ApplyEntry(player, slot, target, source->GetEntry());

  if (source->GetTemplate()->GetBonding() == BIND_ON_EQUIP || source->GetTemplate()->GetBonding() == BIND_ON_USE)
    source->SetBinding(true);


  source->SetNotRefundable(player);
  source->ClearSoulboundTradeable(player);
  source->SetState(ITEM_CHANGED, player);
}

static Item* FindInventoryItemByLowGuid(Player* player, uint32 lowGuid) {
  for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
      if (item->GetGUID().GetCounter() == lowGuid)
        return item;

  for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot) {
    Bag* bag = player->GetBagByPos(bagSlot);
    if (!bag)
      continue;

    for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
      if (Item* item = player->GetItemByPos(bagSlot, slot))
        if (item->GetGUID().GetCounter() == lowGuid)
          return item;
  }

  return NULL;
}

static std::vector<SavedSet> LoadSets(uint32 ownerGuid) {
  std::vector<SavedSet> sets;
  QueryResult result = CharacterDatabase.PQuery("SELECT `PresetID`,`SetName`,`SetData` FROM "
                                                "`custom_transmogrification_sets` WHERE `Owner`=%u ORDER BY `PresetID`",
                                                ownerGuid);

  if (!result)
    return sets;

  do {
    Field* fields = result->Fetch();
    SavedSet set;
    set.Id = fields[0].GetUInt8();
    set.Name = fields[1].GetString();
    set.Data = fields[2].GetString();
    sets.push_back(set);
  } while (result->NextRow());

  return sets;
}

static bool LoadSet(uint32 ownerGuid, uint8 presetId, SavedSet& out) {
  QueryResult result =
      CharacterDatabase.PQuery("SELECT `PresetID`,`SetName`,`SetData` FROM `custom_transmogrification_sets` WHERE "
                               "`Owner`=%u AND `PresetID`=%u LIMIT 1",
                               ownerGuid, uint32(presetId));

  if (!result)
    return false;

  Field* fields = result->Fetch();
  out.Id = fields[0].GetUInt8();
  out.Name = fields[1].GetString();
  out.Data = fields[2].GetString();
  return true;
}

static uint8 FindFreeSetId(std::vector<SavedSet> const& sets) {
  for (uint8 id = 0; id < g_TransmogConfig.MaxSets; ++id) {
    bool used = false;
    for (std::vector<SavedSet>::const_iterator itr = sets.begin(); itr != sets.end(); ++itr)
      if (itr->Id == id) {
        used = true;
        break;
      }

    if (!used)
      return id;
  }

  return g_TransmogConfig.MaxSets;
}

static bool BuildCurrentSet(Player* player, std::string& data, uint64& saveCost) {
  std::ostringstream out;
  bool any = false;
  saveCost = 0;

  for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot) {
    if (!GetSlotName(slot))
      continue;

    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!item)
      continue;

    uint32 entry = GetTransmogEntry(item);
    if (!entry)
      continue;

    ItemTemplate const* appearance = sObjectMgr->GetItemTemplate(entry);
    if (!appearance || item->GetOwnerGUID() != player->GetGUID() || item->IsInTrade() ||
        !CanTransmogTemplates(player, item->GetTemplate(), appearance))
      return false;

    out << uint32(slot) << ' ' << entry << ' ';
    saveCost += Item::GetSpecialPrice(appearance);
    any = true;
  }

  data = out.str();
  saveCost = TransmogValidation::Cost(saveCost, g_TransmogConfig.SetCostModifier, g_TransmogConfig.SetCopperCost);
  return any;
}
static void Persist(Player* player) {
  auto trans = CharacterDatabase.BeginTransaction();
  player->SaveInventoryAndGoldToDB(trans);
  CharacterDatabase.CommitTransaction(trans);
}
} // namespace

class npc_transmogrifier_config: public WorldScript {
  public:
    npc_transmogrifier_config(): WorldScript("npc_transmogrifier_config") {}

    void OnConfigLoad(bool /*reload*/) override {
      LoadModuleConfig();
    }
};

class TransmogMenus {
  public:


    bool OnGossipHello(Player* player, Creature* creature) {
      TC_LOG_DEBUG("modules.npc_transmogrifier", "[mod-npc-transmogrifier] GossipHello: entry=%u scriptId=%u",
          creature->GetEntry(), creature->GetScriptId());
      ClearGossipMenuFor(player);
      InitGossipMenuFor(player, 900101);
      for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot) {
        char const* slotName = GetSlotName(slot);
        if (!slotName)
          continue;

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        std::string label = GetSlotIcon(slot);
        if (item && GetTransmogEntry(item))
          label += "|cff00ff00";
        label += slotName;
        if (item && GetTransmogEntry(item))
          label += "|r";

        // Slot selection uses a dedicated sender and carries the slot in action.
        // This avoids the HEAD slot becoming sender=0/action=0 and being swallowed.
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, label, SENDER_SLOT, slot);
      }

      if (g_TransmogConfig.EnableSets && g_TransmogConfig.MaxSets)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Misc_Statue_02:16:16|tНаборы",
                                SENDER_SETS, 0);

      AddGossipItemFor(player,
          GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Enchant_Disenchant:16:16|tСнять все облики",
          SENDER_REMOVE_ALL, 0, "Снять облики со всех надетых предметов?", 0, false);

      AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                              "|TInterface/PaperDollInfoFrame/UI-GearManager-Undo:16:16|tОбновить меню",
                              SENDER_MAIN, 0);

      AddGossipItemFor(player, GOSSIP_ICON_TALK, "Закрыть", 1000, 0);
      SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
      TC_LOG_DEBUG("modules.npc_transmogrifier", "[mod-npc-transmogrifier] GossipMenu sent: menu=900101 items=%zu",
          player->PlayerTalkClass->GetGossipMenu().GetMenuItems().size());
      return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) {
      player->PlayerTalkClass->ClearMenus();

      // Source appearance entries deliberately use sender == equipment slot and
      // action == source item low GUID.  Main-menu slot selection is SENDER_SLOT.
      if (sender < EQUIPMENT_SLOT_END) {
        uint8 slot = uint8(sender);

        Item* target = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        Item* source = FindInventoryItemByLowGuid(player, action);
        if (!GetSlotName(slot) || !target || !source || target->GetOwnerGUID() != player->GetGUID() ||
            source->GetOwnerGUID() != player->GetGUID() || target->IsInTrade() || source->IsInTrade() || !CanTransmogTemplates(player, target->GetTemplate(), source->GetTemplate())) {
          player->GetSession()->SendNotification("Облики этих предметов несовместимы.");
          return OnGossipHello(player, creature);
        }

        if (GetTransmogEntry(target) == source->GetEntry()) {
          player->GetSession()->SendNotification("Этот облик уже применён.");
          ShowTransmogItems(player, creature, slot);
          return true;
        }

        uint32 cost = GetTransmogCost(target);
        if (cost == UINT32_MAX || (cost && !player->HasEnoughMoney(uint64(cost)))) {
          player->GetSession()->SendNotification("Недостаточно золота.");
          ShowTransmogItems(player, creature, slot);
          return true;
        }

        if (g_TransmogConfig.RequireToken) {
          if (!g_TransmogConfig.TokenEntry || !g_TransmogConfig.TokenAmount ||
              source->GetEntry() == g_TransmogConfig.TokenEntry || target->GetEntry() == g_TransmogConfig.TokenEntry)
            return OnGossipHello(player, creature);
          if (!player->HasItemCount(g_TransmogConfig.TokenEntry, g_TransmogConfig.TokenAmount)) {
            player->GetSession()->SendNotification("Недостаточно жетонов трансмогрификации.");
            ShowTransmogItems(player, creature, slot);
            return true;
          }
          player->DestroyItemCount(g_TransmogConfig.TokenEntry, g_TransmogConfig.TokenAmount, true);
        }

        ApplyFromItem(player, slot, target, source);
        if (cost)
          player->ModifyMoney(-int64(cost));

        Persist(player);
        CloseGossipMenuFor(player);
        return true;
      }

      if (sender >= SENDER_SETS && (!g_TransmogConfig.EnableSets || !g_TransmogConfig.MaxSets))
        return OnGossipHello(player, creature);
      if ((sender == SENDER_SET_VIEW || sender == SENDER_SET_APPLY || sender == SENDER_SET_DELETE) &&
          action >= g_TransmogConfig.MaxSets)
        return OnGossipHello(player, creature);
      switch (sender) {
      case SENDER_SLOT: {
        if (action >= EQUIPMENT_SLOT_END || !GetSlotName(uint8(action)))
          return OnGossipHello(player, creature);

        ShowTransmogItems(player, creature, uint8(action));
        return true;
      }

      case SENDER_MAIN:
        return OnGossipHello(player, creature);

      case SENDER_REMOVE_ALL: {
        bool removed = false;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot) {
          Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
          if (!item || !GetTransmogEntry(item))
            continue;
          RemoveTransmog(player, slot, item);
          removed = true;
        }

        Persist(player);
        if (!removed)
          player->GetSession()->SendNotification("Нет предметов с изменённым обликом.");

        return OnGossipHello(player, creature);
      }

      case SENDER_REMOVE_ONE: {
        if (action >= EQUIPMENT_SLOT_END || !GetSlotName(uint8(action)))
          return OnGossipHello(player, creature);
        uint8 slot = uint8(action);
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && GetTransmogEntry(item))
          RemoveTransmog(player, slot, item);
        else
          player->GetSession()->SendNotification("Облик этого предмета не изменён.");

        Persist(player);
        ShowTransmogItems(player, creature, slot);
        return true;
      }

      case SENDER_SETS:
        if (!g_TransmogConfig.EnableSets || !g_TransmogConfig.MaxSets)
          return OnGossipHello(player, creature);
        ShowSets(player, creature);
        return true;

      case SENDER_SET_VIEW:
        ShowSet(player, creature, uint8(action));
        return true;

      case SENDER_SET_APPLY:
        ApplySet(player, creature, uint8(action));
        return true;

      case SENDER_SET_DELETE:
        CharacterDatabase.DirectPExecute("DELETE FROM `custom_transmogrification_sets` WHERE `Owner`=%u AND `PresetID`=%u",
                                   player->GetGUID().GetCounter(), action);
        ShowSets(player, creature);
        return true;

      case SENDER_SET_SAVE_MENU:
        ShowSaveSet(player, creature);
        return true;

      default:
        return OnGossipHello(player, creature);
      }
    }

    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action,
                            const char* code) {
      player->PlayerTalkClass->ClearMenus();

      if (sender != SENDER_SET_SAVE_CODE || action || !code || !g_TransmogConfig.EnableSets || !g_TransmogConfig.MaxSets)
        return OnGossipHello(player, creature);

      std::string name(code);
      if (!TransmogValidation::Name(name)) {
        player->GetSession()->SendNotification("Название: от 1 до 48 байт UTF-8, без управляющих символов и |.");
        return ShowSetsAndReturn(player, creature);
      }

      std::wstring wideName;
      if (!Utf8toWStr(name, wideName))
        return ShowSetsAndReturn(player, creature);
      std::vector<SavedSet> sets = LoadSets(player->GetGUID().GetCounter());
      if (sets.size() >= g_TransmogConfig.MaxSets) {
        player->GetSession()->SendNotification("Достигнут лимит сохранённых наборов.");
        return ShowSetsAndReturn(player, creature);
      }

      uint8 presetId = FindFreeSetId(sets);
      if (presetId >= g_TransmogConfig.MaxSets)
        return ShowSetsAndReturn(player, creature);

      std::string data;
      uint64 saveCost = 0;
      if (!BuildCurrentSet(player, data, saveCost)) {
        player->GetSession()->SendNotification("Нет подходящих обликов для сохранения.");
        return ShowSetsAndReturn(player, creature);
      }

      if (saveCost == UINT32_MAX || (saveCost && !player->HasEnoughMoney(saveCost))) {
        player->GetSession()->SendNotification("Недостаточно золота для сохранения набора.");
        return ShowSetsAndReturn(player, creature);
      }

      CharacterDatabase.EscapeString(name);
      auto trans = CharacterDatabase.BeginTransaction();
      trans->PAppend("INSERT INTO `custom_transmogrification_sets` "
                                 "(`Owner`,`PresetID`,`SetName`,`SetData`) VALUES (%u,%u,'%s','%s')",
                                 player->GetGUID().GetCounter(), uint32(presetId), name.c_str(), data.c_str());

      if (saveCost)
        player->ModifyMoney(-int64(saveCost));

      player->SaveInventoryAndGoldToDB(trans);
      CharacterDatabase.DirectCommitTransaction(trans);
      player->GetSession()->SendNotification("Сохранение отправлено. Проверьте набор в списке.");
      ShowSets(player, creature);
      return true;
    }

  private:
    bool ShowSetsAndReturn(Player* player, Creature* creature) {
      ShowSets(player, creature);
      return true;
    }

    void AddSourceItem(Player* player, Item* source, uint8 slot, uint32 price) {
      std::ostringstream confirm;
      confirm << "Применить этот облик? Оба предмета утратят право возврата и передачи в группе.\n\n"
              << GetItemLink(source);
      if (g_TransmogConfig.RequireToken)
        confirm << "\nЖетоны: " << g_TransmogConfig.TokenAmount << " (item " << g_TransmogConfig.TokenEntry << ")";

      AddGossipItemFor(player, GOSSIP_ICON_CHAT, GetItemLink(source), slot, source->GetGUID().GetCounter(),
                                       confirm.str(), price, false);
    }

    void ShowTransmogItems(Player* player, Creature* creature, uint8 slot) {
      Item* target = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
      uint32 shown = 0;

      if (target) {
        uint32 currentEntry = GetTransmogEntry(target);
        uint32 price = GetTransmogCost(target);

        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END && shown < MAX_SOURCE_OPTIONS; ++i) {
          Item* source = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
          if (!source || source == target || currentEntry == source->GetEntry())
            continue;
          if (!CanTransmogTemplates(player, target->GetTemplate(), source->GetTemplate()))
            continue;

          AddSourceItem(player, source, slot, price);
          ++shown;
        }

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END && shown < MAX_SOURCE_OPTIONS;
             ++bagSlot) {
          Bag* bag = player->GetBagByPos(bagSlot);
          if (!bag)
            continue;

          for (uint32 i = 0; i < bag->GetBagSize() && shown < MAX_SOURCE_OPTIONS; ++i) {
            Item* source = player->GetItemByPos(bagSlot, i);
            if (!source || currentEntry == source->GetEntry())
              continue;
            if (!CanTransmogTemplates(player, target->GetTemplate(), source->GetTemplate()))
              continue;

            AddSourceItem(player, source, slot, price);
            ++shown;
          }
        }
      }

      if (!shown)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "В сумках нет подходящих предметов.", SENDER_MAIN, 0);

      AddGossipItemFor(player,
          GOSSIP_ICON_CHAT,
          "|TInterface/ICONS/INV_Enchant_Disenchant:16:16|tСнять облик с этого предмета",
          SENDER_REMOVE_ONE, slot, "Снять облик с этого предмета?", 0, false);
      AddGossipItemFor(player, GOSSIP_ICON_TALK, "Назад", SENDER_MAIN, 0);
      AddGossipItemFor(player, GOSSIP_ICON_TALK, "Закрыть", 1000, 0);
      SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void ShowSets(Player* player, Creature* creature) {
      std::vector<SavedSet> sets = LoadSets(player->GetGUID().GetCounter());
      for (std::vector<SavedSet>::const_iterator itr = sets.begin(); itr != sets.end(); ++itr)
        if (itr->Id < g_TransmogConfig.MaxSets && TransmogValidation::Name(itr->Name))
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Misc_Statue_02:16:16|t" + itr->Name,
                                SENDER_SET_VIEW, itr->Id);

      if (sets.size() < g_TransmogConfig.MaxSets)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                                "|TInterface/GuildBankFrame/UI-GuildBankFrame-NewTab:16:16|tСохранить текущий набор",
                                SENDER_SET_SAVE_MENU, 0);

      AddGossipItemFor(player, GOSSIP_ICON_TALK, "Назад", SENDER_MAIN, 0);
      AddGossipItemFor(player, GOSSIP_ICON_TALK, "Закрыть", 1000, 0);
      SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void ShowSaveSet(Player* player, Creature* creature) {
      std::string data;
      uint64 saveCost = 0;
      if (!BuildCurrentSet(player, data, saveCost)) {
        player->GetSession()->SendNotification("Нет подходящих обликов для сохранения.");
        ShowSets(player, creature);
        return;
      }

      std::ostringstream text;
      text << "Сохранить текущий набор обликов";
      if (saveCost)
        text << " (стоимость: " << saveCost << " медных)";

      AddGossipItemFor(player, GOSSIP_ICON_CHAT, text.str(), SENDER_SET_SAVE_CODE, 0, "Введите название набора",
                                       uint32(saveCost), true);
      AddGossipItemFor(player, GOSSIP_ICON_TALK, "Назад", SENDER_SETS, 0);
      AddGossipItemFor(player, GOSSIP_ICON_TALK, "Закрыть", 1000, 0);
      SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void ShowSet(Player* player, Creature* creature, uint8 presetId) {
      SavedSet set;
      if (!LoadSet(player->GetGUID().GetCounter(), presetId, set)) {
        player->GetSession()->SendNotification("Набор не найден.");
        ShowSets(player, creature);
        return;
      }

      std::vector<TransmogValidation::Appearance> appearances;
      if (!TransmogValidation::Parse(set.Data, appearances))
        return;
      for (auto const& appearance : appearances) {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(appearance.entry);
        if (!GetSlotName(appearance.slot) || !proto) return;
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, GetItemLink(proto), SENDER_SET_VIEW, presetId);
      }

      AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                                       "|TInterface/ICONS/INV_Misc_Statue_02:16:16|tПрименить набор",
                                       SENDER_SET_APPLY, presetId, "Применить набор бесплатно? Все его предметы должны быть совместимы.", 0, false);
      AddGossipItemFor(player,
          GOSSIP_ICON_CHAT,
          "|TInterface/PaperDollInfoFrame/UI-GearManager-LeaveItem-Opaque:16:16|tУдалить набор", SENDER_SET_DELETE,
          presetId, "Удалить сохранённый набор?", 0, false);
      AddGossipItemFor(player, GOSSIP_ICON_TALK, "Назад", SENDER_SETS, 0);
      AddGossipItemFor(player, GOSSIP_ICON_TALK, "Закрыть", 1000, 0);
      SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void ApplySet(Player* player, Creature* creature, uint8 presetId) {
      SavedSet set;
      if (!LoadSet(player->GetGUID().GetCounter(), presetId, set)) {
        player->GetSession()->SendNotification("Набор не найден.");
        ShowSets(player, creature);
        return;
      }

      std::vector<TransmogValidation::Appearance> appearances;
      if (!TransmogValidation::Parse(set.Data, appearances))
        return;
      for (auto const& appearance : appearances) {
        Item* target = player->GetItemByPos(INVENTORY_SLOT_BAG_0, appearance.slot);
        auto source = sObjectMgr->GetItemTemplate(appearance.entry);
        if (!GetSlotName(appearance.slot) || !target || target->GetOwnerGUID() != player->GetGUID() ||
            target->IsInTrade() || !CanTransmogTemplates(player, target->GetTemplate(), source))
          return;
      }
      // Presets are paid at creation, as in the donor. They authorize these saved appearances.
      for (auto const& appearance : appearances)
        ApplyEntry(player, appearance.slot, player->GetItemByPos(INVENTORY_SLOT_BAG_0, appearance.slot), appearance.entry);
      Persist(player);
      player->GetSession()->SendNotification("Набор обликов применён.");
      ShowSet(player, creature, presetId);
    }
};

class npc_transmogrifier final : public CreatureScript {
public:
  npc_transmogrifier() : CreatureScript("npc_transmogrifier") { }
  struct AI final : PassiveAI {
    explicit AI(Creature* creature) : PassiveAI(creature) { }
    TransmogMenus menus;
    bool GossipHello(Player* player) override { return menus.OnGossipHello(player, me); }
    bool Select(Player* player, uint32 menuId, uint32 listId, char const* code) {
      auto const& menu = player->PlayerTalkClass->GetGossipMenu();
      auto item = menu.GetItem(listId);
      if (!item || menuId != menu.GetMenuId() ||
          player->PlayerTalkClass->GetInteractionData().SourceGuid != me->GetGUID() ||
          player->GetNPCIfCanInteractWith(me->GetGUID(), UNIT_NPC_FLAG_GOSSIP) != me ||
          player->IsInCombat() || !player->IsAlive() ||
          player->PlayerTalkClass->IsGossipOptionCoded(listId) != bool(code)) {
        ClearGossipMenuFor(player);
        CloseGossipMenuFor(player);
        return true;
      }
      uint32 sender = item->Sender, action = item->OptionType;
      if (sender < EQUIPMENT_SLOT_END) {
        Item* target = player->GetItemByPos(INVENTORY_SLOT_BAG_0, uint8(sender));
        if (!target || GetTransmogCost(target) != item->BoxMoney) {
          ClearGossipMenuFor(player);
          CloseGossipMenuFor(player);
          return true;
        }
      }
      if (sender == SENDER_SET_SAVE_CODE) {
        std::string data;
        uint64 price = 0;
        if (!BuildCurrentSet(player, data, price) || price != item->BoxMoney) {
          ClearGossipMenuFor(player);
          CloseGossipMenuFor(player);
          return true;
        }
      }
      // Invalidate before dispatch: stale list IDs cannot repeat a paid operation.
      ClearGossipMenuFor(player);
      if (sender == 1000) { CloseGossipMenuFor(player); return true; }
      if (code) return menus.OnGossipSelectCode(player, me, sender, action, code);
      return menus.OnGossipSelect(player, me, sender, action);
    }
    bool GossipSelect(Player* player, uint32 menu, uint32 list) override { return Select(player, menu, list, nullptr); }
    bool GossipSelectCode(Player* player, uint32 menu, uint32 list, char const* code) override {
      return Select(player, menu, list, code);
    }
  };
  CreatureAI* GetAI(Creature* creature) const override {
    TC_LOG_DEBUG("modules.npc_transmogrifier", "[mod-npc-transmogrifier] GetAI: entry=%u scriptId=%u",
        creature->GetEntry(), creature->GetScriptId());
    return new AI(creature);
  }
};

void Addmod_npc_transmogrifierScripts() {
  uint32 const scriptId = sObjectMgr->GetScriptId("npc_transmogrifier");
  TC_LOG_INFO("server.loading", "[mod-npc-transmogrifier] Loader called: npc_transmogrifier database scriptId=%u", scriptId);
  new npc_transmogrifier_config();
  new npc_transmogrifier();
  if (!scriptId)
    TC_LOG_ERROR("server.loading", "[mod-npc-transmogrifier] npc_transmogrifier is not bound in the loaded world database; verify module SQL and fully restart worldserver");
}
