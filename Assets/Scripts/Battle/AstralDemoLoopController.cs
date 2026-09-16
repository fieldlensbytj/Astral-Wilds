using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.InputSystem;

namespace AstralWilds
{
    /// <summary>
    /// Minimal playable prototype loop for the demo scene. The combat rules are
    /// intentionally simple and editable; this is not the final combat model.
    /// </summary>
    [DefaultExecutionOrder(-100)]
    public sealed class AstralDemoLoopController : MonoBehaviour
    {
        private enum Flow { Exploration, Encounter, Battle, Recruitment, PartyManagement, Vendor, Victory, Defeat }

        [Serializable] private sealed class DemoMember
        {
            public string id;
            public string displayName;
            public int hp = 30;
            public int maxHp = 30;
            public bool defeated;
        }

        [Serializable] private sealed class SaveData
        {
            public int version = 6;
            public Flow flow;
            public bool beaconActivated;
            public int encountersCompleted;
            public bool victoryAcknowledged;
            // Retained solely so v1-v3 files migrate their Starshards into economy.
            public long starshards;
            public AstralEconomySaveData economy;
            public int alloySold;
            public bool wayfarerCommissionComplete;
            public List<DemoMember> party = new List<DemoMember>();
            public List<DemoMember> reserve = new List<DemoMember>();
            public List<string> clearedEncounterZoneIds = new List<string>();
            public List<string> collectedCurrencyPickupIds = new List<string>();
            public List<string> collectedItemPickupIds = new List<string>();
        }

        private const string SaveFileName = "astralwilds-demo-save-v1.json";
        private static readonly string[] DirectionNames = { "north", "northeast", "east", "southeast", "south", "southwest", "west", "northwest" };
        private readonly List<DemoMember> party = new List<DemoMember>();
        private readonly List<DemoMember> reserve = new List<DemoMember>();
        private readonly DemoMember[] opponent = new DemoMember[2];
        private readonly int[] activeParty = { 0, 1 };
        private readonly int[] activeOpponent = { 0, 1 };
        private Flow flow = Flow.Exploration;
        private int selectedActiveSlot;
        private int selectedTargetSlot;
        private int encountersCompleted;
        private bool beaconActivated;
        private bool restartPending;
        private bool victoryAcknowledged;
        private AstralBattleState battle;
        private readonly bool[] acted = new bool[2];
        private readonly bool[] guarded = new bool[2];
        private readonly AstralWallet wallet = new AstralWallet();
        private readonly AstralInventory inventory = new AstralInventory();
        private readonly AstralWayfarerCommission wayfarerCommission = new AstralWayfarerCommission();
        private readonly HashSet<string> collectedCurrencyPickupIds = new HashSet<string>(StringComparer.Ordinal);
        private readonly HashSet<string> collectedItemPickupIds = new HashSet<string>(StringComparer.Ordinal);
        private AstralPlayerController playerController;
        private AstralThirdPersonCamera orbitCamera;
        private CrashedBeaconObjective beacon;
        private AstralEncounterZone[] encounterZones = Array.Empty<AstralEncounterZone>();
        private AstralCurrencyPickup[] currencyPickups = Array.Empty<AstralCurrencyPickup>();
        private AstralItemPickup[] itemPickups = Array.Empty<AstralItemPickup>();
        private AstralVendorStation vendorStation;
        private AstralEncounterZone activeEncounterZone;
        private string message = "Explore the wilds and find the marked Astral activity. E remains reserved for the beacon objective.";

        public bool InBattle => flow == Flow.Battle;
        public int PartyCount => party.Count;
        public int ReserveCount => reserve.Count;
        public string CurrentState => flow.ToString();
        public string StatusMessage => message;
        public bool BlocksExplorationInput => flow != Flow.Exploration || restartPending;

        // --- Public UI-facing state (additive; keyboard controls above are unchanged) ---

        [Serializable]
        public struct AstralUiInfo
        {
            public string id;
            public string displayName;
            public int hp;
            public int maxHp;
            public bool defeated;
            public bool isActive;
            public int activeSlot; // -1 when not active
        }

        public bool IsExploration => flow == Flow.Exploration && !restartPending;
        public bool IsEncounter => flow == Flow.Encounter;
        public bool IsRecruitment => flow == Flow.Recruitment;
        public bool IsPartyManagement => flow == Flow.PartyManagement;
        public bool IsVendor => flow == Flow.Vendor;
        public bool IsDefeat => flow == Flow.Defeat;
        public bool IsVictory => flow == Flow.Victory;
        public bool IsRestartPending => restartPending;
        public bool CanBeginEncounter => IsExploration && TryGetEncounterZoneAtPlayer(out _);
        public string EncounterActionLabel => CanBeginEncounter ? "Encounter (B)" : "Find wild activity";
        public int SelectedActiveSlot => selectedActiveSlot;
        public int SelectedTargetSlot => selectedTargetSlot;
        public int EncountersCompleted => encountersCompleted;
        public long Starshards => wallet.Balance;
        public int SalvagedAlloy => inventory.GetCount(AstralItemId.SalvagedAlloy);
        public int FieldTonics => inventory.GetCount(AstralItemId.FieldTonic);
        public int AlloySold => wayfarerCommission.AlloySold;
        public bool WayfarerCommissionComplete => wayfarerCommission.IsComplete;
        public bool CanUseVendor => IsExploration && TryGetVendorAtPlayer();
        public bool CanUseFieldTonic => IsExploration && FieldTonics > 0 && FindTonicTarget() != null;
        public bool BeaconActivated => beacon != null ? beacon.IsActivated : beaconActivated;
        public string ObjectiveGuidance => BuildObjectiveGuidance();
        // The vision doc's short demo objective: explore, clear two distinct encounters,
        // and return to (activate) the crashed corvette's beacon.
        public bool DemoObjectiveComplete => BeaconActivated && encountersCompleted >= 2;

