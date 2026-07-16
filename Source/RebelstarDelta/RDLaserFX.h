// Shared laser beam draw + procedural zap audio

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace RDLaserFX
{
	/** Fat, bright debug laser (outer glow + white core + impact flash). */
	void DrawBeam(UWorld* World, const FVector& Start, const FVector& End, const FColor& OuterColor,
		float Thickness = 16.f, float LifeSeconds = 0.28f);

	/** Short synthesized laser zap (no external asset required). */
	void PlayZap(UWorld* World, const FVector& Location, bool bAllyTeam);
}
