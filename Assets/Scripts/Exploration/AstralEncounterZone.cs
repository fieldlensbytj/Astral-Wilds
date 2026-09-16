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
        [Header("Identity")]
        [SerializeField] private string zoneId = "wild-activity";
        [SerializeField] private string displayName = "Wild Astral activity";
        [SerializeField] private string primaryAstralId = "wild-ember";
        [SerializeField] private string primaryAstralName = "Wild Ember Astral";
        [SerializeField] private string companionAstralId = "wild-frost";
        [SerializeField] private string companionAstralName = "Wild Frost Astral";

        [Header("Bounds")]
        [SerializeField, Min(0.5f)] private float radius = 3.5f;

        private SphereCollider zoneCollider;

        public string ZoneId => string.IsNullOrWhiteSpace(zoneId) ? name : zoneId;
        public string DisplayName => string.IsNullOrWhiteSpace(displayName) ? "Wild Astral activity" : displayName;
        public string PrimaryAstralId => primaryAstralId;
        public string PrimaryAstralName => primaryAstralName;
        public string CompanionAstralId => companionAstralId;
        public string CompanionAstralName => companionAstralName;
        public float Radius => radius;
        public bool IsCleared { get; private set; }
        public bool IsAvailable => isActiveAndEnabled && !IsCleared;

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

        public void SetCleared(bool cleared)
        {
            IsCleared = cleared;
            foreach (Renderer zoneRenderer in GetComponentsInChildren<Renderer>(true))
                zoneRenderer.enabled = !cleared;
            foreach (Light zoneLight in GetComponentsInChildren<Light>(true))
                zoneLight.enabled = !cleared;
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
