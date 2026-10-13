/*
 * Project Ambrose by Imjustchico
 * Calculates a wizard's final stats from base level stats, equipped item effects and set bonuses: sums m_hitPointBonus, m_manaBonus, m_damageBonusPercent, m_damageReducePercent, m_accuracyBonusPercent, m_criticalHitRating, m_blockRating, m_powerPipBonusPercent, m_pipConversionRating, m_archmastery and per-school damage/resist/crit/block/pip conversion from each WizStatisticEffect in every equipped item's m_equipEffects, then applies ItemSetBonusTemplate tiers whose NumItemsToEquip threshold is met, and converts ratings to percentages through StatEffectSet for the WizGameStats fields the client reads.
 */

#ifndef AMBROSE_STATCALCULATOR_H
#define AMBROSE_STATCALCULATOR_H

#include "PlayerLevels.h"
#include "StatEffects.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ItemTemplatePart;
struct ItemSetBonusTier;
struct CharacterEquippedItem;
class ItemTemplateRecord;
class CharacterItem;

struct ComputedStats
{
    int32 BaseHitpoints = 0;
    int32 HitPointBonus = 0;
    int32 BaseMana = 0;
    int32 ManaBonus = 0;
    int32 BaseGoldPouch = 0;
    int32 EnergyMax = 0;
    float PowerPipBase = 0.0f;
    float PowerPipBonusPercent = 0.0f;
    int32 PipConversionAll = 0;
    std::array<int32, PlayerLevelInfo::PipSchoolCount> PipConversion{};
    float ShadowPipRating = 0.0f;
    float ArchmasteryBase = 0.0f;
    float ArchmasteryBonus = 0.0f;
    uint32 SchoolId = 0;
    int32 ReferenceLevel = 0;

    // Damage bonuses (flat and percent)
    float DamageBonusPercentAll = 0.0f;
    std::array<float, PlayerLevelInfo::PipSchoolCount> DamageBonusPercentSchool{};
    float DamageReducePercentAll = 0.0f;
    std::array<float, PlayerLevelInfo::PipSchoolCount> DamageReducePercentSchool{};

    // Accuracy
    float AccuracyBonusPercentAll = 0.0f;
    std::array<float, PlayerLevelInfo::PipSchoolCount> AccuracyBonusPercentSchool{};

    // Ratings (before conversion)
    int32 CriticalHitRating = 0;
    int32 BlockRating = 0;
    int32 PipConversionRating = 0;

    // Converted percentages (from StatEffectSet)
    float CriticalHitChance = 0.0f;
    float BlockChance = 0.0f;
    float PipConversionPercent = 0.0f;

    // Current vitals (clamped to maximums)
    int32 CurrentHitpoints = 0;
    int32 CurrentMana = 0;
    int32 CurrentGold = 0;
    float PotionMax = 0.0f;
    float PotionCharge = 0.0f;
    int32 CurrentArenaPoints = 0;

    int32 GetMaxHitpoints() const noexcept { return BaseHitpoints + HitPointBonus; }
    int32 GetMaxMana() const noexcept { return BaseMana + ManaBonus; }
    int32 GetMaxGold() const noexcept { return BaseGoldPouch; }
};

class StatCalculator
{
public:
    StatCalculator() = delete;

    // Computes final stats from base level info, equipped items and set bonuses
    static ComputedStats Compute(PlayerLevelInfo const& base, std::vector<CharacterEquippedItem> const& equipped,
        std::vector<ItemTemplateRecord const*> const& itemTemplates, uint32 itemSetBonusTemplateId,
        std::vector<ItemSetBonusTier> const& setBonusTiers, StatEffectSet const& statEffects,
        int32 currentHitpoints, int32 currentMana, int32 currentGold, float potionMax, float potionCharge, int32 arenaPoints,
        std::string& problem);

private:
    // Sums all WizStatisticEffect fields from an item's EquipEffects
    static void AddItemEffects(ComputedStats& stats, ItemTemplateRecord const* itemTemplate, std::string& problem);
    // Sums all WizStatisticEffect fields from a set bonus tier
    static void AddSetBonusEffects(ComputedStats& stats, ItemSetBonusTier const& tier, std::string& problem);
    // Converts ratings to percentages using StatEffectSet
    static void ConvertRatings(ComputedStats& stats, StatEffectSet const& statEffects, std::string& problem);
    // Extracts a numeric value from an ItemTemplatePart by field name
    static std::optional<double> GetEffectValue(ItemTemplatePart const& part, std::string_view fieldName);
};

#endif