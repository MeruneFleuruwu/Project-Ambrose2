/*
 * Project Ambrose by Imjustchico
 * Applies XP awards against cumulative school thresholds, carries XP across levels, buffers locked and post-cap XP, and updates the base stats a new level gives.
 */

#include "PlayerLevel.h"
#include "PlayerStats.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
    int32 SaturatingAdd(int32 current, int64 amount)
    {
        return static_cast<int32>(std::min<int64>(static_cast<int64>(current) + amount, std::numeric_limits<int32>::max()));
    }
}

PlayerLevelChange PlayerLevel::GiveXP(PlayerStats& stats, PlayerLevelSet const& levels, int64 amount, ExperienceSource source, double rate, double bonusPercent)
{
    PlayerLevelChange change;
    change.Source = source;
    change.PreviousLevel = change.Level = stats.GetLevel();
    change.PreviousExperience = change.Experience = stats.GetExperience();
    change.PreviousTrainingPoints = change.TrainingPoints = stats.GetTrainingPoints();
    change.PreviousOverflowXP = change.OverflowXP = stats.GetOverflowXP();

    if (amount < 0)
    {
        change.Problem = "an XP award cannot be negative";
        return change;
    }
    if (!std::isfinite(rate) || rate < 0.0 || !std::isfinite(bonusPercent) || bonusPercent < 0.0)
    {
        change.Problem = "the XP rate and bonus must be finite and non-negative";
        return change;
    }
    if (amount == 0 || rate == 0.0)
        return change;

    long double const adjusted = std::round(static_cast<long double>(amount) * static_cast<long double>(rate) *
        (1.0L + static_cast<long double>(bonusPercent) / 100.0L));
    if (!std::isfinite(adjusted) || adjusted < 0.0L)
    {
        change.Problem = "the adjusted XP award is not finite";
        return change;
    }
    change.AwardedXP = static_cast<int32>(std::min<long double>(adjusted, std::numeric_limits<int32>::max()));
    if (change.AwardedXP == 0)
        return change;

    if (stats._stored.LevelLocked)
    {
        change.OverflowXP = SaturatingAdd(change.OverflowXP, change.AwardedXP);
        if (!Apply(stats, levels, change, true))
            change.Problem = "the current wizard level has no row in its school's level table";
        return change;
    }

    int64 const pending = static_cast<int64>(change.OverflowXP) + change.AwardedXP;
    PlayerLevelChange advanced = change;
    if (!Advance(stats, levels, pending, advanced))
    {
        change.Problem = std::move(advanced.Problem);
        return change;
    }
    if (!Apply(stats, levels, advanced, false))
    {
        change.Problem = "the resulting wizard level has no row in its school's level table";
        return change;
    }
    change = std::move(advanced);
    return change;
}

PlayerLevelChange PlayerLevel::SetLevelLocked(PlayerStats& stats, PlayerLevelSet const& levels, bool locked)
{
    PlayerLevelChange change;
    change.PreviousLevel = change.Level = stats.GetLevel();
    change.PreviousExperience = change.Experience = stats.GetExperience();
    change.PreviousTrainingPoints = change.TrainingPoints = stats.GetTrainingPoints();
    change.PreviousOverflowXP = change.OverflowXP = stats.GetOverflowXP();

    if (stats._stored.LevelLocked == locked)
        return change;
    change.LevelLockChanged = true;
    if (locked)
    {
        if (!Apply(stats, levels, change, true))
        {
            change.LevelLockChanged = false;
            change.Problem = "the current wizard level has no row in its school's level table";
        }
        return change;
    }

    PlayerLevelChange advanced = change;
    if (advanced.OverflowXP > 0 && !Advance(stats, levels, advanced.OverflowXP, advanced))
    {
        change.LevelLockChanged = false;
        change.Problem = std::move(advanced.Problem);
        return change;
    }
    if (!Apply(stats, levels, advanced, false))
    {
        change.LevelLockChanged = false;
        change.Problem = "the resulting wizard level has no row in its school's level table";
        return change;
    }
    change = std::move(advanced);
    change.LevelLockChanged = true;
    return change;
}