        public List<AstralUiInfo> GetPartyUiInfo()
        {
            var list = new List<AstralUiInfo>(party.Count);
            for (int i = 0; i < party.Count; i++)
            {
                int activeSlot = (i == activeParty[0]) ? 0 : (i == activeParty[1]) ? 1 : -1;
                list.Add(ToUiInfo(party[i], activeSlot >= 0, activeSlot));
            }
            return list;
        }

        public List<AstralUiInfo> GetReserveUiInfo()
        {
            var list = new List<AstralUiInfo>(reserve.Count);
            foreach (var member in reserve)
                list.Add(ToUiInfo(member, false, -1));
            return list;
        }

        public List<AstralUiInfo> GetOpponentUiInfo()
        {
            var list = new List<AstralUiInfo>(2);
            if (flow != Flow.Battle || opponent[0] == null)
                return list;
            for (int i = 0; i < opponent.Length; i++)
                list.Add(ToUiInfo(opponent[i], true, i));
            return list;
        }

        private static AstralUiInfo ToUiInfo(DemoMember member, bool isActive, int activeSlot)
        {
            return new AstralUiInfo
            {
                id = member.id,
                displayName = member.displayName,
                hp = member.hp,
                maxHp = member.maxHp,
                defeated = member.defeated,
                isActive = isActive,
                activeSlot = activeSlot
            };
        }

        // --- Public UI action API; each mirrors the equivalent keyboard handler above ---

        public void UiSelectActiveSlot(int slot) { if (flow == Flow.Battle && slot >= 0 && slot < 2) selectedActiveSlot = slot; }
        public void UiSelectTargetSlot(int slot) { if (flow == Flow.Battle && slot >= 0 && slot < 2) selectedTargetSlot = slot; }
        public void UiAttack() { if (flow == Flow.Battle) ResolvePlayerAction(); }
        public void UiArcBurst() { if (flow == Flow.Battle) ResolveArcBurst(); }
        public void UiGuard() { if (flow == Flow.Battle) GuardSelectedActive(); }
        public void UiReplaceFainted() { if (flow == Flow.Battle) ReplaceFaintedFromReserve(); }
        public void UiSwapBench() { if (flow == Flow.Battle) SwitchToBench(false); }
        public void UiBeginEncounter() { if (flow == Flow.Exploration && !restartPending) TryBeginEncounter(); }
        public void UiBeginBattle() { if (flow == Flow.Encounter) BeginBattle(); }
        public void UiRecruit() { if (flow == Flow.Recruitment) RecruitReward(); }
        public void UiOpenPartyManagement() { if (flow == Flow.Exploration && !restartPending) flow = Flow.PartyManagement; }
        public void UiReorderParty() { if (flow == Flow.PartyManagement) ReorderParty(); }
        public void UiConfirmDefeatRecovery() { if (flow == Flow.Defeat) ReturnToExploration(); }
        public void UiReturnToExploration() { if (flow == Flow.PartyManagement) ReturnToExploration(); }
        public void UiContinueAfterVictory() { if (flow == Flow.Victory) ContinueAfterVictory(); }
        public void UiOpenVendor() { if (CanUseVendor) OpenVendor(); }
        public void UiBuyFieldTonic() { if (flow == Flow.Vendor) BuyFieldTonic(); }
        public void UiSellSalvagedAlloy() { if (flow == Flow.Vendor) SellSalvagedAlloy(); }
        public void UiLeaveVendor() { if (flow == Flow.Vendor) LeaveVendor(); }
        public void UiUseFieldTonic() { if (flow == Flow.Exploration) UseFieldTonic(); }
        public void UiSave() { SaveGame(); }
        public void UiLoad() { LoadGame(); ApplyInputGate(); }
        public void UiRequestRestart() { if (!restartPending) { restartPending = true; message = "Restart progress? Confirm or cancel below. Existing disk save is retained."; ApplyInputGate(); } }
        public void UiConfirmRestart() { if (restartPending) { NewGame(); restartPending = false; ApplyInputGate(); } }
        public void UiCancelRestart() { if (restartPending) { restartPending = false; message = "Restart cancelled."; ApplyInputGate(); } }

        public bool TryCollectExplorationCurrency(string pickupId, long amount)
        {
            if (!IsExploration || string.IsNullOrWhiteSpace(pickupId) || collectedCurrencyPickupIds.Contains(pickupId) ||
                !wallet.TryEarn(amount, AstralCurrencySource.ExplorationFind))
                return false;

            collectedCurrencyPickupIds.Add(pickupId);
            message = $"Found {amount} Starshards while exploring. Balance: {wallet.Balance}.";
            return true;
        }

        public bool TryCollectExplorationItem(string pickupId, AstralItemId item, int quantity)
        {
            if (!IsExploration || string.IsNullOrWhiteSpace(pickupId) || collectedItemPickupIds.Contains(pickupId) ||
                !inventory.TryAdd(item, quantity))
                return false;

            collectedItemPickupIds.Add(pickupId);
            string itemName = item == AstralItemId.SalvagedAlloy ? "Salvaged Alloy" : "Field Tonic";
            message = $"Exploration find: {quantity} {itemName}{(quantity == 1 ? "" : "s")}.";
            return true;
        }

        private void Awake()
        {
            NewGame();
        }

        private void Start()
        {
            playerController = FindAnyObjectByType<AstralPlayerController>();
            orbitCamera = FindAnyObjectByType<AstralThirdPersonCamera>();
            beacon = FindAnyObjectByType<CrashedBeaconObjective>();
            encounterZones = FindObjectsByType<AstralEncounterZone>();
            currencyPickups = FindObjectsByType<AstralCurrencyPickup>(FindObjectsInactive.Include);
            itemPickups = FindObjectsByType<AstralItemPickup>(FindObjectsInactive.Include);
            vendorStation = FindAnyObjectByType<AstralVendorStation>();
            SetClearedEncounterZones(Array.Empty<string>());
            SetCollectedCurrencyPickups(Array.Empty<string>());
            SetCollectedItemPickups(Array.Empty<string>());
            ApplyInputGate();
        }

