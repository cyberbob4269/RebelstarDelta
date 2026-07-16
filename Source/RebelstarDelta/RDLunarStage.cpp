// Rebelstar Delta — Phase 0 lunar vista (strip OpenWorld, one sun, readable sky)

#include "RDLunarStage.h"
#include "Camera/CameraActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	bool IsTemplateJunk(const AActor* A)
	{
		if (!A)
		{
			return false;
		}
		if (A->Tags.Contains(FName(TEXT("RD_Lunar"))) || A->Tags.Contains(FName(TEXT("RD_Keep"))))
		{
			return false;
		}

		const FString ClassName = A->GetClass()->GetName();
		const FString ActorName = A->GetName();

		// Landscape / streaming from OpenWorld template
		if (ClassName.Contains(TEXT("Landscape")) || ActorName.Contains(TEXT("Landscape")))
		{
			return true;
		}
		// Template suns / skies / fog (we spawn our own)
		if (A->IsA(ADirectionalLight::StaticClass())) return true;
		if (A->IsA(ASkyLight::StaticClass())) return true;
		if (ClassName.Contains(TEXT("SkyAtmosphere"))) return true;
		if (ClassName.Contains(TEXT("ExponentialHeightFog"))) return true;
		if (ClassName.Contains(TEXT("VolumetricCloud"))) return true;
		if (ClassName.Contains(TEXT("SkySphere")) || ActorName.Contains(TEXT("SkySphere"))) return true;
		if (ClassName.Contains(TEXT("AtmosphericFog"))) return true;
		if (ClassName.Contains(TEXT("PostProcessVolume"))) return true;
		if (ClassName.Contains(TEXT("ReflectionCapture"))) return true;
		if (ClassName.Contains(TEXT("ExponentialHeightFog"))) return true;
		// OpenWorld often has a default floor plane named Floor or similar
		if (ActorName.Equals(TEXT("Floor")) || ActorName.Contains(TEXT("DefaultPhysicsVolume")) == false)
		{
			// don't nuke physics volume
		}
		if (ActorName.Contains(TEXT("GeoViewer")) || ActorName.Contains(TEXT("WorldPartition")))
		{
			return false; // leave systems alone
		}
		return false;
	}
}

ARDLunarStage::ARDLunarStage()
{
	PrimaryActorTick.bCanEverTick = true; // Earth spin + gentle sun cycle

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lit(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Unlit(TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"));

	if (Cube.Succeeded()) CubeMesh = Cube.Object;
	if (Sphere.Succeeded()) SphereMesh = Sphere.Object;
	if (Plane.Succeeded()) PlaneMesh = Plane.Object;
	if (Lit.Succeeded()) LitMat = Lit.Object;
	if (Unlit.Succeeded()) UnlitMat = Unlit.Object;
	if (!UnlitMat) UnlitMat = LitMat;
}

void ARDLunarStage::TryLoadPackMaterials()
{
	// Free NASA / SolarSystemScope maps via Content/Python/init_unreal.py
	MatMoonGround = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RD/Materials/M_RD_MoonGround.M_RD_MoonGround"));
	MatEarth = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RD/Materials/M_RD_Earth.M_RD_Earth"));
	MatEarthDay = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RD/Materials/M_RD_EarthDay.M_RD_EarthDay"));
	if (!MatEarthDay) MatEarthDay = MatEarth;
	MatEarthNight = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RD/Materials/M_RD_EarthNight.M_RD_EarthNight"));
	MatEarthClouds = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RD/Materials/M_RD_EarthClouds.M_RD_EarthClouds"));
	if (!MatEarthClouds)
	{
		MatEarthClouds = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RD/Materials/M_RD_EarthCloudsSolid.M_RD_EarthCloudsSolid"));
	}
	MatMilkyWay = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RD/Materials/M_RD_MilkyWay.M_RD_MilkyWay"));
	MatMoonGlobe = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RD/Materials/M_RD_MoonGlobe.M_RD_MoonGlobe"));
	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Materials moon=%d earthDay=%d earthNight=%d clouds=%d mw=%d"),
		MatMoonGround != nullptr, MatEarthDay != nullptr, MatEarthNight != nullptr,
		MatEarthClouds != nullptr, MatMilkyWay != nullptr);
}

void ARDLunarStage::BeginPlay()
{
	Super::BeginPlay();
	BuildStage();
}

void ARDLunarStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateEarthVisuals(DeltaSeconds);
}

