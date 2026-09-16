using UnityEngine;

namespace AstralWilds
{
    /// <summary>A one-time world find that grants an inventory item through exploration.</summary>
    public sealed class AstralItemPickup : MonoBehaviour
    {
        [SerializeField] private string pickupId = "salvage-find";
        [SerializeField] private AstralItemId item = AstralItemId.SalvagedAlloy;
        [SerializeField, Min(1)] private int quantity = 1;
        [SerializeField, Min(0.5f)] private float collectionRadius = 1.5f;

        private AstralDemoLoopController controller;
        private AstralPlayerController player;

        public string PickupId => string.IsNullOrWhiteSpace(pickupId) ? name : pickupId;
        public AstralItemId Item => item;
        public int Quantity => quantity;
        public bool IsCollected { get; private set; }

        private void Awake() => ResolveReferences();
        private void OnEnable() => ResolveReferences();

        private void Update()
        {
            if (controller == null || player == null)
                ResolveReferences();
            if (controller != null && player != null)
                TryCollectIfInRange(controller, player.transform.position);
        }

        public bool TryCollectIfInRange(AstralDemoLoopController targetController, Vector3 playerPosition)
        {
            if (IsCollected || targetController == null || !targetController.IsExploration)
                return false;

            Vector3 delta = playerPosition - transform.position;
            if (delta.sqrMagnitude > collectionRadius * collectionRadius ||
                !targetController.TryCollectExplorationItem(PickupId, item, quantity))
                return false;

            SetCollected(true);
            return true;
        }

        public void SetCollected(bool collected)
        {
            IsCollected = collected;
            gameObject.SetActive(!collected);
        }

        private void ResolveReferences()
        {
            controller = FindAnyObjectByType<AstralDemoLoopController>();
            player = FindAnyObjectByType<AstralPlayerController>();
        }
    }
}
