// Rebelstar Delta — big pulsing server racks

#include "RDIsaacNode.h"
#include "RDGameMode.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ARDIsaacNode::ARDIsaacNode()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lit(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Unlit(TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"));
	if (Cube.Succeeded()) CubeMesh = Cube.Object;
	if (Lit.Succeeded()) LitMat = Lit.Object;
	if (Unlit.Succeeded()) UnlitMat = Unlit.Object;
	if (!UnlitMat) UnlitMat = LitMat;

	Chassis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chassis"));
	Chassis->SetupAttachment(Root);
	DoorL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorL"));
	DoorL->SetupAttachment(Root);
	DoorR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorR"));
	DoorR->SetupAttachment(Root);
	TopBeacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TopBeacon"));
	TopBeacon->SetupAttachment(Root);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(Root);
	GlowLight->SetRelativeLocation(FVector(100.f, 0.f, 200.f));
	GlowLight->SetIntensity(45000.f);
	GlowLight->SetAttenuationRadius(2800.f);
	GlowLight->SetLightColor(FLinearColor(0.2f, 0.95f, 1.0f));
	GlowLight->SetCastShadows(false);

	Tags.Add(FName(TEXT("RD_Isaac")));
}

void ARDIsaacNode::BeginPlay()
{
	Super::BeginPlay();
	HP = MaxHP;
	BuildServerRack();
	PulseLook(0.f);

	if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->RegisterIsaac(this);
	}
}

void ARDIsaacNode::BuildServerRack()
{
	if (!CubeMesh)
	{
		return;
	}

	auto Setup = [&](UStaticMeshComponent* M, const FVector& Rel, const FVector& Scale, bool bCol)
	{
		if (!M) return;
		M->SetStaticMesh(CubeMesh);
		M->SetRelativeLocation(Rel);
		M->SetRelativeScale3D(Scale);
		M->SetCollisionEnabled(bCol ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bCol)
		{
			M->SetCollisionProfileName(TEXT("BlockAll"));
		}
		M->SetCastShadow(true);
	};

	// Tall dark server chassis (~3.5m hero rack)
	Setup(Chassis, FVector(0.f, 0.f, 200.f), FVector(1.8f, 2.8f, 4.0f), true);
	// Glass-ish front doors (cyan panels)
	Setup(DoorL, FVector(95.f, -70.f, 190.f), FVector(0.15f, 1.2f, 3.4f), false);
	Setup(DoorR, FVector(95.f, 70.f, 190.f), FVector(0.15f, 1.2f, 3.4f), false);
	// Roof beacon
	Setup(TopBeacon, FVector(0.f, 0.f, 430.f), FVector(0.7f, 0.7f, 0.45f), false);

	// LED strip rows on front (unlit blink)
	BlinkLeds.Reset();
	for (int32 Row = 0; Row < 8; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			const FName Name(*FString::Printf(TEXT("LED_%d_%d"), Row, Col));
			UStaticMeshComponent* Led = NewObject<UStaticMeshComponent>(this, Name);
			Led->SetupAttachment(Root);
			Led->RegisterComponent();
			Led->SetStaticMesh(CubeMesh);
			Led->SetRelativeLocation(FVector(82.f, -70.f + Col * 45.f, 40.f + Row * 35.f));
			Led->SetRelativeScale3D(FVector(0.08f, 0.25f, 0.12f));
			Led->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Led->SetCastShadow(false);
			BlinkLeds.Add(Led);
		}
	}

	// Side rails / rack ears
	for (int32 S = -1; S <= 1; S += 2)
	{
		UStaticMeshComponent* Rail = NewObject<UStaticMeshComponent>(this);
		Rail->SetupAttachment(Root);
		Rail->RegisterComponent();
		Rail->SetStaticMesh(CubeMesh);
		Rail->SetRelativeLocation(FVector(0.f, S * 125.f, 160.f));
		Rail->SetRelativeScale3D(FVector(1.5f, 0.15f, 3.3f));
		Rail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ApplyPartColor(Rail, FLinearColor(0.15f, 0.16f, 0.18f), false);
	}
}

void ARDIsaacNode::ApplyPartColor(UStaticMeshComponent* Part, const FLinearColor& Color, bool bUnlit)
{
	if (!Part)
	{
		return;
	}
	UMaterialInterface* Parent = bUnlit ? UnlitMat.Get() : LitMat.Get();
	if (!Parent)
	{
		return;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
	if (!MID)
	{
		return;
	}
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
	MID->SetVectorParameterValue(TEXT("Tint"), Color);
	MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
	Part->SetMaterial(0, MID);
}

void ARDIsaacNode::PulseLook(float Pulse01)
{
	if (bDead)
	{
		return;
	}

	// Chassis dark metal
	ApplyPartColor(Chassis, FLinearColor(0.12f, 0.14f, 0.16f), false);

	// Front doors: bright cyan that breathes
	const float Glow = 0.45f + 0.55f * Pulse01;
	const FLinearColor DoorCol(0.1f * Glow, 0.9f * Glow, 1.0f * Glow);
	ApplyPartColor(DoorL, DoorCol, true);
	ApplyPartColor(DoorR, DoorCol, true);

	// Beacon strobe
	const FLinearColor BeaconCol(0.2f, 1.0f * Glow, 1.0f * Glow);
	ApplyPartColor(TopBeacon, BeaconCol, true);

	// LEDs chase / blink
	for (int32 i = 0; i < BlinkLeds.Num(); ++i)
	{
		const float Phase = FMath::Fmod(PulseTime * 3.f + i * 0.35f, 1.f);
		const bool bOn = Phase < 0.55f;
		const float LedG = bOn ? (0.7f + 0.3f * Pulse01) : 0.08f;
		ApplyPartColor(BlinkLeds[i], FLinearColor(0.05f, LedG, 0.15f + LedG * 0.5f), true);
	}

	if (GlowLight)
	{
		const float HitBoost = HitFlash > 0.f ? 1.8f : 1.f;
		GlowLight->SetIntensity((18000.f + 22000.f * Pulse01) * HitBoost);
		GlowLight->SetLightColor(FLinearColor(0.15f, 0.95f, 1.0f));
	}

	// Slight scale pulse so it “breathes”
	const float S = 1.f + 0.035f * Pulse01;
	if (Chassis)
	{
		Chassis->SetRelativeScale3D(FVector(1.8f * S, 2.8f * S, 4.0f));
	}
}

void ARDIsaacNode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead)
	{
		return;
	}

	PulseTime += DeltaSeconds;
	HitFlash = FMath::Max(0.f, HitFlash - DeltaSeconds);
	// Smooth sine pulse 0..1
	const float Pulse01 = 0.5f + 0.5f * FMath::Sin(PulseTime * 2.8f);
	PulseLook(Pulse01);
}