void ARDLunarStage::PurgeTemplateEnvironment(UWorld* World)
{
	if (!World)
	{
		return;
	}

	TArray<AActor*> ToKill;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!IsValid(A) || A->IsA(APlayerController::StaticClass()) || A->IsA(APawn::StaticClass()))
		{
			continue;
		}
		// Never kill game framework
		const FString ClassName = A->GetClass()->GetName();
		if (ClassName.Contains(TEXT("GameMode")) || ClassName.Contains(TEXT("GameState")) ||
			ClassName.Contains(TEXT("PlayerState")) || ClassName.Contains(TEXT("HUD")) ||
			ClassName.Contains(TEXT("WorldSettings")) || ClassName.Contains(TEXT("GameNetwork")) ||
			ClassName.Contains(TEXT("ParticleEvent")) || ClassName.Contains(TEXT("GameplayDebugger")) ||
			ClassName.Contains(TEXT("AbstractNavData")) || ClassName.Contains(TEXT("AISystem")) ||
			ClassName.Contains(TEXT("RDLunarStage")) || ClassName.Contains(TEXT("RDCharacter")) ||
			ClassName.Contains(TEXT("RDGameMode")))
		{
			continue;
		}
		if (IsTemplateJunk(A))
		{
			ToKill.Add(A);
		}
	}

	for (AActor* A : ToKill)
	{
		A->Destroy();
	}

	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Purged %d template environment actors."), ToKill.Num());
}

