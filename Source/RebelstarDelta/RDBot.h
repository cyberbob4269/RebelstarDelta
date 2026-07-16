// Rebelstar Delta — squad + enemies (spacesuit humans, US/UK badges)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RDBot.generated.h"

class UPointLightComponent;

UENUM(BlueprintType)
enum class ERDBotTeam : uint8
{
	Ally,
	Defender
};

UENUM(BlueprintType)
enum class ERDSquadOrder : uint8
{
	Follow UMETA(DisplayName = "Follow Me"),
	Attack UMETA(DisplayName = "Attack Enemies"),
	Hold UMETA(DisplayName = "Hold Position"),
	Storm UMETA(DisplayName = "Storm ISAAC")
};

/**
 * Unit roles mirror Fable 5 map glyphs:
 *  P = player leader (not a bot)
 *  A = human ally    R = ally robot
 *  d = operative     b = sentry droid (hold post)   r = patrol droid
 */
UENUM(BlueprintType)
enum class ERDUnitRole : uint8
{
	Trooper,      // A
	AssaultRobot, // R
	Operative,    // d
	SentryDroid,  // b — holds post
	PatrolDroid   // r
};

UCLASS()
class REBELSTARDELTA_API ARDBot : public ACharacter
{
	GENERATED_BODY()

public:
	ARDBot();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	ERDBotTeam Team = ERDBotTeam::Ally;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	ERDSquadOrder Order = ERDSquadOrder::Follow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	ERDUnitRole UnitRole = ERDUnitRole::Trooper;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	FString UnitName;

	/** USA attackers / UK defenders — drives chest flag badge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	FString CountryCode = TEXT("US");

	/** Slightly different suit proportions (men/women in suits). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	bool bFemaleSuit = false;

	/** Gold trim — Trump / VIP leader look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	bool bLeaderSuit = false;

	/** Fable 5: robots are tanky, slow, hit hard, leave wreckage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	bool bIsRobot = false;

	/** Map glyph `b` — hold home post (overwatch) until alarm / door command. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	bool bHoldPost = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float MaxHP = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float MoveSpeed = 520.f;

	/** Shot damage (2D: human 16, robot 34). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float ShotDamage = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float FireInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float WeaponRange = 2800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float SightRange = 2400.f;

	/** ISAAC door choke robot (assigned from nearest sentry). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	bool bDoorGuard = false;

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void ApplyDamage(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	float GetHP() const { return HP; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsRobot() const { return bIsRobot; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetStuckEvents() const { return StuckEvents; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	FString GetCountryCode() const { return CountryCode; }

	void SetTeam(ERDBotTeam InTeam);
	void SetOrder(ERDSquadOrder InOrder);

	/** Apply Fable 5 human/robot baseline stats for role. */
	void ConfigureFromRole(ERDUnitRole InRole, ERDBotTeam InTeam, const FString& InName);

	void AssignDoorGuard(const FVector& Standby, const FVector& BlockPoint);
	void CommandBlockDoorway(const FVector& BlockPoint);
	void SetCurrentHP(float NewHP);

	/** Player is Tab-controlling this unit — skip AI. */
	void SetPlayerDriven(bool bDriven) { bPlayerDriven = bDriven; }
	bool IsPlayerDriven() const { return bPlayerDriven; }

	/** After floor-snap teleport, reset patrol home to current feet. */
	void RehomeHere()
	{
		Home = GetActorLocation();
		HoldPoint = Home;
		WanderTarget = Home;
		if (!bDoorGuard)
		{
			DoorStandbyPoint = Home;
		}
	}

	/** 2D: ~1.2s radio before base-wide alarm when spotting while unaware. */
	void StartAlarmRadio(const FVector& ContactPos);

	/** Apply learned combat multipliers from battle self-improve. */
	void ApplyLearningBoost(float SpeedMul, float DamageMul, float FireRateMul);

	/** Unique squad index 0..N (set at spawn — do not use UniqueID % 3). */
	void SetFormationSlot(int32 Slot);
	int32 GetFormationSlot() const { return FormationSlot; }

	/**
	 * Tactical wedge offset facing +X (toward base):
	 * point / left-right wings / far flanks / rear — keeps squad spread.
	 */
	FVector GetFormationOffset() const;

