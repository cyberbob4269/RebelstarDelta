// Rebelstar Delta — base build + heavy graphics dress

#include "RDMapBuilder.h"
#include "RDDestructible.h"
#include "RDGameMode.h"
#include "RDIsaacNode.h"
#include "RDRoomDoor.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

static const TCHAR* GFallbackMap[] = {
	TEXT("################"),
	TEXT("#..............#"),
	TEXT("#..II..........#"),
	TEXT("#..............#"),
	TEXT("######%#########"),
	TEXT("...............#"),
	TEXT("...............#"),
	TEXT("P..............#"),
	TEXT("################"),
	nullptr
};

ARDMapBuilder::ARDMapBuilder()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Unlit(TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"));
	if (Cube.Succeeded()) CubeMesh = Cube.Object;
	if (Sph.Succeeded()) SphereMesh = Sph.Object;
	if (Cyl.Succeeded()) CylinderMesh = Cyl.Object;
	if (Mat.Succeeded()) ShapeMat = Mat.Object;
	if (Unlit.Succeeded()) UnlitMat = Unlit.Object;
	if (!UnlitMat) UnlitMat = ShapeMat;
}

void ARDMapBuilder::BeginPlay()
{
	Super::BeginPlay();
	BuildBase();
}

bool ARDMapBuilder::LoadLogicRows(TArray<FString>& OutRows) const
{
	OutRows.Reset();
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data/MoonbaseDelta.logic.txt");
	FString Raw;
	if (FFileHelper::LoadFileToString(Raw, *Path))
	{
		Raw.ParseIntoArrayLines(OutRows, false);
		OutRows.RemoveAll([](const FString& S) { return S.TrimStartAndEnd().IsEmpty(); });
		if (OutRows.Num() > 0)
		{
			return true;
		}
	}
	for (int32 i = 0; GFallbackMap[i] != nullptr; ++i)
	{
		OutRows.Add(FString(GFallbackMap[i]));
	}
	return OutRows.Num() > 0;
}

void ARDMapBuilder::ClearPrevious()
{
	if (!GetWorld()) return;
	TArray<AActor*> Kill;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->Tags.Contains(FName(TEXT("RD_Base"))) || It->Tags.Contains(FName(TEXT("RD_Isaac"))))
		{
			Kill.Add(*It);
		}
	}
	for (AActor* A : Kill) A->Destroy();
}

bool ARDMapBuilder::WantSeeThroughRoofs() const
{
	if (const ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		return GM->WantSeeThroughRoofs();
	}
	return true; // default open for spectator-first workflow
}

AActor* ARDMapBuilder::SpawnShape(UStaticMesh* Mesh, const FVector& WorldLoc, const FVector& Scale,
	const FRotator& Rot, const FLinearColor& Color, bool bCollision, bool bUnlit)
{
	if (!GetWorld() || !Mesh) return nullptr;
	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(WorldLoc, Rot);
	if (!Actor) return nullptr;
	Actor->SetMobility(EComponentMobility::Movable);
	UStaticMeshComponent* SMC = Actor->GetStaticMeshComponent();
	SMC->SetMobility(EComponentMobility::Movable);
	SMC->SetStaticMesh(Mesh);
	SMC->SetWorldScale3D(Scale);
	SMC->SetCastShadow(bCollision);
	if (bCollision)
	{
		SMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SMC->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	UMaterialInterface* Parent = bUnlit ? UnlitMat.Get() : ShapeMat.Get();
	if (Parent)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
			MID->SetVectorParameterValue(TEXT("Tint"), Color);
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			SMC->SetMaterial(0, MID);
		}
	}
	Actor->Tags.Add(FName(TEXT("RD_Base")));
	return Actor;
}

void ARDMapBuilder::SpawnCeilingTile(const FVector& Cell, float CeilingZ, float CS, const FLinearColor& Color)
{
	if (WantSeeThroughRoofs()) return; // open top for spectator battle view
	SpawnBox(Cell + FVector(0.f, 0.f, CeilingZ),
		FVector(CS / 100.f * 0.98f, CS / 100.f * 0.98f, 0.24f), Color, false);
}

AActor* ARDMapBuilder::SpawnBox(const FVector& WorldLoc, const FVector& Scale, const FLinearColor& Color,
	bool bCollision, bool bBreach, bool bUnlit)
{
	if (!GetWorld() || !CubeMesh) return nullptr;
	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(WorldLoc, FRotator::ZeroRotator);
	if (!Actor) return nullptr;
	Actor->SetMobility(EComponentMobility::Movable);
	UStaticMeshComponent* SMC = Actor->GetStaticMeshComponent();
	SMC->SetMobility(EComponentMobility::Movable);
	SMC->SetStaticMesh(CubeMesh);
	SMC->SetWorldScale3D(Scale);
	SMC->SetCastShadow(bCollision);
	if (bCollision)
	{
		SMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SMC->SetCollisionProfileName(TEXT("BlockAll"));
		// Tall thin props (walls) block pawns; very flat high pieces treated as roof décor
		// already use bCollision=false. Wall tops remain solid — floor-snap avoids them.
	}
	else
	{
		// Visual-only: still block shots (Visibility) so LOS works, but pawns fall through
		// if they ever get stuck above a false "roof" shell.
		const bool bLikelyCeiling = WorldLoc.Z > 250.f && Scale.Z < 1.f;
		if (bLikelyCeiling)
		{
			SMC->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			SMC->SetCollisionResponseToAllChannels(ECR_Ignore);
			SMC->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			SMC->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			SMC->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			Actor->Tags.Add(FName(TEXT("RD_Ceiling")));
		}
		else
		{
			SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	UMaterialInterface* Parent = bUnlit ? UnlitMat.Get() : ShapeMat.Get();
	if (Parent)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
			MID->SetVectorParameterValue(TEXT("Tint"), Color);
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			SMC->SetMaterial(0, MID);
		}
	}
	Actor->Tags.Add(FName(TEXT("RD_Base")));
	if (bBreach) Actor->Tags.Add(FName(TEXT("RD_Breach")));
	return Actor;
}

void ARDMapBuilder::SpawnInteriorLight(const FVector& Loc, const FLinearColor& Col, float Intensity, float Radius)
{
	if (!GetWorld()) return;
	if (APointLight* L = GetWorld()->SpawnActor<APointLight>(Loc, FRotator::ZeroRotator))
	{
		if (UPointLightComponent* C = Cast<UPointLightComponent>(L->GetLightComponent()))
		{
			C->SetIntensity(Intensity);
			C->SetAttenuationRadius(Radius);
			C->SetLightColor(Col);
			C->SetCastShadows(false);
		}
		L->Tags.Add(FName(TEXT("RD_Base")));
	}
}

