// Rebelstar Delta — reliable move + killable + squad orders

#include "RDBot.h"
#include "RDCharacter.h"
#include "RDDestructible.h"
#include "RDGameMode.h"
#include "RDIsaacNode.h"
#include "RDLaserFX.h"
#include "RDRoomDoor.h"
#include "RDWreckage.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

ARDBot::ARDBot()
{
	PrimaryActorTick.bCanEverTick = true;

	// Slimmer default capsule — greybox corners + props snag fat capsules hard
	GetCapsuleComponent()->InitCapsuleSize(42.f, 90.f);
	// Pawn channel so lasers hit reliably
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// Off-the-shelf UE crowd anti-collision: Reciprocal Velocity Obstacles (RVO)
	// on CharacterMovement — same system Detour Crowd / AI MoveTo uses.
	// Requires RequestDirectMove (not raw SetActorLocation) so avoidance runs.
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = 780.f;
	Move->MaxFlySpeed = 780.f;
	Move->MaxAcceleration = 12000.f;
	Move->GravityScale = 0.f; // free XY on lunar greybox; we stabilize Z
	Move->bOrientRotationToMovement = false;
	Move->BrakingDecelerationWalking = 2000.f;
	Move->BrakingDecelerationFlying = 1800.f; // was 3200 — over-braked every frame
	Move->GroundFriction = 8.f;
	Move->MaxStepHeight = 55.f;
	Move->SetWalkableFloorAngle(50.f);
	Move->DefaultLandMovementMode = MOVE_Flying;
	Move->bUseRVOAvoidance = true;
	Move->AvoidanceConsiderationRadius = 320.f; // tighter so RVO doesn't glue packs
	Move->AvoidanceWeight = 0.35f;
	bUseControllerRotationYaw = false;
	AutoPossessAI = EAutoPossessAI::Disabled;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lit(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Unlit(TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"));
	if (Cube.Succeeded()) CubeMesh = Cube.Object;
	if (Sph.Succeeded()) SphereMesh = Sph.Object;
	if (Cyl.Succeeded()) CylinderMesh = Cyl.Object;
	if (Lit.Succeeded()) LitMat = Lit.Object;
	if (Unlit.Succeeded()) UnlitMat = Unlit.Object;
	if (!UnlitMat) UnlitMat = LitMat;

	auto MakePart = [this](const FName& Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(GetCapsuleComponent());
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(true);
		return C;
	};

	Torso = MakePart(TEXT("Torso"));
	Head = MakePart(TEXT("Head"));
	Visor = MakePart(TEXT("Visor"));
	ShoulderL = MakePart(TEXT("ShoulderL"));
	ShoulderR = MakePart(TEXT("ShoulderR"));
	ArmL = MakePart(TEXT("ArmL"));
	ArmR = MakePart(TEXT("ArmR"));
	LegL = MakePart(TEXT("LegL"));
	LegR = MakePart(TEXT("LegR"));
	Backpack = MakePart(TEXT("Backpack"));
	BadgeBase = MakePart(TEXT("BadgeBase"));
	BadgeCanton = MakePart(TEXT("BadgeCanton"));
	BadgeStripe = MakePart(TEXT("BadgeStripe"));
	BadgeCross = MakePart(TEXT("BadgeCross"));

	Halo = CreateDefaultSubobject<UPointLightComponent>(TEXT("Halo"));
	Halo->SetupAttachment(GetCapsuleComponent());
	Halo->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Halo->SetAttenuationRadius(1100.f);
	Halo->SetCastShadows(false);
	Halo->SetIntensity(20000.f);

	Tags.Add(FName(TEXT("RD_Bot")));
}

void ARDBot::SetTeam(ERDBotTeam InTeam)
{
	Team = InTeam;
	RefreshLook();
}

void ARDBot::SetOrder(ERDSquadOrder InOrder)
{
	Order = InOrder;
	if (Order == ERDSquadOrder::Hold)
	{
		HoldPoint = GetActorLocation();
	}
}

void ARDBot::ConfigureFromRole(ERDUnitRole InRole, ERDBotTeam InTeam, const FString& InName)
{
	UnitRole = InRole;
	Team = InTeam;
	UnitName = InName;
	CountryCode = (InTeam == ERDBotTeam::Ally) ? TEXT("US") : TEXT("UK");
	bLeaderSuit = UnitName.Equals(TEXT("Trump"), ESearchCase::IgnoreCase);

	// Fable 5 unit.gd baselines (scaled to UE cm movement feel)
	switch (InRole)
	{
	case ERDUnitRole::AssaultRobot:
	case ERDUnitRole::SentryDroid:
	case ERDUnitRole::PatrolDroid:
		bIsRobot = true;
		bFemaleSuit = false;
		MaxHP = 220.f;
		MoveSpeed = 560.f;   // was 320 — felt crawl; robots still slower than humans
		ShotDamage = 34.f;
		FireInterval = 0.9f;
		WeaponRange = 2400.f;
		SightRange = 2200.f;
		GetCapsuleComponent()->InitCapsuleSize(48.f, 96.f);
		break;
	default:
		bIsRobot = false;
		bFemaleSuit = UnitName.Contains(TEXT("Haley")) || UnitName.Contains(TEXT("Truss"))
			|| UnitName.Contains(TEXT("May")) || UnitName.Contains(TEXT("Badenoch"))
			|| UnitName.Contains(TEXT("Braverman"));
		MaxHP = bLeaderSuit ? 90.f : 60.f;
		MoveSpeed = bLeaderSuit ? 820.f : 760.f; // assault pace for greybox distances
		ShotDamage = bLeaderSuit ? 22.f : 16.f;
		FireInterval = bLeaderSuit ? 0.4f : 0.5f;
		WeaponRange = 2800.f;
		SightRange = 2400.f;
		GetCapsuleComponent()->InitCapsuleSize(bFemaleSuit ? 44.f : 48.f, bFemaleSuit ? 88.f : 92.f);
		break;
	}

	bHoldPost = (InRole == ERDUnitRole::SentryDroid);
	HP = MaxHP;
	if (UCharacterMovementComponent* Mov = GetCharacterMovement())
	{
		Mov->MaxWalkSpeed = MoveSpeed;
		Mov->MaxFlySpeed = MoveSpeed;
	}
	if (bIsRobot)
	{
		Tags.Add(FName(TEXT("RD_Robot")));
	}
	BuildSuit();
	RefreshLook();
}

void ARDBot::ApplyLearningBoost(float SpeedMul, float DamageMul, float FireRateMul)
{
	MoveSpeed *= SpeedMul;
	ShotDamage *= DamageMul;
	FireInterval = FMath::Max(0.18f, FireInterval * FireRateMul);
	if (UCharacterMovementComponent* Mov = GetCharacterMovement())
	{
		Mov->MaxWalkSpeed = MoveSpeed;
		Mov->MaxFlySpeed = MoveSpeed;
	}
}

void ARDBot::AssignDoorGuard(const FVector& Standby, const FVector& BlockPoint)
{
	// Prefer keeping robot stats; just re-home for the choke
	bDoorGuard = true;
	bHoldPost = true;
	bBlockingDoorway = false;
	DoorStandbyPoint = Standby;
	DoorBlockPoint = BlockPoint;
	Home = Standby;
	HoldPoint = Standby;
	WanderTarget = Standby;
	if (!bIsRobot)
	{
		// Promote human to robot door unit if somehow assigned
		ConfigureFromRole(ERDUnitRole::SentryDroid, Team, UnitName.IsEmpty() ? TEXT("SEC-DOOR") : UnitName);
	}
	Tags.Add(FName(TEXT("RD_DoorGuard")));
	RefreshLook();
}

void ARDBot::StartAlarmRadio(const FVector& ContactPos)
{
	KnownContact = ContactPos;
	if (AlarmRadioT < 0.f)
	{
		AlarmRadioT = 1.2f; // Fable 5 enemy.gd
	}
}

void ARDBot::CommandBlockDoorway(const FVector& BlockPoint)
{
	if (bDead) return;
	bBlockingDoorway = true;
	DoorBlockPoint = BlockPoint;
	HoldPoint = BlockPoint;
	Order = ERDSquadOrder::Hold;
	if (GetWorld())
	{
		DrawDebugSphere(GetWorld(), BlockPoint, 70.f, 10, FColor::Red, false, 3.f, 0, 4.f);
	}
}

void ARDBot::SetCurrentHP(float NewHP)
{
	HP = FMath::Clamp(NewHP, 0.f, MaxHP);
}

void ARDBot::BuildSuit()
{
	// Procedural humanoid spacesuit (free engine primitives — readable from spectator cam)
	UStaticMesh* Cyl = CylinderMesh.Get() ? CylinderMesh.Get() : CubeMesh.Get();
	UStaticMesh* Sph = SphereMesh.Get() ? SphereMesh.Get() : CubeMesh.Get();
	UStaticMesh* Cub = CubeMesh.Get();
	if (!Cyl || !Sph) return;

	const float Slim = bFemaleSuit ? 0.88f : 1.f;
	const float Tall = bFemaleSuit ? 0.94f : 1.f;
	const float Bulk = bIsRobot ? 1.25f : (bLeaderSuit ? 1.12f : 1.f);

	if (Torso)
	{
		Torso->SetStaticMesh(Cyl);
		Torso->SetRelativeLocation(FVector(0.f, 0.f, -5.f * Tall));
		Torso->SetRelativeScale3D(FVector(0.58f * Slim * Bulk, 0.42f * Slim * Bulk, 0.95f * Tall * Bulk));
	}
	if (Head)
	{
		// Helmet bubble
		Head->SetStaticMesh(Sph);
		Head->SetRelativeLocation(FVector(0.f, 0.f, 72.f * Tall * Bulk));
		Head->SetRelativeScale3D(FVector(0.52f * Slim * Bulk));
	}
	if (Visor)
	{
		Visor->SetStaticMesh(Sph);
		Visor->SetRelativeLocation(FVector(18.f * Bulk, 0.f, 72.f * Tall * Bulk));
		Visor->SetRelativeScale3D(FVector(0.28f * Slim, 0.42f * Slim, 0.32f) * Bulk);
	}
	if (ShoulderL && ShoulderR)
	{
		const float Y = 38.f * Slim * Bulk;
		const float Z = 38.f * Tall;
		const float S = 0.32f * Bulk;
		ShoulderL->SetStaticMesh(Sph);
		ShoulderL->SetRelativeLocation(FVector(0.f, -Y, Z));
		ShoulderL->SetRelativeScale3D(FVector(S));
		ShoulderR->SetStaticMesh(Sph);
		ShoulderR->SetRelativeLocation(FVector(0.f, Y, Z));
		ShoulderR->SetRelativeScale3D(FVector(S));
	}
	if (ArmL && ArmR)
	{
		const float Y = 42.f * Slim * Bulk;
		ArmL->SetStaticMesh(Cyl);
		ArmL->SetRelativeLocation(FVector(4.f, -Y, 8.f));
		ArmL->SetRelativeRotation(FRotator(0.f, 0.f, 12.f));
		ArmL->SetRelativeScale3D(FVector(0.16f * Slim, 0.16f * Slim, 0.55f * Tall) * Bulk);
		ArmR->SetStaticMesh(Cyl);
		ArmR->SetRelativeLocation(FVector(4.f, Y, 8.f));
		ArmR->SetRelativeRotation(FRotator(0.f, 0.f, -12.f));
		ArmR->SetRelativeScale3D(FVector(0.16f * Slim, 0.16f * Slim, 0.55f * Tall) * Bulk);
	}
	if (LegL && LegR)
	{
		const float Y = 14.f * Slim;
		LegL->SetStaticMesh(Cyl);
		LegL->SetRelativeLocation(FVector(0.f, -Y, -55.f * Tall));
		LegL->SetRelativeScale3D(FVector(0.2f * Slim, 0.2f * Slim, 0.55f * Tall) * Bulk);
		LegR->SetStaticMesh(Cyl);
		LegR->SetRelativeLocation(FVector(0.f, Y, -55.f * Tall));
		LegR->SetRelativeScale3D(FVector(0.2f * Slim, 0.2f * Slim, 0.55f * Tall) * Bulk);
	}
	if (Backpack && Cub)
	{
		Backpack->SetStaticMesh(Cub);
		Backpack->SetRelativeLocation(FVector(-28.f * Bulk, 0.f, 10.f));
		Backpack->SetRelativeScale3D(FVector(0.28f, 0.45f * Slim, 0.55f) * Bulk);
		Backpack->SetVisibility(true);
	}
	BuildFlagBadge();
}

void ARDBot::BuildFlagBadge()
{
	// Prominent chest nation badge — readable from orbit camera
	UStaticMesh* Cub = CubeMesh.Get();
	if (!Cub || !BadgeBase) return;

	const float ChestZ = bFemaleSuit ? 22.f : 28.f;
	const float Forward = 32.f * (bIsRobot ? 1.15f : 1.f);

	// White plate
	BadgeBase->SetStaticMesh(Cub);
	BadgeBase->SetRelativeLocation(FVector(Forward, 0.f, ChestZ));
	BadgeBase->SetRelativeScale3D(FVector(0.06f, 0.55f, 0.38f)); // large chest flag
	BadgeBase->SetVisibility(true);

	if (BadgeCanton)
	{
		BadgeCanton->SetStaticMesh(Cub);
		BadgeCanton->SetVisibility(true);
	}
	if (BadgeStripe)
	{
		BadgeStripe->SetStaticMesh(Cub);
		BadgeStripe->SetVisibility(true);
	}
	if (BadgeCross)
	{
		BadgeCross->SetStaticMesh(Cub);
		BadgeCross->SetVisibility(true);
	}

	if (CountryCode.Equals(TEXT("UK"), ESearchCase::IgnoreCase))
	{
		// UK: blue field + white saltire suggestion + red cross
		if (BadgeCanton)
		{
			BadgeCanton->SetRelativeLocation(FVector(Forward + 2.f, 0.f, ChestZ));
			BadgeCanton->SetRelativeScale3D(FVector(0.05f, 0.52f, 0.35f));
		}
		if (BadgeStripe) // white horizontal
		{
			BadgeStripe->SetRelativeLocation(FVector(Forward + 3.f, 0.f, ChestZ));
			BadgeStripe->SetRelativeScale3D(FVector(0.04f, 0.5f, 0.08f));
		}
		if (BadgeCross) // red cross
		{
			BadgeCross->SetRelativeLocation(FVector(Forward + 4.f, 0.f, ChestZ));
			BadgeCross->SetRelativeScale3D(FVector(0.035f, 0.1f, 0.32f));
		}
	}
	else
	{
		// USA: blue canton + red stripe bar
		if (BadgeCanton)
		{
			BadgeCanton->SetRelativeLocation(FVector(Forward + 2.f, -12.f, ChestZ + 8.f));
			BadgeCanton->SetRelativeScale3D(FVector(0.05f, 0.22f, 0.18f));
		}
		if (BadgeStripe)
		{
			BadgeStripe->SetRelativeLocation(FVector(Forward + 3.f, 6.f, ChestZ - 4.f));
			BadgeStripe->SetRelativeScale3D(FVector(0.04f, 0.38f, 0.08f));
		}
		if (BadgeCross) // second red stripe
		{
			BadgeCross->SetRelativeLocation(FVector(Forward + 3.f, 6.f, ChestZ + 10.f));
			BadgeCross->SetRelativeScale3D(FVector(0.04f, 0.38f, 0.07f));
		}
	}
}

void ARDBot::BeginPlay()
{
	Super::BeginPlay();
	HP = MaxHP;
	Home = GetActorLocation();
	HoldPoint = Home;
	WanderTarget = Home;
	LastPos = Home;
	// FormationSlot is assigned by GameMode at spawn (unique 0..N). Do not collapse to % 3.
	PreferDetourSign = (GetFormationLaneY() >= 0.f) ? 1.f : -1.f;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = MoveSpeed;
		Move->MaxFlySpeed = MoveSpeed;
		Move->SetMovementMode(MOVE_Flying);
		// Engine RVO: agents push sideways around each other instead of stacking
		Move->bUseRVOAvoidance = true;
		Move->SetAvoidanceEnabled(true);
		Move->AvoidanceConsiderationRadius = bIsRobot ? 480.f : 400.f;
		Move->AvoidanceWeight = bIsRobot ? 0.65f : 0.45f; // robots less yielding
		// Ally group 1, defenders group 2 — both avoid each other
		const int32 Group = (Team == ERDBotTeam::Ally) ? 1 : 2;
		Move->SetAvoidanceGroup(Group);
		Move->SetGroupsToAvoid(1 | 2);
		Move->SetGroupsToIgnore(0);
	}
	BuildSuit();
	RefreshLook();
}

void ARDBot::SetFormationSlot(int32 Slot)
{
	FormationSlot = FMath::Max(0, Slot);
	PreferDetourSign = (GetFormationLaneY() >= 0.f) ? 1.f : -1.f;
}

FVector ARDBot::GetFormationOffset() const
{
	// Depth (negative X = behind point), lateral Y — meters of air between bodies
	struct FLane { float Back; float Side; };
	static const FLane Table[] = {
		{ 0.f, 0.f },         // 0 point / breach
		{ 280.f, -700.f },    // 1 left wing
		{ 280.f, 700.f },     // 2 right wing
		{ 160.f, -1400.f },   // 3 far left flank
		{ 160.f, 1400.f },    // 4 far right flank
		{ 480.f, -380.f },    // 5 rear left
		{ 480.f, 380.f },     // 6 rear right
		{ 620.f, 0.f },       // 7 trail center
	};
	const int32 i = FMath::Clamp(FormationSlot, 0, 7);
	return FVector(-Table[i].Back, Table[i].Side, 0.f);
}

float ARDBot::GetFormationLaneY() const
{
	return GetFormationOffset().Y;
}

FVector ARDBot::GetChokeStagingPoint(const FVector& ChokeLoc, float BackDist) const
{
	const FVector Off = GetFormationOffset();
	// Hold west of choke on own lateral lane — never all at same XY
	return FVector(ChokeLoc.X - BackDist + Off.X * 0.35f, ChokeLoc.Y + Off.Y, 120.f);
}

void ARDBot::BashNearestObstacle()
{
	if (!GetWorld() || FireCD > 0.1f) return;
	AActor* Best = nullptr;
	float BestD = 280.f;
	// Prefer room door / wreck / destructible in face
	if (AActor* D = FindNearestRoomDoor())
	{
		const float Dist = FVector::Dist(GetActorLocation(), D->GetActorLocation());
		if (Dist < BestD) { BestD = Dist; Best = D; }
	}
	if (AActor* P = FindNearestBreachable())
	{
		const float Dist = FVector::Dist(GetActorLocation(), P->GetActorLocation());
		if (Dist < BestD) { BestD = Dist; Best = P; }
	}
	TArray<AActor*> Props;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDDestructible::StaticClass(), Props);
	for (AActor* A : Props)
	{
		if (!A) continue;
		const float Dist = FVector::Dist(GetActorLocation(), A->GetActorLocation());
		if (Dist < BestD) { BestD = Dist; Best = A; }
	}
	if (Best)
	{
		TryBreachTarget(Best, 2.6f);
	}
}

