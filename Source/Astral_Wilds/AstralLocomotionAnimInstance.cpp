// Astral Wilds - see AstralLocomotionAnimInstance.h.
#include "AstralLocomotionAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "AstralWildlifeController.h"
#include "AstralCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

// Holds the eyelids at a fixed Blink value (0..1) so a capture can film them
// shut from any angle; -1 (default) blinks normally.
static TAutoConsoleVariable<float> CVarAstralForceBlink(TEXT("Astral.ForceBlink"), -1.f, TEXT("Hold the Blink morph at this value (0..1); -1 blinks normally."));

// Repeats one fidget every 2s while standing, for filming it: 1 shake, 2 tail
// swish, 3 stretch, 4 paw shuffle; 0 (default) picks at random.
static TAutoConsoleVariable<int32> CVarAstralForceFidget(TEXT("Astral.ForceFidget"), 0, TEXT("Repeat one standing fidget for review: 1 shake, 2 tail swish, 3 stretch, 4 paw shuffle; 0 = random."));

const FAstralFootIKLeg FootIKLegs[6] = {
	{ TEXT("fl_upper"), TEXT("fl_lower"), TEXT("fl_foot") },
	{ TEXT("fr_upper"), TEXT("fr_lower"), TEXT("fr_foot") },
	{ TEXT("bl_upper"), TEXT("bl_lower"), TEXT("bl_foot") },
	{ TEXT("br_upper"), TEXT("br_lower"), TEXT("br_foot") },
	{ TEXT("leg_l_thigh"), TEXT("leg_l_shin"), TEXT("leg_l_foot") },
	{ TEXT("leg_r_thigh"), TEXT("leg_r_shin"), TEXT("leg_r_foot") },
};

void UAstralLocomotionAnimInstance::StepSpring(float& Value, float& Velocity, float Target, float Dt, float Omega, float Zeta)
{
	// Semi-implicit Euler in small steps, so a long frame can't blow it up.
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dt / (1.f / 120.f)), 1, 8);
	const float H = Dt / Steps;
	for (int32 i = 0; i < Steps; ++i)
	{
		Velocity += (Omega * Omega * (Target - Value) - 2.f * Zeta * Omega * Velocity) * H;
		Value += Velocity * H;
	}
}

bool UAstralLocomotionAnimInstance::ReachesSettlePhase(float Prev, float Step)
{
	if (Step < 0.f)
	{
		return false;
	}
	for (const float P : { 0.25f, 0.75f })
	{
		if (FMath::Frac(P - Prev + 1.f) <= Step)   // how far ahead P is
		{
			return true;
		}
	}
	return false;
}

void UAstralLocomotionAnimInstance::UpdateLife(float Dt, float ForwardAccel)
{
	if (Dt <= 0.f)
	{
		return;
	}
	LifeTime += Dt;
	const APawn* Pawn = TryGetPawnOwner();
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const AAstralWildlifeController* AI = Pawn ? Cast<AAstralWildlifeController>(Pawn->GetController()) : nullptr;
	const EAstralWildlifeMode Mode = AI ? AI->GetMode() : EAstralWildlifeMode::Wander;
	const EAstralWanderActivity Activity = AI ? AI->GetActivity() : EAstralWanderActivity::LookAround;
	const bool bQuad = Mesh && Mesh->GetBoneIndex(TEXT("bl_foot")) != INDEX_NONE;
	const float Ground = 1.f - FlightAlpha;

	// Fidgets while standing about (none while alert, chasing or fleeing; a
	// receptive Astral being bonded keeps to the small ones). A grazer swishes
	// its tail and shifts its feet but doesn't stop to shake or stretch.
	const bool bStill = MoveAlpha < 0.05f && FlightAlpha < 0.05f;
	StillTime = bStill ? StillTime + Dt : 0.f;
	const bool bCalm = Activity != EAstralWanderActivity::Alert && Mode != EAstralWildlifeMode::Chase && Mode != EAstralWildlifeMode::Flee;
	// The timer runs whenever it stands (pauses are only 2-5s), firing once it
	// has settled for a moment.
	if (Fidget == EFidget::None && bStill)
	{
		FidgetTimer -= Dt;
	}
	const int32 Forced = CVarAstralForceFidget.GetValueOnGameThread();
	if (Fidget == EFidget::None && bStill && bCalm && StillTime > 0.8f && FidgetTimer <= 0.f)
	{
		FidgetTimer = Forced > 0 ? 2.f : FMath::FRandRange(2.f, 5.f);
		FidgetT = 0.f;
		FidgetSide = FMath::RandBool() ? 1.f : -1.f;
		const bool bBusy = Activity == EAstralWanderActivity::Graze || Mode == EAstralWildlifeMode::Idle;
		const float R = FMath::FRand();
		const int32 Pick = Forced > 0 ? Forced
			: R < 0.3f ? static_cast<int32>(EFidget::TailSwish)
			: R < 0.6f ? static_cast<int32>(EFidget::PawShuffle)
			: (R < 0.8f && !bBusy) ? static_cast<int32>(EFidget::Shake)
			: (R < 0.95f && !bBusy) ? static_cast<int32>(EFidget::Stretch)
			: 0;
		Fidget = static_cast<EFidget>(FMath::Clamp(Pick, 0, 4));
		if (Fidget == EFidget::Stretch && !bQuad)
		{
			Fidget = EFidget::None;
		}
		FidgetLeg = bQuad ? FMath::RandRange(0, 3) : FMath::RandRange(4, 5);
	}
	FidgetPitch = 0.f;
	FidgetLook = 0.f;
	ShakeRoll = 0.f;
	for (float& L : PawLift)
	{
		L = 0.f;
	}
	if (Fidget != EFidget::None)
	{
		FidgetT += Dt;
	}
	switch (Fidget)
	{
	case EFidget::Shake:
	{
		// A shake-off, like a wet dog: head and neck whip side to side, the
		// trunk rolling with them; eyes shut through it.
		constexpr float Dur = 0.9f;
		if (FidgetT <= Dt + KINDA_SMALL_NUMBER && BlinkT < 0.f)
		{
			BlinkT = 0.f;
			bDoubleBlink = true;
		}
		ShakeRoll = 22.f * FidgetSide * FMath::Sin(PI * FMath::Min(FidgetT / Dur, 1.f)) * FMath::Sin(2.f * PI * 4.5f * FidgetT);
		if (FidgetT > Dur)
		{
			Fidget = EFidget::None;
		}
		break;
	}
	case EFidget::TailSwish:
		// Flicking at a fly: a kick one way, then back.
		if (FidgetT <= Dt + KINDA_SMALL_NUMBER)
		{
			TailYawVel += 260.f * FidgetSide;
		}
		if (FidgetT >= 0.35f && FidgetT - Dt < 0.35f)
		{
			TailYawVel -= 300.f * FidgetSide;
		}
		if (FidgetT > 0.9f)
		{
			Fidget = EFidget::None;
		}
		break;
	case EFidget::Stretch:
	{
		// Chest down, rump up, head lifted (a cat or dog stretching); let go
		// of early if it sets off.
		constexpr float Dur = 2.6f;
		if (!bStill && FidgetT < Dur - 0.6f)
		{
			FidgetT = Dur - 0.6f;
		}
		const float Env = FMath::SmoothStep(0.f, 0.7f, FidgetT) * (1.f - FMath::SmoothStep(Dur - 0.7f, Dur, FidgetT));
		FidgetPitch = -7.f * Env;
		FidgetLook = 20.f * Env;
		if (FidgetT > Dur)
		{
			Fidget = EFidget::None;
		}
		break;
	}
	case EFidget::PawShuffle:
	{
		// One foot lifted a little and set back down; sometimes its partner after it.
		constexpr float Dur = 0.5f;
		const float Scale = Mesh ? FMath::Clamp(Mesh->Bounds.SphereRadius / 120.f, 0.6f, 1.5f) : 1.f;
		PawLift[FidgetLeg] = 6.f * Scale * FMath::Square(FMath::Sin(PI * FMath::Min(FidgetT / Dur, 1.f)));
		if (FidgetT > Dur)
		{
			if (bStill && FMath::FRand() < 0.4f)
			{
				FidgetT = 0.f;
				FidgetLeg ^= 1;   // fl <-> fr, bl <-> br, left <-> right
			}
			else
			{
				Fidget = EFidget::None;
			}
		}
		break;
	}
	default:
		break;
	}

	// Weight transfer: pushing off squats the hindquarters, braking drops the
	// chest; a spring, so after a stop the body rocks back and settles.
	SmoothedAccel = FMath::FInterpTo(SmoothedAccel, ForwardAccel, Dt, 6.f);
	const float PitchTarget = (FMath::Clamp(SmoothedAccel * 0.008f, -5.f, 4.f) + FidgetPitch) * Ground;
	StepSpring(BodyPitch, BodyPitchVel, PitchTarget, Dt, 7.f, 0.45f);

	// Winded after a run: breathing quickens and deepens, then calms.
	const float Effort = MoveAlpha * RunAlpha * Ground;
	Exertion = Effort > 0.5f ? FMath::Min(1.f, Exertion + Dt / 5.f) : FMath::Max(0.f, Exertion - Dt / 14.f);
	BreathPhase = FMath::Frac(BreathPhase + Dt * FMath::Lerp(0.4f, 2.4f, Exertion));

	// Nose to the ground: sniffing twitches, in bursts.
	const float Burst = (LookPitch < -30.f && FMath::PerlinNoise1D(LifeTime * 0.6f + NoiseSeed) > 0.f) ? 1.f : 0.f;
	SniffEnv = FMath::FInterpTo(SniffEnv, Burst, Dt, 6.f);
	SniffPitch = SniffEnv * 2.2f * FMath::Sin(2.f * PI * 6.5f * LifeTime);
}

