// Rebelstar Delta — shootable scenery (Fable 5 wall.gd HP values)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RDDestructible.generated.h"

UCLASS()
class REBELSTARDELTA_API ARDDestructible : public AActor
{
	GENERATED_BODY()

public:
	ARDDestructible();

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void TakeDamageAmount(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	bool IsDestroyed() const { return bDead; }

	void Configure(float InMaxHP, const FLinearColor& Color, const FString& InKind,
		const FVector& Scale, bool bUnlit = false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	float MaxHP = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar")
	FString Kind = TEXT("Structure");

protected:
	virtual void BeginPlay() override;

	void ApplyLook();
	void Die();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LitMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitMat;

	FLinearColor BaseColor = FLinearColor(0.4f, 0.38f, 0.32f);
	bool bUseUnlit = false;
	float HP = 120.f;
	bool bDead = false;
};