void ARDLunarStage::ApplyLook(UStaticMeshComponent* SMC, const FLinearColor& Color, bool bUnlit)
{
	if (!SMC)
	{
		return;
	}
	UMaterialInterface* Parent = bUnlit && UnlitMat ? UnlitMat.Get() : LitMat.Get();
	if (!Parent)
	{
		return;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
	if (!MID)
	{
		return;
	}
	// Common parameter names across engine mats
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
	MID->SetVectorParameterValue(TEXT("Tint"), Color);
	MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
	SMC->SetMaterial(0, MID);
}

AStaticMeshActor* ARDLunarStage::SpawnMesh(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
	const FRotator& Rotation, const FLinearColor& Color, bool bUnlit, bool bCollision,
	UMaterialInterface* OverrideMat)
{
	if (!GetWorld() || !Mesh)
	{
		return nullptr;
	}

	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, Rotation);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->SetMobility(EComponentMobility::Movable);
	UStaticMeshComponent* SMC = Actor->GetStaticMeshComponent();
	SMC->SetMobility(EComponentMobility::Movable);
	SMC->SetStaticMesh(Mesh);
	SMC->SetWorldScale3D(Scale);
	if (bCollision)
	{
		SMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SMC->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SMC->SetCastShadow(bCollision && !bUnlit);
	if (OverrideMat)
	{
		SMC->SetMaterial(0, OverrideMat);
	}
	else
	{
		ApplyLook(SMC, Color, bUnlit);
	}
	Actor->Tags.Add(FName(TEXT("RD_Lunar")));
	return Actor;
}

void ARDLunarStage::SpawnSun()
{
	if (!GetWorld()) return;

	// LOW sun → long dramatic shadows across craters and the approach plain.
	// Only one DirectionalLight (second sun causes ForwardShadingPriority spam).
	// MUST be Movable if we ever SetActorRotation — Static+rotate spams PIE Message Log
	// "Mobility ... has to be 'Movable' if you'd like to move" every frame.
	ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>(
		FVector::ZeroVector, FRotator(SunPitch, SunYaw, 0.f));
	if (!Sun) return;
	SunLight = Sun;

	if (USceneComponent* Root = Sun->GetRootComponent())
	{
		Root->SetMobility(EComponentMobility::Movable);
	}
	if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
	{
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensity(SunIntensity);
		// Warm golden low-sun color
		L->SetLightColor(FLinearColor(1.f, 0.88f, 0.72f));
		// AtmosphereSunLight can fight missing SkyAtmosphere — keep false for clean lunar void
		L->SetAtmosphereSunLight(false);
		L->SetCastShadows(true);
		L->SetShadowBias(0.25f);
		L->SetShadowSlopeBias(0.5f);
		// Long reach so far craters + base cast together
		L->SetDynamicShadowDistanceMovableLight(150000.f);
		L->SetDynamicShadowCascades(4);
		L->SetCascadeDistributionExponent(2.5f);
	}
	Sun->Tags.Add(FName(TEXT("RD_Lunar")));
	Sun->Tags.Add(FName(TEXT("RD_Sun")));
}

void ARDLunarStage::SpawnGround()
{
	// Collision slab underneath
	SpawnMesh(CubeMesh, FVector(0.f, 0.f, -220.f), FVector(1600.f, 1600.f, 4.5f),
		FRotator::ZeroRotator, FLinearColor(0.45f, 0.42f, 0.36f), false, true);

	// Textured surface plane (NASA/SSS moon map if materials imported)
	if (PlaneMesh)
	{
		// Plane is 100x100 cm; scale huge, slightly above collision top
		SpawnMesh(PlaneMesh, FVector(0.f, 0.f, 5.f), FVector(1600.f, 1600.f, 1.f),
			FRotator::ZeroRotator, FLinearColor(0.5f, 0.46f, 0.4f), false, false,
			MatMoonGround.Get());
		// Secondary rotated patch for variety near base
		SpawnMesh(PlaneMesh, FVector(8000.f, -6000.f, 8.f), FVector(400.f, 400.f, 1.f),
			FRotator(0.f, 35.f, 0.f), FLinearColor(0.48f, 0.44f, 0.38f), false, false,
			MatMoonGround.Get());
	}

	FMath::RandInit(11);
	// Rocks: if moon material exists, tint darker; still readable silhouette
	for (int32 i = 0; i < 100; ++i)
	{
		const float X = FMath::FRandRange(-55000.f, 55000.f);
		const float Y = FMath::FRandRange(-55000.f, 55000.f);
		const float S = FMath::FRandRange(2.5f, 22.f);
		const float Grey = FMath::FRandRange(0.28f, 0.52f);
		SpawnMesh(SphereMesh, FVector(X, Y, 25.f), FVector(S, S * 0.85f, S * 0.4f),
			FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f),
			FLinearColor(Grey, Grey * 0.94f, Grey * 0.82f), false, true,
			MatMoonGround.Get());
	}
	for (int32 i = 0; i < 16; ++i)
	{
		const float X = FMath::FRandRange(-30000.f, 30000.f);
		const float Y = FMath::FRandRange(-30000.f, 30000.f);
		const float R = FMath::FRandRange(18.f, 42.f);
		SpawnMesh(SphereMesh, FVector(X, Y, -50.f), FVector(R, R, 3.5f),
			FRotator::ZeroRotator, FLinearColor(0.26f, 0.24f, 0.21f), false, false,
			MatMoonGround.Get());
		SpawnMesh(SphereMesh, FVector(X, Y, 40.f), FVector(R * 1.05f, R * 1.05f, 1.2f),
			FRotator::ZeroRotator, FLinearColor(0.55f, 0.5f, 0.42f), false, false,
			MatMoonGround.Get());
	}
}

