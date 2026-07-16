// Shared laser beam draw + procedural zap audio

#include "RDLaserFX.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"

void RDLaserFX::DrawBeam(UWorld* World, const FVector& Start, const FVector& End, const FColor& OuterColor,
	float Thickness, float LifeSeconds)
{
	if (!World) return;

	const FVector Dir = (End - Start).GetSafeNormal();
	FVector Right = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		Right = FVector::CrossProduct(Dir, FVector::RightVector).GetSafeNormal();
	}
	const FVector Up = FVector::CrossProduct(Right, Dir).GetSafeNormal();
	const float Glow = FMath::Max(3.f, Thickness * 0.35f);
	const float Core = FMath::Max(2.f, Thickness * 0.45f);

	// Outer glow (team color) — thick and long-lived for spectator cam
	DrawDebugLine(World, Start, End, OuterColor, false, LifeSeconds, SDPG_World, Thickness);
	// Soft side glow
	DrawDebugLine(World, Start + Right * Glow, End + Right * Glow, OuterColor, false, LifeSeconds * 0.85f, 0, Thickness * 0.55f);
	DrawDebugLine(World, Start - Right * Glow, End - Right * Glow, OuterColor, false, LifeSeconds * 0.85f, 0, Thickness * 0.55f);
	DrawDebugLine(World, Start + Up * Glow * 0.6f, End + Up * Glow * 0.6f, OuterColor, false, LifeSeconds * 0.7f, 0, Thickness * 0.4f);
	// Hot white core
	DrawDebugLine(World, Start, End, FColor(255, 255, 255), false, LifeSeconds * 0.55f, 0, Core);
	// Muzzle flash + impact bloom
	DrawDebugPoint(World, Start, Thickness * 1.2f, OuterColor, false, LifeSeconds * 0.5f);
	DrawDebugPoint(World, End, Thickness * 1.6f, FColor::White, false, LifeSeconds * 0.45f);
	DrawDebugSphere(World, End, 22.f, 8, OuterColor, false, LifeSeconds * 0.35f, 0, 2.5f);
}

void RDLaserFX::PlayZap(UWorld* World, const FVector& Location, bool bAllyTeam)
{
	if (!World) return;

	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>();
	if (!Wave) return;

	const int32 SampleRate = 22050;
	const float Duration = 0.11f;
	const int32 NumSamples = FMath::Max(64, int32(SampleRate * Duration));

	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = 1;
	Wave->bLooping = false;
	Wave->SoundGroup = SOUNDGROUP_Effects;
	Wave->Duration = Duration;
	Wave->bProcedural = true;
	Wave->bCanProcessAsync = false;
	Wave->Volume = 1.f;

	TArray<uint8> PCM;
	PCM.SetNumUninitialized(NumSamples * sizeof(int16));

	float Freq = bAllyTeam ? 1650.f : 980.f;
	const float FreqEnd = bAllyTeam ? 720.f : 420.f;
	int16* Samples = reinterpret_cast<int16*>(PCM.GetData());
	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float T = float(i) / float(SampleRate);
		const float Alpha = float(i) / float(NumSamples - 1);
		const float F = FMath::Lerp(Freq, FreqEnd, Alpha);
		// Fast attack, exponential decay — "zap"
		const float Env = FMath::Clamp(T / 0.004f, 0.f, 1.f) * FMath::Exp(-T * 32.f);
		float S = FMath::Sin(2.f * PI * F * T);
		S += 0.35f * FMath::Sin(2.f * PI * F * 2.3f * T);
		// Light noise grit
		S += 0.12f * (FMath::FRand() * 2.f - 1.f);
		S *= Env * 0.85f;
		Samples[i] = int16(FMath::Clamp(S * 30000.f, -32767.f, 32767.f));
	}

	Wave->QueueAudio(PCM.GetData(), PCM.Num());

	const float Vol = bAllyTeam ? 0.9f : 0.85f;
	const float Pitch = bAllyTeam ? 1.12f : 0.92f;
	UGameplayStatics::PlaySoundAtLocation(World, Wave, Location, Vol, Pitch);
}
