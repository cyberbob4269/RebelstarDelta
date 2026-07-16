// Rebelstar Delta

#include "RDGameMode.h"
#include "RDBot.h"
#include "RDCharacter.h"
#include "RDHUD.h"
#include "RDIsaacNode.h"
#include "RDLunarStage.h"
#include "RDMapBuilder.h"
#include "RDPlayerController.h"
#include "RDMoonBuggy.h"
#include "RDRoomDoor.h"
#include "RDStarship.h"
#include "RDWreckage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

ARDGameMode::ARDGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ARDCharacter::StaticClass();
	PlayerControllerClass = ARDPlayerController::StaticClass();
	HUDClass = ARDHUD::StaticClass();
}

void ARDGameMode::BeginPlay()
{
	Super::BeginPlay();
	// Training series: shorter clocks so 20 rematches are practical
	if (bAIVsAIMode && TargetTrainingRounds > 0)
	{
		MatchDuration = TrainingMatchDuration;
		RematchDelay = FMath::Min(RematchDelay, 4.f);
		AutoAlarmDelay = FMath::Min(AutoAlarmDelay, 1.5f);
	}
	TimeLeft = MatchDuration;
	StatusMessage = bAIVsAIMode
		? FString::Printf(TEXT("TRAINING series target %d — US(Trump) vs UK — roofs open"), TargetTrainingRounds)
		: TEXT("Approach LZ — base is east. Tab=squad  1-4=orders");
	bMatchOver = false;
	bAttackersWin = false;
	bEndingCinematic = false;
	bAlarm = false;
	bIsaacDoorBreached = false;
	bDoorWreckageActive = false;
	AutoAlarmTimer = AutoAlarmDelay;
	RematchTimer = -1.f;
	MonitorLogTimer = 0.f;
	AttackerKills = 0;
	DefenderKills = 0;
	LastAliveAllies = -1;
	LastAliveDefs = -1;
	PeakAllyX = -9000.f;
	RoundIndex = 1;
	AttackerWins = 0;
	DefenderWins = 0;
	LearnedAllySpeedMul = 1.f;
	LearnedAllyDamageMul = 1.f;
	LearnedAllyFireMul = 1.f;
	LearnedDefSpeedMul = 1.f;
	LearnedDefFireMul = 1.f;
	StalemateStreak = 0;
	IsaacNodes.Reset();
	Starship = nullptr;
	IsaacDoor = nullptr;
	DoorGuardBot = nullptr;
	DoorWreckage = nullptr;
	SpawnWorld();
	if (bAIVsAIMode)
	{
		SetupSpectatorCamera();
	}
}

void ARDGameMode::SpawnWorld()
{
	UWorld* World = GetWorld();
	if (!World) return;

	ARDLunarStage::PurgeTemplateEnvironment(World);

	// Lunar exterior
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(World, ARDLunarStage::StaticClass(), Found);
		if (Found.Num() == 0)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			World->SpawnActor<ARDLunarStage>(FVector::ZeroVector, FRotator::ZeroRotator, P);
		}
		else if (ARDLunarStage* Stage = Cast<ARDLunarStage>(Found[0]))
		{
			Stage->BuildStage();
		}
	}

	// Base
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(World, ARDMapBuilder::StaticClass(), Found);
		if (Found.Num() == 0)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			World->SpawnActor<ARDMapBuilder>(FVector::ZeroVector, FRotator::ZeroRotator, P);
		}
		else if (ARDMapBuilder* Builder = Cast<ARDMapBuilder>(Found[0]))
		{
			Builder->BuildBase();
		}
	}

	// Distant Starship (launch-day horizon)
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(World, ARDStarship::StaticClass(), Found);
		for (AActor* A : Found) A->Destroy();
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Distant pad — true-scale stack is ~120 m tall; keep far for silhouette
		Starship = World->SpawnActor<ARDStarship>(FVector(-32000.f, 22000.f, 0.f), FRotator(0.f, 0.f, 0.f), P);
		if (Starship)
		{
			Starship->PadLocation = FVector(-32000.f, 22000.f, 0.f);
		}
	}

	SpawnSquadAndGuards();

	// Player spawn is set inside SpawnSquadAndGuards from map glyph P
}

