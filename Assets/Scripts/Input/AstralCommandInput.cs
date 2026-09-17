using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem;

namespace AstralWilds
{
    public enum AstralCommand
    {
        Confirm,
        Cancel,
        OpenSettings,
        Save,
        Load,
        Restart,
        Interact,
        Encounter,
        Vendor,
        Tonic,
        Party,
        SelectActiveOne,
        SelectActiveTwo,
        TargetOne,
        TargetTwo,
        Attack,
        ArcBurst,
        Guard,
        Replace,
        Swap
    }

    public enum AstralPromptDevice
    {
        KeyboardMouse,
        Gamepad
    }

    /// <summary>
    /// Owns non-movement commands, device-aware prompts, and persistent keyboard
    /// overrides. Gameplay code asks for commands and never polls raw controls.
    /// </summary>
    public sealed class AstralCommandInput : IDisposable
    {
        private const string PlayerPrefsKey = "AstralWilds.CommandBindings.v1";
        private const string KeyboardGroup = "Keyboard&Mouse";
        private const string GamepadGroup = "Gamepad";

        private readonly struct Definition
        {
            public Definition(AstralCommand command, string keyboardPath, string gamepadPath, bool rebindable = false)
            {
                Command = command;
                KeyboardPath = keyboardPath;
                GamepadPath = gamepadPath;
                Rebindable = rebindable;
            }

            public AstralCommand Command { get; }
            public string KeyboardPath { get; }
            public string GamepadPath { get; }
            public bool Rebindable { get; }
        }

        [Serializable]
        private sealed class BindingSaveData
        {
            public List<BindingSaveEntry> bindings = new List<BindingSaveEntry>();
        }

        [Serializable]
        private sealed class BindingSaveEntry
        {
            public string command;
            public string path;
        }

        private static readonly Definition[] Definitions =
        {
            new Definition(AstralCommand.Confirm, "<Keyboard>/enter", "<Gamepad>/buttonSouth"),
            new Definition(AstralCommand.Cancel, "<Keyboard>/escape", "<Gamepad>/buttonEast"),
            new Definition(AstralCommand.OpenSettings, "<Keyboard>/o", "<Gamepad>/select"),
            new Definition(AstralCommand.Save, "<Keyboard>/k", "<Gamepad>/leftShoulder"),
            new Definition(AstralCommand.Load, "<Keyboard>/l", "<Gamepad>/rightShoulder"),
            new Definition(AstralCommand.Restart, "<Keyboard>/n", "<Gamepad>/leftStickPress"),
            new Definition(AstralCommand.Interact, "<Keyboard>/e", "<Gamepad>/buttonNorth"),
            new Definition(AstralCommand.Encounter, "<Keyboard>/b", "<Gamepad>/buttonSouth", true),
            new Definition(AstralCommand.Vendor, "<Keyboard>/v", "<Gamepad>/buttonNorth"),
            new Definition(AstralCommand.Tonic, "<Keyboard>/t", "<Gamepad>/dpad/down"),
            new Definition(AstralCommand.Party, "<Keyboard>/p", "<Gamepad>/dpad/up"),
            new Definition(AstralCommand.SelectActiveOne, "<Keyboard>/1", "<Gamepad>/dpad/left"),
            new Definition(AstralCommand.SelectActiveTwo, "<Keyboard>/2", "<Gamepad>/dpad/right"),
            new Definition(AstralCommand.TargetOne, "<Keyboard>/q", "<Gamepad>/leftShoulder"),
            new Definition(AstralCommand.TargetTwo, "<Keyboard>/w", "<Gamepad>/rightShoulder"),
            new Definition(AstralCommand.Attack, "<Keyboard>/a", "<Gamepad>/rightTrigger", true),
            new Definition(AstralCommand.ArcBurst, "<Keyboard>/f", "<Gamepad>/buttonWest", true),
            new Definition(AstralCommand.Guard, "<Keyboard>/g", "<Gamepad>/buttonEast", true),
            new Definition(AstralCommand.Replace, "<Keyboard>/r", "<Gamepad>/buttonNorth"),
            new Definition(AstralCommand.Swap, "<Keyboard>/s", "<Gamepad>/rightStickPress")
        };

