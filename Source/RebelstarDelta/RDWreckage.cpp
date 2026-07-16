// Rebelstar Delta — doorway wreckage choke

#include "RDWreckage.h"
#include "RDGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ARDWreckage::ARDWreckage()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetupAttachment(Root);
	Volume->SetBoxExtent(FVector(90.f, 160.f, 160.f));
	Volume->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	Volume->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Volume->SetCollisionObjectType(ECC_WorldDynamic);
	Volume->SetCollisionResponseToAllChannels(ECR_Block);
	Volume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Volume->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lit(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Unlit(TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"));
	if (Cube.Succeeded()) CubeMesh = Cube.Object;
	if (Lit.Succeeded()) LitMat = Lit.Object;
	if (Unlit.Succeeded()) UnlitMat = Unlit.Object;
	if (!UnlitMat) UnlitMat = LitMat;

	Tags.Add(FName(TEXT("RD_Base")));
	Tags.Add(FName(TEXT("RD_Wreckage")));
}

void ARDWreckage::ConfigureAsRobotHulk()
{
	// 2D wreckage.gd: hp 160, solid until destroyed
	// NOTE: BeginPlay already ran during SpawnActor — do NOT rely on bDoorChoke there.
	bDoorChoke = false;
	MaxHP = 160.f;
	HP = 160.f;
	PassableAtFraction = 0.f; // never crawl — must clear fully
	SlowMultiplier = 1.f;
	if (Volume)
	{
		Volume->SetBoxExtent(FVector(70.f, 70.f, 90.f));
		Volume->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	}
	RefreshCollisionState();
	RefreshLook();
}

void ARDWreckage::ConfigureAsDoorChoke()
{
	bDoorChoke = true;
	MaxHP = 400.f;
	HP = 400.f;
	PassableAtFraction = 0.45f;
	SlowMultiplier = 0.28f;
	if (Volume)
	{
		Volume->SetBoxExtent(FVector(90.f, 160.f, 160.f));
		Volume->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	}
	RefreshCollisionState();
	RefreshLook();
	if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->StatusNote(TEXT("WRECKAGE blocks the ISAAC door — clear it or take the long corridor."));
	}
}

void ARDWreckage::BeginPlay()
{
	Super::BeginPlay();
	if (HP <= 0.f || HP > MaxHP) HP = MaxHP;
	BuildPile();
	RefreshCollisionState();
	RefreshLook();
	// Door message is posted from ConfigureAsDoorChoke() AFTER spawn (BeginPlay is too early).
}

void ARDWreckage::BuildPile()
{
	if (!CubeMesh) return;
	Chunks.Reset();

	const FVector Offsets[] = {
		FVector(0.f, 0.f, 80.f),
		FVector(40.f, -60.f, 50.f),
		FVector(-30.f, 70.f, 60.f),
		FVector(10.f, -20.f, 140.f),
		FVector(-50.f, 10.f, 120.f),
		FVector(20.f, 50.f, 180.f),
		FVector(-10.f, -80.f, 100.f),
		FVector(55.f, 20.f, 40.f),
	};
	const FVector Scales[] = {
		FVector(1.6f, 2.8f, 1.4f),
		FVector(1.1f, 1.4f, 0.9f),
		FVector(1.2f, 1.1f, 1.0f),
		FVector(0.9f, 1.6f, 0.8f),
		FVector(1.0f, 1.0f, 1.2f),
		FVector(0.8f, 1.3f, 0.7f),
		FVector(1.3f, 0.9f, 0.8f),
		FVector(0.7f, 1.1f, 0.6f),
	};

	for (int32 i = 0; i < 8; ++i)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
		M->SetupAttachment(Root);
		M->RegisterComponent();
		M->SetStaticMesh(CubeMesh);
		M->SetRelativeLocation(Offsets[i]);
		M->SetRelativeScale3D(Scales[i]);
		M->SetRelativeRotation(FRotator(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(-15.f, 15.f)));
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(true);
		Chunks.Add(M);
	}
}

