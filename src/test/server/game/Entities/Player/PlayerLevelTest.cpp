/*
 * Project Ambrose by Imjustchico
 * Tests XP awards, rate and bonus adjustment, carry-over across multiple cumulative level thresholds, level-lock buffering, maximum-level overflow and direct level changes.
 */

#include "Player.h"
#include "PlayerStatsFixtures.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    class PlayerLevelTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::vector<std::string> errors;
            _levels = PlayerLevelSet::Build(PlayerStatsFixtures::FireLevels(5), errors);
            ASSERT_TRUE(_levels) << (errors.empty() ? std::string() : errors.front());
            _effects = StatEffectSet::Build({ { { "m_shadowPipMax", 2.0 } }, {}, {} }, errors);
            ASSERT_TRUE(_effects) << (errors.empty() ? std::string() : errors.front());
            _character.Guid = 7;
            _character.Account = 3;
            _character.SchoolId = PlayerStatsFixtures::Fire;
            _character.Level = 1;
            _character.Experience = 20;
        }

        std::optional<Player> MakePlayer(std::optional<CharacterStats> const& stored = std::nullopt)
        {
            std::string problem;
            std::optional<PlayerStats> stats = PlayerStats::Create(_character, stored, *_levels, *_effects, problem);
            if (!stats)
            {
                ADD_FAILURE() << problem;
                return std::nullopt;
            }
            return Player(std::move(*stats), _levels);
        }

        std::shared_ptr<PlayerLevelSet const> _levels;
        std::shared_ptr<StatEffectSet const> _effects;
        CharacterSummary _character;
    };
}

TEST_F(PlayerLevelTest, AnAwardOfTwoAndAHalfLevelsCarriesTheRemainderAndTrainingPoints)
{
    PlayerLevelData levelData = PlayerStatsFixtures::FireLevels(5);
    for (PlayerLevelInfo& level : levelData.Levels)
        if (level.Level == 2)
            level.TrainingPoints = 1;

    std::vector<std::string> errors;
    std::shared_ptr<PlayerLevelSet const> levels = PlayerLevelSet::Build(std::move(levelData), errors);
    ASSERT_TRUE(levels) << (errors.empty() ? std::string() : errors.front());

    std::string problem;
    std::optional<PlayerStats> stats = PlayerStats::Create(_character, std::nullopt, *levels, *_effects, problem);
    ASSERT_TRUE(stats) << problem;
    Player player(std::move(*stats), levels);

    PlayerLevelChange const change = player.GiveXP(498, ExperienceSource::Quest, 1.0);

    EXPECT_TRUE(change.Problem.empty());
    EXPECT_EQ(change.AwardedXP, 498);
    ASSERT_EQ(change.Updates.size(), 2u);
    EXPECT_EQ(change.Updates[0].Level, 2);
    EXPECT_EQ(change.Updates[0].TrainingPoints, 2);
    EXPECT_EQ(change.Updates[1].Level, 3);
    EXPECT_EQ(change.Updates[1].TrainingPoints, 3);
    EXPECT_EQ(player.GetStats().GetLevel(), 3);
    EXPECT_EQ(player.GetStats().GetExperience(), 158);
    EXPECT_EQ(player.GetStats().GetTrainingPoints(), 3);
    EXPECT_EQ(player.GetStats().GetMaxHitpoints(), 445);
    EXPECT_EQ(player.GetStats().GetHitpoints(), 445);
    EXPECT_TRUE(player.HasDirtyStats());
}

TEST_F(PlayerLevelTest, AXPBonusAndTheLiveRateMultiplyTheAward)
{
    std::optional<Player> created = MakePlayer();
    ASSERT_TRUE(created);
    Player& player = *created;
    ASSERT_TRUE(player.SetXpPercentIncrease(50.0));

    PlayerLevelChange const change = player.GiveXP(100, ExperienceSource::Quest, 2.0);

    EXPECT_TRUE(change.Problem.empty());
    EXPECT_EQ(change.AwardedXP, 300);
    EXPECT_EQ(player.GetStats().GetLevel(), 2);
    EXPECT_EQ(player.GetStats().GetExperience(), 185);
}

