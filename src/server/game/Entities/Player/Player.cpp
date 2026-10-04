/*
 * Project Ambrose by Imjustchico
 * Applies live stat and XP changes to a wizard, persists only character-backed values, consumes a potion only when it holds a full charge, and schedules each later refill with the current interval.
 */

#include "Player.h"

#include <cmath>

bool Player::SetHealth(int32 value) noexcept
{
    bool const changed = _stats.SetHealth(value);
    _dirtyStats = _dirtyStats || changed;
    return changed;
}

bool Player::SetMana(int32 value) noexcept
{
    bool const changed = _stats.SetMana(value);
    _dirtyStats = _dirtyStats || changed;
    return changed;
}

bool Player::SetGold(int64 value) noexcept
{
    bool const changed = _stats.SetGold(value);
    _dirtyStats = _dirtyStats || changed;
    return changed;
}

int64 Player::ModifyGold(int64 amount) noexcept
{
    int64 const overflow = _stats.ModifyGold(amount);
    _dirtyStats = _dirtyStats || overflow != amount;
    return overflow;
}

bool Player::SetPotions(float charge, float maximum) noexcept
{
    bool const changed = _stats.SetPotions(charge, maximum);
    if (changed)
    {
        _dirtyStats = true;
        if (_stats.GetPotionCharge() >= _stats.GetPotionMax())
            _nextPotionRefill.reset();
    }
    return changed;
}

bool Player::SetPotionCapacity(uint32 capacity) noexcept
{
    return SetPotions(static_cast<float>(capacity), static_cast<float>(capacity));
}

bool Player::UsePotion(double restoreFraction, Clock::time_point now, std::chrono::seconds refillInterval) noexcept
{
    if (!_stats.UsePotion(restoreFraction))
        return false;
    _dirtyStats = true;
    _nextPotionRefill = refillInterval.count() > 0 ? std::optional<Clock::time_point>(now + refillInterval) : std::nullopt;
    return true;
}

bool Player::RefillPotion(Clock::time_point now, std::chrono::seconds refillInterval) noexcept
{
    if (!_nextPotionRefill || now < *_nextPotionRefill || !_stats.RefillPotion())
        return false;

    _dirtyStats = true;
    _nextPotionRefill = _stats.GetPotionCharge() < _stats.GetPotionMax() && refillInterval.count() > 0
        ? std::optional<Clock::time_point>(now + refillInterval)
        : std::nullopt;
    return true;
}

bool Player::SetPowerPip(float value) noexcept
{
    bool const changed = _stats.SetPowerPip(value);
    _dirtyStats = _dirtyStats || changed;
    return changed;
}

bool Player::SetShadowPipRating(float value) noexcept
{
    bool const changed = _stats.SetShadowPipRating(value);
    _dirtyStats = _dirtyStats || changed;
    return changed;
}

bool Player::SetXpPercentIncrease(double value) noexcept
{
    if (!std::isfinite(value) || value < 0.0 || _xpPercentIncrease == value)
        return false;
    _xpPercentIncrease = value;
    return true;
}

PlayerLevelChange Player::GiveXP(int64 amount, ExperienceSource source, double rate)
{
    if (!_levels)
    {
        PlayerLevelChange change;
        change.Problem = "the wizard has no level-table snapshot";
        return change;
    }
    PlayerLevelChange change = PlayerLevel::GiveXP(_stats, *_levels, amount, source, rate, _xpPercentIncrease);
    _dirtyStats = _dirtyStats || change.HasChanges();
    return change;
}

PlayerLevelChange Player::SetLevelLocked(bool locked)
{
    if (!_levels)
    {
        PlayerLevelChange change;
        change.Problem = "the wizard has no level-table snapshot";
        return change;
    }
    PlayerLevelChange change = PlayerLevel::SetLevelLocked(_stats, *_levels, locked);
    _dirtyStats = _dirtyStats || change.HasChanges();
    return change;
}

PlayerLevelChange Player::SetLevel(int32 level)
{
    if (!_levels)
    {
        PlayerLevelChange change;
        change.Problem = "the wizard has no level-table snapshot";
        return change;
    }
    PlayerLevelChange change = PlayerLevel::SetLevel(_stats, *_levels, level);
    _dirtyStats = _dirtyStats || change.HasChanges();
    return change;
}
