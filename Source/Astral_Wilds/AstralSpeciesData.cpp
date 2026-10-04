// Astral Wilds - see AstralSpeciesData.h.
#include "AstralSpeciesData.h"
#include "Curves/CurveFloat.h"

FAstralWeaveTemperament UAstralSpeciesData::GetWeaveTemperament() const
{
	FAstralWeaveTemperament Temperament;
	Temperament.Volatility = WeaveVolatility;
	Temperament.PulseInterval = WeavePulseInterval;
	Temperament.ResistanceStrength = WeaveResistanceStrength;
	Temperament.bMayFleeOnFailure = bMayFleeOnWeaveFailure;

	// TUNABLE placeholder: CaptureRate 255 (easiest) -> MinRequiredStability,
	// CaptureRate 0 (hardest) -> MaxRequiredStability. Linear for now.
	constexpr float MinRequiredStability = 40.f;
	constexpr float MaxRequiredStability = 200.f;
	const float DifficultyFraction = 1.f - FMath::Clamp(CaptureRate, 0, 255) / 255.f;
	Temperament.RequiredStability = FMath::Lerp(MinRequiredStability, MaxRequiredStability, DifficultyFraction);

	return Temperament;
}

FAstralBaseStats UAstralSpeciesData::ComputeStatsForLevel(int32 Level) const
{
	// TUNABLE placeholder growth formula: +GrowthRatePerLevel per level above 1, no IV/EV/nature system yet.
	const float Scalar = 1.f + FMath::Max(0, Level - 1) * GrowthRatePerLevel;

	FAstralBaseStats Result;
	Result.HP = FMath::RoundToInt(BaseStats.HP * Scalar);
	Result.Attack = FMath::RoundToInt(BaseStats.Attack * Scalar);
	Result.Defense = FMath::RoundToInt(BaseStats.Defense * Scalar);
	Result.Speed = FMath::RoundToInt(BaseStats.Speed * Scalar);
	return Result;
}

int32 UAstralSpeciesData::ComputeXPRequiredForLevel(int32 Level) const
{
	if (!LevelToXPCurve.IsNull())
	{
		if (UCurveFloat* Curve = LevelToXPCurve.LoadSynchronous())
		{
			return FMath::Max(0, FMath::RoundToInt(Curve->GetFloatValue(static_cast<float>(Level))));
		}
	}

	// TUNABLE placeholder formula (cubic, roughly "medium fast" growth) used when no curve asset is authored.
	return Level * Level * Level;
}
