// Astral Wilds - native locomotion blending for rigged Astrals (no Anim Blueprint needed).
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AstralLocomotionAnimInstance.generated.h"

class UAnimSequence;

/** One clip to sample this frame: which, where, and how much. */
struct FAstralLocomotionLayer
{
	const UAnimSequence* Sequence = nullptr;
	float Time = 0.f;
	float Weight = 0.f;
};

/** Worker-thread side: samples the layers the game thread chose and blends them. */
struct FAstralLocomotionProxy : public FAnimInstanceProxy
{
	FAstralLocomotionProxy() = default;
	explicit FAstralLocomotionProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual bool Evaluate(FPoseContext& Output) override;

	TArray<FAstralLocomotionLayer, TInlineAllocator<3>> Layers;
};

/**
 * Crossfades Idle / Walk / Run by ground speed, the way a 1D blend space
 * would, without needing an Anim Blueprint or blend space asset per species:
 * - Weights ease toward their targets, so starts, stops and gait changes
 *   blend instead of popping.
 * - Walk and Run share one normalised gait phase, so blending between them
 *   keeps the legs in step.
 * - The phase advances by distance travelled (speed / authored speed), so
 *   feet cover the ground they're meant to at any speed.
 */
UCLASS(Transient, NotBlueprintable)
class UAstralLocomotionAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Authored speeds are the ground speeds (cm/s) at which each clip's feet don't slide. */
	void SetClips(UAnimSequence* InIdle, UAnimSequence* InWalk, UAnimSequence* InRun, float InWalkSpeed, float InRunSpeed, float InIdleThreshold);

	/** Current blend weights, for tests and debugging. */
	float GetMoveAlpha() const { return MoveAlpha; }
	float GetRunAlpha() const { return RunAlpha; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WalkAnim;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> RunAnim;

	float WalkSpeed = 150.f;
	float RunSpeed = 380.f;
	float IdleThreshold = 25.f;

	/** 0 = idle, 1 = moving; then within moving, 0 = walk, 1 = run. Both eased. */
	float MoveAlpha = 0.f;
	float RunAlpha = 0.f;
	/** Normalised gait cycle shared by Walk and Run, in [0, 1). */
	float GaitPhase = 0.f;
	float IdleTime = 0.f;
};
