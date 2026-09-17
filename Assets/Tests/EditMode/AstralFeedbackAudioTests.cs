using NUnit.Framework;
using UnityEngine;

namespace AstralWilds.Tests
{
    public sealed class AstralFeedbackAudioTests
    {
        [Test]
        public void Awake_ConfiguresSingleReusableTwoDimensionalSource()
        {
            var gameObject = new GameObject("FeedbackAudioTest");
            try
            {
                AstralFeedbackAudio feedback = gameObject.AddComponent<AstralFeedbackAudio>();
                feedback.EnsureConfigured();

                Assert.That(feedback.IsConfigured, Is.True);
                Assert.That(feedback.CachedCueCount, Is.EqualTo(7));
                Assert.That(gameObject.GetComponents<AudioSource>(), Has.Length.EqualTo(1));
                Assert.That(feedback.Source.playOnAwake, Is.False);
                Assert.That(feedback.Source.loop, Is.False);
                Assert.That(feedback.Source.spatialBlend, Is.Zero);
                Assert.That(feedback.Source.ignoreListenerPause, Is.True);
                Assert.DoesNotThrow(() => feedback.Play(AstralFeedbackCue.Confirm));
            }
            finally
            {
                Object.DestroyImmediate(gameObject);
            }
        }
    }
}
