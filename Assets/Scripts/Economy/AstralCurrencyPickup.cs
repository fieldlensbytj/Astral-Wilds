using UnityEngine;

namespace AstralWilds
{
    public sealed class AstralCurrencyPickup : MonoBehaviour
    {
        [SerializeField] private string pickupId = "starshard-cache";
        [SerializeField, Min(1)] private int amount = 25;
        [SerializeField, Min(0.5f)] private float collectionRadius = 1.5f;

        private AstralDemoLoopController controller;
        private AstralPlayerController player;

        public string PickupId => string.IsNullOrWhiteSpace(pickupId) ? name : pickupId;
        public int Amount => amount;
        public bool IsCollected { get; private set; }

        private void Awake()
        {
            ResolveReferences();
        }

        private void OnEnable()
        {
            ResolveReferences();
        }

        private void Update()
        {
            if (controller == null || player == null)
                ResolveReferences();
            if (controller == null || player == null)
                return;

            TryCollectIfInRange(controller, player.transform.position);
        }

        public bool TryCollectIfInRange(AstralDemoLoopController targetController, Vector3 playerPosition)
        {
            if (IsCollected || targetController == null || !targetController.IsExploration)
                return false;

            Vector3 delta = playerPosition - transform.position;
            if (delta.sqrMagnitude > collectionRadius * collectionRadius)
                return false;

            if (!targetController.TryCollectExplorationCurrency(PickupId, amount))
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
