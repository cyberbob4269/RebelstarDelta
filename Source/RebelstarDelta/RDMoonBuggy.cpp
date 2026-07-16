#include "RDMoonBuggy.h"
#include "RDCharacter.h"
#include "RDWreckage.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ARDMoonBuggy::ARDMoonBuggy()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BodyCol = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyCol"));
	BodyCol->SetupAttachment(Root);
	BodyCol->SetBoxExtent(FVector(160.f, 110.f, 70.f));
	BodyCol->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	BodyCol->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BodyCol->SetCollisionObjectType(ECC_WorldDynamic);
	BodyCol->SetCollisionResponseToAllChannels(ECR_Block);
	BodyCol->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BodyCol->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // walk into to board
	BodyCol->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Cabin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cabin"));
	Cabin->SetupAttachment(Root);
	Cabin->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lit(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Unlit(TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"));
	if (Cube.Succeeded()) CubeMesh = Cube.Object;
	if (Lit.Succeeded()) LitMat = Lit.Object;
	if (Unlit.Succeeded()) UnlitMat = Unlit.Object;
	if (!UnlitMat) UnlitMat = LitMat;

	Tags.Add(FName(TEXT("RD_Base")));
	Tags.Add(FName(TEXT("RD_Buggy")));
}

void ARDMoonBuggy::BeginPlay()
{
	Super::BeginPlay();
	HP = MaxHP;
	BuildMesh();
}

void ARDMoonBuggy::BuildMesh()
{
	if (!CubeMesh) return;
	if (Body)
	{
		Body->SetStaticMesh(CubeMesh);
		Body->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
		Body->SetRelativeScale3D(FVector(3.2f, 2.2f, 1.0f));
		ApplyColor(Body, FLinearColor(0.75f, 0.55f, 0.12f), false);
	}
	if (Cabin)
	{
		Cabin->SetStaticMesh(CubeMesh);
		Cabin->SetRelativeLocation(FVector(-40.f, 0.f, 120.f));
		Cabin->SetRelativeScale3D(FVector(1.4f, 1.8f, 0.9f));
		ApplyColor(Cabin, FLinearColor(0.2f, 0.55f, 0.85f), true);
	}
	// Wheels
	for (int32 i = 0; i < 4; ++i)
	{
		const float SX = (i < 2) ? 100.f : -100.f;
		const float SY = (i % 2) ? 95.f : -95.f;
		UStaticMeshComponent* W = NewObject<UStaticMeshComponent>(this);
		W->SetupAttachment(Root);
		W->RegisterComponent();
		W->SetStaticMesh(CubeMesh);
		W->SetRelativeLocation(FVector(SX, SY, 30.f));
		W->SetRelativeScale3D(FVector(0.55f, 0.35f, 0.55f));
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ApplyColor(W, FLinearColor(0.08f, 0.08f, 0.09f), false);
	}
}

void ARDMoonBuggy::ApplyColor(UStaticMeshComponent* M, const FLinearColor& C, bool bUnlit)
{
	if (!M) return;
	UMaterialInterface* P = bUnlit ? UnlitMat.Get() : LitMat.Get();
	if (!P) return;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(P, this);
	if (!MID) return;
	MID->SetVectorParameterValue(TEXT("Color"), C);
	MID->SetVectorParameterValue(TEXT("BaseColor"), C);
	MID->SetVectorParameterValue(TEXT("EmissiveColor"), bUnlit ? C : FLinearColor::Black);
	M->SetMaterial(0, MID);
}

void ARDMoonBuggy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	BoardCD = FMath::Max(0.f, BoardCD - DeltaSeconds);

	if (bOccupied && IsValid(Rider))
	{
		// Keep rider seated
		const FVector Seat = GetActorLocation() + FVector(0.f, 0.f, 140.f);
		Rider->SetActorLocation(Seat);
		if (AController* C = Rider->GetController())
		{
			// Face buggy heading
			const FRotator R = GetActorRotation();
			C->SetControlRotation(FRotator(C->GetControlRotation().Pitch, R.Yaw, 0.f));
		}
	}

	// Simple integration when driven
	if (!Velocity.IsNearlyZero())
	{
		FHitResult Hit;
		SetActorLocation(GetActorLocation() + Velocity * DeltaSeconds, true, &Hit);
		if (Hit.bBlockingHit)
		{
			Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal) * 0.4f;
		}
		Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, DeltaSeconds, 3.5f);
	}
}

bool ARDMoonBuggy::TryBoard(ARDCharacter* InRider)
{
	if (bOccupied || !InRider || BoardCD > 0.f) return false;
	if (FVector::Dist(GetActorLocation(), InRider->GetActorLocation()) > BoardRadius) return false;

	bOccupied = true;
	Rider = InRider;
	InRider->NotifyBoardedBuggy(this);

	if (UCharacterMovementComponent* Mov = InRider->GetCharacterMovement())
	{
		Mov->DisableMovement();
	}
	if (UCapsuleComponent* Cap = InRider->GetCapsuleComponent())
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(8830, 2.5f, FColor::Cyan, TEXT("BUGGY — E to disembark  |  WASD drive"));
	}
	return true;
}

void ARDMoonBuggy::Disembark()
{
	if (!bOccupied) return;
	bOccupied = false;
	BoardCD = 1.2f;
	Velocity = FVector::ZeroVector;

	if (IsValid(Rider))
	{
		const FVector Exit = GetActorLocation() + GetActorRightVector() * 220.f + FVector(0.f, 0.f, 100.f);
		Rider->SetActorLocation(Exit);
		if (UCharacterMovementComponent* Mov = Rider->GetCharacterMovement())
		{
			Mov->SetMovementMode(MOVE_Walking);
		}
		if (UCapsuleComponent* Cap = Rider->GetCapsuleComponent())
		{
			Cap->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		Rider->NotifyLeftBuggy();
	}
	Rider = nullptr;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(8830, 1.5f, FColor::Green, TEXT("Disembarked"));
	}
}

void ARDMoonBuggy::DriveInput(const FVector& WorldDir, float DeltaSeconds)
{
	if (!bOccupied) return;
	FVector Dir = WorldDir;
	Dir.Z = 0.f;
	if (Dir.IsNearlyZero())
	{
		return;
	}
	Dir.Normalize();
	Velocity = Dir * DriveSpeed;
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Dir.Rotation(), DeltaSeconds, 8.f));
}

void ARDMoonBuggy::TakeDamageAmount(float Amount)
{
	if (Amount <= 0.f || HP <= 0.f) return;
	HP -= Amount;
	if (GetWorld())
	{
		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 160),
			FString::Printf(TEXT("BUGGY %.0f"), HP), nullptr, FColor::Yellow, 0.4f, true);
	}
	if (HP <= 0.f) Die();
}

void ARDMoonBuggy::Die()
{
	if (bOccupied) Disembark();
	// Damage nearby not applied in 3D for simplicity; wreckage left
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (UWorld* World = GetWorld())
	{
		if (ARDWreckage* W = World->SpawnActor<ARDWreckage>(GetActorLocation(), GetActorRotation(), P))
		{
			W->ConfigureAsRobotHulk();
			W->Tags.Add(FName(TEXT("RD_Base")));
		}
	}
	Destroy();
}