        private void OnDisable()
        {
            if (playerController != null) playerController.InputBlocked = false;
            if (orbitCamera != null) orbitCamera.InputBlocked = false;
            if (beacon != null) beacon.InputBlocked = false;
        }

        private void ApplyInputGate()
        {
            if (playerController != null) playerController.InputBlocked = BlocksExplorationInput;
            if (orbitCamera != null) orbitCamera.InputBlocked = BlocksExplorationInput;
            if (beacon != null) beacon.InputBlocked = BlocksExplorationInput;
        }

        private void Update()
        {
            Keyboard keyboard = Keyboard.current;
            if (keyboard == null)
                return;

            if (restartPending)
            {
                if (keyboard.enterKey.wasPressedThisFrame) { NewGame(); restartPending = false; }
                else if (keyboard.escapeKey.wasPressedThisFrame) { restartPending = false; message = "Restart cancelled."; }
                ApplyInputGate();
                return;
            }
            if (keyboard.nKey.wasPressedThisFrame)
            {
                restartPending = true;
                message = "Restart progress? Enter confirms; Escape cancels. Existing disk save is retained.";
                ApplyInputGate();
                return;
            }
            if (keyboard.kKey.wasPressedThisFrame)
                SaveGame();
            if (keyboard.lKey.wasPressedThisFrame)
            {
                LoadGame();
                ApplyInputGate();
                return;
            }

            if (flow == Flow.Exploration && DemoObjectiveComplete && !victoryAcknowledged)
                EnterVictory();

            switch (flow)
            {
                case Flow.Exploration:
                    if (keyboard.bKey.wasPressedThisFrame)
                        TryBeginEncounter();
                    else if (keyboard.vKey.wasPressedThisFrame)
                        OpenVendor();
                    else if (keyboard.tKey.wasPressedThisFrame)
                        UseFieldTonic();
                    else if (keyboard.pKey.wasPressedThisFrame)
                        flow = Flow.PartyManagement;
                    break;
                case Flow.Encounter:
                    if (keyboard.enterKey.wasPressedThisFrame || keyboard.bKey.wasPressedThisFrame)
                        BeginBattle();
                    break;
                case Flow.Battle:
                    UpdateBattleInput(keyboard);
                    break;
                case Flow.Recruitment:
                    if (keyboard.rKey.wasPressedThisFrame)
                        RecruitReward();
                    break;
                case Flow.PartyManagement:
                    if (keyboard.pKey.wasPressedThisFrame)
                        ReorderParty();
                    if (keyboard.enterKey.wasPressedThisFrame || keyboard.eKey.wasPressedThisFrame)
                        ReturnToExploration();
                    break;
                case Flow.Vendor:
                    if (keyboard.digit1Key.wasPressedThisFrame) BuyFieldTonic();
                    else if (keyboard.digit2Key.wasPressedThisFrame) SellSalvagedAlloy();
                    else if (keyboard.enterKey.wasPressedThisFrame || keyboard.escapeKey.wasPressedThisFrame) LeaveVendor();
                    break;
                case Flow.Defeat:
                    if (keyboard.enterKey.wasPressedThisFrame) ReturnToExploration();
                    break;
                case Flow.Victory:
                    if (keyboard.enterKey.wasPressedThisFrame) ContinueAfterVictory();
                    break;
            }
            ApplyInputGate();
        }

        private void UpdateBattleInput(Keyboard keyboard)
        {
            if (keyboard.digit1Key.wasPressedThisFrame) selectedActiveSlot = 0;
            if (keyboard.digit2Key.wasPressedThisFrame) selectedActiveSlot = 1;
            if (keyboard.qKey.wasPressedThisFrame) selectedTargetSlot = 0;
            if (keyboard.wKey.wasPressedThisFrame) selectedTargetSlot = 1;
            if (keyboard.aKey.wasPressedThisFrame)
                ResolvePlayerAction();
            if (keyboard.fKey.wasPressedThisFrame)
                ResolveArcBurst();
            if (keyboard.gKey.wasPressedThisFrame)
                GuardSelectedActive();
            if (keyboard.rKey.wasPressedThisFrame)
                ReplaceFaintedFromReserve();
            if (keyboard.sKey.wasPressedThisFrame)
                SwitchToBench(false);
        }

        private void NewGame()
        {
            party.Clear();
            reserve.Clear();
            AddParty("cindrel", "Cindrel");
            AddParty("mossling", "Mossling");
            AddParty("ripplefin", "Ripplefin");
            AddParty("stormrook", "Stormrook");
            AddParty("ironbur", "Ironbur");
            flow = Flow.Exploration;
            encountersCompleted = 0;
            beaconActivated = false;
            victoryAcknowledged = false;
            wallet.Reset();
            inventory.Reset();
            wayfarerCommission.Reset();
            collectedCurrencyPickupIds.Clear();
            collectedItemPickupIds.Clear();
            activeParty[0] = 0;
            activeParty[1] = 1;
            opponent[0] = opponent[1] = null;
            acted[0] = acted[1] = false;
            guarded[0] = guarded[1] = false;
            battle = null;
            activeEncounterZone = null;
            SetClearedEncounterZones(Array.Empty<string>());
            SetCollectedCurrencyPickups(Array.Empty<string>());
            SetCollectedItemPickups(Array.Empty<string>());
            message = "New game: explore for the marked wild activity. B starts an encounter there; E activates the beacon.";
        }

        private void AddParty(string id, string displayName)
        {
            party.Add(new DemoMember { id = id, displayName = displayName });
        }

