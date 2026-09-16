using UnityEngine;

namespace AstralWilds
{
    /// <summary>
    /// Defines a discoverable place in the world where a wild encounter can begin.
    /// Eligibility uses the collider's world-space bounds so it remains deterministic
    /// even when physics trigger callbacks are delayed or the player starts inside it.
    /// </summary>
    [RequireComponent(typeof(SphereCollider))]
    public sealed class AstralEncounterZone : MonoBehaviour
    {
        [SerializeField] private string displayName = "Wild Astral activity";
        [SerializeField, Min(0.5f)] private float radius = 3.5f;

        private SphereCollider zoneCollider;

        public string DisplayName => string.IsNullOrWhiteSpace(displayName) ? "Wild Astral activity" : displayName;
        public float Radius => radius;

        private void Awake()
        {
            ApplyColliderSettings();
        }

        private void OnValidate()
        {
            radius = Mathf.Max(0.5f, radius);
            ApplyColliderSettings();
        }

        public bool Contains(Vector3 worldPosition)
        {
            EnsureCollider();
            if (zoneCollider == null || !isActiveAndEnabled || !zoneCollider.enabled)
                return false;

            Vector3 center = transform.TransformPoint(zoneCollider.center);
            Vector3 scale = transform.lossyScale;
            float scaleFactor = Mathf.Max(Mathf.Abs(scale.x), Mathf.Abs(scale.y), Mathf.Abs(scale.z));
            float worldRadius = zoneCollider.radius * scaleFactor;
            return (worldPosition - center).sqrMagnitude <= worldRadius * worldRadius;
        }

        private void ApplyColliderSettings()
        {
            EnsureCollider();
            if (zoneCollider == null)
                return;

            zoneCollider.isTrigger = true;
            zoneCollider.radius = radius;
        }

        private void EnsureCollider()
        {
            if (zoneCollider == null)
                zoneCollider = GetComponent<SphereCollider>();
        }
    }
}