	/** Preferred world Y lane for this slot (absolute offset from -5850). */
	float GetFormationLaneY() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void MoveToward(const FVector& WorldTarget, float DeltaSeconds, float SpeedScale = 1.f);
	void TickAlly(float DeltaSeconds);
	void TickAllyStormAssault(float DeltaSeconds, AActor* Enemy, AActor* Isaac, const FVector& AssaultRally);
	/**
	 * Assault objective order (AI-vs-AI / Storm):
	 * 1) Survive close enemy  2) Breach doors/%  3) Destroy ISAAC  4) Hunt remaining UK
	 */
	void TickAllyObjectives(float DeltaSeconds, AActor* Enemy, AActor* Isaac, const FVector& AssaultRally);
	void TickDefender(float DeltaSeconds);
	void TickDoorGuard(float DeltaSeconds);
	void TickStuckRecovery(float DeltaSeconds);
	/** True if capsule is not embedded in wall solid. */
	bool IsLocationClear(const FVector& TestLoc) const;
	/** Capsule walkability from A→B (ignores pawns; walls block). */
	bool HasWalkableLOS(const FVector& From, const FVector& To) const;
	/** Rebuild sticky multi-waypoint path to goal (landmarks + side detours). */
	void RebuildPathTo(const FVector& Goal);
	/** Current path step, or Goal if empty. */
	FVector GetPathFollowPoint(const FVector& Goal) const;
	/** Moonbase landmarks for routing through airlock / flanks. */
	void CollectPathLandmarks(TArray<FVector>& Out) const;
	/** Side-offset corners when a segment is wall-blocked. */
	bool TryAppendDetour(const FVector& A, const FVector& B, TArray<FVector>& OutPath) const;
	/** If inside wall mesh, push to nearest free space (sweep-safe). */
	void ResolveWallPenetration();
	/** Pick free walk direction toward goal (local obstacle fan). */
	FVector FindBestSteerDir(const FVector& DesiredDir, float LookAhead) const;
	/** Bash nearest clutter / door when wedged against it. */
	void BashNearestObstacle();
	/** Push away from nearby allies (doorway jam / stacking). */
	FVector ComputeSeparation() const;
	/** Staging hold west of a choke — unique per slot so the pack does not stack. */
	FVector GetChokeStagingPoint(const FVector& ChokeLoc, float BackDist = 450.f) const;
	/** True if another ally is closer to choke and we should wait. */
	bool ShouldYieldAtChoke(const FVector& ChokeLoc, float Radius = 420.f) const;
	void TryFireAt(AActor* Target);
	/** Blast door/wreck/breach prop (ignores perfect LOS when very close). */
	void TryBreachTarget(AActor* Target, float DamageMul = 1.f);
	/** US attackers: if near a door/wreck/sealed panel, always fire on it. */
	void AlwaysShootNearbyBreach();
	void RefreshLook();
	void Die();
	void BuildSuit();
	void BuildFlagBadge();
	void SpawnRobotWreckage();
	AActor* FindNearestEnemy() const;
	/** Prefer soft targets; humans deprioritize enemy robots (never head-on). */
	AActor* FindTacticalEnemy() const;
	/** Engage with role tactics: human kite robots, robots tank. */
	bool EngageTactically(AActor* Target, float DeltaSeconds);
	AActor* FindNearestIsaac() const;
	AActor* FindNearestRoomDoor() const;
	AActor* FindNearestFriendlyRobot() const;
	/** Sealed % panels / breachable scenery in path. */
	AActor* FindNearestBreachable() const;
	int32 CountAlliesNear(const FVector& Loc, float Radius) const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Torso;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Visor;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ShoulderL;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ShoulderR;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ArmL;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ArmR;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> LegL;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> LegR;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Backpack;

	/** Large chest nation badge (base plate). */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BadgeBase;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BadgeCanton;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BadgeStripe;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BadgeCross;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Halo;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LitMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitMat;

	float HP = 60.f;
	bool bDead = false;
	bool bBlockingDoorway = false;
	bool bPlayerDriven = false;
	float FireCD = 0.f;
	float RepathCD = 0.f;
	float HitFlash = 0.f;
	float AlarmRadioT = -1.f;
	int32 FormationSlot = 0;
	FVector Home = FVector::ZeroVector;
	FVector HoldPoint = FVector::ZeroVector;
	FVector WanderTarget = FVector::ZeroVector;
	FVector DoorStandbyPoint = FVector::ZeroVector;
	FVector DoorBlockPoint = FVector::ZeroVector;
	FVector KnownContact = FVector::ZeroVector;

	// 2D-style stuck recovery
	FVector LastPos = FVector::ZeroVector;
	float StuckT = 0.f;
	float UnstickT = 0.f;
	FVector UnstickDir = FVector::ForwardVector;
	int32 StuckEvents = 0;

	// Sticky multi-waypoint path (no NavMesh required — works on greybox map)
	TArray<FVector> PathWaypoints;
	int32 PathIndex = 0;
	FVector PathGoal = FVector::ZeroVector;
	float PathRebuildT = 0.f;
	int32 PathFailStreak = 0;
	float PreferDetourSign = 1.f;
	/** Damped walk heading — stops frame-to-frame steer thrash / visual hopping. */
	FVector SmoothMoveDir = FVector(1.f, 0.f, 0.f);
	bool bSmoothMoveInit = false;
};