        private void TryBeginEncounter()
        {
            if (flow != Flow.Exploration)
                return;

            if (!TryGetEncounterZoneAtPlayer(out AstralEncounterZone encounterZone))
            {
                message = "No wild Astral activity here. Explore to the glowing encounter site, then press B.";
                return;
            }

            activeEncounterZone = encounterZone;
            flow = Flow.Encounter;
            string preview = encounterZone.PrimaryAstralName;
            message = $"{encounterZone.DisplayName}: {preview} and a companion detected. Enter/B begins the 2v2 battle.";
        }

        private bool TryGetEncounterZoneAtPlayer(out AstralEncounterZone encounterZone)
        {
            encounterZone = null;
            if (playerController == null)
                return false;

            Vector3 playerPosition = playerController.transform.position;
            for (int i = 0; i < encounterZones.Length; i++)
            {
                AstralEncounterZone candidate = encounterZones[i];
                if (candidate == null || !candidate.IsAvailable || !candidate.Contains(playerPosition))
                    continue;

                encounterZone = candidate;
                return true;
            }

            return false;
        }

        private bool TryGetVendorAtPlayer()
        {
            return playerController != null && vendorStation != null &&
                   vendorStation.Contains(playerController.transform.position);
        }

        private void OpenVendor()
        {
            if (flow != Flow.Exploration)
                return;

            if (!TryGetVendorAtPlayer())
            {
                message = "No supply relay in range. Find the cyan field terminal, then press V.";
                return;
            }

            flow = Flow.Vendor;
            message = $"{vendorStation.DisplayName}: 1 buys a Field Tonic for {AstralVendorService.FieldTonicPrice} Starshards; " +
                      $"2 sells one Salvaged Alloy for {AstralVendorService.SalvagedAlloySaleValue}.";
        }

        private void BuyFieldTonic()
        {
            if (flow != Flow.Vendor)
                return;

            if (AstralVendorService.TryBuyFieldTonic(wallet, inventory))
                message = $"Purchased one Field Tonic with earned Starshards. Balance: {wallet.Balance}.";
            else
                message = $"Trade declined. A Field Tonic costs {AstralVendorService.FieldTonicPrice} Starshards; no currency or items changed.";
        }

        private void SellSalvagedAlloy()
        {
            if (flow != Flow.Vendor)
                return;

            if (AstralVendorService.TrySellSalvagedAlloy(wallet, inventory))
            {
                wayfarerCommission.TryRecordAlloySale(1);
                bool commissionCompleted = TryCompleteWayfarerCommission();
                message = commissionCompleted
                    ? $"Commission complete. Sold the alloy and earned {AstralWayfarerCommission.CompletionReward} bonus Starshards. Balance: {wallet.Balance}."
                    : $"Sold one Salvaged Alloy for {AstralVendorService.SalvagedAlloySaleValue} Starshards. Balance: {wallet.Balance}.";
            }
            else
                message = "Trade declined. No Salvaged Alloy is available, or the wallet is full; no currency or items changed.";
        }

        private void LeaveVendor()
        {
            if (flow != Flow.Vendor)
                return;

            flow = Flow.Exploration;
            message = "Left the supply relay. T uses a Field Tonic on the most injured conscious party member.";
        }

        private void UseFieldTonic()
        {
            if (flow != Flow.Exploration)
                return;

            DemoMember target = FindTonicTarget();
            if (target == null || !inventory.TryRemove(AstralItemId.FieldTonic, 1))
            {
                message = FieldTonics == 0
                    ? "No Field Tonics available. Earn Starshards through play and visit the supply relay."
                    : "The party has no conscious injured Astral; no tonic was consumed.";
                return;
            }

            int healed = Mathf.Min(12, target.maxHp - target.hp);
            target.hp += healed;
            message = $"{target.displayName} recovered {healed} HP using one Field Tonic.";
        }

        private DemoMember FindTonicTarget()
        {
            DemoMember best = null;
            int mostMissingHp = 0;
            for (int i = 0; i < party.Count; i++)
            {
                DemoMember candidate = party[i];
                int missingHp = candidate.maxHp - candidate.hp;
                if (candidate.defeated || missingHp <= mostMissingHp)
                    continue;

                best = candidate;
                mostMissingHp = missingHp;
            }
            return best;
        }

        private string BuildObjectiveGuidance()
        {
            if (DemoObjectiveComplete)
            {
                if (!victoryAcknowledged)
                    return "Objective complete: two wild sites cleared and the beacon is online.";
                if (!wayfarerCommission.IsComplete)
                    return $"Wayfarer Commission: sell Salvaged Alloy at the relay ({wayfarerCommission.AlloySold}/{AstralWayfarerCommission.RequiredAlloySales}).";
                return $"Wayfarer Commission complete: earned {AstralWayfarerCommission.CompletionReward} Starshards.";
            }

            if (playerController == null)
                return "Objective: explore the wilds.";

            if (encountersCompleted < 2)
            {
                AstralEncounterZone nearest = null;
                float nearestSqrDistance = float.PositiveInfinity;
                Vector3 playerPosition = playerController.transform.position;
                for (int i = 0; i < encounterZones.Length; i++)
                {
                    AstralEncounterZone candidate = encounterZones[i];
                    if (candidate == null || !candidate.IsAvailable)
                        continue;

                    Vector3 delta = candidate.transform.position - playerPosition;
                    delta.y = 0f;
                    if (delta.sqrMagnitude >= nearestSqrDistance)
                        continue;

                    nearest = candidate;
                    nearestSqrDistance = delta.sqrMagnitude;
                }

                if (nearest == null)
                    return "Objective: no uncleared wild activity remains in this area.";

                Vector3 direction = nearest.transform.position - playerPosition;
                direction.y = 0f;
                int remaining = 2 - encountersCompleted;
                string siteWord = remaining == 1 ? "site" : "sites";
                return $"Objective: clear {remaining} wild {siteWord}. {nearest.DisplayName}: {Mathf.Sqrt(nearestSqrDistance):0} m {DescribeDirection(direction)}.";
            }

            if (!BeaconActivated && beacon != null)
            {
                Vector3 direction = beacon.transform.position - playerController.transform.position;
                direction.y = 0f;
                return $"Objective: return to the crashed beacon: {direction.magnitude:0} m {DescribeDirection(direction)}; hold Interact to activate.";
            }

            return "Objective: return to the crashed beacon and activate it.";
        }

