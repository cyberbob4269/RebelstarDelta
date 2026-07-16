// Rebelstar Delta — destructible ISAAC room door (classic choke)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RDRoomDoor.generated.h"

class UStaticMeshComponent;

UCLASS()
class REBELSTARDELTA_API ARDRoomDoor : public AActor
{
	GENERATED_BODY()

public:
	ARDRoomDoor();

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void TakeDamageAmount(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsBreached() const { return bBreached; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	float GetHealthPercent() const { return MaxHP > 0.f ? HP / MaxHP : 0.f; }

	/** Where the standby robot should stand when door is intact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	FVector StandbyPoint = FVector::ZeroVector;

	/** Where the robot wedges after the door falls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	FVector BlockPoint = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	/** Slightly lower so AI storm training can crack the choke. */
	float MaxHP = 160.f;

protected:
	virtual void BeginPlay() override;

	void BuildDoor();
	void ApplyColor(UStaticMeshComponent* M, const FLinearColor& C, bool bUnlit);
	void Breach();
	void RefreshLook();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FrameL;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FrameR;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FrameTop;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PanelL;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PanelR;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Stripe;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LitMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitMat;

	float HP = 220.f;
	bool bBreached = false;
};