void ARDGameMode::SpawnSquadAndGuards()
{
	// Port of Fable 5 main.gd spawn: every P/A/R/d/b/r on MoonbaseDelta.logic.txt
	UWorld* World = GetWorld();
	if (!World) return;

	TArray<AActor*> Old;
	UGameplayStatics::GetAllActorsOfClass(World, ARDBot::StaticClass(), Old);
	for (AActor* A : Old) A->Destroy();

	// Match MapBuilder defaults
	const FVector MapOrigin(4000.f, -9900.f, 0.f);
	const float CS = 300.f;

	TArray<FString> Rows;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data/MoonbaseDelta.logic.txt");
	FString Raw;
	if (FFileHelper::LoadFileToString(Raw, *Path))
	{
		Raw.ParseIntoArrayLines(Rows, false);
		Rows.RemoveAll([](const FString& S) { return S.TrimStartAndEnd().IsEmpty(); });
	}
	if (Rows.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[RebelstarDelta] No logic map — cannot spawn 2D roster"));
		return;
	}

	// USA assault team (Trump first). Pool larger than Desired* so auto-balance can grow.
	TArray<FString> AllyNames = {
		TEXT("Trump"), TEXT("Vance"), TEXT("Rubio"), TEXT("Haley"), TEXT("Cruz"), TEXT("Scott")
	};
	TArray<FString> RobotNames = { TEXT("APEX-1"), TEXT("EAGLE-9"), TEXT("PATRIOT-3"), TEXT("HAWK-4") };
	// UK moonbase defence — politicians + agency / mechs
	TArray<FString> OpNames = {
		TEXT("Starmer"), TEXT("Farage"), TEXT("Sunak"), TEXT("Truss"),
		TEXT("Johnson"), TEXT("May"), TEXT("Hunt"), TEXT("Badenoch")
	};
	TArray<FString> SentryNames = { TEXT("GCHQ-1"), TEXT("MI5-2") };
	TArray<FString> DroidNames = { TEXT("Britannia"), TEXT("Lion"), TEXT("Crown") };
	int32 AllyNameI = 0, RobotNameI = 0, OpNameI = 0, SentryI = 0, DroidI = 0;
	int32 NAllyH = 0, NAllyR = 0, NDefH = 0, NDefB = 0, NDefR = 0;
	int32 NMapTroopers = 0, NMapRobots = 0;

	// Clamp desired roster to safe close-run band (never empty either side)
	const int32 CapAllyH = FMath::Clamp(DesiredAllyHumans, 2, AllyNames.Num());
	const int32 CapAllyR = FMath::Clamp(DesiredAllyRobots, 1, RobotNames.Num());
	const int32 CapDefH = FMath::Clamp(DesiredDefHumans, 2, OpNames.Num());
	const int32 CapDefB = FMath::Clamp(DesiredDefSentries, 1, SentryNames.Num());
	const int32 CapDefR = FMath::Clamp(DesiredDefDroids, 1, DroidNames.Num());

	// Open lunar plain WEST of the base — not on map P (that sat inside walls / airlock mesh).
	// Base wall face ~X=5200; LZ is ~140 m west on clear regolith, facing the airlock.
	// Z high enough to clear pad collision slab (~Z=4–12).
	const FVector ApproachLZ(-9000.f, -5850.f, 280.f);

	// XY only — Z is resolved by floor snap (never spawn mid-air / on roof tops)
	auto WorldOf = [&](int32 X, int32 Y) -> FVector
	{
		return MapOrigin + FVector((X + 0.5f) * CS, (Y + 0.5f) * CS, 0.f);
	};

	// Trace down onto FLOOR only (reject wall-tops / roof hits above ~1.5 m)
	auto SnapToFloor = [&](AActor* Actor, const FVector& XY) -> FVector
	{
		if (!Actor) return XY + FVector(0.f, 0.f, 110.f);
		float HalfH = 96.f;
		if (const ARDBot* B = Cast<ARDBot>(Actor))
		{
			if (const UCapsuleComponent* Cap = B->GetCapsuleComponent())
			{
				HalfH = Cap->GetScaledCapsuleHalfHeight();
			}
		}
		else if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			if (const UCapsuleComponent* Cap = Pawn->FindComponentByClass<UCapsuleComponent>())
			{
				HalfH = Cap->GetScaledCapsuleHalfHeight();
			}
		}

		const FVector Start(XY.X, XY.Y, 600.f);
		const FVector End(XY.X, XY.Y, -100.f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RDSnapFloor), false, Actor);
		FHitResult Best;
		bool bFound = false;
		TArray<FHitResult> Hits;
		if (World->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, Params))
		{
			for (const FHitResult& H : Hits)
			{
				// Prefer low surfaces (floor ~0–40 cm). Skip wall tops (~3 m) and roof props.
				if (H.bBlockingHit && H.ImpactPoint.Z < 150.f)
				{
					if (!bFound || H.ImpactPoint.Z > Best.ImpactPoint.Z)
					{
						Best = H;
						bFound = true;
					}
				}
			}
		}
		if (bFound)
		{
			return Best.ImpactPoint + FVector(0.f, 0.f, HalfH + 4.f);
		}
		// Fallback indoor floor: slab top ~15 cm + capsule
		return FVector(XY.X, XY.Y, HalfH + 20.f);
	};

	auto SpawnUnit = [&](const FVector& Loc, ERDBotTeam InTeam, ERDUnitRole InRole, const FString& Name) -> ARDBot*
	{
		FActorSpawnParameters P;
		// Adjust so we don't embed in walls then get ejected onto the roof
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		// Start high then snap down — avoids spawning inside wall volumes
		const FVector Air(Loc.X, Loc.Y, 500.f);
		ARDBot* Bot = World->SpawnActor<ARDBot>(Air, FRotator(0.f, 0.f, 0.f), P);
		if (!Bot) return nullptr;
		Bot->ConfigureFromRole(InRole, InTeam, Name);
		Bot->Tags.Add(FName(TEXT("RD_Base")));
		const FVector Feet = SnapToFloor(Bot, Loc);
		Bot->SetActorLocation(Feet, false, nullptr, ETeleportType::TeleportPhysics);
		Bot->RehomeHere(); // BeginPlay already set Home from air-spawn height
		UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Spawn %s @ Z=%.0f (floor snap)"), *Name, Feet.Z);
		return Bot;
	};

	// Defenders only at map cells. Attackers counted from map, spawned at ApproachLZ.
	// Moon buggy: not spawned for now (user will hide / rework later).
	for (int32 Y = 0; Y < Rows.Num(); ++Y)
	{
		const FString& Row = Rows[Y];
		for (int32 X = 0; X < Row.Len(); ++X)
		{
			const TCHAR C = Row[X];
			const FVector Loc = WorldOf(X, Y);
			switch (C)
			{
			case TEXT('A'):
				++NMapTroopers;
				break;
			case TEXT('R'):
				++NMapRobots;
				break;
			case TEXT('V'):
				// Intentionally no buggy spawn — parked content later
				break;
			case TEXT('d'):
			{
				// Cap UK humans for close-run balance (map still lists full 2D roster)
				if (NDefH >= CapDefH) break;
				const FString Nm = OpNames.IsValidIndex(OpNameI) ? OpNames[OpNameI++] : TEXT("Operative");
				if (SpawnUnit(Loc, ERDBotTeam::Defender, ERDUnitRole::Operative, Nm)) ++NDefH;
				break;
			}
			case TEXT('b'):
			{
				if (NDefB >= CapDefB) { ++SentryI; break; }
				const FString Nm = SentryNames.IsValidIndex(SentryI)
					? SentryNames[SentryI] : FString::Printf(TEXT("GCHQ-%d"), SentryI + 1);
				++SentryI;
				if (SpawnUnit(Loc, ERDBotTeam::Defender, ERDUnitRole::SentryDroid, Nm)) ++NDefB;
				break;
			}
			case TEXT('r'):
			{
				if (NDefR >= CapDefR) { ++DroidI; break; }
				const FString Nm = DroidNames.IsValidIndex(DroidI)
					? DroidNames[DroidI] : FString::Printf(TEXT("UK-MECH-%d"), DroidI + 1);
				++DroidI;
				if (SpawnUnit(Loc, ERDBotTeam::Defender, ERDUnitRole::PatrolDroid, Nm)) ++NDefR;
				break;
			}
			default:
				break;
			}
		}
	}

	// Attack team at far approach LZ (formation on open plain)
	// Offsets in cm — spread so nothing stacks
	const TArray<FVector> AllyOffsets = {
		FVector(-250.f, -320.f, 0.f),
		FVector(-400.f, 280.f, 0.f),
		FVector(-550.f, -80.f, 0.f),
		FVector(-700.f, 420.f, 0.f),
		FVector(-850.f, -450.f, 0.f),
		FVector(-1000.f, 150.f, 0.f),
		FVector(-1150.f, -200.f, 0.f),
		FVector(-1300.f, 350.f, 0.f),
		FVector(-1450.f, -100.f, 0.f),
		FVector(-1600.f, 250.f, 0.f),
	};
	int32 AllySlot = 0;
	// Close-run: use Desired* caps (not full map glyph counts)
	const int32 TrooperCount = CapAllyH;
	const int32 RobotCount = CapAllyR;
	for (int32 i = 0; i < TrooperCount; ++i)
	{
		const FString Nm = AllyNames.IsValidIndex(AllyNameI) ? AllyNames[AllyNameI++] : TEXT("Trooper");
		const FVector Off = AllyOffsets.IsValidIndex(AllySlot) ? AllyOffsets[AllySlot] : FVector(-200.f * (i + 1), 0.f, 0.f);
		++AllySlot;
		if (SpawnUnit(ApproachLZ + Off, ERDBotTeam::Ally, ERDUnitRole::Trooper, Nm)) ++NAllyH;
	}
	for (int32 i = 0; i < RobotCount; ++i)
	{
		const FString Nm = RobotNames.IsValidIndex(RobotNameI) ? RobotNames[RobotNameI++] : TEXT("ROBOT");
		const FVector Off = AllyOffsets.IsValidIndex(AllySlot) ? AllyOffsets[AllySlot] : FVector(-200.f * (i + 4), 200.f, 0.f);
		++AllySlot;
		if (SpawnUnit(ApproachLZ + Off, ERDBotTeam::Ally, ERDUnitRole::AssaultRobot, Nm)) ++NAllyR;
	}

	// Player at LZ = TRUMP (leader). AI-vs-AI: spectator cam; FPS: first-person Trump.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			// Teleport with sweep off so we never start inside a blocking volume
			Pawn->SetActorLocation(ApproachLZ, false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(FRotator(-6.f, 0.f, 0.f)); // yaw 0 = +X toward base / Earthrise
			// One more push upward if still overlapping (pad / prop)
			FHitResult Hit;
			Pawn->SetActorLocation(ApproachLZ + FVector(0.f, 0.f, 80.f), true, &Hit, ETeleportType::TeleportPhysics);
			if (Hit.bBlockingHit)
			{
				Pawn->SetActorLocation(ApproachLZ + FVector(0.f, 0.f, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
			if (ARDCharacter* Leader = Cast<ARDCharacter>(Pawn))
			{
				Leader->LeaderName = TEXT("Trump");
				Leader->CountryCode = TEXT("US");
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Attack LZ at %s — US team led by Trump"), *ApproachLZ.ToCompactString());

	// ISAAC door guard = nearest sentry droid to the door (2D choke robot)
	const FVector DoorLoc = IsaacDoor ? IsaacDoor->GetActorLocation() : FVector(13450.f, -6750.f, 0.f);
	const FVector Standby = IsaacDoor ? IsaacDoor->StandbyPoint : (DoorLoc + FVector(0.f, -450.f, 160.f));
	const FVector BlockPt = IsaacDoor ? IsaacDoor->BlockPoint : (DoorLoc + FVector(0.f, 0.f, 160.f));
	IsaacDoorStandbyPoint = Standby;
	IsaacDoorBlockPoint = BlockPt;

	TArray<AActor*> AllBots;
	UGameplayStatics::GetAllActorsOfClass(World, ARDBot::StaticClass(), AllBots);
	ARDBot* BestGuard = nullptr;
	float BestD = 1.e12f;
	for (AActor* A : AllBots)
	{
		ARDBot* B = Cast<ARDBot>(A);
		if (!B || B->Team != ERDBotTeam::Defender || !B->bIsRobot) continue;
		// Prefer sentries for door duty
		const float Bias = (B->UnitRole == ERDUnitRole::SentryDroid) ? 0.f : 800.f;
		const float D = FVector::Dist(B->GetActorLocation(), DoorLoc) + Bias;
		if (D < BestD) { BestD = D; BestGuard = B; }
	}
	if (BestGuard)
	{
		const FVector GuardStand = SnapToFloor(BestGuard, Standby);
		const FVector GuardBlock = SnapToFloor(BestGuard, BlockPt);
		BestGuard->AssignDoorGuard(GuardStand, GuardBlock);
		BestGuard->SetActorLocation(GuardStand, false, nullptr, ETeleportType::TeleportPhysics);
		DoorGuardBot = BestGuard;
		UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Door guard = %s at floor %s"),
			*BestGuard->UnitName, *GuardStand.ToCompactString());
	}

	// Safety pass: any defender still high up (on wall-top / roof) — re-snap to floor
	UGameplayStatics::GetAllActorsOfClass(World, ARDBot::StaticClass(), AllBots);
	for (AActor* A : AllBots)
	{
		if (ARDBot* B = Cast<ARDBot>(A))
		{
			if (B->Team != ERDBotTeam::Defender || B->bDoorGuard) continue;
			if (B->GetActorLocation().Z > 180.f)
			{
				const FVector Fixed = SnapToFloor(B, B->GetActorLocation());
				B->SetActorLocation(Fixed, false, nullptr, ETeleportType::TeleportPhysics);
				B->RehomeHere();
				UE_LOG(LogTemp, Warning, TEXT("[RebelstarDelta] Re-snapped roof spawn %s → Z=%.0f"),
					*B->UnitName, Fixed.Z);
			}
		}
	}

	// Pick approach / defense plan for this round (learned weights)
	if (bAIVsAIMode)
	{
		SelectTacticsForRound();
	}

	// Refresh list so every ally gets orders (door-guard pass may have stale set)
	UGameplayStatics::GetAllActorsOfClass(World, ARDBot::StaticClass(), AllBots);
	for (AActor* A : AllBots)
	{
		if (ARDBot* B = Cast<ARDBot>(A))
		{
			if (B->Team == ERDBotTeam::Ally)
			{
				// AI-vs-AI: storm the base. FPS mode: follow the leader.
				B->SetOrder(bAIVsAIMode ? ERDSquadOrder::Storm : ERDSquadOrder::Follow);
				B->Order = bAIVsAIMode ? ERDSquadOrder::Storm : ERDSquadOrder::Follow;
				if (bAIVsAIMode)
				{
					// Sprint + hotter guns so spectator rounds resolve in minutes not hours
					// Learning multipliers stack across rematches (self-improve)
					// Robots lead the breach; humans stay slightly slower (kite / support)
					const float Sp = (B->bIsRobot ? 520.f : 700.f) * LearnedAllySpeedMul;
					B->MoveSpeed = Sp;
					B->FireInterval = FMath::Max(0.18f, B->FireInterval * 0.55f * LearnedAllyFireMul);
					B->ShotDamage *= 1.35f * LearnedAllyDamageMul;
					B->WeaponRange *= B->bIsRobot ? 1.05f : 1.2f; // humans prefer standoff
					if (UCharacterMovementComponent* Mov = B->GetCharacterMovement())
					{
						Mov->MaxWalkSpeed = B->MoveSpeed;
					}
				}
			}
			else if (bAIVsAIMode)
			{
				// Defenders slightly sharper too (fair fight, not steamroll)
				B->FireInterval = FMath::Max(0.28f, B->FireInterval * 0.7f * LearnedDefFireMul);
				B->SightRange *= 1.15f;
				B->MoveSpeed *= LearnedDefSpeedMul;
				if (UCharacterMovementComponent* Mov = B->GetCharacterMovement())
				{
					Mov->MaxWalkSpeed = B->MoveSpeed;
				}
			}
		}
	}

	if (bAIVsAIMode)
	{
		StatusNote(FString::Printf(
			TEXT("R%d  US %d (%dH+%dR) vs UK %d (%d+%d+%d) | %s vs %s"),
			RoundIndex, NAllyH + NAllyR, NAllyH, NAllyR,
			NDefH + NDefB + NDefR, NDefH, NDefB, NDefR,
			*GetApproachName(), *GetDefenseName()));
	}
	else
	{
		StatusNote(FString::Printf(
			TEXT("Roster US %dH+%dR | UK %d ops + %d sentries + %d droids"),
			NAllyH, NAllyR, NDefH, NDefB, NDefR));
	}
	UE_LOG(LogTemp, Log,
		TEXT("[RebelstarDelta] Balanced spawn US %dH+%dR=%d | UK %d+%d+%d=%d  AIvsAI=%d"),
		NAllyH, NAllyR, NAllyH + NAllyR, NDefH, NDefB, NDefR, NDefH + NDefB + NDefR, bAIVsAIMode ? 1 : 0);
}

FString ARDGameMode::GetApproachName() const
{
	switch (CurrentApproach)
	{
	case ERDApproachRoute::NorthFlank: return TEXT("NORTH FLANK");
	case ERDApproachRoute::SouthFlank: return TEXT("SOUTH FLANK");
	default: return TEXT("AIRLOCK");
	}
}

FString ARDGameMode::GetDefenseName() const
{
	switch (CurrentDefense)
	{
	case ERDDefensePlan::RushAirlock: return TEXT("RUSH AIRLOCK");
	case ERDDefensePlan::SplitGuard: return TEXT("SPLIT GUARD");
	default: return TEXT("HOLD DEEP");
	}
}

FVector ARDGameMode::GetApproachWaypoint(int32 FormationSlot) const
{
	const float Spread = (FormationSlot - 1) * 90.f;
	switch (CurrentApproach)
	{
	case ERDApproachRoute::NorthFlank:
		// Northern open approach then east into base
		return FVector(5400.f, -7600.f + Spread, 120.f);
	case ERDApproachRoute::SouthFlank:
		// Southern / workshop-side approach
		return FVector(5400.f, -3800.f + Spread, 120.f);
	default:
		// Classic P-row airlock mouth
		return FVector(5300.f, -5850.f + Spread * 0.7f, 120.f);
	}
}

FVector ARDGameMode::GetDefenseRally(const ARDBot* Bot) const
{
	if (!Bot) return FVector(9000.f, -5850.f, 120.f);
	const int32 Slot = Bot->GetUniqueID() % 3;
	switch (CurrentDefense)
	{
	case ERDDefensePlan::RushAirlock:
		// Meet the assault at the west mouth
		return FVector(5600.f + Slot * 120.f, -5850.f + (Slot - 1) * 200.f, 120.f);
	case ERDDefensePlan::SplitGuard:
		// Even units west, odd hold ISAAC corridor
		if (Slot == 0) return FVector(7000.f, -5850.f, 120.f);
		if (Slot == 1) return FVector(12000.f, -6750.f, 120.f); // near ISAAC door
		return Bot->GetActorLocation(); // keep post
	default:
		return Bot->GetActorLocation(); // hold deep / home
	}
}

int32 ARDGameMode::PickWeightedIndex(const float* Scores, int32 Num, float Explore) const
{
	if (Num <= 0) return 0;
	// Epsilon explore: try a random route sometimes so we keep learning
	if (FMath::FRand() < Explore)
	{
		return FMath::RandRange(0, Num - 1);
	}
	float Sum = 0.f;
	for (int32 i = 0; i < Num; ++i) Sum += FMath::Max(0.05f, Scores[i]);
	float R = FMath::FRand() * Sum;
	for (int32 i = 0; i < Num; ++i)
	{
		R -= FMath::Max(0.05f, Scores[i]);
		if (R <= 0.f) return i;
	}
	return Num - 1;
}

void ARDGameMode::SelectTacticsForRound()
{
	const int32 Ai = PickWeightedIndex(ApproachScore, 3, 0.22f);
	const int32 Di = PickWeightedIndex(DefenseScore, 3, 0.22f);
	CurrentApproach = static_cast<ERDApproachRoute>(Ai);
	CurrentDefense = static_cast<ERDDefensePlan>(Di);
	UE_LOG(LogTemp, Warning, TEXT("[RebelstarDelta] TACTICS R%d  US=%s (scores %.2f/%.2f/%.2f)  UK=%s (%.2f/%.2f/%.2f)"),
		RoundIndex, *GetApproachName(),
		ApproachScore[0], ApproachScore[1], ApproachScore[2],
		*GetDefenseName(),
		DefenseScore[0], DefenseScore[1], DefenseScore[2]);
}

void ARDGameMode::CountTeams(int32& OutAllies, int32& OutDefenders) const
{
	OutAllies = 0;
	OutDefenders = 0;
	UWorld* World = GetWorld();
	if (!World) return;
	TArray<AActor*> AllBots;
	UGameplayStatics::GetAllActorsOfClass(World, ARDBot::StaticClass(), AllBots);
	for (AActor* A : AllBots)
	{
		const ARDBot* B = Cast<ARDBot>(A);
		if (!B || B->IsDead()) continue;
		if (B->Team == ERDBotTeam::Ally) ++OutAllies;
		else ++OutDefenders;
	}
}

int32 ARDGameMode::GetAliveAllies() const
{
	int32 A = 0, D = 0;
	CountTeams(A, D);
	return A;
}

int32 ARDGameMode::GetAliveDefenders() const
{
	int32 A = 0, D = 0;
	CountTeams(A, D);
	return D;
}

void ARDGameMode::NotifyBotKilled(ARDBot* Bot)
{
	if (!Bot || bMatchOver) return;
	if (Bot->Team == ERDBotTeam::Defender) ++AttackerKills;
	else ++DefenderKills;
}

void ARDGameMode::SetupSpectatorCamera()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	Pawn->Tags.AddUnique(FName(TEXT("RD_Spectator")));
	// Front of base = WEST airlock facade. Camera sits west looking east into the fight.
	const FVector CamLoc(1200.f, -5850.f, 2400.f);
	const FVector LookAt(7500.f, -5850.f, 200.f); // airlock mouth + first corridors
	Pawn->SetActorLocation(CamLoc, false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation((LookAt - CamLoc).Rotation());
	SpecOrbitYaw = 0.f;

	if (ACharacter* Ch = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Mov = Ch->GetCharacterMovement())
		{
			Mov->GravityScale = 0.f;
			Mov->SetMovementMode(MOVE_Flying);
			Mov->MaxFlySpeed = 2400.f;
			Mov->BrakingDecelerationFlying = 4000.f;
		}
		if (UCapsuleComponent* Cap = Ch->GetCapsuleComponent())
		{
			Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		// Hide leader mesh so it does not clutter the view
		TArray<UStaticMeshComponent*> Meshes;
		Ch->GetComponents<UStaticMeshComponent>(Meshes);
		for (UStaticMeshComponent* M : Meshes)
		{
			if (M) M->SetVisibility(false, true);
		}
		if (USkeletalMeshComponent* Sk = Ch->GetMesh())
		{
			Sk->SetVisibility(false, true);
		}
	}
	if (ARDCharacter* Leader = Cast<ARDCharacter>(Pawn))
	{
		Leader->SetSpectatorInvulnerable(true);
	}
	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] Spectator FRONT cam @ %s → airlock"), *CamLoc.ToCompactString());
}

void ARDGameMode::TickSpectatorCamera(float DeltaSeconds)
{
	if (!bAIVsAIMode || bMatchOver) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	// Hold a WEST→EAST “front of base” angle (airlock is the action).
	// Gentle sway/dolly so the shot stays alive without spinning behind the base.
	SpecOrbitYaw += DeltaSeconds;

	// Focus: airlock / west halls (combat approach). Nudge toward live units near the front.
	FVector Focus(7000.f, -5850.f, 180.f);
	int32 N = 0;
	FVector Sum = FVector::ZeroVector;
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	for (AActor* A : Bots)
	{
		const ARDBot* B = Cast<ARDBot>(A);
		if (!B || B->IsDead()) continue;
		const FVector L = B->GetActorLocation();
		// Only units near the western front / approach (not deep east ISAAC corner only)
		if (L.X > -2000.f && L.X < 16000.f)
		{
			Sum += L;
			++N;
		}
	}
	if (N > 0)
	{
		const FVector Combat = Sum / float(N);
		// Bias look toward combat but keep focus on front facade
		Focus = FMath::Lerp(Focus, FVector(Combat.X, Combat.Y, 160.f), 0.55f);
		Focus.X = FMath::Clamp(Focus.X, 4500.f, 12000.f);
		Focus.Y = FMath::Clamp(Focus.Y, -8500.f, -2500.f);
	}

	const float SwayY = FMath::Sin(SpecOrbitYaw * 0.28f) * 900.f;
	const float DollyX = FMath::Sin(SpecOrbitYaw * 0.18f) * 500.f;
	const float BobZ = FMath::Sin(SpecOrbitYaw * 0.4f) * 180.f;

	// Camera stays WEST of the wall (~X 5200), looking EAST at the front
	const FVector CamLoc(
		FMath::Clamp(800.f + DollyX, -500.f, 3200.f),
		Focus.Y + SwayY * 0.35f - 200.f,
		FMath::Clamp(2200.f + BobZ + (Focus.X - 5000.f) * 0.05f, 1600.f, 3600.f));

	Pawn->SetActorLocation(CamLoc, false, nullptr, ETeleportType::TeleportPhysics);
	const FRotator LookRot = (Focus + FVector(0.f, 0.f, 80.f) - CamLoc).Rotation();
	PC->SetControlRotation(LookRot);
}

void ARDGameMode::TickBattleMonitor(float DeltaSeconds)
{
	int32 Allies = 0, Defs = 0;
	CountTeams(Allies, Defs);

	// Wipe checks (leader is spectator — not part of combat roster)
	if (bAIVsAIMode && !bMatchOver)
	{
		if (Allies <= 0)
		{
			EndMatch(false, FString::Printf(
				TEXT("SQUAD WIPED — defenders hold ISAAC. Round %d  Kills A%d/D%d"),
				RoundIndex, AttackerKills, DefenderKills));
			return;
		}
	}

	// Track deepest US push for post-mortem even if they die later
	{
		TArray<AActor*> Bots;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
		for (AActor* A : Bots)
		{
			if (const ARDBot* B = Cast<ARDBot>(A))
			{
				if (B->Team == ERDBotTeam::Ally && !B->IsDead())
				{
					PeakAllyX = FMath::Max(PeakAllyX, B->GetActorLocation().X);
				}
			}
		}
	}

	MonitorLogTimer -= DeltaSeconds;
	if (MonitorLogTimer <= 0.f)
	{
		MonitorLogTimer = 4.f;
		if (Allies != LastAliveAllies || Defs != LastAliveDefs)
		{
			LastAliveAllies = Allies;
			LastAliveDefs = Defs;
			const FString Line = FString::Printf(
				TEXT("[Battle] R%d  US %d vs UK %d  kills %d/%d  ISAAC %d  peakX=%.0f door=%d"),
				RoundIndex, Allies, Defs, AttackerKills, DefenderKills,
				GetIsaacAliveCount(), PeakAllyX, bIsaacDoorBreached ? 1 : 0);
			UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] %s"), *Line);
			if (bAIVsAIMode)
			{
				StatusMessage = FString::Printf(
					TEXT("R%d  US(Trump) %d k%d  |  UK %d k%d  |  ISAAC %d  W %d-%d"),
					RoundIndex, Allies, AttackerKills, Defs, DefenderKills,
					GetIsaacAliveCount(), AttackerWins, DefenderWins);
			}
		}
	}
}

void ARDGameMode::RestartMatch()
{
	UE_LOG(LogTemp, Warning, TEXT("[RebelstarDelta] === REMATCH round %d → %d (A wins %d / D wins %d) ==="),
		RoundIndex, RoundIndex + 1, AttackerWins, DefenderWins);

	++RoundIndex;
	bMatchOver = false;
	bAttackersWin = false;
	bEndingCinematic = false;
	bAlarm = false;
	bIsaacDoorBreached = false;
	bDoorWreckageActive = false;
	TimeLeft = MatchDuration;
	EndingTime = 0.f;
	AutoAlarmTimer = AutoAlarmDelay;
	RematchTimer = -1.f;
	AttackerKills = 0;
	DefenderKills = 0;
	LastAliveAllies = -1;
	LastAliveDefs = -1;
	PeakAllyX = -9000.f;
	IsaacNodes.Reset();
	IsaacDoor = nullptr;
	DoorGuardBot = nullptr;
	DoorWreckage = nullptr;

	UWorld* World = GetWorld();
	if (!World) return;

	// Clear combat actors; rebuild base geometry (also clears RD_Base tags)
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(World, ARDBot::StaticClass(), Bots);
	for (AActor* A : Bots) if (A) A->Destroy();
	TArray<AActor*> Wrecks;
	UGameplayStatics::GetAllActorsOfClass(World, ARDWreckage::StaticClass(), Wrecks);
	for (AActor* A : Wrecks) if (A) A->Destroy();
	TArray<AActor*> Doors;
	UGameplayStatics::GetAllActorsOfClass(World, ARDRoomDoor::StaticClass(), Doors);
	for (AActor* A : Doors) if (A) A->Destroy();
	TArray<AActor*> Isaacs;
	UGameplayStatics::GetAllActorsOfClass(World, ARDIsaacNode::StaticClass(), Isaacs);
	for (AActor* A : Isaacs) if (A) A->Destroy();

	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(World, ARDMapBuilder::StaticClass(), Found);
		if (Found.Num() == 0)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			World->SpawnActor<ARDMapBuilder>(FVector::ZeroVector, FRotator::ZeroRotator, P);
		}
		else if (ARDMapBuilder* Builder = Cast<ARDMapBuilder>(Found[0]))
		{
			Builder->BuildBase();
		}
	}

	SpawnSquadAndGuards();
	if (bAIVsAIMode)
	{
		SetupSpectatorCamera();
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}
	StatusNote(FString::Printf(TEXT("ROUND %d — AI assault resumes"), RoundIndex));
}