        internal static string DescribeDirection(Vector3 delta)
        {
            delta.y = 0f;
            if (delta.sqrMagnitude < 0.25f)
                return "here";

            float angle = Mathf.Atan2(delta.x, delta.z) * Mathf.Rad2Deg;
            int index = Mathf.RoundToInt(angle / 45f);
            if (index < 0)
                index += DirectionNames.Length;
            return DirectionNames[index % DirectionNames.Length];
        }

        private void BeginBattle()
        {
            if (flow != Flow.Encounter)
                return;
            if (activeEncounterZone == null)
            {
                flow = Flow.Exploration;
                message = "The wild activity faded. Explore for another encounter site.";
                return;
            }

            opponent[0] = new DemoMember { id = activeEncounterZone.PrimaryAstralId, displayName = activeEncounterZone.PrimaryAstralName };
            opponent[1] = new DemoMember { id = activeEncounterZone.CompanionAstralId, displayName = activeEncounterZone.CompanionAstralName };
            activeParty[0] = FindFirstEligibleParty(0);
            activeParty[1] = FindFirstEligibleParty(activeParty[0] + 1);
            if (activeParty[0] < 0) { flow = Flow.Defeat; message = "No healthy Astrals. Enter to recover."; return; }
            activeOpponent[0] = 0;
            activeOpponent[1] = 1;
            selectedActiveSlot = 0;
            selectedTargetSlot = 0;
            flow = Flow.Battle;
            var playerParty = new AstralParty(AstralParty.PlayerCapacity);
            foreach (var member in party)
            {
                var combatant = new AstralCombatant(member.id, member.displayName);
                if (member.defeated) combatant.MarkDefeated();
                playerParty.TryAdd(combatant);
            }
            var enemyParty = new AstralParty(AstralParty.OpponentCapacity);
            foreach (var member in opponent) enemyParty.TryAdd(new AstralCombatant(member.id, member.displayName));
            battle = new AstralBattleState(playerParty, enemyParty);
            for (int slot = 0; slot < 2; slot++)
            {
                battle.TryActivate(BattleSide.Player, activeParty[slot], slot);
                battle.TryActivate(BattleSide.Opponent, slot, slot);
            }
            acted[0] = acted[1] = false;
            guarded[0] = guarded[1] = false;
            message = "2v2 battle: 1/2 selects your active slot; Q/W target; A attacks; F bursts both opponents.";
            message += $" {activeEncounterZone.TacticalBrief} G guards the selected slot.";
        }

        private void ResolvePlayerAction()
        {
            DemoMember actor = GetActiveParty(selectedActiveSlot);
            DemoMember target = opponent[selectedTargetSlot];
            if (flow != Flow.Battle || actor == null || actor.defeated || target == null || target.defeated)
            {
                message = "Invalid action: choose an eligible active Astral and a living target.";
                return;
            }
            if (acted[selectedActiveSlot] || !battle.TryQueueAction(BattleSide.Player, selectedActiveSlot,
                new QueuedAstralAction("attack", TargetScope.OneEnemy, selectedTargetSlot)))
            { message = "That Astral already acted this round. Choose the other active slot."; return; }
            acted[selectedActiveSlot] = true;

            DamageOpponent(selectedTargetSlot, AstralCombatRules.BasicAttackDamage);
            if (AllOpponentsDefeated())
            {
                CompleteBattle(true);
                return;
            }

            FinishRoundIfReady();
        }

        private void ResolveArcBurst()
        {
            DemoMember actor = GetActiveParty(selectedActiveSlot);
            if (flow != Flow.Battle || actor == null || actor.defeated)
            {
                message = "Invalid Arc Burst: choose a conscious active Astral.";
                return;
            }
            if (acted[selectedActiveSlot] || !battle.TryQueueAction(BattleSide.Player, selectedActiveSlot,
                new QueuedAstralAction("arc-burst", TargetScope.BothEnemies)))
            {
                message = "That Astral already acted this round. Choose the other active slot.";
                return;
            }

            acted[selectedActiveSlot] = true;
            int targetsHit = 0;
            for (int slot = 0; slot < opponent.Length; slot++)
            {
                if (opponent[slot] == null || opponent[slot].defeated)
                    continue;
                DamageOpponent(slot, AstralCombatRules.ArcBurstDamagePerTarget);
                targetsHit++;
            }

            if (AllOpponentsDefeated())
            {
                CompleteBattle(true);
                return;
            }

            message = $"{actor.displayName}'s Arc Burst dealt {AstralCombatRules.ArcBurstDamagePerTarget} damage to {targetsHit} opponent{(targetsHit == 1 ? "" : "s")}.";
            FinishRoundIfReady();
        }

        private void DamageOpponent(int slot, int damage)
        {
            DemoMember target = opponent[slot];
            if (target == null || target.defeated || damage <= 0)
                return;

            target.hp = Mathf.Max(0, target.hp - damage);
            if (target.hp == 0)
            {
                target.defeated = true;
                battle.OpponentParty.Astrals[slot].MarkDefeated();
            }
        }

