using System.Collections.Generic;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem.UI;
using UnityEngine.UI;

namespace AstralWilds
{
    /// <summary>
    /// Real (clickable) battle + party interface, built entirely in code so it needs
    /// no scene/prefab editing. Reads AstralDemoLoopController's public Ui* state and
    /// drives it through its public Ui* action methods; the keyboard shortcuts on
    /// AstralDemoLoopController itself keep working unchanged alongside this.
    /// </summary>
    [DefaultExecutionOrder(-50)]
    public sealed class AstralBattleHUD : MonoBehaviour
    {
        private static readonly Color PanelColor = new Color(0.06f, 0.08f, 0.10f, 0.82f);
        private static readonly Color ActiveHighlight = new Color(1f, 0.85f, 0.35f, 1f);
        private static readonly Color SelectedTarget = new Color(1f, 0.35f, 0.30f, 1f);
        private static readonly Color HpGood = new Color(0.35f, 0.85f, 0.45f, 1f);
        private static readonly Color HpLow = new Color(0.85f, 0.30f, 0.25f, 1f);
        private static readonly Color ButtonIdle = new Color(0.16f, 0.20f, 0.26f, 0.95f);
        private static readonly Color FaintedTint = new Color(0.35f, 0.35f, 0.35f, 1f);

        private AstralDemoLoopController controller;
        private readonly Dictionary<string, Sprite> portraits = new Dictionary<string, Sprite>();

        private Canvas canvas;
        private Text statusText;
        private Text messageText;
        private Text objectiveText;
        private GameObject completionBanner;
        private Transform opponentRow;
        private readonly List<CreatureSlotView> opponentSlots = new List<CreatureSlotView>();
        private Transform partyRow;
        private readonly List<CreatureSlotView> partySlots = new List<CreatureSlotView>();
        private Transform actionBar;
        private string lastLayoutKey = "";

        private sealed class CreatureSlotView
        {
            public GameObject root;
            public Image portrait;
            public Image border;
            public Text nameText;
            public Image hpFill;
            public Text hpText;
            public Button button;
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        private static void Bootstrap()
        {
            // Self-installing: no scene/prefab edit needed. Only attaches when a demo
            // loop controller is actually present in the loaded scene, and only once.
            if (FindFirstObjectByType<AstralBattleHUD>() != null) return;
            if (FindFirstObjectByType<AstralDemoLoopController>() == null) return;
            var go = new GameObject("AstralBattleHUD");
            go.AddComponent<AstralBattleHUD>();
        }

        private void Awake()
        {
            controller = FindFirstObjectByType<AstralDemoLoopController>();
            LoadPortraits();
            BuildCanvas();
            BuildStatusPanel();
            BuildCompletionBanner();
            opponentRow = BuildCreatureRow("OpponentRow", new Vector2(0.5f, 1f), new Vector2(0f, -170f), 2, opponentSlots, isOpponentRow: true);
            partyRow = BuildCreatureRow("PartyRow", new Vector2(0.5f, 0f), new Vector2(0f, 150f), 6, partySlots, isOpponentRow: false);
            actionBar = BuildActionBarRoot();
            EnsureEventSystem();
        }

        private void LoadPortraits()
        {
            foreach (var id in new[] { "cindrel", "mossling", "ripplefin", "stormrook", "ironbur" })
            {
                string cap = char.ToUpperInvariant(id[0]) + id.Substring(1);
                var sprite = Resources.Load<Sprite>("Portraits/Portrait_" + cap);
                if (sprite != null) portraits[id] = sprite;
            }
        }

        private void Update()
        {
            if (controller == null) return;
            Refresh();
        }

        private void Refresh()
        {
            RefreshStatusText();
            RefreshRow(controller.GetOpponentUiInfo(), opponentSlots, opponentRow.gameObject, controller.InBattle, isOpponentRow: true);
            RefreshRow(controller.GetPartyUiInfo(), partySlots, partyRow.gameObject, true, isOpponentRow: false);
            RefreshActionBar();
            if (completionBanner != null) completionBanner.SetActive(controller.DemoObjectiveComplete);
        }

        private void RefreshStatusText()
        {
            statusText.text = $"{controller.CurrentState}    Party {controller.PartyCount}/6    Reserve {controller.ReserveCount}";
            messageText.text = controller.StatusMessage;
            objectiveText.text = controller.ObjectiveGuidance;
        }