void ARDLunarStage::SpawnCraterRim()
{
	const FLinearColor Rim(0.34f, 0.31f, 0.28f);
	const FLinearColor RimHi(0.55f, 0.5f, 0.42f);
	const FLinearColor RimLit(0.62f, 0.55f, 0.45f);
	const float Radius = 42000.f;
	for (int32 i = 0; i < 36; ++i)
	{
		const float T = float(i) / 35.f;
		const float AngleDeg = -75.f + T * 150.f;
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const FVector Loc(
			FMath::Cos(Rad) * Radius,
			FMath::Sin(Rad) * Radius,
			400.f + FMath::FRandRange(0.f, 1400.f));
		const FVector Scale(50.f + FMath::FRandRange(0.f, 40.f), 18.f + FMath::FRandRange(0.f, 12.f),
			9.f + FMath::FRandRange(0.f, 16.f));
		const FLinearColor C = (i % 3 == 0) ? RimLit : ((i % 2 == 0) ? Rim : RimHi);
		// Prefer sphere rims — cubes read as Minecraft mountains
		UStaticMesh* RimMesh = SphereMesh.Get() ? SphereMesh.Get() : CubeMesh.Get();
		SpawnMesh(RimMesh, Loc, Scale * FVector(1.f, 1.f, 0.55f),
			FRotator(FMath::FRandRange(-12.f, 12.f), AngleDeg, FMath::FRandRange(-10.f, 10.f)),
			C, false, true);
	}
}

void ARDLunarStage::SpawnEarth()
{
	// Earthrise on the eastern horizon — MUST NOT intersect the playable base.
	// UE sphere radius at scale 1 ≈ 50 cm; scale S ⇒ radius ≈ 50*S cm.
	// Bug found in QA: scale 200 at X=32000 put surface near X=12000 (inside the base),
	// so the player walked into a giant blue ocean. Keep surface well past base (~X=16k).
	// Target: center far east, surface starts ~X≥45000 from origin.
	const FVector EarthLoc(70000.f, -3000.f, 9000.f);
	const float DayScale = 110.f;   // radius ~55 m — readable disc, clear of base
	const float NightScale = 111.2f;
	const float CloudScale = 113.f;

	EarthDaySphere = nullptr;
	EarthNightSphere = nullptr;
	EarthCloudSphere = nullptr;

	// Prefer dedicated day mat; also try loading texture for log diagnostics
	UTexture2D* DayTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/RD/Textures/T_Earth_Day.T_Earth_Day"));
	UTexture2D* NightTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/RD/Textures/T_Earth_Night.T_Earth_Night"));
	UTexture2D* CloudTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/RD/Textures/T_Earth_Clouds.T_Earth_Clouds"));
	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Earth textures day=%d night=%d clouds=%d"),
		DayTex != nullptr, NightTex != nullptr, CloudTex != nullptr);

	UMaterialInterface* DayMat = MatEarthDay ? MatEarthDay.Get() : MatEarth.Get();
	if (DayMat)
	{
		EarthDaySphere = SpawnMesh(SphereMesh, EarthLoc, FVector(DayScale),
			FRotator(12.f, -25.f, 8.f), FLinearColor(0.15f, 0.35f, 0.7f), true, false, DayMat);
	}
	else
	{
		// Strong blue fallback only if materials missing
		EarthDaySphere = SpawnMesh(SphereMesh, EarthLoc, FVector(DayScale),
			FRotator(12.f, -25.f, 8.f), FLinearColor(0.1f, 0.32f, 0.72f), true, false);
	}

	// City lights shell — needs textured night mat (boost set in Python)
	if (MatEarthNight)
	{
		EarthNightSphere = SpawnMesh(SphereMesh, EarthLoc, FVector(NightScale),
			FRotator(12.f, -25.f, 8.f), FLinearColor(1.f, 0.85f, 0.4f), true, false, MatEarthNight.Get());
	}

	// Clouds — translucent material preferred; solid only if needed
	if (MatEarthClouds)
	{
		EarthCloudSphere = SpawnMesh(SphereMesh, EarthLoc, FVector(CloudScale),
			FRotator(18.f, 5.f, 0.f), FLinearColor(1.f, 1.f, 1.f), true, false, MatEarthClouds.Get());
	}

	// NO solid atmosphere wash sphere — that made Earth look pale grey-blue.

	// Soft blue earthshine fill (point only — not a second sun)
	if (GetWorld())
	{
		if (APointLight* PL = GetWorld()->SpawnActor<APointLight>(
			EarthLoc + FVector(-6000.f, 0.f, -1500.f), FRotator::ZeroRotator))
		{
			EarthShineLight = PL;
			if (UPointLightComponent* C = Cast<UPointLightComponent>(PL->GetLightComponent()))
			{
				C->SetIntensity(35000.f);
				C->SetAttenuationRadius(80000.f);
				C->SetLightColor(FLinearColor(0.4f, 0.55f, 1.f));
				C->SetCastShadows(false);
			}
			PL->Tags.Add(FName(TEXT("RD_Lunar")));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Earthrise dayMat=%d nightMat=%d cloudsMat=%d at %s scale=%.0f"),
		DayMat != nullptr, MatEarthNight != nullptr, MatEarthClouds != nullptr,
		*EarthLoc.ToCompactString(), DayScale);
}

