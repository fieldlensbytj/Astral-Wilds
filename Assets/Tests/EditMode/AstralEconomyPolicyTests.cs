using System.IO;
using NUnit.Framework;
using UnityEngine;

namespace AstralWilds.Tests
{
    public sealed class AstralEconomyPolicyTests
    {
        [Test]
        public void ProjectManifestContainsNoPurchasingOrAdMonetizationPackages()
        {
            string projectRoot = Path.GetFullPath(Path.Combine(Application.dataPath, ".."));
            string manifest = File.ReadAllText(Path.Combine(projectRoot, "Packages", "manifest.json"));
            string[] forbiddenPackageIds =
            {
                "com.unity.purchasing",
                "com.unity.ads",
                "com.unity.services.levelplay",
                "com.unity.mediation",
                "googlemobileads"
            };

            foreach (string packageId in forbiddenPackageIds)
                StringAssert.DoesNotContain(packageId, manifest, $"Real-money or ad monetization package is forbidden: {packageId}");
        }
    }
}
