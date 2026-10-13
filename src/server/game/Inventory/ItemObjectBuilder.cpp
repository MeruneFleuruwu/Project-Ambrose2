/*
 * Project Ambrose by Imjustchico
 * Makes an item's game object from the catalog's own class and defaults, refusing a template class the catalog or core_template_type does not know rather than guessing a core type, sets only its header, global id, template id, its colors and, where the class has one, the pattern word whose top bit the client reads as the lock; a pet item built with its template and the behavior classes named gets its template's behaviors in the order the client pairs them and a hatched time of zero with a level of one on the behavior that holds the hatched time, because the client reads a pet behavior at level zero as an unhatched egg whatever its hatched time says, naming it from the egg names and counting down its hatched time against the clock (a past or zero hatched time wraps the count) and refusing to put the item on, while a level-one behavior reads as a hatched pet; and encodes it through the message field ObjectFields declares for it, so the envelope and the CoreObject form are the field's, not this file's; the backpack's objects go to the player's behavior that has both an m_itemList and an m_numItemsAllowed, since the equipment behavior carries an m_itemList too, and a list the class refuses fails the fill whole, leaving the player object as it was; the capacity goes to whichever of them has an m_numItemsAllowed, ClientWizInventoryBehavior in the type dump. The worn items go to the behavior with an m_itemList and no m_numItemsAllowed, ClientWizEquipmentBehavior, as the same item objects the backpack holds, since the client loads both lists and MSG_EQUIPMENTBEHAVIOR_EQUIPITEM's SerializedItem with the reader MSG_INVENTORYBEHAVIOR_ADDITEM's uses; m_slotList and m_publicItemList are filled only where the class has them, a slot entry's m_itemSlotNameID is the KI string id of the slot's name, as the client's own equip handler hashes the slot name for the entry it makes, and a public entry is a WizardEquippedItemInfo, since the client's ClientWizEquipmentBehavior casts what it is sent to that class and drops anything else, carrying the item's template id and the same colors and pattern its own object carries, without the lock bit, so other wizards tint it as its wearer does; the public entry is encoded on its own through the SerializedInfo field ObjectFields declares.
 */

#include "ItemObjectBuilder.h"
#include "ObjectFields.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"
#include "PetObjectBuilder.h"
#include "PlayerBackpack.h"
#include "ObjectSerializer.h"
#include "PropertyFiller.h"
#include "StringHash.h"

#include <fmt/format.h>

#include <algorithm>
#include <limits>
#include <span>

PropertyObjectPtr ItemObjectBuilder::Build(TypeCatalogPtr const& catalog, CoreObjectTypeTable const& types, ItemTemplateRecord const& itemTemplate, CharacterItem const& item,
    std::string& problem, ObjectTemplate const* objectTemplate, BehaviorClientClasses const* behaviors)
{
    problem.clear();
    if (!catalog)
    {
        problem = "no type dump is loaded";
        return nullptr;
    }
    ClassInfo const* const templateClass = catalog->FindClass(itemTemplate.ClassName);
    if (!templateClass)
    {
        problem = fmt::format("the type dump has no template class {}", itemTemplate.ClassName);
        return nullptr;
    }
    std::optional<CoreObjectHeader> const header = types.HeaderFor(*templateClass, itemTemplate.TemplateId);
    if (!header)
    {
        problem = fmt::format("core_template_type gives the item template class {} no core type, so the client could not create item {} from it", itemTemplate.ClassName,
            itemTemplate.TemplateId);
        return nullptr;
    }
    std::span<CoreObjectType const> const built = types.GetTypes();
    auto const type = std::find_if(built.begin(), built.end(), [&header](CoreObjectType const& entry) { return entry.CoreType == header->Block; });
    if (type == built.end())
    {
        problem = fmt::format("core_object_type does not say which class core type {} builds", header->Block);
        return nullptr;
    }
    PropertyObjectPtr object = PropertyObject::Create(catalog, type->ClassName);
    if (!object)
    {
        problem = fmt::format("the type dump has no property class {}", type->ClassName);
        return nullptr;
    }
    object->SetCoreHeader(*header);
    PropertyFiller(*object, problem)
        .Set("m_globalID.m_full", item.Guid)
        .Set("m_permID", uint64{ 0 })
        .Set("m_templateID.m_full", uint64{ itemTemplate.TemplateId });
    if (!problem.empty())
        return nullptr;
    if (object->GetClass().FindProperty(PatternProperty))
        PropertyFiller(*object, problem).Set(PatternProperty, static_cast<int32>(PlayerBackpack::LockWord(item)));
    if (object->GetClass().FindProperty("m_primaryColor") && object->GetClass().FindProperty("m_secondaryColor"))
        PropertyFiller(*object, problem).Set("m_primaryColor", int32{ item.PrimaryColor }).Set("m_secondaryColor", int32{ item.SecondaryColor });
    if (!problem.empty())
        return nullptr;
    if (objectTemplate && behaviors && PetObjectBuilder::IsPetObject(*objectTemplate) && object->GetClass().FindProperty("m_inactiveBehaviors")
        && !FillPetBehaviors(*object, catalog, *behaviors, *objectTemplate, item, problem))
        return nullptr;
    if (!problem.empty())
        return nullptr;
    return object;
}

