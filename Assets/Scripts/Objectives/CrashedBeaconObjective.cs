using UnityEngine;
using UnityEngine.InputSystem;

namespace AstralWilds
{
    public sealed class CrashedBeaconObjective : MonoBehaviour
    {
        public enum BeaconState
        {
            Dormant,
            Activating,
            Activated
        }

        [Header("References")]
        [SerializeField] private InputActionAsset inputActions;
        [SerializeField] private Transform player;
        [SerializeField] private BeaconObjectiveUI objectiveUI;
        [SerializeField] private Light activationLight;
        [SerializeField] private Transform beaconCore;

        [Header("Interaction")]
        [SerializeField, Min(0.5f)] private float interactionRadius = 3f;
        [SerializeField, Min(0.1f)] private float activationDuration = 2.5f;

        [Header("Activation Visuals")]
        [SerializeField, Min(0f)] private float activatedLightIntensity = 6f;
        [SerializeField, Min(1f)] private float activatedCoreScale = 1.35f;

        private const string ActionMapName = "Player";
        private const string InteractActionName = "Interact";

        private InputActionMap playerActions;
        private InputAction interactAction;
        private BeaconState state;
        private float activationProgress;
        private Vector3 coreBaseScale = Vector3.one;
        private string interactBinding = "Interact";

        public BeaconState State => state;
        public bool IsActivated => state == BeaconState.Activated;
        public bool InputBlocked { get; set; }

        public void RestoreProgress(bool activated)
        {
            if (activated) CompleteActivation();
            else { state = BeaconState.Dormant; SetDormantVisuals(); objectiveUI?.HidePrompt(); }
        }

        private void Awake()
        {
            if (player == null)
            {
                GameObject playerObject = GameObject.Find("Player");
                if (playerObject != null)
                    player = playerObject.transform;
            }

            if (beaconCore != null)
                coreBaseScale = beaconCore.localScale;

            TryResolveActions();
            SetDormantVisuals();
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
            if (InputBlocked) { CancelActivation(); objectiveUI?.HidePrompt(); return; }
            if (state == BeaconState.Activated || player == null)
                return;

            bool inRange = (player.position - transform.position).sqrMagnitude <= interactionRadius * interactionRadius;
            if (!inRange)
            {
                CancelActivation();
                objectiveUI?.HidePrompt();
                return;
            }

            bool interactHeld = interactAction != null && interactAction.IsPressed();
            if (!interactHeld)
            {
                CancelActivation();
                objectiveUI?.ShowPrompt($"{interactBinding} to activate beacon");
                return;
            }

            state = BeaconState.Activating;
            activationProgress = Mathf.Clamp01(activationProgress + Time.deltaTime / activationDuration);
            UpdateActivationVisuals(activationProgress);
            objectiveUI?.ShowPrompt($"Activating beacon... {activationProgress:P0}");

            if (activationProgress >= 1f)
                CompleteActivation();
        }

        private void CancelActivation()
        {
            if (state != BeaconState.Activating)
                return;

            state = BeaconState.Dormant;
            activationProgress = 0f;
            SetDormantVisuals();
        }

        private void CompleteActivation()
        {
            state = BeaconState.Activated;
            activationProgress = 1f;
            UpdateActivationVisuals(activationProgress);
            objectiveUI?.ShowSuccess("BEACON ONLINE\nObjective complete: signal transmitted");
        }

        private void SetDormantVisuals()
        {
            activationProgress = 0f;
            if (activationLight != null)
                activationLight.intensity = 0f;
            if (beaconCore != null)
                beaconCore.localScale = coreBaseScale;
        }

        private void UpdateActivationVisuals(float normalizedProgress)
        {
            if (activationLight != null)
                activationLight.intensity = Mathf.Lerp(0f, activatedLightIntensity, normalizedProgress);
            if (beaconCore != null)
                beaconCore.localScale = coreBaseScale * Mathf.Lerp(1f, activatedCoreScale, normalizedProgress);
        }

        private bool TryResolveActions()
        {
            if (inputActions == null)
            {
                Debug.LogError($"{nameof(CrashedBeaconObjective)} requires an InputActionAsset.", this);
                return false;
            }

            playerActions = inputActions.FindActionMap(ActionMapName, true);
            interactAction = playerActions.FindAction(InteractActionName, true);
            string bindingDisplay = interactAction.GetBindingDisplayString();
            if (!string.IsNullOrEmpty(bindingDisplay))
                interactBinding = bindingDisplay;
            return true;
        }
    }
}