void ARDIsaacNode::SpawnDebrisBurst()
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh)
	{
		return;
	}

	const FVector Origin = GetActorLocation() + FVector(0.f, 0.f, 160.f);
	for (int32 i = 0; i < 18; ++i)
	{
		const FVector Offset = FMath::VRand() * FMath::FRandRange(40.f, 180.f);
		AStaticMeshActor* Chunk = World->SpawnActor<AStaticMeshActor>(Origin + Offset, FRotator::MakeFromEuler(FMath::VRand() * 180.f));
		if (!Chunk)
		{
			continue;
		}
		Chunk->SetMobility(EComponentMobility::Movable);
		UStaticMeshComponent* SMC = Chunk->GetStaticMeshComponent();
		SMC->SetMobility(EComponentMobility::Movable);
		SMC->SetStaticMesh(CubeMesh);
		SMC->SetWorldScale3D(FVector(FMath::FRandRange(0.15f, 0.45f)));
		SMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SMC->SetSimulatePhysics(true);
		SMC->SetPhysicsLinearVelocity(Offset.GetSafeNormal() * FMath::FRandRange(600.f, 1600.f) + FVector(0.f, 0.f, 400.f));
		SMC->AddAngularImpulseInDegrees(FMath::VRand() * 900.f, NAME_None, true);

		UMaterialInterface* Parent = UnlitMat ? UnlitMat.Get() : LitMat.Get();
		if (Parent)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
			if (MID)
			{
				const FLinearColor C(0.2f, FMath::FRandRange(0.6f, 1.f), 1.f);
				MID->SetVectorParameterValue(TEXT("Color"), C);
				MID->SetVectorParameterValue(TEXT("BaseColor"), C);
				SMC->SetMaterial(0, MID);
			}
		}
		Chunk->Tags.Add(FName(TEXT("RD_Debris")));
		// Auto-destroy debris after a few seconds
		Chunk->SetLifeSpan(FMath::FRandRange(2.5f, 4.5f));
	}

	// Explosion flash light
	if (UPointLightComponent* Flash = NewObject<UPointLightComponent>(this))
	{
		Flash->RegisterComponent();
		Flash->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		Flash->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
		Flash->SetIntensity(120000.f);
		Flash->SetAttenuationRadius(3500.f);
		Flash->SetLightColor(FLinearColor(0.4f, 0.9f, 1.f));
		Flash->SetCastShadows(false);
	}
}

void ARDIsaacNode::Explode()
{
	bDead = true;

	// Hide rack parts
	TArray<UStaticMeshComponent*> Parts = { Chassis, DoorL, DoorR, TopBeacon };
	for (UStaticMeshComponent* P : Parts)
	{
		if (P)
		{
			P->SetVisibility(false);
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UStaticMeshComponent* Led : BlinkLeds)
	{
		if (Led)
		{
			Led->SetVisibility(false);
		}
	}
	if (GlowLight)
	{
		GlowLight->SetIntensity(0.f);
	}

	SpawnDebrisBurst();

	// Scorch pad left behind
	if (GetWorld() && CubeMesh)
	{
		AStaticMeshActor* Scar = GetWorld()->SpawnActor<AStaticMeshActor>(
			GetActorLocation() + FVector(0.f, 0.f, 15.f), FRotator::ZeroRotator);
		if (Scar)
		{
			Scar->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* SMC = Scar->GetStaticMeshComponent();
			SMC->SetMobility(EComponentMobility::Movable);
			SMC->SetStaticMesh(CubeMesh);
			SMC->SetWorldScale3D(FVector(2.2f, 2.5f, 0.15f));
			SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (LitMat)
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(LitMat, this);
				if (MID)
				{
					const FLinearColor C(0.08f, 0.08f, 0.09f);
					MID->SetVectorParameterValue(TEXT("Color"), C);
					MID->SetVectorParameterValue(TEXT("BaseColor"), C);
					SMC->SetMaterial(0, MID);
				}
			}
			Scar->Tags.Add(FName(TEXT("RD_Base")));
		}
	}

	if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->NotifyIsaacDestroyed(this);
	}
}

void ARDIsaacNode::TakeDamageAmount(float Amount)
{
	if (bDead || Amount <= 0.f)
	{
		return;
	}
	HP = FMath::Max(0.f, HP - Amount);
	HitFlash = 0.25f;
	// Flash white-hot on hit
	if (GlowLight)
	{
		GlowLight->SetIntensity(90000.f);
		GlowLight->SetLightColor(FLinearColor(1.f, 1.f, 1.f));
	}
	if (HP <= 0.f)
	{
		Explode();
	}
}
