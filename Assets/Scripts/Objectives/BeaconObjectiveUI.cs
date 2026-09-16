using UnityEngine;
using UnityEngine.UI;

namespace AstralWilds
{
    public sealed class BeaconObjectiveUI : MonoBehaviour
    {
        [SerializeField] private GameObject promptPanel;
        [SerializeField] private Text promptText;
        [SerializeField] private GameObject successPanel;
        [SerializeField] private Text successText;

        private string currentPrompt;
        private bool promptVisible;

        private void Awake()
        {
            HidePrompt();

            if (successPanel != null)
                successPanel.SetActive(false);
        }

        public void ShowPrompt(string message)
        {
            if (promptText != null && currentPrompt != message)
            {
                promptText.text = message;
                currentPrompt = message;
            }

            if (!promptVisible && promptPanel != null)
            {
                promptPanel.SetActive(true);
                promptVisible = true;
            }
        }

        public void HidePrompt()
        {
            if (promptPanel != null && promptVisible)
                promptPanel.SetActive(false);

            promptVisible = false;
            currentPrompt = null;
        }

        public void ShowSuccess(string message)
        {
            HidePrompt();

            if (successText != null)
                successText.text = message;

            if (successPanel != null)
                successPanel.SetActive(true);
        }
    }
}