bool ItemObjectBuilder::FillPetBehaviors(PropertyObject& object, TypeCatalogPtr const& catalog, BehaviorClientClasses const& behaviors, ObjectTemplate const& objectTemplate,
    CharacterItem const& item, std::string& problem)
{
    PropertyValue::List inactive;
    inactive.reserve(objectTemplate.Behaviors.size());
    for (std::string const& behaviorName : objectTemplate.Behaviors)
    {
        if (behaviorName.empty())
        {
            inactive.emplace_back(PropertyObjectPtr());
            continue;
        }
        BehaviorClientClass const* const row = behaviors.Find(behaviorName);
        if (!row)
        {
            problem = fmt::format("item {} of template {} names behavior {}, which behavior_client_class does not list, so the class the client builds for it is not known",
                item.Guid, objectTemplate.TemplateId, behaviorName);
            return false;
        }
        if (!row->ClassName)
        {
            inactive.emplace_back(PropertyObjectPtr());
            continue;
        }
        PropertyObjectPtr behavior = PropertyObject::Create(catalog, *row->ClassName);
        if (!behavior)
        {
            problem = fmt::format("the type dump has no property class {}", *row->ClassName);
            return false;
        }
        if (behavior->GetClass().FindProperty("m_hatchedTimeSecs"))
            PropertyFiller(*behavior, problem).Set("m_hatchedTimeSecs", uint32{ 0 }).Set("m_level", uint8{ 1 });
        if (!problem.empty())
            return false;
        inactive.emplace_back(std::move(behavior));
    }
    PropertyFiller(object, problem).Set("m_inactiveBehaviors", std::move(inactive));
    return problem.empty();
}

bool ItemObjectBuilder::FillBackpack(PropertyObject& player, CoreObjectTypeTable const& types, ItemTemplateStore const& templates, std::vector<CharacterItem> const& items,
    std::vector<uint64>& missing, std::string& problem, ObjectTemplateMgr* objectTemplates, BehaviorClientClasses const* behaviors)
{
    problem.clear();
    PropertyValue const* const held = player.Get("m_inactiveBehaviors");
    PropertyValue::List const* const current = held ? held->GetList() : nullptr;
    if (!current)
    {
        problem = fmt::format("{} has no behavior list", player.GetClass().Name);
        return false;
    }
    PropertyValue::List behaviors_list = *current;
    PropertyObject* const inventory = FindBackpack(behaviors_list);
    if (!inventory)
    {
        problem = fmt::format("no behavior of the player object carries {} and {}", ItemListProperty, ItemsAllowedProperty);
        return false;
    }
    PropertyValue::List list;
    list.reserve(items.size());
    for (CharacterItem const& item : items)
    {
        ItemTemplateRecord const* const itemTemplate = templates.Find(item.TemplateId);
        if (!itemTemplate)
        {
            missing.push_back(item.Guid);
            continue;
        }
        std::shared_ptr<ObjectTemplate const> const petTemplate = objectTemplates ? objectTemplates->Lookup(item.TemplateId).Template : nullptr;
        PropertyObjectPtr object = Build(player.GetCatalog(), types, *itemTemplate, item, problem, petTemplate.get(), behaviors);
        if (!object)
            return false;
        list.emplace_back(std::move(object));
    }
    PropertyFiller(*inventory, problem).Set(ItemListProperty, std::move(list));
    PropertyFiller(player, problem).Set("m_inactiveBehaviors", std::move(behaviors_list));
    return problem.empty();
}

