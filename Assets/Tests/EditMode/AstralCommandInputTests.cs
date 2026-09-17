using NUnit.Framework;

namespace AstralWilds.Tests
{
    public sealed class AstralCommandInputTests
    {
        [Test]
        public void Defaults_ExposeKeyboardAndGamepadPrompts()
        {
            using var input = new AstralCommandInput();
            input.ResetKeyboardBindings(false);

            Assert.That(input.GetPrompt(AstralCommand.Attack, AstralPromptDevice.KeyboardMouse), Is.Not.EqualTo("Unbound"));
            Assert.That(input.GetPrompt(AstralCommand.Attack, AstralPromptDevice.Gamepad), Is.Not.EqualTo("Unbound"));
            Assert.That(input.GetPrompt(AstralCommand.Guard, AstralPromptDevice.Gamepad), Is.Not.EqualTo("Unbound"));
            Assert.That(input.RebindableCommands, Is.EquivalentTo(new[]
            {
                AstralCommand.Encounter,
                AstralCommand.Attack,
                AstralCommand.ArcBurst,
                AstralCommand.Guard
            }));
        }

        [Test]
        public void KeyboardOverride_RoundTripsThroughValidatedJson()
        {
            using var source = new AstralCommandInput();
            source.ResetKeyboardBindings(false);
            Assert.That(source.TryApplyKeyboardBinding(AstralCommand.Attack, "<Keyboard>/z", false, out string error), Is.True, error);
            string json = source.SaveOverridesToJson();

            using var restored = new AstralCommandInput();
            restored.ResetKeyboardBindings(false);
            Assert.That(restored.TryLoadOverridesFromJson(json), Is.True);
            Assert.That(restored.GetPrompt(AstralCommand.Attack, AstralPromptDevice.KeyboardMouse), Does.Contain("Z").IgnoreCase);
        }

        [Test]
        public void DuplicateRebind_IsRejectedWithoutMutation()
        {
            using var input = new AstralCommandInput();
            input.ResetKeyboardBindings(false);
            string original = input.GetPrompt(AstralCommand.Attack, AstralPromptDevice.KeyboardMouse);

            Assert.That(input.TryApplyKeyboardBinding(AstralCommand.Attack, "<Keyboard>/f", false, out string error), Is.False);
            Assert.That(error, Does.Contain("already assigned"));
            Assert.That(input.GetPrompt(AstralCommand.Attack, AstralPromptDevice.KeyboardMouse), Is.EqualTo(original));
        }

        [TestCase(AstralCommand.Replace, "<Keyboard>/z")]
        [TestCase(AstralCommand.Attack, "<Gamepad>/buttonSouth")]
        [TestCase(AstralCommand.Attack, "")]
        public void InvalidOverride_IsRejected(AstralCommand command, string path)
        {
            using var input = new AstralCommandInput();
            input.ResetKeyboardBindings(false);

            Assert.That(input.TryApplyKeyboardBinding(command, path, false, out string error), Is.False);
            Assert.That(error, Is.Not.Empty);
        }
    }
}
