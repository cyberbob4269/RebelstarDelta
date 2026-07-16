// Rebelstar Delta — accurate-scale Starship + Super Heavy + Mechazilla
// Public SpaceX numbers (spacex.com / common Starbase refs), UE cm (1 uu = 1 cm):
//   Diameter 9 m | Super Heavy ~72 m | Ship ~52 m | Stack ~124 m
//   Mechazilla tower ~146 m | Chopsticks catch Super Heavy mid/upper body
//   OLM / pad plinth ~20 m visual base

#include "RDStarship.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float Meter = 100.f;
	constexpr float Diameter = 9.f * Meter;       // 900 cm
	constexpr float SuperHeavyH = 72.f * Meter;   // official ~72 m
	constexpr float ShipH = 52.f * Meter;         // official ~52 m
	constexpr float StackH = SuperHeavyH + ShipH; // ~124 m
	constexpr float TowerH = 146.f * Meter;       // Mechazilla
	constexpr float OLM_H = 18.f * Meter;         // orbital launch mount height visual
}

ARDStarship::ARDStarship()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lit(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Unlit(TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"));
	if (Cube.Succeeded()) CubeMesh = Cube.Object;
	if (Cyl.Succeeded()) CylinderMesh = Cyl.Object;
	if (Sph.Succeeded()) SphereMesh = Sph.Object;
	if (Lit.Succeeded()) LitMat = Lit.Object;
	if (Unlit.Succeeded()) UnlitMat = Unlit.Object;
	if (!UnlitMat) UnlitMat = LitMat;

	EngineGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("EngineGlow"));
	EngineGlow->SetupAttachment(Root);
	EngineGlow->SetIntensity(220000.f);
	EngineGlow->SetAttenuationRadius(28000.f);
	EngineGlow->SetLightColor(FLinearColor(1.f, 0.48f, 0.12f));
	EngineGlow->SetCastShadows(false);

	Tags.Add(FName(TEXT("RD_Starship")));
	Tags.Add(FName(TEXT("RD_Lunar")));
}

void ARDStarship::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(PadLocation);
	BuildShip();
}

FVector ARDStarship::GetLookAtPoint() const
{
	// Mid-stack + tower so ending camera reads ship + Mechazilla
	return GetActorLocation() + FVector(900.f, 0.f, StackH * 0.42f);
}

void ARDStarship::ApplyColor(UStaticMeshComponent* M, const FLinearColor& C, bool bUnlit)
{
	if (!M) return;
	UMaterialInterface* P = bUnlit ? UnlitMat.Get() : LitMat.Get();
	if (!P) return;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(P, this);
	if (!MID) return;
	MID->SetVectorParameterValue(TEXT("Color"), C);
	MID->SetVectorParameterValue(TEXT("BaseColor"), C);
	MID->SetVectorParameterValue(TEXT("EmissiveColor"), C);
	M->SetMaterial(0, MID);
}