        private static readonly AstralCommand[] Rebindable =
        {
            AstralCommand.Encounter,
            AstralCommand.Attack,
            AstralCommand.ArcBurst,
            AstralCommand.Guard
        };

        private readonly InputActionMap commandMap = new InputActionMap("Astral Commands");
        private readonly Dictionary<AstralCommand, InputAction> actions = new Dictionary<AstralCommand, InputAction>();
        private InputActionRebindingExtensions.RebindingOperation rebindOperation;
        private AstralPromptDevice promptDevice = AstralPromptDevice.KeyboardMouse;
        private int presentationVersion;
        private string rebindStatus = "Choose a command below to change its keyboard binding.";

        public AstralCommandInput()
        {
            foreach (Definition definition in Definitions)
            {
                InputAction action = commandMap.AddAction(definition.Command.ToString(), InputActionType.Button);
                action.AddBinding(new InputBinding { path = definition.KeyboardPath, groups = KeyboardGroup });
                action.AddBinding(new InputBinding { path = definition.GamepadPath, groups = GamepadGroup });
                actions.Add(definition.Command, action);
            }

            LoadPersistedOverrides();
        }

        public AstralPromptDevice PromptDevice => promptDevice;
        public string PromptDeviceLabel => promptDevice == AstralPromptDevice.Gamepad ? "Gamepad" : "Keyboard & Mouse";
        public bool IsRebinding => rebindOperation != null;
        public string RebindStatus => rebindStatus;
        public int PresentationVersion => presentationVersion;
        public IReadOnlyList<AstralCommand> RebindableCommands => Rebindable;

        public void Enable() => commandMap.Enable();

        public void Disable()
        {
            CancelRebind();
            commandMap.Disable();
        }

        public bool WasPressed(AstralCommand command)
        {
            return !IsRebinding && actions.TryGetValue(command, out InputAction action) && action.WasPressedThisFrame();
        }

        public void PollPromptDevice()
        {
            if (Keyboard.current != null && Keyboard.current.anyKey.wasPressedThisFrame)
                SetPromptDevice(AstralPromptDevice.KeyboardMouse);

            Gamepad gamepad = Gamepad.current;
            if (gamepad != null && AnyGamepadButtonPressed(gamepad))
                SetPromptDevice(AstralPromptDevice.Gamepad);
        }

        public string GetPrompt(AstralCommand command) => GetPrompt(command, promptDevice);

        public string GetPrompt(AstralCommand command, AstralPromptDevice device)
        {
            if (!actions.TryGetValue(command, out InputAction action))
                return "Unbound";

            string group = device == AstralPromptDevice.Gamepad ? GamepadGroup : KeyboardGroup;
            int index = FindBindingIndex(action, group);
            return index >= 0 ? action.GetBindingDisplayString(index) : "Unbound";
        }

        public bool BeginKeyboardRebind(AstralCommand command)
        {
            if (IsRebinding || !IsRebindable(command) || !actions.TryGetValue(command, out InputAction action))
                return false;

            int bindingIndex = FindBindingIndex(action, KeyboardGroup);
            if (bindingIndex < 0)
                return false;

            string previousOverridePath = action.bindings[bindingIndex].overridePath;
            action.Disable();
            rebindStatus = $"Press a new keyboard key for {CommandLabel(command)}. Escape cancels.";
            presentationVersion++;
            rebindOperation = action.PerformInteractiveRebinding(bindingIndex)
                .WithControlsHavingToMatchPath("<Keyboard>")
                .WithCancelingThrough("<Keyboard>/escape")
                .OnMatchWaitForAnother(0.1f)
                .OnCancel(operation => FinishRebind(operation, action, command, bindingIndex, previousOverridePath, false))
                .OnComplete(operation => FinishRebind(operation, action, command, bindingIndex, previousOverridePath, true));
            rebindOperation.Start();
            return true;
        }

        public void CancelRebind()
        {
            rebindOperation?.Cancel();
        }

