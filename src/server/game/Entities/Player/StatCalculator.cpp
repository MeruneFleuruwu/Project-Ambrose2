/*
 * Project Ambrose by Imjustchico
 * Implements StatCalculator: sums base stats from player_level_stats, each equipped item's WizStatisticEffect m_equipEffects (m_hitPointBonus, m_manaBonus, m_damageBonusPercent, m_damageReducePercent, m_accuracyBonusPercent, m_criticalHitRating, m_blockRating, m_powerPipBonusPercent, m_pipConversionRating, m_archmastery, per-school via effect category), ItemSetBonusTemplate tiers by equipped count, then converts ratings to percentages through StatEffectSet for WizGameStats.
 */

#include "StatCalculator.h"
#include "ItemTemplateRecord.h"
#include "PlayerEquipment.h"
#include "PropertyObject.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace
{
    // Magic school indices in the client order: Fire, Ice, Storm, Life, Myth, Death, Balance
    constexpr std::array<std::string_view, 7> SchoolNames = { "Fire", "Ice", "Storm", "Life", "Myth", "Death", "Balance" };

    int32 SchoolIndex(std::string_view school)
    {
        for (int32 i = 0; i < 7; ++i)
            if (SchoolNames[i] == school)
                return i;
        return -1;
    }
}

ComputedStats StatCalculator::Compute(PlayerLevelInfo const& base, std::vector<CharacterEquippedItem> const& equipped,
    std::vector<ItemTemplateRecord const*> const& itemTemplates, uint32 itemSetBonusTemplateId,
    std::vector<ItemSetBonusTier> const& setBonusTiers, StatEffectSet const& statEffects,
    int32 currentHitpoints, int32 currentMana, int32 currentGold, float potionMax, float potionCharge, int32 arenaPoints,
    std::string& problem)
{
    problem.clear();

    ComputedStats stats;
    stats.BaseHitpoints = base.Hitpoints;
    stats.BaseMana = base.Mana;
    stats.BaseGoldPouch = base.Gold;
    stats.EnergyMax = base.PetEnergy;
    stats.PowerPipBase = base.PipChance;
    stats.PipConversionAll = base.PipConversionAll;
    stats.PipConversion = base.PipConversion;
    stats.ShadowPipRating = base.ShadowPipRating;
    stats.ArchmasteryBase = base.Archmastery;
    stats.SchoolId = base.SchoolId;
    stats.ReferenceLevel = static_cast<int32>(base.Level);

    // Sum equip effects from all equipped items
    for (std::size_t i = 0; i < equipped.size() && i < itemTemplates.size(); ++i)
    {
        ItemTemplateRecord const* itemTemplate = itemTemplates[i];
        if (itemTemplate)
            AddItemEffects(stats, itemTemplate, problem);
        if (!problem.empty())
            return stats;
    }

    // Sum set bonus effects for tiers whose threshold is met
    int32 equippedCount = static_cast<int32>(equipped.size());
    for (ItemSetBonusTier const& tier : setBonusTiers)
    {
        if (tier.NumItemsToEquip <= equippedCount)
            AddSetBonusEffects(stats, tier, problem);
        if (!problem.empty())
            return stats;
    }

    // Convert ratings to percentages
    ConvertRatings(stats, statEffects, problem);
    if (!problem.empty())
        return stats;

    // Clamp current vitals to new maximums
    stats.CurrentHitpoints = std::clamp(currentHitpoints, 0, std::max(stats.GetMaxHitpoints(), 0));
    stats.CurrentMana = std::clamp(currentMana, 0, std::max(stats.GetMaxMana(), 0));
    stats.CurrentGold = std::clamp(currentGold, 0, std::max(stats.BaseGoldPouch, 0));
    stats.PotionMax = std::max(potionMax, 0.0f);
    stats.PotionCharge = std::clamp(potionCharge, 0.0f, stats.PotionMax);
    stats.CurrentArenaPoints = arenaPoints;

    return stats;
}

