using System;

namespace AstralWilds
{
    /// <summary>
    /// Lightweight HP/identity record used by the demo loop's party, reserve, and
    /// opponent slots. Extracted from AstralDemoLoopController so it can be shared
    /// with AstralBattleEngine and referenced directly by tests, without needing a
    /// live MonoBehaviour/scene context.
    /// </summary>
    [Serializable]
    public sealed class AstralDemoMember
    {
        public string id;
        public string displayName;
        public int hp = 30;
        public int maxHp = 30;
        public bool defeated;
    }
}