void UAstralLocomotionAnimInstance::LookAngles(const FTransform& Body, const FVector& From, const FVector& Target, float& OutYaw, float& OutPitch)
{
	const FVector L = Body.InverseTransformVectorNoScale(Target - From);
	OutYaw = FMath::RadiansToDegrees(FMath::Atan2(L.Y, L.X));
	OutPitch = FMath::RadiansToDegrees(FMath::Atan2(L.Z, FMath::Max(L.Size2D(), 1.f)));
}

void UAstralLocomotionAnimInstance::UpdateGaze(float Dt)
{
	const APawn* Pawn = TryGetPawnOwner();
	if (!Pawn || Dt <= 0.f)
	{
		return;
	}
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const bool bRaptor = FlyAnim != nullptr;                 // snaps and holds still, like a hawk
	const bool bFlying = FlightAlpha > 0.5f;
	const AAstralWildlifeController* AI = Cast<AAstralWildlifeController>(Pawn->GetController());
	const EAstralWildlifeMode Mode = AI ? AI->GetMode() : EAstralWildlifeMode::Wander;
	const EAstralWanderActivity Activity = AI ? AI->GetActivity() : EAstralWanderActivity::LookAround;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(Pawn, 0);
	const float PlayerDist = Player ? FVector::Dist(Player->GetActorLocation(), Pawn->GetActorLocation()) : TNumericLimits<float>::Max();
	auto Rand = [](float A, float B) { return FMath::FRandRange(A, B); };
	auto Side = []() { return FMath::RandBool() ? 1.f : -1.f; };

	GazeHold -= Dt;
	// Noticing the Mage, or starting to graze, takes the eyes at once.
	if (static_cast<uint8>(Activity) != LastActivity)
	{
		LastActivity = static_cast<uint8>(Activity);
		if (Activity == EAstralWanderActivity::Alert || Activity == EAstralWanderActivity::Graze)
		{
			GazeHold = 0.f;
		}
	}
	// Starting or stopping changes what an animal attends to (a look picked
	// while standing shouldn't carry on into the walk, or eyes-front past a
	// stop), and the Mage stops being interesting once far off.
	const bool bMovingNow = MoveAlpha > 0.5f;
	if (bMovingNow != bGazeWasMoving)
	{
		GazeHold = FMath::Min(GazeHold, 0.15f);
		bGazeWasMoving = bMovingNow;
	}
	if (Gaze == EGaze::Watch && GazeActor.Get() == Player && PlayerDist > 1800.f && Mode != EAstralWildlifeMode::Chase)
	{
		GazeHold = 0.f;
	}
	// A hawk in the air works whatever it has picked out on the ground: a new
	// quarry gets the eyes at once.
	const AActor* Quarry = (bFlying && AI) ? AI->GetFlightQuarry() : nullptr;
	if (Quarry && Gaze == EGaze::Watch && GazeActor.Get() != Quarry && Mode != EAstralWildlifeMode::Chase)
	{
		GazeHold = 0.f;
	}
	if (GazeHold <= 0.f)
	{
		Gaze = EGaze::Ahead;
		GazeYaw = 0.f;
		GazePitch = -4.f;
		GazeActor.Reset();
		const float R = FMath::FRand();
		if (Activity == EAstralWanderActivity::Alert && Player)
		{
			Gaze = EGaze::Watch; GazeActor = Player; GazeHold = Rand(1.5f, 3.f);              // frozen, staring at the Mage
		}
		else if (Mode == EAstralWildlifeMode::Chase && Player)
		{
			Gaze = EGaze::Watch; GazeActor = Player; GazeHold = Rand(0.6f, 1.2f);          // eyes locked on
		}
		else if (Mode == EAstralWildlifeMode::Flee && Player)
		{
			if (R < 0.45f) { Gaze = EGaze::Watch; GazeActor = Player; GazeHold = Rand(0.35f, 0.7f); }   // a look back over the shoulder
			else { GazeHold = Rand(0.8f, 1.6f); }
		}
		else if (Mode == EAstralWildlifeMode::Idle && Player)
		{
			Gaze = EGaze::Watch; GazeActor = Player; GazeHold = Rand(2.f, 4.f);              // receptive: attentive to the Mage
		}
		else if (Quarry)
		{
			// Hunting from the air: head locked on the quarry, the body
			// circling under it; now and then a quick scan of the ground.
			if (R < 0.85f) { Gaze = EGaze::Watch; GazeActor = Quarry; GazeHold = Rand(1.5f, 3.f); }
			else { Gaze = EGaze::Glance; GazeYaw = Side() * Rand(20.f, 60.f); GazePitch = -Rand(25.f, 45.f); GazeHold = Rand(0.4f, 0.8f); }
		}
		else if (Activity == EAstralWanderActivity::Graze && !bFlying)
		{
			// Grazing: nose down working the ground, now and then the head
			// comes up to look round (at the Mage if it is near) while chewing.
			Gaze = EGaze::Glance;
			if (R < 0.78f) { GazeYaw = Rand(-30.f, 30.f); GazePitch = -Rand(50.f, 65.f); GazeHold = Rand(2.f, 4.5f); }
			else if (Player && PlayerDist < 1400.f) { Gaze = EGaze::Watch; GazeActor = Player; GazeHold = Rand(1.f, 2.f); }
			else { GazeYaw = Side() * Rand(20.f, 50.f); GazePitch = Rand(-5.f, 10.f); GazeHold = Rand(1.f, 2.f); }
		}
		else if (Player && PlayerDist < 1400.f && R < 0.45f)
		{
			Gaze = EGaze::Watch; GazeActor = Player; GazeHold = Rand(1.5f, 3.5f);            // keeping an eye on the Mage
		}
		else if (bFlying)
		{
			// Soaring: scan the ground below, one side then the other.
			if (R < 0.75f) { Gaze = EGaze::Glance; GazeYaw = Side() * Rand(10.f, 45.f); GazePitch = -Rand(25.f, 45.f); }
			GazeHold = Rand(0.8f, 2.f);
		}
		else if (MoveAlpha > 0.5f)
		{
			// Travelling: mostly eyes on the way ahead, now and then a side glance.
			if (R < 0.55f) { GazeHold = Rand(1.f, 2.5f); }
			else { Gaze = EGaze::Glance; GazeYaw = Side() * Rand(25.f, 50.f); GazePitch = Rand(-12.f, 10.f); GazeHold = Rand(0.5f, 1.f); }
		}
		else
		{
			// Standing: look around.
			const float S = FMath::FRand();
			Gaze = EGaze::Glance;
			if (S < 0.35f) { GazeYaw = Side() * Rand(30.f, 70.f); GazePitch = Rand(-6.f, 10.f); GazeHold = Rand(0.8f, 2.f); }       // scan left / right
			else if (S < 0.6f) { GazeYaw = Rand(-25.f, 25.f); GazePitch = -Rand(45.f, 65.f); GazeHold = Rand(1.5f, 3.5f); }   // nose to the ground: sniff, graze
			else if (S < 0.75f) { GazeYaw = Rand(-40.f, 40.f); GazePitch = Rand(20.f, 35.f); GazeHold = Rand(1.f, 2.f); }     // up at the sky
			else if (S < 0.9f)
			{
				// Another Astral nearby.
				const AActor* Best = nullptr;
				float BestD = 1500.f;
				for (TActorIterator<AAstralCharacter> It(Pawn->GetWorld()); It; ++It)
				{
					const float D = FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation());
					if (*It != Pawn && D < BestD) { BestD = D; Best = *It; }
				}
				if (Best) { Gaze = EGaze::Watch; GazeActor = Best; }
				else { GazeYaw = Side() * Rand(30.f, 60.f); GazePitch = Rand(-5.f, 8.f); }
				GazeHold = Rand(1.5f, 3.f);
			}
			else { Gaze = EGaze::Ahead; GazeHold = Rand(1.f, 2.f); }
		}
		if (bRaptor && !bFlying)
		{
			GazeHold *= 1.4f;                                     // hawks hold still between snaps
		}
	}

	float TargetYaw = GazeYaw, TargetPitch = GazePitch;
	if (Gaze == EGaze::Watch && GazeActor.IsValid())
	{
		const FVector From = Mesh && Mesh->GetBoneIndex(TEXT("head")) != INDEX_NONE ? Mesh->GetBoneLocation(TEXT("head")) : Pawn->GetActorLocation();
		// Measured from the body as it is displayed: a circling flyer is
		// banked into its turn, so something below and inside the circle is
		// mostly "down" for it, not a huge sideways twist of the neck (at
		// 100 deg round, Stormrook's neck feathers tore open).
		FTransform Body = Pawn->GetActorTransform();
		if (const AAstralCharacter* Astral = Cast<AAstralCharacter>(Pawn))
		{
			Body.SetRotation(Body.GetRotation() * FQuat(Astral->GetBodyTilt()));
		}
		LookAngles(Body, From, GazeActor->GetActorLocation() + FVector(0.f, 0.f, 40.f), TargetYaw, TargetPitch);
	}
	// Running: shorter, smaller looks (the way ahead matters), except a fleeing look back.
	// A raptor in the air keeps its looks full length (it is hunting, not
	// minding the way ahead).
	const bool bHunting = bRaptor && bFlying;
	// Alert: head held high. Otherwise a slow drift, so the head is never
	// dead still (a hawk barely moves its head between snaps), and the
	// stretch's lifted head.
	if (Activity == EAstralWanderActivity::Alert)
	{
		TargetPitch += 6.f;
	}
	const float Drift = bRaptor ? 0.3f : 1.f;
	TargetYaw += Drift * 4.f * FMath::PerlinNoise1D(LifeTime * 0.35f + NoiseSeed);
	TargetPitch += Drift * 2.5f * FMath::PerlinNoise1D(LifeTime * 0.3f + NoiseSeed + 17.f) + FidgetLook;
	const float Calm = (Mode == EAstralWildlifeMode::Flee || bHunting) ? 1.f : 1.f - 0.5f * RunAlpha * MoveAlpha;
	TargetYaw = FMath::Clamp(TargetYaw, -75.f, 75.f) * Calm;
	TargetPitch = FMath::Clamp(TargetPitch, -65.f, 38.f) * Calm;
	StepSpring(LookYaw, LookYawVel, TargetYaw, Dt, bRaptor ? 22.f : 10.f, bRaptor ? 1.f : 0.85f);
	StepSpring(LookPitch, LookPitchVel, TargetPitch, Dt, bRaptor ? 22.f : 10.f, bRaptor ? 1.f : 0.85f);
}

