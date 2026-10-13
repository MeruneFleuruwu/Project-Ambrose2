/*
 * Project Ambrose by Imjustchico
 * A wizard's stats while it plays: its level, experience and school from its character row, what character_stats keeps, and what its school's row for its level gives, the base health, mana, gold pouch, energy, power pip chance, shadow pip rating, archmastery and pip conversion rating, with the stat configuration's shadow pip limit; live health, mana, gold, power pip and potion changes are clamped to those bases; it fills the WizGameStats the player object carries and the ClientMagicSchoolBehavior that holds the level, experience and training points, and gives back the row a save writes.
 */

#ifndef AMBROSE_PLAYERSTATS_H
#define AMBROSE_PLAYERSTATS_H

#include "CharacterStats.h"
#include "CharacterSummary.h"
#include "PlayerLevels.h"
#include "PropertyObject.h"
#include "StatEffects.h"
#include "StatCalculator.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PlayerStats
{
public:
    static constexpr std::string_view ShadowPipMaxSetting = "m_shadowPipMax";

    PlayerStats() = default;

    static std::optional<PlayerStats> Create(CharacterSummary const& character, std::optional<CharacterStats> const& stored, PlayerLevelSet const& levels, StatEffectSet const& effects,
        std::string& problem);

    // Creates a PlayerStats with computed stats from equipped items
    static std::optional<PlayerStats> CreateWithEquipment(CharacterSummary const& character, std::optional<CharacterStats> const& stored, PlayerLevelSet const& levels, StatEffectSet const& effects,
        std::vector<CharacterEquippedItem> const& equipped, std::vector<ItemTemplateRecord const*> const& itemTemplates, uint32 itemSetBonusTemplateId,
        std::vector<ItemSetBonusTier> const& setBonusTiers, std::string& problem);

    uint32 GetSchoolId() const noexcept { return _schoolId; }
    int32 GetLevel() const noexcept { return _level; }
    int32 GetExperience() const noexcept { return _experience; }
    PlayerLevelInfo const& GetBase() const noexcept { return _base; }
    std::optional<int32> GetShadowPipMax() const noexcept { return _shadowPipMax; }
    int32 GetMaxHitpoints() const noexcept { return _base.Hitpoints + _hitPointBonus; }
    int32 GetMaxMana() const noexcept { return _base.Mana + _manaBonus; }
    int32 GetHitpoints() const noexcept { return _hitpoints; }
    int32 GetMana() const noexcept { return _mana; }
    int32 GetGold() const noexcept { return _stored.Gold; }
    float GetPotionCharge() const noexcept { return _stored.PotionCharge; }
    float GetPotionMax() const noexcept { return _stored.PotionMax; }
    float GetPowerPip() const noexcept { return _powerPip; }
    float GetShadowPipRating() const noexcept { return _shadowPipRating; }
    int32 GetTrainingPoints() const noexcept { return _stored.TrainingPoints; }
    std::array<uint32, 3> const& GetPurchasedCustomEmotes() const noexcept { return _stored.PurchasedCustomEmotes; }
    std::array<uint32, 3> const& GetPurchasedCustomTeleportEffects() const noexcept { return _stored.PurchasedCustomTeleportEffects; }
    bool GetShowItemLock() const noexcept { return _stored.ShowItemLock; }

    // Computed stat bonuses from equipment
    int32 GetHitPointBonus() const noexcept { return _hitPointBonus; }
    int32 GetManaBonus() const noexcept { return _manaBonus; }
    float GetPowerPipBonusPercent() const noexcept { return _powerPipBonusPercent; }
    int32 GetPipConversionRating() const noexcept { return _pipConversionRating; }
    float GetCriticalHitChance() const noexcept { return _criticalHitChance; }
    float GetBlockChance() const noexcept { return _blockChance; }
    float GetPipConversionPercent() const noexcept { return _pipConversionPercent; }
    float GetDamageBonusPercentAll() const noexcept { return _damageBonusPercentAll; }
    float GetDamageReducePercentAll() const noexcept { return _damageReducePercentAll; }
    float GetAccuracyBonusPercentAll() const noexcept { return _accuracyBonusPercentAll; }
    float GetArchmasteryBonus() const noexcept { return _archmasteryBonus; }
    std::array<float, 7> const& GetDamageBonusPercentSchool() const noexcept { return _damageBonusPercentSchool; }
    std::array<float, 7> const& GetDamageReducePercentSchool() const noexcept { return _damageReducePercentSchool; }
    std::array<float, 7> const& GetAccuracyBonusPercentSchool() const noexcept { return _accuracyBonusPercentSchool; }

    bool SetHealth(int32 value) noexcept;
    bool SetMana(int32 value) noexcept;
    bool SetGold(int64 value) noexcept;
    int64 ModifyGold(int64 amount) noexcept;
    bool SetPotions(float charge, float maximum) noexcept;
    bool UsePotion(double restoreFraction) noexcept;
    bool RefillPotion() noexcept;
    bool SetPowerPip(float value) noexcept;
    bool SetShadowPipRating(float value) noexcept;
    bool SetShowItemLock(bool value) noexcept;
    CharacterStats ToStored() const;
    bool WriteGameStats(PropertyObject& gameStats, std::string& problem) const;
    bool WriteSchool(PropertyObject& behavior, std::string& problem) const;

    // Recomputes stats when equipment changes
    void RecomputeStats(std::vector<CharacterEquippedItem> const& equipped, std::vector<ItemTemplateRecord const*> const& itemTemplates, uint32 itemSetBonusTemplateId,
        std::vector<ItemSetBonusTier> const& setBonusTiers, StatEffectSet const& statEffects, std::string& problem);

private:
    uint32 _schoolId = 0;
    int32 _level = 0;
    int32 _experience = 0;
    PlayerLevelInfo _base;
    std::optional<int32> _shadowPipMax;
    CharacterStats _stored;
    int32 _hitpoints = 0;
    int32 _mana = 0;
    float _powerPip = 0.0f;
    float _shadowPipRating = 0.0f;

    // Computed stat bonuses from equipment
    int32 _hitPointBonus = 0;
    int32 _manaBonus = 0;
    float _powerPipBonusPercent = 0.0f;
    int32 _pipConversionRating = 0;
    float _criticalHitChance = 0.0f;
    float _blockChance = 0.0f;
    float _pipConversionPercent = 0.0f;
    float _damageBonusPercentAll = 0.0f;
    float _damageReducePercentAll = 0.0f;
    float _accuracyBonusPercentAll = 0.0f;
    float _archmasteryBonus = 0.0f;
    std::array<float, 7> _damageBonusPercentSchool{};
    std::array<float, 7> _damageReducePercentSchool{};
    std::array<float, 7> _accuracyBonusPercentSchool{};
};

#endif
