#include "AstralDebugHUD.h"
#include "AstralMageCharacter.h"
#include "AstralCharacter.h"
#include "AstralResonanceWeaveComponent.h"
#include "AstralSpeciesData.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace
{
	FString DescribeAstral(const AAstralCharacter* Astral)
	{
		if (!Astral)
		{
			return TEXT("wild Astral");
		}
		const FString Species = Astral->SpeciesData ? Astral->SpeciesData->SpeciesName.ToString() : TEXT("Unknown Astral");
		return FString::Printf(TEXT("%s (Lv %d)"), *Species, Astral->Level);
	}

	FString DescribeResult(EAstralWeaveResult Result)
	{
		switch (Result)
		{
		case EAstralWeaveResult::Succeeded:		return TEXT("Bond formed - added to party");
		case EAstralWeaveResult::Fled:			return TEXT("The Astral fled");
		case EAstralWeaveResult::TurnedHostile:	return TEXT("The Astral turned hostile");
		case EAstralWeaveResult::MayRetry:		return TEXT("Weave faltered - you may try again");
		default:								return TEXT("Weave ended");
		}
	}
}

void AAstralDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	const AAstralMageCharacter* Mage = Cast<AAstralMageCharacter>(GetOwningPawn());
	if (!Mage || !Canvas)
	{
		return;
	}

	UAstralResonanceWeaveComponent* Weave = Mage->GetResonanceWeave();
	if (Weave && BoundWeave.Get() != Weave)
	{
		Weave->OnWeaveResult.AddUniqueDynamic(this, &AAstralDebugHUD::HandleWeaveResult);
		BoundWeave = Weave;
	}

	int32 Row = 0;
	DrawStatusLine(FString::Printf(TEXT("Party %d/%d"), Mage->GetParty().Num(), Mage->GetPartyCapacity()), Row++);

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastResultTime < ResultDisplaySeconds)
	{
		DrawStatusLine(LastResultText, Row++, FLinearColor(1.f, 0.85f, 0.3f));
	}

	if (Weave && Weave->IsWeaveActive())
	{
		DrawSigil(*Weave, DescribeAstral(Mage->GetCurrentWeaveTarget()));
	}
	else if (const AAstralCharacter* Receptive = Mage->GetInteractableWildAstral())
	{
		DrawStatusLine(FString::Printf(TEXT("[E / Y] Begin Resonance Weave with %s"), *DescribeAstral(Receptive)), Row++, FLinearColor(0.5f, 1.f, 0.7f));
	}
}

void AAstralDebugHUD::HandleWeaveResult(EAstralWeaveResult Result)
{
	LastResultText = DescribeResult(Result);
	LastResultTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void AAstralDebugHUD::DrawCircle(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness, int32 Segments)
{
	FVector2D Prev = Center + FVector2D(Radius, 0.f);
	for (int32 i = 1; i <= Segments; ++i)
	{
		const float Angle = 2.f * PI * i / Segments;
		const FVector2D Next = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		DrawLine(Prev.X, Prev.Y, Next.X, Next.Y, Color, Thickness);
		Prev = Next;
	}
}

void AAstralDebugHUD::DrawStatusLine(const FString& Line, int32 Row, const FLinearColor& Color)
{
	DrawText(Line, Color, 24.f, 24.f + Row * 22.f, GEngine->GetMediumFont());
}

void AAstralDebugHUD::DrawSigil(const UAstralResonanceWeaveComponent& Weave, const FString& TargetName)
{
	const float Radius = Canvas->ClipY * SigilRadiusFraction;
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	// Unit-circle space -> screen: +Y is up in the Sigil, down on screen.
	auto ToScreen = [&](const FVector2D& P) { return Center + FVector2D(P.X, -P.Y) * Radius; };

	const bool bPulse = Weave.IsAwaitingPulseResponse();
	const FLinearColor SigilColor = bPulse ? FLinearColor(1.f, 0.35f, 0.35f) : FLinearColor(0.6f, 0.7f, 1.f);

	DrawCircle(Center, Radius, SigilColor, bPulse ? 3.f : 1.5f, 64);
	if (bPulse)
	{
		// Shrinking inner ring = Harmonize window closing.
		DrawCircle(Center, Radius * Weave.GetPulseWindowFraction(), FLinearColor(1.f, 0.5f, 0.3f), 2.f, 48);
	}

	// Resonance Point and its alignment tolerance.
	const FVector2D Point = ToScreen(Weave.GetResonancePoint());
	DrawCircle(Point, Radius * UAstralResonanceWeaveComponent::GetAlignmentToleranceRadius(), FLinearColor(1.f, 0.9f, 0.4f, 0.6f), 1.f, 32);
	DrawRect(FLinearColor(1.f, 0.9f, 0.4f), Point.X - 5.f, Point.Y - 5.f, 10.f, 10.f);

	// Player reticle: a cross, brighter while channeling.
	const FVector2D Reticle = ToScreen(Weave.GetAlignmentReticle());
	const FLinearColor ReticleColor = Weave.IsChanneling() ? FLinearColor(0.4f, 1.f, 0.6f) : FLinearColor::White;
	DrawLine(Reticle.X - 12.f, Reticle.Y, Reticle.X + 12.f, Reticle.Y, ReticleColor, 2.f);
	DrawLine(Reticle.X, Reticle.Y - 12.f, Reticle.X, Reticle.Y + 12.f, ReticleColor, 2.f);

	// Stability bar under the Sigil.
	const float BarW = Radius * 2.f, BarH = 12.f;
	const float BarX = Center.X - Radius, BarY = Center.Y + Radius + 24.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), BarX, BarY, BarW, BarH);
	DrawRect(FLinearColor(0.45f, 0.8f, 1.f), BarX, BarY, BarW * Weave.GetStabilityFraction(), BarH);

	UFont* Font = GEngine->GetMediumFont();
	DrawText(FString::Printf(TEXT("Resonance Weave - %s"), *TargetName), FLinearColor::White, BarX, Center.Y - Radius - 48.f, Font);
	DrawText(FString::Printf(TEXT("Stability %d%%   %s"), FMath::RoundToInt(Weave.GetStabilityFraction() * 100.f), Weave.IsChanneling() ? TEXT("CHANNELING") : TEXT("hold LMB / RT to channel")),
		FLinearColor::White, BarX, BarY + BarH + 6.f, Font);
	if (bPulse)
	{
		DrawText(TEXT("HARMONIZE!  [Space / X]"), FLinearColor(1.f, 0.5f, 0.3f), BarX, Center.Y - Radius - 26.f, Font);
	}
}