        private void GuardSelectedActive()
        {
            DemoMember actor = GetActiveParty(selectedActiveSlot);
            if (flow != Flow.Battle || actor == null || actor.defeated)
            {
                message = "Invalid guard: choose a conscious active Astral.";
                return;
            }
            if (acted[selectedActiveSlot] || !battle.TryQueueAction(BattleSide.Player, selectedActiveSlot,
                new QueuedAstralAction("guard", TargetScope.Self, selectedActiveSlot)))
            {
                message = "That Astral already acted this round. Choose the other active slot.";
                return;
            }

            acted[selectedActiveSlot] = true;
            guarded[selectedActiveSlot] = true;
            message = $"{actor.displayName} guards its position against the next counterattack.";
            FinishRoundIfReady();
        }

        private void FinishRoundIfReady()
        {
            for (int slot = 0; slot < 2; slot++)
            {
                var member = GetActiveParty(slot);
                if (member != null && !member.defeated && !acted[slot])
                { selectedActiveSlot = slot; message = "Choose an action for the remaining active Astral."; return; }
            }
            string roundResult = ResolveOpponentActions();
            acted[0] = acted[1] = false;
            battle.ClearQueuedActions();
            if (AllPartyDefeated()) CompleteBattle(false);
            else message = $"Round resolved: {roundResult} A attacks, F bursts both, G guards, S swaps.";
        }

        private string ResolveOpponentActions()
        {
            var results = new List<string>(2);
            for (int i = 0; i < activeOpponent.Length; i++)
            {
                DemoMember enemy = opponent[activeOpponent[i]];
                int targetSlot = i % 2;
                DemoMember target = GetActiveParty(targetSlot);
                if (target == null || target.defeated)
                {
                    targetSlot = (i + 1) % 2;
                    target = GetActiveParty(targetSlot);
                }
                if (enemy == null || enemy.defeated || target == null || target.defeated)
                    continue;
                int baseDamage = activeEncounterZone != null ? activeEncounterZone.OpponentDamage : 6;
                bool wasGuarded = guarded[targetSlot];
                int damage = AstralCombatRules.ResolveIncomingDamage(baseDamage, wasGuarded);
                target.hp = Mathf.Max(0, target.hp - damage);
                if (target.hp == 0)
                {
                    target.defeated = true;
                    battle.PlayerParty.Astrals[party.IndexOf(target)].MarkDefeated();
                }
                results.Add($"{target.displayName}{(wasGuarded ? " guarded and" : "")} took {damage}{(target.defeated ? " and fainted" : "")}");
            }
            guarded[0] = guarded[1] = false;
            return results.Count == 0 ? "no counterattacks landed." : string.Join("; ", results) + ".";
        }

        private void ReplaceFaintedFromReserve()
        {
            SwitchToBench(true);
        }

        private void SwitchToBench(bool requireFainted)
        {
            if (flow != Flow.Battle) return;
            DemoMember current = GetActiveParty(selectedActiveSlot);
            if (current == null || (requireFainted && !current.defeated))
            {
                message = "Replacement is available only for a fainted active slot.";
                return;
            }
            if (!current.defeated && acted[selectedActiveSlot])
            { message = "This slot already acted this round."; return; }
            for (int i = 0; i < party.Count; i++)
            {
                if (party[i].defeated || i == activeParty[0] || i == activeParty[1])
                    continue;
                if (!battle.TrySwitch(BattleSide.Player, selectedActiveSlot, i)) continue;
                bool voluntary = !current.defeated;
                activeParty[selectedActiveSlot] = i;
                message = "Healthy bench Astral deployed. Party size unchanged.";
                if (voluntary) { acted[selectedActiveSlot] = true; FinishRoundIfReady(); }
                return;
            }
            message = "No healthy inactive party members remain. Storage is unavailable in battle.";
        }

        private void ForceSelectedFaintForPrototypeTesting()
        {
            DemoMember current = GetActiveParty(selectedActiveSlot);
            if (current != null)
                current.hp = 0;
            if (current != null)
                current.defeated = true;
            message = "Prototype test: selected active Astral fainted. R: replace from reserve.";
        }

        private void CompleteBattle(bool playerWon)
        {
            if (flow != Flow.Battle)
                return;
            guarded[0] = guarded[1] = false;
            flow = playerWon ? Flow.Recruitment : Flow.Defeat;
            if (playerWon)
            {
                int reward = activeEncounterZone != null ? activeEncounterZone.ClearReward : 0;
                if (reward > 0)
                    wallet.TryEarn(reward, AstralCurrencySource.EncounterClear);
                bool gainedAlloy = inventory.TryAdd(AstralItemId.SalvagedAlloy, 1);
                activeEncounterZone?.SetCleared(true);
                encountersCompleted++;
                message = reward > 0
                    ? $"Victory. Earned {reward} Starshards{(gainedAlloy ? " and 1 Salvaged Alloy" : "")}. R: recruit exactly one Astral reward."
                    : $"Victory{(gainedAlloy ? ". Recovered 1 Salvaged Alloy" : "")}. R: recruit exactly one Astral reward.";
            }
            else
            {
                message = "Defeat. Enter: return to exploration; no reward was granted.";
            }
        }

        private void RecruitReward()
        {
            if (flow != Flow.Recruitment)
                return;
            string id = "recruited-" + encountersCompleted;
            for (int i = 0; i < party.Count; i++)
                if (party[i].id == id)
                    return;
            for (int i = 0; i < reserve.Count; i++)
                if (reserve[i].id == id)
                    return;
            DemoMember reward = new DemoMember { id = id, displayName = "Recruited Astral " + encountersCompleted };
            if (party.Count < AstralCampaignState.PartyCapacity)
            {
                party.Add(reward);
                message = "Astral recruited to party. P: party management, Enter: exploration.";
            }
            else
            {
                reserve.Add(reward);
                message = "Party full: Astral safely stored in reserve. P: party management, Enter: exploration.";
            }
            flow = Flow.PartyManagement;
        }

