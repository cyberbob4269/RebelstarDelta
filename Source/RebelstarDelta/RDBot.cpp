// Rebelstar Delta — reliable move + killable + squad orders

#include "RDBot.h"
#include "RDCharacter.h"
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
#include "UObject/ConstructorHelpers.h"

ARDBot::ARDBot()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.f);
	// Pawn channel so lasers hit reliably
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	GetCharacterMovement()->MaxWalkSpeed = 520.f;
	GetCharacterMovement()->GravityScale = 0.16f;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->BrakingDecelerationWalking = 3000.f;
	GetCharacterMovement()->GroundFriction = 8.f;
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
		MoveSpeed = 320.f;   // 2D: 80 vs human 130 → ~62% of human
		ShotDamage = 34.f;
		FireInterval = 0.9f;
		WeaponRange = 2400.f;
		SightRange = 2200.f;
		GetCapsuleComponent()->InitCapsuleSize(62.f, 100.f);
		break;
	default:
		bIsRobot = false;
		bFemaleSuit = UnitName.Contains(TEXT("Haley")) || UnitName.Contains(TEXT("Truss"))
			|| UnitName.Contains(TEXT("May")) || UnitName.Contains(TEXT("Badenoch"))
			|| UnitName.Contains(TEXT("Braverman"));
		MaxHP = bLeaderSuit ? 90.f : 60.f;
		MoveSpeed = bLeaderSuit ? 540.f : 520.f;
		ShotDamage = bLeaderSuit ? 22.f : 16.f;
		FireInterval = bLeaderSuit ? 0.4f : 0.5f;
		WeaponRange = 2800.f;
		SightRange = 2400.f;
		GetCapsuleComponent()->InitCapsuleSize(bFemaleSuit ? 44.f : 48.f, bFemaleSuit ? 88.f : 92.f);
		break;
	}

	bHoldPost = (InRole == ERDUnitRole::SentryDroid);
	HP = MaxHP;
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
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
	FormationSlot = GetUniqueID() % 3;
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	BuildSuit();
	RefreshLook();
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
	if (Moved < 8.f)
	{
		StuckT += DeltaSeconds;
	}
	else
	{
		StuckT = 0.f;
	}
	LastPos = Now;

	if (UnstickT > 0.f)
	{
		UnstickT -= DeltaSeconds;
		const FVector Nudge = UnstickDir * MoveSpeed * 1.4f * DeltaSeconds;
		SetActorLocation(Now + FVector(Nudge.X, Nudge.Y, 0.f), false);
		return;
	}

	if (StuckT > 1.1f)
	{
		++StuckEvents;
		StuckT = 0.f;
		UnstickT = 0.5f;
		// Sidestep with SWEEP only — no wall-phasing teleports
		const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
		const FVector Side = FVector(-Fwd.Y, Fwd.X, 0.f) * (FMath::FRand() < 0.5f ? 1.f : -1.f);
		UnstickDir = (Side + Fwd * 0.35f).GetSafeNormal2D();
		FVector Try = Now + UnstickDir * 100.f + Fwd * 40.f;
		Try.Z = FMath::Clamp(Now.Z, 100.f, 160.f);
		FHitResult Hit;
		SetActorLocation(Try, true, &Hit);
		if (Hit.bBlockingHit || !IsLocationClear(GetActorLocation()))
		{
			// Other side
			Try = Now - UnstickDir * 100.f + Fwd * 30.f;
			Try.Z = FMath::Clamp(Now.Z, 100.f, 160.f);
			SetActorLocation(Try, true);
		}
		if (!IsLocationClear(GetActorLocation()))
		{
			ResolveWallPenetration();
		}
		UE_LOG(LogTemp, Verbose, TEXT("[RebelstarDelta] Unstick %s (event %d)"), *UnitName, StuckEvents);
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

void ARDBot::MoveToward(const FVector& WorldTarget, float DeltaSeconds, float SpeedScale)
{
	FVector To = WorldTarget - GetActorLocation();
	To.Z = 0.f;
	const float Dist = To.Size();
	if (Dist < 90.f)
	{
		return;
	}
	const FVector Dir = To / Dist;
	const float Step = MoveSpeed * SpeedScale * DeltaSeconds;
	FVector Desired = GetActorLocation() + Dir * FMath::Min(Step, Dist);
	// Keep feet on playable height (plain + base floor)
	Desired.Z = FMath::Clamp(GetActorLocation().Z, 100.f, 160.f);

	// ALWAYS sweep against walls. (No-sweep long moves were walking through base geometry.)
	// On open regolith west of the base we still sweep, but take a slightly larger step.
	const bool bOpenPlain = GetActorLocation().X < 4800.f;
	if (bOpenPlain && Dist > 2000.f)
	{
		// Pad bumps: try sweep first; only if blocked by a tiny floor prop, slide sideways with sweep
		FHitResult Hit;
		SetActorLocation(Desired, true, &Hit);
		if (Hit.bBlockingHit)
		{
			const FVector Slide = FVector::VectorPlaneProject(Dir, Hit.Normal).GetSafeNormal() * Step;
			FVector SlidePos = GetActorLocation() + Slide;
			SlidePos.Z = Desired.Z;
			SetActorLocation(SlidePos, true);
		}
	}
	else
	{
		FHitResult Hit;
		SetActorLocation(Desired, true, &Hit);
		if (Hit.bBlockingHit)
		{
			// Wall slide — never disable sweep (that caused wall-phasing)
			const FVector Slide = FVector::VectorPlaneProject(Dir, Hit.Normal).GetSafeNormal() * Step * 0.9f;
			FVector SlidePos = GetActorLocation() + Slide;
			SlidePos.Z = Desired.Z;
			FHitResult Hit2;
			SetActorLocation(SlidePos, true, &Hit2);
			if (Hit2.bBlockingHit)
			{
				// Try opposite slide axis
				const FVector Side(-Dir.Y, Dir.X, 0.f);
				for (float S : { 1.f, -1.f })
				{
					FVector Alt = GetActorLocation() + Side * S * Step * 0.8f + Dir * Step * 0.25f;
					Alt.Z = Desired.Z;
					FHitResult Hit3;
					SetActorLocation(Alt, true, &Hit3);
					if (!Hit3.bBlockingHit) break;
				}
			}
		}
	}

	// If we somehow ended inside solid (spawn / unstick residue), eject
	if (!IsLocationClear(GetActorLocation()))
	{
		ResolveWallPenetration();
	}

	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Dir.Rotation(), DeltaSeconds, 12.f));
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
			const FVector Side = FVector(-To.Y, To.X, 0.f) * ((FormationSlot % 2 == 0) ? 1.f : -1.f);
			MoveToward(GetActorLocation() + Side * 350.f - To * 80.f, DeltaSeconds, 1.1f);
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

