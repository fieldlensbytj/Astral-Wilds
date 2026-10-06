// Astral Wilds - see AstralLocomotionAnimInstance.h.
#include "AstralLocomotionAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "GameFramework/Pawn.h"

void UAstralLocomotionAnimInstance::SetClips(UAnimSequence* InIdle, UAnimSequence* InWalk, UAnimSequence* InRun, float InWalkSpeed, float InRunSpeed, float InIdleThreshold)
{
	IdleAnim = InIdle;
	WalkAnim = InWalk;
	RunAnim = InRun;
	WalkSpeed = FMath::Max(InWalkSpeed, 1.f);
	RunSpeed = FMath::Max(InRunSpeed, WalkSpeed + 1.f);
	IdleThreshold = FMath::Max(InIdleThreshold, 0.f);
}

FAnimInstanceProxy* UAstralLocomotionAnimInstance::CreateAnimInstanceProxy()
{
	return new FAstralLocomotionProxy(this);
}

void UAstralLocomotionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const APawn* Pawn = TryGetPawnOwner();
	const float Speed = Pawn ? Pawn->GetVelocity().Size2D() : 0.f;
	const bool bCanMove = WalkAnim || RunAnim;

	// Targets: fade idle -> moving over the first half of walk speed, then
	// walk -> run between the two authored speeds.
	const float MoveTarget = !bCanMove ? 0.f : (IdleAnim ? FMath::SmoothStep(IdleThreshold, FMath::Max(IdleThreshold + 1.f, 0.5f * WalkSpeed), Speed) : 1.f);
	// Walk and Run differ in stride and footfall timing, so mixing them reads
	// as muddle: blend only in the gap between the two paces.
	const float RunTarget = !RunAnim ? 0.f : (!WalkAnim ? 1.f : FMath::SmoothStep(FMath::Lerp(WalkSpeed, RunSpeed, 0.25f), FMath::Lerp(WalkSpeed, RunSpeed, 0.75f), Speed));
	if (DeltaSeconds > 0.f)
	{
		MoveAlpha = FMath::FInterpTo(MoveAlpha, MoveTarget, DeltaSeconds, 8.f);
		RunAlpha = FMath::FInterpTo(RunAlpha, RunTarget, DeltaSeconds, 5.f);
	}

	// Gait cycles per second so the feet cover the ground: one cycle of a clip
	// covers (authored speed x clip length).
	const float WalkLen = WalkAnim ? FMath::Max(WalkAnim->GetPlayLength(), 0.01f) : 1.f;
	const float RunLen = RunAnim ? FMath::Max(RunAnim->GetPlayLength(), 0.01f) : 1.f;
	const float WalkCps = Speed / (WalkSpeed * WalkLen);
	const float RunCps = Speed / (RunSpeed * RunLen);
	// Keep a minimum cadence while blending out, so legs don't freeze mid-stride.
	const float Cps = FMath::Max(FMath::Lerp(WalkCps, RunCps, RunAlpha), 0.5f / FMath::Lerp(WalkLen, RunLen, RunAlpha));
	GaitPhase = FMath::Frac(GaitPhase + DeltaSeconds * Cps);

	const float IdleLen = IdleAnim ? FMath::Max(IdleAnim->GetPlayLength(), 0.01f) : 1.f;
	IdleTime = FMath::Fmod(IdleTime + DeltaSeconds, IdleLen);

	// Body bend into turns: ~0.1 deg of bend per deg/s of yaw rate, capped,
	// eased so it builds into a turn and unwinds after it.
	const float Yaw = Pawn ? Pawn->GetActorRotation().Yaw : 0.f;
	float YawRate = 0.f;
	if (bHasLastYaw && DeltaSeconds > 0.f)
	{
		YawRate = FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / DeltaSeconds;
	}
	LastYaw = Yaw;
	bHasLastYaw = Pawn != nullptr;
	if (DeltaSeconds > 0.f)
	{
		TurnBend = FMath::FInterpTo(TurnBend, FMath::Clamp(YawRate * 0.1f, -22.f, 22.f), DeltaSeconds, 4.f);
	}

	FAstralLocomotionProxy& Proxy = GetProxyOnGameThread<FAstralLocomotionProxy>();
	Proxy.TurnBend = TurnBend;
	Proxy.Layers.Reset();
	auto Add = [&Proxy](const UAnimSequence* Seq, float Time, float Weight)
	{
		if (Seq && Weight > 0.001f)
		{
			Proxy.Layers.Add({ Seq, Time, Weight });
		}
	};
	const float Move = bCanMove ? MoveAlpha : 0.f;
	Add(IdleAnim, IdleTime, IdleAnim ? 1.f - Move : 0.f);
	Add(WalkAnim, GaitPhase * WalkLen, Move * (RunAnim ? 1.f - RunAlpha : 1.f));
	Add(RunAnim, GaitPhase * RunLen, Move * (WalkAnim ? RunAlpha : 1.f));

	// Normalise (e.g. idle missing while slow).
	float Total = 0.f;
	for (const FAstralLocomotionLayer& L : Proxy.Layers)
	{
		Total += L.Weight;
	}
	for (FAstralLocomotionLayer& L : Proxy.Layers)
	{
		L.Weight /= Total;
	}
}

