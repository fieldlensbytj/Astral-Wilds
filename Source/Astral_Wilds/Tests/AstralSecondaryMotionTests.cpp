// Astral Wilds - automation tests for the tail spring that drives Astral
// secondary motion (UAstralLocomotionAnimInstance::StepSpring). Foot IK needs
// a world and a rigged mesh; it is judged with Astral.MotionCapture <Name> 20 Ramps.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralLocomotionAnimInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralSecondary_TailSpring, "AstralWilds.Animation.TailSpring", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralSecondary_TailSpring::RunTest(const FString& Parameters)
{
	// Pulled toward 20 deg from rest at 60 fps: it overshoots (weight in the
	// tail), then settles on the target within a couple of seconds.
	float Value = 0.f, Vel = 0.f, Peak = 0.f;
	for (int32 f = 0; f < 120; ++f)
	{
		UAstralLocomotionAnimInstance::StepSpring(Value, Vel, 20.f, 1.f / 60.f);
		Peak = FMath::Max(Peak, Value);
	}
	TestTrue(TEXT("The tail overshoots its target (under-damped)"), Peak > 21.f);
	TestTrue(TEXT("...but not wildly"), Peak < 35.f);
	TestTrue(TEXT("It settles on the target"), FMath::IsNearlyEqual(Value, 20.f, 0.5f));

	// A long hitch (0.5s) is sub-stepped and stays finite and bounded.
	float V2 = 0.f, Vel2 = 0.f;
	UAstralLocomotionAnimInstance::StepSpring(V2, Vel2, 20.f, 0.5f);
	TestTrue(TEXT("A long frame stays bounded"), FMath::IsFinite(V2) && FMath::Abs(V2) < 40.f);

	// Released from a swing, it returns to rest.
	float V3 = 25.f, Vel3 = 0.f;
	for (int32 f = 0; f < 180; ++f)
	{
		UAstralLocomotionAnimInstance::StepSpring(V3, Vel3, 0.f, 1.f / 60.f);
	}
	TestTrue(TEXT("Released, it comes back to rest"), FMath::Abs(V3) < 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralSecondary_LookAngles, "AstralWilds.Animation.LookAngles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralSecondary_LookAngles::RunTest(const FString& Parameters)
{
	// A body at the origin facing +X (yaw 0), head at 1m up.
	const FTransform Body(FRotator::ZeroRotator, FVector::ZeroVector);
	const FVector Head(0.f, 0.f, 100.f);
	float Y = 0.f, P = 0.f;
	UAstralLocomotionAnimInstance::LookAngles(Body, Head, FVector(500.f, 0.f, 100.f), Y, P);
	TestTrue(TEXT("Straight ahead: yaw 0, pitch 0"), FMath::IsNearlyZero(Y, 0.01f) && FMath::IsNearlyZero(P, 0.01f));
	UAstralLocomotionAnimInstance::LookAngles(Body, Head, FVector(0.f, 500.f, 100.f), Y, P);
	TestTrue(TEXT("Something to the right (+Y): yaw +90"), FMath::IsNearlyEqual(Y, 90.f, 0.01f));
	UAstralLocomotionAnimInstance::LookAngles(Body, Head, FVector(100.f, 0.f, 0.f), Y, P);
	TestTrue(TEXT("The ground just ahead: pitch -45 (looking down)"), FMath::IsNearlyEqual(P, -45.f, 0.01f));
	// The same target from a body turned to face +Y: now dead ahead.
	const FTransform Turned(FRotator(0.f, 90.f, 0.f), FVector::ZeroVector);
	UAstralLocomotionAnimInstance::LookAngles(Turned, Head, FVector(0.f, 500.f, 100.f), Y, P);
	TestTrue(TEXT("Relative to the body's facing"), FMath::IsNearlyZero(Y, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralSecondary_Blink, "AstralWilds.Animation.Blink", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralSecondary_Blink::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Open before the blink"), UAstralLocomotionAnimInstance::BlinkCurve(-0.01f), 0.f);
	TestTrue(TEXT("Fully closed partway through"), FMath::IsNearlyEqual(UAstralLocomotionAnimInstance::BlinkCurve(0.064f), 1.f, 0.01f));
	TestEqual(TEXT("Open again after it"), UAstralLocomotionAnimInstance::BlinkCurve(0.2f), 0.f);
	TestTrue(TEXT("Closes faster than it opens"), UAstralLocomotionAnimInstance::BlinkCurve(0.032f) > 1.f - UAstralLocomotionAnimInstance::BlinkCurve(0.112f) - 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralSecondary_SettlePhase, "AstralWilds.Animation.SettlePhase", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralSecondary_SettlePhase::RunTest(const FString& Parameters)
{
	// A stop settles when a diagonal pair is planted mid-stance: 0.25 or 0.75 of the cycle.
	TestTrue(TEXT("Stepping over 0.25 settles"), UAstralLocomotionAnimInstance::ReachesSettlePhase(0.24f, 0.02f));
	TestTrue(TEXT("Stepping over 0.75 settles"), UAstralLocomotionAnimInstance::ReachesSettlePhase(0.7f, 0.06f));
	TestFalse(TEXT("Mid-way between, it carries on"), UAstralLocomotionAnimInstance::ReachesSettlePhase(0.4f, 0.05f));
	TestFalse(TEXT("Just past a settle point, it carries on to the next"), UAstralLocomotionAnimInstance::ReachesSettlePhase(0.26f, 0.1f));
	TestTrue(TEXT("Across the wrap from 0.95 by 0.35"), UAstralLocomotionAnimInstance::ReachesSettlePhase(0.95f, 0.35f));
	TestFalse(TEXT("No step, no settle (unless on the point)"), UAstralLocomotionAnimInstance::ReachesSettlePhase(0.5f, 0.f));
	return true;
}

#endif