void ARDMapBuilder::FindWestEntrance(const TArray<FString>& Rows, int32& OutY, int32& OutWallX) const
{
	// LOCKED PLAN (do not re-derive from "deepest west gap"):
	// Classic Rebelstar / Moonbase Delta: player approaches from the west at the P marker.
	// Airlock sits on the west outer wall face at column 4, centered on the P row.
	// The northern open row (first wall near col ~25 on some rows) is NOT the airlock —
	// that was the plan drift that shifted the whole entrance.
	//
	// Map truth (MoonbaseDelta.logic.txt):
	//   Y≈13:  ..P.#....   → P outside, wall col 4
	//   Y≈11:  .A.R#....  → ally corridor just north (doorway may span into it)
	OutY = Rows.Num() / 2;
	OutWallX = 4;
	int32 PRow = -1;
	int32 FallbackCorridor = -1;

	for (int32 Y = 0; Y < Rows.Num(); ++Y)
	{
		const FString& R = Rows[Y];
		if (R.Contains(TEXT("P")))
		{
			PRow = Y;
		}
		// Fallback only if map has no P: open west-face corridor at col 4
		if (R.Len() > 6 && R[4] == TEXT('#') && (R.Contains(TEXT("A")) || R.Contains(TEXT("R"))))
		{
			bool bOpenHall = true;
			for (int32 X = 5; X < FMath::Min(R.Len(), 14); ++X)
			{
				if (R[X] == TEXT('#')) { bOpenHall = false; break; }
			}
			if (bOpenHall) FallbackCorridor = Y;
		}
	}

	// Prefer P-row always when present (player approach = airlock center)
	if (PRow >= 0)
	{
		OutY = PRow;
		OutWallX = 4;
	}
	else if (FallbackCorridor >= 0)
	{
		OutY = FallbackCorridor;
		OutWallX = 4;
	}

	// Wall column: first solid cell on that row, but NEVER accept a deep interior wall
	// (col > 8) — that recreates the northern-gap drift.
	if (Rows.IsValidIndex(OutY))
	{
		const FString& R = Rows[OutY];
		for (int32 X = 0; X < FMath::Min(R.Len(), 8); ++X)
		{
			if (R[X] == TEXT('#') || R[X] == TEXT('%'))
			{
				OutWallX = X;
				break;
			}
		}
	}
	// Hard floor: classic west face is column 4 on this map
	if (OutWallX > 6)
	{
		OutWallX = 4;
	}

	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] West airlock LOCKED to plan: row=%d wallCol=%d (P-row approach, not northern gap)"),
		OutY, OutWallX);
}

void ARDMapBuilder::SpawnAirlockVolume(const FVector& Center, float OuterX, float InnerX)
{
	// Dual-chamber airlock: OuterX (moon/EVA) → mid seal → InnerX (base wall face).
	// Matches Foundation / Artemis airlock-first stacks: two yellow frames + pressure collar.
	const float MidX = (OuterX + InnerX) * 0.5f;
	const float HalfLen = FMath::Abs(InnerX - OuterX) * 0.5f;
	const float LenScale = FMath::Max(2.f, (HalfLen * 2.f) / 100.f);
	const float ChamberX = MidX; // pressure door between EVA and base

	const FVector C(MidX, Center.Y, 0.f);
	const float Y = Center.Y;

	const FLinearColor Yel(1.f, 0.85f, 0.12f);
	const FLinearColor YelGlow(1.f, 0.9f, 0.3f);
	const FLinearColor Hull(0.30f, 0.32f, 0.36f);
	const FLinearColor HullHi(0.48f, 0.50f, 0.55f);
	const FLinearColor HullRim(0.55f, 0.58f, 0.62f);
	const FLinearColor Floor(0.16f, 0.17f, 0.19f);
	const FLinearColor Stripe(0.9f, 0.2f, 0.1f);
	const FLinearColor Glass(0.35f, 0.55f, 0.75f);
	const FLinearColor Panel(0.22f, 0.24f, 0.28f);

	// Main tunnel hull
	SpawnBox(C + FVector(0.f, -210.f, 175.f), FVector(LenScale, 0.55f, 3.5f), Hull, true);
	SpawnBox(C + FVector(0.f, 210.f, 175.f), FVector(LenScale, 0.55f, 3.5f), Hull, true);
	SpawnBox(C + FVector(0.f, 0.f, 360.f), FVector(LenScale, 4.4f, 0.5f), HullHi, true);
	SpawnBox(C + FVector(0.f, 0.f, 12.f), FVector(LenScale, 4.1f, 0.28f), Floor, true);

	// Rib rings along tunnel (structural)
	for (int32 i = 0; i < 5; ++i)
	{
		const float T = float(i) / 4.f;
		const float RX = FMath::Lerp(OuterX + 80.f, InnerX - 80.f, T);
		SpawnBox(FVector(RX, Y, 175.f), FVector(0.22f, 4.3f, 3.4f), HullRim, true);
	}

	// Mid pressure collar (frame only — walkable gap down the center)
	SpawnBox(FVector(ChamberX, Y - 175.f, 175.f), FVector(0.35f, 0.7f, 3.3f), Panel, true);
	SpawnBox(FVector(ChamberX, Y + 175.f, 175.f), FVector(0.35f, 0.7f, 3.3f), Panel, true);
	SpawnBox(FVector(ChamberX, Y, 340.f), FVector(0.35f, 3.6f, 0.4f), Panel, true);
	SpawnBox(FVector(ChamberX, Y, 30.f), FVector(0.35f, 3.6f, 0.35f), Panel, true);
	SpawnBox(FVector(ChamberX, Y - 120.f, 175.f), FVector(0.2f, 0.15f, 2.4f), Yel, true);
	SpawnBox(FVector(ChamberX, Y + 120.f, 175.f), FVector(0.2f, 0.15f, 2.4f), Yel, true);
	SpawnBox(FVector(ChamberX, Y, 320.f), FVector(0.25f, 2.2f, 0.2f), YelGlow, false, false, true);

	auto SpawnFrame = [&](float X, const FLinearColor& Col)
	{
		SpawnBox(FVector(X, Y - 200.f, 175.f), FVector(0.6f, 0.8f, 3.6f), Col, true);
		SpawnBox(FVector(X, Y + 200.f, 175.f), FVector(0.6f, 0.8f, 3.6f), Col, true);
		SpawnBox(FVector(X, Y, 370.f), FVector(0.6f, 4.4f, 0.55f), Col, true);
		SpawnBox(FVector(X, Y, 25.f), FVector(0.6f, 4.4f, 0.35f), Col, true);
		SpawnBox(FVector(X, Y, 175.f), FVector(0.18f, 3.9f, 0.12f), YelGlow, false, false, true);
	};
	SpawnFrame(OuterX, Yel);   // EVA hatch
	SpawnFrame(InnerX, Yel);   // base hatch (flush with wall face)

	// Viewport ports on side walls
	SpawnBox(FVector(MidX - 150.f, Y - 220.f, 220.f), FVector(0.9f, 0.12f, 0.7f), Glass, false, false, true);
	SpawnBox(FVector(MidX + 150.f, Y + 220.f, 220.f), FVector(0.9f, 0.12f, 0.7f), Glass, false, false, true);

	// Hazard chevrons on floor
	const int32 Stripes = FMath::Clamp(FMath::RoundToInt(LenScale), 4, 10);
	for (int32 i = 0; i < Stripes; ++i)
	{
		const float T = (Stripes <= 1) ? 0.5f : float(i) / float(Stripes - 1);
		const float SX = FMath::Lerp(OuterX + 50.f, InnerX - 50.f, T);
		SpawnBox(FVector(SX, Y, 22.f), FVector(0.5f, 3.3f, 0.1f),
			(i % 2 == 0) ? Yel : Stripe, true);
	}

	// Status lights + control panel stubs
	SpawnBox(FVector(MidX, Y - 195.f, 300.f), FVector(0.18f, 0.18f, 0.18f), FLinearColor(0.2f, 1.f, 0.4f), false, false, true);
	SpawnBox(FVector(MidX, Y + 195.f, 300.f), FVector(0.18f, 0.18f, 0.18f), FLinearColor(0.2f, 1.f, 0.4f), false, false, true);
	SpawnBox(FVector(OuterX + 120.f, Y - 230.f, 140.f), FVector(0.35f, 0.15f, 0.9f), Panel, true);
	SpawnBox(FVector(InnerX - 120.f, Y + 230.f, 140.f), FVector(0.35f, 0.15f, 0.9f), Panel, true);

	// EVA rack outside outer hatch (suit stow / umbilical stub)
	SpawnBox(FVector(OuterX - 180.f, Y - 320.f, 90.f), FVector(1.2f, 0.8f, 1.6f), Hull, true);
	SpawnBox(FVector(OuterX - 180.f, Y - 320.f, 190.f), FVector(0.4f, 0.4f, 0.3f), Yel, false, false, true);
	SpawnBox(FVector(OuterX - 180.f, Y + 320.f, 90.f), FVector(1.2f, 0.8f, 1.6f), Hull, true);

	SpawnInteriorLight(FVector(MidX, Y, 300.f), FLinearColor(1.f, 0.92f, 0.55f), 28000.f, 2400.f);
	SpawnInteriorLight(FVector(OuterX + 80.f, Y, 280.f), FLinearColor(1.f, 0.85f, 0.4f), 14000.f, 1600.f);
}