TEST_F(PlayerLevelTest, AQuestRateOfTwoDoublesAnUnbonusedAward)
{
    std::optional<Player> created = MakePlayer();
    ASSERT_TRUE(created);
    Player& player = *created;

    PlayerLevelChange const change = player.GiveXP(100, ExperienceSource::Quest, 2.0);

    EXPECT_TRUE(change.Problem.empty());
    EXPECT_EQ(change.AwardedXP, 200);
}

TEST_F(PlayerLevelTest, LockedXPIsBufferedAndConsumedWhenTheLevelIsUnlocked)
{
    CharacterStats stored;
    stored.LevelLocked = true;
    std::optional<Player> created = MakePlayer(stored);
    ASSERT_TRUE(created);
    Player& player = *created;

    PlayerLevelChange const gained = player.GiveXP(200, ExperienceSource::Quest, 1.0);
    ASSERT_TRUE(gained.Problem.empty());
    EXPECT_EQ(player.GetStats().GetLevel(), 1);
    EXPECT_EQ(player.GetStats().GetExperience(), 20);
    EXPECT_EQ(player.GetStats().GetOverflowXP(), 200);

    PlayerLevelChange const unlocked = player.SetLevelLocked(false);

    EXPECT_TRUE(unlocked.Problem.empty());
    EXPECT_EQ(unlocked.Updates.size(), 1u);
    EXPECT_EQ(player.GetStats().GetLevel(), 2);
    EXPECT_EQ(player.GetStats().GetExperience(), 85);
    EXPECT_EQ(player.GetStats().GetOverflowXP(), 0);
    EXPECT_FALSE(player.GetStats().IsLevelLocked());
}

TEST_F(PlayerLevelTest, XPAtTheMaximumLevelGoesToOverflow)
{
    _character.Level = 5;
    std::optional<Player> created = MakePlayer();
    ASSERT_TRUE(created);
    Player& player = *created;

    PlayerLevelChange const change = player.GiveXP(900, ExperienceSource::Quest, 1.0);

    EXPECT_TRUE(change.Problem.empty());
    EXPECT_TRUE(change.Updates.empty());
    EXPECT_EQ(player.GetStats().GetLevel(), 5);
    EXPECT_EQ(player.GetStats().GetExperience(), 20);
    EXPECT_EQ(player.GetStats().GetOverflowXP(), 900);
}

TEST_F(PlayerLevelTest, ADirectLevelChangeClearsProgressAndOverflowButPreservesTrainingPoints)
{
    CharacterStats stored;
    stored.OverflowXp = 100;
    stored.TrainingPoints = 7;
    std::optional<Player> created = MakePlayer(stored);
    ASSERT_TRUE(created);
    Player& player = *created;

    PlayerLevelChange const change = player.SetLevel(4);

    EXPECT_TRUE(change.Problem.empty());
    EXPECT_EQ(player.GetStats().GetLevel(), 4);
    EXPECT_EQ(player.GetStats().GetExperience(), 0);
    EXPECT_EQ(player.GetStats().GetOverflowXP(), 0);
    EXPECT_EQ(player.GetStats().GetTrainingPoints(), 7);
    EXPECT_EQ(player.GetStats().GetHitpoints(), 460);
    EXPECT_EQ(player.GetStats().GetMaxHitpoints(), 460);
}

TEST_F(PlayerLevelTest, InvalidAwardsAndBonusesAreRejectedWithoutChangingProgress)
{
    std::optional<Player> created = MakePlayer();
    ASSERT_TRUE(created);
    Player& player = *created;
    PlayerLevelChange const negative = player.GiveXP(-1, ExperienceSource::Quest, 1.0);
    EXPECT_FALSE(negative.Problem.empty());
    EXPECT_EQ(player.GetStats().GetExperience(), 20);
    EXPECT_FALSE(player.SetXpPercentIncrease(std::numeric_limits<double>::quiet_NaN()));
}