FVector ARDBot::FindBestSteerDir(const FVector& DesiredDir, float LookAhead) const
{
	UWorld* World = GetWorld();
	if (!World || DesiredDir.IsNearlyZero()) return DesiredDir;

	const float Rad = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.78f : 36.f;
	const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.5f : 48.f;
	const FVector Origin = GetActorLocation();
	FVector Base = DesiredDir.GetSafeNormal2D();
	if (Base.IsNearlyZero()) Base = GetActorForwardVector().GetSafeNormal2D();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RDSteer), false, this);
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(World, ARDBot::StaticClass(), Bots);
	for (AActor* ABot : Bots)
	{
		if (ABot && ABot != this) Params.AddIgnoredActor(ABot);
	}
	const FCollisionShape Cap = FCollisionShape::MakeCapsule(Rad, Half);

	// Prefer goal angle, then sticky detour side, then opposite
	const float Prefer = PreferDetourSign;
	const float Angles[] = {
		0.f, 22.f * Prefer, -22.f * Prefer, 45.f * Prefer, -45.f * Prefer,
		70.f * Prefer, -70.f * Prefer, 100.f * Prefer, -100.f * Prefer,
		135.f * Prefer, -135.f * Prefer, 180.f
	};

	FVector BestDir = Base;
	float BestScore = -1.e9f;
	for (float Deg : Angles)
	{
		const float RadA = FMath::DegreesToRadians(Deg);
		const FVector Dir(
			Base.X * FMath::Cos(RadA) - Base.Y * FMath::Sin(RadA),
			Base.X * FMath::Sin(RadA) + Base.Y * FMath::Cos(RadA),
			0.f);
		const FVector End = Origin + Dir * LookAhead;
		FHitResult Hit;
		const bool bHit = World->SweepSingleByChannel(Hit, Origin, End, FQuat::Identity, ECC_Pawn, Cap, Params);
		const float Free = (!bHit || !Hit.bBlockingHit) ? LookAhead : Hit.Distance;
		if (Free < Rad * 1.1f) continue;
		const float Align = FVector::DotProduct(Dir, Base);
		const float Score = Free * 1.6f + Align * 140.f - FMath::Abs(Deg) * 0.35f;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestDir = Dir;
		}
	}
	return BestDir.GetSafeNormal2D();
}

void ARDBot::TickStuckRecovery(float DeltaSeconds)
{
	// Port of Fable 5 unit.gd stuck / unstick
	// Sentries / door guards *should* stand still — not "stuck"
	if (bHoldPost || bDoorGuard || bBlockingDoorway || Order == ERDSquadOrder::Hold)
	{
		StuckT = 0.f;
		LastPos = GetActorLocation();
		return;
	}

	const FVector Now = GetActorLocation();
	const float Moved = FVector::Dist2D(Now, LastPos);
	if (Moved < 6.f)
	{
		StuckT += DeltaSeconds;
	}
	else
	{
		StuckT = FMath::Max(0.f, StuckT - DeltaSeconds * 1.5f);
	}
	LastPos = Now;

	if (UnstickT > 0.f)
	{
		UnstickT -= DeltaSeconds;
		// Gentle slide via CMC so RVO still runs (no teleport hop)
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->RequestDirectMove(UnstickDir * MoveSpeed * 0.9f, false);
		}
		else
		{
			FVector Nudge = Now + UnstickDir * MoveSpeed * 0.85f * DeltaSeconds;
			Nudge.Z = FMath::Clamp(Now.Z, 100.f, 160.f);
			FHitResult H;
			SetActorLocation(Nudge, true, &H);
		}
		return;
	}

	// Patient snag detect — less thrash; still recover without leaping
	if (StuckT > 0.85f)
	{
		++StuckEvents;
		StuckT = 0.35f; // partial decay so we don't re-trigger every frame
		UnstickT = 0.4f;
		PreferDetourSign *= -1.f;
		PathWaypoints.Reset();
		PathIndex = 0;
		PathRebuildT = 0.2f;
		PathFailStreak = 0;

		BashNearestObstacle();

		const FVector GoalHint = PathGoal.IsNearlyZero()
			? GetActorForwardVector().GetSafeNormal2D()
			: (PathGoal - Now).GetSafeNormal2D();
		FVector BestDir = FindBestSteerDir(GoalHint.IsNearlyZero() ? GetActorForwardVector() : GoalHint, 280.f);
		if (BestDir.IsNearlyZero())
		{
			BestDir = FVector(PreferDetourSign, 0.f, 0.f).GetSafeNormal2D();
		}
		UnstickDir = BestDir;
		// Tiny lateral ease (≤50cm) — not a 1.8m hop
		FVector Ease = Now + BestDir * 45.f;
		Ease.Z = FMath::Clamp(Now.Z, 100.f, 160.f);
		if (IsLocationClear(Ease))
		{
			FHitResult Hit;
			SetActorLocation(Ease, true, &Hit);
		}
		else if (!IsLocationClear(Now))
		{
			ResolveWallPenetration();
		}
		UE_LOG(LogTemp, Verbose, TEXT("[RebelstarDelta] Unstick %s (event %d) smooth"), *UnitName, StuckEvents);
	}
}