        public bool TryApplyKeyboardBinding(AstralCommand command, string controlPath, bool persist, out string error)
        {
            error = string.Empty;
            if (!IsRebindable(command))
            {
                error = "That command is not rebindable.";
                return false;
            }
            if (string.IsNullOrWhiteSpace(controlPath) || !controlPath.StartsWith("<Keyboard>/", StringComparison.OrdinalIgnoreCase))
            {
                error = "Choose a keyboard key.";
                return false;
            }
            if (HasConflict(command, controlPath))
            {
                error = "That key is already assigned to another rebindable command.";
                return false;
            }

            InputAction action = actions[command];
            int bindingIndex = FindBindingIndex(action, KeyboardGroup);
            action.ApplyBindingOverride(bindingIndex, controlPath);
            rebindStatus = $"{CommandLabel(command)} is now {GetPrompt(command, AstralPromptDevice.KeyboardMouse)}.";
            presentationVersion++;
            if (persist)
                SavePersistedOverrides();
            return true;
        }

        public void ResetKeyboardBindings(bool persist = true)
        {
            foreach (AstralCommand command in Rebindable)
            {
                InputAction action = actions[command];
                int bindingIndex = FindBindingIndex(action, KeyboardGroup);
                if (bindingIndex >= 0)
                    action.RemoveBindingOverride(bindingIndex);
            }

            rebindStatus = "Keyboard bindings restored to defaults.";
            presentationVersion++;
            if (persist)
            {
                PlayerPrefs.DeleteKey(PlayerPrefsKey);
                PlayerPrefs.Save();
            }
        }

        public string SaveOverridesToJson()
        {
            var data = new BindingSaveData();
            foreach (AstralCommand command in Rebindable)
            {
                InputAction action = actions[command];
                int bindingIndex = FindBindingIndex(action, KeyboardGroup);
                data.bindings.Add(new BindingSaveEntry
                {
                    command = command.ToString(),
                    path = action.bindings[bindingIndex].effectivePath
                });
            }
            return JsonUtility.ToJson(data);
        }

        public bool TryLoadOverridesFromJson(string json)
        {
            if (string.IsNullOrWhiteSpace(json))
                return false;

            try
            {
                BindingSaveData data = JsonUtility.FromJson<BindingSaveData>(json);
                if (data?.bindings == null || data.bindings.Count != Rebindable.Length)
                {
                    ResetKeyboardBindings(false);
                    return false;
                }

                ResetKeyboardBindings(false);
                var restoredCommands = new HashSet<AstralCommand>();
                foreach (BindingSaveEntry entry in data.bindings)
                {
                    if (entry == null || !Enum.TryParse(entry.command, out AstralCommand command) ||
                        !restoredCommands.Add(command) ||
                        !TryApplyKeyboardBinding(command, entry.path, false, out _))
                    {
                        ResetKeyboardBindings(false);
                        return false;
                    }
                }
                if (!ValidateRebindableBindings())
                {
                    ResetKeyboardBindings(false);
                    return false;
                }

                presentationVersion++;
                return true;
            }
            catch (Exception exception) when (exception is ArgumentException || exception is InvalidOperationException)
            {
                ResetKeyboardBindings(false);
                return false;
            }
        }

        public static string CommandLabel(AstralCommand command)
        {
            return command switch
            {
                AstralCommand.ArcBurst => "Arc Burst",
                AstralCommand.SelectActiveOne => "Active 1",
                AstralCommand.SelectActiveTwo => "Active 2",
                AstralCommand.TargetOne => "Target 1",
                AstralCommand.TargetTwo => "Target 2",
                _ => command.ToString()
            };
        }

        public void Dispose()
        {
            CancelRebind();
            commandMap.Disable();
        }

