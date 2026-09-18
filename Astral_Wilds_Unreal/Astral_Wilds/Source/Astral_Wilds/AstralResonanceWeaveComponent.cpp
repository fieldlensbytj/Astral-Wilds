#include "AstralResonanceWeaveComponent.h"

UAstralResonanceWeaveComponent::UAstralResonanceWeaveComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAstralResonanceWeaveComponent::BeginWeave(const FAstralWeaveTemperament& Temperament, bool bInUseOldConcordance)
{
	if (bIsActive)
	{
		OnWeaveResult.Broadcast(EAstralWeaveResult::MayRetry);
		return;
	}

	CurrentTemperament = Temperament;
	bUseOldConcordance = bInUseOldConcordance;
	bIsActive = true;
	bIsChanneling = false;
	Stability = 0.f;
	ResonancePoint = FVector2D::ZeroVector;
	PlayerAlignment = FVector2D::ZeroVector;
	TimeUntilNextPulse = FMath::Max(0.4f, CurrentTemperament.PulseInterval);
	bAwaitingPulseResponse = false;
	PulseResponseTimeRemaining = 0.f;
	MovementPhase = 0.f;
	MovementRandomStream.Initialize(FMath::Rand());

	SetComponentTickEnabled(true);
	OnStabilityChanged.Broadcast(0.f);
}

void UAstralResonanceWeaveComponent::CancelWeave()
{
	if (!bIsActive)
	{
		return;
	}
	bIsActive = false;
	SetComponentTickEnabled(false);
}

void UAstralResonanceWeaveComponent::SetAlignmentInput(FVector2D Delta)
{
	if (!bIsActive)
	{
		return;
	}
	// Delta nudges the reticle around the Sigil; clamped to the unit circle so
	// the player can always reach any point but can't fly off the interface.
	PlayerAlignment += Delta;
	if (PlayerAlignment.SizeSquared() > 1.f)
	{
		PlayerAlignment.Normalize();
	}
}

void UAstralResonanceWeaveComponent::SetChanneling(bool bNewChanneling)
{
	bIsChanneling = bNewChanneling;
}

void UAstralResonanceWeaveComponent::RespondToHarmonize()
{
	if (!bIsActive || !bAwaitingPulseResponse)
	{
		// Pressing Harmonize with no pulse active is simply ignored - there is
		// no punishment for an eager or mistimed press outside a pulse window.
		return;
	}

	bAwaitingPulseResponse = false;

	const float AlignmentQuality = ComputeAlignmentQuality();
	const bool bWellAligned = AlignmentQuality > 0.5f;
	const bool bWithinWindow = PulseResponseTimeRemaining > 0.f;

	if (bWithinWindow && (bWellAligned || bUseOldConcordance))
	{
		// Old Concordance: matching the rhythm forgives imperfect alignment,
		// because the point is to listen, not to overpower.
		Stability += 15.f + (10.f * AlignmentQuality);
	}
	else
	{
		Stability -= 10.f * (1.f + CurrentTemperament.ResistanceStrength);
	}

	Stability = FMath::Clamp(Stability, 0.f, CurrentTemperament.RequiredStability);
	OnStabilityChanged.Broadcast(GetStabilityFraction());

	if (Stability <= 0.f)
	{
		ResolveWeave(CurrentTemperament.bMayFleeOnFailure ? EAstralWeaveResult::Fled : EAstralWeaveResult::MayRetry);
	}
}

void UAstralResonanceWeaveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsActive)
	{
		return;
	}

	UpdateResonancePointMovement(DeltaTime);
	UpdatePulseTimer(DeltaTime);

	if (bIsChanneling)
	{
		const float AlignmentQuality = ComputeAlignmentQuality();
		if (AlignmentQuality > 0.f)
		{
			Stability += AlignmentQuality * 18.f * DeltaTime;
		}
		else
		{
			Stability -= 6.f * (1.f + CurrentTemperament.ResistanceStrength) * DeltaTime;
		}
	}
	else
	{
		// Releasing Hold lets the bond settle rather than actively decaying it -
		// there's no penalty for taking a breath, only for losing alignment
		// while channeling or missing a pulse outright.
	}

	Stability = FMath::Clamp(Stability, 0.f, CurrentTemperament.RequiredStability);
	OnStabilityChanged.Broadcast(GetStabilityFraction());

	if (Stability >= CurrentTemperament.RequiredStability)
	{
		ResolveWeave(EAstralWeaveResult::Succeeded);
	}
}

void UAstralResonanceWeaveComponent::UpdateResonancePointMovement(float DeltaTime)
{
	// A simple two-axis drift, phase-shifted per axis, scaled by Volatility -
	// timid/slow species (low Volatility) barely move; erratic ones (high
	// Volatility) dart around the Sigil.
	MovementPhase += DeltaTime * (0.6f + CurrentTemperament.Volatility * 1.8f);

	const float Jitter = MovementRandomStream.FRandRange(-1.f, 1.f) * CurrentTemperament.Volatility * 0.15f;
	ResonancePoint.X = FMath::Clamp(FMath::Sin(MovementPhase) * (0.4f + CurrentTemperament.Volatility * 0.5f) + Jitter, -1.f, 1.f);
	ResonancePoint.Y = FMath::Clamp(FMath::Cos(MovementPhase * 0.77f) * (0.4f + CurrentTemperament.Volatility * 0.5f), -1.f, 1.f);
}

void UAstralResonanceWeaveComponent::UpdatePulseTimer(float DeltaTime)
{
	if (bAwaitingPulseResponse)
	{
		PulseResponseTimeRemaining -= DeltaTime;
		if (PulseResponseTimeRemaining <= 0.f)
		{
			// Missed the window entirely.
			bAwaitingPulseResponse = false;
			Stability -= 12.f * (1.f + CurrentTemperament.ResistanceStrength);
			Stability = FMath::Clamp(Stability, 0.f, CurrentTemperament.RequiredStability);
			OnStabilityChanged.Broadcast(GetStabilityFraction());

			if (Stability <= 0.f)
			{
				ResolveWeave(CurrentTemperament.bMayFleeOnFailure ? EAstralWeaveResult::Fled : EAstralWeaveResult::MayRetry);
			}
		}
		return;
	}

	TimeUntilNextPulse -= DeltaTime;
	if (TimeUntilNextPulse <= 0.f)
	{
		bAwaitingPulseResponse = true;
		PulseResponseTimeRemaining = PulseResponseWindow;
		TimeUntilNextPulse = FMath::Max(0.4f, CurrentTemperament.PulseInterval);
		OnPulse.Broadcast(PulseResponseWindow);
	}
}

float UAstralResonanceWeaveComponent::ComputeAlignmentQuality() const
{
	const float Distance = FVector2D::Distance(PlayerAlignment, ResonancePoint);
	if (Distance >= AlignmentToleranceRadius)
	{
		return 0.f;
	}
	return 1.f - (Distance / AlignmentToleranceRadius);
}

void UAstralResonanceWeaveComponent::ResolveWeave(EAstralWeaveResult Result)
{
	bIsActive = false;
	SetComponentTickEnabled(false);
	OnWeaveResult.Broadcast(Result);
}