void ARDGameMode::RaiseAlarm(const FVector& AtLocation)
{
	if (bAlarm || bMatchOver) return;
	bAlarm = true;
	StatusMessage = TEXT("ALARM! Base is awake — defenders hunting.");
	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] ALARM at %s"), *AtLocation.ToString());
}

void ARDGameMode::StatusNote(const FString& Message)
{
	StatusMessage = Message;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(8810, 4.5f, FColor::Orange, Message);
	}
	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] %s"), *Message);
}

void ARDGameMode::RegisterIsaacDoor(ARDRoomDoor* Door)
{
	if (!Door) return;
	IsaacDoor = Door;
	IsaacDoorStandbyPoint = Door->StandbyPoint;
	IsaacDoorBlockPoint = Door->BlockPoint;
	UE_LOG(LogTemp, Log, TEXT("[RebelstarDelta] ISAAC door registered at %s"), *Door->GetActorLocation().ToCompactString());
}

void ARDGameMode::NotifyIsaacDoorBreached(ARDRoomDoor* Door)
{
	if (bIsaacDoorBreached) return;
	bIsaacDoorBreached = true;
	RaiseAlarm(Door ? Door->GetActorLocation() : FVector::ZeroVector);
	StatusNote(TEXT("ISAAC DOOR BREACHED — defender robot moving to block the doorway!"));

	if (IsValid(DoorGuardBot) && !DoorGuardBot->IsDead())
	{
		const FVector BlockPt = Door ? Door->BlockPoint : IsaacDoorBlockPoint;
		DoorGuardBot->CommandBlockDoorway(BlockPt);
	}
	else
	{
		// No living guard — wreckage immediately (rare)
		SpawnDoorWreckage(Door ? Door->GetActorLocation() : IsaacDoorBlockPoint);
	}
}