bool FAstralLocomotionProxy::Evaluate(FPoseContext& Output)
{
	const int32 Num = Layers.Num();
	if (Num == 0)
	{
		Output.ResetToRefPose();
		return true;
	}
	if (Num == 1)
	{
		FAnimationPoseData PoseData(Output);
		Layers[0].Sequence->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Layers[0].Time), false, {}, true));
		ApplyTurnBend(Output);
		return true;
	}

	// Same pattern as UBlendSpace::GetAnimationPose_Internal.
	TArray<FCompactPose, TInlineAllocator<3>> Poses;
	TArray<FBlendedCurve, TInlineAllocator<3>> Curves;
	TArray<UE::Anim::FStackAttributeContainer, TInlineAllocator<3>> Attributes;
	TArray<float, TInlineAllocator<3>> Weights;
	Poses.AddZeroed(Num);
	Curves.AddZeroed(Num);
	Attributes.AddZeroed(Num);
	for (int32 i = 0; i < Num; ++i)
	{
		Poses[i].SetBoneContainer(&Output.Pose.GetBoneContainer());
		Curves[i].InitFrom(Output.Curve);
		FAnimationPoseData PoseData(Poses[i], Curves[i], Attributes[i]);
		Layers[i].Sequence->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Layers[i].Time), false, {}, true));
		Weights.Add(Layers[i].Weight);
	}
	FAnimationPoseData OutData(Output);
	FAnimationRuntime::BlendPosesTogether(Poses, Curves, Attributes, Weights, OutData);
	ApplyTurnBend(Output);
	return true;
}

void FAstralLocomotionProxy::CacheBendBones(const FBoneContainer& Bones)
{
	BendBones.Reset();
	BendBonesFor = &Bones;
	BendBonesSerial = Bones.GetSerialNumber();

	// Front of the body leads into the turn; the tail swings to the inside of
	// the arc too, which for a backward-pointing chain is the opposite sign.
	static const TPair<const TCHAR*, float> Shares[] = {
		{ TEXT("spine_01"), 0.15f }, { TEXT("chest"), 0.25f }, { TEXT("neck"), 0.3f }, { TEXT("head"), 0.3f },
		{ TEXT("tail_01"), -0.2f }, { TEXT("tail_02"), -0.2f }, { TEXT("tail_03"), -0.15f }, { TEXT("tail_04"), -0.15f },
	};
	const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
	for (const TPair<const TCHAR*, float>& S : Shares)
	{
		const int32 MeshIndex = Ref.FindBoneIndex(FName(S.Key));
		if (MeshIndex == INDEX_NONE)
		{
			continue;
		}
		const FCompactPoseBoneIndex Compact = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		const int32 ParentIndex = Ref.GetParentIndex(MeshIndex);
		if (Compact == INDEX_NONE || ParentIndex == INDEX_NONE)
		{
			continue;
		}
		// Component up expressed in the parent's reference-pose frame; the
		// clips only rotate these bones a few degrees, so the ref frame is close enough.
		const FTransform ParentRef = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, ParentIndex);
		BendBones.Add({ Compact, ParentRef.GetRotation().UnrotateVector(FVector::UpVector).GetSafeNormal(), S.Value });
	}
}

void FAstralLocomotionProxy::ApplyTurnBend(FPoseContext& Output)
{
	if (FMath::Abs(TurnBend) < 0.05f)
	{
		return;
	}
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	if (BendBonesFor != &Bones || BendBonesSerial != Bones.GetSerialNumber())
	{
		CacheBendBones(Bones);
	}
	for (const FBendBone& B : BendBones)
	{
		FTransform& Local = Output.Pose[B.Index];
		Local.SetRotation((FQuat(B.Axis, FMath::DegreesToRadians(TurnBend * B.Share)) * Local.GetRotation()).GetNormalized());
	}
}