PropertyObject* ItemObjectBuilder::FindBackpack(PropertyValue::List& behaviors)
{
    for (PropertyValue& entry : behaviors)
        if (PropertyObject* const behavior = entry.AsObject();
            behavior && behavior->GetClass().FindProperty(ItemListProperty) && behavior->GetClass().FindProperty(ItemsAllowedProperty))
            return behavior;
    return nullptr;
}

bool ItemObjectBuilder::SetItemsAllowed(PropertyObject& player, uint32 capacity, std::string& problem)
{
    problem.clear();
    PropertyValue const* const held = player.Get("m_inactiveBehaviors");
    PropertyValue::List const* const current = held ? held->GetList() : nullptr;
    if (!current)
    {
        problem = fmt::format("{} has no behavior list", player.GetClass().Name);
        return false;
    }
    PropertyValue::List behaviors = *current;
    PropertyObject* const inventory = FindBackpack(behaviors);
    if (!inventory)
    {
        problem = fmt::format("no behavior of the player object carries {}", ItemsAllowedProperty);
        return false;
    }
    PropertyFiller(*inventory, problem).Set(ItemsAllowedProperty, static_cast<int32>(std::min<uint32>(capacity, std::numeric_limits<int32>::max())));
    PropertyFiller(player, problem).Set("m_inactiveBehaviors", std::move(behaviors));
    return problem.empty();
}

std::optional<std::string> ItemObjectBuilder::Encode(std::string_view message, PropertyObject const& object, CoreObjectTypeTable const& types, std::string& problem)
{
    ObjectField const* const field = ObjectFields::Find(message, SerializedItemField);
    if (!field)
    {
        problem = fmt::format("no field describes the {} of {}", SerializedItemField, message);
        return std::nullopt;
    }
    EncodeResult const encoded = CoreObjectSerializer::EncodeField(*field, object, types);
    if (!encoded.Ok())
    {
        problem = fmt::format("the item does not encode: {}", encoded.Detail);
        return std::nullopt;
    }
    return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
}