void StatCalculator::AddItemEffects(ComputedStats& stats, ItemTemplateRecord const* itemTemplate, std::string& problem)
{
    for (ItemTemplatePart const& part : itemTemplate->EquipEffects)
    {
        // The EffectName tells us which WizStatisticEffect field this part represents
        std::optional<std::string> const& effectName = part.EffectName;
        if (!effectName)
            continue;

        std::optional<double> const value = GetEffectValue(part, "NumericValue");
        if (!value)
            continue;

        float const fval = static_cast<float>(*value);

        if (*effectName == "m_hitPointBonus")
            stats.HitPointBonus += static_cast<int32>(std::lround(fval));
        else if (*effectName == "m_manaBonus")
            stats.ManaBonus += static_cast<int32>(std::lround(fval));
        else if (*effectName == "m_damageBonusPercent")
        {
            std::optional<double> const school = GetEffectValue(part, "MagicSchool");
            if (school)
            {
                int32 idx = SchoolIndex(*school);
                if (idx >= 0)
                    stats.DamageBonusPercentSchool[idx] += fval;
            }
            else
                stats.DamageBonusPercentAll += fval;
        }
        else if (*effectName == "m_damageReducePercent")
        {
            std::optional<double> const school = GetEffectValue(part, "MagicSchool");
            if (school)
            {
                int32 idx = SchoolIndex(*school);
                if (idx >= 0)
                    stats.DamageReducePercentSchool[idx] += fval;
            }
            else
                stats.DamageReducePercentAll += fval;
        }
        else if (*effectName == "m_accuracyBonusPercent")
        {
            std::optional<double> const school = GetEffectValue(part, "MagicSchool");
            if (school)
            {
                int32 idx = SchoolIndex(*school);
                if (idx >= 0)
                    stats.AccuracyBonusPercentSchool[idx] += fval;
            }
            else
                stats.AccuracyBonusPercentAll += fval;
        }
        else if (*effectName == "m_criticalHitRating")
            stats.CriticalHitRating += static_cast<int32>(std::lround(fval));
        else if (*effectName == "m_blockRating")
            stats.BlockRating += static_cast<int32>(std::lround(fval));
        else if (*effectName == "m_powerPipBonusPercent")
            stats.PowerPipBonusPercent += fval;
        else if (*effectName == "m_pipConversionRating")
            stats.PipConversionRating += static_cast<int32>(std::lround(fval));
        else if (*effectName == "m_archmastery")
            stats.ArchmasteryBonus += fval;
    }
}

void StatCalculator::AddSetBonusEffects(ComputedStats& stats, ItemSetBonusTier const& tier, std::string& problem)
{
    for (ItemTemplatePart const& part : tier.Effects)
    {
        std::optional<std::string> const& effectName = part.EffectName;
        if (!effectName)
            continue;

        std::optional<double> const value = GetEffectValue(part, "NumericValue");
        if (!value)
            continue;

        float const fval = static_cast<float>(*value);

        if (*effectName == "m_hitPointBonus")
            stats.HitPointBonus += static_cast<int32>(std::lround(fval));
        else if (*effectName == "m_manaBonus")
            stats.ManaBonus += static_cast<int32>(std::lround(fval));
        else if (*effectName == "m_damageBonusPercent")
        {
            std::optional<double> const school = GetEffectValue(part, "MagicSchool");
            if (school)
            {
                int32 idx = SchoolIndex(*school);
                if (idx >= 0)
                    stats.DamageBonusPercentSchool[idx] += fval;
            }
            else
                stats.DamageBonusPercentAll += fval;
        }
        else if (*effectName == "m_damageReducePercent")
        {
            std::optional<double> const school = GetEffectValue(part, "MagicSchool");
            if (school)
            {
                int32 idx = SchoolIndex(*school);
                if (idx >= 0)
                    stats.DamageReducePercentSchool[idx] += fval;
            }
            else
                stats.DamageReducePercentAll += fval;
        }
        else if (*effectName == "m_accuracyBonusPercent")
        {
            std::optional<double> const school = GetEffectValue(part, "MagicSchool");
            if (school)
            {
                int32 idx = SchoolIndex(*school);
                if (idx >= 0)
                    stats.AccuracyBonusPercentSchool[idx] += fval;
            }
            else
                stats.AccuracyBonusPercentAll += fval;
        }
        else if (*effectName == "m_criticalHitRating")
            stats.CriticalHitRating += static_cast<int32>(std::lround(fval));
        else if (*effectName == "m_blockRating")
            stats.BlockRating += static_cast<int32>(std::lround(fval));
        else if (*effectName == "m_powerPipBonusPercent")
            stats.PowerPipBonusPercent += fval;
        else if (*effectName == "m_pipConversionRating")
            stats.PipConversionRating += static_cast<int32>(std::lround(fval));
        else if (*effectName == "m_archmastery")
            stats.ArchmasteryBonus += fval;
    }
}

