using UnityEngine;
using UnityEngine.InputSystem;

namespace AstralWilds
{
    [RequireComponent(typeof(Camera))]
    public sealed class AstralThirdPersonCamera : MonoBehaviour
    {
        [Header("Input")]
        [SerializeField] private InputActionAsset inputActions;
        [SerializeField] private string actionMapName = "Player";
        [SerializeField] private string lookActionName = "Look";

        [Header("Target")]
        [SerializeField] private Transform target;
        [SerializeField, Min(0f)] private float targetHeight = 1.4f;
        [SerializeField] private float shoulderOffset = 0.45f;

        [Header("Orbit")]
        [SerializeField, Min(0.5f)] private float distance = 5.5f;
        [SerializeField] private float startingPitch = 12f;
        [SerializeField] private float minPitch = -20f;
        [SerializeField] private float maxPitch = 65f;
        [SerializeField, Min(0f)] private float mouseSensitivity = 0.08f;
        [SerializeField, Min(0f)] private float stickSensitivity = 180f;

        [Header("Follow")]
        [SerializeField, Min(0f)] private float positionSmoothTime = 0.08f;
        [SerializeField, Min(0f)] private float collisionRadius = 0.2f;
        [SerializeField, Min(0f)] private float collisionBuffer = 0.1f;
        [SerializeField] private LayerMask collisionMask = ~0;

        private Camera sceneCamera;
        private InputActionMap playerActions;
        private InputAction lookAction;
        private Vector3 positionVelocity;
        private float yaw;
        private float pitch;
        public bool InputBlocked { get; set; }
        public float SensitivityScale { get; set; } = 1f;

        private void Awake()
        {
            sceneCamera = GetComponent<Camera>();
            yaw = target != null ? target.eulerAngles.y : transform.eulerAngles.y;
            pitch = startingPitch;
            TryResolveActions();
        }

        private void OnEnable()
        {
            if (TryResolveActions())
                playerActions.Enable();
        }

        private void OnDisable()
        {
            if (playerActions != null)
                playerActions.Disable();
        }

        private void LateUpdate()
        {
            if (target == null)
                return;

            UpdateOrbit();

            Quaternion orbitRotation = Quaternion.Euler(pitch, yaw, 0f);
            Vector3 pivot = target.position + Vector3.up * targetHeight + target.right * shoulderOffset;
            Vector3 desiredPosition = pivot - orbitRotation * Vector3.forward * distance;
            desiredPosition = ResolveObstruction(pivot, desiredPosition);

            if (positionSmoothTime > 0f)
                transform.position = Vector3.SmoothDamp(transform.position, desiredPosition, ref positionVelocity, positionSmoothTime);
            else
                transform.position = desiredPosition;

            transform.rotation = orbitRotation;
        }

        private void UpdateOrbit()
        {
            if (InputBlocked || lookAction == null)
                return;

            Vector2 lookInput = lookAction.ReadValue<Vector2>();
            bool isPointerInput = lookAction.activeControl != null && lookAction.activeControl.device is Pointer;
            float scale = (isPointerInput ? mouseSensitivity : stickSensitivity * Time.unscaledDeltaTime) *
                          Mathf.Max(0.1f, SensitivityScale);

            yaw += lookInput.x * scale;
            pitch = Mathf.Clamp(pitch - lookInput.y * scale, minPitch, maxPitch);
        }

        private Vector3 ResolveObstruction(Vector3 pivot, Vector3 desiredPosition)
        {
            Vector3 cameraRay = desiredPosition - pivot;
            float rayLength = cameraRay.magnitude;
            if (rayLength <= 0.001f)
                return desiredPosition;

            Vector3 direction = cameraRay / rayLength;
            if (Physics.SphereCast(pivot, collisionRadius, direction, out RaycastHit hit, rayLength, collisionMask, QueryTriggerInteraction.Ignore)
                && !hit.transform.IsChildOf(target))
            {
                return pivot + direction * Mathf.Max(0.1f, hit.distance - collisionBuffer);
            }

            return desiredPosition;
        }

        private bool TryResolveActions()
        {
            if (inputActions == null)
            {
                Debug.LogError($"{nameof(AstralThirdPersonCamera)} requires an InputActionAsset.", this);
                return false;
            }

            playerActions = inputActions.FindActionMap(actionMapName, true);
            lookAction = playerActions.FindAction(lookActionName, true);
            return true;
        }

        private void OnValidate()
        {
            maxPitch = Mathf.Max(minPitch, maxPitch);
            distance = Mathf.Max(0.5f, distance);
        }
    }
}
