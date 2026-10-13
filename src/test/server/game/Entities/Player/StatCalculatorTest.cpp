/*
 * Project Ambrose by Imjustchico
 * Tests the StatCalculator: computes final stats from base level stats, equipped item effects, and set bonuses; converts ratings to percentages using StatEffectSet; handles per-school damage/resist/accuracy bonuses.
 */

#include "ItemTemplateRecord.h"
#include "PlayerLevels.h"
#include "StatCalculator.h"
#include "StatEffects.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
    using CharacterSummary = Ambrose::CharacterSummary;
    using CharacterEquippedItem = Ambrose::CharacterEquippedItem;
    using CharacterItem = Ambrose::CharacterItem;
    using ItemTemplateRecord = Ambrose::ItemTemplateRecord;
    using ItemSetBonusTier = Ambrose::ItemSetBonusTier;
    using PlayerLevelInfo = Ambrose::PlayerLevelInfo;
    using StatEffectSet = Ambrose::StatEffectSet;
    using ComputedStats = Ambrose::ComputedStats;

    TypeCatalogPtr LoadCatalog()
    {
        // Minimal type dump for testing - only what we need for StatCalculator
        std::string const dump = R"(
            {
                "classes": [
                    {
                        "name": "class WizItemTemplate",
                        "type": "PropertyClass"
                    },
                    {
                        "name": "class ItemSetBonusTemplate",
                        "type": "PropertyClass"
                    }
                ]
            }
        )";

        Json json = Json::parse(dump);
        Json& classes = json["classes"];
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(dump.dump(), "test.json"))
            << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    class StatCalculatorTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _catalog = LoadCatalog();
            ASSERT_TRUE(_catalog);

            // Set up level data for Fire school level 1
            _levels = std::make_shared<PlayerLevelSet>();
            _levels->_data.Levels = {
                { 0, 1, 0, 100, 50, 25, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, "Fire Level 1" } // SchoolId, Level, XpToLevel, Hitpoints, Mana, Gold, PipChance, TrainingPoints, CraftingSlots, PetEnergy, PipConversionAll, PipConversion[7], ShadowPipRating, Archmastery, LevelName
            };
            _levels->_data.Schools = {
                { 0, "Fire", 0, 0 } // Id, Name, MinLevel, Index
            };
            _levels->BuildIndex();
            _levels->_generation = 1;

            // Set up stat effect data for ratings conversion
            StatEffectData statData;
            statData.Settings = { { "m_shadowPipMax", 2.0 } };
            statData.CritAndBlock = {
                { 0, 0, 100, 0.01f, 0.0f, 0.015f, 0.0f }, // MinLevel, Position, CapValue, CriticalHitScalarBase, CriticalHitScalingFactor, BlockScalarBase, BlockScalingFactor
                { 50, 0, 200, 0.02f, 0.0f, 0.03f, 0.0f }
            };
            statData.PipConversion = {
                { 0, 0, 100, 0.005f, 0.0f }, // MinLevel, Position, CapValue, ScalarBase, ScalingFactor
                { 50, 0, 200, 0.01f, 0.0f }
            };
            _effects = StatEffectSet::Build(std::move(statData), _statErrors);
            ASSERT_TRUE(_effects) << _statErrors.front();
        }

        std::shared_ptr<PlayerLevelSet const> _levels;
        std::shared_ptr<StatEffectSet const> _effects;
        TypeCatalogPtr _catalog;
        std::vector<std::string> _statErrors;
        CharacterSummary _character{};
    };

    ItemTemplateRecord CreateItemWithEffect(std::string const& effectName, double value, std::string const& school = "")
    {
        ItemTemplateRecord item;
        item.TemplateId = 1;
        item.ClassName = "class WizItemTemplate";
        item.File = "test.xml";
        item.ObjectName = "TestItem";

        ItemTemplatePart part;
        part.ClassName = "class WizStatisticEffect";
        part.ClassHash = 0x12345678;
        part.Object = std::make_shared<PropertyObject const>(PropertyObject::Create(item.GetClass().Name)); // Simplified
        part.EffectName = effectName;
        part.NumericValue = value;
        if (!school.empty())
            part.MagicSchool = school;
        item.EquipEffects.push_back(part);

        return item;
    }

    ItemSetBonusTier CreateSetBonusTier(int32 numItemsToEquip, std::string const& effectName, double value, std::string const& school = "")
    {
        ItemSetBonusTier tier;
        tier.NumItemsToEquip = numItemsToEquip;
        tier.Description = "Test Tier";

        ItemTemplatePart part;
        part.ClassName = "class WizStatisticEffect";
        part.ClassHash = 0x12345678;
        part.Object = std::make_shared<PropertyObject const>(PropertyObject::Create(item.GetClass().Name)); // Simplified
        part.EffectName = effectName;
        part.NumericValue = value;
        if (!school.empty())
            part.MagicSchool = school;
        tier.Effects.push_back(part);

        return tier;
    }
}