PlayerLevelChange PlayerLevel::SetLevel(PlayerStats& stats, PlayerLevelSet const& levels, int32 level)
{
    PlayerLevelChange change;
    change.PreviousLevel = change.Level = stats.GetLevel();
    change.PreviousExperience = change.Experience = stats.GetExperience();
    change.PreviousTrainingPoints = change.TrainingPoints = stats.GetTrainingPoints();
    change.PreviousOverflowXP = change.OverflowXP = stats.GetOverflowXP();

    PlayerLevelInfo const* const cap = levels.GetInfo(stats.GetSchoolId(), levels.GetMaxLevel());
    if (!cap || level < 1 || level > static_cast<int32>(cap->Level))
    {
        change.Problem = "the requested level is outside this school's level table";
        return change;
    }
    PlayerLevelInfo const* const row = levels.GetInfo(stats.GetSchoolId(), level);
    if (!row || row->Level != static_cast<uint32>(level))
    {
        change.Problem = "the requested level has no row in this school's level table";
        return change;
    }
    if (level == stats.GetLevel())
        return change;

    change.Level = level;
    change.Experience = 0;
    change.OverflowXP = 0;
    change.Updates.push_back({ level, 0, change.TrainingPoints });
    if (!Apply(stats, levels, change, stats._stored.LevelLocked))
    {
        change.Updates.clear();
        change.Problem = "the requested level has no row in this school's level table";
    }
    return change;
}

bool PlayerLevel::Advance(PlayerStats const& stats, PlayerLevelSet const& levels, int64 pending, PlayerLevelChange& change)
{
    PlayerLevelInfo const* const cap = levels.GetInfo(stats.GetSchoolId(), levels.GetMaxLevel());
    if (!cap || change.Level < 1 || change.Level > static_cast<int32>(cap->Level))
    {
        change.Problem = "the current wizard level is outside this school's level table";
        return false;
    }

    if (change.Level == static_cast<int32>(cap->Level))
    {
        change.OverflowXP = SaturatingAdd(0, pending);
        return true;
    }

    PlayerLevelInfo const* current = levels.GetInfo(stats.GetSchoolId(), change.Level);
    if (!current || current->Level != static_cast<uint32>(change.Level) || change.Experience < 0)
    {
        change.Problem = "the current wizard level or experience is invalid";
        return false;
    }

    while (change.Level < static_cast<int32>(cap->Level))
    {
        PlayerLevelInfo const* const next = levels.GetInfo(stats.GetSchoolId(), change.Level + 1);
        if (!next || next->Level != static_cast<uint32>(change.Level + 1))
        {
            change.Problem = "the next level has no row in this school's level table";
            return false;
        }
        int64 const needed = static_cast<int64>(next->XpToLevel) - current->XpToLevel;
        if (needed <= 0)
        {
            change.Problem = "the school's cumulative XP thresholds are not increasing";
            return false;
        }
        int64 const remaining = std::max<int64>(needed - change.Experience, 0);
        if (pending < remaining)
        {
            change.Experience = static_cast<int32>(static_cast<int64>(change.Experience) + pending);
            pending = 0;
            break;
        }

        pending -= remaining;
        change.Level = static_cast<int32>(next->Level);
        change.Experience = 0;
        change.TrainingPoints = SaturatingAdd(change.TrainingPoints, std::max(next->TrainingPoints, 0));
        change.Updates.push_back({ change.Level, change.Experience, change.TrainingPoints });
        current = next;
        if (change.Level == static_cast<int32>(cap->Level))
            break;
    }

    if (change.Level == static_cast<int32>(cap->Level))
        change.OverflowXP = SaturatingAdd(0, pending);
    else
        change.OverflowXP = 0;
    return true;
}

bool PlayerLevel::Apply(PlayerStats& stats, PlayerLevelSet const& levels, PlayerLevelChange const& change, bool locked)
{
    PlayerLevelInfo const* const row = levels.GetInfo(stats.GetSchoolId(), change.Level);
    if (!row || row->Level != static_cast<uint32>(change.Level))
        return false;

    stats._level = change.Level;
    stats._experience = change.Experience;
    stats._stored.OverflowXp = change.OverflowXP;
    stats._stored.TrainingPoints = change.TrainingPoints;
    stats._stored.LevelLocked = locked;
    if (change.PreviousLevel != change.Level)
    {
        stats._base = *row;
        stats._hitpoints = std::max(row->Hitpoints, 0);
        stats._mana = std::max(row->Mana, 0);
        stats._stored.Gold = std::clamp(stats._stored.Gold, 0, std::max(row->Gold, 0));
        stats._powerPip = std::max(row->PipChance, 0.0f);
        stats._shadowPipRating = std::max(row->ShadowPipRating, 0.0f);
    }
    return true;
}
