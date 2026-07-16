// Rebelstar Delta — doorway wreckage after door-guard robot dies

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RDWreckage.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * Blocks the doorway while HP is high.
 * After enough damage, becomes passable but applies a heavy movement slow.
 * Fully destroyed when HP hits 0.
 */
UCLASS()
class REBELSTARDELTA_API ARDWreckage : public AActor
{
	GENERATED_BODY()

public:
	ARDWreckage();

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void TakeDamageAmount(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsPassable() const { return bPassable; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsCleared() const { return bCleared; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	float GetSlowMultiplier() const { return bPassable && !bCleared ? SlowMultiplier : 1.f; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsActorInside(const AActor* Other) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float MaxHP = 400.f;

	/** Below this fraction of MaxHP, wreckage becomes crawl-through (slow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float PassableAtFraction = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float SlowMultiplier = 0.28f;

	/**
	 * Fable 5 robot hulk: solid block, shoot to clear (no crawl-through).
	 * Call AFTER SpawnActor (BeginPlay already ran).
	 */
	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void ConfigureAsRobotHulk();

	/** ISAAC door choke only — crawl-through + door status messages. Call AFTER spawn. */
	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void ConfigureAsDoorChoke();

	/** True only for ISAAC door wreckage. Default false so normal robot hulks stay quiet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	bool bDoorChoke = false;

protected:
	virtual void BeginPlay() override;

	void BuildPile();
	void ApplyColor(UStaticMeshComponent* M, const FLinearColor& C, bool bUnlit);
	void RefreshCollisionState();
	void RefreshLook();
	void ClearFully();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> Volume;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Chunks;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LitMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitMat;

	float HP = 400.f;
	bool bPassable = false;
	bool bCleared = false;
};
