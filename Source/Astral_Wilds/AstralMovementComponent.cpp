// Astral Wilds - see AstralMovementComponent.h.
#include "AstralMovementComponent.h"

float UAstralMovementComponent::TurnAccelerationNow() const
{
	return IsFlying() ? MaxFlightTurnAcceleration : MaxTurnAcceleration;
}

float UAstralMovementComponent::ArcAlpha(float Speed) const
{
	return FMath::SmoothStep(ArcMinSpeed, FMath::Max(ArcFullSpeed, ArcMinSpeed + 1.f), Speed);
}

FVector UAstralMovementComponent::ClampSteer(const FVector& InputAcceleration, const FVector& CurrentVelocity, float MaxAngleDeg) const
{
	const FVector Vel2D(CurrentVelocity.X, CurrentVelocity.Y, 0.f);
	const float Speed = Vel2D.Size();
	const float InputSize = InputAcceleration.Size2D();
	if (Speed < ArcMinSpeed || InputSize < KINDA_SMALL_NUMBER)
	{
		return InputAcceleration;
	}

	const float MaxAngle = FMath::DegreesToRadians(FMath::Lerp(180.f, FMath::Clamp(MaxAngleDeg, 0.f, 180.f), ArcAlpha(Speed)));
	const FVector Heading = Vel2D / Speed;
	const FVector Desired = FVector(InputAcceleration.X, InputAcceleration.Y, 0.f) / InputSize;
	const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Heading, Desired), -1.f, 1.f));
	if (Angle <= MaxAngle)
	{
		return InputAcceleration;
	}

	// Rotate the heading toward the desired side by MaxAngle. Dead astern has
	// no side, so pick one (left) rather than braking straight back.
	float Side = FVector::CrossProduct(Heading, Desired).Z;
	Side = FMath::Abs(Side) < KINDA_SMALL_NUMBER ? 1.f : FMath::Sign(Side);
	const FVector Steer = Heading.RotateAngleAxisRad(MaxAngle * Side, FVector::UpVector);
	return FVector(Steer.X * InputSize, Steer.Y * InputSize, InputAcceleration.Z);
}

FVector UAstralMovementComponent::LimitHeadingChange(const FVector& OldVelocity, const FVector& NewVelocity, float DeltaTime) const
{
	const FVector Old2D(OldVelocity.X, OldVelocity.Y, 0.f);
	const FVector New2D(NewVelocity.X, NewVelocity.Y, 0.f);
	const float OldSpeed = Old2D.Size();
	const float NewSpeed = New2D.Size();
	if (OldSpeed < ArcMinSpeed || NewSpeed < KINDA_SMALL_NUMBER || DeltaTime <= 0.f)
	{
		return NewVelocity;
	}

	const float MaxStep = FMath::Lerp(PI, FMath::Max(TurnAccelerationNow(), 1.f) / OldSpeed * DeltaTime, ArcAlpha(OldSpeed));
	const FVector OldDir = Old2D / OldSpeed;
	const FVector NewDir = New2D / NewSpeed;
	const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(OldDir, NewDir), -1.f, 1.f));
	if (Angle <= MaxStep)
	{
		return NewVelocity;
	}
	float Side = FVector::CrossProduct(OldDir, NewDir).Z;
	Side = FMath::Abs(Side) < KINDA_SMALL_NUMBER ? 1.f : FMath::Sign(Side);
	const FVector Dir = OldDir.RotateAngleAxisRad(MaxStep * Side, FVector::UpVector);
	return FVector(Dir.X * NewSpeed, Dir.Y * NewSpeed, NewVelocity.Z);
}

float UAstralMovementComponent::SteerAngleForStep(float Speed, float DeltaTime, float Friction) const
{
	// Friction pulls the velocity a share K of the way to the steering
	// direction each step, which turns it by about K x the steer angle. Aim
	// just far enough off the heading for that to equal the allowed turn.
	const float K = FMath::Clamp(DeltaTime * Friction, 0.05f, 1.f);
	const float MaxStep = FMath::Max(TurnAccelerationNow(), 1.f) / FMath::Max(Speed, 1.f) * DeltaTime;
	return FMath::Clamp(FMath::RadiansToDegrees(MaxStep / K), 1.f, MaxSteerAngle);
}

FRotator UAstralMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const
{
	// From rest (or a slow walk) turn toward where it's going, as before.
	if (Velocity.Size2D() < ArcMinSpeed)
	{
		return Super::ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRotation);
	}
	return Velocity.GetSafeNormal2D().Rotation();
}

void UAstralMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	const FVector OldVelocity = Velocity;
	const FVector Intent = Acceleration;
	// Flying turns carve arcs too (a soaring bird banks round, it never pivots).
	const bool bArc = (IsMovingOnGround() || IsFlying()) && DeltaTime > 0.f;
	if (bArc)
	{
		Acceleration = ClampSteer(Acceleration, Velocity, SteerAngleForStep(Velocity.Size2D(), DeltaTime, Friction));
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
	if (bArc)
	{
		// Backstop for anything else that swings the velocity (e.g. a requested direct move).
		Velocity = LimitHeadingChange(OldVelocity, Velocity, DeltaTime);
	}
	// Keep the unclamped intent as the current acceleration: the anim's head
	// lead looks where the Astral wants to go, not where it can steer this frame.
	Acceleration = Intent;
}
