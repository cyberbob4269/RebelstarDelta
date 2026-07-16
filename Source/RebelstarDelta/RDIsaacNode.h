// Rebelstar Delta — ISAAC server-rack cores (pulse + explode)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RDIsaacNode.generated.h"

class UPointLightComponent;

UCLASS()
class REBELSTARDELTA_API ARDIsaacNode : public AActor
{
	GENERATED_BODY()

public:
	ARDIsaacNode();

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void TakeDamageAmount(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsDestroyed() const { return bDead; }

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	float GetHealthPercent() const { return MaxHP > 0.f ? HP / MaxHP : 0.f; }

	/** Fable 5 spawn uses 200 per node; keep slightly higher for 3D combat pace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float MaxHP = 280.f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void BuildServerRack();
	void ApplyPartColor(UStaticMeshComponent* Part, const FLinearColor& Color, bool bUnlit);
	void PulseLook(float Pulse01);
	void Explode();
	void SpawnDebrisBurst();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Chassis;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> DoorL;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> DoorR;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TopBeacon;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BlinkLeds;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LitMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitMat;

	float HP = 280.f;
	bool bDead = false;
	float PulseTime = 0.f;
	float HitFlash = 0.f;
};