void ARDGameMode::NotifyDoorGuardKilled(ARDBot* Bot)
{
	if (!Bot || !Bot->bDoorGuard) return;
	if (bDoorWreckageActive) return;
	StatusNote(TEXT("Door guard down — WRECKAGE seals the doorway!"));
	SpawnDoorWreckage(Bot->GetActorLocation());
}

void ARDGameMode::SpawnDoorWreckage(const FVector& At)
{
	UWorld* World = GetWorld();
	if (!World || bDoorWreckageActive) return;

	const FVector Loc = IsaacDoor ? IsaacDoor->GetActorLocation() : At;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	DoorWreckage = World->SpawnActor<ARDWreckage>(Loc, FRotator::ZeroRotator, P);
	bDoorWreckageActive = DoorWreckage != nullptr;
	if (DoorWreckage)
	{
		// AFTER BeginPlay — configure door choke (not default robot hulk)
		DoorWreckage->ConfigureAsDoorChoke();
		DoorWreckage->Tags.Add(FName(TEXT("RD_Base")));
	}
}

void ARDGameMode::RegisterIsaac(ARDIsaacNode* Node)
{
	if (Node && !IsaacNodes.Contains(Node))
	{
		IsaacNodes.Add(Node);
	}
}

int32 ARDGameMode::GetIsaacAliveCount() const
{
	int32 N = 0;
	for (const ARDIsaacNode* Node : IsaacNodes)
	{
		if (IsValid(Node) && !Node->IsDestroyed()) ++N;
	}
	return N;
}