void ARDBot::RefreshLook()
{
	auto Paint = [&](UStaticMeshComponent* M, const FLinearColor& C, bool bUnlit)
	{
		if (!M) return;
		UMaterialInterface* P = bUnlit ? UnlitMat.Get() : LitMat.Get();
		if (!P) return;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(P, this);
		if (!MID) return;
		MID->SetVectorParameterValue(TEXT("Color"), C);
		MID->SetVectorParameterValue(TEXT("BaseColor"), C);
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), bUnlit ? C : FLinearColor(0.f, 0.f, 0.f));
		M->SetMaterial(0, MID);
	};

	if (bDead)
	{
		const FLinearColor Dead(0.12f, 0.12f, 0.12f);
		for (UStaticMeshComponent* M : { Torso.Get(), Head.Get(), Visor.Get(), ShoulderL.Get(), ShoulderR.Get(),
			ArmL.Get(), ArmR.Get(), LegL.Get(), LegR.Get(), Backpack.Get(),
			BadgeBase.Get(), BadgeCanton.Get(), BadgeStripe.Get(), BadgeCross.Get() })
		{
			Paint(M, Dead, false);
		}
		if (Halo) Halo->SetIntensity(0.f);
		return;
	}

	const float Flash = HitFlash > 0.f ? 0.35f : 0.f;
	const bool bUS = CountryCode.Equals(TEXT("US"), ESearchCase::IgnoreCase);

	// Spacesuit body: white/grey EVA suit with team accent
	FLinearColor Suit(0.82f + Flash, 0.84f + Flash, 0.88f);
	FLinearColor Helmet(0.75f, 0.78f, 0.82f);
	FLinearColor VisorCol(0.15f, 0.55f, 0.95f); // gold-blue reflective
	FLinearColor Accent = bUS ? FLinearColor(0.15f, 0.25f, 0.75f) : FLinearColor(0.05f, 0.12f, 0.45f);
	FLinearColor Pack(0.35f, 0.38f, 0.42f);

	if (bLeaderSuit)
	{
		Suit = FLinearColor(0.95f + Flash * 0.05f, 0.92f, 0.85f); // cream VIP suit
		Accent = FLinearColor(0.95f, 0.75f, 0.15f); // gold
		VisorCol = FLinearColor(1.f, 0.85f, 0.2f);
		Pack = FLinearColor(0.25f, 0.22f, 0.18f);
	}
	else if (bIsRobot)
	{
		Suit = bUS
			? FLinearColor(0.55f + Flash, 0.62f, 0.7f)  // US power armor
			: FLinearColor(0.45f + Flash, 0.42f, 0.4f); // UK plate
		Helmet = Suit * 1.05f;
		VisorCol = bUS ? FLinearColor(0.3f, 0.9f, 1.f) : FLinearColor(1.f, 0.45f, 0.15f);
		Accent = VisorCol;
	}
	else if (!bUS)
	{
		// UK ops: subtle burgundy joint accents on white suit
		Accent = FLinearColor(0.55f, 0.08f, 0.12f);
		VisorCol = FLinearColor(0.2f, 0.35f, 0.75f);
	}

	Paint(Torso, Suit, false);
	Paint(Head, Helmet, false);
	Paint(Visor, VisorCol, true);
	Paint(ShoulderL, Accent, false);
	Paint(ShoulderR, Accent, false);
	Paint(ArmL, Suit * 0.95f, false);
	Paint(ArmR, Suit * 0.95f, false);
	Paint(LegL, Suit * 0.9f, false);
	Paint(LegR, Suit * 0.9f, false);
	Paint(Backpack, Pack, false);

	// Flag badge — always bright / unlit so it pops from above
	if (bUS)
	{
		Paint(BadgeBase, FLinearColor(1.f, 1.f, 1.f), true);
		Paint(BadgeCanton, FLinearColor(0.05f, 0.15f, 0.65f), true); // blue canton
		Paint(BadgeStripe, FLinearColor(0.85f, 0.1f, 0.12f), true);  // red stripe
		Paint(BadgeCross, FLinearColor(0.85f, 0.1f, 0.12f), true);
	}
	else
	{
		Paint(BadgeBase, FLinearColor(1.f, 1.f, 1.f), true);
		Paint(BadgeCanton, FLinearColor(0.02f, 0.1f, 0.45f), true); // Union blue
		Paint(BadgeStripe, FLinearColor(1.f, 1.f, 1.f), true);
		Paint(BadgeCross, FLinearColor(0.85f, 0.05f, 0.1f), true);  // St George
	}

	if (Halo)
	{
		Halo->SetLightColor(bLeaderSuit ? FLinearColor(1.f, 0.85f, 0.3f)
			: (bUS ? FLinearColor(0.3f, 0.5f, 1.f) : FLinearColor(0.7f, 0.15f, 0.2f)));
		Halo->SetIntensity(bLeaderSuit ? 32000.f : (bBlockingDoorway ? 38000.f : 18000.f));
	}
}

void ARDBot::ApplyDamage(float Amount)
{
	if (bDead || Amount <= 0.f) return;
	HP -= Amount;
	HitFlash = 0.2f;
	RefreshLook();

	// Big hit marker in world
	if (GetWorld())
	{
		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 140),
			FString::Printf(TEXT("%.0f"), HP), nullptr, FColor::Yellow, 0.6f, true, 1.4f);
	}

	if (Team == ERDBotTeam::Defender)
	{
		if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GM->RaiseAlarm(GetActorLocation());
		}
	}
	if (HP <= 0.f)
	{
		Die();
	}
}

void ARDBot::SpawnRobotWreckage()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Door guard: special choke wreckage via GameMode
	if (bDoorGuard)
	{
		if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GM->NotifyDoorGuardKilled(this);
		}
		return;
	}

	// Fable 5: every robot leaves a hulk that blocks movement + shots
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Loc = GetActorLocation() + FVector(0.f, 0.f, -40.f);
	if (ARDWreckage* W = World->SpawnActor<ARDWreckage>(Loc, GetActorRotation(), P))
	{
		W->ConfigureAsRobotHulk(); // 2D-style solid pile
		W->Tags.Add(FName(TEXT("RD_Base")));
	}
}

void ARDBot::Die()
{
	bDead = true;
	RefreshLook();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (GetWorld())
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), 80.f, 12, FColor::Orange, false, 1.5f, 0, 4.f);
	}
	if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->NotifyBotKilled(this);
	}

	// 2D rule: robots → wreckage; humans → just drop
	if (bIsRobot)
	{
		SpawnRobotWreckage();
		SetLifeSpan(1.5f);
		return;
	}
	SetLifeSpan(8.f);
}

bool ARDBot::IsLocationClear(const FVector& TestLoc) const
{
	UWorld* World = GetWorld();
	if (!World) return true;
	const float Rad = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.9f : 45.f;
	const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.85f : 90.f;
	const FVector Start = TestLoc + FVector(0.f, 0.f, Half);
	// If short rays from body hit solids immediately in most directions → inside a wall
	int32 Hits = 0;
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = float(i) * 45.f;
		const FVector Dir(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0.f);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RDClear), false, this);
		if (World->SweepSingleByChannel(Hit, Start, Start + Dir * (Rad + 12.f), FQuat::Identity,
			ECC_Visibility, FCollisionShape::MakeSphere(8.f), Params))
		{
			if (Hit.bBlockingHit && Hit.Distance < Rad * 0.55f) ++Hits;
		}
	}
	return Hits < 5;
}

void ARDBot::ResolveWallPenetration()
{
	if (bDead || !GetWorld() || !GetCapsuleComponent()) return;
	const FVector Loc = GetActorLocation();
	if (IsLocationClear(Loc)) return;

	const float Rad = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RDUnclip), false, this);
	FCollisionShape Cap = FCollisionShape::MakeCapsule(Rad * 0.92f, Half * 0.92f);

	// Find nearest free capsule pose (query only — do not thrash actor)
	float BestDist = 1.e9f;
	FVector Best = Loc;
	bool bFound = false;
	for (int32 i = 0; i < 16; ++i)
	{
		const float A = float(i) * 22.5f;
		const FVector Dir(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0.f);
		for (float Dist = 50.f; Dist <= 280.f; Dist += 45.f)
		{
			FVector Candidate = Loc + Dir * Dist;
			Candidate.Z = FMath::Clamp(Loc.Z, 100.f, 160.f);
			FHitResult Hit;
			const bool bBlocked = GetWorld()->OverlapAnyTestByChannel(
				Candidate + FVector(0.f, 0.f, Half * 0.1f), FQuat::Identity, ECC_Visibility, Cap, Params);
			if (!bBlocked && IsLocationClear(Candidate))
			{
				if (Dist < BestDist)
				{
					BestDist = Dist;
					Best = Candidate;
					bFound = true;
				}
				break;
			}
		}
	}
	if (bFound)
	{
		// Teleport only after verifying free, then settle with sweep
		SetActorLocation(Best, false, nullptr, ETeleportType::TeleportPhysics);
		FHitResult Hit;
		SetActorLocation(Best + FVector(8.f, 0.f, 0.f), true, &Hit);
		UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Unclip %s → free (%.0f cm)"), *UnitName, BestDist);
	}
}

FVector ARDBot::ComputeSeparation() const
{
	FVector Push = FVector::ZeroVector;
	// Hard bubble (collision trap) + soft tactical bubble (keep lanes open)
	const float HardR = bIsRobot ? 260.f : 220.f;
	const float SoftR = bIsRobot ? 420.f : 380.f;
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	for (AActor* A : Bots)
	{
		const ARDBot* O = Cast<ARDBot>(A);
		if (!O || O == this || O->IsDead() || O->Team != Team) continue;
		FVector D = GetActorLocation() - O->GetActorLocation();
		D.Z = 0.f;
		const float L = D.Size();
		if (L < 1.f || L > SoftR) continue;
		const FVector N = D / L;
		if (L < HardR)
		{
			// Strong inverse push inside hard bubble
			const float T = (HardR - L) / HardR;
			Push += N * (HardR - L) * (1.4f + T * 2.2f);
		}
		else
		{
			// Mild soft spacing so the squad does not re-clump after unstacking
			const float T = (SoftR - L) / (SoftR - HardR);
			Push += N * T * 55.f;
		}
		// Bias push along formation lane (prefer own Y over same-X stack)
		const float MyLane = GetFormationLaneY();
		const float TheirLane = O->GetFormationLaneY();
		if (FMath::Abs(MyLane - TheirLane) > 80.f)
		{
			const float LanePush = FMath::Sign(MyLane - TheirLane);
			Push.Y += LanePush * 40.f;
		}
	}
	return Push;
}