float UAstralLocomotionAnimInstance::BlinkCurve(float T, float Duration)
{
	if (T < 0.f || T > Duration)
	{
		return 0.f;
	}
	const float Close = 0.4f * Duration;            // lids come down faster than they go up
	return T < Close ? FMath::SmoothStep(0.f, Close, T) : 1.f - FMath::SmoothStep(Close, Duration, T);
}

void UAstralLocomotionAnimInstance::UpdateBlink(float Dt)
{
	if (Dt <= 0.f)
	{
		return;
	}
	// A sharp look elsewhere often comes with a blink.
	const bool bSnapLook = FMath::Abs(LookYaw - LastLookYaw) / Dt > 220.f;
	LastLookYaw = LookYaw;
	BlinkTimer -= Dt;
	if (BlinkT < 0.f && (BlinkTimer <= 0.f || (bSnapLook && BlinkTimer < 1.5f)))
	{
		BlinkT = 0.f;
		bDoubleBlink = FMath::FRand() < 0.2f;
		BlinkTimer = FMath::FRandRange(2.f, 6.f);
	}
	if (BlinkT >= 0.f)
	{
		BlinkT += Dt;
		const float Dur = 0.16f;
		if (BlinkT > Dur && bDoubleBlink && BlinkT < Dur + 0.1f)
		{
			BlinkValue = 0.f;                          // a brief open between the two
		}
		else if (BlinkT > Dur && bDoubleBlink)
		{
			BlinkValue = BlinkCurve(BlinkT - Dur - 0.1f, Dur);
			if (BlinkT > 2.f * Dur + 0.1f) { BlinkT = -1.f; }
		}
		else
		{
			BlinkValue = BlinkCurve(BlinkT, Dur);
			if (BlinkT > Dur && !bDoubleBlink) { BlinkT = -1.f; }
		}
	}
	else
	{
		BlinkValue = 0.f;
	}
	if (CVarAstralForceBlink.GetValueOnGameThread() >= 0.f)
	{
		BlinkValue = FMath::Min(CVarAstralForceBlink.GetValueOnGameThread(), 1.f);
	}
	SetMorphTarget(TEXT("Blink"), BlinkValue);
}