void ARDWreckage::ApplyColor(UStaticMeshComponent* M, const FLinearColor& C, bool bUnlit)
{
	if (!M) return;
	UMaterialInterface* P = bUnlit ? UnlitMat.Get() : LitMat.Get();
	if (!P) return;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(P, this);
	if (!MID) return;
	MID->SetVectorParameterValue(TEXT("Color"), C);
	MID->SetVectorParameterValue(TEXT("BaseColor"), C);
	MID->SetVectorParameterValue(TEXT("EmissiveColor"), C);
	M->SetMaterial(0, MID);
}

void ARDWreckage::RefreshLook()
{
	const float Hurt = 1.f - (MaxHP > 0.f ? HP / MaxHP : 0.f);
	for (int32 i = 0; i < Chunks.Num(); ++i)
	{
		const float Grey = 0.18f + 0.08f * (i % 3);
		FLinearColor C(Grey + Hurt * 0.1f, Grey * 0.7f, Grey * 0.5f);
		if (bPassable) C = FLinearColor(0.45f, 0.25f, 0.08f);
		ApplyColor(Chunks[i], C, bPassable);
		// Shrink pile as it is chewed through
		if (Chunks[i])
		{
			const float S = bPassable ? 0.65f : (1.f - Hurt * 0.25f);
			const FVector Base = Chunks[i]->GetRelativeScale3D().GetAbs().GetMax() > 0.01f
				? Chunks[i]->GetRelativeScale3D()
				: FVector(1.f);
			// only apply once-ish via absolute scales stored at build — re-set from original pattern
		}
	}
	// Dim outer chunks when passable
	if (bPassable)
	{
		for (int32 i = 4; i < Chunks.Num(); ++i)
		{
			if (Chunks[i]) Chunks[i]->SetVisibility(false);
		}
	}
}

void ARDWreckage::RefreshCollisionState()
{
	if (!Volume) return;
	if (bCleared)
	{
		Volume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}
	if (bPassable)
	{
		// Overlap only — walkable, used for slow query
		Volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Volume->SetCollisionResponseToAllChannels(ECR_Overlap);
		Volume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // still shootable
		Volume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Volume->SetBoxExtent(FVector(100.f, 180.f, 120.f));
	}
	else
	{
		Volume->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Volume->SetCollisionResponseToAllChannels(ECR_Block);
		Volume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Volume->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
}

void ARDWreckage::TakeDamageAmount(float Amount)
{
	if (bCleared || Amount <= 0.f) return;
	HP = FMath::Max(0.f, HP - Amount);

	if (GetWorld())
	{
		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 240.f),
			FString::Printf(TEXT("WRECK %.0f%s"), HP, bPassable ? TEXT(" SLOW") : TEXT("")),
			nullptr, bPassable ? FColor::Yellow : FColor::Orange, 0.45f, true, 1.1f);
	}

	const float Frac = MaxHP > 0.f ? HP / MaxHP : 0.f;
	// Door choke only: open a slow crawl gap. Normal robot hulks stay solid until 0.
	if (bDoorChoke && !bPassable && PassableAtFraction > 0.f && Frac <= PassableAtFraction)
	{
		bPassable = true;
		RefreshCollisionState();
		if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GM->StatusNote(TEXT("Wreckage broken open — crawl through (SLOW) or finish clearing."));
		}
	}

	if (HP <= 0.f)
	{
		ClearFully();
		return;
	}
	RefreshLook();
}

void ARDWreckage::ClearFully()
{
	bCleared = true;
	bPassable = true;
	RefreshCollisionState();
	for (UStaticMeshComponent* M : Chunks)
	{
		if (M)
		{
			M->SetVisibility(false);
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	if (bDoorChoke)
	{
		if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GM->StatusNote(TEXT("Doorway cleared — ISAAC room open."));
		}
	}
	SetLifeSpan(1.5f);
}

bool ARDWreckage::IsActorInside(const AActor* Other) const
{
	if (!Other || bCleared || !bPassable || !Volume) return false;
	const FVector P = Other->GetActorLocation();
	const FVector Local = Volume->GetComponentTransform().InverseTransformPosition(P);
	const FVector Ext = Volume->GetScaledBoxExtent();
	return FMath::Abs(Local.X) <= Ext.X + 40.f
		&& FMath::Abs(Local.Y) <= Ext.Y + 40.f
		&& FMath::Abs(Local.Z) <= Ext.Z + 80.f;
}
