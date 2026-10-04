// Astral Wilds - placeholder debug HUD so the Resonance Weave and bonding loop
// are observable in Play before any real UI exists. Draws, for the possessed
// AAstralMageCharacter: a status line (party size, interact prompt for a
// receptive wild Astral in reach) and, during a weave, the Sigil - Resonance
// Point with its alignment tolerance ring, the player's reticle, Stability,
// channel state and the Harmonize window - plus the last weave result.
// Canvas-only, no assets; replace with a UMG Sigil widget once one exists.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AstralTypes.h"
#include "AstralDebugHUD.generated.h"

class UAstralResonanceWeaveComponent;

UCLASS()
class AAstralDebugHUD : public AHUD
{
	GENERATED_BODY()

public:

	virtual void DrawHUD() override;

	/** How long the last weave result stays on screen, in seconds. */
	UPROPERTY(EditAnywhere, Category = "Astral|Debug")
	float ResultDisplaySeconds = 3.f;

	/** Sigil radius as a fraction of the viewport height. */
	UPROPERTY(EditAnywhere, Category = "Astral|Debug")
	float SigilRadiusFraction = 0.16f;

protected:

	UFUNCTION()
	void HandleWeaveResult(EAstralWeaveResult Result);

private:

	void DrawCircle(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness = 1.5f, int32 Segments = 48);
	void DrawSigil(const UAstralResonanceWeaveComponent& Weave, const FString& TargetName);
	void DrawStatusLine(const FString& Line, int32 Row, const FLinearColor& Color = FLinearColor::White);

	TWeakObjectPtr<UAstralResonanceWeaveComponent> BoundWeave;
	FString LastResultText;
	float LastResultTime = -1000.f;
};