void ARDMapBuilder::SpawnIsaacAt(const FVector& WorldLoc)
{
	if (!GetWorld()) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARDIsaacNode* Node = GetWorld()->SpawnActor<ARDIsaacNode>(WorldLoc, FRotator(0.f, 180.f, 0.f), P);
	if (Node) Node->Tags.Add(FName(TEXT("RD_Base")));
}

void ARDMapBuilder::FindIsaacDoorCell(const TArray<FString>& Rows, int32& OutX, int32& OutY) const
{
	// Default: classic south wall breach under ISAAC cluster (col 31, row 10)
	OutX = 31;
	OutY = 10;

	// Prefer a % (breach) cell south of the I cluster if present
	int32 MinIY = 999999;
	int32 MaxIY = 0;
	int32 SumIX = 0;
	int32 CountI = 0;
	for (int32 Y = 0; Y < Rows.Num(); ++Y)
	{
		const FString& R = Rows[Y];
		for (int32 X = 0; X < R.Len(); ++X)
		{
			if (R[X] == TEXT('I'))
			{
				MinIY = FMath::Min(MinIY, Y);
				MaxIY = FMath::Max(MaxIY, Y);
				SumIX += X;
				++CountI;
			}
		}
	}
	if (CountI == 0) return;

	const int32 CenterIX = SumIX / CountI;
	// Walk south from ISAAC for a wall/breach on the corridor wall
	for (int32 Y = MaxIY + 1; Y < FMath::Min(Rows.Num(), MaxIY + 8); ++Y)
	{
		const FString& R = Rows[Y];
		// Prefer center column, then nearby
		for (int32 D = 0; D <= 3; ++D)
		{
			for (int32 S = -1; S <= 1; S += 2)
			{
				const int32 X = CenterIX + D * (D == 0 ? 0 : S);
				if (!R.IsValidIndex(X)) continue;
				if (R[X] == TEXT('%') || R[X] == TEXT('#'))
				{
					// Only accept if south of this is open corridor (the long hall)
					if (Y + 1 < Rows.Num() && Rows[Y + 1].IsValidIndex(X))
					{
						const TCHAR Below = Rows[Y + 1][X];
						if (Below == TEXT('.') || Below == TEXT('A') || Below == TEXT('R') || Below == TEXT('d'))
						{
							OutX = X;
							OutY = Y;
							UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] ISAAC door cell found at col=%d row=%d"), OutX, OutY);
							return;
						}
					}
				}
			}
			if (D == 0) break;
		}
	}
}

void ARDMapBuilder::SpawnDestructibleAt(const FVector& WorldLoc, float HP, const FLinearColor& Color,
	const FString& Kind, const FVector& Scale, bool bUnlit)
{
	if (!GetWorld()) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ARDDestructible* D = GetWorld()->SpawnActor<ARDDestructible>(WorldLoc, FRotator::ZeroRotator, P))
	{
		D->Configure(HP, Color, Kind, Scale, bUnlit);
		D->Tags.Add(FName(TEXT("RD_Base")));
	}
}

void ARDMapBuilder::SpawnIsaacRoomDoor(const FVector& WorldLoc, float CS)
{
	if (!GetWorld()) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Door faces north-south approach (rotation 0 keeps panels on Y axis)
	ARDRoomDoor* Door = GetWorld()->SpawnActor<ARDRoomDoor>(WorldLoc, FRotator::ZeroRotator, P);
	if (!Door) return;

	// Standby just inside (north of door, into ISAAC sector). Map Y increases south in our convention?
	// MapOrigin.Y + (row+0.5)*CS — larger row = larger Y. Row 10 door, row 9 is smaller Y = north if Y increases south...
	// Rows increase downward in the text file = +Y in world. So "inside" ISAAC is smaller Y (north).
	Door->BlockPoint = WorldLoc + FVector(0.f, 0.f, 160.f);
	Door->StandbyPoint = WorldLoc + FVector(0.f, -CS * 1.4f, 160.f); // north of door
	Door->Tags.Add(FName(TEXT("RD_Base")));
	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Spawned ISAAC room door at %s"), *WorldLoc.ToCompactString());
}

