// Astral Wilds - character movement for Astrals: turns carve arcs instead of
// pivoting. Reference (TJ, 2026-10-08: "more like a baseball player rounding
// a base or a gazelle turning as it's being chased"): a runner at speed never
// steers straight at a corner - it bows out and keeps its speed round a curve
// (the base-runner's "banana route"), and a gazelle cuts on an arc while
// banking into it. Notes: art repo Docs/Design/TurnReference.md.
//
// The stock walking physics turns the velocity toward the steering input
// through ground friction, at ~500 deg/s, and cuts the corner of that turn:
// steered 70 deg off its heading, an Astral loses ~14% of its speed a frame,
// slows to a walk and then pivots. Here the steering input is aimed only as
// far off the heading as friction needs to turn at the allowed rate, so the
// velocity swings round smoothly at (nearly) full speed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AstralMovementComponent.generated.h"

UCLASS()
class UAstralMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	/**
	 * Most sideways (centripetal) acceleration a turn may use, cm/s^2. It
	 * caps how fast the heading can swing: speed^2 / this is the tightest
	 * arc, ~2.5m at a 450 cm/s run, while a 140 cm/s walk can still turn
	 * almost on the spot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Turning", meta = (Units = "cm/s^2"))
	float MaxTurnAcceleration = 800.f;

	/** The same limit while flying (a flyer's species sets it from FAstralFlightTuning::TurnAcceleration). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Turning", meta = (Units = "cm/s^2"))
	float MaxFlightTurnAcceleration = 700.f;

	/** Largest angle (deg) the steering input may point off the current heading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Turning")
	float MaxSteerAngle = 70.f;

	/** Below this speed turning is unlimited (slow walks and starts from rest can turn on the spot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Turning", meta = (Units = "cm/s"))
	float ArcMinSpeed = 100.f;

	/** The turning limits ease in between ArcMinSpeed and this speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Turning", meta = (Units = "cm/s"))
	float ArcFullSpeed = 350.f;

	/** InputAcceleration turned to at most MaxAngleDeg off CurrentVelocity's heading (eased in by speed), same size. Exposed for tests. */
	FVector ClampSteer(const FVector& InputAcceleration, const FVector& CurrentVelocity, float MaxAngleDeg) const;

	/** NewVelocity with its heading turned at most MaxTurnAcceleration / speed (rad/s) from OldVelocity's. Exposed for tests. */
	FVector LimitHeadingChange(const FVector& OldVelocity, const FVector& NewVelocity, float DeltaTime) const;

	/** How far (deg) to aim the steering off the heading this step, given ground friction: enough to turn at the allowed rate. Exposed for tests. */
	float SteerAngleForStep(float Speed, float DeltaTime, float Friction) const;

protected:

	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

	/** Faces along the velocity once moving (stock faces the acceleration, which on an arc points at the goal, so the body crabbed sideways round the curve). */
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;

private:

	/** 0 at ArcMinSpeed -> 1 at ArcFullSpeed. */
	float ArcAlpha(float Speed) const;

	/** MaxFlightTurnAcceleration while flying, else MaxTurnAcceleration. */
	float TurnAccelerationNow() const;
};
