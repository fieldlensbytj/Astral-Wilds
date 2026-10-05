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
	const float RunTarget = !RunAnim ? 0.f : (!WalkAnim ? 1.f : FMath::SmoothStep(WalkSpeed, RunSpeed, Speed));
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

	FAstralLocomotionProxy& Proxy = GetProxyOnGameThread<FAstralLocomotionProxy>();
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
	return true;
}