void ARDMapBuilder::SpawnConstructionYard(const FVector& Anchor)
{
	const FLinearColor Brick(0.58f, 0.52f, 0.42f);
	const FLinearColor BrickDark(0.4f, 0.36f, 0.3f);
	const FLinearColor Yellow(0.95f, 0.72f, 0.1f);
	const FLinearColor Solar(0.12f, 0.18f, 0.55f);
	const FLinearColor SolarHi(0.25f, 0.4f, 0.9f);

	for (int32 z = 0; z < 5; ++z)
	{
		for (int32 x = 0; x < 9; ++x)
		{
			for (int32 y = 0; y < 3; ++y)
			{
				if (z > 2 && x > 5) continue;
				SpawnBox(Anchor + FVector(x * 125.f, y * 95.f, z * 100.f + 55.f),
					FVector(1.15f, 0.7f, 0.9f), (x + y + z) % 2 ? Brick : BrickDark, true);
			}
		}
	}
	for (int32 i = 0; i < 14; ++i)
	{
		const float H = (i < 8) ? 3.5f : (i < 11 ? 2.2f : 1.0f);
		SpawnBox(Anchor + FVector(-350.f, 100.f + i * 145.f, H * 50.f),
			FVector(1.0f, 1.35f, H), Brick, true);
	}
	// Digger
	SpawnBox(Anchor + FVector(1200.f, 700.f, 120.f), FVector(3.8f, 2.2f, 1.7f), Yellow, true);
	SpawnBox(Anchor + FVector(1550.f, 850.f, 95.f), FVector(2.8f, 0.55f, 0.5f), Yellow, true);
	SpawnBox(Anchor + FVector(1750.f, 900.f, 70.f), FVector(0.8f, 1.2f, 0.9f), FLinearColor(0.3f, 0.3f, 0.28f), true);
	// Solar farm
	for (int32 i = 0; i < 10; ++i)
	{
		SpawnBox(Anchor + FVector(80.f + i * 190.f, -550.f, 55.f), FVector(1.7f, 1.5f, 0.1f), Solar, true);
		SpawnBox(Anchor + FVector(80.f + i * 190.f, -550.f, 62.f), FVector(1.5f, 1.3f, 0.05f), SolarHi, false, false, true);
		SpawnBox(Anchor + FVector(80.f + i * 190.f, -550.f, 25.f), FVector(0.18f, 0.18f, 0.55f), FLinearColor(0.35f, 0.35f, 0.38f), true);
	}
	SpawnInteriorLight(Anchor + FVector(700.f, 300.f, 450.f), FLinearColor(1.f, 0.88f, 0.5f), 30000.f, 3500.f);
	SpawnInteriorLight(Anchor + FVector(200.f, 900.f, 380.f), FLinearColor(1.f, 0.8f, 0.4f), 20000.f, 2800.f);
}

void ARDMapBuilder::SpawnApproachLZ(const FVector& LZ, const FVector& AirlockMouth)
{
	// Clear regolith pad + chevron road so the attack team is never inside geometry
	const FLinearColor Pad(0.42f, 0.40f, 0.36f);
	const FLinearColor PadEdge(0.55f, 0.52f, 0.42f);
	const FLinearColor Yel(0.95f, 0.78f, 0.12f);
	const FLinearColor YelGlow(1.f, 0.9f, 0.35f);
	const FLinearColor LightPole(0.35f, 0.36f, 0.4f);

	// Landing disc — thin floor only (no tall volumes that can trap the player)
	SpawnBox(LZ + FVector(0.f, 0.f, 4.f), FVector(22.f, 22.f, 0.12f), Pad, true);
	SpawnBox(LZ + FVector(0.f, 0.f, 12.f), FVector(20.f, 20.f, 0.08f), PadEdge, true);
	// Yellow ring + H mark (no collision — visual only)
	SpawnBox(LZ + FVector(0.f, 0.f, 16.f), FVector(17.f, 17.f, 0.06f), Yel, false, false, true);
	SpawnBox(LZ + FVector(0.f, 0.f, 18.f), FVector(0.35f, 3.5f, 0.08f), YelGlow, false, false, true);
	SpawnBox(LZ + FVector(0.f, 0.f, 18.f), FVector(2.8f, 0.35f, 0.08f), YelGlow, false, false, true);

	// Perimeter poles
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = i * 45.f;
		const float Rad = FMath::DegreesToRadians(A);
		const FVector P = LZ + FVector(FMath::Cos(Rad) * 900.f, FMath::Sin(Rad) * 900.f, 0.f);
		SpawnBox(P + FVector(0.f, 0.f, 200.f), FVector(0.25f, 0.25f, 4.f), LightPole, true);
		SpawnBox(P + FVector(0.f, 0.f, 420.f), FVector(0.5f, 0.5f, 0.3f), YelGlow, false, false, true);
		SpawnInteriorLight(P + FVector(0.f, 0.f, 400.f), FLinearColor(1.f, 0.95f, 0.8f), 20000.f, 2500.f);
	}

	// Chevron path LZ → airlock (only on open plain, stops short of tunnel)
	const FVector ToAir = AirlockMouth - LZ;
	const float Dist = ToAir.Size2D();
	const FVector Dir = Dist > 1.f ? FVector(ToAir.X, ToAir.Y, 0.f).GetSafeNormal() : FVector(1.f, 0.f, 0.f);
	const int32 Steps = FMath::Clamp(FMath::RoundToInt(Dist / 450.f), 8, 40);
	for (int32 i = 1; i < Steps; ++i)
	{
		const float T = float(i) / float(Steps);
		// Stop ~12 m before airlock mouth so we don't stack into tunnel mesh
		if (T > 0.88f) break;
		const FVector Pt = FMath::Lerp(LZ, AirlockMouth, T);
		const bool bEven = (i % 2) == 0;
		SpawnBox(Pt + FVector(0.f, 0.f, 12.f), FVector(1.2f, 2.8f, 0.1f),
			bEven ? Yel : FLinearColor(0.9f, 0.25f, 0.1f), true);
		if ((i % 3) == 0)
		{
			const FVector Side = FVector(-Dir.Y, Dir.X, 0.f) * 380.f;
			SpawnBox(Pt + Side + FVector(0.f, 0.f, 160.f), FVector(0.22f, 0.22f, 3.2f), LightPole, true);
			SpawnBox(Pt + Side + FVector(0.f, 0.f, 340.f), FVector(0.45f, 0.45f, 0.25f), YelGlow, false, false, true);
		}
	}

	SpawnInteriorLight(LZ + FVector(0.f, 0.f, 500.f), FLinearColor(1.f, 0.92f, 0.7f), 35000.f, 4000.f);
	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Approach LZ pad at %s"), *LZ.ToCompactString());
}

