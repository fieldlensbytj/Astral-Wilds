using NUnit.Framework;

namespace AstralWilds.Tests
{
    public sealed class AstralGameSettingsTests
    {
        [Test]
        public void Defaults_AreFullVolumeAndStandardSensitivity()
        {
            var settings = new AstralGameSettings();

            Assert.That(settings.VolumePercent, Is.EqualTo(100));
            Assert.That(settings.LookSensitivityPercent, Is.EqualTo(100));
        }

        [Test]
        public void Cycles_WrapAcrossBoundedOptions()
        {
            var settings = new AstralGameSettings();
            settings.CycleVolume();
            settings.CycleLookSensitivity();

            Assert.That(settings.VolumePercent, Is.Zero);
            Assert.That(settings.LookSensitivityPercent, Is.EqualTo(125));
            for (int i = 0; i < 4; i++) settings.CycleVolume();
            for (int i = 0; i < 4; i++) settings.CycleLookSensitivity();
            Assert.That(settings.VolumePercent, Is.EqualTo(100));
            Assert.That(settings.LookSensitivityPercent, Is.EqualTo(100));
        }

        [Test]
        public void JsonRoundTrip_PreservesValidatedOptions()
        {
            var source = new AstralGameSettings();
            source.TryRestore(2, 4);
            var restored = new AstralGameSettings();

            Assert.That(restored.TryRestoreJson(source.ToJson()), Is.True);
            Assert.That(restored.VolumePercent, Is.EqualTo(50));
            Assert.That(restored.LookSensitivityPercent, Is.EqualTo(150));
        }

        [Test]
        public void InvalidRestore_DoesNotMutateDefaults()
        {
            var settings = new AstralGameSettings();

            Assert.That(settings.TryRestore(-1, 8), Is.False);
            Assert.That(settings.TryRestoreJson("{\"volumeStep\":99,\"sensitivityStep\":2}"), Is.False);
            Assert.That(settings.VolumePercent, Is.EqualTo(100));
            Assert.That(settings.LookSensitivityPercent, Is.EqualTo(100));
        }
    }
}