void ARDGameMode::NotifyIsaacDestroyed(ARDIsaacNode* Node)
{
	if (bMatchOver) return;
	RaiseAlarm(Node ? Node->GetActorLocation() : FVector::ZeroVector);
	const int32 Alive = GetIsaacAliveCount();
	StatusMessage = FString::Printf(TEXT("ISAAC core destroyed! Remaining: %d"), Alive);
	if (Alive <= 0 && IsaacNodes.Num() > 0)
	{
		EndMatch(true, TEXT("ISAAC OFFLINE — Moonbase Delta secured."));
	}
}

void ARDGameMode::NotifyLeaderKilled()
{
	if (bMatchOver) return;
	EndMatch(false, TEXT("LEADER DOWN — mission failed."));
}

void ARDGameMode::EndMatch(bool bWin, const FString& Message)
{
	if (bMatchOver) return;
	bMatchOver = true;
	bAttackersWin = bWin;
	StatusMessage = Message;
	if (bWin) ++AttackerWins;
	else ++DefenderWins;
	UE_LOG(LogTemp, Warning, TEXT("[RebelstarDelta] END R%d %s | score A%d-D%d | %s"),
		RoundIndex, bWin ? TEXT("US / TRUMP") : TEXT("UK DEFENCE"),
		AttackerWins, DefenderWins, *Message);

	// Always analyze — especially draws / timeouts / stalemates (2D self-improve spirit)
	AnalyzeBattleAndLearn(bWin, Message);

	if (bAIVsAIMode)
	{
		// Analyze may set RematchTimer < 0 when training series is complete
		if (RematchTimer >= 0.f || (TargetTrainingRounds > 0 && RoundIndex < TargetTrainingRounds))
		{
			if (TargetTrainingRounds <= 0 || RoundIndex < TargetTrainingRounds)
			{
				RematchTimer = RematchDelay;
			}
			else
			{
				RematchTimer = -1.f;
			}
		}
		// Skip long cinematic in spectator loop — quick scoreboard then rematch
		bEndingCinematic = false;
		return;
	}
	if (bWin) StartEndingCinematic();
}