void ARDMapBuilder::SpawnExteriorDressing(const FVector& AirlockCenter, float CS, int32 MapH)
{
	const FLinearColor Hull(0.42f, 0.44f, 0.48f);
	const FLinearColor HullDark(0.28f, 0.30f, 0.34f);
	const FLinearColor Regolith(0.48f, 0.44f, 0.36f);
	const FLinearColor RegDark(0.32f, 0.30f, 0.26f);
	const FLinearColor Solar(0.12f, 0.18f, 0.55f);
	const FLinearColor SolarHi(0.28f, 0.45f, 0.95f);
	const FLinearColor Yel(0.95f, 0.75f, 0.12f);

	// Approach floodlights (runway into airlock)
	for (int32 i = 0; i < 8; ++i)
	{
		const FVector Pole = AirlockCenter + FVector(-600.f - i * 380.f, (i % 2 ? 1.f : -1.f) * 420.f, 0.f);
		SpawnBox(Pole + FVector(0.f, 0.f, 220.f), FVector(0.28f, 0.28f, 4.4f), FLinearColor(0.35f, 0.36f, 0.4f), true);
		SpawnBox(Pole + FVector(40.f, 0.f, 450.f), FVector(0.9f, 0.55f, 0.35f), FLinearColor(1.f, 0.95f, 0.85f), false, false, true);
		SpawnInteriorLight(Pole + FVector(60.f, 0.f, 440.f), FLinearColor(1.f, 0.95f, 0.88f), 35000.f, 3200.f);
	}

	// Landing beacons / approach chevrons
	for (int32 i = 0; i < 12; ++i)
	{
		const FVector B = AirlockCenter + FVector(-200.f - i * 250.f, (i % 2 ? 180.f : -180.f), 15.f);
		SpawnBox(B, FVector(0.35f, 0.35f, 0.2f), FLinearColor(1.f, 0.2f, 0.1f), true);
		SpawnBox(B + FVector(0.f, 0.f, 25.f), FVector(0.2f, 0.2f, 0.2f), FLinearColor(1.f, 0.35f, 0.15f), false, false, true);
	}

	// Regolith berms shielding airlock approach (Artemis / ISRU mood)
	for (int32 i = 0; i < 6; ++i)
	{
		const float Side = (i % 2 == 0) ? 1.f : -1.f;
		SpawnBox(AirlockCenter + FVector(-400.f - i * 220.f, Side * (520.f + i * 40.f), 60.f),
			FVector(3.5f, 2.2f, 1.2f + i * 0.15f), (i % 2) ? Regolith : RegDark, true);
	}

	// Modular habitat cylinders flanking west face (Foundation-style)
	for (int32 i = 0; i < 3; ++i)
	{
		const float Side = (i == 1) ? 0.f : (i == 0 ? -1.f : 1.f);
		const FVector Hab = AirlockCenter + FVector(200.f + i * 80.f, Side * 900.f, 0.f);
		SpawnBox(Hab + FVector(0.f, 0.f, 140.f), FVector(5.5f, 3.2f, 2.6f), Hull, true);
		SpawnBox(Hab + FVector(0.f, 0.f, 280.f), FVector(5.2f, 3.0f, 0.35f), HullDark, true);
		// Window strip
		SpawnBox(Hab + FVector(-280.f, 0.f, 160.f), FVector(0.15f, 2.4f, 0.7f),
			FLinearColor(0.4f, 0.65f, 0.9f), false, false, true);
		SpawnBox(Hab + FVector(0.f, 0.f, 40.f), FVector(5.8f, 3.4f, 0.4f), RegDark, true);
		// Connecting tube toward base
		if (i < 2)
		{
			SpawnBox(Hab + FVector(350.f, -Side * 200.f, 120.f), FVector(3.5f, 1.2f, 1.4f), HullDark, true);
		}
	}

	// Antenna cluster north of base
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector A = MapOrigin + FVector(CS * 10.f + i * 450.f, CS * 3.5f, 0.f);
		SpawnBox(A + FVector(0.f, 0.f, 280.f), FVector(0.35f, 0.35f, 5.6f), FLinearColor(0.5f, 0.52f, 0.55f), true);
		SpawnBox(A + FVector(0.f, 0.f, 580.f), FVector(3.2f, 0.2f, 1.8f), FLinearColor(0.75f, 0.75f, 0.8f), false);
		SpawnBox(A + FVector(0.f, 0.f, 600.f), FVector(0.3f, 0.3f, 0.3f), FLinearColor(0.3f, 1.f, 0.5f), false, false, true);
	}

	// Exterior solar trackers near airlock approach
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector S = AirlockCenter + FVector(-900.f - i * 160.f, 780.f, 0.f);
		SpawnBox(S + FVector(0.f, 0.f, 40.f), FVector(0.2f, 0.2f, 0.7f), HullDark, true);
		SpawnBox(S + FVector(0.f, 0.f, 90.f), FVector(2.0f, 1.4f, 0.1f), Solar, true);
		SpawnBox(S + FVector(0.f, 0.f, 98.f), FVector(1.8f, 1.2f, 0.05f), SolarHi, false, false, true);
	}

	// Rover / cargo stub on approach pad
	const FLinearColor Tire(0.08f, 0.08f, 0.09f);
	SpawnBox(AirlockCenter + FVector(-500.f, -700.f, 70.f), FVector(2.8f, 1.6f, 1.2f), Yel, true);
	SpawnBox(AirlockCenter + FVector(-620.f, -700.f, 50.f), FVector(0.9f, 1.8f, 0.7f), HullDark, true);
	SpawnBox(AirlockCenter + FVector(-380.f, -700.f, 40.f), FVector(0.5f, 0.5f, 0.5f), Hull, true);
	SpawnBox(AirlockCenter + FVector(-500.f, -620.f, 35.f), FVector(0.4f, 0.15f, 0.4f), Tire, true);
	SpawnBox(AirlockCenter + FVector(-500.f, -780.f, 35.f), FVector(0.4f, 0.15f, 0.4f), Tire, true);

	// Habitat dome teases on exterior (south-east of map)
	if (SphereMesh)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			AStaticMeshActor* Dome = GetWorld()->SpawnActor<AStaticMeshActor>(
				MapOrigin + FVector(CS * 15.f + i * 900.f, CS * (MapH * 0.7f), 80.f), FRotator::ZeroRotator);
			if (Dome)
			{
				Dome->SetMobility(EComponentMobility::Movable);
				UStaticMeshComponent* SMC = Dome->GetStaticMeshComponent();
				SMC->SetMobility(EComponentMobility::Movable);
				SMC->SetStaticMesh(SphereMesh);
				SMC->SetWorldScale3D(FVector(6.f, 6.f, 3.2f));
				SMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				if (ShapeMat)
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ShapeMat, this);
					if (MID)
					{
						const FLinearColor C(0.55f, 0.58f, 0.62f);
						MID->SetVectorParameterValue(TEXT("Color"), C);
						MID->SetVectorParameterValue(TEXT("BaseColor"), C);
						SMC->SetMaterial(0, MID);
					}
				}
				Dome->Tags.Add(FName(TEXT("RD_Base")));
			}
		}
	}
}

