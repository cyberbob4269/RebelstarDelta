// Rebelstar Delta — ISAAC room door

#include "RDRoomDoor.h"
#include "RDGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ARDRoomDoor::ARDRoomDoor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lit(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Unlit(TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"));
	if (Cube.Succeeded()) CubeMesh = Cube.Object;
	if (Lit.Succeeded()) LitMat = Lit.Object;
	if (Unlit.Succeeded()) UnlitMat = Unlit.Object;
	if (!UnlitMat) UnlitMat = LitMat;

	auto Make = [&](const FName& Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* M = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		M->SetupAttachment(Root);
		M->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		M->SetCollisionObjectType(ECC_WorldDynamic);
		M->SetCollisionResponseToAllChannels(ECR_Block);
		M->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		M->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		M->SetCastShadow(true);
		return M;
	};

	FrameL = Make(TEXT("FrameL"));
	FrameR = Make(TEXT("FrameR"));
	FrameTop = Make(TEXT("FrameTop"));
	PanelL = Make(TEXT("PanelL"));
	PanelR = Make(TEXT("PanelR"));
	Stripe = Make(TEXT("Stripe"));

	Tags.Add(FName(TEXT("RD_Base")));
	Tags.Add(FName(TEXT("RD_RoomDoor")));
	Tags.Add(FName(TEXT("RD_IsaacDoor")));
}

void ARDRoomDoor::BeginPlay()
{
	Super::BeginPlay();
	HP = MaxHP;
	BuildDoor();
	RefreshLook();

	if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->RegisterIsaacDoor(this);
	}
}

void ARDRoomDoor::BuildDoor()
{
	if (!CubeMesh) return;

	auto Setup = [&](UStaticMeshComponent* M, const FVector& Rel, const FVector& Scale)
	{
		if (!M) return;
		M->SetStaticMesh(CubeMesh);
		M->SetRelativeLocation(Rel);
		M->SetRelativeScale3D(Scale);
	};

	// Door spans corridor opening ~3m wide, 3.4m tall — faces north/south approach
	Setup(FrameL, FVector(0.f, -160.f, 170.f), FVector(0.45f, 0.45f, 3.4f));
	Setup(FrameR, FVector(0.f, 160.f, 170.f), FVector(0.45f, 0.45f, 3.4f));
	Setup(FrameTop, FVector(0.f, 0.f, 350.f), FVector(0.5f, 3.4f, 0.4f));
	Setup(PanelL, FVector(0.f, -70.f, 170.f), FVector(0.28f, 1.2f, 3.1f));
	Setup(PanelR, FVector(0.f, 70.f, 170.f), FVector(0.28f, 1.2f, 3.1f));
	Setup(Stripe, FVector(20.f, 0.f, 170.f), FVector(0.12f, 2.6f, 0.25f));
}

void ARDRoomDoor::ApplyColor(UStaticMeshComponent* M, const FLinearColor& C, bool bUnlit)
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

void ARDRoomDoor::RefreshLook()
{
	const float Hurt = 1.f - GetHealthPercent();
	const FLinearColor Frame(0.35f, 0.36f, 0.4f);
	const FLinearColor Panel(0.55f - Hurt * 0.25f, 0.22f + Hurt * 0.15f, 0.12f);
	const FLinearColor Warn(1.f, 0.75f - Hurt * 0.4f, 0.1f);
	ApplyColor(FrameL, Frame, false);
	ApplyColor(FrameR, Frame, false);
	ApplyColor(FrameTop, Frame, false);
	ApplyColor(PanelL, Panel, false);
	ApplyColor(PanelR, Panel, false);
	ApplyColor(Stripe, Warn, true);
}

void ARDRoomDoor::TakeDamageAmount(float Amount)
{
	if (bBreached || Amount <= 0.f) return;
	HP = FMath::Max(0.f, HP - Amount);
	RefreshLook();

	if (GetWorld())
	{
		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 220.f),
			FString::Printf(TEXT("DOOR %.0f"), HP), nullptr, FColor::Orange, 0.5f, true, 1.2f);
	}

	if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->RaiseAlarm(GetActorLocation());
	}

	if (HP <= 0.f)
	{
		Breach();
	}
}

void ARDRoomDoor::Breach()
{
	if (bBreached) return;
	bBreached = true;

	// Drop panels (open path) — leave scorched frame as marker
	if (PanelL)
	{
		PanelL->SetVisibility(false);
		PanelL->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (PanelR)
	{
		PanelR->SetVisibility(false);
		PanelR->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (Stripe)
	{
		Stripe->SetVisibility(false);
		Stripe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	// Frames stay as doorway outline but don't block center
	if (FrameL) FrameL->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (FrameR) FrameR->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (FrameTop) FrameTop->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (GetWorld())
	{
		DrawDebugSphere(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 100.f),
			120.f, 16, FColor::Orange, false, 2.5f, 0, 5.f);
	}

	if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->NotifyIsaacDoorBreached(this);
	}
}