void ARDGameMode::AnalyzeBattleAndLearn(bool bWin, const FString& Message)
{
	UWorld* World = GetWorld();
	if (!World) return;

	int32 Allies = 0, Defs = 0;
	CountTeams(Allies, Defs);

	int32 TotalStuck = 0;
	float MaxAllyX = PeakAllyX; // tracked live during the round
	float AvgAllyX = 0.f;
	int32 AllySamples = 0;
	TArray<FString> StuckNames;
	TArray<AActor*> AllBots;
	UGameplayStatics::GetAllActorsOfClass(World, ARDBot::StaticClass(), AllBots);
	for (AActor* A : AllBots)
	{
		ARDBot* B = Cast<ARDBot>(A);
		if (!B) continue;
		const int32 SE = B->GetStuckEvents();
		// Only count attacker stuck events for learning (defenders often hold post)
		if (B->Team == ERDBotTeam::Ally)
		{
			TotalStuck += SE;
			if (SE >= 3)
			{
				StuckNames.Add(FString::Printf(TEXT("%s x%d"), *B->UnitName, SE));
			}
			const float X = B->GetActorLocation().X;
			MaxAllyX = FMath::Max(MaxAllyX, X);
			AvgAllyX += X;
			++AllySamples;
		}
	}
	if (AllySamples > 0) AvgAllyX /= AllySamples;

	const bool bTimeout = Message.Contains(TEXT("TIME"));
	const bool bWipe = Message.Contains(TEXT("WIPED")) || Message.Contains(TEXT("LEADER"));
	const bool bIsaacWin = bWin && Message.Contains(TEXT("ISAAC"));
	const bool bNoWinner = bTimeout || (!bWin && !bWipe) || (bTimeout);
	const bool bAirlockJam = MaxAllyX > 0.f && MaxAllyX < 7000.f && !bIsaacDoorBreached;
	const bool bStalemate = !bWin && Allies > 0 && Defs > 0 && AttackerKills + DefenderKills < 3;

	FString Diagnosis;
	if (bIsaacWin) Diagnosis = TEXT("Clean win — ISAAC destroyed");
	else if (bWipe) Diagnosis = TEXT("Attack squad eliminated");
	else if (bAirlockJam) Diagnosis = TEXT("STUCK near airlock / west approach — pathing jam");
	else if (bStalemate) Diagnosis = TEXT("STALEMATE — low kills, neither side closing");
	else if (bTimeout) Diagnosis = TEXT("TIMEOUT — ISAAC held, no decisive breach");
	else Diagnosis = TEXT("Defenders held the base");

	// --- Tactical score update (different ways into the base) ---
	const int32 Ai = FMath::Clamp(static_cast<int32>(CurrentApproach), 0, 2);
	const int32 Di = FMath::Clamp(static_cast<int32>(CurrentDefense), 0, 2);
	if (bIsaacWin)
	{
		ApproachScore[Ai] = FMath::Min(4.f, ApproachScore[Ai] * 1.35f + 0.15f);
		DefenseScore[Di] = FMath::Max(0.25f, DefenseScore[Di] * 0.75f);
		Diagnosis += FString::Printf(TEXT(" | TACTIC: US %s WORKED"), *GetApproachName());
	}
	else
	{
		ApproachScore[Ai] = FMath::Max(0.25f, ApproachScore[Ai] * 0.82f);
		DefenseScore[Di] = FMath::Min(4.f, DefenseScore[Di] * 1.28f + 0.1f);
		// If airlock jammed, boost flank scores so next pick explores
		if (bAirlockJam || (Ai == 0 && TotalStuck > 8))
		{
			ApproachScore[1] = FMath::Min(4.f, ApproachScore[1] * 1.2f + 0.1f);
			ApproachScore[2] = FMath::Min(4.f, ApproachScore[2] * 1.2f + 0.1f);
			Diagnosis += TEXT(" | TACTIC: try flanks next");
		}
		else
		{
			Diagnosis += FString::Printf(TEXT(" | TACTIC: UK %s held"), *GetDefenseName());
		}
	}
	// Softmax-ish normalize so scores stay comparable
	{
		float ASum = ApproachScore[0] + ApproachScore[1] + ApproachScore[2];
		float DSum = DefenseScore[0] + DefenseScore[1] + DefenseScore[2];
		if (ASum > 0.f) for (int32 i = 0; i < 3; ++i) ApproachScore[i] = ApproachScore[i] / ASum * 3.f;
		if (DSum > 0.f) for (int32 i = 0; i < 3; ++i) DefenseScore[i] = DefenseScore[i] / DSum * 3.f;
	}

	// Auto roster balance — keep the series a close run (not 5v13 stomps)
	if (bAutoBalanceRosters && (AttackerWins + DefenderWins) >= 2)
	{
		const int32 Lead = AttackerWins - DefenderWins;
		if (Lead >= 2)
		{
			// US running away → add UK or trim US slightly
			if (DesiredDefHumans < 6) { ++DesiredDefHumans; Diagnosis += TEXT(" | BAL: +1 UK op"); }
			else if (DesiredDefDroids < 3) { ++DesiredDefDroids; Diagnosis += TEXT(" | BAL: +1 UK mech"); }
			else if (DesiredAllyHumans > 3) { --DesiredAllyHumans; Diagnosis += TEXT(" | BAL: -1 US human"); }
		}
		else if (Lead <= -2)
		{
			// UK running away → reinforce US or thin UK
			if (DesiredAllyHumans < 5) { ++DesiredAllyHumans; Diagnosis += TEXT(" | BAL: +1 US human"); }
			else if (DesiredAllyRobots < 3) { ++DesiredAllyRobots; Diagnosis += TEXT(" | BAL: +1 US mech"); }
			else if (DesiredDefHumans > 3) { --DesiredDefHumans; Diagnosis += TEXT(" | BAL: -1 UK op"); }
		}
		// Soft clamp totals ~5–8 each side
		DesiredAllyHumans = FMath::Clamp(DesiredAllyHumans, 2, 6);
		DesiredAllyRobots = FMath::Clamp(DesiredAllyRobots, 1, 4);
		DesiredDefHumans = FMath::Clamp(DesiredDefHumans, 2, 6);
		DesiredDefSentries = FMath::Clamp(DesiredDefSentries, 1, 2);
		DesiredDefDroids = FMath::Clamp(DesiredDefDroids, 1, 3);
	}

	// Self-improve every failed / non-ISAAC outcome (training series)
	if (!bIsaacWin)
	{
		++StalemateStreak;
		// Always nudge attackers after a loss so 20-round series actually climbs
		LearnedAllySpeedMul = FMath::Min(2.4f, LearnedAllySpeedMul * 1.08f);
		LearnedAllyDamageMul = FMath::Min(2.5f, LearnedAllyDamageMul * 1.08f);
		LearnedAllyFireMul = FMath::Max(0.35f, LearnedAllyFireMul * 0.94f);
		Diagnosis += TEXT(" | LEARN: base climb");

		if (bAirlockJam || TotalStuck > 5)
		{
			LearnedAllySpeedMul = FMath::Min(2.4f, LearnedAllySpeedMul * 1.1f);
			LearnedAllyFireMul = FMath::Max(0.35f, LearnedAllyFireMul * 0.9f);
			Diagnosis += TEXT(" | LEARN: airlock/stuck boost");
		}
		if (bStalemate || bTimeout)
		{
			LearnedAllyDamageMul = FMath::Min(2.5f, LearnedAllyDamageMul * 1.12f);
			LearnedDefFireMul = FMath::Max(0.4f, LearnedDefFireMul * 0.92f);
			Diagnosis += TEXT(" | LEARN: stalemate break");
		}
		if (bWipe && AttackerKills < 2)
		{
			// Soften UK slightly if US wiped with almost no kills
			LearnedDefFireMul = FMath::Max(0.4f, LearnedDefFireMul * 0.9f);
			LearnedDefSpeedMul = FMath::Max(0.75f, LearnedDefSpeedMul * 0.95f);
			Diagnosis += TEXT(" | LEARN: UK pressure down");
		}
		if (bIsaacDoorBreached && !bIsaacWin)
		{
			// Got door open but failed cores — more ISAAC focus (damage)
			LearnedAllyDamageMul = FMath::Min(2.5f, LearnedAllyDamageMul * 1.1f);
			Diagnosis += TEXT(" | LEARN: post-breach damage");
		}
		if (StalemateStreak >= 3)
		{
			LearnedAllySpeedMul = FMath::Min(2.5f, LearnedAllySpeedMul * 1.12f);
			LearnedAllyDamageMul = FMath::Min(2.6f, LearnedAllyDamageMul * 1.12f);
			Diagnosis += TEXT(" | LEARN: streak boost");
		}
	}
	else
	{
		StalemateStreak = 0;
		// Slight rebalance so wins don't snowball forever
		LearnedAllySpeedMul = FMath::Max(1.f, LearnedAllySpeedMul * 0.96f);
		LearnedAllyDamageMul = FMath::Max(1.f, LearnedAllyDamageMul * 0.96f);
		LearnedDefFireMul = FMath::Min(1.15f, LearnedDefFireMul * 1.03f);
	}

	const FString Report = FString::Printf(
		TEXT("=== BATTLE REPORT R%d ===\nResult: %s\nUS route: %s (%.2f/%.2f/%.2f)\nUK plan: %s (%.2f/%.2f/%.2f)\n%s\nAlive A%d D%d  Kills A%d D%d\nISAAC alive %d  door=%d wreck=%d\nMaxAllyX=%.0f AvgAllyX=%.0f  StuckEvents=%d\nStuck heavy: %s\nLearning: allySpd=%.2f allyDmg=%.2f allyFire=%.2f defFire=%.2f defSpd=%.2f streak=%d  series %d-%d\n"),
		RoundIndex,
		bWin ? TEXT("US WIN") : TEXT("UK HOLD / US FAIL"),
		*GetApproachName(), ApproachScore[0], ApproachScore[1], ApproachScore[2],
		*GetDefenseName(), DefenseScore[0], DefenseScore[1], DefenseScore[2],
		*Diagnosis,
		Allies, Defs, AttackerKills, DefenderKills,
		GetIsaacAliveCount(), bIsaacDoorBreached ? 1 : 0, bDoorWreckageActive ? 1 : 0,
		MaxAllyX, AvgAllyX, TotalStuck,
		StuckNames.Num() > 0 ? *FString::Join(StuckNames, TEXT(", ")) : TEXT("(none)"),
		LearnedAllySpeedMul, LearnedAllyDamageMul, LearnedAllyFireMul, LearnedDefFireMul, LearnedDefSpeedMul,
		StalemateStreak, AttackerWins, DefenderWins);

	UE_LOG(LogTemp, Warning, TEXT("[RebelstarDelta]\n%s"), *Report);
	StatusMessage = FString::Printf(TEXT("R%d done | W %d-%d | %s"), RoundIndex, AttackerWins, DefenderWins, *Diagnosis);

	// Per-round + rolling series master log for agent training
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("AgentVision");
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Path = Dir / FString::Printf(TEXT("battle_report_R%d.txt"), RoundIndex);
	FFileHelper::SaveStringToFile(Report, *Path);

	const FString CsvLine = FString::Printf(
		TEXT("%d,%s,%s,%s,%d,%d,%d,%d,%d,%d,%.0f,%d,%.3f,%.3f,%.3f,%.3f\n"),
		RoundIndex, bWin ? TEXT("US") : TEXT("UK"),
		*GetApproachName(), *GetDefenseName(),
		Allies, Defs, AttackerKills, DefenderKills,
		bIsaacDoorBreached ? 1 : 0, GetIsaacAliveCount(),
		MaxAllyX, TotalStuck,
		LearnedAllySpeedMul, LearnedAllyDamageMul, LearnedAllyFireMul, LearnedDefFireMul);
	const FString SeriesPath = Dir / TEXT("training_series.csv");
	if (!FPaths::FileExists(SeriesPath))
	{
		FFileHelper::SaveStringToFile(
			TEXT("round,winner,usRoute,ukPlan,aliveA,aliveD,kA,kD,door,isaac,peakX,stuck,spd,dmg,fire,defFire\n"), *SeriesPath);
	}
	FFileHelper::SaveStringToFile(CsvLine, *SeriesPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);

	if (TargetTrainingRounds > 0 && RoundIndex >= TargetTrainingRounds)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RebelstarDelta] TRAINING COMPLETE %d rounds — final score US %d / UK %d"),
			TargetTrainingRounds, AttackerWins, DefenderWins);
		StatusMessage = FString::Printf(TEXT("TRAINING DONE %d rds — US %d UK %d"), TargetTrainingRounds, AttackerWins, DefenderWins);
		// Stop auto rematch after target
		RematchTimer = -1.f;
	}
}