TEST_F(StatCalculatorTest, BaseStatsAreComputedCorrectly)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    std::vector<CharacterEquippedItem> equipped; // No equipment
    std::vector<ItemTemplateRecord const*> itemTemplates;
    uint32 itemSetBonusTemplateId = 0;
    std::vector<ItemSetBonusTier> setBonusTiers;
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.BaseHitpoints, 100);
    EXPECT_EQ(stats.HitPointBonus, 0);
    EXPECT_EQ(stats.GetMaxHitpoints(), 100);
    EXPECT_EQ(stats.BaseMana, 50);
    EXPECT_EQ(stats.ManaBonus, 0);
    EXPECT_EQ(stats.GetMaxMana(), 50);
    EXPECT_EQ(stats.BaseGoldPouch, 25);
    EXPECT_EQ(stats.GetMaxGold(), 25);
}

TEST_F(StatCalculatorTest, HitPointBonusFromEquipmentIsAdded)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    auto item = CreateItemWithEffect("m_hitPointBonus", 50.0); // +50 HP
    std::vector<CharacterEquippedItem> equipped = { { item.Guid, 0, item.TemplateId, "", 0, 0 } };
    std::vector<ItemTemplateRecord const*> itemTemplates = { &item };
    uint32 itemSetBonusTemplateId = 0;
    std::vector<ItemSetBonusTier> setBonusTiers;
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.HitPointBonus, 50);
    EXPECT_EQ(stats.GetMaxHitpoints(), 150); // Base 100 + Bonus 50
}

TEST_F(StatCalculatorTest, ManaBonusFromEquipmentIsAdded)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    auto item = CreateItemWithEffect("m_manaBonus", 30.0); // +30 Mana
    std::vector<CharacterEquippedItem> equipped = { { item.Guid, 0, item.TemplateId, "", 0, 0 } };
    std::vector<ItemTemplateRecord const*> itemTemplates = { &item };
    uint32 itemSetBonusTemplateId = 0;
    std::vector<ItemSetBonusTier> setBonusTiers;
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.ManaBonus, 30);
    EXPECT_EQ(stats.GetMaxMana(), 80); // Base 50 + Bonus 30
}

TEST_F(StatCalculatorTest, DamageBonusPercentIsAdded)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    auto item = CreateItemWithEffect("m_damageBonusPercent", 15.0); // +15% damage
    std::vector<CharacterEquippedItem> equipped = { { item.Guid, 0, item.TemplateId, "", 0, 0 } };
    std::vector<ItemTemplateRecord const*> itemTemplates = { &item };
    uint32 itemSetBonusTemplateId = 0;
    std::vector<ItemSetBonusTier> setBonusTiers;
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.DamageBonusPercentAll, 15.0f);
}

TEST_F(StatCalculatorTest, PerSchoolDamageBonusIsAppliedCorrectly)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    auto item = CreateItemWithEffect("m_damageBonusPercent", 20.0, "Fire"); // +20% Fire damage
    std::vector<CharacterEquippedItem> equipped = { { item.Guid, 0, item.TemplateId, "", 0, 0 } };
    std::vector<ItemTemplateRecord const*> itemTemplates = { &item };
    uint32 itemSetBonusTemplateId = 0;
    std::vector<ItemSetBonusTier> setBonusTiers;
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.DamageBonusPercentSchool[0], 20.0f); // Fire is index 0
    // Other schools should be 0
    for (size_t i = 1; i < 7; ++i)
        EXPECT_EQ(stats.DamageBonusPercentSchool[i], 0.0f);
}

TEST_F(StatCalculatorTest, CriticalHitRatingIsConvertedToChance)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    auto item = CreateItemWithEffect("m_criticalHitRating", 100.0); // 100 crit rating
    std::vector<CharacterEquippedItem> equipped = { { item.Guid, 0, item.TemplateId, "", 0, 0 } };
    std::vector<ItemTemplateRecord const*> itemTemplates = { &item };
    uint32 itemSetBonusTemplateId = 0;
    std::vector<ItemSetBonusTier> setBonusTiers;
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.CriticalHitRating, 100);
    // For level 1 (< 50), using first band: CritChance = 0.01f + 0.0f * 100 = 0.01f (1%)
    EXPECT_FLOAT_EQ(stats.CriticalHitChance, 0.01f);
}

TEST_F(StatCalculatorTest, CriticalHitRatingIsCappedAtBandLimit)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    auto item = CreateItemWithEffect("m_criticalHitRating", 150.0); // 150 crit rating (over cap of 100)
    std::vector<CharacterEquippedItem> equipped = { { item.Guid, 0, item.TemplateId, "", 0, 0 } };
    std::vector<ItemTemplateRecord const*> itemTemplates = { &item };
    uint32 itemSetBonusTemplateId = 0;
    std::vector<ItemSetBonusTier> setBonusTiers;
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.CriticalHitRating, 150);
    // For level 1 (< 50), using first band with cap: CritChance = 0.01f + 0.0f * 100 = 0.01f (1%)
    EXPECT_FLOAT_EQ(stats.CriticalHitChance, 0.01f);
}

