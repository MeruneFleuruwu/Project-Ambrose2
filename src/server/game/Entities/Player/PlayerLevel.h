/*
 * Project Ambrose by Imjustchico
 * XP awards, level transitions, overflow XP and level locking for a wizard, using the cumulative XP thresholds in its immutable school-level snapshot.
 */

#ifndef AMBROSE_PLAYERLEVEL_H
#define AMBROSE_PLAYERLEVEL_H

#include "PlayerLevels.h"

#include <string>
#include <vector>

class PlayerStats;

enum class ExperienceSource : uint8
{
    Quest,
    Kill,
    Command
};

struct PlayerLevelUpdate
{
    int32 Level = 0;
    int32 Experience = 0;
    int32 TrainingPoints = 0;
};

struct PlayerLevelChange
{
    ExperienceSource Source = ExperienceSource::Quest;
    int32 PreviousLevel = 0;
    int32 Level = 0;
    int32 PreviousExperience = 0;
    int32 Experience = 0;
    int32 PreviousTrainingPoints = 0;
    int32 TrainingPoints = 0;
    int32 PreviousOverflowXP = 0;
    int32 OverflowXP = 0;
    int32 AwardedXP = 0;
    bool LevelLockChanged = false;
    std::vector<PlayerLevelUpdate> Updates;
    std::string Problem;

    bool HasProgressChanges() const noexcept
    {
        return PreviousLevel != Level || PreviousExperience != Experience || PreviousTrainingPoints != TrainingPoints || PreviousOverflowXP != OverflowXP;
    }

    bool HasChanges() const noexcept { return HasProgressChanges() || LevelLockChanged; }
};

class PlayerLevel
{
public:
    static PlayerLevelChange GiveXP(PlayerStats& stats, PlayerLevelSet const& levels, int64 amount, ExperienceSource source, double rate, double bonusPercent);
    static PlayerLevelChange SetLevelLocked(PlayerStats& stats, PlayerLevelSet const& levels, bool locked);
    static PlayerLevelChange SetLevel(PlayerStats& stats, PlayerLevelSet const& levels, int32 level);

private:
    static bool Advance(PlayerStats const& stats, PlayerLevelSet const& levels, int64 pending, PlayerLevelChange& change);
    static bool Apply(PlayerStats& stats, PlayerLevelSet const& levels, PlayerLevelChange const& change, bool locked);
};

#endif