bool ARDBot::ShouldYieldAtChoke(const FVector& ChokeLoc, float Radius) const
{
	const float MyD = FVector::Dist2D(GetActorLocation(), ChokeLoc);
	if (MyD > Radius + 120.f) return false;
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	int32 Closer = 0;
	int32 InBubble = 0;
	for (AActor* A : Bots)
	{
		const ARDBot* O = Cast<ARDBot>(A);
		if (!O || O == this || O->IsDead() || O->Team != Team) continue;
		const float OD = FVector::Dist2D(O->GetActorLocation(), ChokeLoc);
		if (OD < Radius) ++InBubble;
		if (OD + 40.f < MyD && OD < Radius)
		{
			++Closer;
			// Humans always yield to robots at the door
			if (!bIsRobot && O->IsRobot()) return true;
			// Lower slot (point/breach) has right-of-way
			if (FormationSlot > O->GetFormationSlot() && O->GetFormationSlot() <= 1) return true;
		}
	}
	// One body in the choke mouth; second only if robot trail
	const int32 MaxInChoke = 1;
	if (InBubble >= 2 && MyD < Radius * 0.85f) return true;
	return Closer >= MaxInChoke;
}

int32 ARDBot::CountAlliesNear(const FVector& Loc, float Radius) const
{
	int32 N = 0;
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	for (AActor* A : Bots)
	{
		const ARDBot* O = Cast<ARDBot>(A);
		if (!O || O->IsDead() || O->Team != Team) continue;
		if (FVector::Dist2D(O->GetActorLocation(), Loc) < Radius) ++N;
	}
	return N;
}

bool ARDBot::HasWalkableLOS(const FVector& From, const FVector& To) const
{
	UWorld* World = GetWorld();
	if (!World) return true;

	const float Rad = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.72f : 40.f;
	const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.55f : 55.f;
	const float Z = FMath::Clamp(From.Z, 110.f, 150.f);

	FVector A = From;
	FVector B = To;
	A.Z = Z;
	B.Z = Z;

	const float Dist2D = FVector::Dist2D(A, B);
	if (Dist2D < 25.f) return true;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RDWalkLOS), false, this);
	Params.bTraceComplex = false;
	// Ignore living pawns so pathing routes through allies (separation handles crowd)
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(World, ARDBot::StaticClass(), Bots);
	for (AActor* ABot : Bots)
	{
		if (ABot && ABot != this) Params.AddIgnoredActor(ABot);
	}
	if (APawn* P = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		Params.AddIgnoredActor(P);
	}

	FHitResult Hit;
	const FCollisionShape Cap = FCollisionShape::MakeCapsule(Rad, Half);
	const bool bHit = World->SweepSingleByChannel(Hit, A, B, FQuat::Identity, ECC_Visibility, Cap, Params);
	if (!bHit || !Hit.bBlockingHit) return true;
	// Allow very grazing hits near destination (door frames / tip of capsule)
	return Hit.Distance > Dist2D * 0.90f;
}

void ARDBot::CollectPathLandmarks(TArray<FVector>& Out) const
{
	Out.Reset();
	const float Z = 120.f;

	// Fixed moonbase graph (MapOrigin 4000,-9900 CS=300 — airlock P-row)
	// Outer approach / LZ chain
	Out.Add(FVector(-9000.f, -5850.f, Z)); // Approach LZ
	Out.Add(FVector(-4000.f, -5850.f, Z)); // Mid regolith
	Out.Add(FVector(1000.f, -5850.f, Z));
	Out.Add(FVector(3500.f, -5850.f, Z)); // Outer airlock mouth
	Out.Add(FVector(4300.f, -5850.f, Z)); // Airlock tunnel outer
	Out.Add(FVector(5300.f, -5850.f, Z)); // Inner airlock / P-row
	Out.Add(FVector(5600.f, -5850.f, Z)); // Just inside base

	// North / south flanks into base
	Out.Add(FVector(3500.f, -7600.f, Z));
	Out.Add(FVector(5400.f, -7600.f, Z));
	Out.Add(FVector(6200.f, -7600.f, Z));
	Out.Add(FVector(3500.f, -3800.f, Z));
	Out.Add(FVector(5400.f, -3800.f, Z));
	Out.Add(FVector(6200.f, -3800.f, Z));

	// Interior spine + plaza + ISAAC approach
	Out.Add(FVector(7000.f, -5850.f, Z));
	Out.Add(FVector(8500.f, -5850.f, Z));
	Out.Add(FVector(10000.f, -5850.f, Z));
	Out.Add(FVector(8500.f, -6750.f, Z));
	Out.Add(FVector(10000.f, -6750.f, Z));
	Out.Add(FVector(11500.f, -6750.f, Z)); // ISAAC corridor
	Out.Add(FVector(12000.f, -6750.f, Z)); // ISAAC door zone
	Out.Add(FVector(12500.f, -5850.f, Z));
	Out.Add(FVector(12500.f, -7600.f, Z));
	Out.Add(FVector(7000.f, -4500.f, Z)); // south interior
	Out.Add(FVector(7000.f, -7200.f, Z)); // north interior
	Out.Add(FVector(9000.f, -4500.f, Z));
	Out.Add(FVector(9000.f, -7200.f, Z));

	// Dynamic: door / ISAAC from game mode
	if (const ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		const FVector Door = GM->GetIsaacDoorBlockPoint();
		if (!Door.IsNearlyZero())
		{
			Out.Add(Door);
			Out.Add(Door + FVector(-400.f, 0.f, 0.f));
			Out.Add(Door + FVector(-400.f, 250.f, 0.f));
			Out.Add(Door + FVector(-400.f, -250.f, 0.f));
		}
		const FVector Way = GM->GetApproachWaypoint(FormationSlot);
		Out.Add(Way);
		Out.Add(Way + FVector(-800.f, 0.f, 0.f));
	}

	// Soft Z clamp
	for (FVector& L : Out)
	{
		L.Z = Z;
	}
}

bool ARDBot::TryAppendDetour(const FVector& A, const FVector& B, TArray<FVector>& OutPath) const
{
	FVector Delta = B - A;
	Delta.Z = 0.f;
	const float Len = Delta.Size();
	if (Len < 80.f) return false;

	const FVector Dir = Delta / Len;
	const FVector Side(-Dir.Y, Dir.X, 0.f);
	const FVector Mid = (A + B) * 0.5f;

	// Try preferred side first, then opposite, at several offsets
	const float Offsets[] = { 220.f, 380.f, 560.f, 780.f, 1100.f };
	const float Signs[] = { PreferDetourSign, -PreferDetourSign };

	for (float Sign : Signs)
	{
		for (float Off : Offsets)
		{
			const FVector Corner = Mid + Side * Sign * Off;
			FVector C = Corner;
			C.Z = 120.f;
			// Also try two-point detours along the blocked segment
			const FVector Q1 = A + Dir * (Len * 0.33f) + Side * Sign * Off;
			const FVector Q2 = A + Dir * (Len * 0.66f) + Side * Sign * Off;
			FVector P1 = Q1; P1.Z = 120.f;
			FVector P2 = Q2; P2.Z = 120.f;

			if (HasWalkableLOS(A, C) && HasWalkableLOS(C, B))
			{
				OutPath.Add(C);
				return true;
			}
			if (HasWalkableLOS(A, P1) && HasWalkableLOS(P1, P2) && HasWalkableLOS(P2, B))
			{
				OutPath.Add(P1);
				OutPath.Add(P2);
				return true;
			}
			if (HasWalkableLOS(A, P1) && HasWalkableLOS(P1, B))
			{
				OutPath.Add(P1);
				return true;
			}
		}
	}
	return false;
}

void ARDBot::RebuildPathTo(const FVector& Goal)
{
	PathWaypoints.Reset();
	PathIndex = 0;
	PathGoal = Goal;
	PathGoal.Z = 120.f;

	const FVector Start = GetActorLocation();
	FVector End = Goal;
	End.Z = 120.f;

	const float GoalDist = FVector::Dist2D(Start, End);
	if (GoalDist < 90.f)
	{
		return;
	}

	// Direct walkable → single hop
	if (HasWalkableLOS(Start, End))
	{
		PathWaypoints.Add(End);
		PathFailStreak = 0;
		return;
	}

	// === Off-the-shelf: Unreal NavigationSystem / Recast path (if NavMesh built) ===
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		const FVector NavStart(Start.X, Start.Y, 120.f);
		const FVector NavEnd(End.X, End.Y, 120.f);
		if (UNavigationPath* NavPath = NavSys->FindPathToLocationSynchronously(GetWorld(), NavStart, NavEnd, this))
		{
			if (NavPath->IsValid() && NavPath->PathPoints.Num() >= 2)
			{
				for (int32 i = 1; i < NavPath->PathPoints.Num(); ++i)
				{
					FVector P = NavPath->PathPoints[i];
					P.Z = 120.f;
					if (PathWaypoints.Num() == 0 || FVector::Dist2D(PathWaypoints.Last(), P) > 80.f)
					{
						PathWaypoints.Add(P);
					}
				}
				if (PathWaypoints.Num() > 0)
				{
					PathIndex = 0;
					PathFailStreak = 0;
					PreferDetourSign = (FormationSlot % 2 == 0) ? 1.f : -1.f;
					return; // engine path wins over hand landmarks
				}
			}
		}
	}

	// === Fallback: authored moonbase landmarks + geometric detours ===
	TArray<FVector> Marks;
	CollectPathLandmarks(Marks);

	// Greedy landmark chain: repeatedly pick nearest landmark that advances toward goal with walkable LOS
	FVector Cursor = Start;
	TArray<FVector> Chain;
	const int32 MaxHops = 10;
	TSet<int32> Used;

	for (int32 Hop = 0; Hop < MaxHops; ++Hop)
	{
		if (HasWalkableLOS(Cursor, End))
		{
			Chain.Add(End);
			break;
		}

		int32 BestIdx = INDEX_NONE;
		float BestScore = -1.e9f;
		for (int32 i = 0; i < Marks.Num(); ++i)
		{
			if (Used.Contains(i)) continue;
			const FVector& M = Marks[i];
			if (FVector::Dist2D(Cursor, M) < 120.f) continue;
			if (FVector::Dist2D(M, End) > FVector::Dist2D(Cursor, End) + 400.f) continue; // must roughly advance
			if (!HasWalkableLOS(Cursor, M)) continue;

			// Score: progress toward goal minus path length cost
			const float Progress = FVector::Dist2D(Cursor, End) - FVector::Dist2D(M, End);
			const float Cost = FVector::Dist2D(Cursor, M) * 0.15f;
			const float Score = Progress * 2.2f - Cost;
			if (Score > BestScore)
			{
				BestScore = Score;
				BestIdx = i;
			}
		}

		if (BestIdx == INDEX_NONE)
		{
			// No landmark LOS — try geometric detour toward goal, then bail to partial
			TArray<FVector> Det;
			if (TryAppendDetour(Cursor, End, Det))
			{
				for (const FVector& D : Det)
				{
					Chain.Add(D);
					Cursor = D;
				}
				if (HasWalkableLOS(Cursor, End))
				{
					Chain.Add(End);
				}
				else
				{
					// leave partial path; MoveToward will repath soon
					Chain.Add(End);
				}
			}
			else
			{
				// Last resort: push a side offset waypoint and the goal (local slide still helps)
				const FVector Dir = (End - Cursor).GetSafeNormal2D();
				const FVector Side(-Dir.Y, Dir.X, 0.f);
				const FVector Esc = Cursor + Side * PreferDetourSign * 420.f + Dir * 200.f;
				Chain.Add(FVector(Esc.X, Esc.Y, 120.f));
				Chain.Add(End);
			}
			break;
		}

		Used.Add(BestIdx);
		const FVector Next = Marks[BestIdx];
		// If direct landmark hop blocked somehow (shouldn't), insert detour
		if (!HasWalkableLOS(Cursor, Next))
		{
			TArray<FVector> Det;
			if (TryAppendDetour(Cursor, Next, Det))
			{
				for (const FVector& D : Det) Chain.Add(D);
			}
		}
		Chain.Add(Next);
		Cursor = Next;
	}

	// Ensure final goal present
	if (Chain.Num() == 0 || FVector::Dist2D(Chain.Last(), End) > 50.f)
	{
		// Try detour from last chain point (or start) to end
		const FVector From = Chain.Num() ? Chain.Last() : Start;
		if (!HasWalkableLOS(From, End))
		{
			TArray<FVector> Det;
			if (TryAppendDetour(From, End, Det))
			{
				for (const FVector& D : Det) Chain.Add(D);
			}
		}
		Chain.Add(End);
	}

	// Compact collinear-ish points
	PathWaypoints.Reserve(Chain.Num());
	for (int32 i = 0; i < Chain.Num(); ++i)
	{
		FVector P = Chain[i];
		P.Z = 120.f;
		if (PathWaypoints.Num() > 0 && FVector::Dist2D(PathWaypoints.Last(), P) < 90.f) continue;
		// Skip if we can already see past this point to the next
		if (PathWaypoints.Num() > 0 && i + 1 < Chain.Num())
		{
			const FVector NextP = Chain[i + 1];
			if (HasWalkableLOS(PathWaypoints.Last(), NextP))
			{
				continue; // skip redundant mid
			}
		}
		PathWaypoints.Add(P);
	}
	if (PathWaypoints.Num() == 0)
	{
		PathWaypoints.Add(End);
	}

	PathIndex = 0;
	PathFailStreak = 0;
	// Alternate preferred detour side each rebuild so packs split around walls
	PreferDetourSign = (FormationSlot % 2 == 0) ? 1.f : -1.f;
	if ((StuckEvents + GetUniqueID()) % 3 == 0) PreferDetourSign *= -1.f;
}