float UAstralLocomotionAnimInstance::GetMaxFootOffset() const
{
	float M = 0.f;
	for (float G : FootGround)
	{
		M = FMath::Max(M, FMath::Abs(G));
	}
	return M;
}

void UAstralLocomotionAnimInstance::UpdateFootIK(float DeltaSeconds, FAstralLocomotionProxy& Proxy)
{
	// Foot IK on uneven ground (2026-10-09; the research brief's "full-body
	// IK solves foot placement on uneven terrain using line traces"). Each
	// foot traces down at its current XY; the ground height there relative
	// to the floor under the body says how far that foot must reach. The
	// body drops by the deepest reach (so a downhill foot can get there);
	// the two-bone leg solve on the worker thread does the rest.
	const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	const bool bGrounded = Move && Move->IsMovingOnGround() && Move->CurrentFloor.bBlockingHit;
	float Lowest = 0.f;
	bool bAny = false;
	for (int32 i = 0; i < 6; ++i)
	{
		float Target = 0.f;
		const FName Foot(FootIKLegs[i].Foot);
		if (bGrounded && Mesh && Mesh->GetBoneIndex(Foot) != INDEX_NONE)
		{
			bAny = true;
			const FVector P = Mesh->GetBoneLocation(Foot, EBoneSpaces::WorldSpace);
			const float FloorZ = Move->CurrentFloor.HitResult.ImpactPoint.Z;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(AstralFootIK), false, Character);
			const FVector From(P.X, P.Y, FloorZ + 50.f);
			const FVector To(P.X, P.Y, FloorZ - 60.f);
			if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
			{
				Target = FMath::Clamp(Hit.ImpactPoint.Z - FloorZ, -45.f, 45.f);
			}
		}
		FootGround[i] = DeltaSeconds > 0.f ? FMath::FInterpTo(FootGround[i], Target, DeltaSeconds, 14.f) : Target;
		Lowest = FMath::Min(Lowest, FootGround[i]);
	}

	// Quadrupeds (all four of fl/fr/bl/br): pitch the body along the slope -
	// front ground minus hind ground - and set it at the average height, the
	// way an animal walking down a bank tips its whole body forward (a level
	// body sunk to the lowest foot dragged Glacielle's rump and tail into
	// the ramp). Two-legged (Stormrook): just lower to the lowest foot.
	const bool bQuad = Mesh && Mesh->GetBoneIndex(TEXT("bl_foot")) != INDEX_NONE;
	const float Front = 0.5f * (FootGround[0] + FootGround[1]);
	const float Hind = 0.5f * (FootGround[2] + FootGround[3]);
	const float Lift = bQuad ? 0.5f * (Front + Hind) : Lowest;
	PelvisDrop = DeltaSeconds > 0.f ? FMath::FInterpTo(PelvisDrop, Lift, DeltaSeconds, 10.f) : Lift;

	Proxy.bFootIK = bAny && Mesh;
	if (!Proxy.bFootIK)
	{
		return;
	}
	// World Z offsets into component space (the mesh is scaled, yawed and rolled).
	const FTransform& C2W = Mesh->GetComponentTransform();
	// Winded: the trunk heaves (only ever down from the clip, so the legs can
	// always reach), standing only - a running gait has its own bob.
	const float Heave = Exertion * (1.f - MoveAlpha) * (0.5f + 0.5f * FMath::Sin(2.f * PI * BreathPhase));
	Proxy.PelvisOffset = C2W.InverseTransformVector(FVector(0.f, 0.f, PelvisDrop - 1.2f * Heave));
	Proxy.BreathNod = 2.5f * Heave;
	Proxy.BodySlope = bQuad ? C2W.InverseTransformVector(FVector(0.f, 0.f, Front - Hind)) : FVector::ZeroVector;
	Proxy.ComponentUp = C2W.InverseTransformVector(FVector::UpVector).GetSafeNormal();
	for (int32 i = 0; i < 6; ++i)
	{
		Proxy.FootOffsets[i] = C2W.InverseTransformVector(FVector(0.f, 0.f, FootGround[i] + PawLift[i]));
	}
}

