using NUnit.Framework;
using UnityEngine;

namespace AstralWilds.Tests
{
    public sealed class AstralBattleFormatTests
    {
        [Test]
        public void PlayerPartyRejectsSeventhMemberIntoReserve()
        {
            AstralParty party = new AstralParty(AstralParty.PlayerCapacity);
            AstralReserveCollection reserve = new AstralReserveCollection();
            for (int i = 0; i < 7; i++)
                Assert.AreEqual(i < 6 ? AstralCaptureResult.AddedToParty : AstralCaptureResult.AddedToReserve, party.Capture(new AstralCombatant($"p{i}"), reserve));
            Assert.AreEqual(6, party.Astrals.Count);
            Assert.AreEqual(1, reserve.Astrals.Count);
        }

        [Test]
        public void BattleAllowsExactlyTwoActivePerSideAndFourTotal()
        {
            AstralParty player = Party(6, "p");
            AstralParty opponent = Party(6, "o");
            AstralBattleState battle = new AstralBattleState(player, opponent);
            Assert.IsTrue(battle.TryActivate(BattleSide.Player, 0, 0));
            Assert.IsTrue(battle.TryActivate(BattleSide.Player, 1, 1));
            Assert.IsFalse(battle.TryActivate(BattleSide.Player, 2, 0));
            Assert.IsTrue(battle.TryActivate(BattleSide.Opponent, 0, 0));
            Assert.IsTrue(battle.TryActivate(BattleSide.Opponent, 1, 1));
            Assert.IsFalse(battle.TryActivate(BattleSide.Opponent, 2, 0));
            Assert.AreEqual(4, battle.ActiveAstralCount);
        }

        [Test]
        public void SameAstralCannotOccupyBothSlots()
        {
            AstralParty player = Party(2, "p");
            AstralBattleState battle = new AstralBattleState(player, Party(2, "o"));
            Assert.IsTrue(battle.TryActivate(BattleSide.Player, 0, 0));
            Assert.IsFalse(battle.TryActivate(BattleSide.Player, 0, 1));
        }

        [Test]
        public void SwitchingReplacesOnlySelectedSlotAndKeepsPartyIntact()
        {
            AstralParty player = Party(3, "p");
            AstralBattleState battle = new AstralBattleState(player, Party(2, "o"));
            Assert.IsTrue(battle.TryActivate(BattleSide.Player, 0, 0));
            Assert.IsTrue(battle.TryActivate(BattleSide.Player, 1, 1));
            Assert.IsTrue(battle.TrySwitch(BattleSide.Player, 0, 2));
            Assert.AreEqual("p2", battle.PlayerSlots[0].Astral.Id);
            Assert.AreEqual("p1", battle.PlayerSlots[1].Astral.Id);
            Assert.AreEqual(3, player.Astrals.Count);
        }

        [Test]
        public void DefeatedAstralsCannotActivateOrReturnThroughSwitch()
        {
            AstralParty player = Party(3, "p");
            AstralBattleState battle = new AstralBattleState(player, Party(2, "o"));
            player.Astrals[0].MarkDefeated();
            Assert.IsFalse(battle.TryActivate(BattleSide.Player, 0, 0));
            Assert.IsTrue(battle.TryActivate(BattleSide.Player, 1, 0));
            Assert.IsFalse(battle.TrySwitch(BattleSide.Player, 0, 0));
        }

        [Test]
        public void BattleEndsWhenOnePartyHasNoUsableAstrals()
        {
            AstralParty player = Party(2, "p");
            AstralParty opponent = Party(2, "o");
            AstralBattleState battle = new AstralBattleState(player, opponent);
            Assert.IsFalse(battle.HasBattleEnded);
            opponent.Astrals[0].MarkDefeated();
            opponent.Astrals[1].MarkDefeated();
            Assert.IsTrue(battle.HasBattleEnded);
        }