FVector ARDBot::GetPathFollowPoint(const FVector& Goal) const
{
	if (PathWaypoints.Num() == 0 || PathIndex >= PathWaypoints.Num())
	{
		return Goal;
	}
	return PathWaypoints[PathIndex];
}

void ARDBot::MoveToward(const FVector& WorldTarget, float DeltaSeconds, float SpeedScale)
{
	FVector Goal = WorldTarget;
	Goal.Z = FMath::Clamp(GetActorLocation().Z, 100.f, 160.f);

	// --- Path management ---
	PathRebuildT -= DeltaSeconds;
	const float GoalShift = FVector::Dist2D(Goal, PathGoal);
	const bool bNeedPath =
		PathWaypoints.Num() == 0
		|| PathIndex >= PathWaypoints.Num()
		|| GoalShift > 280.f
		|| PathRebuildT <= 0.f
		|| PathFailStreak >= 3;

	if (bNeedPath)
	{
		RebuildPathTo(Goal);
		// Longer sticky paths — less repath thrash / direction flips
		PathRebuildT = FMath::FRandRange(0.9f, 1.5f);
	}

	// Advance along waypoints when close
	while (PathIndex < PathWaypoints.Num())
	{
		const float DWp = FVector::Dist2D(GetActorLocation(), PathWaypoints[PathIndex]);
		if (DWp < 110.f)
		{
			++PathIndex;
			continue;
		}
		// Skip waypoint if we already have walkable LOS to a later one (shortcut)
		bool bSkipped = false;
		for (int32 Look = PathWaypoints.Num() - 1; Look > PathIndex; --Look)
		{
			if (HasWalkableLOS(GetActorLocation(), PathWaypoints[Look]))
			{
				PathIndex = Look;
				bSkipped = true;
				break;
			}
		}
		if (bSkipped) continue;
		break;
	}

	FVector StepTarget = GetPathFollowPoint(Goal);
	// If current segment blocked, force rebuild with opposite detour
	if (PathIndex < PathWaypoints.Num() && !HasWalkableLOS(GetActorLocation(), StepTarget))
	{
		++PathFailStreak;
		if (PathFailStreak >= 2)
		{
			PreferDetourSign *= -1.f;
			RebuildPathTo(Goal);
			PathRebuildT = 0.55f;
			StepTarget = GetPathFollowPoint(Goal);
		}
	}

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	FVector To = StepTarget - GetActorLocation();
	To.Z = 0.f;
	const float Dist = To.Size();

	// Mild crowd ease only — heavy 0.42× made whole squad crawl
	const int32 Crowd = CountAlliesNear(GetActorLocation(), 280.f);
	float CrowdSlow = 1.f;
	if (Crowd >= 5) CrowdSlow = 0.78f;
	else if (Crowd >= 3) CrowdSlow = 0.88f;

	// Assault pace: base speeds already raised; SpeedScale from tactics still applies
	const float Spd = MoveSpeed * SpeedScale * CrowdSlow;
	if (MoveComp)
	{
		MoveComp->MaxWalkSpeed = Spd;
		MoveComp->MaxFlySpeed = Spd;
		MoveComp->MaxAcceleration = 14000.f;
		if (MoveComp->MovementMode != MOVE_Flying)
		{
			MoveComp->SetMovementMode(MOVE_Flying);
		}
	}

	if (Dist < 70.f)
	{
		const FVector Sep = ComputeSeparation();
		if (Sep.Size() > 8.f)
		{
			FVector Nudge = GetActorLocation() + Sep.GetSafeNormal() * Spd * 0.7f * DeltaSeconds;
			Nudge.Z = FMath::Clamp(GetActorLocation().Z, 100.f, 160.f);
			FHitResult H;
			SetActorLocation(Nudge, true, &H);
			if (MoveComp) MoveComp->RequestDirectMove(Sep.GetSafeNormal() * Spd * 0.5f, true);
		}
		else if (MoveComp)
		{
			MoveComp->RequestDirectMove(FVector::ZeroVector, false);
		}
		return;
	}

	FVector WantDir = To / Dist;
	WantDir = FindBestSteerDir(WantDir, FMath::Clamp(Spd * 0.25f + 140.f, 140.f, 320.f));

	const FVector Sep = ComputeSeparation();
	if (Sep.Size() > 1.f)
	{
		const float SepW = FMath::Clamp(Sep.Size() / 160.f, 0.f, 0.85f);
		WantDir = (WantDir + Sep.GetSafeNormal() * SepW * 0.75f).GetSafeNormal();
	}

	if (!bSmoothMoveInit || SmoothMoveDir.IsNearlyZero())
	{
		SmoothMoveDir = WantDir;
		bSmoothMoveInit = true;
	}
	else
	{
		// Faster heading catch-up (5.5 felt sticky / slow to commit)
		SmoothMoveDir = FMath::VInterpTo(SmoothMoveDir, WantDir, DeltaSeconds, 11.f).GetSafeNormal2D();
		if (SmoothMoveDir.IsNearlyZero()) SmoothMoveDir = WantDir;
	}
	const FVector Dir = SmoothMoveDir;

	// Primary: reliable swept step (CMC-only fly was under-moving without AIController)
	const float Step = Spd * DeltaSeconds;
	FVector Desired = GetActorLocation() + Dir * FMath::Min(Step, Dist);
	Desired.Z = FMath::Clamp(GetActorLocation().Z, 110.f, 155.f);
	FHitResult Hit;
	SetActorLocation(Desired, true, &Hit);
	if (Hit.bBlockingHit)
	{
		++PathFailStreak;
		const FVector Slide = FVector::VectorPlaneProject(Dir, Hit.Normal).GetSafeNormal2D();
		if (!Slide.IsNearlyZero())
		{
			FVector SlidePos = GetActorLocation() + Slide * Step * 0.95f;
			SlidePos.Z = Desired.Z;
			SetActorLocation(SlidePos, true);
			SmoothMoveDir = FMath::VInterpTo(SmoothMoveDir, Slide, DeltaSeconds, 10.f).GetSafeNormal2D();
		}
		if (AActor* Blocker = Hit.GetActor())
		{
			if (Cast<ARDDestructible>(Blocker) || Cast<ARDRoomDoor>(Blocker) || Cast<ARDWreckage>(Blocker)
				|| Blocker->ActorHasTag(FName(TEXT("RD_Breach"))))
			{
				TryBreachTarget(Blocker, 2.6f);
			}
		}
	}
	else if (PathFailStreak > 0)
	{
		--PathFailStreak;
	}

	// Secondary: feed RVO so engine still de-stacks pawns
	if (MoveComp)
	{
		MoveComp->RequestDirectMove(Dir * Spd, true);
		MoveComp->Velocity = FVector(Dir.X * Spd, Dir.Y * Spd, 0.f);
	}

	if (!IsLocationClear(GetActorLocation()))
	{
		ResolveWallPenetration();
		PathFailStreak += 1;
		PathRebuildT = FMath::Min(PathRebuildT, 0.25f);
	}
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Dir.Rotation(), DeltaSeconds, 7.f));
	}
}

AActor* ARDBot::FindNearestEnemy() const
{
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	AActor* Best = nullptr;
	float BestD = 5000.f;
	const ERDBotTeam Want = (Team == ERDBotTeam::Ally) ? ERDBotTeam::Defender : ERDBotTeam::Ally;
	for (AActor* A : Bots)
	{
		const ARDBot* B = Cast<ARDBot>(A);
		if (!B || B == this || B->IsDead() || B->Team != Want) continue;
		const float D = FVector::Dist(GetActorLocation(), B->GetActorLocation());
		if (D < BestD) { BestD = D; Best = A; }
	}
	if (Team == ERDBotTeam::Defender)
	{
		// In AI-vs-AI the player is a flying spectator — never target them
		const ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
		const bool bSpec = GM && GM->IsAIVsAIMode();
		if (!bSpec)
		{
			if (APawn* P = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
			{
				if (!P->Tags.Contains(FName(TEXT("RD_Spectator"))))
				{
					const float D = FVector::Dist(GetActorLocation(), P->GetActorLocation());
					if (D < BestD) { Best = P; }
				}
			}
		}
	}
	return Best;
}

AActor* ARDBot::FindTacticalEnemy() const
{
	// Humans: prefer enemy humans / soft targets. NEVER treat a robot as the primary
	// charge target (head-on vs robots is always a bad move).
	// Robots: prefer enemy robots (tank duel) else nearest.
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	const ERDBotTeam Want = (Team == ERDBotTeam::Ally) ? ERDBotTeam::Defender : ERDBotTeam::Ally;

	AActor* BestHuman = nullptr;
	AActor* BestRobot = nullptr;
	AActor* BestAny = nullptr;
	float BestHumanD = 1.e9f, BestRobotD = 1.e9f, BestAnyD = 1.e9f;

	for (AActor* A : Bots)
	{
		const ARDBot* B = Cast<ARDBot>(A);
		if (!B || B == this || B->IsDead() || B->Team != Want) continue;
		const float D = FVector::Dist(GetActorLocation(), B->GetActorLocation());
		if (D < BestAnyD) { BestAnyD = D; BestAny = A; }
		if (B->IsRobot())
		{
			if (D < BestRobotD) { BestRobotD = D; BestRobot = A; }
		}
		else
		{
			if (D < BestHumanD) { BestHumanD = D; BestHuman = A; }
		}
	}

	if (!bIsRobot)
	{
		// Human: soft targets first; only acknowledge robots if no human in range
		if (BestHuman && BestHumanD < 3200.f) return BestHuman;
		if (BestRobot && BestRobotD < 2800.f) return BestRobot; // shoot, but Engage will kite
		return BestHuman ? BestHuman : BestAny;
	}

	// Robot: take armor first, then anyone
	if (BestRobot && BestRobotD < 3000.f) return BestRobot;
	return BestAny;
}

AActor* ARDBot::FindNearestFriendlyRobot() const
{
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	AActor* Best = nullptr;
	float BestD = 4000.f;
	for (AActor* A : Bots)
	{
		const ARDBot* B = Cast<ARDBot>(A);
		if (!B || B == this || B->IsDead() || B->Team != Team || !B->IsRobot()) continue;
		const float D = FVector::Dist(GetActorLocation(), B->GetActorLocation());
		if (D < BestD) { BestD = D; Best = A; }
	}
	return Best;
}

AActor* ARDBot::FindNearestBreachable() const
{
	TArray<AActor*> Props;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDDestructible::StaticClass(), Props);
	AActor* Best = nullptr;
	float BestD = 2200.f;
	for (AActor* A : Props)
	{
		const ARDDestructible* D = Cast<ARDDestructible>(A);
		if (!D || D->IsDestroyed()) continue;
		const bool bDoorish = D->Kind.Contains(TEXT("door"), ESearchCase::IgnoreCase)
			|| D->Kind.Contains(TEXT("Sealed"), ESearchCase::IgnoreCase)
			|| A->ActorHasTag(FName(TEXT("RD_Breach")));
		const float Dist = FVector::Dist(GetActorLocation(), A->GetActorLocation());
		const float Score = Dist * (bDoorish ? 0.55f : 1.f);
		if (Score < BestD) { BestD = Score; Best = A; }
	}
	return Best;
}