void UAstralLocomotionAnimInstance::SetClips(UAnimSequence* InIdle, UAnimSequence* InWalk, UAnimSequence* InRun, float InWalkSpeed, float InRunSpeed, float InIdleThreshold)
{
	IdleAnim = InIdle;
	WalkAnim = InWalk;
	RunAnim = InRun;
	WalkSpeed = FMath::Max(InWalkSpeed, 1.f);
	RunSpeed = FMath::Max(InRunSpeed, WalkSpeed + 1.f);
	IdleThreshold = FMath::Max(InIdleThreshold, 0.f);
	// No two Astrals breathe, fidget or drift in step.
	IdleTime = IdleAnim ? FMath::FRand() * IdleAnim->GetPlayLength() : 0.f;
	IdleRate = FMath::FRandRange(0.9f, 1.1f);
	NoiseSeed = FMath::FRandRange(0.f, 100.f);
	FidgetTimer = FMath::FRandRange(2.f, 6.f);
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

	// Targets: fade idle -> moving over the first 40% of walk speed, then
	// walk -> run between the two authored speeds.
	float MoveTarget = !bCanMove ? 0.f : (IdleAnim ? FMath::SmoothStep(IdleThreshold, FMath::Max(IdleThreshold + 1.f, 0.4f * WalkSpeed), Speed) : 1.f);
	// Starting from a stand: begin the cycle with one diagonal pair planted
	// under the body and the other about to lift, so the first step comes
	// out of the standing pose rather than from mid-stride.
	if (MoveAlpha < 0.02f)
	{
		bStopSettled = true;
	}
	if (bStopSettled && MoveTarget > 0.05f && MoveAlpha < 0.05f)
	{
		GaitPhase = FMath::RandBool() ? 0.18f : 0.68f;
		bStopSettled = false;
	}
	if (MoveTarget > 0.6f)
	{
		bStopSettled = false;
	}
	// Stopping: finish the step. Hold the gait until a diagonal pair is
	// planted mid-stance, then settle into the idle (it used to fade out of
	// whatever stride it was in, sliding the feet into place).
	bStopHolding = !bStopSettled && MoveTarget < 0.5f && MoveAlpha > 0.2f && IdleAnim != nullptr;
	StopHoldTime = bStopHolding ? StopHoldTime + DeltaSeconds : 0.f;
	if (bStopHolding)
	{
		MoveTarget = MoveAlpha;
	}
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
	// Finishing a step on stopping goes at a steady stepping pace.
	const float Cps = FMath::Max(FMath::Lerp(WalkCps, RunCps, RunAlpha), (bStopHolding ? 0.8f : 0.5f) / FMath::Lerp(WalkLen, RunLen, RunAlpha));
	const float PhaseStep = DeltaSeconds * Cps;
	if (bStopHolding && (ReachesSettlePhase(GaitPhase, PhaseStep) || StopHoldTime > 0.6f))
	{
		bStopSettled = true;
	}
	GaitPhase = FMath::Frac(GaitPhase + PhaseStep);

	const float IdleLen = IdleAnim ? FMath::Max(IdleAnim->GetPlayLength(), 0.01f) : 1.f;
	IdleTime = FMath::Fmod(IdleTime + DeltaSeconds * IdleRate, IdleLen);

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
		// Plus a slow wander of the spine while moving, so no two strides are quite the same.
		const float Wobble = 2.f * MoveAlpha * FMath::PerlinNoise1D(LifeTime * 0.45f + NoiseSeed + 40.f);
		TurnBend = FMath::FInterpTo(TurnBend, FMath::Clamp(YawRate * 0.1f + Wobble, -22.f, 22.f), DeltaSeconds, 4.f);
	}

	// The head leads the turn (gazelle cutting away from a cheetah, a runner
	// looking to the next base): it looks toward where the Astral is steering
	// (its input acceleration) before the body swings round, then settles as
	// the body catches up.
	float LeadTarget = 0.f;
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		const FVector Accel = Character->GetCharacterMovement()->GetCurrentAcceleration();
		if (Accel.SizeSquared2D() > 1.f && Speed > IdleThreshold)
		{
			LeadTarget = FMath::Clamp(FMath::FindDeltaAngleDegrees(Yaw, Accel.Rotation().Yaw), -HeadLeadMax, HeadLeadMax) * MoveAlpha;
		}
	}
	if (DeltaSeconds > 0.f)
	{
		HeadLead = FMath::FInterpTo(HeadLead, LeadTarget, DeltaSeconds, 6.f);
	}

	// Tail spring (secondary motion): the tail lags the body. Turning swings
	// it to the outside; speeding up pushes it back and down, braking throws
	// it forward and up; then it overshoots and settles (under-damped), so a
	// stop reads as weight in the tail rather than a freeze.
	float ForwardAccel = 0.f;
	if (DeltaSeconds > 0.f && Pawn)
	{
		const float ForwardSpeed = FVector::DotProduct(Pawn->GetVelocity(), Pawn->GetActorForwardVector());
		ForwardAccel = (ForwardSpeed - LastForwardSpeed) / DeltaSeconds;
		LastForwardSpeed = ForwardSpeed;
		StepSpring(TailYaw, TailYawVel, FMath::Clamp(-0.08f * YawRate, -28.f, 28.f), DeltaSeconds);
		StepSpring(TailPitch, TailPitchVel, FMath::Clamp(-0.012f * ForwardAccel, -18.f, 18.f), DeltaSeconds, 7.f, 0.35f);
	}

	FAstralLocomotionProxy& Proxy = GetProxyOnGameThread<FAstralLocomotionProxy>();
	Proxy.TurnBend = TurnBend;
	Proxy.HeadLead = HeadLead;
	Proxy.TailYaw = TailYaw;
	Proxy.TailPitch = TailPitch;
	UpdateLife(DeltaSeconds, ForwardAccel);
	UpdateGaze(DeltaSeconds);
	UpdateBlink(DeltaSeconds);
	Proxy.LookYaw = LookYaw;
	Proxy.LookPitch = LookPitch + SniffPitch;
	Proxy.BodyPitch = BodyPitch;
	Proxy.BodyRoll = 0.3f * ShakeRoll;
	Proxy.ShakeRoll = ShakeRoll;
	UpdateFootIK(DeltaSeconds, Proxy);
	Proxy.Layers.Reset();
	auto Add = [&Proxy](const UAnimSequence* Seq, float Time, float Weight)
	{
		if (Seq && Weight > 0.001f)
		{
			Proxy.Layers.Add({ Seq, Time, Weight });
		}
	};
	// Flyers (Stormrook): in the air the ground clips fade out for Fly
	// (flapping) and Glide (wings held spread). It flaps taking off,
	// climbing and when slow (the landing flare); it glides when cruising
	// level or diving. Fly plays faster when climbing hard.
	const UCharacterMovementComponent* MoveComp = Pawn ? Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()) : nullptr;
	const bool bFlying = (FlyAnim || GlideAnim) && MoveComp && MoveComp->IsFlying();
	const FVector Vel = Pawn ? Pawn->GetVelocity() : FVector::ZeroVector;
	// TJ (2026-10-08): "the wings do not flap at all" - it used to glide
	// whenever it cruised level, which was most of a flight. Now it flaps in
	// bursts (raven-style: ~4s of wingbeats, then a ~1.2s glide), and glides
	// longer only when diving. It always flaps climbing or slow.
	if (bFlying && DeltaSeconds > 0.f)
	{
		FlapBoutTime += DeltaSeconds;
		if (FlapBoutTime > FlapBoutLength + GlideBoutLength)
		{
			FlapBoutTime = 0.f;
		}
	}
	const bool bGlideBout = FlapBoutTime > FlapBoutLength;
	const bool bDiving = Vel.Z < -220.f;
	const bool bFlapping = !GlideAnim || Vel.Z > 60.f || Vel.Size2D() < 300.f || (!bGlideBout && !bDiving);
	if (DeltaSeconds > 0.f)
	{
		FlightAlpha = FMath::FInterpTo(FlightAlpha, bFlying ? 1.f : 0.f, DeltaSeconds, 6.f);
		FlapAlpha = FMath::FInterpTo(FlapAlpha, (bFlapping && FlyAnim) ? 1.f : 0.f, DeltaSeconds, 4.f);
		const float FlyLen = FlyAnim ? FMath::Max(FlyAnim->GetPlayLength(), 0.01f) : 1.f;
		const float GlideLen = GlideAnim ? FMath::Max(GlideAnim->GetPlayLength(), 0.01f) : 1.f;
		FlyTime = FMath::Fmod(FlyTime + DeltaSeconds * (Vel.Z > 150.f ? 1.4f : 1.f), FlyLen);
		GlideTime = FMath::Fmod(GlideTime + DeltaSeconds, GlideLen);

		// Braking (TJ: "if it slows down then its wings turn forward to slow
		// momentum"): flare while losing speed in the air, or flying slow and
		// not climbing (the landing approach). Not on take-off, which is slow
		// but climbing hard and beats its wings.
		const float AirSpeed = Vel.Size2D();
		const float Decel = bFlying ? (LastAirSpeed - AirSpeed) / DeltaSeconds : 0.f;
		LastAirSpeed = AirSpeed;
		SmoothedDecel = FMath::FInterpTo(SmoothedDecel, Decel, DeltaSeconds, 5.f);
		const bool bBraking = bFlying && FlareAnim && Vel.Z < 100.f && (SmoothedDecel > 120.f || AirSpeed < 260.f);
		BrakeAlpha = FMath::FInterpTo(BrakeAlpha, bBraking ? 1.f : 0.f, DeltaSeconds, bBraking ? 5.f : 2.5f);
		const float FlareLen = FlareAnim ? FMath::Max(FlareAnim->GetPlayLength(), 0.01f) : 1.f;
		FlareTime = FMath::Fmod(FlareTime + DeltaSeconds, FlareLen);
	}

	const float Ground = 1.f - FlightAlpha;
	const float Move = bCanMove ? MoveAlpha : 0.f;
	Add(IdleAnim, IdleTime, Ground * (IdleAnim ? 1.f - Move : 0.f));
	Add(WalkAnim, GaitPhase * WalkLen, Ground * Move * (RunAnim ? 1.f - RunAlpha : 1.f));
	Add(RunAnim, GaitPhase * RunLen, Ground * Move * (WalkAnim ? RunAlpha : 1.f));
	const float Cruise = FlightAlpha * (1.f - BrakeAlpha);
	Add(FlyAnim, FlyTime, Cruise * FlapAlpha);
	Add(GlideAnim, GlideTime, Cruise * (1.f - FlapAlpha));
	Add(FlareAnim, FlareTime, FlightAlpha * BrakeAlpha);

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
		ApplyFootIK(Output);
		return true;
	}

	// Same pattern as UBlendSpace::GetAnimationPose_Internal.
	TArray<FCompactPose, TInlineAllocator<6>> Poses;
	TArray<FBlendedCurve, TInlineAllocator<6>> Curves;
	TArray<UE::Anim::FStackAttributeContainer, TInlineAllocator<6>> Attributes;
	TArray<float, TInlineAllocator<6>> Weights;
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
	ApplyFootIK(Output);
	return true;
}

