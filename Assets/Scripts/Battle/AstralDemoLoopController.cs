using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

namespace AstralWilds
{
    /// <summary>
    /// Minimal playable prototype loop for the demo scene. The combat rules are
    /// intentionally simple and editable; this is not the final combat model.
    /// Battle-round mechanics (queuing actions, applying damage, resolving
    /// counterattacks) live in <see cref="AstralBattleEngine"/> so they are
    /// unit-testable without a live scene; this controller owns shell/flow state,
    /// messages, save data, and economy/progression side effects.
    /// </summary>
    [DefaultExecutionOrder(-100)]
    public sealed class AstralDemoLoopController : MonoBehaviour
    {
        private enum Flow { Exploration, Encounter, Battle, Recruitment, PartyManagement, Vendor, Victory, Defeat }
        private enum ShellState { Gameplay, Title, Paused, SettingsFromTitle, SettingsFromPause }

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
            public List<AstralDemoMember> party = new List<AstralDemoMember>();
            public List<AstralDemoMember> reserve = new List<AstralDemoMember>();
            public List<string> clearedEncounterZoneIds = new List<string>();
            public List<string> collectedCurrencyPickupIds = new List<string>();
            public List<string> collectedItemPickupIds = new List<string>();
        }

        private const string SaveFileName = "astralwilds-demo-save-v1.json";
        private static readonly string[] DirectionNames = { "north", "northeast", "east", "southeast", "south", "southwest", "west", "northwest" };
        private readonly List<AstralDemoMember> party = new List<AstralDemoMember>();
        private readonly List<AstralDemoMember> reserve = new List<AstralDemoMember>();
        private readonly int[] activeParty = { 0, 1 };
        private Flow flow = Flow.Exploration;
        private ShellState shellState = ShellState.Gameplay;
        private int selectedActiveSlot;
        private int selectedTargetSlot;
        private int encountersCompleted;
        private bool beaconActivated;
        private bool restartPending;
        private bool victoryAcknowledged;
        private AstralBattleEngine battleEngine;
        private readonly AstralWallet wallet = new AstralWallet();
        private readonly AstralInventory inventory = new AstralInventory();
        private readonly AstralWayfarerCommission wayfarerCommission = new AstralWayfarerCommission();
        private readonly AstralGameSettings gameSettings = new AstralGameSettings();
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
        private AstralCommandInput commandInput;
        private AstralFeedbackAudio feedbackAudio;
        private bool hasSaveGame;
        private string message = "Explore the wilds and find the marked Astral activity. E remains reserved for the beacon objective.";

        public bool InBattle => shellState == ShellState.Gameplay && flow == Flow.Battle;
        public int PartyCount => party.Count;
        public int ReserveCount => reserve.Count;
        public string CurrentState => shellState switch
        {
            ShellState.Title => "Title",
            ShellState.Paused => "Paused",
            ShellState.SettingsFromTitle or ShellState.SettingsFromPause => "Settings",
            _ => flow.ToString()
        };
        public string StatusMessage => message;
        public bool BlocksExplorationInput => shellState != ShellState.Gameplay || flow != Flow.Exploration || restartPending;

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

        public bool IsTitleScreen => shellState == ShellState.Title;
        public bool IsPaused => shellState == ShellState.Paused;
        public bool IsSettings => shellState == ShellState.SettingsFromTitle || shellState == ShellState.SettingsFromPause;
        public bool IsExploration => shellState == ShellState.Gameplay && flow == Flow.Exploration && !restartPending;
        public bool IsEncounter => shellState == ShellState.Gameplay && flow == Flow.Encounter;
        public bool IsRecruitment => shellState == ShellState.Gameplay && flow == Flow.Recruitment;
        public bool IsPartyManagement => shellState == ShellState.Gameplay && flow == Flow.PartyManagement;
        public bool IsVendor => shellState == ShellState.Gameplay && flow == Flow.Vendor;
        public bool IsDefeat => shellState == ShellState.Gameplay && flow == Flow.Defeat;
        public bool IsVictory => shellState == ShellState.Gameplay && flow == Flow.Victory;
        public bool IsRestartPending => restartPending;
        public bool HasSaveGame => hasSaveGame;
        public bool CanSaveNow => (shellState == ShellState.Gameplay || shellState == ShellState.Paused) &&
                                  (flow == Flow.Exploration || flow == Flow.PartyManagement || flow == Flow.Vendor || flow == Flow.Victory);
        public int MasterVolumePercent => gameSettings.VolumePercent;
        public int LookSensitivityPercent => gameSettings.LookSensitivityPercent;
        public string InputDeviceLabel => commandInput?.PromptDeviceLabel ?? "Keyboard & Mouse";
        public bool IsRebinding => commandInput != null && commandInput.IsRebinding;
        public string RebindStatus => commandInput?.RebindStatus ?? "Input bindings unavailable.";
        public int InputPresentationVersion => commandInput?.PresentationVersion ?? 0;
        public bool CanBeginEncounter => IsExploration && TryGetEncounterZoneAtPlayer(out _);
        public string EncounterActionLabel => CanBeginEncounter ? $"Encounter ({Prompt(AstralCommand.Encounter)})" : "Find wild activity";
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