void ARDStarship::BuildShip()
{
	auto Add = [&](UStaticMesh* Mesh, const FVector& Rel, const FVector& Scale, const FLinearColor& Col, bool bUnlit,
		const FRotator& Rot = FRotator::ZeroRotator) -> UStaticMeshComponent*
	{
		if (!Mesh) return nullptr;
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
		M->SetupAttachment(Root);
		M->RegisterComponent();
		M->SetStaticMesh(Mesh);
		M->SetRelativeLocation(Rel);
		M->SetRelativeRotation(Rot);
		M->SetRelativeScale3D(Scale);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(true);
		ApplyColor(M, Col, bUnlit);
		return M;
	};

	// Stainless / heat-tile / industrial palette (Starbase look)
	const FLinearColor Steel(0.82f, 0.84f, 0.88f);
	const FLinearColor SteelBand(0.68f, 0.70f, 0.74f);
	const FLinearColor SteelDark(0.38f, 0.40f, 0.44f);
	const FLinearColor Tile(0.70f, 0.72f, 0.76f);
	const FLinearColor Black(0.05f, 0.05f, 0.06f);
	const FLinearColor HeatTile(0.10f, 0.10f, 0.11f);
	const FLinearColor Flame(1.f, 0.42f, 0.06f);
	const FLinearColor FlameCore(1.f, 0.88f, 0.40f);
	const FLinearColor Concrete(0.32f, 0.31f, 0.29f);
	const FLinearColor ConcreteHi(0.42f, 0.40f, 0.37f);
	const FLinearColor Yellow(0.95f, 0.72f, 0.08f);
	const FLinearColor TowerGrey(0.28f, 0.30f, 0.33f);
	const FLinearColor TowerDark(0.14f, 0.15f, 0.17f);
	const FLinearColor Flood(1.f, 0.95f, 0.82f);

	const float Dxy = Diameter / 100.f; // cylinder scale for 9 m diameter

	// ========== OLM / PAD ==========
	// Wide concrete apron
	Add(CubeMesh, FVector(0.f, 0.f, 40.f), FVector(55.f, 55.f, 0.8f), Concrete, false);
	Add(CubeMesh, FVector(0.f, 0.f, 90.f), FVector(36.f, 36.f, 0.7f), ConcreteHi, false);
	// OLM table / hold-down structure
	Add(CubeMesh, FVector(0.f, 0.f, OLM_H * 0.45f), FVector(16.f, 16.f, OLM_H / 100.f * 0.85f), SteelDark, false);
	// Flame trench (east–west cut under stack)
	Add(CubeMesh, FVector(0.f, 1600.f, 50.f), FVector(22.f, 10.f, 1.0f), Black, false);
	Add(CubeMesh, FVector(0.f, -1600.f, 50.f), FVector(22.f, 10.f, 1.0f), Black, false);
	Add(CubeMesh, FVector(0.f, 0.f, 30.f), FVector(14.f, 28.f, 0.5f), Black, false);
	// Pad hazard ring
	Add(CubeMesh, FVector(0.f, 0.f, 130.f), FVector(32.f, 32.f, 0.12f), Yellow, false);

	// ========== SUPER HEAVY ==========
	const float BoosterCenterZ = OLM_H + SuperHeavyH * 0.5f;
	if (CylinderMesh)
	{
		Add(CylinderMesh, FVector(0.f, 0.f, BoosterCenterZ),
			FVector(Dxy, Dxy, SuperHeavyH / 100.f), Steel, false);

		// Barrel rings (stainless weld bands)
		for (int32 i = 1; i <= 7; ++i)
		{
			const float Z = OLM_H + SuperHeavyH * (float(i) / 8.f);
			Add(CubeMesh, FVector(0.f, 0.f, Z),
				FVector(Dxy * 1.02f, Dxy * 1.02f, 0.18f), SteelBand, false);
		}

		// Grid fins (4) near top of booster — black carbon look
		const float FinZ = OLM_H + SuperHeavyH * 0.84f;
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = i * 90.f + 45.f;
			const float Rad = FMath::DegreesToRadians(A);
			const float R = Diameter * 0.68f;
			Add(CubeMesh,
				FVector(FMath::Cos(Rad) * R, FMath::Sin(Rad) * R, FinZ),
				FVector(0.35f, 4.0f, 2.6f), Black, false,
				FRotator(0.f, A, 15.f));
		}

		// 33 Raptor bells — outer ring + mid + center (simplified clusters)
		const float EngZ = OLM_H + 70.f;
		for (int32 i = 0; i < 13; ++i)
		{
			const float A = i * (360.f / 13.f);
			const float Rad = FMath::DegreesToRadians(A);
			const float R = Diameter * 0.34f;
			Add(SphereMesh ? SphereMesh : CubeMesh,
				FVector(FMath::Cos(Rad) * R, FMath::Sin(Rad) * R, EngZ),
				FVector(1.05f, 1.05f, 1.55f), Black, false);
		}
		for (int32 i = 0; i < 10; ++i)
		{
			const float A = i * 36.f;
			const float Rad = FMath::DegreesToRadians(A);
			const float R = Diameter * 0.20f;
			Add(SphereMesh ? SphereMesh : CubeMesh,
				FVector(FMath::Cos(Rad) * R, FMath::Sin(Rad) * R, EngZ - 25.f),
				FVector(0.95f, 0.95f, 1.4f), Black, false);
		}
		for (int32 i = 0; i < 3; ++i)
		{
			const float A = i * 120.f;
			const float Rad = FMath::DegreesToRadians(A);
			Add(SphereMesh ? SphereMesh : CubeMesh,
				FVector(FMath::Cos(Rad) * 80.f, FMath::Sin(Rad) * 80.f, EngZ - 45.f),
				FVector(1.1f, 1.1f, 1.45f), Black, false);
		}

		// ========== SHIP (upper stage) ==========
		const float ShipBaseZ = OLM_H + SuperHeavyH;
		const float ShipCenterZ = ShipBaseZ + ShipH * 0.5f;
		Add(CylinderMesh, FVector(0.f, 0.f, ShipCenterZ),
			FVector(Dxy * 0.98f, Dxy * 0.98f, ShipH / 100.f * 0.92f), Tile, false);

		// Heat shield (windward dark half)
		Add(CubeMesh, FVector(-Diameter * 0.46f, 0.f, ShipCenterZ - 100.f),
			FVector(0.18f, Dxy * 0.92f, ShipH / 100.f * 0.78f), HeatTile, false);

		// Ship barrel rings
		for (int32 i = 1; i <= 4; ++i)
		{
			const float Z = ShipBaseZ + ShipH * (float(i) / 5.f);
			Add(CubeMesh, FVector(0.f, 0.f, Z),
				FVector(Dxy * 1.0f, Dxy * 1.0f, 0.15f), SteelBand, false);
		}

		// Forward flaps (2) — upper ship
		const float FlapZ = ShipBaseZ + ShipH * 0.58f;
		Add(CubeMesh, FVector(0.f, Diameter * 0.58f, FlapZ),
			FVector(0.32f, 3.0f, 5.0f), Black, false, FRotator(0.f, 0.f, -12.f));
		Add(CubeMesh, FVector(0.f, -Diameter * 0.58f, FlapZ),
			FVector(0.32f, 3.0f, 5.0f), Black, false, FRotator(0.f, 0.f, 12.f));

		// Aft flaps (2)
		const float AftFlapZ = ShipBaseZ + ShipH * 0.22f;
		Add(CubeMesh, FVector(0.f, Diameter * 0.52f, AftFlapZ),
			FVector(0.28f, 2.4f, 3.4f), Black, false);
		Add(CubeMesh, FVector(0.f, -Diameter * 0.52f, AftFlapZ),
			FVector(0.28f, 2.4f, 3.4f), Black, false);

		// Nose cone (sphere + taper rings)
		const float NoseZ = ShipBaseZ + ShipH - 280.f;
		Add(SphereMesh, FVector(0.f, 0.f, NoseZ),
			FVector(Dxy * 0.92f, Dxy * 0.92f, Dxy * 1.15f), Tile, false);
		Add(CylinderMesh, FVector(0.f, 0.f, NoseZ - 350.f),
			FVector(Dxy * 0.75f, Dxy * 0.75f, 3.5f), Tile, false);

		// Ship engines: 3 SL + 3 Vac (outer larger bells)
		const float ShipEngZ = ShipBaseZ + 70.f;
		for (int32 i = 0; i < 3; ++i)
		{
			const float A = i * 120.f;
			const float Rad = FMath::DegreesToRadians(A);
			Add(SphereMesh, FVector(FMath::Cos(Rad) * 180.f, FMath::Sin(Rad) * 180.f, ShipEngZ),
				FVector(1.25f, 1.25f, 1.7f), Black, false);
			Add(SphereMesh, FVector(FMath::Cos(Rad) * 340.f, FMath::Sin(Rad) * 340.f, ShipEngZ + 50.f),
				FVector(1.55f, 1.55f, 2.3f), SteelDark, false);
		}

		// Interstage ring highlight
		Add(CubeMesh, FVector(0.f, 0.f, ShipBaseZ + 40.f),
			FVector(Dxy * 1.05f, Dxy * 1.05f, 0.55f), Yellow, false);
	}
	else
	{
		Add(CubeMesh, FVector(0.f, 0.f, BoosterCenterZ),
			FVector(Dxy, Dxy, SuperHeavyH / 100.f), Steel, false);
		Add(CubeMesh, FVector(0.f, 0.f, OLM_H + SuperHeavyH + ShipH * 0.5f),
			FVector(Dxy, Dxy, ShipH / 100.f), Tile, false);
	}

	// Engine plume (launch-day glow)
	Add(SphereMesh ? SphereMesh : CubeMesh, FVector(0.f, 0.f, OLM_H * 0.25f),
		FVector(14.f, 14.f, 7.f), Flame, true);
	Add(SphereMesh ? SphereMesh : CubeMesh, FVector(0.f, 0.f, OLM_H * 0.12f),
		FVector(8.f, 8.f, 4.5f), FlameCore, true);
	if (EngineGlow)
	{
		EngineGlow->SetRelativeLocation(FVector(0.f, 0.f, OLM_H * 0.35f));
	}

	// ========== MECHAZILLA (launch tower + chopsticks) ==========
	// Tower stands beside stack so arms can reach the vehicle (~22–28 m offset).
	const float TowerX = 2400.f; // ~24 m from pad centerline
	const float TowerW = 4.2f;   // cube scale → ~4.2 m square shaft (visual lattice core)

	// Main tower shaft
	Add(CubeMesh, FVector(TowerX, 0.f, TowerH * 0.5f),
		FVector(TowerW, TowerW, TowerH / 100.f), TowerGrey, false);
	// Secondary verticals (lattice look)
	Add(CubeMesh, FVector(TowerX + 180.f, 180.f, TowerH * 0.5f),
		FVector(0.9f, 0.9f, TowerH / 100.f), TowerDark, false);
	Add(CubeMesh, FVector(TowerX + 180.f, -180.f, TowerH * 0.5f),
		FVector(0.9f, 0.9f, TowerH / 100.f), TowerDark, false);
	Add(CubeMesh, FVector(TowerX - 180.f, 180.f, TowerH * 0.5f),
		FVector(0.9f, 0.9f, TowerH / 100.f), TowerDark, false);
	Add(CubeMesh, FVector(TowerX - 180.f, -180.f, TowerH * 0.5f),
		FVector(0.9f, 0.9f, TowerH / 100.f), TowerDark, false);

	// Cross bracing every ~15 m
	for (int32 i = 1; i <= 9; ++i)
	{
		const float Z = (TowerH / 10.f) * float(i);
		Add(CubeMesh, FVector(TowerX, 0.f, Z),
			FVector(4.8f, 0.4f, 0.35f), Black, false);
		Add(CubeMesh, FVector(TowerX, 0.f, Z),
			FVector(0.4f, 4.8f, 0.35f), Black, false);
		// Diagonal-ish mid brace
		Add(CubeMesh, FVector(TowerX + 50.f, 0.f, Z + 200.f),
			FVector(0.35f, 4.2f, 0.3f), TowerDark, false, FRotator(0.f, 0.f, 25.f));
	}

	// Tower base / concrete plinth
	Add(CubeMesh, FVector(TowerX, 0.f, 200.f),
		FVector(8.f, 8.f, 4.f), Concrete, false);
	Add(CubeMesh, FVector(TowerX, 0.f, 80.f),
		FVector(12.f, 12.f, 1.2f), ConcreteHi, false);

	// Quick-disconnect boom (mid booster, yellow)
	const float QD_Z = OLM_H + SuperHeavyH * 0.48f;
	const float QD_Len = TowerX * 0.72f;
	Add(CubeMesh, FVector(TowerX - QD_Len * 0.5f, 0.f, QD_Z),
		FVector(QD_Len / 100.f, 1.0f, 1.1f), Yellow, false);
	Add(CubeMesh, FVector(Diameter * 0.55f + 80.f, 0.f, QD_Z),
		FVector(1.2f, 1.8f, 1.6f), SteelDark, false);

	// Ship QD / crew-access arm higher up
	const float ShipQD_Z = OLM_H + SuperHeavyH + ShipH * 0.35f;
	Add(CubeMesh, FVector(TowerX - QD_Len * 0.45f, 200.f, ShipQD_Z),
		FVector(QD_Len / 100.f * 0.85f, 0.7f, 0.7f), SteelBand, false);

	// ----- CHOPSTICKS -----
	// Catch height on Super Heavy (public: mid/upper booster body)
	const float CatchZ = OLM_H + SuperHeavyH * 0.70f;
	const float ArmReach = TowerX - Diameter * 0.55f; // from tower face toward stack
	const float ArmY = 620.f; // left/right separation of the two arms

	// Carriage rails on tower (vertical travel path for arms)
	Add(CubeMesh, FVector(TowerX - 220.f, ArmY, TowerH * 0.45f),
		FVector(0.55f, 0.55f, TowerH / 100.f * 0.7f), Black, false);
	Add(CubeMesh, FVector(TowerX - 220.f, -ArmY, TowerH * 0.45f),
		FVector(0.55f, 0.55f, TowerH / 100.f * 0.7f), Black, false);

	// Carriage blocks at catch height
	Add(CubeMesh, FVector(TowerX - 250.f, ArmY, CatchZ),
		FVector(2.2f, 2.0f, 2.5f), Steel, false);
	Add(CubeMesh, FVector(TowerX - 250.f, -ArmY, CatchZ),
		FVector(2.2f, 2.0f, 2.5f), Steel, false);

	// Long chopstick arms (horizontal)
	const float ArmMidX = TowerX - ArmReach * 0.5f - 100.f;
	Add(CubeMesh, FVector(ArmMidX, ArmY, CatchZ),
		FVector(ArmReach / 100.f, 1.35f, 1.55f), Steel, false);
	Add(CubeMesh, FVector(ArmMidX, -ArmY, CatchZ),
		FVector(ArmReach / 100.f, 1.35f, 1.55f), Steel, false);

	// Inner catch pads (yellow) near vehicle skin
	Add(CubeMesh, FVector(Diameter * 0.62f, ArmY * 0.78f, CatchZ),
		FVector(1.8f, 2.8f, 2.2f), Yellow, false);
	Add(CubeMesh, FVector(Diameter * 0.62f, -ArmY * 0.78f, CatchZ),
		FVector(1.8f, 2.8f, 2.2f), Yellow, false);

	// Cross-brace between chopsticks (rear)
	Add(CubeMesh, FVector(TowerX - 400.f, 0.f, CatchZ),
		FVector(0.8f, ArmY * 2.f / 100.f * 0.9f, 0.9f), SteelDark, false);

	// Counterweights / mass on tower side of arms
	Add(CubeMesh, FVector(TowerX + 350.f, ArmY, CatchZ),
		FVector(2.5f, 1.8f, 2.0f), TowerDark, false);
	Add(CubeMesh, FVector(TowerX + 350.f, -ArmY, CatchZ),
		FVector(2.5f, 1.8f, 2.0f), TowerDark, false);

	// Tower crown / work platform
	Add(CubeMesh, FVector(TowerX, 0.f, TowerH - 150.f),
		FVector(6.5f, 6.5f, 1.2f), SteelDark, false);
	Add(CubeMesh, FVector(TowerX, 0.f, TowerH - 40.f),
		FVector(3.5f, 3.5f, 1.0f), Yellow, false);

	// Floodlights up the tower (starbase night silhouette)
	for (int32 i = 0; i < 6; ++i)
	{
		const float Z = TowerH * (0.18f + i * 0.14f);
		Add(CubeMesh, FVector(TowerX + 280.f, 0.f, Z),
			FVector(0.55f, 0.55f, 0.4f), Flood, true);
		Add(CubeMesh, FVector(TowerX - 100.f, 280.f, Z),
			FVector(0.45f, 0.45f, 0.35f), Flood, true);
	}

	// Pad perimeter light poles
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = i * 36.f;
		const float Rad = FMath::DegreesToRadians(A);
		const float R = 2600.f;
		const FVector Base(FMath::Cos(Rad) * R, FMath::Sin(Rad) * R, 0.f);
		Add(CubeMesh, Base + FVector(0.f, 0.f, 280.f),
			FVector(0.32f, 0.32f, 5.5f), SteelDark, false);
		Add(CubeMesh, Base + FVector(0.f, 0.f, 560.f),
			FVector(0.55f, 0.55f, 0.35f), Flood, true);
	}

	// Water deluge / tank stub near pad
	Add(CubeMesh, FVector(-1800.f, 2200.f, 400.f),
		FVector(6.f, 6.f, 8.f), SteelDark, false);
	Add(CylinderMesh, FVector(-1800.f, 2200.f, 900.f),
		FVector(5.f, 5.f, 4.f), SteelBand, false);
}

void ARDStarship::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	GlowPhase += DeltaSeconds;
	if (EngineGlow)
	{
		const float P = 0.5f + 0.5f * FMath::Sin(GlowPhase * 2.5f);
		EngineGlow->SetIntensity(130000.f + 110000.f * P);
	}
}