void FAstralLocomotionProxy::CacheBendBones(const FBoneContainer& Bones)
{
	BendBones.Reset();
	BendBonesFor = &Bones;
	BendBonesSerial = Bones.GetSerialNumber();

	// Front of the body leads into the turn; the tail swings to the inside of
	// the arc too, which for a backward-pointing chain is the opposite sign.
	// Third column: share of HeadLead (neck turns first, head finishes it).
	// Fourth column: share of the tail spring, growing toward the tip (the
	// spring now carries the tail's swing into turns; it used to be a fixed
	// share of TurnBend with no overshoot).
	// Fifth and sixth: shares of a shake's roll and the winded nod (neck and head).
	struct FShare { const TCHAR* Bone; float Bend; float Lead; float Tail; float Roll; float Nod; };
	static const FShare Shares[] = {
		{ TEXT("spine_01"), 0.15f, 0.f, 0.f, 0.f, 0.f }, { TEXT("chest"), 0.25f, 0.f, 0.f, 0.f, 0.f }, { TEXT("neck"), 0.3f, 0.4f, 0.f, 0.4f, 0.6f }, { TEXT("head"), 0.3f, 0.6f, 0.f, 0.6f, 0.4f },
		{ TEXT("tail_01"), 0.f, 0.f, 0.15f, 0.f, 0.f }, { TEXT("tail_02"), 0.f, 0.f, 0.22f, 0.f, 0.f }, { TEXT("tail_03"), 0.f, 0.f, 0.28f, 0.f, 0.f }, { TEXT("tail_04"), 0.f, 0.f, 0.35f, 0.f, 0.f },
	};
	const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();

	// The tail's sideways axis (for its up/down swing): across the tail's
	// rest direction, in component space.
	FVector TailSide = FVector::RightVector;
	{
		const int32 T1 = Ref.FindBoneIndex(TEXT("tail_01"));
		const int32 T4 = Ref.FindBoneIndex(TEXT("tail_04"));
		if (T1 != INDEX_NONE && T4 != INDEX_NONE)
		{
			const FVector D = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, T4).GetLocation() - FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, T1).GetLocation();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, D).GetSafeNormal();
			if (!Side.IsNearlyZero())
			{
				TailSide = Side;
			}
		}
	}

	// The body's sideways axis for looking up/down: across the horizontal
	// line from the pelvis to the head (works for upright Stormrook too).
	FVector LookSide = FVector::RightVector;
	FVector BodyForward = FVector::ForwardVector;   // the roll axis of a shake
	{
		const int32 P = Ref.FindBoneIndex(TEXT("pelvis"));
		const int32 H = Ref.FindBoneIndex(TEXT("head"));
		if (P != INDEX_NONE && H != INDEX_NONE)
		{
			FVector D = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, H).GetLocation() - FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, P).GetLocation();
			D.Z = 0.f;
			const FVector S = FVector::CrossProduct(D, FVector::UpVector).GetSafeNormal();
			if (!S.IsNearlyZero())
			{
				LookSide = S;   // + rotation about it tips the head up
				BodyForward = D.GetSafeNormal();
			}
		}
	}

	// Leg chains and the pelvis, for foot IK.
	auto Compact = [&](const TCHAR* Name)
	{
		const int32 MeshIndex = Ref.FindBoneIndex(FName(Name));
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	};
	for (int32 i = 0; i < 6; ++i)
	{
		LegChains[i] = { Compact(FootIKLegs[i].Upper), Compact(FootIKLegs[i].Lower), Compact(FootIKLegs[i].Foot) };
	}
	PelvisIndex = Compact(TEXT("pelvis"));

	for (const FShare& S : Shares)
	{
		const int32 MeshIndex = Ref.FindBoneIndex(FName(S.Bone));
		if (MeshIndex == INDEX_NONE)
		{
			continue;
		}
		const FCompactPoseBoneIndex BoneIndex = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		const int32 ParentIndex = Ref.GetParentIndex(MeshIndex);
		if (BoneIndex == INDEX_NONE || ParentIndex == INDEX_NONE)
		{
			continue;
		}
		// Component up expressed in the parent's reference-pose frame; the
		// clips only rotate these bones a few degrees, so the ref frame is close enough.
		const FTransform ParentRef = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, ParentIndex);
		BendBones.Add({ BoneIndex, ParentRef.GetRotation().UnrotateVector(FVector::UpVector).GetSafeNormal(),
			ParentRef.GetRotation().UnrotateVector(TailSide).GetSafeNormal(), S.Bend, S.Lead, S.Tail,
			ParentRef.GetRotation().UnrotateVector(LookSide).GetSafeNormal(), S.Lead,   // the gaze uses the head-lead shares: neck 40%, head 60%
			ParentRef.GetRotation().UnrotateVector(BodyForward).GetSafeNormal(), S.Roll, S.Nod });
	}
}