        private void FinishRebind(
            InputActionRebindingExtensions.RebindingOperation operation,
            InputAction action,
            AstralCommand command,
            int bindingIndex,
            string previousOverridePath,
            bool completed)
        {
            string candidatePath = completed ? action.bindings[bindingIndex].effectivePath : null;
            operation.Dispose();
            rebindOperation = null;

            if (!completed)
            {
                rebindStatus = "Rebind cancelled.";
            }
            else if (HasConflict(command, candidatePath))
            {
                if (string.IsNullOrEmpty(previousOverridePath)) action.RemoveBindingOverride(bindingIndex);
                else action.ApplyBindingOverride(bindingIndex, previousOverridePath);
                rebindStatus = "That key is already used by another rebindable command. The previous binding was kept.";
            }
            else
            {
                rebindStatus = $"{CommandLabel(command)} is now {action.GetBindingDisplayString(bindingIndex)}.";
                SavePersistedOverrides();
            }

            action.Enable();
            presentationVersion++;
        }

        private bool HasConflict(AstralCommand changedCommand, string candidatePath)
        {
            if (string.IsNullOrWhiteSpace(candidatePath))
                return true;

            foreach (AstralCommand command in Rebindable)
            {
                if (command == changedCommand)
                    continue;
                InputAction action = actions[command];
                int bindingIndex = FindBindingIndex(action, KeyboardGroup);
                if (bindingIndex >= 0 && string.Equals(action.bindings[bindingIndex].effectivePath, candidatePath, StringComparison.OrdinalIgnoreCase))
                    return true;
            }
            return false;
        }

        private bool ValidateRebindableBindings()
        {
            var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (AstralCommand command in Rebindable)
            {
                InputAction action = actions[command];
                int bindingIndex = FindBindingIndex(action, KeyboardGroup);
                if (bindingIndex < 0)
                    return false;
                string path = action.bindings[bindingIndex].effectivePath;
                if (string.IsNullOrWhiteSpace(path) || !path.StartsWith("<Keyboard>/", StringComparison.OrdinalIgnoreCase) || !paths.Add(path))
                    return false;
            }
            return true;
        }

        private void LoadPersistedOverrides()
        {
            if (!PlayerPrefs.HasKey(PlayerPrefsKey))
                return;
            if (!TryLoadOverridesFromJson(PlayerPrefs.GetString(PlayerPrefsKey)))
                PlayerPrefs.DeleteKey(PlayerPrefsKey);
        }

        private void SavePersistedOverrides()
        {
            PlayerPrefs.SetString(PlayerPrefsKey, SaveOverridesToJson());
            PlayerPrefs.Save();
        }

        private void SetPromptDevice(AstralPromptDevice device)
        {
            if (promptDevice == device)
                return;
            promptDevice = device;
            presentationVersion++;
        }

        private static bool IsRebindable(AstralCommand command)
        {
            for (int i = 0; i < Rebindable.Length; i++)
                if (Rebindable[i] == command)
                    return true;
            return false;
        }

        private static int FindBindingIndex(InputAction action, string group)
        {
            for (int i = 0; i < action.bindings.Count; i++)
            {
                string groups = action.bindings[i].groups;
                if (!string.IsNullOrEmpty(groups) && groups.IndexOf(group, StringComparison.OrdinalIgnoreCase) >= 0)
                    return i;
            }
            return -1;
        }

        private static bool AnyGamepadButtonPressed(Gamepad gamepad)
        {
            return gamepad.buttonSouth.wasPressedThisFrame || gamepad.buttonNorth.wasPressedThisFrame ||
                   gamepad.buttonEast.wasPressedThisFrame || gamepad.buttonWest.wasPressedThisFrame ||
                   gamepad.leftShoulder.wasPressedThisFrame || gamepad.rightShoulder.wasPressedThisFrame ||
                   gamepad.leftTrigger.wasPressedThisFrame || gamepad.rightTrigger.wasPressedThisFrame ||
                   gamepad.startButton.wasPressedThisFrame || gamepad.selectButton.wasPressedThisFrame ||
                   gamepad.leftStickButton.wasPressedThisFrame || gamepad.rightStickButton.wasPressedThisFrame ||
                   gamepad.dpad.up.wasPressedThisFrame || gamepad.dpad.down.wasPressedThisFrame ||
                   gamepad.dpad.left.wasPressedThisFrame || gamepad.dpad.right.wasPressedThisFrame;
        }
    }
}
