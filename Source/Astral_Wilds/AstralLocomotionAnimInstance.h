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

	TArray<FAstralLocomotionLayer, TInlineAllocator<6>> Layers;

	/** Body curve into the current turn, in degrees: + bends toward +yaw. */
	float TurnBend = 0.f;

	/** Head and neck turned toward where the Astral is steering, ahead of the body, in degrees (+ toward +yaw). */
	float HeadLead = 0.f;

	/** Tail spring (secondary motion): sideways swing (+ toward +yaw) and up/down swing, degrees, spread down the tail. */
	float TailYaw = 0.f;
	float TailPitch = 0.f;

	/** Gaze: where the head is looking relative to the body, degrees (+yaw toward +yaw, +pitch up); the neck takes part, the head the rest. */
	float LookYaw = 0.f;
	float LookPitch = 0.f;

	/** Foot IK on uneven ground, in component space: the body raised/lowered by PelvisOffset and pitched so its front-to-back line rises by BodySlope (quadrupeds), then each foot moved from where the clip put it by FootOffsets[i] (leg order: see FootIKLegs). */
	FVector PelvisOffset = FVector::ZeroVector;
	FVector BodySlope = FVector::ZeroVector;
	FVector FootOffsets[6];
	bool bFootIK = false;

private:
	/** Per bent bone: compact index, rotation axes (component up / tail side, in its parent's ref frame), shares of TurnBend, HeadLead and the tail spring. */
	struct FBendBone
	{
		FCompactPoseBoneIndex Index = FCompactPoseBoneIndex(INDEX_NONE);
		FVector Axis = FVector::UpVector;
		FVector PitchAxis = FVector::RightVector;
		float Share = 0.f;
		float LeadShare = 0.f;
		float TailShare = 0.f;
		FVector LookPitchAxis = FVector::RightVector;
		float LookShare = 0.f;
	};
	TArray<FBendBone> BendBones;
	const FBoneContainer* BendBonesFor = nullptr;
	uint16 BendBonesSerial = 0;
	void CacheBendBones(const FBoneContainer& Bones);
	void ApplyTurnBend(FPoseContext& Output);

	/** Leg chains found on this skeleton (upper, lower, foot), in FootIKLegs order; invalid where a leg is missing. */
	struct FLegChain
	{
		FCompactPoseBoneIndex Upper = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Lower = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Foot = FCompactPoseBoneIndex(INDEX_NONE);
	};
	FLegChain LegChains[6];
	FCompactPoseBoneIndex PelvisIndex = FCompactPoseBoneIndex(INDEX_NONE);
	void ApplyFootIK(FPoseContext& Output);
};

/** Leg chains foot IK looks for, by bone name: the quadrupeds' fl/fr/bl/br and Stormrook's bird legs. */
struct FAstralFootIKLeg
{
	const TCHAR* Upper;
	const TCHAR* Lower;
	const TCHAR* Foot;
};
extern const FAstralFootIKLeg FootIKLegs[6];