        private void RefreshRow(List<AstralDemoLoopController.AstralUiInfo> infos, List<CreatureSlotView> slots, GameObject rowRoot, bool visible, bool isOpponentRow)
        {
            rowRoot.SetActive(visible && infos.Count > 0);
            if (!visible) return;
            for (int i = 0; i < slots.Count; i++)
            {
                var slot = slots[i];
                if (i >= infos.Count) { slot.root.SetActive(false); continue; }
                slot.root.SetActive(true);
                var info = infos[i];
                slot.nameText.text = info.displayName + (info.defeated ? "\n(fainted)" : "");
                slot.hpText.text = info.hp + "/" + info.maxHp;
                float pct = info.maxHp > 0 ? (float)info.hp / info.maxHp : 0f;
                slot.hpFill.fillAmount = Mathf.Clamp01(pct);
                slot.hpFill.color = pct <= 0.3f ? HpLow : HpGood;
                slot.portrait.color = info.defeated ? FaintedTint : Color.white;
                if (portraits.TryGetValue(info.id, out var sprite)) slot.portrait.sprite = sprite;

                bool selected = isOpponentRow
                    ? (controller.InBattle && info.activeSlot == controller.SelectedTargetSlot)
                    : (controller.InBattle && info.activeSlot == controller.SelectedActiveSlot);
                bool isActiveBattleSlot = info.isActive && controller.InBattle;
                slot.border.enabled = isActiveBattleSlot || selected;
                slot.border.color = selected ? SelectedTarget : ActiveHighlight;

                int capturedActiveSlot = info.activeSlot;
                bool capturedOpponentRow = isOpponentRow;
                slot.button.onClick.RemoveAllListeners();
                slot.button.interactable = controller.InBattle && capturedActiveSlot >= 0;
                slot.button.onClick.AddListener(() =>
                {
                    if (capturedOpponentRow) controller.UiSelectTargetSlot(capturedActiveSlot);
                    else controller.UiSelectActiveSlot(capturedActiveSlot);
                });
            }
        }

        private void RefreshActionBar()
        {
            string key = controller.CurrentState + "|" + controller.IsRestartPending + "|" + controller.CanBeginEncounter;
            if (key == lastLayoutKey) return;
            lastLayoutKey = key;

            foreach (Transform child in actionBar) Destroy(child.gameObject);

            if (controller.IsRestartPending)
            {
                AddButton(actionBar, "Confirm Restart (Enter)", controller.UiConfirmRestart);
                AddButton(actionBar, "Cancel (Esc)", controller.UiCancelRestart);
                return;
            }

            if (controller.IsExploration)
            {
                AddButton(actionBar, controller.EncounterActionLabel, controller.UiBeginEncounter, controller.CanBeginEncounter);
                AddButton(actionBar, "Party (P)", controller.UiOpenPartyManagement);
                AddButton(actionBar, "Save (K)", controller.UiSave);
                AddButton(actionBar, "Load (L)", controller.UiLoad);
                AddButton(actionBar, "Restart (N)", controller.UiRequestRestart);
            }
            else if (controller.IsEncounter)
            {
                AddButton(actionBar, "Begin Battle (Enter)", controller.UiBeginBattle);
            }
            else if (controller.InBattle)
            {
                AddButton(actionBar, "Attack (A)", controller.UiAttack);
                AddButton(actionBar, "Replace Fainted (R)", controller.UiReplaceFainted);
                AddButton(actionBar, "Swap (S)", controller.UiSwapBench);
            }
            else if (controller.IsRecruitment)
            {
                AddButton(actionBar, "Recruit (R)", controller.UiRecruit);
            }
            else if (controller.IsPartyManagement)
            {
                AddButton(actionBar, "Reorder (P)", controller.UiReorderParty);
                AddButton(actionBar, "Exploration (Enter)", controller.UiReturnToExploration);
            }
            else if (controller.IsDefeat)
            {
                AddButton(actionBar, "Recover (Enter)", controller.UiConfirmDefeatRecovery);
            }
        }

        // ---------------------------------------------------------------- build helpers

        private void BuildCanvas()
        {
            var canvasGo = new GameObject("AstralBattleHUD_Canvas");
            canvasGo.transform.SetParent(transform, false);
            canvas = canvasGo.AddComponent<Canvas>();
            canvas.renderMode = RenderMode.ScreenSpaceOverlay;
            canvas.sortingOrder = 100;
            var scaler = canvasGo.AddComponent<CanvasScaler>();
            scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
            scaler.referenceResolution = new Vector2(1920, 1080);
            scaler.matchWidthOrHeight = 0.5f;
            canvasGo.AddComponent<GraphicRaycaster>();
        }

