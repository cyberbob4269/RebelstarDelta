// Rebelstar Delta — moon buggy (Fable 5 buggy.gd)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RDMoonBuggy.generated.h"

class ARDCharacter;
class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class REBELSTARDELTA_API ARDMoonBuggy : public AActor
{
	GENERATED_BODY()

public:
	ARDMoonBuggy();

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void TakeDamageAmount(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsOccupied() const { return bOccupied; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool TryBoard(ARDCharacter* Rider);

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void Disembark();

	/** Drive this frame (called by rider). */
	void DriveInput(const FVector& WorldDir, float DeltaSeconds);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float MaxHP = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float DriveSpeed = 1100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float BoardRadius = 280.f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void BuildMesh();
	void ApplyColor(UStaticMeshComponent* M, const FLinearColor& C, bool bUnlit);
	void Die();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> BodyCol;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Cabin;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LitMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitMat;

	UPROPERTY()
	TObjectPtr<ARDCharacter> Rider;

	float HP = 180.f;
	bool bOccupied = false;
	float BoardCD = 0.f;
	FVector Velocity = FVector::ZeroVector;
};
