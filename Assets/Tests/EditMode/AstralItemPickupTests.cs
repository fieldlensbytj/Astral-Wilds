using NUnit.Framework;
using UnityEngine;

namespace AstralWilds.Tests
{
    public sealed class AstralItemPickupTests
    {
        private GameObject controllerObject;
        private GameObject pickupObject;

        [TearDown]
        public void TearDown()
        {
            if (pickupObject != null) Object.DestroyImmediate(pickupObject);
            if (controllerObject != null) Object.DestroyImmediate(controllerObject);
        }

        [Test]
        public void Pickup_RequiresRangeThenGrantsItemOnce()
        {
            controllerObject = new GameObject("Controller");
            var controller = controllerObject.AddComponent<AstralDemoLoopController>();
            pickupObject = new GameObject("Salvage");
            var pickup = pickupObject.AddComponent<AstralItemPickup>();
            pickupObject.transform.position = new Vector3(4f, 0f, 0f);

            Assert.That(pickup.TryCollectIfInRange(controller, Vector3.zero), Is.False);
            Assert.That(controller.SalvagedAlloy, Is.Zero);
            Assert.That(pickup.TryCollectIfInRange(controller, pickupObject.transform.position), Is.True);
            Assert.That(controller.SalvagedAlloy, Is.EqualTo(1));
            Assert.That(pickup.IsCollected, Is.True);
            Assert.That(pickupObject.activeSelf, Is.False);
            Assert.That(pickup.TryCollectIfInRange(controller, pickupObject.transform.position), Is.False);
            Assert.That(controller.SalvagedAlloy, Is.EqualTo(1));
        }

        [Test]
        public void DuplicateStableId_IsRejectedWithoutAddingAnotherItem()
        {
            controllerObject = new GameObject("Controller");
            var controller = controllerObject.AddComponent<AstralDemoLoopController>();
            pickupObject = new GameObject("SalvageA");
            var first = pickupObject.AddComponent<AstralItemPickup>();
            var duplicateObject = new GameObject("SalvageB");
            var duplicate = duplicateObject.AddComponent<AstralItemPickup>();

            try
            {
                Assert.That(first.TryCollectIfInRange(controller, Vector3.zero), Is.True);
                Assert.That(duplicate.TryCollectIfInRange(controller, Vector3.zero), Is.False);
                Assert.That(controller.SalvagedAlloy, Is.EqualTo(1));
                Assert.That(duplicateObject.activeSelf, Is.True);
            }
            finally
            {
                Object.DestroyImmediate(duplicateObject);
            }
        }
    }
}