void StatCalculator::ConvertRatings(ComputedStats& stats, StatEffectSet const& statEffects, std::string& problem)
{
    // Critical hit rating -> chance
    if (stats.CriticalHitRating > 0)
    {
        // Find the band for the reference level
        for (CritAndBlockValues const& band : statEffects.GetData().CritAndBlock)
        {
            if (band.MinLevel <= stats.ReferenceLevel)
            {
                if (stats.CriticalHitRating <= band.CapValue)
                {
                    stats.CriticalHitChance = band.CriticalHitScalarBase + band.CriticalHitScalingFactor * stats.CriticalHitRating;
                    break;
                }
                else
                {
                    // Above cap, use cap value
                    stats.CriticalHitChance = band.CriticalHitScalarBase + band.CriticalHitScalingFactor * band.CapValue;
                    break;
                }
            }
        }
    }

    // Block rating -> chance
    if (stats.BlockRating > 0)
    {
        for (CritAndBlockValues const& band : statEffects.GetData().CritAndBlock)
        {
            if (band.MinLevel <= stats.ReferenceLevel)
            {
                if (stats.BlockRating <= band.CapValue)
                {
                    stats.BlockChance = band.BlockScalarBase + band.BlockScalingFactor * stats.BlockRating;
                    break;
                }
                else
                {
                    stats.BlockChance = band.BlockScalarBase + band.BlockScalingFactor * band.CapValue;
                    break;
                }
            }
        }
    }

    // Pip conversion rating -> percent
    if (stats.PipConversionRating > 0)
    {
        for (PipConversionValues const& band : statEffects.GetData().PipConversion)
        {
            if (band.MinLevel <= stats.ReferenceLevel)
            {
                if (stats.PipConversionRating <= band.CapValue)
                {
                    stats.PipConversionPercent = band.ScalarBase + band.ScalingFactor * stats.PipConversionRating;
                    break;
                }
                else
                {
                    stats.PipConversionPercent = band.ScalarBase + band.ScalingFactor * band.CapValue;
                    break;
                }
            }
        }
    }
}

std::optional<double> StatCalculator::GetEffectValue(ItemTemplatePart const& part, std::string_view fieldName)
{
    // The NumericValue field is the primary numeric value for effects
    if (fieldName == "NumericValue")
        return part.NumericValue;

    if (fieldName == "MagicSchool" && part.MagicSchool)
        return 0.0; // Just a marker that school is present, actual value is in NumericValue

    if (fieldName == "LookupIndex" && part.LookupIndex)
        return static_cast<double>(*part.LookupIndex);

    if (fieldName == "PipsGiven" && part.PipsGiven)
        return static_cast<double>(*part.PipsGiven);

    if (fieldName == "PowerPipsGiven" && part.PowerPipsGiven)
        return static_cast<double>(*part.PowerPipsGiven);

    if (fieldName == "NumSpells" && part.NumSpells)
        return static_cast<double>(*part.NumSpells);

    if (fieldName == "SpeedMultiplier" && part.SpeedMultiplier)
        return static_cast<double>(*part.SpeedMultiplier);

    if (fieldName == "OperatorType" && part.OperatorType)
        return static_cast<double>(*part.OperatorType);

    if (fieldName == "Quantity" && part.Quantity)
        return static_cast<double>(*part.Quantity);

    if (fieldName == "ItemTemplateId" && part.ItemTemplateId)
        return static_cast<double>(*part.ItemTemplateId);

    return std::nullopt;
}