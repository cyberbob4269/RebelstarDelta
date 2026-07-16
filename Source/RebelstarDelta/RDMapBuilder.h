// Rebelstar Delta — greybox Moonbase with heavy visual pass

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RDMapBuilder.generated.h"

UCLASS()
class REBELSTARDELTA_API ARDMapBuilder : public AActor
{
	GENERATED_BODY()

public:
	ARDMapBuilder();

	UFUNCTION(BlueprintCallable, Category = "Rebelstar")
	void BuildBase();

	UPROPERTY(EditAnywhere, Category = "Rebelstar")
	float CellSize = 300.f;

	UPROPERTY(EditAnywhere, Category = "Rebelstar")
	FVector MapOrigin = FVector(4000.f, -9900.f, 0.f);

protected:
	virtual void BeginPlay() override;

	bool LoadLogicRows(TArray<FString>& OutRows) const;
	void ClearPrevious();
	bool WantSeeThroughRoofs() const;
	AActor* SpawnBox(const FVector& WorldLoc, const FVector& Scale, const FLinearColor& Color,
		bool bCollision, bool bBreach = false, bool bUnlit = false);
	AActor* SpawnShape(UStaticMesh* Mesh, const FVector& WorldLoc, const FVector& Scale,
		const FRotator& Rot, const FLinearColor& Color, bool bCollision, bool bUnlit = false);
	void SpawnIsaacAt(const FVector& WorldLoc);
	void SpawnIsaacRoomDoor(const FVector& WorldLoc, float CS);
	void SpawnDestructibleAt(const FVector& WorldLoc, float HP, const FLinearColor& Color,
		const FString& Kind, const FVector& Scale, bool bUnlit = false);
	void SpawnConstructionYard(const FVector& Anchor);
	void SpawnAirlockVolume(const FVector& Center, float OuterX, float InnerX);
	void SpawnInteriorLight(const FVector& Loc, const FLinearColor& Col, float Intensity = 12000.f, float Radius = 1600.f);
	void SpawnExteriorDressing(const FVector& AirlockCenter, float CS, int32 MapH);
	void SpawnApproachLZ(const FVector& LZ, const FVector& AirlockMouth);
	void SpawnRoofScenery(const FVector& MapOrigin, float CS, int32 W, int32 H);
	/** Optional ceiling tile — skipped when see-through roofs enabled. */
	void SpawnCeilingTile(const FVector& Cell, float CeilingZ, float CS, const FLinearColor& Color);
	/** Find best west entrance (row + wall column) from logic map. */
	void FindWestEntrance(const TArray<FString>& Rows, int32& OutY, int32& OutWallX) const;
	/** South breach into ISAAC sector (classic door choke). */
	void FindIsaacDoorCell(const TArray<FString>& Rows, int32& OutX, int32& OutY) const;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitMat;
};
