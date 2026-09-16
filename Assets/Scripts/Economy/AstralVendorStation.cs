using UnityEngine;

namespace AstralWilds
{
    /// <summary>A world-space station that exposes earned-currency trades nearby.</summary>
    [RequireComponent(typeof(SphereCollider))]
    public sealed class AstralVendorStation : MonoBehaviour
    {
        [SerializeField] private string displayName = "Wayfarer Supply Relay";
        [SerializeField, Min(0.5f)] private float interactionRadius = 3f;

        private SphereCollider interactionCollider;

        public string DisplayName => string.IsNullOrWhiteSpace(displayName) ? "Supply Relay" : displayName;

        private void Awake() => ApplyColliderSettings();

        private void OnValidate()
        {
            interactionRadius = Mathf.Max(0.5f, interactionRadius);
            ApplyColliderSettings();
        }

        public bool Contains(Vector3 worldPosition)
        {
            EnsureCollider();
            if (interactionCollider == null || !isActiveAndEnabled || !interactionCollider.enabled)
                return false;

            Vector3 center = transform.TransformPoint(interactionCollider.center);
            Vector3 scale = transform.lossyScale;
            float scaleFactor = Mathf.Max(Mathf.Abs(scale.x), Mathf.Abs(scale.y), Mathf.Abs(scale.z));
            float worldRadius = interactionCollider.radius * scaleFactor;
            return (worldPosition - center).sqrMagnitude <= worldRadius * worldRadius;
        }

        private void ApplyColliderSettings()
        {
            EnsureCollider();
            if (interactionCollider == null)
                return;

            interactionCollider.isTrigger = true;
            interactionCollider.radius = interactionRadius;
        }

        private void EnsureCollider()
        {
            if (interactionCollider == null)
                interactionCollider = GetComponent<SphereCollider>();
        }
    }
}