void ARDMapBuilder::SpawnRoofScenery(const FVector& Origin, float CS, int32 W, int32 H)
{
	// When see-through roofs: only outer-edge towers so the interior stays open for spectator cam
	const bool bOpen = WantSeeThroughRoofs();
	const float RoofZ = bOpen ? 620.f : 380.f;
	const int32 Count = bOpen ? 6 : 16;
	for (int32 i = 0; i < Count; ++i)
	{
		const float X = Origin.X + CS * (bOpen ? 2.f : 8.f) + (i % 8) * CS * (bOpen ? 4.5f : 2.2f);
		const float Y = Origin.Y + CS * (bOpen ? 2.f : 8.f) + (i / 8) * CS * (bOpen ? 10.f : 6.f);
		if (CylinderMesh)
		{
			SpawnShape(CylinderMesh, FVector(X, Y, RoofZ * 0.5f), FVector(0.35f, 0.35f, RoofZ / 100.f),
				FRotator::ZeroRotator, FLinearColor(0.25f, 0.28f, 0.32f), false);
		}
		if (!bOpen)
		{
			SpawnBox(FVector(X, Y, RoofZ), FVector(2.2f, 1.6f, 0.12f), FLinearColor(0.1f, 0.15f, 0.45f), false);
			SpawnBox(FVector(X, Y, RoofZ + 8.f), FVector(2.0f, 1.4f, 0.05f), FLinearColor(0.3f, 0.5f, 1.f), false, false, true);
		}
	}
	// Comms dish (edge of base — does not block interior view)
	if (SphereMesh)
	{
		const FVector DishLoc = Origin + FVector(CS * W * 0.92f, CS * H * 0.15f, bOpen ? 380.f : 450.f);
		SpawnShape(SphereMesh, DishLoc, FVector(5.f, 5.f, 1.2f), FRotator(-25.f, 40.f, 0.f),
			FLinearColor(0.7f, 0.72f, 0.78f), false);
	}
	// Nav beacon
	if (CylinderMesh)
	{
		SpawnShape(CylinderMesh, Origin + FVector(CS * W * 0.5f, CS * H * 0.08f, 280.f),
			FVector(0.35f, 0.35f, 2.8f), FRotator::ZeroRotator, FLinearColor(1.f, 0.25f, 0.15f), false, true);
	}
	else
	{
		SpawnBox(Origin + FVector(CS * W * 0.5f, CS * H * 0.08f, 320.f), FVector(0.4f, 0.4f, 0.4f),
			FLinearColor(1.f, 0.2f, 0.15f), false, false, true);
	}
	SpawnInteriorLight(Origin + FVector(CS * W * 0.5f, CS * H * 0.08f, 360.f),
		FLinearColor(1.f, 0.3f, 0.2f), 15000.f, 4000.f);
}

