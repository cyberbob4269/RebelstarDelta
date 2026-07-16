// Rebelstar Delta — match flow, alarm, ISAAC door defense, AI-vs-AI spectator loop

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RDGameMode.generated.h"

class ARDIsaacNode;
class ARDStarship;
class ARDBot;
class ARDRoomDoor;
class ARDWreckage;

/** How the US assault tries to enter the base this round (learned over rematches). */
UENUM(BlueprintType)
enum class ERDApproachRoute : uint8
{
	Airlock UMETA(DisplayName = "Main airlock"),
	NorthFlank UMETA(DisplayName = "North flank"),
	SouthFlank UMETA(DisplayName = "South flank"),
	COUNT UMETA(Hidden)
};

/** How UK deploys once the alarm is up (learned over rematches). */
UENUM(BlueprintType)
enum class ERDDefensePlan : uint8
{
	HoldDeep UMETA(DisplayName = "Hold deep / posts"),
	RushAirlock UMETA(DisplayName = "Rush airlock"),
	SplitGuard UMETA(DisplayName = "Split west + ISAAC"),
	COUNT UMETA(Hidden)
};

UCLASS()
class REBELSTARDELTA_API ARDGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARDGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void RegisterIsaac(ARDIsaacNode* Node);
	void NotifyIsaacDestroyed(ARDIsaacNode* Node);
	void RaiseAlarm(const FVector& AtLocation);

	/** Classic ISAAC-room door choke. */
	void RegisterIsaacDoor(ARDRoomDoor* Door);
	void NotifyIsaacDoorBreached(ARDRoomDoor* Door);
	void NotifyDoorGuardKilled(ARDBot* Bot);
	void SpawnDoorWreckage(const FVector& At);
	void NotifyLeaderKilled();
	void NotifyBotKilled(ARDBot* Bot);

	/** Short HUD / status line (door, wreckage, tactics). */
	void StatusNote(const FString& Message);

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsMatchOver() const { return bMatchOver; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool DidAttackersWin() const { return bAttackersWin; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsAlarmRaised() const { return bAlarm; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsIsaacDoorBreached() const { return bIsaacDoorBreached; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsDoorWreckageActive() const { return bDoorWreckageActive; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	float GetTimeLeft() const { return TimeLeft; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetIsaacAliveCount() const;

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetIsaacTotalCount() const { return IsaacNodes.Num(); }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	FString GetStatusMessage() const { return StatusMessage; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsEndingCinematic() const { return bEndingCinematic; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	FVector GetStarshipLookPoint() const;

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	FVector GetIsaacDoorBlockPoint() const { return IsaacDoorBlockPoint; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	ARDRoomDoor* GetIsaacDoor() const { return IsaacDoor.Get(); }

	/** Both teams under AI; player is free-fly spectator above the base. */
	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsAIVsAIMode() const { return bAIVsAIMode; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool WantSeeThroughRoofs() const { return bSeeThroughRoofs; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetAliveAllies() const;

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetAliveDefenders() const;

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetRoundIndex() const { return RoundIndex; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetAttackerKills() const { return AttackerKills; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetDefenderKills() const { return DefenderKills; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetAttackerWins() const { return AttackerWins; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetDefenderWins() const { return DefenderWins; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float MatchDuration = 360.f;

	/** Default ON: AI vs AI spectator loop for autonomous tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Spectator")
	bool bAIVsAIMode = true;

	/** Remove solid ceilings so the battle is visible from above. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Spectator")
	bool bSeeThroughRoofs = true;

	/** Seconds after match end before auto rematch (AI-vs-AI only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Spectator")
	float RematchDelay = 4.f;

	/** Raise alarm after this many seconds so the fight actually starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Spectator")
	float AutoAlarmDelay = 1.5f;

	/** Training series target (agent loop). 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Spectator")
	int32 TargetTrainingRounds = 10;

	/** Shorter matches while training so 20 rounds finish faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Spectator")
	float TrainingMatchDuration = 75.f;

	// --- Close-run roster (AI-vs-AI; map glyphs are a pool, not hard counts) ---
	/** US human combatants (includes Trump). Default 4 → closer than 5v13. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Balance")
	int32 DesiredAllyHumans = 4;

	/** US assault robots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Balance")
	int32 DesiredAllyRobots = 2;

	/** UK human operatives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Balance")
	int32 DesiredDefHumans = 4;

	/** UK door/sentry bots (keep ≥1 for choke). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Balance")
	int32 DesiredDefSentries = 1;

	/** UK patrol mechs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Balance")
	int32 DesiredDefDroids = 2;

	/** Auto-nudge roster when one side runs away with the series. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Balance")
	bool bAutoBalanceRosters = true;

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetDesiredUSCount() const { return DesiredAllyHumans + DesiredAllyRobots; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetDesiredUKCount() const { return DesiredDefHumans + DesiredDefSentries + DesiredDefDroids; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	float GetLearnedAllySpeedMul() const { return LearnedAllySpeedMul; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	float GetLearnedAllyDamageMul() const { return LearnedAllyDamageMul; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	int32 GetStalemateStreak() const { return StalemateStreak; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	float GetPeakAllyX() const { return PeakAllyX; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	ERDApproachRoute GetCurrentApproach() const { return CurrentApproach; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	ERDDefensePlan GetCurrentDefense() const { return CurrentDefense; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	FString GetApproachName() const;

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	FString GetDefenseName() const;

	/** World waypoint for US entry (slot spreads formation). */
	FVector GetApproachWaypoint(int32 FormationSlot) const;

	/** World rally for UK reaction (slot / role). */
	FVector GetDefenseRally(const ARDBot* Bot) const;

	/** Humans should never charge enemy robots head-on. */
	bool ShouldHumanKiteRobot() const { return bHumanKiteRobots; }

protected:
	void SpawnWorld();
	void SpawnSquadAndGuards();
	void StartEndingCinematic();
	void TickEnding(float DeltaSeconds);
	void EndMatch(bool bWin, const FString& Message);
	void SetupSpectatorCamera();
	void TickSpectatorCamera(float DeltaSeconds);
	/** Prefer Trump; if dead, next living US attacker in succession order. */
	ARDBot* ResolveSpectatorSubject();
	void TickBattleMonitor(float DeltaSeconds);
	void RestartMatch();
	void CountTeams(int32& OutAllies, int32& OutDefenders) const;
	/** 2D-style post-mortem: stuck units, airlock jams, timeout → learn params. */
	void AnalyzeBattleAndLearn(bool bWin, const FString& Message);
	void SelectTacticsForRound();
	int32 PickWeightedIndex(const float* Scores, int32 Num, float Explore) const;

	UPROPERTY()
	TArray<TObjectPtr<ARDIsaacNode>> IsaacNodes;

	UPROPERTY()
	TObjectPtr<ARDStarship> Starship;

	UPROPERTY()
	TObjectPtr<ARDRoomDoor> IsaacDoor;

	UPROPERTY()
	TObjectPtr<ARDBot> DoorGuardBot;

	UPROPERTY()
	TObjectPtr<ARDWreckage> DoorWreckage;

	FVector IsaacDoorBlockPoint = FVector::ZeroVector;
	FVector IsaacDoorStandbyPoint = FVector::ZeroVector;

	float TimeLeft = 360.f;
	bool bMatchOver = false;
	bool bAttackersWin = false;
	bool bEndingCinematic = false;
	bool bAlarm = false;
	bool bIsaacDoorBreached = false;
	bool bDoorWreckageActive = false;
	float EndingTime = 0.f;
	FString StatusMessage;

	FVector EndingStartLoc = FVector::ZeroVector;
	FRotator EndingStartRot = FRotator::ZeroRotator;

	// AI-vs-AI spectator / loop
	float AutoAlarmTimer = 0.f;
	float RematchTimer = -1.f;
	float MonitorLogTimer = 0.f;
	float SpecOrbitYaw = 0.f;
	/** Smooth camera follow of current US hero. */
	FVector SpecCamLoc = FVector::ZeroVector;
	FVector SpecLookAt = FVector::ZeroVector;
	bool bSpecCamInitialized = false;
	UPROPERTY()
	TObjectPtr<ARDBot> SpectatorSubject;
	FString LastSpectatorSubjectName;
	int32 RoundIndex = 1;
	int32 AttackerKills = 0;
	int32 DefenderKills = 0;
	int32 AttackerWins = 0;
	int32 DefenderWins = 0;
	int32 LastAliveAllies = -1;
	int32 LastAliveDefs = -1;
	float PeakAllyX = -9000.f;

	// Self-improving battle loop (persists across rematches in this PIE session)
	float LearnedAllySpeedMul = 1.f;
	float LearnedAllyDamageMul = 1.f;
	float LearnedAllyFireMul = 1.f;
	float LearnedDefSpeedMul = 1.f;
	float LearnedDefFireMul = 1.f;
	int32 StalemateStreak = 0;

	// Tactical learning — different ways into / out of the base
	ERDApproachRoute CurrentApproach = ERDApproachRoute::Airlock;
	ERDDefensePlan CurrentDefense = ERDDefensePlan::HoldDeep;
	float ApproachScore[3] = { 1.2f, 1.0f, 1.0f }; // Airlock, North, South
	float DefenseScore[3] = { 1.2f, 1.0f, 1.0f };  // HoldDeep, RushAirlock, Split
	bool bHumanKiteRobots = true; // always on; reinforced by learning
	int32 HumanHeadOnDeaths = 0;  // tracked if we ever detect humans dying near robots
};