void ARDLunarStage::UpdateEarthVisuals(float DeltaSeconds)
{
	// Spin Earth so day side and city lights roll across the disc (readable day/night cycle)
	if (EarthDaySeconds > 1.f)
	{
		EarthSpin += 360.f * (DeltaSeconds / EarthDaySeconds);
	}
	if (CloudDaySeconds > 1.f)
	{
		CloudSpin += 360.f * (DeltaSeconds / CloudDaySeconds);
	}

	const FRotator DayRot(15.f, EarthSpin - 40.f, 12.f);
	const FRotator CloudRot(20.f, CloudSpin + 10.f, 0.f);

	if (IsValid(EarthDaySphere))
	{
		EarthDaySphere->SetActorRotation(DayRot);
	}
	if (IsValid(EarthNightSphere))
	{
		// Night shell shares spin so cities stick to continents
		EarthNightSphere->SetActorRotation(DayRot);
		// Pulse city lights slightly so night side feels alive
		// (material is unlit — scale a hair)
		const float Pulse = 1.f + 0.03f * FMath::Sin(EarthSpin * 0.05f);
		EarthNightSphere->SetActorScale3D(FVector(131.2f * Pulse));
	}
	if (IsValid(EarthCloudSphere))
	{
		EarthCloudSphere->SetActorRotation(CloudRot);
	}

	// Keep sun FIXED after spawn for stable long shadows.
	// (We used to rotate it every tick; with Static mobility that spammed 200+ PIE warnings.)
	// Optional: very rare drift only if light is Movable — disabled by default for clean Message Log.
	(void)DeltaSeconds;
	(void)SunCycle;
}

void ARDLunarStage::SpawnStarfield()
{
	FMath::RandInit(42);
	int32 Placed = 0;
	int32 Guard = 0;
	while (Placed < 420 && Guard < 2000)
	{
		++Guard;
		FVector Dir = FMath::VRand();
		if (Dir.Z < -0.05f) continue;
		Dir = Dir.GetSafeNormal();
		const float Dist = FMath::FRandRange(28000.f, 85000.f);
		const FVector Loc = Dir * Dist;
		const float S = FMath::FRandRange(0.25f, 1.4f);
		const float B = FMath::FRandRange(0.85f, 1.0f);
		// Occasional blue/warm tint stars
		FLinearColor Col(B, B * 0.97f, B * 0.92f);
		if (Placed % 17 == 0) Col = FLinearColor(0.7f * B, 0.8f * B, B);
		if (Placed % 23 == 0) Col = FLinearColor(B, 0.85f * B, 0.65f * B);
		SpawnMesh(SphereMesh, Loc, FVector(S), FRotator::ZeroRotator, Col, true, false);
		++Placed;
	}
}

