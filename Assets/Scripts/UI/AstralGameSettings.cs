using System;
using UnityEngine;

namespace AstralWilds
{
    /// <summary>Validated local presentation preferences; contains no progression state.</summary>
    [Serializable]
    public sealed class AstralGameSettings
    {
        private static readonly int[] PercentSteps = { 0, 25, 50, 75, 100 };
        private static readonly int[] SensitivitySteps = { 50, 75, 100, 125, 150 };

        [SerializeField] private int volumeStep = 4;
        [SerializeField] private int sensitivityStep = 2;

        public int VolumePercent => PercentSteps[volumeStep];
        public int LookSensitivityPercent => SensitivitySteps[sensitivityStep];
        public float Volume => VolumePercent / 100f;
        public float LookSensitivityScale => LookSensitivityPercent / 100f;

        public void CycleVolume() => volumeStep = (volumeStep + 1) % PercentSteps.Length;
        public void CycleLookSensitivity() => sensitivityStep = (sensitivityStep + 1) % SensitivitySteps.Length;

        public bool TryRestore(int savedVolumeStep, int savedSensitivityStep)
        {
            if (savedVolumeStep < 0 || savedVolumeStep >= PercentSteps.Length ||
                savedSensitivityStep < 0 || savedSensitivityStep >= SensitivitySteps.Length)
                return false;

            volumeStep = savedVolumeStep;
            sensitivityStep = savedSensitivityStep;
            return true;
        }

        public string ToJson() => JsonUtility.ToJson(this);

        public bool TryRestoreJson(string json)
        {
            if (string.IsNullOrWhiteSpace(json))
                return false;

            try
            {
                AstralGameSettings restored = JsonUtility.FromJson<AstralGameSettings>(json);
                return restored != null && TryRestore(restored.volumeStep, restored.sensitivityStep);
            }
            catch (ArgumentException)
            {
                return false;
            }
        }
    }

    public static class AstralGameSettingsStore
    {
        private const string PlayerPrefsKey = "AstralWilds.Settings.v1";

        public static void LoadInto(AstralGameSettings settings)
        {
            if (settings == null || !PlayerPrefs.HasKey(PlayerPrefsKey))
                return;

            settings.TryRestoreJson(PlayerPrefs.GetString(PlayerPrefsKey));
        }

        public static void Save(AstralGameSettings settings)
        {
            if (settings == null)
                return;

            PlayerPrefs.SetString(PlayerPrefsKey, settings.ToJson());
            PlayerPrefs.Save();
        }
    }
}
