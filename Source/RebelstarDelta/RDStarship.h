// Rebelstar Delta — distant Starship on lunar horizon (launch-day silhouette)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RDStarship.generated.h"

class UPointLightComponent;

UCLASS()
class REBELSTARDELTA_API ARDStarship : public AActor
{
	GENERATED_BODY()

public:
	ARDStarship();

	/** World location of stack base (pad). */
	UPROPERTY(EditAnywhere, Category = "Rebelstar")
	FVector PadLocation = FVector(-28000.f, 18000.f, 0.f);

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	FVector GetLookAtPoint() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void BuildShip();
	void ApplyColor(UStaticMeshComponent* M, const FLinearColor& C, bool bUnlit);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> EngineGlow;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LitMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitMat;

	float GlowPhase = 0.f;
};
