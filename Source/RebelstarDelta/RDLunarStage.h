// Rebelstar Delta — lunar stage: low sun, Earthrise day/night/clouds

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RDLunarStage.generated.h"

class UStaticMeshComponent;
class ADirectionalLight;
class APointLight;

UCLASS()
class REBELSTARDELTA_API ARDLunarStage : public AActor
{
	GENERATED_BODY()

public:
	ARDLunarStage();

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void BuildStage();

	/** Remove OpenWorld template landscape / extra suns / fog so our moon owns the scene. */
	static void PurgeTemplateEnvironment(UWorld* World);

	/** Hours of simulated Earth rotation rate (visual). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Earth")
	float EarthDaySeconds = 180.f; // full spin in 3 real minutes (readable day/night)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Earth")
	float CloudDaySeconds = 90.f; // clouds drift faster

	/** Low sun for long crater shadows (pitch degrees, negative = above horizon). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Sun")
	float SunPitch = -12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Sun")
	float SunYaw = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rebelstar|Sun")
	float SunIntensity = 45.f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	AStaticMeshActor* SpawnMesh(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
		const FRotator& Rotation, const FLinearColor& Color, bool bUnlit, bool bCollision,
		UMaterialInterface* OverrideMat = nullptr);
	void ApplyLook(UStaticMeshComponent* SMC, const FLinearColor& Color, bool bUnlit);
	void TryLoadPackMaterials();
	void SpawnSun();
	void SpawnGround();
	void SpawnCraterRim();
	void SpawnEarth();
	void SpawnStarfield();
	void SpawnMilkyWayBand();
	void SpawnConstructionHint();
	void PlacePlayerOnPad();
	void UpdateEarthVisuals(float DeltaSeconds);

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LitMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MatMoonGround;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MatEarth;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MatEarthDay;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MatEarthNight;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MatEarthClouds;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MatMilkyWay;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MatMoonGlobe;

	/** Earth root + layered shells (day / night cities / clouds). */
	UPROPERTY()
	TObjectPtr<AActor> EarthRoot;

	UPROPERTY()
	TObjectPtr<AStaticMeshActor> EarthDaySphere;

	UPROPERTY()
	TObjectPtr<AStaticMeshActor> EarthNightSphere;

	UPROPERTY()
	TObjectPtr<AStaticMeshActor> EarthCloudSphere;

	UPROPERTY()
	TObjectPtr<ADirectionalLight> SunLight;

	UPROPERTY()
	TObjectPtr<APointLight> EarthShineLight;

	float EarthSpin = 0.f;
	float CloudSpin = 0.f;
	float SunCycle = 0.f;
};