void FAstralLocomotionProxy::ApplyTurnBend(FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	if (BendBonesFor != &Bones || BendBonesSerial != Bones.GetSerialNumber())
	{
		CacheBendBones(Bones);
	}
	if (FMath::Abs(TurnBend) < 0.05f && FMath::Abs(HeadLead) < 0.05f && FMath::Abs(TailYaw) < 0.05f && FMath::Abs(TailPitch) < 0.05f && FMath::Abs(LookYaw) < 0.05f && FMath::Abs(LookPitch) < 0.05f
		&& FMath::Abs(ShakeRoll) < 0.05f && FMath::Abs(BreathNod) < 0.05f)
	{
		return;
	}
	for (const FBendBone& B : BendBones)
	{
		FTransform& Local = Output.Pose[B.Index];
		const FQuat Yaw(B.Axis, FMath::DegreesToRadians(TurnBend * B.Share + HeadLead * B.LeadShare + TailYaw * B.TailShare + LookYaw * B.LookShare));
		const FQuat Pitch(B.PitchAxis, FMath::DegreesToRadians(TailPitch * B.TailShare));
		const FQuat Look(B.LookPitchAxis, FMath::DegreesToRadians(LookPitch * B.LookShare + BreathNod * B.NodShare));
		const FQuat Roll(B.RollAxis, FMath::DegreesToRadians(ShakeRoll * B.RollShare));
		Local.SetRotation((Yaw * Look * Pitch * Roll * Local.GetRotation()).GetNormalized());
	}
}

