#include "RDDestructible.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ARDDestructible::ARDDestructible()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lit(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Unlit(TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"));
	if (Cube.Succeeded()) CubeMesh = Cube.Object;
	if (Lit.Succeeded()) LitMat = Lit.Object;
	if (Unlit.Succeeded()) UnlitMat = Unlit.Object;
	if (!UnlitMat) UnlitMat = LitMat;

	Tags.Add(FName(TEXT("RD_Base")));
	Tags.Add(FName(TEXT("RD_Destructible")));
}

void ARDDestructible::BeginPlay()
{
	Super::BeginPlay();
	if (HP <= 0.f) HP = MaxHP;
}

void ARDDestructible::Configure(float InMaxHP, const FLinearColor& Color, const FString& InKind,
	const FVector& Scale, bool bUnlit)
{
	MaxHP = InMaxHP;
	HP = InMaxHP;
	Kind = InKind;
	BaseColor = Color;
	bUseUnlit = bUnlit;
	if (Mesh && CubeMesh)
	{
		Mesh->SetStaticMesh(CubeMesh);
		Mesh->SetWorldScale3D(Scale);
	}
	// Small furniture / plants: still shootable cover, but AI must not snag on them.
	// Sealed doors / heavy machinery / tanks stay solid (breach objectives).
	const bool bSolidObstacle =
		InKind.Contains(TEXT("Sealed"), ESearchCase::IgnoreCase)
		|| InKind.Contains(TEXT("door"), ESearchCase::IgnoreCase)
		|| InKind.Contains(TEXT("Machinery"), ESearchCase::IgnoreCase)
		|| InKind.Contains(TEXT("tank"), ESearchCase::IgnoreCase)
		|| InKind.Contains(TEXT("Wall"), ESearchCase::IgnoreCase);
	if (Mesh && !bSolidObstacle)
	{
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Tags.Add(FName(TEXT("RD_Clutter")));
	}
	ApplyLook();
}

void ARDDestructible::ApplyLook()
{
	if (!Mesh) return;
	UMaterialInterface* P = bUseUnlit ? UnlitMat.Get() : LitMat.Get();
	if (!P) return;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(P, this);
	if (!MID) return;
	const float Hurt = 1.f - (MaxHP > 0.f ? HP / MaxHP : 0.f);
	const FLinearColor C = FMath::Lerp(BaseColor, FLinearColor(0.15f, 0.1f, 0.08f), Hurt * 0.55f);
	MID->SetVectorParameterValue(TEXT("Color"), C);
	MID->SetVectorParameterValue(TEXT("BaseColor"), C);
	MID->SetVectorParameterValue(TEXT("EmissiveColor"), bUseUnlit ? C : FLinearColor::Black);
	Mesh->SetMaterial(0, MID);
}

void ARDDestructible::TakeDamageAmount(float Amount)
{
	if (bDead || Amount <= 0.f) return;
	HP = FMath::Max(0.f, HP - Amount);
	ApplyLook();
	if (GetWorld())
	{
		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 80),
			FString::Printf(TEXT("%s %.0f"), *Kind, HP), nullptr, FColor::Silver, 0.35f, true, 1.f);
	}
	if (HP <= 0.f) Die();
}

void ARDDestructible::Die()
{
	if (bDead) return;
	bDead = true;
	if (GetWorld())
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), 60.f, 10, FColor(255, 140, 40), false, 1.2f, 0, 3.f);
	}
	Destroy();
}