void ARDLunarStage::SpawnMilkyWayBand()
{
	// Textured MW dome strip if material available
	if (MatMilkyWay && SphereMesh)
	{
		SpawnMesh(SphereMesh, FVector(0.f, 0.f, 0.f), FVector(-900.f, 900.f, 900.f),
			FRotator(60.f, 20.f, 0.f), FLinearColor(0.5f, 0.55f, 1.f), true, false, MatMilkyWay.Get());
	}

	const int32 Count = 140;
	for (int32 i = 0; i < Count; ++i)
	{
		const float T = float(i) / float(Count - 1);
		const float Angle = FMath::Lerp(-90.f, 90.f, T);
		const float Rad = FMath::DegreesToRadians(Angle);
		const float JitterY = FMath::FRandRange(-0.18f, 0.18f);
		const float JitterZ = FMath::FRandRange(-0.08f, 0.08f);
		const FVector Dir = FVector(
			FMath::Cos(Rad) * 0.5f,
			FMath::Sin(Rad) + JitterY,
			0.5f + 0.12f * FMath::Sin(T * 12.f) + JitterZ).GetSafeNormal();
		const FVector Loc = Dir * (48000.f + FMath::FRandRange(-3000.f, 5000.f));
		const float S = FMath::FRandRange(1.8f, 9.f);
		const float V = FMath::FRandRange(0.28f, 0.85f);
		SpawnMesh(SphereMesh, Loc, FVector(S, S, S * 0.45f), FRotator::ZeroRotator,
			FLinearColor(0.55f * V, 0.62f * V, 1.0f * V), true, false);
	}
	// Dense core of the band
	for (int32 i = 0; i < 40; ++i)
	{
		const float T = float(i) / 39.f;
		const float Angle = FMath::Lerp(-25.f, 25.f, T);
		const float Rad = FMath::DegreesToRadians(Angle);
		const FVector Dir = FVector(FMath::Cos(Rad) * 0.35f, FMath::Sin(Rad), 0.72f).GetSafeNormal();
		SpawnMesh(SphereMesh, Dir * 46000.f, FVector(FMath::FRandRange(4.f, 12.f)),
			FRotator::ZeroRotator, FLinearColor(0.75f, 0.8f, 1.f), true, false);
	}
}

void ARDLunarStage::SpawnConstructionHint()
{
	// Yard is owned by RDMapBuilder (west of base). Keep a tiny pad marker only.
	const FLinearColor Brick(0.55f, 0.5f, 0.42f);
	SpawnMesh(CubeMesh, FVector(1200.f, -800.f, 40.f), FVector(2.f, 2.f, 0.3f),
		FRotator::ZeroRotator, Brick, false, true);
}

void ARDLunarStage::PlacePlayerOnPad()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetActorLocation(FVector(0.f, 0.f, 180.f));
			PC->SetControlRotation(FRotator(-8.f, 20.f, 0.f)); // look toward Earthrise-ish
		}
	}
}

void ARDLunarStage::BuildStage()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 1) Kill OpenWorld template environment
	PurgeTemplateEnvironment(World);
	TryLoadPackMaterials();

	// 2) Clear previous RD lunar actors
	TArray<AActor*> Existing;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->Tags.Contains(FName(TEXT("RD_Lunar"))) && *It != this)
		{
			Existing.Add(*It);
		}
	}
	for (AActor* A : Existing)
	{
		A->Destroy();
	}

	// 3) Build moon
	SpawnSun();
	SpawnGround();
	SpawnCraterRim();
	SpawnEarth();
	SpawnStarfield();
	SpawnMilkyWayBand();
	SpawnConstructionHint();
	PlacePlayerOnPad();

	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Lunar stage rebuilt: low sun, Earth day/night/clouds, long shadows."));
}