void FAstralLocomotionProxy::ApplyFootIK(FPoseContext& Output)
{
	if (!bFootIK || !PelvisIndex.IsValid())
	{
		return;
	}
	bool bAnyOffset = PelvisOffset.SizeSquared() > 0.01f || BodySlope.SizeSquared() > 0.01f || FMath::Abs(BodyPitch) > 0.05f || FMath::Abs(BodyRoll) > 0.05f;
	for (const FVector& F : FootOffsets)
	{
		bAnyOffset |= F.SizeSquared() > 0.01f;
	}
	if (!bAnyOffset)
	{
		return;
	}

	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);

	// Where the clip put each foot, before the body moves: the targets are
	// these plus the ground offsets.
	FVector AnkleFromClip[6];
	for (int32 i = 0; i < 6; ++i)
	{
		AnkleFromClip[i] = LegChains[i].Foot.IsValid() ? CS.GetComponentSpaceTransform(LegChains[i].Foot).GetLocation() : FVector::ZeroVector;
	}

	// Pitch the body along the slope about the middle of its back (between
	// the hips and the shoulders), then raise/lower it; everything under the
	// pelvis follows.
	FQuat Pitch = FQuat::Identity;
	FVector Pivot = CS.GetComponentSpaceTransform(PelvisIndex).GetLocation();
	const bool bQuad = LegChains[0].Upper.IsValid() && LegChains[1].Upper.IsValid() && LegChains[2].Upper.IsValid() && LegChains[3].Upper.IsValid();
	FVector Shoulders = Pivot, Hips = Pivot;
	if (bQuad)
	{
		Shoulders = 0.5f * (CS.GetComponentSpaceTransform(LegChains[0].Upper).GetLocation() + CS.GetComponentSpaceTransform(LegChains[1].Upper).GetLocation());
		Hips = 0.5f * (CS.GetComponentSpaceTransform(LegChains[2].Upper).GetLocation() + CS.GetComponentSpaceTransform(LegChains[3].Upper).GetLocation());
	}
	const FVector Forward = Shoulders - Hips;
	if (!BodySlope.IsNearlyZero() && bQuad)
	{
		// Feet spread wider than the hips-to-shoulders line, so scale the
		// ground rise to it: rise over the feet's span, same angle.
		const float FootSpan = FMath::Max((0.5f * (AnkleFromClip[0] + AnkleFromClip[1]) - 0.5f * (AnkleFromClip[2] + AnkleFromClip[3])).Size(), 1.f);
		const FVector Rise = BodySlope * (Forward.Size() / FootSpan);
		Pitch = FQuat::FindBetweenVectors(Forward, Forward + Rise);
		const float MaxPitch = FMath::DegreesToRadians(30.f);
		if (Pitch.GetAngle() > MaxPitch)
		{
			Pitch = FQuat(Pitch.GetRotationAxis(), MaxPitch);
		}
		Pivot = 0.5f * (Shoulders + Hips);
	}
	FTransform Pelvis = CS.GetComponentSpaceTransform(PelvisIndex);
	auto RotateAbout = [&Pelvis](const FQuat& Q, const FVector& At)
	{
		Pelvis.SetLocation(Q.RotateVector(Pelvis.GetLocation() - At) + At);
		Pelvis.SetRotation((Q * Pelvis.GetRotation()).GetNormalized());
	};
	RotateAbout(Pitch, Pivot);
	// Weight shift on top: nose up about the shoulders (hindquarters squat),
	// nose down about the hips (chest drops), so the body only ever lowers and
	// the legs can always reach their feet; then a shake's roll along the back.
	const FVector Side = FVector::CrossProduct(Forward, ComponentUp).GetSafeNormal();
	if (bQuad && !Side.IsNearlyZero() && (FMath::Abs(BodyPitch) > 0.05f || FMath::Abs(BodyRoll) > 0.05f))
	{
		const FVector At = Pitch.RotateVector((BodyPitch > 0.f ? Shoulders : Hips) - Pivot) + Pivot;
		RotateAbout(FQuat(Pitch.RotateVector(Side), FMath::DegreesToRadians(BodyPitch)), At);
		const FVector Mid = Pitch.RotateVector(0.5f * (Shoulders + Hips) - Pivot) + Pivot;
		RotateAbout(FQuat(Pitch.RotateVector(Forward).GetSafeNormal(), FMath::DegreesToRadians(BodyRoll)), Mid);
	}
	Pelvis.AddToTranslation(PelvisOffset);
	CS.SafeSetCSBoneTransforms({ FBoneTransform(PelvisIndex, Pelvis) });

	// Two-bone solve per leg: the knee stays in its plane (pole = where it
	// is now), the foot keeps its orientation and moves by its offset.
	TArray<FBoneTransform> Out;
	for (int32 i = 0; i < 6; ++i)
	{
		const FLegChain& L = LegChains[i];
		if (!L.Upper.IsValid() || !L.Lower.IsValid() || !L.Foot.IsValid())
		{
			continue;
		}
		const FTransform U = CS.GetComponentSpaceTransform(L.Upper);
		const FTransform K = CS.GetComponentSpaceTransform(L.Lower);
		const FTransform F = CS.GetComponentSpaceTransform(L.Foot);
		const FVector Hip = U.GetLocation(), Knee = K.GetLocation(), Ankle = F.GetLocation();
		FVector NewKnee, NewAnkle;
		AnimationCore::SolveTwoBoneIK(Hip, Knee, Ankle, Knee, AnkleFromClip[i] + FootOffsets[i], NewKnee, NewAnkle,
			(Knee - Hip).Size(), (Ankle - Knee).Size(), false, 1.0, 1.0);
		FTransform NU = U, NK = K, NF = F;
		NU.SetRotation((FQuat::FindBetweenVectors(Knee - Hip, NewKnee - Hip) * U.GetRotation()).GetNormalized());
		NK.SetLocation(NewKnee);
		NK.SetRotation((FQuat::FindBetweenVectors(Ankle - Knee, NewAnkle - NewKnee) * K.GetRotation()).GetNormalized());
		NF.SetLocation(NewAnkle);
		Out.Add(FBoneTransform(L.Upper, NU));
		Out.Add(FBoneTransform(L.Lower, NK));
		Out.Add(FBoneTransform(L.Foot, NF));
	}
	if (Out.Num())
	{
		Out.Sort(FCompareBoneTransformIndex());
		CS.SafeSetCSBoneTransforms(Out);
	}
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
}
