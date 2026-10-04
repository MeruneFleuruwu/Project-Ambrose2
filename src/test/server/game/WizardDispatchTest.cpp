/*
 * Project Ambrose by Imjustchico
 * Checks that MSG_PLAYERWIZBANG and MSG_LOCKLEVEL are dispatched only for an in-world wizard, XP progression updates are server-sent, and the named spellbook state maps to the client identifier while all other states clear it.
 */

#include "GameMessageTable.h"
#include "GameTestHarness.h"
#include "PlayerWizBang.h"
#include "StringHash.h"

#include <gtest/gtest.h>

TEST(WizardDispatchTest, PlayerWizBangIsQueuedForInWorldAndItsReplyIsServerSent)
{
    GameTesting::GameDefinitions definitions;
    MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
    ASSERT_TRUE(catalog);

    MessageInfo const* const playerWizBang = catalog->Find(GameMessages::WizardService, "MSG_PLAYERWIZBANG");
    ASSERT_NE(playerWizBang, nullptr);
    MessageRule const* const handler = GameMessageTable::Get().FindRule(catalog, GameMessages::WizardService, playerWizBang->Definition->Order);
    ASSERT_NE(handler, nullptr);
    EXPECT_EQ(handler->Kind, MessageRuleKind::Handled);
    EXPECT_EQ(handler->Statuses, SessionStatuses::InWorld);
    EXPECT_EQ(handler->Processing, MessageProcessing::Queued);

    MessageInfo const* const wizBang = catalog->Find(GameMessages::GameService, "MSG_WIZBANG");
    ASSERT_NE(wizBang, nullptr);
    MessageRule const* const senderRule = GameMessageTable::Get().FindRule(catalog, GameMessages::GameService, wizBang->Definition->Order);
    ASSERT_NE(senderRule, nullptr);
    EXPECT_EQ(senderRule->Kind, MessageRuleKind::Refused);
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::WizBang>());
}

TEST(WizardDispatchTest, OnlyTheSpellbookStateHasANonzeroWizBangId)
{
    EXPECT_NE(PlayerWizBang::SpellbookId, 0u);
    EXPECT_EQ(PlayerWizBang::SpellbookId, StringHash::KiStringHash("SpellbookWizbang"));
    EXPECT_EQ(PlayerWizBang::IdForState("SpellbookWizbang"), PlayerWizBang::SpellbookId);
    EXPECT_EQ(PlayerWizBang::IdForState(""), 0u);
    EXPECT_EQ(PlayerWizBang::IdForState("Jumping"), 0u);
}

TEST(WizardDispatchTest, LevelLockIsQueuedAndProgressMessagesAreServerSent)
{
    GameTesting::GameDefinitions definitions;
    MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
    ASSERT_TRUE(catalog);

    MessageInfo const* const lockLevel = catalog->Find(GameMessages::Wizard3Service, "MSG_LOCKLEVEL");
    ASSERT_NE(lockLevel, nullptr);
    MessageRule const* const lockHandler = GameMessageTable::Get().FindRule(catalog, GameMessages::Wizard3Service, lockLevel->Definition->Order);
    ASSERT_NE(lockHandler, nullptr);
    EXPECT_EQ(lockHandler->Kind, MessageRuleKind::Handled);
    EXPECT_EQ(lockHandler->Statuses, SessionStatuses::InWorld);
    EXPECT_EQ(lockHandler->Processing, MessageProcessing::Queued);

    EXPECT_TRUE(catalog->IsDeclared<GameMessages::UpdateXP>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::LevelUp>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::UpdateTraining>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::UpdateOverflowXP>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::PetEnergyMax>());
}