void ARDMapBuilder::BuildBase()
{
	if (!GetWorld()) return;
	ClearPrevious();

	TArray<FString> Rows;
	if (!LoadLogicRows(Rows)) return;

	const int32 H = Rows.Num();
	const int32 W = Rows[0].Len();
	const float CS = CellSize;
	const float WallH = 3.4f;

	const FLinearColor Wall(0.32f, 0.34f, 0.40f);
	const FLinearColor WallWarm(0.42f, 0.36f, 0.30f);
	const FLinearColor WallCool(0.28f, 0.34f, 0.42f);
	const FLinearColor Breach(1.0f, 0.42f, 0.08f);
	const FLinearColor Floor(0.16f, 0.18f, 0.21f);
	const FLinearColor FloorHydro(0.12f, 0.22f, 0.16f);
	const FLinearColor FloorMed(0.48f, 0.52f, 0.58f);
	const FLinearColor FloorIsaac(0.06f, 0.12f, 0.18f);
	const FLinearColor Hydro(0.12f, 0.55f, 0.28f);
	const FLinearColor HydroGlow(0.25f, 1.f, 0.45f);
	const FLinearColor Machine(0.38f, 0.32f, 0.26f);
	const FLinearColor Ceiling(0.12f, 0.13f, 0.16f);
	const FLinearColor Trim(0.65f, 0.55f, 0.28f);
	const FLinearColor NeonCyan(0.2f, 0.95f, 1.f);
	const FLinearColor NeonAmber(1.f, 0.7f, 0.2f);

	int32 IsaacCount = 0;
	int32 LightEvery = 0;

	// Align airlock to real west entrance in the logic map
	int32 EntranceY = H / 2;
	int32 EntranceWallX = 4;
	FindWestEntrance(Rows, EntranceY, EntranceWallX);
	// Doorway is 3 cells tall (Y) centered on entrance row — leave wall open there
	const int32 DoorY0 = FMath::Max(0, EntranceY - 1);
	const int32 DoorY1 = FMath::Min(H - 1, EntranceY + 1);

	// Classic ISAAC room door cell (south wall choke) — spawn ARDRoomDoor, not a one-shot breach
	int32 IsaacDoorX = 31;
	int32 IsaacDoorY = 10;
	FindIsaacDoorCell(Rows, IsaacDoorX, IsaacDoorY);
	bool bSpawnedIsaacDoor = false;

	for (int32 Y = 0; Y < H; ++Y)
	{
		const FString& Row = Rows[Y];
		for (int32 X = 0; X < Row.Len() && X < W; ++X)
		{
			const TCHAR C = Row[X];
			const FVector Cell = MapOrigin + FVector((X + 0.5f) * CS, (Y + 0.5f) * CS, 0.f);
			const float CeilingZ = WallH * 100.f + 25.f;

			// ISAAC room door — multi-HP choke, not a free one-shot % panel
			if (X == IsaacDoorX && Y == IsaacDoorY && (C == TEXT('%') || C == TEXT('#') || C == TEXT('.')))
			{
				if (!bSpawnedIsaacDoor)
				{
					SpawnIsaacRoomDoor(Cell, CS);
					// Floor under door
					SpawnBox(Cell + FVector(0.f, 0.f, 8.f),
						FVector(CS / 100.f, CS / 100.f, 0.15f), Floor, true);
					// Warning paint
					SpawnBox(Cell + FVector(0.f, 0.f, 20.f),
						FVector(CS / 100.f * 0.9f, CS / 100.f * 0.35f, 0.08f), NeonAmber, false, false, true);
					bSpawnedIsaacDoor = true;
				}
				continue;
			}

			// Open doorway where airlock meets base (don't block with wall)
			const bool bDoorway =
				(X == EntranceWallX || X == EntranceWallX + 1) &&
				(Y >= DoorY0 && Y <= DoorY1);

			FLinearColor UseWall = Wall;
			if (Y > H * 0.55f) UseWall = WallWarm;
			if (Y < H * 0.35f && X > W * 0.3f) UseWall = WallCool;

			if ((C == TEXT('#') || C == TEXT('%')) && !bDoorway)
			{
				const bool bBreach = (C == TEXT('%'));
				if (bBreach)
				{
					// Fable 5 sealed door ~60 HP — multi-hit, not one-shot
					SpawnDestructibleAt(Cell + FVector(0.f, 0.f, WallH * 50.f), 60.f, Breach,
						TEXT("Sealed door"), FVector(CS / 100.f * 0.98f, CS / 100.f * 0.98f, WallH), false);
					SpawnBox(Cell + FVector(0.f, 0.f, WallH * 50.f),
						FVector(CS / 100.f * 1.05f, CS / 100.f * 0.12f, WallH * 0.9f),
						FLinearColor(1.f, 0.5f, 0.1f), false, false, true);
				}
				else
				{
					// Main wall + slight inset panel (less pure cubes)
					SpawnBox(Cell + FVector(0.f, 0.f, WallH * 50.f),
						FVector(CS / 100.f * 0.98f, CS / 100.f * 0.98f, WallH),
						UseWall, true, false);
					SpawnBox(Cell + FVector(0.f, 0.f, WallH * 50.f),
						FVector(CS / 100.f * 0.86f, CS / 100.f * 0.86f, WallH * 0.88f),
						UseWall * FLinearColor(0.85f, 0.88f, 0.92f), false, false);
					// Soft column posts (cylinder) at wall junctions
					if (CylinderMesh && ((X + Y) % 2 == 0))
					{
						SpawnShape(CylinderMesh, Cell + FVector(CS * 0.38f, CS * 0.38f, WallH * 50.f),
							FVector(0.28f, 0.28f, WallH * 0.95f), FRotator::ZeroRotator,
							UseWall * FLinearColor(1.1f, 1.05f, 1.f), true);
					}
					SpawnBox(Cell + FVector(0.f, 0.f, 22.f),
						FVector(CS / 100.f * 1.02f, CS / 100.f * 1.02f, 0.32f), Trim, true);
					if ((X + Y) % 3 == 0)
					{
						SpawnBox(Cell + FVector(0.f, 0.f, WallH * 90.f),
							FVector(CS / 100.f * 0.9f, CS / 100.f * 0.15f, 0.12f),
							NeonAmber, false, false, true);
					}
				}
			}
			else if (C == TEXT('I'))
			{
				SpawnBox(Cell + FVector(0.f, 0.f, 10.f),
					FVector(CS / 100.f, CS / 100.f, 0.22f), FloorIsaac, true);
				SpawnCeilingTile(Cell, CeilingZ, CS, FLinearColor(0.05f, 0.12f, 0.2f));
				// Floor neon ring under cores
				if (CylinderMesh)
				{
					SpawnShape(CylinderMesh, Cell + FVector(0.f, 0.f, 18.f),
						FVector(CS / 100.f * 0.55f, CS / 100.f * 0.55f, 0.06f), FRotator::ZeroRotator,
						NeonCyan, false, true);
				}
				else
				{
					SpawnBox(Cell + FVector(0.f, 0.f, 18.f),
						FVector(CS / 100.f * 0.7f, CS / 100.f * 0.7f, 0.08f), NeonCyan, false, false, true);
				}

				const bool bLeftIsI = (X > 0 && Row[X - 1] == TEXT('I'));
				const bool bUpIsI = (Y > 0 && Rows[Y - 1].IsValidIndex(X) && Rows[Y - 1][X] == TEXT('I'));
				if (!bLeftIsI && !bUpIsI)
				{
					const FVector Rack = Cell + FVector(CS * 0.5f, CS * 0.5f, 0.f);
					SpawnIsaacAt(Rack);
					SpawnInteriorLight(Rack + FVector(0.f, 0.f, 450.f), FLinearColor(0.2f, 0.95f, 1.f), 40000.f, 3200.f);
					SpawnInteriorLight(Rack + FVector(250.f, 180.f, 280.f), FLinearColor(0.35f, 0.7f, 1.f), 18000.f, 2000.f);
					// Pillars around ISAAC (cylinders read less blocky)
					for (int32 p = 0; p < 4; ++p)
					{
						const float A = p * 90.f + 45.f;
						const float Rad = FMath::DegreesToRadians(A);
						const FVector PLoc = Rack + FVector(FMath::Cos(Rad) * 280.f, FMath::Sin(Rad) * 280.f, 180.f);
						if (CylinderMesh)
						{
							SpawnShape(CylinderMesh, PLoc, FVector(0.45f, 0.45f, 3.5f), FRotator::ZeroRotator,
								FLinearColor(0.2f, 0.22f, 0.28f), true);
						}
						else
						{
							SpawnBox(PLoc, FVector(0.4f, 0.4f, 3.5f), FLinearColor(0.2f, 0.22f, 0.28f), true);
						}
					}
					++IsaacCount;
				}
			}
			else if (C == TEXT('g'))
			{
				SpawnBox(Cell + FVector(0.f, 0.f, 8.f),
					FVector(CS / 100.f * 0.95f, CS / 100.f * 0.95f, 0.15f), FloorHydro, true);
				SpawnCeilingTile(Cell, CeilingZ, CS, FLinearColor(0.08f, 0.18f, 0.1f));
				// Plants 30 HP (2D) — sphere foliage
				SpawnDestructibleAt(Cell + FVector(0.f, 0.f, 90.f), 30.f, Hydro,
					TEXT("Hydroponics"), FVector(0.75f, 0.75f, 1.7f), false);
				if (SphereMesh)
				{
					SpawnShape(SphereMesh, Cell + FVector(0.f, 0.f, 180.f), FVector(0.65f, 0.65f, 0.55f),
						FRotator::ZeroRotator, HydroGlow, false, true);
				}
				else
				{
					SpawnBox(Cell + FVector(0.f, 0.f, 180.f), FVector(0.55f, 0.55f, 0.55f), HydroGlow, false, false, true);
				}
				if ((X + Y) % 2 == 0)
				{
					SpawnInteriorLight(Cell + FVector(0.f, 0.f, 240.f), FLinearColor(0.35f, 1.f, 0.5f), 14000.f, 1400.f);
				}
			}
			else if (C == TEXT('M') || C == TEXT('m'))
			{
				SpawnBox(Cell + FVector(0.f, 0.f, 8.f),
					FVector(CS / 100.f * 0.95f, CS / 100.f * 0.95f, 0.15f), Floor, true);
				SpawnCeilingTile(Cell, CeilingZ, CS, Ceiling);
				// Machinery 250 / furniture 80 (2D)
				const bool bBig = (C == TEXT('M'));
				SpawnDestructibleAt(Cell + FVector(0.f, 0.f, 75.f), bBig ? 250.f : 80.f, Machine,
					bBig ? TEXT("Machinery") : TEXT("Furniture"),
					bBig ? FVector(1.15f, 0.85f, 1.4f) : FVector(0.9f, 0.7f, 1.0f), false);
				if (bBig)
				{
					SpawnBox(Cell + FVector(45.f, 0.f, 110.f), FVector(0.12f, 0.7f, 0.4f), NeonCyan, false, false, true);
				}
			}
			else if (C == TEXT('T'))
			{
				// Water tank 220 HP — cylindrical silhouette via scale
				SpawnDestructibleAt(Cell + FVector(0.f, 0.f, 110.f), 220.f,
					FLinearColor(0.18f, 0.4f, 0.85f), TEXT("Water tank"), FVector(1.6f, 1.6f, 2.2f), false);
				if (CylinderMesh)
				{
					SpawnShape(CylinderMesh, Cell + FVector(0.f, 0.f, 120.f), FVector(1.35f, 1.35f, 2.0f),
						FRotator::ZeroRotator, FLinearColor(0.2f, 0.45f, 0.9f), false);
					SpawnShape(SphereMesh ? SphereMesh.Get() : CylinderMesh.Get(),
						Cell + FVector(0.f, 0.f, 230.f), FVector(0.9f, 0.9f, 0.45f),
						FRotator::ZeroRotator, FLinearColor(0.4f, 0.85f, 1.f), false, true);
				}
				else
				{
					SpawnBox(Cell + FVector(0.f, 0.f, 230.f), FVector(0.45f, 0.45f, 0.35f),
						FLinearColor(0.4f, 0.85f, 1.f), false, false, true);
				}
				SpawnInteriorLight(Cell + FVector(0.f, 0.f, 200.f), FLinearColor(0.4f, 0.7f, 1.f), 10000.f, 1200.f);
			}
			else if (C == TEXT('P'))
			{
				SpawnBox(Cell + FVector(0.f, 0.f, 15.f),
					FVector(CS / 100.f * 1.3f, CS / 100.f * 1.3f, 0.28f),
					FLinearColor(1.f, 0.85f, 0.15f), true);
			}
			else if (C == TEXT('.') || C == TEXT('A') || C == TEXT('R') || C == TEXT('d') ||
				C == TEXT('b') || C == TEXT('r') || C == TEXT('V'))
			{
				if (Row.Contains(TEXT("#")) || Row.Contains(TEXT("%")))
				{
					// Fable 5 support rooms (authored cells, no PAD)
					const bool bMedZone = (X >= 26 && X < 35 && Y >= 26 && Y < 33);
					const bool bWorkshop = (X >= 5 && X < 16 && Y >= 17 && Y < 23);
					FLinearColor FloorCol = Floor;
					if (bMedZone) FloorCol = FLinearColor(0.55f, 0.22f, 0.28f); // medbay tint
					else if (bWorkshop) FloorCol = FLinearColor(0.45f, 0.38f, 0.18f); // workshop amber
					else if (Y > H * 0.55f && X > W * 0.35f) FloorCol = FloorMed;
					// Floor plate + thin edge trim (reads less like a Minecraft slab)
					SpawnBox(Cell + FVector(0.f, 0.f, 8.f),
						FVector(CS / 100.f * 0.95f, CS / 100.f * 0.95f, 0.15f),
						FloorCol, true);
					SpawnBox(Cell + FVector(0.f, 0.f, 14.f),
						FVector(CS / 100.f * 0.88f, CS / 100.f * 0.88f, 0.04f),
						FloorCol * FLinearColor(1.15f, 1.15f, 1.2f), false);
					SpawnCeilingTile(Cell, CeilingZ, CS, Ceiling);

					// Cable trays (only when roofs present — otherwise clutter sky view)
					if (!WantSeeThroughRoofs() && (X % 3) == 0)
					{
						SpawnBox(Cell + FVector(0.f, 0.f, CeilingZ - 45.f),
							FVector(0.22f, CS / 100.f * 0.92f, 0.18f),
							FLinearColor(0.25f, 0.28f, 0.22f), false);
					}
					// Floor path stripe
					if ((Y % 5) == 0)
					{
						SpawnBox(Cell + FVector(0.f, 0.f, 16.f),
							FVector(CS / 100.f * 0.3f, CS / 100.f * 0.9f, 0.06f),
							NeonAmber, false, false, true);
					}

					if ((++LightEvery % 5) == 0)
					{
						const FLinearColor LC = bMedZone
							? FLinearColor(0.8f, 0.9f, 1.f)
							: (bWorkshop ? FLinearColor(1.f, 0.85f, 0.4f) : FLinearColor(1.f, 0.86f, 0.6f));
						SpawnInteriorLight(Cell + FVector(0.f, 0.f, 245.f), LC, 16000.f, 1500.f);
						// Wall-wash glow instead of ceiling fixture when roofs are open
						const float LightZ = WantSeeThroughRoofs() ? 160.f : (CeilingZ - 18.f);
						if (CylinderMesh && WantSeeThroughRoofs())
						{
							SpawnShape(CylinderMesh, Cell + FVector(0.f, 0.f, LightZ),
								FVector(0.25f, 0.25f, 0.35f), FRotator::ZeroRotator,
								FLinearColor(1.f, 0.92f, 0.75f), false, true);
						}
						else
						{
							SpawnBox(Cell + FVector(0.f, 0.f, LightZ),
								FVector(0.9f, 0.4f, 0.12f), FLinearColor(1.f, 0.92f, 0.75f), false, false, true);
						}
					}
				}
			}
		}
	}

	SpawnConstructionYard(MapOrigin + FVector(-2200.f, CS * H * 0.35f, 0.f));

	// Airlock: outer mouth west of wall, inner frame flush with west face of wall column
	const float WallWestFace = MapOrigin.X + float(EntranceWallX) * CS;
	const float OuterX = WallWestFace - 900.f; // tunnel length ~9m
	const float InnerX = WallWestFace;         // meets base opening
	const float EntranceWorldY = MapOrigin.Y + (float(EntranceY) + 0.5f) * CS;
	const FVector AirlockCenter(OuterX * 0.5f + InnerX * 0.5f, EntranceWorldY, 0.f);
	SpawnAirlockVolume(AirlockCenter, OuterX, InnerX);

	// Floor path connecting tunnel into doorway cells
	for (int32 Y = DoorY0; Y <= DoorY1; ++Y)
	{
		for (int32 X = EntranceWallX; X <= EntranceWallX + 1; ++X)
		{
			const FVector Cell = MapOrigin + FVector((X + 0.5f) * CS, (Y + 0.5f) * CS, 0.f);
			SpawnBox(Cell + FVector(0.f, 0.f, 10.f),
				FVector(CS / 100.f, CS / 100.f, 0.2f), FLinearColor(0.2f, 0.2f, 0.22f), true);
		}
	}

	SpawnExteriorDressing(FVector(OuterX, EntranceWorldY, 0.f), CS, H);

	// Far western attack LZ (must match ARDGameMode ApproachLZ)
	const FVector ApproachLZ(-9000.f, -5850.f, 0.f);
	const FVector AirlockMouth(OuterX, EntranceWorldY, 0.f);
	SpawnApproachLZ(ApproachLZ, AirlockMouth);

	SpawnRoofScenery(MapOrigin, CS, W, H);

	SpawnInteriorLight(MapOrigin + FVector(CS * W * 0.45f, CS * H * 0.45f, 600.f),
		FLinearColor(0.7f, 0.8f, 1.f), 8000.f, 12000.f);

	if (!bSpawnedIsaacDoor)
	{
		// Fallback: force door at default cell even if glyph missing
		const FVector Cell = MapOrigin + FVector((IsaacDoorX + 0.5f) * CS, (IsaacDoorY + 0.5f) * CS, 0.f);
		SpawnIsaacRoomDoor(Cell, CS);
	}

	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Base %dx%d ISAAC=%d airlock@row=%d wallCol=%d isaacDoor=(%d,%d)"),
		W, H, IsaacCount, EntranceY, EntranceWallX, IsaacDoorX, IsaacDoorY);
}
