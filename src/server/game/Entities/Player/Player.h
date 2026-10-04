/*
 * Project Ambrose by Imjustchico
 * A live wizard whose stats and XP progression change on the world thread, whose potion refill is timed there, and whose dirty character stats are saved by its session.
 */

#ifndef AMBROSE_PLAYER_H
#define AMBROSE_PLAYER_H

#include "PlayerStats.h"
#include "PlayerLevel.h"

#include <chrono>
#include <memory>
#include <optional>
#include <utility>

class Player
{
public:
    using Clock = std::chrono::steady_clock;

    explicit Player(PlayerStats stats, std::shared_ptr<PlayerLevelSet const> levels = {}) : _stats(std::move(stats)), _levels(std::move(levels)) {}

    PlayerStats const& GetStats() const noexcept { return _stats; }
    bool HasDirtyStats() const noexcept { return _dirtyStats; }
    void ClearDirtyStats() noexcept { _dirtyStats = false; }

    bool SetHealth(int32 value) noexcept;
    bool SetMana(int32 value) noexcept;
    bool SetGold(int64 value) noexcept;
    int64 ModifyGold(int64 amount) noexcept;
    bool SetPotions(float charge, float maximum) noexcept;
    bool SetPotionCapacity(uint32 capacity) noexcept;
    bool UsePotion(double restoreFraction, Clock::time_point now, std::chrono::seconds refillInterval) noexcept;
    bool RefillPotion(Clock::time_point now, std::chrono::seconds refillInterval) noexcept;
    bool SetPowerPip(float value) noexcept;
    bool SetShadowPipRating(float value) noexcept;
    bool SetXpPercentIncrease(double value) noexcept;
    double GetXpPercentIncrease() const noexcept { return _xpPercentIncrease; }
    PlayerLevelChange GiveXP(int64 amount, ExperienceSource source, double rate);
    PlayerLevelChange SetLevelLocked(bool locked);
    PlayerLevelChange SetLevel(int32 level);

private:
    PlayerStats _stats;
    std::shared_ptr<PlayerLevelSet const> _levels;
    std::optional<Clock::time_point> _nextPotionRefill;
    double _xpPercentIncrease = 0.0;
    bool _dirtyStats = false;
};

#endif