void ARDBot::TryBreachTarget(AActor* Target, float DamageMul)
{
	if (!Target || !GetWorld() || bDead) return;
	const bool bDoor = Cast<ARDRoomDoor>(Target) != nullptr;
	const bool bWreck = Cast<ARDWreckage>(Target) != nullptr;
	const bool bPriorityBreach = bDoor || bWreck;
	// Doors: allow faster re-fire so "always shoot when close" actually melts the choke
	if (FireCD > 0.f)
	{
		if (!bPriorityBreach) return;
		if (FireCD > FireInterval * 0.22f) return;
	}

	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 70.f);
	const FVector Aim = Target->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const float Dist = FVector::Dist(Start, Aim);
	const float MaxR = bPriorityBreach ? FMath::Max(WeaponRange * 1.15f, 1500.f) : WeaponRange * 1.05f;
	if (Dist > MaxR) return;

	// Doors/wreck in range: always blast (no LOS reject — greybox frames block traces)
	const bool bClose = Dist < 900.f || bPriorityBreach;
	if (!bClose)
	{
		FHitResult LosHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RDBreachLos), false, this);
		if (GetWorld()->LineTraceSingleByChannel(LosHit, Start, Aim, ECC_Visibility, Params))
		{
			if (LosHit.GetActor() != Target && LosHit.GetActor() != Target->GetOwner())
			{
				return;
			}
		}
	}

	FireCD = FireInterval * (bPriorityBreach ? (bIsRobot ? 0.28f : 0.32f) : (bIsRobot ? 0.45f : 0.55f));
	const FColor Beam = (Team == ERDBotTeam::Ally)
		? (bIsRobot ? FColor(40, 230, 255) : FColor(60, 255, 90))
		: FColor(255, 40, 30);
	RDLaserFX::DrawBeam(GetWorld(), Start, Aim, Beam, bIsRobot ? 20.f : 15.f, 0.3f);
	RDLaserFX::PlayZap(GetWorld(), Start, Team == ERDBotTeam::Ally);

	const float Dmg = ShotDamage * DamageMul * (bIsRobot ? 1.25f : 1.f);
	if (ARDRoomDoor* Door = Cast<ARDRoomDoor>(Target))
	{
		Door->TakeDamageAmount(Dmg * 2.6f);
	}
	else if (ARDWreckage* W = Cast<ARDWreckage>(Target))
	{
		W->TakeDamageAmount(Dmg * 2.0f);
	}
	else if (ARDDestructible* Prop = Cast<ARDDestructible>(Target))
	{
		Prop->TakeDamageAmount(Dmg * 1.6f);
	}
}

void ARDBot::AlwaysShootNearbyBreach()
{
	if (bDead || Team != ERDBotTeam::Ally) return;

	// Priority 1: ISAAC room door / door wreck within "close" range → always fire
	const float DoorShootR = 1400.f;
	if (AActor* DoorOrWreck = FindNearestRoomDoor())
	{
		const float D = FVector::Dist(GetActorLocation(), DoorOrWreck->GetActorLocation());
		if (D < DoorShootR)
		{
			TryBreachTarget(DoorOrWreck, 3.0f);
			return;
		}
	}

	// Priority 2: sealed % panels / breach-tagged scenery nearby
	if (AActor* Prop = FindNearestBreachable())
	{
		const float D = FVector::Dist(GetActorLocation(), Prop->GetActorLocation());
		if (D < 1000.f)
		{
			const ARDDestructible* Des = Cast<ARDDestructible>(Prop);
			const bool bDoorish = Des && (Des->Kind.Contains(TEXT("door"), ESearchCase::IgnoreCase)
				|| Des->Kind.Contains(TEXT("Sealed"), ESearchCase::IgnoreCase)
				|| Prop->ActorHasTag(FName(TEXT("RD_Breach"))));
			if (bDoorish || D < 550.f)
			{
				TryBreachTarget(Prop, bDoorish ? 2.4f : 1.6f);
			}
		}
	}
}


bool ARDBot::EngageTactically(AActor* Target, float DeltaSeconds)
{
	if (!Target || bDead) return false;
	const float D = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	const ARDBot* EnemyBot = Cast<ARDBot>(Target);
	const bool bEnemyIsRobot = EnemyBot && EnemyBot->IsRobot();

	// === HUMAN vs ROBOT: never charge head-on ===
	if (!bIsRobot && bEnemyIsRobot)
	{
		const float Danger = 1100.f;
		const float Comfort = 1600.f;
		if (D < Danger)
		{
			// Back off while shooting
			const FVector Away = (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			FVector KitePos = GetActorLocation() + Away * 450.f;
			// Prefer kiting toward a friendly robot (use them as cover / tank)
			if (AActor* FriendR = FindNearestFriendlyRobot())
			{
				const FVector Fr = FriendR->GetActorLocation();
				KitePos = Fr + Away * 200.f;
			}
			MoveToward(KitePos, DeltaSeconds, 1.35f);
			TryFireAt(Target);
			return true;
		}
		if (D < Comfort)
		{
			// Strafe — keep distance, do not close
			const FVector To = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			const float LaneSign = (GetFormationLaneY() >= 0.f) ? 1.f : -1.f;
			const FVector Side = FVector(-To.Y, To.X, 0.f) * LaneSign;
			// Strafe on own flank — keep soft separation from friendlies while kiting
			MoveToward(GetActorLocation() + Side * 420.f - To * 90.f + GetFormationOffset() * 0.15f, DeltaSeconds, 1.1f);
			TryFireAt(Target);
			return true;
		}
		// Far: fire if LOS, but do not walk into the robot — hold or drift sideways
		TryFireAt(Target);
		return true; // claim engagement so caller does not charge
	}

	// === ROBOT vs anything: tank and close ===
	if (bIsRobot)
	{
		if (D > 380.f) MoveToward(Target->GetActorLocation(), DeltaSeconds, 1.15f);
		if (D < WeaponRange) TryFireAt(Target);
		return true;
	}

	// === HUMAN vs HUMAN: normal firefight, slight kite if too close ===
	if (D < 320.f)
	{
		const FVector Away = (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
		MoveToward(GetActorLocation() + Away * 250.f, DeltaSeconds, 1.05f);
	}
	else if (D > 700.f)
	{
		MoveToward(Target->GetActorLocation(), DeltaSeconds, 1.05f);
	}
	if (D < WeaponRange) TryFireAt(Target);
	return true;
}

AActor* ARDBot::FindNearestIsaac() const
{
	TArray<AActor*> Isaacs;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDIsaacNode::StaticClass(), Isaacs);
	AActor* Best = nullptr;
	float BestD = 20000.f;
	for (AActor* A : Isaacs)
	{
		if (const ARDIsaacNode* I = Cast<ARDIsaacNode>(A))
		{
			if (I->IsDestroyed()) continue;
			const float D = FVector::Dist(GetActorLocation(), I->GetActorLocation());
			if (D < BestD) { BestD = D; Best = A; }
		}
	}
	return Best;
}

AActor* ARDBot::FindNearestRoomDoor() const
{
	TArray<AActor*> Doors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDRoomDoor::StaticClass(), Doors);
	AActor* Best = nullptr;
	float BestD = 12000.f;
	for (AActor* A : Doors)
	{
		const ARDRoomDoor* D = Cast<ARDRoomDoor>(A);
		if (!D || D->IsBreached()) continue;
		const float Dist = FVector::Dist(GetActorLocation(), D->GetActorLocation());
		if (Dist < BestD) { BestD = Dist; Best = A; }
	}
	// Prefer wreckage if door is already down and wreck still blocks
	TArray<AActor*> Wrecks;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDWreckage::StaticClass(), Wrecks);
	for (AActor* A : Wrecks)
	{
		const ARDWreckage* W = Cast<ARDWreckage>(A);
		if (!W || W->IsCleared()) continue;
		const float Dist = FVector::Dist(GetActorLocation(), W->GetActorLocation());
		if (Dist < BestD) { BestD = Dist; Best = A; }
	}
	return Best;
}

static bool HasClearLOS(UWorld* World, AActor* Self, const FVector& From, AActor* Target)
{
	if (!World || !Target) return false;
	const FVector To = Target->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RDLos), false, Self);
	Params.AddIgnoredActor(Self);
	if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
	{
		return true; // nothing in the way
	}
	// Clear only if we hit the target (or its attach parent)
	return Hit.GetActor() == Target || (Hit.GetActor() && Hit.GetActor()->GetOwner() == Target);
}

void ARDBot::TryFireAt(AActor* Target)
{
	if (!Target || !GetWorld() || FireCD > 0.f || bDead) return;

	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 70.f);
	if (!HasClearLOS(GetWorld(), this, Start, Target))
	{
		return; // wall / prop blocks line of sight — no shot
	}

	FireCD = FireInterval;
	const FVector Aim = Target->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector End = Start + (Aim - Start).GetSafeNormal() * WeaponRange;
	const bool bAlly = (Team == ERDBotTeam::Ally);
	const FColor Beam = bAlly
		? (bIsRobot ? FColor(40, 230, 255) : FColor(60, 255, 90))
		: FColor(255, 40, 30);

	// Re-trace for damage — first hit only
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RDBotFire), false, this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	const FVector BeamEnd = bHit ? Hit.ImpactPoint : End;
	// Fat visible beam + zap (even if miss — shot still visible)
	RDLaserFX::DrawBeam(GetWorld(), Start, BeamEnd, Beam, bIsRobot ? 18.f : 14.f, 0.32f);
	RDLaserFX::PlayZap(GetWorld(), Start, bAlly);

	if (!bHit)
	{
		return;
	}

	AActor* Victim = Hit.GetActor();
	if (ARDBot* Bot = Cast<ARDBot>(Victim))
	{
		if (Bot->Team != Team) Bot->ApplyDamage(ShotDamage);
	}
	else if (ARDRoomDoor* Door = Cast<ARDRoomDoor>(Victim))
	{
		// Storm bonus: crack ISAAC door faster under AI training
		if (Team == ERDBotTeam::Ally)
		{
			const float Mul = (Order == ERDSquadOrder::Storm) ? 1.45f : 1.f;
			Door->TakeDamageAmount(ShotDamage * Mul);
		}
	}
	else if (ARDWreckage* Wreck = Cast<ARDWreckage>(Victim))
	{
		if (Team == ERDBotTeam::Ally)
		{
			const float Mul = (Order == ERDSquadOrder::Storm) ? 1.35f : 1.f;
			Wreck->TakeDamageAmount(ShotDamage * Mul);
		}
	}
	else if (ARDIsaacNode* Isaac = Cast<ARDIsaacNode>(Victim))
	{
		if (Team == ERDBotTeam::Ally)
		{
			const float Mul = (Order == ERDSquadOrder::Storm) ? 1.15f : 0.85f;
			Isaac->TakeDamageAmount(ShotDamage * Mul);
		}
	}
	else if (ARDCharacter* Leader = Cast<ARDCharacter>(Victim))
	{
		if (Team == ERDBotTeam::Defender)
		{
			Leader->ApplyDamage(ShotDamage);
		}
	}
}


