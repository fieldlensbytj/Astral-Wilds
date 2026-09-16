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
        private enum Flow { Exploration, Encounter, Battle, Recruitment, PartyManagement, Victory, Defeat }

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
            public int version = 1;
            public Flow flow;
            public bool beaconActivated;
            public int encountersCompleted;
            public List<DemoMember> party = new List<DemoMember>();
            public List<DemoMember> reserve = new List<DemoMember>();
        }

        private const string SaveFileName = "astralwilds-demo-save-v1.json";
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
        private AstralBattleState battle;
        private readonly bool[] acted = new bool[2];
        private AstralPlayerController playerController;
        private AstralThirdPersonCamera orbitCamera;
        private CrashedBeaconObjective beacon;
        private AstralEncounterZone encounterZone;
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
        public bool IsDefeat => flow == Flow.Defeat;
        public bool IsRestartPending => restartPending;
        public bool CanBeginEncounter => IsExploration && playerController != null && encounterZone != null && encounterZone.Contains(playerController.transform.position);
        public string EncounterActionLabel => CanBeginEncounter ? "Encounter (B)" : "Find wild activity";
        public int SelectedActiveSlot => selectedActiveSlot;
        public int SelectedTargetSlot => selectedTargetSlot;
        public int EncountersCompleted => encountersCompleted;
        public bool BeaconActivated => beacon != null ? beacon.IsActivated : beaconActivated;
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
        public void UiReplaceFainted() { if (flow == Flow.Battle) ReplaceFaintedFromReserve(); }
        public void UiSwapBench() { if (flow == Flow.Battle) SwitchToBench(false); }
        public void UiBeginEncounter() { if (flow == Flow.Exploration && !restartPending) TryBeginEncounter(); }
        public void UiBeginBattle() { if (flow == Flow.Encounter) BeginBattle(); }
        public void UiRecruit() { if (flow == Flow.Recruitment) RecruitReward(); }
        public void UiOpenPartyManagement() { if (flow == Flow.Exploration && !restartPending) flow = Flow.PartyManagement; }
        public void UiReorderParty() { if (flow == Flow.PartyManagement) ReorderParty(); }
        public void UiConfirmDefeatRecovery() { if (flow == Flow.Defeat) ReturnToExploration(); }
        public void UiReturnToExploration() { if (flow == Flow.PartyManagement) ReturnToExploration(); }
        public void UiSave() { SaveGame(); }
        public void UiLoad() { LoadGame(); ApplyInputGate(); }
        public void UiRequestRestart() { if (!restartPending) { restartPending = true; message = "Restart progress? Confirm or cancel below. Existing disk save is retained."; ApplyInputGate(); } }
        public void UiConfirmRestart() { if (restartPending) { NewGame(); restartPending = false; ApplyInputGate(); } }
        public void UiCancelRestart() { if (restartPending) { restartPending = false; message = "Restart cancelled."; ApplyInputGate(); } }

        private void Awake()
        {
            NewGame();
        }

        private void Start()
        {
            playerController = FindFirstObjectByType<AstralPlayerController>();
            orbitCamera = FindFirstObjectByType<AstralThirdPersonCamera>();
            beacon = FindFirstObjectByType<CrashedBeaconObjective>();
            encounterZone = FindFirstObjectByType<AstralEncounterZone>();
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

            switch (flow)
            {
                case Flow.Exploration:
                    if (keyboard.bKey.wasPressedThisFrame)
                        TryBeginEncounter();
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
                case Flow.Defeat:
                    if (keyboard.enterKey.wasPressedThisFrame) ReturnToExploration();
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
            activeParty[0] = 0;
            activeParty[1] = 1;
            opponent[0] = opponent[1] = null;
            acted[0] = acted[1] = false;
            battle = null;
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

            if (!CanBeginEncounter)
            {
                message = "No wild Astral activity here. Explore to the glowing encounter site, then press B.";
                return;
            }

            flow = Flow.Encounter;
            string preview = EncounterPresets[encountersCompleted % EncounterPresets.Length].nameA;
            message = $"{encounterZone.DisplayName}: {preview} and a companion detected. Enter/B begins the 2v2 battle.";
        }

        // Each completed encounter reveals a different wild pair, so exploring for a
        // second encounter is a genuinely different discovery, not a repeat of the first.
        private static readonly (string idA, string nameA, string idB, string nameB)[] EncounterPresets =
        {
            ("wild-ember", "Wild Ember Astral", "wild-frost", "Wild Frost Astral"),
            ("wild-stone", "Wild Stone Astral", "wild-gale", "Wild Gale Astral"),
            ("wild-tide", "Wild Tide Astral", "wild-verdant", "Wild Verdant Astral"),
        };

        private void BeginBattle()
        {
            if (flow != Flow.Encounter)
                return;
            var preset = EncounterPresets[encountersCompleted % EncounterPresets.Length];
            opponent[0] = new DemoMember { id = preset.idA, displayName = preset.nameA };
            opponent[1] = new DemoMember { id = preset.idB, displayName = preset.nameB };
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
            message = "2v2 battle: 1/2 select your active slot, Q/W select target, A attack, R reserve replacement.";
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

            target.hp = Mathf.Max(0, target.hp - 12);
            if (target.hp == 0)
            {
                target.defeated = true;
                battle.OpponentParty.Astrals[selectedTargetSlot].MarkDefeated();
            }
            if (AllOpponentsDefeated())
            {
                CompleteBattle(true);
                return;
            }

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
            ResolveOpponentActions();
            acted[0] = acted[1] = false;
            battle.ClearQueuedActions();
            if (AllPartyDefeated()) CompleteBattle(false);
            else message = "Round resolved. A: attack, R: replace fainted slot, S: swap to next healthy bench member.";
        }

        private void ResolveOpponentActions()
        {
            for (int i = 0; i < activeOpponent.Length; i++)
            {
                DemoMember enemy = opponent[activeOpponent[i]];
                DemoMember target = GetActiveParty(i % 2);
                if (target == null || target.defeated) target = GetActiveParty((i + 1) % 2);
                if (enemy == null || enemy.defeated || target == null || target.defeated)
                    continue;
                target.hp = Mathf.Max(0, target.hp - 6);
                if (target.hp == 0)
                {
                    target.defeated = true;
                    battle.PlayerParty.Astrals[party.IndexOf(target)].MarkDefeated();
                }
            }
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
            flow = playerWon ? Flow.Recruitment : Flow.Defeat;
            if (playerWon)
            {
                encountersCompleted++;
                message = "Victory. R: recruit exactly one Astral reward.";
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
        }

        private void SaveGame()
        {
            if (flow != Flow.Exploration && flow != Flow.PartyManagement)
            { message = "Save between battles, after recruitment is resolved."; return; }
            try
            {
                SaveData data = new SaveData { flow = Flow.Exploration, beaconActivated = beacon != null ? beacon.IsActivated : beaconActivated, encountersCompleted = encountersCompleted };
                data.party.AddRange(party);
                data.reserve.AddRange(reserve);
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
                flow = Flow.Exploration; beaconActivated = data.beaconActivated; encountersCompleted = data.encountersCompleted;
                if (AllPartyDefeated()) foreach (var member in party) { member.hp = member.maxHp; member.defeated = false; }
                activeParty[0] = FindFirstEligibleParty(0);
                activeParty[1] = FindFirstEligibleParty(activeParty[0] + 1);
                opponent[0] = opponent[1] = null;
                acted[0] = acted[1] = false;
                battle = null;
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
            if (data == null || data.version != 1 || data.party == null || data.reserve == null ||
                data.party.Count < 1 || data.party.Count > AstralParty.PlayerCapacity || data.encountersCompleted < 0)
                return false;
            var ids = new HashSet<string>(StringComparer.Ordinal);
            foreach (var list in new[] { data.party, data.reserve })
                foreach (var member in list)
                    if (member == null || string.IsNullOrWhiteSpace(member.id) || !ids.Add(member.id) ||
                        string.IsNullOrWhiteSpace(member.displayName) || member.maxHp <= 0 ||
                        member.hp < 0 || member.hp > member.maxHp || member.defeated != (member.hp == 0)) return false;
            return true;
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
    }
}