void ARDGameMode::StartEndingCinematic()
{
	bEndingCinematic = true;
	EndingTime = 0.f;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			EndingStartLoc = Pawn->GetActorLocation();
			EndingStartRot = PC->GetControlRotation();
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);
		}
	}
}

FVector ARDGameMode::GetStarshipLookPoint() const
{
	if (IsValid(Starship)) return Starship->GetLookAtPoint();
	return FVector(-28000.f, 18000.f, 2800.f);
}

void ARDGameMode::TickEnding(float DeltaSeconds)
{
	EndingTime += DeltaSeconds;
	const float T = FMath::Clamp(EndingTime / 12.f, 0.f, 1.f);
	const float E = 1.f - FMath::Pow(1.f - T, 2.f);

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	const FVector Ship = GetStarshipLookPoint();
	// Rise up and pull back toward a viewpoint that sees base + Starship + sky
	const FVector EndLoc = FMath::Lerp(EndingStartLoc, EndingStartLoc + FVector(-2500.f, 4000.f, 7500.f), E);
	Pawn->SetActorLocation(EndLoc);

	const FVector ToShip = (Ship - EndLoc).GetSafeNormal();
	const FRotator LookRot = ToShip.Rotation();
	// Blend from player look to Starship framing, slight pitch up for stars
	FRotator TargetRot = LookRot;
	TargetRot.Pitch = FMath::Clamp(TargetRot.Pitch + 8.f, -40.f, 25.f);
	const FRotator Blended = FMath::Lerp(EndingStartRot, TargetRot, E);
	PC->SetControlRotation(Blended);

	if (T >= 1.f)
	{
		StatusMessage = TEXT("Starship on the horizon. Moon in a sea of stars.  Stop Play to restart.");
	}
}

void ARDGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bAIVsAIMode && bMatchOver && RematchTimer >= 0.f)
	{
		RematchTimer -= DeltaSeconds;
		if (RematchTimer <= 0.f)
		{
			RestartMatch();
		}
		return;
	}

	if (bEndingCinematic)
	{
		TickEnding(DeltaSeconds);
		return;
	}
	if (bMatchOver) return;

	// Kick the fight open so AI-vs-AI does not sit in stealth forever
	if (bAIVsAIMode && !bAlarm && AutoAlarmTimer > 0.f)
	{
		AutoAlarmTimer -= DeltaSeconds;
		if (AutoAlarmTimer <= 0.f)
		{
			RaiseAlarm(FVector(9000.f, -5850.f, 100.f));
			StatusNote(TEXT("AUTO-ALARM — AI assault engaged"));
		}
	}

	TickBattleMonitor(DeltaSeconds);
	TickSpectatorCamera(DeltaSeconds);

	TimeLeft = FMath::Max(0.f, TimeLeft - DeltaSeconds);
	if (TimeLeft <= 0.f)
	{
		EndMatch(false, TEXT("TIME EXPIRED — ISAAC still online."));
	}
}
