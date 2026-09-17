using System.Collections.Generic;
using UnityEngine;

namespace AstralWilds
{
    public enum AstralFeedbackCue
    {
        Confirm,
        Cancel,
        Combat,
        Guard,
        Pickup,
        Reward,
        Denied
    }

    /// <summary>
    /// Small self-contained feedback layer. It synthesizes and caches short tones once,
    /// then reuses one non-spatial AudioSource for every transient cue.
    /// </summary>
    [DisallowMultipleComponent]
    public sealed class AstralFeedbackAudio : MonoBehaviour
    {
        private const int SampleRate = 24000;
        private readonly Dictionary<AstralFeedbackCue, AudioClip> clips = new Dictionary<AstralFeedbackCue, AudioClip>();
        private AudioSource source;

        public bool IsConfigured => source != null && clips.Count == 7;
        public int CachedCueCount => clips.Count;
        public AudioSource Source => source;

        private void Awake()
        {
            EnsureConfigured();
        }

        public void EnsureConfigured()
        {
            if (IsConfigured)
                return;
            source = GetComponent<AudioSource>();
            if (source == null)
                source = gameObject.AddComponent<AudioSource>();
            source.playOnAwake = false;
            source.loop = false;
            source.spatialBlend = 0f;
            source.ignoreListenerPause = true;
            source.volume = 0.55f;

            clips[AstralFeedbackCue.Confirm] = CreateTone("UI Confirm", 620f, 820f, 0.09f, 0.24f);
            clips[AstralFeedbackCue.Cancel] = CreateTone("UI Cancel", 420f, 300f, 0.10f, 0.20f);
            clips[AstralFeedbackCue.Combat] = CreateTone("Combat Impact", 170f, 90f, 0.12f, 0.38f, true);
            clips[AstralFeedbackCue.Guard] = CreateTone("Guard", 260f, 210f, 0.14f, 0.30f);
            clips[AstralFeedbackCue.Pickup] = CreateTone("Pickup", 760f, 1120f, 0.12f, 0.24f);
            clips[AstralFeedbackCue.Reward] = CreateTone("Reward", 520f, 1040f, 0.20f, 0.28f);
            clips[AstralFeedbackCue.Denied] = CreateTone("Denied", 190f, 160f, 0.11f, 0.24f, true);
        }

        public void Play(AstralFeedbackCue cue)
        {
            EnsureConfigured();
            if (source != null && clips.TryGetValue(cue, out AudioClip clip) && clip != null)
                source.PlayOneShot(clip);
        }

        private void OnDestroy()
        {
            foreach (AudioClip clip in clips.Values)
            {
                if (clip == null)
                    continue;
                if (Application.isPlaying) Destroy(clip);
                else DestroyImmediate(clip);
            }
            clips.Clear();
        }

        private static AudioClip CreateTone(string name, float startFrequency, float endFrequency, float duration, float gain, bool addNoise = false)
        {
            int sampleCount = Mathf.Max(1, Mathf.CeilToInt(duration * SampleRate));
            var samples = new float[sampleCount];
            uint noiseState = 0x9E3779B9u;
            float phase = 0f;
            for (int i = 0; i < sampleCount; i++)
            {
                float progress = i / (float)sampleCount;
                float frequency = Mathf.Lerp(startFrequency, endFrequency, progress);
                phase += 2f * Mathf.PI * frequency / SampleRate;
                float attack = Mathf.Clamp01(progress / 0.08f);
                float release = 1f - Mathf.SmoothStep(0f, 1f, progress);
                float value = Mathf.Sin(phase);
                if (addNoise)
                {
                    noiseState = noiseState * 1664525u + 1013904223u;
                    float noise = ((noiseState >> 8) / 16777215f) * 2f - 1f;
                    value = value * 0.78f + noise * 0.22f;
                }
                samples[i] = value * attack * release * gain;
            }

            AudioClip clip = AudioClip.Create(name, sampleCount, 1, SampleRate, false);
            clip.SetData(samples, 0);
            return clip;
        }
    }
}