        [Test]
        public void UiSnapshotExposesTwoActiveSlotsSixPartySlotsAndFourBattlePositions()
        {
            AstralBattleUiSnapshot snapshot = new AstralBattleUiSnapshot(new AstralBattleState(Party(6, "p"), Party(6, "o")));
            Assert.AreEqual(2, snapshot.PlayerActive.Count);
            Assert.AreEqual(2, snapshot.OpponentActive.Count);
            Assert.AreEqual(6, snapshot.PlayerPartySlotCount);
            Assert.AreEqual(4, snapshot.BattlefieldPositionCount);
        }

        [Test]
        public void DefeatedActiveSlotCanBeReplacedWithoutChangingOtherSlot()
        {
            AstralParty player = Party(3, "p");
            AstralBattleState battle = new AstralBattleState(player, Party(2, "o"));
            Assert.IsTrue(battle.TryActivate(BattleSide.Player, 0, 0));
            Assert.IsTrue(battle.TryActivate(BattleSide.Player, 1, 1));
            player.Astrals[0].MarkDefeated();
            Assert.IsTrue(battle.TrySwitch(BattleSide.Player, 0, 2));
            Assert.AreEqual("p2", battle.PlayerSlots[0].Astral.Id);
            Assert.AreEqual("p1", battle.PlayerSlots[1].Astral.Id);
        }

        [Test]
        public void CampaignRecruitmentUsesReserveAndRejectsDuplicateRewards()
        {
            var campaign = new AstralCampaignState();
            for (int i = 0; i < 7; i++)
                Assert.AreEqual(i < 6 ? AstralRecruitmentResult.AddedToParty : AstralRecruitmentResult.AddedToReserve, campaign.Recruit($"a{i}"));
            Assert.AreEqual(AstralRecruitmentResult.DuplicateReward, campaign.Recruit("a6"));
            Assert.AreEqual(6, campaign.Party.Count);
            Assert.AreEqual(1, campaign.Reserve.Count);
        }

        [Test]
        public void CampaignSaveReloadPreservesPartyReserveAndObjectiveState()
        {
            var campaign = new AstralCampaignState();
            campaign.Recruit("cindrel", "Cindrel");
            campaign.Recruit("mossling", "Mossling");
            campaign.Recruit("reserve-astral");
            campaign.TrySetStage(AstralCampaignStage.SecondEncounter);
            campaign.CompleteBeaconObjective();

            var loaded = AstralCampaignState.LoadJson(campaign.SaveJson());
            Assert.AreEqual(AstralCampaignStage.ReturnToExploration, loaded.Stage);
            Assert.IsTrue(loaded.BeaconActivated);
            Assert.AreEqual(3, loaded.Party.Count);
            Assert.AreEqual("Cindrel", loaded.Party[0].DisplayName);
        }

        [Test]
        public void ReserveAstralCanReplaceAnActiveSlotAndDuplicateActionIsRejected()
        {
            AstralParty player = Party(2, "p");
            AstralReserveCollection reserve = new AstralReserveCollection();
            reserve.TryAdd(new AstralCombatant("reserve0"));
            AstralBattleState battle = new AstralBattleState(player, Party(2, "o"));
            Assert.IsTrue(battle.TryActivate(BattleSide.Player, 0, 0));
            Assert.IsTrue(battle.TryQueueAction(BattleSide.Player, 0, new QueuedAstralAction("scan", TargetScope.Self)));
            Assert.IsFalse(battle.TryQueueAction(BattleSide.Player, 0, new QueuedAstralAction("scan", TargetScope.Self)));
            Assert.IsTrue(battle.TrySwitchFromReserve(BattleSide.Player, 0, reserve, 0));
            Assert.AreEqual("reserve0", battle.PlayerSlots[0].Astral.Id);
            Assert.AreEqual(0, reserve.Astrals.Count);
        }

        private static AstralParty Party(int count, string prefix)
        {
            AstralParty party = new AstralParty(count);
            for (int i = 0; i < count; i++)
                Assert.IsTrue(party.TryAdd(new AstralCombatant($"{prefix}{i}")));
            return party;
        }
    }
}