/**
 * Crossfades Idle / Walk / Run by ground speed, the way a 1D blend space
 * would, without needing an Anim Blueprint or blend space asset per species:
 * - Weights ease toward their targets, so starts, stops and gait changes
 *   blend instead of popping.
 * - Walk and Run share one normalised gait phase, so blending between them
 *   keeps the legs in step.
 * - The phase advances by distance travelled (speed / authored speed), so
 *   feet cover the ground they're meant to at any speed.
 * - Walk only blends toward Run between cruising speeds, never at them, so a
 *   steady pace shows one clean gait rather than a trot/gallop average.
 * - The spine curves into turns (chest, neck and head lead; the tail swings
 *   out), eased from the actor's yaw rate, so turning bends the body rather
 *   than rotating it rigidly.
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

	/** Most the head and neck turn toward the steering direction, in degrees (per species: UAstralSpeciesData::HeadLeadMax). */
	float HeadLeadMax = 35.f;

	/** Flight clips for a flying species (both optional): Fly flaps, Glide holds the wings spread. */
	void SetFlightClips(UAnimSequence* InFly, UAnimSequence* InGlide, UAnimSequence* InFlare = nullptr) { FlyAnim = InFly; GlideAnim = InGlide; FlareAnim = InFlare; }
	float GetBrakeAlpha() const { return BrakeAlpha; }
	float GetFlightAlpha() const { return FlightAlpha; }
	float GetFlapAlpha() const { return FlapAlpha; }

	/** Foot IK state, for tests and the MotionCapture log: how far the body is lowered and the largest foot adjustment (cm, world). */
	float GetPelvisDrop() const { return PelvisDrop; }
	float GetMaxFootOffset() const;
	float GetTailYaw() const { return TailYaw; }
	float GetLookYaw() const { return LookYaw; }
	float GetLookPitch() const { return LookPitch; }
	/** Current "Blink" morph value (0 open, 1 closed), for tests and the capture log. */
	float GetBlink() const { return BlinkValue; }
	/** Blink curve: 0 -> 1 -> 0 over Duration seconds (fast close, slightly slower open). Exposed for tests. */
	static float BlinkCurve(float T, float Duration = 0.16f);

	/** Yaw and pitch (degrees, relative to the body facing Body) of looking from From at Target. Exposed for tests. */
	static void LookAngles(const FTransform& Body, const FVector& From, const FVector& Target, float& OutYaw, float& OutPitch);

	/**
	 * The tail spring, exposed for tests: an underdamped spring (natural
	 * frequency Omega rad/s, damping ratio Zeta) pulled toward Target, stepped
	 * by Dt. Under-damped so the tail overshoots and settles after a turn or
	 * stop instead of stopping dead.
	 */
	static void StepSpring(float& Value, float& Velocity, float Target, float Dt, float Omega = 9.f, float Zeta = 0.3f);

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
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> FlyAnim;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> GlideAnim;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> FlareAnim;

	/** 0 on the ground -> 1 in the air; within the air, 0 gliding -> 1 flapping. Both eased. */
	float FlightAlpha = 0.f;
	float FlapAlpha = 0.f;
	float FlyTime = 0.f;
	float GlideTime = 0.f;
	/** 0 -> 1 while braking in the air (slowing, or slow and not climbing). */
	float BrakeAlpha = 0.f;
	float FlareTime = 0.f;
	float LastAirSpeed = 0.f;
	float SmoothedDecel = 0.f;
	/** Flap-flap-glide rhythm while airborne: seconds into the current bout cycle. */
	float FlapBoutTime = 0.f;
	static constexpr float FlapBoutLength = 4.f;
	static constexpr float GlideBoutLength = 1.2f;

	float WalkSpeed = 150.f;
	float RunSpeed = 380.f;
	float IdleThreshold = 25.f;

	/** 0 = idle, 1 = moving; then within moving, 0 = walk, 1 = run. Both eased. */
	float MoveAlpha = 0.f;
	float RunAlpha = 0.f;
	/** Normalised gait cycle shared by Walk and Run, in [0, 1). */
	float GaitPhase = 0.f;
	float IdleTime = 0.f;

	/** Eased body bend into turns (degrees), from the owner's yaw rate. */
	float TurnBend = 0.f;
	float LastYaw = 0.f;
	bool bHasLastYaw = false;
	float HeadLead = 0.f;

	/** Tail spring state (degrees, deg/s) and the last forward speed (for acceleration). */
	float TailYaw = 0.f;
	float TailYawVel = 0.f;
	float TailPitch = 0.f;
	float TailPitchVel = 0.f;
	float LastForwardSpeed = 0.f;

	/** Foot IK: smoothed ground height under each foot relative to the floor under the body (cm), and the body drop. */
	float FootGround[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	float PelvisDrop = 0.f;
	void UpdateFootIK(float DeltaSeconds, struct FAstralLocomotionProxy& Proxy);

	/**
	 * Attention (2026-10-09, TJ: "animals look around like humans ... up, down,
	 * left, right, in the sky, on the ground"): every so often the Astral picks
	 * something to look at - the Mage when near (watching, wary, or locked on
	 * in a chase, glancing back in a flee), another Astral, the sky, the ground
	 * (sniffing, grazing), a scan left or right, or straight ahead - moves its
	 * head there quickly, and holds. A raptor (flyer) snaps and holds still;
	 * others ease. Layered on the clips and the head lead into turns.
	 */
	void UpdateGaze(float DeltaSeconds);

	/** Blinking (TJ, 2026-10-09: "what about opening and closing eyes"): drives the "Blink" morph target (eyelids built by astral_add_eyelids.py) closed and open in ~0.16s every 2-6s, sometimes twice in a row, and on a sharp head turn. */
	void UpdateBlink(float DeltaSeconds);
	float BlinkTimer = 2.f;
	float BlinkValue = 0.f;
	float BlinkT = -1.f;      // < 0: eyes open; else seconds into the blink
	bool bDoubleBlink = false;
	float LastLookYaw = 0.f;
	enum class EGaze : uint8 { Ahead, Glance, Watch };
	EGaze Gaze = EGaze::Ahead;
	TWeakObjectPtr<const AActor> GazeActor;
	float GazeYaw = 0.f;      // target for Ahead/Glance (relative)
	float GazePitch = 0.f;
	float GazeHold = 0.f;     // seconds left on the current target
	bool bGazeWasMoving = false;
	float LookYaw = 0.f, LookYawVel = 0.f, LookPitch = 0.f, LookPitchVel = 0.f;
};