        public string Prompt(AstralCommand command) => commandInput?.GetPrompt(command) ?? "Unbound";

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
            if (flow != Flow.Battle || battleEngine == null)
                return list;
            for (int i = 0; i < 2; i++)
                list.Add(ToUiInfo(battleEngine.GetOpponent(i), true, i));
            return list;
        }

        private static AstralUiInfo ToUiInfo(AstralDemoMember member, bool isActive, int activeSlot)
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

        public void UiStartNewExpedition()
        {
            if (!IsTitleScreen) return;
            NewGame();
            EnterGameplay("New expedition begun. Explore for the marked wild activity.");
            PlayFeedback(AstralFeedbackCue.Confirm);
        }
        public void UiContinueGame()
        {
            if (!IsTitleScreen || !hasSaveGame) return;
            if (LoadGame())
            {
                EnterGameplay("Checkpoint loaded. The expedition continues.");
            }
        }
        public void UiPause()
        {
            if (shellState != ShellState.Gameplay || restartPending) return;
            shellState = ShellState.Paused;
            Time.timeScale = 0f;
            message = "Expedition paused.";
            ApplyInputGate();
            PlayFeedback(AstralFeedbackCue.Confirm);
        }
        public void UiResume()
        {
            if (!IsPaused) return;
            EnterGameplay("Expedition resumed.");
            PlayFeedback(AstralFeedbackCue.Confirm);
        }
        public void UiOpenSettings()
        {
            if (IsTitleScreen) shellState = ShellState.SettingsFromTitle;
            else if (IsPaused) shellState = ShellState.SettingsFromPause;
            else return;
            message = "Settings are saved automatically.";
            ApplyInputGate();
            PlayFeedback(AstralFeedbackCue.Confirm);
        }
        public void UiCloseSettings()
        {
            if (shellState == ShellState.SettingsFromTitle) shellState = ShellState.Title;
            else if (shellState == ShellState.SettingsFromPause) shellState = ShellState.Paused;
            else return;
            message = shellState == ShellState.Title ? "Choose an expedition." : "Expedition paused.";
            ApplyInputGate();
            PlayFeedback(AstralFeedbackCue.Cancel);
        }
        public void UiCycleMasterVolume()
        {
            if (!IsSettings) return;
            gameSettings.CycleVolume();
            SaveAndApplySettings();
            PlayFeedback(AstralFeedbackCue.Confirm);
        }
        public void UiCycleLookSensitivity()
        {
            if (!IsSettings) return;
            gameSettings.CycleLookSensitivity();
            SaveAndApplySettings();
            PlayFeedback(AstralFeedbackCue.Confirm);
        }
        public void UiReturnToTitle()
        {
            if (!IsPaused && shellState != ShellState.SettingsFromPause) return;
            shellState = ShellState.Title;
            Time.timeScale = 0f;
            message = "Returned to title. Current progress remains in memory; save before leaving to keep it on disk.";
            ApplyInputGate();
            PlayFeedback(AstralFeedbackCue.Cancel);
        }
        public void UiSelectActiveSlot(int slot) { if (InBattle && slot >= 0 && slot < 2) { selectedActiveSlot = slot; PlayFeedback(AstralFeedbackCue.Confirm); } }
        public void UiSelectTargetSlot(int slot) { if (InBattle && slot >= 0 && slot < 2) { selectedTargetSlot = slot; PlayFeedback(AstralFeedbackCue.Confirm); } }
        public void UiAttack() { if (InBattle) ResolvePlayerAction(); }
        public void UiArcBurst() { if (InBattle) ResolveArcBurst(); }
        public void UiGuard() { if (InBattle) GuardSelectedActive(); }
        public void UiReplaceFainted() { if (InBattle) ReplaceFaintedFromReserve(); }
        public void UiSwapBench() { if (InBattle) SwitchToBench(false); }
        public void UiBeginEncounter() { if (IsExploration) TryBeginEncounter(); }
        public void UiBeginBattle() { if (IsEncounter) BeginBattle(); }
        public void UiRecruit() { if (IsRecruitment) RecruitReward(); }
        public void UiOpenPartyManagement() { if (IsExploration) flow = Flow.PartyManagement; }
        public void UiReorderParty() { if (IsPartyManagement) ReorderParty(); }
        public void UiConfirmDefeatRecovery() { if (IsDefeat) ReturnToExploration(); }
        public void UiReturnToExploration() { if (IsPartyManagement) ReturnToExploration(); }
        public void UiContinueAfterVictory() { if (IsVictory) ContinueAfterVictory(); }
        public void UiOpenVendor() { if (CanUseVendor) OpenVendor(); }
        public void UiBuyFieldTonic() { if (IsVendor) BuyFieldTonic(); }
        public void UiSellSalvagedAlloy() { if (IsVendor) SellSalvagedAlloy(); }
        public void UiLeaveVendor() { if (IsVendor) LeaveVendor(); }
        public void UiUseFieldTonic() { if (IsExploration) UseFieldTonic(); }
        public void UiSave() { SaveGame(); }
        public void UiLoad() { if (shellState == ShellState.Gameplay) LoadGame(); ApplyInputGate(); }
        public void UiRequestRestart() { if (shellState == ShellState.Gameplay && !restartPending) { restartPending = true; message = "Restart progress? Confirm or cancel below. Existing disk save is retained."; ApplyInputGate(); PlayFeedback(AstralFeedbackCue.Confirm); } }
        public void UiConfirmRestart() { if (restartPending) { NewGame(); restartPending = false; ApplyInputGate(); PlayFeedback(AstralFeedbackCue.Confirm); } }
        public void UiCancelRestart() { if (restartPending) { restartPending = false; message = "Restart cancelled."; ApplyInputGate(); PlayFeedback(AstralFeedbackCue.Cancel); } }
        public void UiBeginRebind(AstralCommand command)
        {
            if (!IsSettings || commandInput == null)
                return;
            if (commandInput.BeginKeyboardRebind(command))
                PlayFeedback(AstralFeedbackCue.Confirm);
        }
        public void UiCancelRebind() => commandInput?.CancelRebind();
        public void UiResetBindings()
        {
            if (!IsSettings || commandInput == null)
                return;
            commandInput.ResetKeyboardBindings();
            PlayFeedback(AstralFeedbackCue.Confirm);
        }

        public bool TryCollectExplorationCurrency(string pickupId, long amount)
        {
            if (!IsExploration || string.IsNullOrWhiteSpace(pickupId) || collectedCurrencyPickupIds.Contains(pickupId) ||
                !wallet.TryEarn(amount, AstralCurrencySource.ExplorationFind))
                return false;

            collectedCurrencyPickupIds.Add(pickupId);
            message = $"Found {amount} Starshards while exploring. Balance: {wallet.Balance}.";
            PlayFeedback(AstralFeedbackCue.Pickup);
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
            PlayFeedback(AstralFeedbackCue.Pickup);
            return true;
        }

        private void Awake()
        {
            commandInput = new AstralCommandInput();
            feedbackAudio = GetComponent<AstralFeedbackAudio>();
            if (feedbackAudio == null)
                feedbackAudio = gameObject.AddComponent<AstralFeedbackAudio>();
            NewGame();
        }

        private void OnEnable()
        {
            commandInput?.Enable();
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
            AstralGameSettingsStore.LoadInto(gameSettings);
            SaveAndApplySettings(false);
            hasSaveGame = File.Exists(Path.Combine(Application.persistentDataPath, SaveFileName));
            SetClearedEncounterZones(Array.Empty<string>());
            SetCollectedCurrencyPickups(Array.Empty<string>());
            SetCollectedItemPickups(Array.Empty<string>());
            shellState = ShellState.Title;
            Time.timeScale = 0f;
            message = "Choose New Expedition or Continue. No real-money purchases exist in Astral Wilds.";
            ApplyInputGate();
        }

        private void OnDisable()
        {
            commandInput?.Disable();
            if (playerController != null) playerController.InputBlocked = false;
            if (orbitCamera != null) orbitCamera.InputBlocked = false;
            if (beacon != null) beacon.InputBlocked = false;
            Time.timeScale = 1f;
        }

        private void OnDestroy()
        {
            commandInput?.Dispose();
        }

        private void ApplyInputGate()
        {
            if (playerController != null) playerController.InputBlocked = BlocksExplorationInput;
            if (orbitCamera != null) orbitCamera.InputBlocked = BlocksExplorationInput;
            if (beacon != null) beacon.InputBlocked = BlocksExplorationInput;
        }

        private void EnterGameplay(string statusMessage)
        {
            shellState = ShellState.Gameplay;
            Time.timeScale = 1f;
            message = statusMessage;
            ApplyInputGate();
        }

        private void SaveAndApplySettings(bool persist = true)
        {
            AudioListener.volume = gameSettings.Volume;
            if (orbitCamera != null)
                orbitCamera.SensitivityScale = gameSettings.LookSensitivityScale;
            if (persist)
                AstralGameSettingsStore.Save(gameSettings);
            message = $"Settings saved: volume {gameSettings.VolumePercent}%, look sensitivity {gameSettings.LookSensitivityPercent}%.";
        }

        private void PlayFeedback(AstralFeedbackCue cue) => feedbackAudio?.Play(cue);

        private void Update()
        {
            if (commandInput == null)
                return;
            commandInput.PollPromptDevice();
            if (commandInput.IsRebinding)
            {
                ApplyInputGate();
                return;
            }

            if (IsTitleScreen)
            {
                if (Pressed(AstralCommand.Confirm)) UiStartNewExpedition();
                else if (Pressed(AstralCommand.Load)) UiContinueGame();
                else if (Pressed(AstralCommand.OpenSettings)) UiOpenSettings();
                ApplyInputGate();
                return;
            }
            if (IsSettings)
            {
                if (Pressed(AstralCommand.Cancel) || Pressed(AstralCommand.OpenSettings)) UiCloseSettings();
                ApplyInputGate();
                return;
            }
            if (IsPaused)
            {
                if (Pressed(AstralCommand.Cancel)) UiResume();
                else if (Pressed(AstralCommand.OpenSettings)) UiOpenSettings();
                ApplyInputGate();
                return;
            }

            if (restartPending)
            {
                if (Pressed(AstralCommand.Confirm)) { NewGame(); restartPending = false; PlayFeedback(AstralFeedbackCue.Confirm); }
                else if (Pressed(AstralCommand.Cancel)) { restartPending = false; message = "Restart cancelled."; PlayFeedback(AstralFeedbackCue.Cancel); }
                ApplyInputGate();
                return;
            }
            if (Pressed(AstralCommand.Cancel))
            {
                UiPause();
                return;
            }
            if (Pressed(AstralCommand.Restart))
            {
                restartPending = true;
                message = $"Restart progress? {Prompt(AstralCommand.Confirm)} confirms; {Prompt(AstralCommand.Cancel)} cancels. Existing disk save is retained.";
                ApplyInputGate();
                return;
            }
            if (Pressed(AstralCommand.Save))
                SaveGame();
            if (Pressed(AstralCommand.Load))
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
                    if (Pressed(AstralCommand.Encounter))
                        TryBeginEncounter();
                    else if (Pressed(AstralCommand.Vendor))
                        OpenVendor();
                    else if (Pressed(AstralCommand.Tonic))
                        UseFieldTonic();
                    else if (Pressed(AstralCommand.Party))
                        flow = Flow.PartyManagement;
                    break;
                case Flow.Encounter:
                    if (Pressed(AstralCommand.Confirm) || Pressed(AstralCommand.Encounter))
                        BeginBattle();
                    break;
                case Flow.Battle:
                    UpdateBattleInput();
                    break;
                case Flow.Recruitment:
                    if (Pressed(AstralCommand.Replace))
                        RecruitReward();
                    break;
                case Flow.PartyManagement:
                    if (Pressed(AstralCommand.Party))
                        ReorderParty();
                    if (Pressed(AstralCommand.Confirm) || Pressed(AstralCommand.Cancel))
                        ReturnToExploration();
                    break;
                case Flow.Vendor:
                    if (Pressed(AstralCommand.SelectActiveOne)) BuyFieldTonic();
                    else if (Pressed(AstralCommand.SelectActiveTwo)) SellSalvagedAlloy();
                    else if (Pressed(AstralCommand.Confirm) || Pressed(AstralCommand.Cancel)) LeaveVendor();
                    break;
                case Flow.Defeat:
                    if (Pressed(AstralCommand.Confirm)) ReturnToExploration();
                    break;
                case Flow.Victory:
                    if (Pressed(AstralCommand.Confirm)) ContinueAfterVictory();
                    break;
            }
            ApplyInputGate();
        }

        private void UpdateBattleInput()
        {
            if (Pressed(AstralCommand.SelectActiveOne)) UiSelectActiveSlot(0);
            if (Pressed(AstralCommand.SelectActiveTwo)) UiSelectActiveSlot(1);
            if (Pressed(AstralCommand.TargetOne)) UiSelectTargetSlot(0);
            if (Pressed(AstralCommand.TargetTwo)) UiSelectTargetSlot(1);
            if (Pressed(AstralCommand.Attack))
                ResolvePlayerAction();
            if (Pressed(AstralCommand.ArcBurst))
                ResolveArcBurst();
            if (Pressed(AstralCommand.Guard))
                GuardSelectedActive();
            if (Pressed(AstralCommand.Replace))
                ReplaceFaintedFromReserve();
            if (Pressed(AstralCommand.Swap))
                SwitchToBench(false);
        }

        private bool Pressed(AstralCommand command) => commandInput != null && commandInput.WasPressed(command);

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
            battleEngine = null;
            activeEncounterZone = null;
            SetClearedEncounterZones(Array.Empty<string>());
            SetCollectedCurrencyPickups(Array.Empty<string>());
            SetCollectedItemPickups(Array.Empty<string>());
            message = $"New game: explore for the marked wild activity. {Prompt(AstralCommand.Encounter)} starts an encounter there; {Prompt(AstralCommand.Interact)} activates the beacon.";
        }

        private void AddParty(string id, string displayName)
        {
            party.Add(new AstralDemoMember { id = id, displayName = displayName });
        }

        private void TryBeginEncounter()
        {
            if (flow != Flow.Exploration)
                return;

            if (!TryGetEncounterZoneAtPlayer(out AstralEncounterZone encounterZone))
            {
                message = $"No wild Astral activity here. Explore to the glowing encounter site, then press {Prompt(AstralCommand.Encounter)}.";
                PlayFeedback(AstralFeedbackCue.Denied);
                return;
            }

            activeEncounterZone = encounterZone;
            flow = Flow.Encounter;
            string preview = encounterZone.PrimaryAstralName;
            message = $"{encounterZone.DisplayName}: {preview} and a companion detected. {Prompt(AstralCommand.Confirm)} begins the 2v2 battle.";
            PlayFeedback(AstralFeedbackCue.Confirm);
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
                message = $"No supply relay in range. Find the cyan field terminal, then press {Prompt(AstralCommand.Vendor)}.";
                PlayFeedback(AstralFeedbackCue.Denied);
                return;
            }

            flow = Flow.Vendor;
            message = $"{vendorStation.DisplayName}: 1 buys a Field Tonic for {AstralVendorService.FieldTonicPrice} Starshards; " +
                      $"2 sells one Salvaged Alloy for {AstralVendorService.SalvagedAlloySaleValue}.";
            PlayFeedback(AstralFeedbackCue.Confirm);
        }

        private void BuyFieldTonic()
        {
            if (flow != Flow.Vendor)
                return;

            if (AstralVendorService.TryBuyFieldTonic(wallet, inventory))
            {
                message = $"Purchased one Field Tonic with earned Starshards. Balance: {wallet.Balance}.";
                PlayFeedback(AstralFeedbackCue.Confirm);
            }
            else
            {
                message = $"Trade declined. A Field Tonic costs {AstralVendorService.FieldTonicPrice} Starshards; no currency or items changed.";
                PlayFeedback(AstralFeedbackCue.Denied);
            }
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
                PlayFeedback(commissionCompleted ? AstralFeedbackCue.Reward : AstralFeedbackCue.Confirm);
            }
            else
            {
                message = "Trade declined. No Salvaged Alloy is available, or the wallet is full; no currency or items changed.";
                PlayFeedback(AstralFeedbackCue.Denied);
            }
        }

        private void LeaveVendor()
        {
            if (flow != Flow.Vendor)
                return;

            flow = Flow.Exploration;
            message = $"Left the supply relay. {Prompt(AstralCommand.Tonic)} uses a Field Tonic on the most injured conscious party member.";
            PlayFeedback(AstralFeedbackCue.Cancel);
        }

        private void UseFieldTonic()
        {
            if (flow != Flow.Exploration)
                return;

            AstralDemoMember target = FindTonicTarget();
            if (target == null || !inventory.TryRemove(AstralItemId.FieldTonic, 1))
            {
                message = FieldTonics == 0
                    ? "No Field Tonics available. Earn Starshards through play and visit the supply relay."
                    : "The party has no conscious injured Astral; no tonic was consumed.";
                PlayFeedback(AstralFeedbackCue.Denied);
                return;
            }

            int healed = Mathf.Min(12, target.maxHp - target.hp);
            target.hp += healed;
            message = $"{target.displayName} recovered {healed} HP using one Field Tonic.";
            PlayFeedback(AstralFeedbackCue.Confirm);
        }

        private AstralDemoMember FindTonicTarget()
        {
            AstralDemoMember best = null;
            int mostMissingHp = 0;
            for (int i = 0; i < party.Count; i++)
            {
                AstralDemoMember candidate = party[i];
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

            activeParty[0] = AstralBattleEngine.FindFirstEligibleParty(party, 0);
            activeParty[1] = AstralBattleEngine.FindFirstEligibleParty(party, activeParty[0] + 1);
            if (activeParty[0] < 0) { flow = Flow.Defeat; message = $"No healthy Astrals. {Prompt(AstralCommand.Confirm)} recovers."; return; }
            selectedActiveSlot = 0;
            selectedTargetSlot = 0;
            flow = Flow.Battle;
            battleEngine = new AstralBattleEngine(
                party, activeParty,
                activeEncounterZone.PrimaryAstralId, activeEncounterZone.PrimaryAstralName,
                activeEncounterZone.CompanionAstralId, activeEncounterZone.CompanionAstralName,
                activeEncounterZone.OpponentDamage);
            message = $"2v2 battle: {Prompt(AstralCommand.SelectActiveOne)}/{Prompt(AstralCommand.SelectActiveTwo)} selects your active slot; " +
                      $"{Prompt(AstralCommand.TargetOne)}/{Prompt(AstralCommand.TargetTwo)} targets; {Prompt(AstralCommand.Attack)} attacks; " +
                      $"{Prompt(AstralCommand.ArcBurst)} bursts both opponents. {activeEncounterZone.TacticalBrief} " +
                      $"{Prompt(AstralCommand.Guard)} guards the selected slot.";
            PlayFeedback(AstralFeedbackCue.Combat);
        }

        private void ResolvePlayerAction()
        {
            if (flow != Flow.Battle || battleEngine == null)
                return;

            var outcome = battleEngine.QueueAttack(selectedActiveSlot, selectedTargetSlot, out _);
            if (outcome == AstralBattleEngine.ActionOutcome.Invalid)
            {
                message = "Invalid action: choose an eligible active Astral and a living target.";
                return;
            }
            if (outcome == AstralBattleEngine.ActionOutcome.SlotAlreadyActed)
            {
                message = "That Astral already acted this round. Choose the other active slot.";
                return;
            }

            PlayFeedback(AstralFeedbackCue.Combat);
            if (battleEngine.AllOpponentsDefeated())
            {
                CompleteBattle(true);
                return;
            }

            FinishRoundIfReady();
        }

        private void ResolveArcBurst()
        {
            if (flow != Flow.Battle || battleEngine == null)
                return;

            var outcome = battleEngine.QueueArcBurst(selectedActiveSlot, out int targetsHit);
            if (outcome == AstralBattleEngine.ActionOutcome.Invalid)
            {
                message = "Invalid Arc Burst: choose a conscious active Astral.";
                return;
            }
            if (outcome == AstralBattleEngine.ActionOutcome.SlotAlreadyActed)
            {
                message = "That Astral already acted this round. Choose the other active slot.";
                return;
            }

            if (battleEngine.AllOpponentsDefeated())
            {
                CompleteBattle(true);
                return;
            }

            AstralDemoMember actor = battleEngine.GetActiveParty(selectedActiveSlot);
            message = $"{actor.displayName}'s Arc Burst dealt {AstralCombatRules.ArcBurstDamagePerTarget} damage to {targetsHit} opponent{(targetsHit == 1 ? "" : "s")}.";
            PlayFeedback(AstralFeedbackCue.Combat);
            FinishRoundIfReady();
        }

        private void GuardSelectedActive()
        {
            if (flow != Flow.Battle || battleEngine == null)
                return;

            var outcome = battleEngine.QueueGuard(selectedActiveSlot);
            if (outcome == AstralBattleEngine.ActionOutcome.Invalid)
            {
                message = "Invalid guard: choose a conscious active Astral.";
                return;
            }
            if (outcome == AstralBattleEngine.ActionOutcome.SlotAlreadyActed)
            {
                message = "That Astral already acted this round. Choose the other active slot.";
                return;
            }

            AstralDemoMember actor = battleEngine.GetActiveParty(selectedActiveSlot);
            message = $"{actor.displayName} guards its position against the next counterattack.";
            PlayFeedback(AstralFeedbackCue.Guard);
            FinishRoundIfReady();
        }

        private void FinishRoundIfReady()
        {
            if (battleEngine == null)
                return;

            for (int slot = 0; slot < 2; slot++)
            {
                var member = battleEngine.GetActiveParty(slot);
                if (member != null && !member.defeated && !battleEngine.HasActed(slot))
                { selectedActiveSlot = slot; message = "Choose an action for the remaining active Astral."; return; }
            }

            var result = battleEngine.TryFinishRound();
            if (!result.Resolved)
                return;

            if (result.PlayerDefeat) CompleteBattle(false);
            else message = $"Round resolved: {result.Summary} {Prompt(AstralCommand.Attack)} attacks, {Prompt(AstralCommand.ArcBurst)} bursts both, " +
                           $"{Prompt(AstralCommand.Guard)} guards, {Prompt(AstralCommand.Swap)} swaps.";
        }

        private void ReplaceFaintedFromReserve()
        {
            SwitchToBench(true);
        }

        private void SwitchToBench(bool requireFainted)
        {
            if (flow != Flow.Battle || battleEngine == null) return;
            AstralDemoMember current = battleEngine.GetActiveParty(selectedActiveSlot);
            if (current == null || (requireFainted && !current.defeated))
            {
                message = "Replacement is available only for a fainted active slot.";
                return;
            }
            if (!current.defeated && battleEngine.HasActed(selectedActiveSlot))
            { message = "This slot already acted this round."; return; }
            for (int i = 0; i < party.Count; i++)
            {
                if (party[i].defeated || i == activeParty[0] || i == activeParty[1])
                    continue;
                if (!battleEngine.Battle.TrySwitch(BattleSide.Player, selectedActiveSlot, i)) continue;
                bool voluntary = !current.defeated;
                activeParty[selectedActiveSlot] = i;
                message = "Healthy bench Astral deployed. Party size unchanged.";
                PlayFeedback(AstralFeedbackCue.Confirm);
                if (voluntary) { battleEngine.MarkActed(selectedActiveSlot); FinishRoundIfReady(); }
                return;
            }
            message = "No healthy inactive party members remain. Storage is unavailable in battle.";
        }

        private void ForceSelectedFaintForPrototypeTesting()
        {
            AstralDemoMember current = battleEngine?.GetActiveParty(selectedActiveSlot);
            if (current != null)
                current.hp = 0;
            if (current != null)
                current.defeated = true;
            message = $"Prototype test: selected active Astral fainted. {Prompt(AstralCommand.Replace)} replaces from reserve.";
        }

        private void CompleteBattle(bool playerWon)
        {
            if (flow != Flow.Battle)
                return;
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
                    ? $"Victory. Earned {reward} Starshards{(gainedAlloy ? " and 1 Salvaged Alloy" : "")}. {Prompt(AstralCommand.Replace)} recruits exactly one Astral reward."
                    : $"Victory{(gainedAlloy ? ". Recovered 1 Salvaged Alloy" : "")}. {Prompt(AstralCommand.Replace)} recruits exactly one Astral reward.";
                PlayFeedback(AstralFeedbackCue.Reward);
            }
            else
            {
                message = $"Defeat. {Prompt(AstralCommand.Confirm)} returns to exploration; no reward was granted.";
                PlayFeedback(AstralFeedbackCue.Denied);
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
            AstralDemoMember reward = new AstralDemoMember { id = id, displayName = "Recruited Astral " + encountersCompleted };
            if (party.Count < AstralCampaignState.PartyCapacity)
            {
                party.Add(reward);
                message = $"Astral recruited to party. {Prompt(AstralCommand.Party)} manages the party; {Prompt(AstralCommand.Confirm)} returns to exploration.";
            }
            else
            {
                reserve.Add(reward);
                message = $"Party full: Astral safely stored in reserve. {Prompt(AstralCommand.Party)} manages the party; {Prompt(AstralCommand.Confirm)} returns to exploration.";
            }
            flow = Flow.PartyManagement;
            PlayFeedback(AstralFeedbackCue.Reward);
        }

        private void ReorderParty()
        {
            if (party.Count > 1)
            {
                AstralDemoMember first = party[0];
                party[0] = party[1];
                party[1] = first;
                activeParty[0] = 0;
                activeParty[1] = Mathf.Min(1, party.Count - 1);
            }
            message = $"Party reordered; the next encounter uses the current active pair. {Prompt(AstralCommand.Confirm)} returns to exploration.";
            PlayFeedback(AstralFeedbackCue.Confirm);
        }

        private void ReturnToExploration()
        {
            if (flow == Flow.Defeat)
            {
                foreach (var member in party) { member.hp = member.maxHp; member.defeated = false; }
                message = "Party recovered. Return to the wild activity site for another encounter.";
            }
            else
            message = $"Returned to exploration. Visit the wild activity site for another encounter; {Prompt(AstralCommand.Interact)} activates the beacon; {Prompt(AstralCommand.Save)} saves.";
            flow = Flow.Exploration;
            battleEngine = null;
            activeEncounterZone = null;
            PlayFeedback(AstralFeedbackCue.Confirm);
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
            PlayFeedback(commissionCompleted ? AstralFeedbackCue.Reward : AstralFeedbackCue.Confirm);
        }

        private bool TryCompleteWayfarerCommission()
        {
            return wayfarerCommission.TryComplete(DemoObjectiveComplete && victoryAcknowledged, wallet);
        }

        private void SaveGame()
        {
            if (!CanSaveNow)
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
                hasSaveGame = true;
                message = "Game saved.";
                PlayFeedback(AstralFeedbackCue.Confirm);
            }
            catch (Exception exception)
            {
                Debug.LogError("Astral save failed: " + exception.Message, this);
            }
        }

        private bool LoadGame()
        {
            try
            {
                string path = Path.Combine(Application.persistentDataPath, SaveFileName);
                if (!File.Exists(path)) { hasSaveGame = false; message = "No save found; new game retained."; return false; }
                SaveData data = JsonUtility.FromJson<SaveData>(File.ReadAllText(path));
                if (!ValidateSave(data)) { message = "Save invalid; current progress retained."; return false; }
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
                if (AstralBattleEngine.AllDefeated(party)) foreach (var member in party) { member.hp = member.maxHp; member.defeated = false; }
                activeParty[0] = AstralBattleEngine.FindFirstEligibleParty(party, 0);
                activeParty[1] = AstralBattleEngine.FindFirstEligibleParty(party, activeParty[0] + 1);
                battleEngine = null;
                activeEncounterZone = null;
                IReadOnlyCollection<string> clearedZoneIds = data.clearedEncounterZoneIds;
                if (clearedZoneIds == null)
                    clearedZoneIds = Array.Empty<string>();
                SetClearedEncounterZones(clearedZoneIds);
                SetCollectedCurrencyPickups(collectedCurrencyPickupIds);
                SetCollectedItemPickups(collectedItemPickupIds);
                if (beacon != null) beacon.RestoreProgress(beaconActivated);
                message = "Checkpoint loaded. Explore to the wild activity site to begin an encounter.";
                hasSaveGame = true;
                PlayFeedback(AstralFeedbackCue.Confirm);
                return true;
            }
            catch (Exception exception)
            {
                Debug.LogError("Astral load failed: " + exception.Message, this);
                message = "Save could not be loaded; current progress retained.";
                return false;
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
            if (flow == Flow.Battle && battleEngine != null)
            {
                GUILayout.Space(8);
                AstralDemoMember opp0 = battleEngine.GetOpponent(0);
                AstralDemoMember opp1 = battleEngine.GetOpponent(1);
                GUILayout.Label("Opponent slot 1: " + opp0.displayName + " HP " + opp0.hp + (opp0.defeated ? " FAINTED" : ""));
                GUILayout.Label("Opponent slot 2: " + opp1.displayName + " HP " + opp1.hp + (opp1.defeated ? " FAINTED" : ""));
                GUILayout.Label("Selected active slot: " + (selectedActiveSlot + 1) + "  target: " + (selectedTargetSlot + 1));
                GUILayout.Label("A attack | Q/W target | 1/2 active slot | R replace | S swap bench");
            }
            GUILayout.Label("K save | L load | N new game");
            GUILayout.EndArea();
        }
#endif
    }
}