void ARDBot::TickAllyStormAssault(float DeltaSeconds, AActor* Enemy, AActor* Isaac, const FVector& AssaultRally)
{
	const FVector Me = GetActorLocation();
	const FVector Form = GetFormationOffset();
	// Squad roles by unique slot (not % 3 — that collapsed 6 people into 3 piles)
	// 0 BREACH point · 1/2 WING support · 3/4 FAR FLANK · 5/6 REAR
	int32 SquadRole = 1; // support default
	if (FormationSlot == 0 || bIsRobot || UnitName.Equals(TEXT("Trump"), ESearchCase::IgnoreCase)) SquadRole = 0;
	else if (FormationSlot == 3 || FormationSlot == 4
		|| UnitName.Contains(TEXT("Haley")) || UnitName.Contains(TEXT("Rubio"))) SquadRole = 2;
	else if (FormationSlot == 5 || FormationSlot == 6) SquadRole = 1;

	AActor* DoorOrWreck = FindNearestRoomDoor();
	AActor* BreachProp = FindNearestBreachable();
	AActor* PrimaryBreach = DoorOrWreck ? DoorOrWreck : BreachProp;

	// Open-ground approach: each unit owns a lateral lane + depth — wedge not blob
	const float ApproachGateX = 5600.f;
	if (Me.X < ApproachGateX)
	{
		FVector Way = AssaultRally + Form;
		// Far flanks peel early north/south of airlock mouth
		if (SquadRole == 2)
		{
			Way.Y = AssaultRally.Y + Form.Y;
			Way.X = FMath::Min(Way.X, ApproachGateX - 200.f);
		}
		// Wings / rear: trail the breach element on their own lane — do NOT sit on the robot
		if (!bIsRobot && SquadRole != 2)
		{
			if (AActor* Tank = FindNearestFriendlyRobot())
			{
				const FVector T = Tank->GetActorLocation();
				if (T.X > Me.X - 120.f)
				{
					// Stay behind and offset — never stack on tank XY
					Way = T + Form + FVector(-180.f, 0.f, 0.f);
				}
			}
		}
		// Choke mouth: only point man presses; others hold staged lanes west
		if (ShouldYieldAtChoke(AssaultRally, 520.f) && SquadRole != 0)
		{
			const FVector Hold = GetChokeStagingPoint(AssaultRally, 520.f + FormationSlot * 60.f);
			MoveToward(Hold, DeltaSeconds, 0.65f);
			if (Enemy) EngageTactically(Enemy, DeltaSeconds);
			return;
		}
		// Stagger speed: breach first, flanks run, rear walks
		float Spd = 1.15f;
		if (SquadRole == 0) Spd = bIsRobot ? 1.55f : 1.35f;
		else if (SquadRole == 2) Spd = 1.4f;
		else Spd = 1.05f + 0.05f * (FormationSlot % 2);
		MoveToward(Way, DeltaSeconds, Spd);
		if (Enemy && FVector::Dist(Me, Enemy->GetActorLocation()) < 1200.f)
		{
			EngageTactically(Enemy, DeltaSeconds);
		}
		if (BreachProp && FVector::Dist(Me, BreachProp->GetActorLocation()) < 900.f)
		{
			TryBreachTarget(BreachProp, 1.4f);
		}
		return;
	}

	// Inside base — FAR FLANKS stay wide, overwatch / alternate breach
	if (SquadRole == 2 && PrimaryBreach)
	{
		const FVector DoorL = PrimaryBreach->GetActorLocation();
		const float Side = Form.Y >= 0.f ? 1.f : -1.f;
		const FVector Flank = DoorL + FVector(-280.f, Side * 900.f, 0.f);
		if (FVector::Dist2D(Me, Flank) > 220.f)
		{
			MoveToward(Flank, DeltaSeconds, 1.25f);
		}
		if (Enemy) EngageTactically(Enemy, DeltaSeconds);
		else TryBreachTarget(PrimaryBreach, 1.1f);
		return;
	}

	// WINGS — support from spaced rear corners, not the door sill
	if (SquadRole == 1 && PrimaryBreach)
	{
		const FVector DoorL = PrimaryBreach->GetActorLocation();
		const FVector Support = GetChokeStagingPoint(DoorL, 480.f);
		if (ShouldYieldAtChoke(DoorL, 560.f) || FVector::Dist2D(Me, Support) > 200.f)
		{
			MoveToward(Support, DeltaSeconds, ShouldYieldAtChoke(DoorL, 560.f) ? 0.75f : 1.1f);
		}
		if (Enemy && FVector::Dist(Me, Enemy->GetActorLocation()) < 1600.f)
		{
			EngageTactically(Enemy, DeltaSeconds);
		}
		TryBreachTarget(PrimaryBreach, 1.5f);
		if (BreachProp && BreachProp != PrimaryBreach) TryBreachTarget(BreachProp, 1.2f);
		return;
	}

	// BREACH element — robots first; humans only if clear
	if (PrimaryBreach)
	{
		const FVector DoorL = PrimaryBreach->GetActorLocation();
		const float D = FVector::Dist(Me, DoorL);
		if (ShouldYieldAtChoke(DoorL, 480.f) && !bIsRobot)
		{
			MoveToward(GetChokeStagingPoint(DoorL, 420.f), DeltaSeconds, 0.7f);
			TryBreachTarget(PrimaryBreach, 1.3f);
			if (Enemy) EngageTactically(Enemy, DeltaSeconds);
			return;
		}
		if (D > 220.f)
		{
			// Tiny lateral only for multi-robot; never all on door center
			FVector DoorGoal = DoorL + FVector(0.f, Form.Y * 0.08f, 0.f);
			MoveToward(DoorGoal, DeltaSeconds, bIsRobot ? 1.5f : 1.15f);
		}
		TryBreachTarget(PrimaryBreach, bIsRobot ? 2.4f : 1.9f);
		if (Enemy && FVector::Dist(Me, Enemy->GetActorLocation()) < 700.f)
		{
			if (bIsRobot) EngageTactically(Enemy, DeltaSeconds);
			else if (const ARDBot* EB = Cast<ARDBot>(Enemy))
			{
				if (!EB->IsRobot()) EngageTactically(Enemy, DeltaSeconds);
				else TryFireAt(Enemy);
			}
		}
		return;
	}

	if (Isaac)
	{
		const float D = FVector::Dist(Me, Isaac->GetActorLocation());
		if (ShouldYieldAtChoke(Isaac->GetActorLocation(), 400.f) && SquadRole != 0)
		{
			MoveToward(GetChokeStagingPoint(Isaac->GetActorLocation(), 380.f), DeltaSeconds, 0.85f);
		}
		else if (D > 300.f)
		{
			const FVector Atk = Isaac->GetActorLocation() + FVector(Form.X * 0.3f, Form.Y * 0.45f, 0.f);
			MoveToward(Atk, DeltaSeconds, 1.3f);
		}
		TryFireAt(Isaac);
		if (Enemy && D < 1100.f) EngageTactically(Enemy, DeltaSeconds);
		return;
	}

	if (Enemy) EngageTactically(Enemy, DeltaSeconds);
	else MoveToward(AssaultRally + Form + FVector(1800.f, 0.f, 0.f), DeltaSeconds, 1.15f);
}

void ARDBot::TickAllyObjectives(float DeltaSeconds, AActor* Enemy, AActor* Isaac, const FVector& AssaultRally)
{
	const FVector Me = GetActorLocation();
	const FVector Form = GetFormationOffset();
	AActor* Door = FindNearestRoomDoor();       // ISAAC door or wreck
	AActor* Panel = FindNearestBreachable();    // sealed % / other doors
	const float EnemyD = Enemy ? FVector::Dist(Me, Enemy->GetActorLocation()) : 1.e9f;
	const float DoorD = Door ? FVector::Dist(Me, Door->GetActorLocation()) : 1.e9f;
	const float PanelD = Panel ? FVector::Dist(Me, Panel->GetActorLocation()) : 1.e9f;
	const float IsaacD = Isaac ? FVector::Dist(Me, Isaac->GetActorLocation()) : 1.e9f;

	// --- 0) Survive: human vs enemy robot in face ---
	if (!bIsRobot && Enemy)
	{
		if (const ARDBot* EB = Cast<ARDBot>(Enemy))
		{
			if (EB->IsRobot() && EnemyD < 1000.f)
			{
				EngageTactically(Enemy, DeltaSeconds);
				AlwaysShootNearbyBreach();
				return;
			}
		}
	}

	// --- 1) DOORS (ISAAC room door / wreck, then sealed panels) ---
	// Always shoot when close; move in if not at muzzle range
	if (Door && DoorD < 1600.f)
	{
		AlwaysShootNearbyBreach();
		TryBreachTarget(Door, 3.2f);
		if (DoorD > 220.f)
		{
			MoveToward(Door->GetActorLocation() + Form * 0.12f, DeltaSeconds, bIsRobot ? 1.55f : 1.4f);
		}
		if (Enemy && EnemyD < 750.f) TryFireAt(Enemy);
		return;
	}
	if (Panel && PanelD < 1100.f)
	{
		const ARDDestructible* Des = Cast<ARDDestructible>(Panel);
		const bool bDoorish = Des && (Des->Kind.Contains(TEXT("door"), ESearchCase::IgnoreCase)
			|| Des->Kind.Contains(TEXT("Sealed"), ESearchCase::IgnoreCase)
			|| Panel->ActorHasTag(FName(TEXT("RD_Breach"))));
		if (bDoorish || PanelD < 600.f)
		{
			TryBreachTarget(Panel, bDoorish ? 2.8f : 1.8f);
			if (PanelD > 200.f)
			{
				MoveToward(Panel->GetActorLocation(), DeltaSeconds, 1.35f);
			}
			if (Enemy && EnemyD < 700.f) TryFireAt(Enemy);
			return;
		}
	}

	// --- 2) ISAAC (win condition) ---
	if (Isaac)
	{
		// Path into base first if still west of airlock mouth
		if (Me.X < 5600.f)
		{
			MoveToward(AssaultRally + Form, DeltaSeconds, bIsRobot ? 1.6f : 1.5f);
			AlwaysShootNearbyBreach();
			if (Enemy && EnemyD < 900.f) EngageTactically(Enemy, DeltaSeconds);
			return;
		}
		if (ShouldYieldAtChoke(Isaac->GetActorLocation(), 380.f) && !bIsRobot)
		{
			MoveToward(GetChokeStagingPoint(Isaac->GetActorLocation(), 360.f), DeltaSeconds, 1.05f);
		}
		else if (IsaacD > 280.f)
		{
			const FVector Atk = Isaac->GetActorLocation() + FVector(Form.X * 0.25f, Form.Y * 0.4f, 0.f);
			MoveToward(Atk, DeltaSeconds, 1.45f);
		}
		TryFireAt(Isaac);
		// Secondary: enemy while on objective
		if (Enemy && EnemyD < 1200.f) TryFireAt(Enemy);
		AlwaysShootNearbyBreach();
		return;
	}

	// --- 3) ENEMY (clear remaining UK when ISAAC is gone or not found) ---
	if (Enemy)
	{
		EngageTactically(Enemy, DeltaSeconds);
		AlwaysShootNearbyBreach();
		return;
	}

	// --- 4) Push deeper into base / search ---
	MoveToward(AssaultRally + Form + FVector(2500.f, 0.f, 0.f), DeltaSeconds, 1.4f);
	AlwaysShootNearbyBreach();
}

