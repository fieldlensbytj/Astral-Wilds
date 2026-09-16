using UnityEngine;
using UnityEngine.InputSystem;

namespace AstralWilds
{
    [RequireComponent(typeof(CharacterController))]
    public sealed class AstralPlayerController : MonoBehaviour
    {
        [Header("Input")]
        [SerializeField] private InputActionAsset inputActions;
        [SerializeField] private string actionMapName = "Player";
        [SerializeField] private string moveActionName = "Move";
        [SerializeField] private string sprintActionName = "Sprint";
        [SerializeField] private string jumpActionName = "Jump";

        [Header("Movement")]
        [SerializeField, Min(0f)] private float moveSpeed = 4.5f;
        [SerializeField, Min(1f)] private float sprintMultiplier = 1.6f;
        [SerializeField, Min(0f)] private float rotationSharpness = 12f;
        [SerializeField] private float gravity = -25f;
        [SerializeField, Min(0f)] private float jumpHeight = 1.4f;

        [Header("References")]
        [SerializeField] private Transform cameraTransform;

        private CharacterController characterController;
        private InputActionMap playerActions;
        private InputAction moveAction;
        private InputAction sprintAction;
        private InputAction jumpAction;
        private float verticalVelocity;
        public bool InputBlocked { get; set; }

        private void Awake()
        {
            characterController = GetComponent<CharacterController>();

            if (cameraTransform == null && Camera.main != null)
                cameraTransform = Camera.main.transform;

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

        private void Update()
        {
            Vector2 input = !InputBlocked && moveAction != null ? moveAction.ReadValue<Vector2>() : Vector2.zero;
            Vector3 moveDirection = GetCameraRelativeDirection(input);
            float speed = moveSpeed;

            if (sprintAction != null && sprintAction.IsPressed())
                speed *= sprintMultiplier;

            if (moveDirection.sqrMagnitude > 0.001f)
            {
                Quaternion targetRotation = Quaternion.LookRotation(moveDirection, Vector3.up);
                float rotationBlend = 1f - Mathf.Exp(-rotationSharpness * Time.deltaTime);
                transform.rotation = Quaternion.Slerp(transform.rotation, targetRotation, rotationBlend);
            }

            if (characterController.isGrounded && verticalVelocity < 0f)
                verticalVelocity = -2f;

            if (!InputBlocked && jumpAction != null && jumpAction.WasPressedThisFrame() && characterController.isGrounded)
                verticalVelocity = Mathf.Sqrt(jumpHeight * -2f * gravity);

            verticalVelocity += gravity * Time.deltaTime;

            Vector3 velocity = moveDirection * speed;
            velocity.y = verticalVelocity;
            characterController.Move(velocity * Time.deltaTime);
        }

        private Vector3 GetCameraRelativeDirection(Vector2 input)
        {
            Vector3 forward = cameraTransform != null ? cameraTransform.forward : Vector3.forward;
            Vector3 right = cameraTransform != null ? cameraTransform.right : Vector3.right;
            forward.y = 0f;
            right.y = 0f;
            forward.Normalize();
            right.Normalize();

            Vector3 direction = forward * input.y + right * input.x;
            return direction.sqrMagnitude > 1f ? direction.normalized : direction;
        }

        private bool TryResolveActions()
        {
            if (inputActions == null)
            {
                Debug.LogError($"{nameof(AstralPlayerController)} requires an InputActionAsset.", this);
                return false;
            }

            playerActions = inputActions.FindActionMap(actionMapName, true);
            moveAction = playerActions.FindAction(moveActionName, true);
            sprintAction = playerActions.FindAction(sprintActionName, true);
            jumpAction = playerActions.FindAction(jumpActionName, true);
            return true;
        }
    }
}
