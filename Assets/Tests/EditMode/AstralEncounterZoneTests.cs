using NUnit.Framework;
using UnityEngine;

namespace AstralWilds.Tests
{
    public sealed class AstralEncounterZoneTests
    {
        private GameObject zoneObject;
        private AstralEncounterZone zone;

        [SetUp]
        public void SetUp()
        {
            zoneObject = new GameObject("EncounterZoneTest");
            zoneObject.transform.position = new Vector3(10f, 2f, -4f);
            zone = zoneObject.AddComponent<AstralEncounterZone>();
        }

        [TearDown]
        public void TearDown()
        {
            Object.DestroyImmediate(zoneObject);
        }

        [Test]
        public void ContainsAcceptsPointInsideAndRejectsPointOutside()
        {
            Assert.IsTrue(zone.Contains(new Vector3(12f, 2f, -4f)));
            Assert.IsFalse(zone.Contains(new Vector3(14f, 2f, -4f)));
        }

        [Test]
        public void ContainsAccountsForWorldScale()
        {
            zoneObject.transform.localScale = new Vector3(2f, 1f, 1f);
            Assert.IsTrue(zone.Contains(new Vector3(16.5f, 2f, -4f)));
            Assert.IsFalse(zone.Contains(new Vector3(17.5f, 2f, -4f)));
        }

        [Test]
        public void ColliderIsConfiguredAsTrigger()
        {
            SphereCollider collider = zoneObject.GetComponent<SphereCollider>();
            Assert.IsTrue(collider.isTrigger);
            Assert.AreEqual(zone.Radius, collider.radius);
        }
    }
}
