// Astral Wilds - automation tests for the flying species' pure rules in
// AAstralWildlifeController (flight phase choice, glide slope, soaring
// circle). The flying itself is judged with Astral.MotionCapture Stormrook.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralWildlifeController.h"
#include "AstralSpeciesData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralFlight_PhaseRules, "AstralWilds.Flight.PhaseRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralFlight_PhaseRules::RunTest(const FString& Parameters)
{
	using EPhase = EAstralFlightPhase;
	using EMode = EAstralWildlifeMode;
	auto Choose = [](EPhase Current, EMode Mode, float PhaseTime, float RestTime = 10.f, float Height = 0.f, bool bTouched = false)
	{
		return AAstralWildlifeController::ChooseFlightPhase(Current, Mode, PhaseTime, RestTime, Height, 650.f, bTouched);
	};

	// Grounded: rests its ground time, but a chase, flee or trip home gets it up at once.
	TestEqual(TEXT("Resting flyer stays down until its ground time is up"), Choose(EPhase::Grounded, EMode::Wander, 5.f), EPhase::Grounded);
	TestEqual(TEXT("Rested flyer takes off"), Choose(EPhase::Grounded, EMode::Wander, 10.f), EPhase::TakingOff);
	TestEqual(TEXT("A chase takes off at once"), Choose(EPhase::Grounded, EMode::Chase, 0.f), EPhase::TakingOff);
	TestEqual(TEXT("A flee takes off at once"), Choose(EPhase::Grounded, EMode::Flee, 0.f), EPhase::TakingOff);
	TestEqual(TEXT("A Receptive flyer stays down (so it can be bonded)"), Choose(EPhase::Grounded, EMode::Idle, 100.f), EPhase::Grounded);

	// Taking off: airborne once high enough (or after 3s).
	TestEqual(TEXT("Still climbing"), Choose(EPhase::TakingOff, EMode::Wander, 1.f, 10.f, 200.f), EPhase::TakingOff);
	TestEqual(TEXT("High enough: airborne"), Choose(EPhase::TakingOff, EMode::Wander, 1.f, 10.f, 400.f), EPhase::Airborne);
	TestEqual(TEXT("Climb timeout: airborne"), Choose(EPhase::TakingOff, EMode::Wander, 3.5f, 10.f, 100.f), EPhase::Airborne);

	// Airborne: soars its air time while wandering, then lands; chasing keeps it up.
	TestEqual(TEXT("Soaring"), Choose(EPhase::Airborne, EMode::Wander, 5.f, 20.f, 650.f), EPhase::Airborne);
	TestEqual(TEXT("Air time up: lands"), Choose(EPhase::Airborne, EMode::Wander, 21.f, 20.f, 650.f), EPhase::Landing);
	TestEqual(TEXT("A chase keeps it up past its air time"), Choose(EPhase::Airborne, EMode::Chase, 60.f, 20.f, 650.f), EPhase::Airborne);
	TestEqual(TEXT("Receptive: lands"), Choose(EPhase::Airborne, EMode::Idle, 1.f, 20.f, 650.f), EPhase::Landing);

	// Landing: grounded on touchdown; disturbed on the way down, back up.
	TestEqual(TEXT("Gliding in"), Choose(EPhase::Landing, EMode::Wander, 2.f, 10.f, 300.f), EPhase::Landing);
	TestEqual(TEXT("Touchdown"), Choose(EPhase::Landing, EMode::Wander, 2.f, 10.f, 0.f, true), EPhase::Grounded);
	TestEqual(TEXT("Chased while landing: back up"), Choose(EPhase::Landing, EMode::Chase, 2.f, 10.f, 300.f), EPhase::Airborne);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralFlight_Geometry, "AstralWilds.Flight.GlideAndOrbit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralFlight_Geometry::RunTest(const FString& Parameters)
{
	// Glide slope: ~24 deg, capped at cruise height, zero at the spot.
	TestEqual(TEXT("On the landing spot the glide height is zero"), AAstralWildlifeController::GlideSlopeHeight(0.f, 650.f), 0.f);
	TestTrue(TEXT("10m out the glide height is 4.5m"), FMath::IsNearlyEqual(AAstralWildlifeController::GlideSlopeHeight(1000.f, 650.f), 450.f));
	TestEqual(TEXT("Far out it is capped at cruise height"), AAstralWildlifeController::GlideSlopeHeight(5000.f, 650.f), 650.f);

	// Orbit: the target sits on the circle, ahead of the bird in its direction of travel.
	const FVector Home(0.f, 0.f, 0.f);
	const FVector Here(900.f, 0.f, 600.f);
	const FVector Ccw = AAstralWildlifeController::OrbitTarget(Here, Home, 900.f, 1.f);
	const FVector Cw = AAstralWildlifeController::OrbitTarget(Here, Home, 900.f, -1.f);
	TestTrue(TEXT("The orbit target is on the circle"), FMath::IsNearlyEqual(FVector::Dist2D(Ccw, Home), 900.f, 1.f));
	TestTrue(TEXT("Anticlockwise leads to +Y from +X"), Ccw.Y > 0.f);
	TestTrue(TEXT("Clockwise leads to -Y from +X"), Cw.Y < 0.f);
	TestTrue(TEXT("The orbit target keeps the bird's height (altitude is steered separately)"), FMath::IsNearlyEqual(Ccw.Z, 600.0));

	// Species defaults: nobody flies unless asked to.
	TestFalse(TEXT("Species don't fly by default"), NewObject<UAstralSpeciesData>()->bCanFly);
	return true;
}

#endif
