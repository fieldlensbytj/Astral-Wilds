// Astral Wilds - automation tests for UAstralMovementComponent's steer clamp
// (turns carve arcs at speed, see AstralMovementComponent.h). ClampSteer is
// pure maths on its inputs, so no world is needed.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralCharacter.h"
#include "AstralMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralMovement_SteerClamp, "AstralWilds.Movement.SteerClamp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralMovement_SteerClamp::RunTest(const FString& Parameters)
{
	UAstralMovementComponent* Move = NewObject<UAstralMovementComponent>();
	Move->MaxSteerAngle = 70.f;
	Move->ArcMinSpeed = 100.f;
	Move->ArcFullSpeed = 350.f;
	auto AngleBetween = [](const FVector& A, const FVector& B)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(A.GetSafeNormal2D(), B.GetSafeNormal2D())));
	};

	// Slow (or from rest): no clamp, so a walk can still turn on the spot.
	const FVector Back(-900.f, 0.f, 0.f);
	TestEqual(TEXT("Below ArcMinSpeed the input is unchanged"), Move->ClampSteer(Back, FVector(80.f, 0.f, 0.f), 70.f), Back);
	TestEqual(TEXT("From rest the input is unchanged"), Move->ClampSteer(Back, FVector::ZeroVector, 70.f), Back);

	// Full speed, gentle steer inside the limit: unchanged.
	const FVector Gentle = FVector(900.f, 0.f, 0.f).RotateAngleAxis(40.f, FVector::UpVector);
	TestEqual(TEXT("A steer inside MaxSteerAngle is unchanged"), Move->ClampSteer(Gentle, FVector(450.f, 0.f, 0.f), 70.f), Gentle);

	// Full speed, hard right (120 deg): clamped to 70 deg on the same side, same magnitude.
	const FVector Hard = FVector(900.f, 0.f, 0.f).RotateAngleAxis(120.f, FVector::UpVector);
	const FVector Clamped = Move->ClampSteer(Hard, FVector(450.f, 0.f, 0.f), 70.f);
	TestTrue(TEXT("A hard steer is clamped to MaxSteerAngle"), FMath::IsNearlyEqual(AngleBetween(Clamped, FVector::ForwardVector), 70.f, 0.5f));
	TestTrue(TEXT("The clamp keeps the steer's side"), Clamped.Y > 0.f);
	TestTrue(TEXT("The clamp keeps the input's size"), FMath::IsNearlyEqual(Clamped.Size2D(), 900.f, 1.f));
	TestTrue(TEXT("At speed the clamped steer still pushes forward (an arc, not a brake)"), Clamped.X > 0.f);

	// Dead astern at full speed: swings round one side instead of braking straight back.
	const FVector Reverse = Move->ClampSteer(Back, FVector(450.f, 0.f, 0.f), 70.f);
	TestTrue(TEXT("Reversing at speed is clamped to MaxSteerAngle"), FMath::IsNearlyEqual(AngleBetween(Reverse, FVector::ForwardVector), 70.f, 0.5f));

	// Heading limit: at 450 cm/s with 800 cm/s^2, one 1/30s step turns at most
	// 800/450/30 rad (~3.4 deg); a walk-speed step below ArcMinSpeed is free.
	Move->MaxTurnAcceleration = 800.f;
	const FVector Run(450.f, 0.f, 0.f);
	const FVector Swung = FVector(450.f, 0.f, 0.f).RotateAngleAxis(60.f, FVector::UpVector);
	const FVector Limited = Move->LimitHeadingChange(Run, Swung, 1.f / 30.f);
	TestTrue(TEXT("At a run the heading turns by MaxTurnAcceleration / speed per second"),
		FMath::IsNearlyEqual(AngleBetween(Limited, Run), FMath::RadiansToDegrees(800.f / 450.f / 30.f), 0.05f));
	TestTrue(TEXT("The limited velocity keeps its speed"), FMath::IsNearlyEqual(Limited.Size2D(), 450.f, 0.5f));
	TestTrue(TEXT("The limited velocity turns toward the new heading's side"), Limited.Y > 0.f);
	const FVector Slow(80.f, 0.f, 0.f);
	const FVector SlowSwung = FVector(80.f, 0.f, 0.f).RotateAngleAxis(60.f, FVector::UpVector);
	TestEqual(TEXT("Below ArcMinSpeed the heading may change freely"), Move->LimitHeadingChange(Slow, SlowSwung, 1.f / 30.f), SlowSwung);

	// Steering off the heading only as far as friction needs: at 450 cm/s,
	// 1/30s, friction 8 (K = 0.267) it is 800/450/30 / 0.267 rad, ~12.7 deg -
	// small, so the turn barely cuts the corner and the speed holds.
	TestTrue(TEXT("Steer angle per step matches the allowed turn rate under friction"),
		FMath::IsNearlyEqual(Move->SteerAngleForStep(450.f, 1.f / 30.f, 8.f), FMath::RadiansToDegrees(800.f / 450.f / 30.f / (8.f / 30.f)), 0.05f));
	TestEqual(TEXT("A slow step may steer up to MaxSteerAngle"), Move->SteerAngleForStep(20.f, 1.f / 30.f, 8.f), 70.f);

	// Astrals get this component.
	AAstralCharacter* Astral = NewObject<AAstralCharacter>();
	TestNotNull(TEXT("AAstralCharacter uses UAstralMovementComponent"), Cast<UAstralMovementComponent>(Astral->GetCharacterMovement()));
	return true;
}

#endif