        private void ReorderParty()
        {
            if (party.Count > 1)
            {
                DemoMember first = party[0];
                party[0] = party[1];
                party[1] = first;
                activeParty[0] = 0;
                activeParty[1] = Mathf.Min(1, party.Count - 1);
            }
            message = "Party reordered; the next encounter uses the current active pair. Enter: exploration.";
        }

        private void ReturnToExploration()
        {
            if (flow == Flow.Defeat)
            {
                foreach (var member in party) { member.hp = member.maxHp; member.defeated = false; }
                message = "Party recovered. Return to the wild activity site for another encounter.";
            }
            else
            message = "Returned to exploration. Visit the wild activity site for another encounter; E: beacon; K: save.";
            flow = Flow.Exploration;
            battle = null;
            activeEncounterZone = null;
        }

        private void EnterVictory()
        {
            flow = Flow.Victory;
            message = "Expedition complete: two wild sites cleared and the crashed beacon restored.";
        }

        private void ContinueAfterVictory()
        {
            victoryAcknowledged = true;
            flow = Flow.Exploration;
            bool commissionCompleted = TryCompleteWayfarerCommission();
            message = commissionCompleted
                ? $"Expedition and Wayfarer Commission complete. Earned {AstralWayfarerCommission.CompletionReward} bonus Starshards."
                : "Expedition complete. Wayfarer Commission available: sell two Salvaged Alloy at the relay.";
            ApplyInputGate();
        }

        private bool TryCompleteWayfarerCommission()
        {
            return wayfarerCommission.TryComplete(DemoObjectiveComplete && victoryAcknowledged, wallet);
        }

        private void SaveGame()
        {
            if (flow != Flow.Exploration && flow != Flow.PartyManagement && flow != Flow.Vendor && flow != Flow.Victory)
            { message = "Save between battles, after recruitment is resolved."; return; }
            try
            {
                SaveData data = new SaveData
                {
                    flow = Flow.Exploration,
                    beaconActivated = beacon != null ? beacon.IsActivated : beaconActivated,
                    encountersCompleted = encountersCompleted,
                    victoryAcknowledged = victoryAcknowledged,
                    economy = AstralEconomySaveData.Capture(wallet, inventory),
                    alloySold = wayfarerCommission.AlloySold,
                    wayfarerCommissionComplete = wayfarerCommission.IsComplete
                };
                data.party.AddRange(party);
                data.reserve.AddRange(reserve);
                for (int i = 0; i < encounterZones.Length; i++)
                    if (encounterZones[i] != null && encounterZones[i].IsCleared)
                        data.clearedEncounterZoneIds.Add(encounterZones[i].ZoneId);
                data.collectedCurrencyPickupIds.AddRange(collectedCurrencyPickupIds);
                data.collectedItemPickupIds.AddRange(collectedItemPickupIds);
                string path = Path.Combine(Application.persistentDataPath, SaveFileName);
                string temp = path + ".tmp";
                File.WriteAllText(temp, JsonUtility.ToJson(data, true));
                if (File.Exists(path)) File.Replace(temp, path, path + ".bak");
                else File.Move(temp, path);
                message = "Game saved.";
            }
            catch (Exception exception)
            {
                Debug.LogError("Astral save failed: " + exception.Message, this);
            }
        }

        private void LoadGame()
        {
            try
            {
                string path = Path.Combine(Application.persistentDataPath, SaveFileName);
                if (!File.Exists(path)) { message = "No save found; new game retained."; return; }
                SaveData data = JsonUtility.FromJson<SaveData>(File.ReadAllText(path));
                if (!ValidateSave(data)) { message = "Save invalid; current progress retained."; return; }
                party.Clear(); reserve.Clear();
                if (data.party != null) party.AddRange(data.party);
                if (data.reserve != null) reserve.AddRange(data.reserve);
                // Old v1 files can contain a battle mode without any battle state.
                // Restore these as safe exploration checkpoints instead of inventing opponents.
                flow = Flow.Exploration; beaconActivated = data.beaconActivated; encountersCompleted = data.encountersCompleted; victoryAcknowledged = data.victoryAcknowledged;
                AstralEconomySaveData economyState = data.version >= 4
                    ? data.economy
                    : new AstralEconomySaveData { starshards = data.starshards };
                economyState.TryRestore(wallet, inventory);
                wayfarerCommission.TryRestore(data.version >= 6 ? data.alloySold : 0,
                    data.version >= 6 && data.wayfarerCommissionComplete);
                collectedCurrencyPickupIds.Clear();
                if (data.collectedCurrencyPickupIds != null)
                    collectedCurrencyPickupIds.UnionWith(data.collectedCurrencyPickupIds);
                collectedItemPickupIds.Clear();
                if (data.collectedItemPickupIds != null)
                    collectedItemPickupIds.UnionWith(data.collectedItemPickupIds);
                if (AllPartyDefeated()) foreach (var member in party) { member.hp = member.maxHp; member.defeated = false; }
                activeParty[0] = FindFirstEligibleParty(0);
                activeParty[1] = FindFirstEligibleParty(activeParty[0] + 1);
                opponent[0] = opponent[1] = null;
                acted[0] = acted[1] = false;
                guarded[0] = guarded[1] = false;
                battle = null;
                activeEncounterZone = null;
                IReadOnlyCollection<string> clearedZoneIds = data.clearedEncounterZoneIds;
                if (clearedZoneIds == null)
                    clearedZoneIds = Array.Empty<string>();
                SetClearedEncounterZones(clearedZoneIds);
                SetCollectedCurrencyPickups(collectedCurrencyPickupIds);
                SetCollectedItemPickups(collectedItemPickupIds);
                if (beacon != null) beacon.RestoreProgress(beaconActivated);
                message = "Checkpoint loaded. Explore to the wild activity site to begin an encounter.";
            }
            catch (Exception exception)
            {
                Debug.LogError("Astral load failed: " + exception.Message, this);
                message = "Save could not be loaded; current progress retained.";
            }
        }

