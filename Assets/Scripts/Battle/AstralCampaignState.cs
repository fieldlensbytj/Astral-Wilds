using System;
using System.Collections.Generic;
using UnityEngine;

namespace AstralWilds
{
    public enum AstralCampaignStage
    {
        Exploration,
        FirstEncounter,
        Battle,
        Recruitment,
        PartyManagement,
        SecondEncounter,
        ReturnToExploration
    }

    [Serializable]
    public sealed class AstralMemberRecord
    {
        [SerializeField] private string id;
        [SerializeField] private string displayName;
        [SerializeField] private bool defeated;

        public AstralMemberRecord(string id, string displayName = null)
        {
            if (string.IsNullOrWhiteSpace(id))
                throw new ArgumentException("An Astral requires a stable id.", nameof(id));
            this.id = id;
            this.displayName = string.IsNullOrWhiteSpace(displayName) ? id : displayName;
        }

        public string Id => id;
        public string DisplayName => displayName;
        public bool IsDefeated => defeated;
        public void MarkDefeated() => defeated = true;
    }

    public enum AstralRecruitmentResult
    {
        AddedToParty,
        AddedToReserve,
        DuplicateReward,
        Invalid,
        Rejected
    }

    [Serializable]
    public sealed class AstralCampaignSaveData
    {
        public AstralCampaignStage stage;
        public bool beaconActivated;
        public List<AstralMemberRecord> party = new List<AstralMemberRecord>();
        public List<AstralMemberRecord> reserve = new List<AstralMemberRecord>();
    }

    /// <summary>
    /// Small persistence-ready campaign boundary for the demo loop. It deliberately
    /// contains no combat timing, abilities, or balance decisions.
    /// </summary>
    public sealed class AstralCampaignState
    {
        public const int PartyCapacity = 6;

        private readonly List<AstralMemberRecord> party = new List<AstralMemberRecord>();
        private readonly List<AstralMemberRecord> reserve = new List<AstralMemberRecord>();

        public IReadOnlyList<AstralMemberRecord> Party => party;
        public IReadOnlyList<AstralMemberRecord> Reserve => reserve;
        public AstralCampaignStage Stage { get; private set; } = AstralCampaignStage.Exploration;
        public bool BeaconActivated { get; private set; }

        public AstralRecruitmentResult Recruit(string id, string displayName = null)
        {
            if (string.IsNullOrWhiteSpace(id))
                return AstralRecruitmentResult.Invalid;
            if (Contains(id))
                return AstralRecruitmentResult.DuplicateReward;

            var member = new AstralMemberRecord(id, displayName);
            if (party.Count < PartyCapacity)
            {
                party.Add(member);
                return AstralRecruitmentResult.AddedToParty;
            }

            reserve.Add(member);
            return AstralRecruitmentResult.AddedToReserve;
        }

        public bool TrySetStage(AstralCampaignStage stage)
        {
            Stage = stage;
            return true;
        }

        public void CompleteBeaconObjective()
        {
            BeaconActivated = true;
            Stage = AstralCampaignStage.ReturnToExploration;
        }

        public string SaveJson()
        {
            var data = new AstralCampaignSaveData
            {
                stage = Stage,
                beaconActivated = BeaconActivated,
                party = new List<AstralMemberRecord>(party),
                reserve = new List<AstralMemberRecord>(reserve)
            };
            return JsonUtility.ToJson(data);
        }

        public static AstralCampaignState LoadJson(string json)
        {
            if (string.IsNullOrWhiteSpace(json))
                throw new ArgumentException("Save data is empty.", nameof(json));

            var data = JsonUtility.FromJson<AstralCampaignSaveData>(json);
            if (data == null)
                throw new ArgumentException("Save data could not be parsed.", nameof(json));

            var state = new AstralCampaignState
            {
                Stage = data.stage,
                BeaconActivated = data.beaconActivated
            };
            if (data.party != null)
                state.party.AddRange(data.party);
            if (data.reserve != null)
                state.reserve.AddRange(data.reserve);
            return state;
        }

        private bool Contains(string id)
        {
            for (int i = 0; i < party.Count; i++)
                if (party[i].Id == id)
                    return true;
            for (int i = 0; i < reserve.Count; i++)
                if (reserve[i].Id == id)
                    return true;
            return false;
        }
    }
}