bool ItemObjectBuilder::FillEquipment(PropertyObject& player, CoreObjectTypeTable const& types, ItemTemplateStore const& templates, std::vector<CharacterEquippedItem> const& items,
    std::vector<uint64>& missing, std::string& problem, ObjectTemplateMgr* objectTemplates, BehaviorClientClasses const* behaviors)
{
    problem.clear();
    PropertyValue const* const held = player.Get("m_inactiveBehaviors");
    PropertyValue::List const* const current = held ? held->GetList() : nullptr;
    if (!current)
    {
        problem = fmt::format("{} has no behavior list", player.GetClass().Name);
        return false;
    }
    PropertyValue::List behaviorList = *current;
    PropertyObject* const equipment = FindEquipment(behaviorList);
    if (!equipment)
    {
        problem = fmt::format("no behavior of the player object carries {} without {}", ItemListProperty, ItemsAllowedProperty);
        return false;
    }
    bool const hasSlots = equipment->GetClass().FindProperty(SlotListProperty) != nullptr;
    bool const hasPublic = equipment->GetClass().FindProperty(PublicItemListProperty) != nullptr;
    PropertyValue::List list;
    PropertyValue::List slots;
    PropertyValue::List shown;
    list.reserve(items.size());
    for (CharacterEquippedItem const& worn : items)
    {
        ItemTemplateRecord const* const itemTemplate = templates.Find(worn.Item.TemplateId);
        if (!itemTemplate)
        {
            missing.push_back(worn.Item.Guid);
            continue;
        }
        std::shared_ptr<ObjectTemplate const> const petTemplate = objectTemplates ? objectTemplates->Lookup(worn.Item.TemplateId).Template : nullptr;
        PropertyObjectPtr object = Build(player.GetCatalog(), types, *itemTemplate, worn.Item, problem, petTemplate.get(), behaviors);
        if (!object)
            return false;
        list.emplace_back(std::move(object));
        if (hasSlots)
        {
            PropertyObjectPtr slot = PropertyObject::Create(player.GetCatalog(), SlotInfoClass);
            if (!slot)
            {
                problem = fmt::format("the type dump has no {}", SlotInfoClass);
                return false;
            }
            PropertyFiller(*slot, problem).Set("m_itemID", worn.Item.Guid).Set("m_itemSlotNameID", SlotNameId(worn.Slot));
            if (!problem.empty())
                return false;
            slots.emplace_back(std::move(slot));
        }
        if (hasPublic)
        {
            PropertyObjectPtr info = BuildPublicInfo(player.GetCatalog(), worn.Item, problem);
            if (!info)
                return false;
            shown.emplace_back(std::move(info));
        }
    }
    PropertyFiller filler(*equipment, problem);
    filler.Set(ItemListProperty, std::move(list));
    if (hasSlots)
        filler.Set(SlotListProperty, std::move(slots));
    if (hasPublic)
        filler.Set(PublicItemListProperty, std::move(shown));
    if (!problem.empty())
        return false;
    PropertyFiller(player, problem).Set("m_inactiveBehaviors", std::move(behaviorList));
    return problem.empty();
}

PropertyObject* ItemObjectBuilder::FindEquipment(PropertyValue::List& behaviors)
{
    for (PropertyValue& entry : behaviors)
        if (PropertyObject* const behavior = entry.AsObject();
            behavior && behavior->GetClass().FindProperty(ItemListProperty) && !behavior->GetClass().FindProperty(ItemsAllowedProperty))
            return behavior;
    return nullptr;
}

PropertyObjectPtr ItemObjectBuilder::BuildPublicInfo(TypeCatalogPtr const& catalog, CharacterItem const& item, std::string& problem)
{
    PropertyObjectPtr info = catalog ? PropertyObject::Create(catalog, ItemInfoClass) : nullptr;
    if (!info)
    {
        problem = fmt::format("the type dump has no {}", ItemInfoClass);
        return nullptr;
    }
    PropertyFiller(*info, problem)
        .Set("m_itemID", item.TemplateId)
        .Set("m_baseColor", uint32{ item.PrimaryColor })
        .Set("m_trimColor", uint32{ item.SecondaryColor })
        .Set("m_pattern", uint32{ item.Pattern });
    if (!problem.empty())
        return nullptr;
    return info;
}

std::optional<std::string> ItemObjectBuilder::EncodePublicInfo(std::string_view message, PropertyObject const& info, CoreObjectTypeTable const& types, std::string& problem)
{
    ObjectField const* const field = ObjectFields::Find(message, SerializedInfoField);
    if (!field)
    {
        problem = fmt::format("no field describes the {} of {}", SerializedInfoField, message);
        return std::nullopt;
    }
    EncodeResult const encoded = CoreObjectSerializer::EncodeField(*field, info, types);
    if (!encoded.Ok())
    {
        problem = fmt::format("the equipped item's public entry does not encode: {}", encoded.Detail);
        return std::nullopt;
    }
    return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
}

uint32 ItemObjectBuilder::SlotNameId(std::string_view slot) noexcept
{
    return StringHash::StringId(slot);
}