        private void BuildStatusPanel()
        {
            var panel = CreatePanel(canvas.transform, "StatusPanel", new Vector2(0f, 1f), new Vector2(0f, 1f), new Vector2(20f, -20f), new Vector2(680f, 132f));
            statusText = CreateText(panel.transform, "StateLine", 22, TextAnchor.UpperLeft, FontStyle.Bold);
            SetRect(statusText.rectTransform, new Vector2(0f, 1f), new Vector2(1f, 1f), new Vector2(14f, -6f), new Vector2(-14f, -34f));
            messageText = CreateText(panel.transform, "MessageLine", 17, TextAnchor.UpperLeft, FontStyle.Normal);
            SetRect(messageText.rectTransform, new Vector2(0f, 0f), new Vector2(1f, 1f), new Vector2(14f, 36f), new Vector2(-14f, -34f));
            objectiveText = CreateText(panel.transform, "ObjectiveLine", 15, TextAnchor.MiddleLeft, FontStyle.Bold);
            SetRect(objectiveText.rectTransform, new Vector2(0f, 0f), new Vector2(1f, 0f), new Vector2(14f, 6f), new Vector2(-14f, 32f));
            objectiveText.color = new Color(0.45f, 0.95f, 0.90f, 1f);
        }

        private void BuildCompletionBanner()
        {
            completionBanner = CreatePanel(canvas.transform, "CompletionBanner", new Vector2(0.5f, 1f), new Vector2(0.5f, 1f), new Vector2(0f, -20f), new Vector2(640f, 64f));
            var text = CreateText(completionBanner.transform, "Label", 22, TextAnchor.MiddleCenter, FontStyle.Bold);
            SetRect(text.rectTransform, Vector2.zero, Vector2.one, new Vector2(10f, 0f), new Vector2(-10f, 0f));
            text.text = "DEMO OBJECTIVE COMPLETE -- beacon activated, two encounters cleared";
            text.color = new Color(1f, 0.9f, 0.5f, 1f);
            completionBanner.GetComponent<Image>().color = new Color(0.10f, 0.28f, 0.14f, 0.92f);
            completionBanner.SetActive(false);
        }

        private Transform BuildCreatureRow(string name, Vector2 anchor, Vector2 anchoredPos, int slotCount, List<CreatureSlotView> slots, bool isOpponentRow)
        {
            var rowGo = new GameObject(name, typeof(RectTransform));
            rowGo.transform.SetParent(canvas.transform, false);
            var rect = rowGo.GetComponent<RectTransform>();
            rect.anchorMin = rect.anchorMax = rect.pivot = anchor;
            rect.anchoredPosition = anchoredPos;
            float slotWidth = 150f;
            rect.sizeDelta = new Vector2(slotWidth * slotCount + 16f * (slotCount - 1), 190f);
            var layout = rowGo.AddComponent<HorizontalLayoutGroup>();
            layout.spacing = 16f;
            layout.childAlignment = TextAnchor.MiddleCenter;
            layout.childControlWidth = false;
            layout.childControlHeight = false;

            for (int i = 0; i < slotCount; i++)
                slots.Add(BuildCreatureSlot(rowGo.transform, isOpponentRow ? $"Opponent{i}" : $"Party{i}"));

            return rowGo.transform;
        }

        private CreatureSlotView BuildCreatureSlot(Transform parent, string name)
        {
            var root = new GameObject(name, typeof(RectTransform));
            root.transform.SetParent(parent, false);
            var rootRect = root.GetComponent<RectTransform>();
            rootRect.sizeDelta = new Vector2(150f, 190f);

            var bg = root.AddComponent<Image>();
            bg.color = PanelColor;

            var button = root.AddComponent<Button>();
            var colors = button.colors;
            colors.normalColor = Color.white;
            colors.highlightedColor = new Color(1f, 1f, 1f, 0.85f);
            button.colors = colors;

            var border = new GameObject("Border", typeof(RectTransform)).AddComponent<Image>();
            border.transform.SetParent(root.transform, false);
            SetRect(border.rectTransform, Vector2.zero, Vector2.one, new Vector2(-4f, -4f), new Vector2(4f, 4f));
            border.color = ActiveHighlight;
            border.sprite = null;
            border.type = Image.Type.Sliced;
            border.enabled = false;

            var portrait = new GameObject("Portrait", typeof(RectTransform)).AddComponent<Image>();
            portrait.transform.SetParent(root.transform, false);
            SetRect(portrait.rectTransform, new Vector2(0.08f, 0.32f), new Vector2(0.92f, 0.96f), Vector2.zero, Vector2.zero);
            portrait.preserveAspect = true;
            portrait.color = new Color(0.3f, 0.3f, 0.3f, 1f);

            var nameText = CreateText(root.transform, "Name", 13, TextAnchor.MiddleCenter, FontStyle.Bold);
            SetRect(nameText.rectTransform, new Vector2(0f, 0.16f), new Vector2(1f, 0.32f), Vector2.zero, Vector2.zero);

            var hpBack = new GameObject("HpBack", typeof(RectTransform)).AddComponent<Image>();
            hpBack.transform.SetParent(root.transform, false);
            SetRect(hpBack.rectTransform, new Vector2(0.08f, 0.06f), new Vector2(0.92f, 0.15f), Vector2.zero, Vector2.zero);
            hpBack.color = new Color(0f, 0f, 0f, 0.6f);

            var hpFillGo = new GameObject("HpFill", typeof(RectTransform));
            hpFillGo.transform.SetParent(hpBack.transform, false);
            var hpFillRect = hpFillGo.GetComponent<RectTransform>();
            SetRect(hpFillRect, Vector2.zero, Vector2.one, Vector2.zero, Vector2.zero);
            var hpFill = hpFillGo.AddComponent<Image>();
            hpFill.color = HpGood;
            hpFill.type = Image.Type.Filled;
            hpFill.fillMethod = Image.FillMethod.Horizontal;
            hpFill.fillAmount = 1f;

            var hpText = CreateText(root.transform, "HpText", 12, TextAnchor.MiddleCenter, FontStyle.Normal);
            SetRect(hpText.rectTransform, new Vector2(0.08f, 0.06f), new Vector2(0.92f, 0.15f), Vector2.zero, Vector2.zero);

            return new CreatureSlotView
            {
                root = root,
                portrait = portrait,
                border = border,
                nameText = nameText,
                hpFill = hpFill,
                hpText = hpText,
                button = button
            };
        }