void ARDBot::TickAlly(float DeltaSeconds)
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this));
	const bool bAIVsAI = GM && GM->IsAIVsAIMode();
	const FVector AssaultRally = GM
		? GM->GetApproachWaypoint(FormationSlot)
		: FVector(5200.f, -5850.f, 120.f);

	// Spectator loop: always storm (SetOrder can race BeginPlay / Live Coding)
	if (bAIVsAI && Order != ERDSquadOrder::Storm && Order != ERDSquadOrder::Attack)
	{
		Order = ERDSquadOrder::Storm;
	}

	// Humans use tactical target selection (avoid robot head-ons)
	AActor* Enemy = bIsRobot ? FindNearestEnemy() : FindTacticalEnemy();
	AActor* Isaac = FindNearestIsaac();

	// Humans: if a robot is too close, ALWAYS kite first (even mid-objective)
	if (!bIsRobot && Enemy)
	{
		if (const ARDBot* EB = Cast<ARDBot>(Enemy))
		{
			if (EB->IsRobot() && FVector::Dist(GetActorLocation(), Enemy->GetActorLocation()) < 1200.f)
			{
				EngageTactically(Enemy, DeltaSeconds);
				// Still allow progress if very far west — but never walk into the robot
				return;
			}
		}
	}

	switch (Order)
	{
	case ERDSquadOrder::Hold:
		MoveToward(HoldPoint, DeltaSeconds, 0.8f);
		if (Enemy && FVector::Dist(GetActorLocation(), Enemy->GetActorLocation()) < 2200.f)
		{
			EngageTactically(Enemy, DeltaSeconds);
		}
		break;

	case ERDSquadOrder::Attack:
		if (Enemy)
		{
			EngageTactically(Enemy, DeltaSeconds);
		}
		else if (bAIVsAI)
		{
			MoveToward(AssaultRally, DeltaSeconds, 1.3f);
		}
		else if (Player)
		{
			MoveToward(Player->GetActorLocation(), DeltaSeconds, 1.f);
		}
		break;

	case ERDSquadOrder::Storm:
	{
		// Approach via learned route (airlock / north flank / south flank)
		const FVector Me = GetActorLocation();
		const float ApproachGateX = 5600.f;
		if (Me.X < ApproachGateX)
		{
			// Robots lead: humans lag slightly behind friendly robots when possible
			FVector Way = AssaultRally;
			if (!bIsRobot)
			{
				if (AActor* Tank = FindNearestFriendlyRobot())
				{
					const FVector T = Tank->GetActorLocation();
					// Stay behind the tank relative to base (+X)
					if (T.X > Me.X - 100.f)
					{
						Way = T + FVector(-220.f, (FormationSlot - 1) * 60.f, 0.f);
					}
				}
			}
			MoveToward(Way, DeltaSeconds, bIsRobot ? 1.55f : 1.35f);
			if (Enemy && FVector::Dist(Me, Enemy->GetActorLocation()) < 1100.f)
			{
				EngageTactically(Enemy, DeltaSeconds);
			}
			break;
		}

		// Classic order of work: door → wreckage → ISAAC
		// Skirmish only with tactics (no human head-on vs robots)
		AActor* DoorOrWreck = FindNearestRoomDoor();
		if (Enemy && FVector::Dist(Me, Enemy->GetActorLocation()) < 850.f)
		{
			EngageTactically(Enemy, DeltaSeconds);
			// Humans that are kiting skip objective push this frame
			if (!bIsRobot)
			{
				if (const ARDBot* EB = Cast<ARDBot>(Enemy))
				{
					if (EB->IsRobot()) break;
				}
			}
		}
		if (DoorOrWreck)
		{
			const float D = FVector::Dist(Me, DoorOrWreck->GetActorLocation());
			// Robots push door; humans support from slightly back if robots alive
			FVector DoorGoal = DoorOrWreck->GetActorLocation();
			if (!bIsRobot && FindNearestFriendlyRobot())
			{
				DoorGoal += FVector(-180.f, (FormationSlot - 1) * 70.f, 0.f);
			}
			if (D > 280.f) MoveToward(DoorGoal, DeltaSeconds, bIsRobot ? 1.45f : 1.25f);
			TryFireAt(DoorOrWreck);
			// Point-blank breach: LOS often blocked by door frame / own teammates
			if (D < 550.f)
			{
				if (ARDRoomDoor* Door = Cast<ARDRoomDoor>(DoorOrWreck))
				{
					if (!Door->IsBreached() && FireCD <= 0.f)
					{
						FireCD = FireInterval * 0.55f;
						Door->TakeDamageAmount(ShotDamage * (bIsRobot ? 2.0f : 1.5f));
						if (GetWorld())
						{
							const FVector S = Me + FVector(0, 0, 70);
							const FVector E = Door->GetActorLocation() + FVector(0, 0, 100);
							RDLaserFX::DrawBeam(GetWorld(), S, E, FColor(80, 255, 120), 16.f, 0.25f);
							RDLaserFX::PlayZap(GetWorld(), S, true);
						}
					}
				}
				else if (ARDWreckage* W = Cast<ARDWreckage>(DoorOrWreck))
				{
					if (!W->IsCleared() && FireCD <= 0.f)
					{
						FireCD = FireInterval * 0.6f;
						W->TakeDamageAmount(ShotDamage * 1.6f);
					}
				}
			}
			if (Enemy && D < 900.f) EngageTactically(Enemy, DeltaSeconds);
		}
		else if (Isaac)
		{
			const float D = FVector::Dist(Me, Isaac->GetActorLocation());
			if (D > 350.f) MoveToward(Isaac->GetActorLocation(), DeltaSeconds, 1.3f);
			TryFireAt(Isaac);
			if (D < 1000.f) TryFireAt(Isaac);
			if (Enemy && D < 1000.f) EngageTactically(Enemy, DeltaSeconds);
		}
		else if (Enemy)
		{
			EngageTactically(Enemy, DeltaSeconds);
		}
		else if (bAIVsAI)
		{
			MoveToward(AssaultRally, DeltaSeconds, 1.4f);
		}
		else if (Player)
		{
			MoveToward(Player->GetActorLocation(), DeltaSeconds, 1.f);
		}
		break;
	}

	case ERDSquadOrder::Follow:
	default:
	{
		if (bAIVsAI)
		{
			if (Enemy) EngageTactically(Enemy, DeltaSeconds);
			else MoveToward(AssaultRally, DeltaSeconds, 1.5f);
			break;
		}
		if (!Player) return;
		// Formation offset behind player (works from far Approach LZ across the plain)
		const FVector Offset(-180.f - FormationSlot * 110.f, (FormationSlot - 1) * 200.f, 0.f);
		const FVector FollowPt = Player->GetActorLocation() + Player->GetActorRotation().RotateVector(Offset);
		const float Dist = FVector::Dist2D(GetActorLocation(), Player->GetActorLocation());
		if (Dist > 220.f)
		{
			// Sprint catch-up across open regolith (LZ can be 100m+ from base)
			float Scale = 1.15f;
			if (Dist > 6000.f) Scale = 2.6f;
			else if (Dist > 3000.f) Scale = 2.2f;
			else if (Dist > 1200.f) Scale = 1.8f;
			else if (Dist > 600.f) Scale = 1.45f;
			MoveToward(FollowPt, DeltaSeconds, Scale);
		}
		// Shoot while following if enemy close
		if (Enemy && FVector::Dist(GetActorLocation(), Enemy->GetActorLocation()) < 1800.f)
		{
			TryFireAt(Enemy);
		}
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
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed * SpeedMul;

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