void ARDBot::TickAlly(float DeltaSeconds)
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this));
	const bool bAIVsAI = GM && GM->IsAIVsAIMode();
	const FVector AssaultRally = GM
		? GM->GetApproachWaypoint(FormationSlot)
		: FVector(5200.f, -5850.f, 120.f);

	// Spectator / training: always assault with full objective chain
	if (bAIVsAI && Order != ERDSquadOrder::Storm && Order != ERDSquadOrder::Attack)
	{
		Order = ERDSquadOrder::Storm;
	}

	AActor* Enemy = bIsRobot ? FindNearestEnemy() : FindTacticalEnemy();
	AActor* Isaac = FindNearestIsaac();

	// AI-vs-AI and Storm/Attack share the same objective brain
	if (bAIVsAI || Order == ERDSquadOrder::Storm || Order == ERDSquadOrder::Attack)
	{
		TickAllyObjectives(DeltaSeconds, Enemy, Isaac, AssaultRally);
		return;
	}

	switch (Order)
	{
	case ERDSquadOrder::Hold:
		MoveToward(HoldPoint, DeltaSeconds, 0.95f);
		if (Enemy && FVector::Dist(GetActorLocation(), Enemy->GetActorLocation()) < 2200.f)
		{
			EngageTactically(Enemy, DeltaSeconds);
		}
		AlwaysShootNearbyBreach();
		break;

	case ERDSquadOrder::Follow:
	default:
	{
		if (!Player) return;
		const FVector Offset = GetFormationOffset() + FVector(-120.f, 0.f, 0.f);
		const FVector FollowPt = Player->GetActorLocation() + Player->GetActorRotation().RotateVector(Offset);
		const float Dist = FVector::Dist2D(GetActorLocation(), Player->GetActorLocation());
		if (Dist > 220.f)
		{
			float Scale = 1.25f;
			if (Dist > 6000.f) Scale = 2.8f;
			else if (Dist > 3000.f) Scale = 2.3f;
			else if (Dist > 1200.f) Scale = 1.9f;
			else if (Dist > 600.f) Scale = 1.5f;
			MoveToward(FollowPt, DeltaSeconds, Scale);
		}
		if (Enemy && FVector::Dist(GetActorLocation(), Enemy->GetActorLocation()) < 1800.f)
		{
			TryFireAt(Enemy);
		}
		AlwaysShootNearbyBreach();
		break;
	}
	}
}

void ARDBot::TickDoorGuard(float DeltaSeconds)
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	AActor* Enemy = FindNearestEnemy();

	if (bBlockingDoorway)
	{
		// Wedge the doorway — stay planted, shoot anyone approaching
		const float Dist = FVector::Dist2D(GetActorLocation(), DoorBlockPoint);
		if (Dist > 80.f)
		{
			MoveToward(DoorBlockPoint, DeltaSeconds, 1.4f);
		}
		else
		{
			// Face outward (south — toward main approach)
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, 90.f, 0.f), DeltaSeconds, 8.f));
		}
		if (Enemy && FVector::Dist(GetActorLocation(), Enemy->GetActorLocation()) < 2500.f)
		{
			TryFireAt(Enemy);
		}
		else if (Player && !Player->Tags.Contains(FName(TEXT("RD_Spectator")))
			&& FVector::Dist(GetActorLocation(), Player->GetActorLocation()) < 2500.f)
		{
			TryFireAt(Player);
		}
		return;
	}

	// Standby: hold near door, small patrol, shoot if LOS to attackers
	const float DistHome = FVector::Dist2D(GetActorLocation(), DoorStandbyPoint);
	if (DistHome > 200.f)
	{
		MoveToward(DoorStandbyPoint, DeltaSeconds, 0.85f);
	}
	else
	{
		RepathCD -= DeltaSeconds;
		if (RepathCD <= 0.f)
		{
			RepathCD = FMath::FRandRange(1.5f, 3.f);
			WanderTarget = DoorStandbyPoint + FVector(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-120.f, 120.f), 0.f);
		}
		MoveToward(WanderTarget, DeltaSeconds, 0.4f);
	}

	if (Enemy && FVector::Dist(GetActorLocation(), Enemy->GetActorLocation()) < 1800.f)
	{
		const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 70.f);
		if (HasClearLOS(GetWorld(), this, Eye, Enemy))
		{
			TryFireAt(Enemy);
			if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
			{
				GM->RaiseAlarm(GetActorLocation());
			}
		}
	}
	if (Player && !Player->Tags.Contains(FName(TEXT("RD_Spectator")))
		&& FVector::Dist(GetActorLocation(), Player->GetActorLocation()) < 1600.f)
	{
		const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 70.f);
		if (HasClearLOS(GetWorld(), this, Eye, Player))
		{
			TryFireAt(Player);
			if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
			{
				GM->RaiseAlarm(GetActorLocation());
			}
		}
	}
}

void ARDBot::TickDefender(float DeltaSeconds)
{
	if (bDoorGuard)
	{
		TickDoorGuard(DeltaSeconds);
		return;
	}

	ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this));
	const bool bAlarm = GM && GM->IsAlarmRaised();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bSpecPlayer = (GM && GM->IsAIVsAIMode()) ||
		(Player && Player->Tags.Contains(FName(TEXT("RD_Spectator"))));
	// Humans prefer soft targets; robots take armor
	AActor* Target = bIsRobot ? FindNearestEnemy() : FindTacticalEnemy();
	if (!Target && !bSpecPlayer) Target = Player;

	// Spot → 1.2s radio (2D) then base alarm
	if (Target)
	{
		const float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
		if (Dist < SightRange)
		{
			const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 70.f);
			if (HasClearLOS(GetWorld(), this, Eye, Target))
			{
				StartAlarmRadio(Target->GetActorLocation());
				TryFireAt(Target);
			}
		}
	}
	if (AlarmRadioT > 0.f)
	{
		AlarmRadioT -= DeltaSeconds;
		if (AlarmRadioT <= 0.f && GM)
		{
			GM->RaiseAlarm(KnownContact);
		}
	}

	if (!bAlarm)
	{
		// 2D ST_UNAWARE: sentries freeze on post; others wander near home
		if (bHoldPost)
		{
			const float DistHome = FVector::Dist2D(GetActorLocation(), Home);
			if (DistHome > 100.f) MoveToward(Home, DeltaSeconds, 0.7f);
			return;
		}
		RepathCD -= DeltaSeconds;
		if (RepathCD <= 0.f || FVector::Dist2D(GetActorLocation(), WanderTarget) < 120.f)
		{
			RepathCD = FMath::FRandRange(2.0f, 4.5f);
			WanderTarget = Home + FVector(FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-500.f, 500.f), 0.f);
		}
		MoveToward(WanderTarget, DeltaSeconds, 0.5f);
		return;
	}

	// Alarm: execute learned defense plan
	if (GM && !bHoldPost && !bDoorGuard)
	{
		const ERDDefensePlan Plan = GM->GetCurrentDefense();
		if (Plan == ERDDefensePlan::RushAirlock || Plan == ERDDefensePlan::SplitGuard)
		{
			const FVector Rally = GM->GetDefenseRally(this);
			const float RD = FVector::Dist2D(GetActorLocation(), Rally);
			// Move to rally unless actively fighting up close
			if (RD > 250.f && (!Target || FVector::Dist(GetActorLocation(), Target->GetActorLocation()) > 900.f))
			{
				MoveToward(Rally, DeltaSeconds, 1.15f);
				if (Target) TryFireAt(Target);
				return;
			}
		}
	}

	// Alarm: hunt with tactics. Sentries hold post.
	if (Target)
	{
		const float D = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
		const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 70.f);
		const bool bSee = HasClearLOS(GetWorld(), this, Eye, Target);
		if (bSee || D < 600.f)
		{
			if (bHoldPost)
			{
				TryFireAt(Target);
			}
			else
			{
				// UK humans also kite US robots — same rule both ways
				EngageTactically(Target, DeltaSeconds);
			}
		}
		else if (!bHoldPost)
		{
			if (D > 200.f)
			{
				// Don't sprint humans into unknown robot packs
				if (!bIsRobot)
				{
					if (const ARDBot* TB = Cast<ARDBot>(Target))
					{
						if (TB->IsRobot())
						{
							// Flank / wait for LOS instead of charging
							const FVector Side = FVector(-(Target->GetActorLocation() - GetActorLocation()).Y,
								(Target->GetActorLocation() - GetActorLocation()).X, 0.f).GetSafeNormal() * 300.f;
							MoveToward(GetActorLocation() + Side, DeltaSeconds, 0.85f);
							return;
						}
					}
				}
				MoveToward(Target->GetActorLocation(), DeltaSeconds, 0.9f);
			}
		}
		else
		{
			MoveToward(Home, DeltaSeconds, 0.8f);
		}
	}
	else if (bHoldPost)
	{
		MoveToward(Home, DeltaSeconds, 0.6f);
	}
	else if (GM)
	{
		MoveToward(GM->GetDefenseRally(this), DeltaSeconds, 0.8f);
	}
}

void ARDBot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead) return;
	FireCD = FMath::Max(0.f, FireCD - DeltaSeconds);
	HitFlash = FMath::Max(0.f, HitFlash - DeltaSeconds);

	// Anti-roof: if we got ejected onto wall tops / ceilings, drop back inside
	const FVector Loc = GetActorLocation();
	if (Loc.Z > 220.f)
	{
		const float HalfH = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.f;
		const FVector Start(Loc.X, Loc.Y, 550.f);
		const FVector End(Loc.X, Loc.Y, -50.f);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RDAntiRoof), false, this);
		if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)
			&& Hit.bBlockingHit && Hit.ImpactPoint.Z < 150.f)
		{
			SetActorLocation(Hit.ImpactPoint + FVector(0.f, 0.f, HalfH + 4.f), false, nullptr, ETeleportType::TeleportPhysics);
			Home = GetActorLocation();
			HoldPoint = Home;
			WanderTarget = Home;
		}
		else
		{
			SetActorLocation(FVector(Loc.X, Loc.Y, HalfH + 20.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	// Tab-controlled by player — no AI
	if (bPlayerDriven) return;

	// Wreckage slows bots too (door crawl-through)
	float SpeedMul = 1.f;
	TArray<AActor*> Wrecks;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDWreckage::StaticClass(), Wrecks);
	for (AActor* A : Wrecks)
	{
		if (const ARDWreckage* W = Cast<ARDWreckage>(A))
		{
			if (W->IsActorInside(this))
			{
				SpeedMul = FMath::Min(SpeedMul, W->GetSlowMultiplier());
			}
		}
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = MoveSpeed * SpeedMul;
		Move->MaxFlySpeed = MoveSpeed * SpeedMul;
	}

	TickStuckRecovery(DeltaSeconds);
	// Periodic wall-clip repair (cheap — only when not clear)
	if ((GetUniqueID() + GFrameNumber) % 8 == 0)
	{
		ResolveWallPenetration();
	}

	// Name + country always visible for spectator QA
	if (GetWorld() && !UnitName.IsEmpty())
	{
		const bool bUS = CountryCode.Equals(TEXT("US"), ESearchCase::IgnoreCase);
		const FColor NameCol = bLeaderSuit ? FColor(255, 215, 60)
			: (bUS ? FColor(120, 180, 255) : FColor(220, 90, 90));
		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 145.f),
			FString::Printf(TEXT("%s  %s"), *UnitName, *CountryCode),
			nullptr, NameCol, 0.f, true, 1.2f);
	}

	if (Team == ERDBotTeam::Ally) TickAlly(DeltaSeconds);
	else TickDefender(DeltaSeconds);
}