        private static bool ValidateSave(SaveData data)
        {
            if (data == null || data.version < 1 || data.version > 6 || data.party == null || data.reserve == null ||
                data.party.Count < 1 || data.party.Count > AstralParty.PlayerCapacity || data.encountersCompleted < 0 ||
                (data.version < 4 && data.starshards < 0) || (data.version >= 4 && (data.economy == null || !data.economy.IsValid)) ||
                (data.version >= 6 && (data.alloySold < 0 ||
                    (data.wayfarerCommissionComplete && (data.alloySold < AstralWayfarerCommission.RequiredAlloySales ||
                        !data.victoryAcknowledged || data.encountersCompleted < 2 || !data.beaconActivated)))))
                return false;
            var ids = new HashSet<string>(StringComparer.Ordinal);
            foreach (var list in new[] { data.party, data.reserve })
                foreach (var member in list)
                    if (member == null || string.IsNullOrWhiteSpace(member.id) || !ids.Add(member.id) ||
                        string.IsNullOrWhiteSpace(member.displayName) || member.maxHp <= 0 ||
                        member.hp < 0 || member.hp > member.maxHp || member.defeated != (member.hp == 0)) return false;
            if (data.clearedEncounterZoneIds != null)
            {
                var zoneIds = new HashSet<string>(StringComparer.Ordinal);
                foreach (string zoneId in data.clearedEncounterZoneIds)
                    if (string.IsNullOrWhiteSpace(zoneId) || !zoneIds.Add(zoneId))
                        return false;
            }
            if (data.collectedCurrencyPickupIds != null)
            {
                var pickupIds = new HashSet<string>(StringComparer.Ordinal);
                foreach (string pickupId in data.collectedCurrencyPickupIds)
                    if (string.IsNullOrWhiteSpace(pickupId) || !pickupIds.Add(pickupId))
                        return false;
            }
            if (data.collectedItemPickupIds != null)
            {
                var pickupIds = new HashSet<string>(StringComparer.Ordinal);
                foreach (string pickupId in data.collectedItemPickupIds)
                    if (string.IsNullOrWhiteSpace(pickupId) || !pickupIds.Add(pickupId))
                        return false;
            }
            return true;
        }

        private void SetClearedEncounterZones(IReadOnlyCollection<string> clearedZoneIds)
        {
            var cleared = new HashSet<string>(clearedZoneIds ?? Array.Empty<string>(), StringComparer.Ordinal);
            for (int i = 0; i < encounterZones.Length; i++)
                if (encounterZones[i] != null)
                    encounterZones[i].SetCleared(cleared.Contains(encounterZones[i].ZoneId));
        }

        private void SetCollectedCurrencyPickups(IEnumerable<string> collectedIds)
        {
            var collected = new HashSet<string>(collectedIds ?? Array.Empty<string>(), StringComparer.Ordinal);
            for (int i = 0; i < currencyPickups.Length; i++)
                if (currencyPickups[i] != null)
                    currencyPickups[i].SetCollected(collected.Contains(currencyPickups[i].PickupId));
        }

        private void SetCollectedItemPickups(IEnumerable<string> collectedIds)
        {
            var collected = new HashSet<string>(collectedIds ?? Array.Empty<string>(), StringComparer.Ordinal);
            for (int i = 0; i < itemPickups.Length; i++)
                if (itemPickups[i] != null)
                    itemPickups[i].SetCollected(collected.Contains(itemPickups[i].PickupId));
        }

        private DemoMember GetActiveParty(int slot)
        {
            if (slot < 0 || slot >= activeParty.Length || activeParty[slot] < 0 || activeParty[slot] >= party.Count)
                return null;
            return party[activeParty[slot]];
        }

        private int FindFirstEligibleParty(int start)
        {
            for (int i = Mathf.Max(0, start); i < party.Count; i++)
                if (!party[i].defeated)
                    return i;
            return -1;
        }

        private bool AllPartyDefeated()
        {
            for (int i = 0; i < party.Count; i++) if (!party[i].defeated) return false;
            return true;
        }

        private bool AllOpponentsDefeated()
        {
            return opponent[0] == null || (opponent[0].defeated && opponent[1].defeated);
        }

#if UNITY_EDITOR || DEBUG
        private void OnGUI()
        {
            GUI.Box(new Rect(18, 18, 480, flow == Flow.Battle ? 430 : 230), "ASTRAL WILDS PROTOTYPE");
            GUILayout.BeginArea(new Rect(34, 48, 448, flow == Flow.Battle ? 390 : 190));
            GUILayout.Label("State: " + flow + "   Party: " + party.Count + "/6   Reserve: " + reserve.Count);
            GUILayout.Label(message);
            GUILayout.Space(8);
            for (int i = 0; i < party.Count; i++)
                GUILayout.Label((i == activeParty[0] || i == activeParty[1] ? "ACTIVE " : "      ") + party[i].displayName + "  HP " + party[i].hp + "/" + party[i].maxHp + (party[i].defeated ? " FAINTED" : ""));
            if (flow == Flow.Battle)
            {
                GUILayout.Space(8);
                GUILayout.Label("Opponent slot 1: " + opponent[0].displayName + " HP " + opponent[0].hp + (opponent[0].defeated ? " FAINTED" : ""));
                GUILayout.Label("Opponent slot 2: " + opponent[1].displayName + " HP " + opponent[1].hp + (opponent[1].defeated ? " FAINTED" : ""));
                GUILayout.Label("Selected active slot: " + (selectedActiveSlot + 1) + "  target: " + (selectedTargetSlot + 1));
                GUILayout.Label("A attack | Q/W target | 1/2 active slot | R replace | S swap bench");
            }
            GUILayout.Label("K save | L load | N new game");
            GUILayout.EndArea();
        }
#endif
    }
}