TEST_F(StatCalculatorTest, SetBonusEffectsAreAppliedWhenThresholdMet)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    auto item = CreateItemWithEffect("m_hitPointBonus", 25.0); // +25 HP each
    std::vector<CharacterEquippedItem> equipped = {
        { item.Guid, 0, item.TemplateId, "", 0, 0 },
        { item.Guid + 1, 0, item.TemplateId, "", 0, 0 } // Two items
    };
    std::vector<ItemTemplateRecord const*> itemTemplates = { &item, &item };
    uint32 itemSetBonusTemplateId = 0;
    // Set bonus: +50 HP when 2 items equipped
    auto setBonusTier = CreateSetBonusTier(2, "m_hitPointBonus", 50.0);
    std::vector<ItemSetBonusTier> setBonusTiers = { setBonusTier };
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.HitPointBonus, 100); // 2*25 from items + 50 from set bonus
    EXPECT_EQ(stats.GetMaxHitpoints(), 200); // Base 100 + Bonus 100
}

TEST_F(StatCalculatorTest, SetBonusEffectsAreNotAppliedWhenThresholdNotMet)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    auto item = CreateItemWithEffect("m_hitPointBonus", 25.0); // +25 HP each
    std::vector<CharacterEquippedItem> equipped = { { item.Guid, 0, item.TemplateId, "", 0, 0 } }; // Only one item
    std::vector<ItemTemplateRecord const*> itemTemplates = { &item };
    uint32 itemSetBonusTemplateId = 0;
    // Set bonus: +50 HP when 2 items equipped (threshold not met)
    auto setBonusTier = CreateSetBonusTier(2, "m_hitPointBonus", 50.0);
    std::vector<ItemSetBonusTier> setBonusTiers = { setBonusTier };
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.HitPointBonus, 25); // Only from items, no set bonus
    EXPECT_EQ(stats.GetMaxHitpoints(), 125); // Base 100 + Bonus 25
}

TEST_F(StatCalculatorTest, MultipleSetBonusTiersAreApplied)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    auto item = CreateItemWithEffect("m_hitPointBonus", 10.0); // +10 HP each
    std::vector<CharacterEquippedItem> equipped = {
        { item.Guid, 0, item.TemplateId, "", 0, 0 },
        { item.Guid + 1, 0, item.TemplateId, "", 0, 0 },
        { item.Guid + 2, 0, item.TemplateId, "", 0, 0 } // Three items
    };
    std::vector<ItemTemplateRecord const*> itemTemplates = { &item, &item, &item };
    uint32 itemSetBonusTemplateId = 0;
    // Set bonus tier 1: +20 HP when 2 items equipped
    auto setBonusTier1 = CreateSetBonusTier(2, "m_hitPointBonus", 20.0);
    // Set bonus tier 2: +30 HP when 3 items equipped
    auto setBonusTier2 = CreateSetBonusTier(3, "m_hitPointBonus", 30.0);
    std::vector<ItemSetBonusTier> setBonusTiers = { setBonusTier1, setBonusTier2 };
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.HitPointBonus, 60); // 3*10 from items + 20 from tier 1 + 30 from tier 2
    EXPECT_EQ(stats.GetMaxHitpoints(), 160); // Base 100 + Bonus 60
}

TEST_F(StatCalculatorTest, PowerPipBonusPercentIsAdded)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    auto item = CreateItemWithEffect("m_powerPipBonusPercent", 0.2f); // +20% power pip
    std::vector<CharacterEquippedItem> equipped = { { item.Guid, 0, item.TemplateId, "", 0, 0 } };
    std::vector<ItemTemplateRecord const*> itemTemplates = { &item };
    uint32 itemSetBonusTemplateId = 0;
    std::vector<ItemSetBonusTier> setBonusTiers;
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.PowerPipBonusPercent, 0.2f);
}

TEST_F(StatCalculatorTest, PipConversionRatingIsConvertedToPercent)
{
    _character.Guid = 1;
    _character.Account = 1;
    _character.SchoolId = 0; // Fire
    _character.Level = 1;
    _character.Experience = 0;

    auto item = CreateItemWithEffect("m_pipConversionRating", 100.0); // 100 pip conversion rating
    std::vector<CharacterEquippedItem> equipped = { { item.Guid, 0, item.TemplateId, "", 0, 0 } };
    std::vector<ItemTemplateRecord const*> itemTemplates = { &item };
    uint32 itemSetBonusTemplateId = 0;
    std::vector<ItemSetBonusTier> setBonusTiers;
    std::string problem;

    ComputedStats stats = StatCalculator::Compute(
        *_levels->GetInfo(0, 1),
        equipped,
        itemTemplates,
        itemSetBonusTemplateId,
        setBonusTiers,
        *_effects,
        0, 0, 0, 0.0f, 0.0f, 0,
        problem);

    EXPECT_TRUE(problem.empty()) << problem;
    EXPECT_EQ(stats.PipConversionRating, 100);
    // For level 1 (< 50), using first band: PipConv% = 0.005f + 0.0f * 100 = 0.005f (0.5%)
    EXPECT_FLOAT_EQ(stats.PipConversionPercent, 0.005f);
}