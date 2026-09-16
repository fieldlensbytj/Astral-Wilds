using NUnit.Framework;
using UnityEngine;

namespace AstralWilds.Tests
{
    public sealed class AstralExplorationGuidanceTests
    {
        [TestCase(0f, 1f, "north")]
        [TestCase(1f, 1f, "northeast")]
        [TestCase(1f, 0f, "east")]
        [TestCase(-1f, -1f, "southwest")]
        [TestCase(-1f, 1f, "northwest")]
        public void DescribeDirectionUsesReadableWorldDirections(float x, float z, string expected)
        {
            Assert.AreEqual(expected, AstralDemoLoopController.DescribeDirection(new Vector3(x, 0f, z)));
        }

        [Test]
        public void DescribeDirectionTreatsNearbyTargetAsHere()
        {
            Assert.AreEqual("here", AstralDemoLoopController.DescribeDirection(new Vector3(0.2f, 20f, 0.2f)));
        }
    }
}