        private Transform BuildActionBarRoot()
        {
            var go = new GameObject("ActionBar", typeof(RectTransform));
            go.transform.SetParent(canvas.transform, false);
            var rect = go.GetComponent<RectTransform>();
            rect.anchorMin = rect.anchorMax = new Vector2(1f, 0f);
            rect.pivot = new Vector2(1f, 0f);
            rect.anchoredPosition = new Vector2(-24f, 24f);
            rect.sizeDelta = new Vector2(260f, 260f);
            var layout = go.AddComponent<VerticalLayoutGroup>();
            layout.spacing = 10f;
            layout.childAlignment = TextAnchor.LowerRight;
            layout.childControlWidth = true;
            layout.childControlHeight = false;
            layout.childForceExpandWidth = true;
            layout.childForceExpandHeight = false;
            var fitter = go.AddComponent<ContentSizeFitter>();
            fitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;
            return go.transform;
        }

        private void AddButton(Transform parent, string label, UnityEngine.Events.UnityAction onClick, bool interactable = true)
        {
            var go = new GameObject("Btn_" + label, typeof(RectTransform));
            go.transform.SetParent(parent, false);
            var rect = go.GetComponent<RectTransform>();
            rect.sizeDelta = new Vector2(260f, 44f);
            var image = go.AddComponent<Image>();
            image.color = ButtonIdle;
            var button = go.AddComponent<Button>();
            button.onClick.AddListener(onClick);
            button.interactable = interactable;
            var layoutElement = go.AddComponent<LayoutElement>();
            layoutElement.preferredHeight = 44f;
            var text = CreateText(go.transform, "Label", 16, TextAnchor.MiddleCenter, FontStyle.Bold);
            SetRect(text.rectTransform, Vector2.zero, Vector2.one, Vector2.zero, Vector2.zero);
            text.text = label;
        }

        private GameObject CreatePanel(Transform parent, string name, Vector2 anchorMin, Vector2 anchorMax, Vector2 anchoredPos, Vector2 size)
        {
            var go = new GameObject(name, typeof(RectTransform));
            go.transform.SetParent(parent, false);
            var rect = go.GetComponent<RectTransform>();
            rect.anchorMin = anchorMin;
            rect.anchorMax = anchorMax;
            rect.pivot = new Vector2(anchorMin.x, anchorMax.y);
            rect.anchoredPosition = anchoredPos;
            rect.sizeDelta = size;
            var image = go.AddComponent<Image>();
            image.color = PanelColor;
            return go;
        }

        private Text CreateText(Transform parent, string name, int size, TextAnchor anchor, FontStyle style)
        {
            var go = new GameObject(name, typeof(RectTransform));
            go.transform.SetParent(parent, false);
            var text = go.AddComponent<Text>();
            text.font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
            text.fontSize = size;
            text.alignment = anchor;
            text.fontStyle = style;
            text.color = Color.white;
            text.horizontalOverflow = HorizontalWrapMode.Wrap;
            text.verticalOverflow = VerticalWrapMode.Overflow;
            return text;
        }

        private static void SetRect(RectTransform rect, Vector2 anchorMin, Vector2 anchorMax, Vector2 offsetMin, Vector2 offsetMax)
        {
            rect.anchorMin = anchorMin;
            rect.anchorMax = anchorMax;
            rect.offsetMin = offsetMin;
            rect.offsetMax = offsetMax;
        }

        private static void EnsureEventSystem()
        {
            if (FindFirstObjectByType<EventSystem>() != null)
                return;
            var go = new GameObject("EventSystem");
            go.AddComponent<EventSystem>();
            go.AddComponent<InputSystemUIInputModule>();
        }
    }
}
