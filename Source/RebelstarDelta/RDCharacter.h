// Rebelstar Delta — FPS leader (Fable 5 P) + squad control + buggy

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RDBot.h"
#include "RDCharacter.generated.h"

class ARDMoonBuggy;

UCLASS()
class REBELSTARDELTA_API ARDCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARDCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	ERDSquadOrder GetCurrentOrder() const { return CurrentOrder; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void ApplyDamage(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	float GetHP() const { return HP; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	float GetMaxHP() const { return MaxHP; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsInBuggy() const { return bInBuggy; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	FString GetControlLabel() const;

	void NotifyBoardedBuggy(ARDMoonBuggy* Buggy);
	void NotifyLeftBuggy();

	/** Heal/repair from zone (GameMode). */
	void HealAmount(float Amount);

	/** AI-vs-AI spectator: cannot die / does not count as combatant. */
	void SetSpectatorInvulnerable(bool bOn) { bSpectatorInvulnerable = bOn; }
	bool IsSpectatorInvulnerable() const { return bSpectatorInvulnerable; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float MaxHP = 80.f; // leader slightly tougher than 60 HP troopers

	/** Display name — always Trump for the US assault leader. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	FString LeaderName = TEXT("Trump");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	FString CountryCode = TEXT("US");

protected:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void OnJump();
	void OnFire();
	void OnInteract(); // E — buggy board/leave
	void OnCycleControl(); // Tab — possess squadmate

	/** Agent / Monolith: fire laser from current camera. */
	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void AgentFire();

	void OrderFollow();
	void OrderAttack();
	void OrderHold();
	void OrderStorm();
	void IssueOrder(ERDSquadOrder Order);

	void ApplyShotAt(const FVector& Start, const FVector& End);
	void Die();
	void GatherLivingAllies(TArray<ARDBot*>& Out) const;
	AActor* GetFireSourceActor() const;
	FVector GetFireStart() const;
	FVector GetFireForward() const;
	void UpdateControlCamera();
	void TickSupportZones(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<class UCameraComponent> FirstPersonCamera;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float LaserDamage = 45.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float LaserRange = 25000.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float FireCooldown = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float BaseWalkSpeed = 500.f;

	float FireReady = 0.f;
	float HP = 80.f;
	bool bDead = false;
	bool bInBuggy = false;
	bool bSpectatorInvulnerable = false;
	float HitFlash = 0.f;
	ERDSquadOrder CurrentOrder = ERDSquadOrder::Follow;

	/** -1 = self (leader). Else index into living allies. */
	int32 ControlIndex = -1;

	UPROPERTY()
	TObjectPtr<ARDBot> ControlledAlly;

	UPROPERTY()
	TObjectPtr<ARDMoonBuggy> CurrentBuggy;

	float MoveAxisF = 0.f;
	float MoveAxisR = 0.f;
};
