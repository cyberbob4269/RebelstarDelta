// Rebelstar Delta — leader + Tab squad + E buggy + HP

#include "RDCharacter.h"
#include "RDBot.h"
#include "RDDestructible.h"
#include "RDGameMode.h"
#include "RDIsaacNode.h"
#include "RDLaserFX.h"
#include "RDMoonBuggy.h"
#include "RDRoomDoor.h"
#include "RDWreckage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"

ARDCharacter::ARDCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->GravityScale = 0.16f;
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.35f;
	BaseWalkSpeed = 500.f;
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	GetCharacterMovement()->BrakingDecelerationWalking = 1400.f;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	Tags.Add(FName(TEXT("RD_Leader")));
}

void ARDCharacter::BeginPlay()
{
	Super::BeginPlay();
	HP = MaxHP;
	LeaderName = TEXT("Trump");
	CountryCode = TEXT("US");
	IssueOrder(ERDSquadOrder::Follow);
}

void ARDCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead) return;

	FireReady = FMath::Max(0.f, FireReady - DeltaSeconds);
	HitFlash = FMath::Max(0.f, HitFlash - DeltaSeconds);

	// Buggy drive
	if (bInBuggy && IsValid(CurrentBuggy) && Controller)
	{
		const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		FVector Dir = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * MoveAxisF
			+ FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * MoveAxisR;
		CurrentBuggy->DriveInput(Dir, DeltaSeconds);
		return;
	}

	// Manual ally control: move them, park leader body
	if (IsValid(ControlledAlly) && !ControlledAlly->IsDead() && Controller)
	{
		const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		FVector Dir = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * MoveAxisF
			+ FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * MoveAxisR;
		if (!Dir.IsNearlyZero())
		{
			const FVector Dest = ControlledAlly->GetActorLocation() + Dir.GetSafeNormal() * 400.f;
			// Use bot move helper via location step
			const float Step = ControlledAlly->MoveSpeed * DeltaSeconds;
			FHitResult Hit;
			ControlledAlly->SetActorLocation(
				ControlledAlly->GetActorLocation() + Dir.GetSafeNormal() * Step, true, &Hit);
			ControlledAlly->SetActorRotation(FMath::RInterpTo(
				ControlledAlly->GetActorRotation(), Dir.Rotation(), DeltaSeconds, 12.f));
		}
		UpdateControlCamera();
		// Freeze leader in place while remote-controlling
		GetCharacterMovement()->StopMovementImmediately();
	}
	else
	{
		// Wreckage crawl slow
		float SpeedMul = 1.f;
		bool bInWreck = false;
		TArray<AActor*> Wrecks;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDWreckage::StaticClass(), Wrecks);
		for (AActor* A : Wrecks)
		{
			if (const ARDWreckage* W = Cast<ARDWreckage>(A))
			{
				if (W->IsActorInside(this))
				{
					SpeedMul = FMath::Min(SpeedMul, W->GetSlowMultiplier());
					bInWreck = true;
				}
			}
		}
		GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * SpeedMul;
		if (bInWreck && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(8820, 0.1f, FColor::Yellow, TEXT("WRECKAGE — crawling…"));
		}
	}

	TickSupportZones(DeltaSeconds);
}

void ARDCharacter::TickSupportZones(float DeltaSeconds)
{
	// Fable 5: MED_ZONE (26,26,9,7) humans; WORKSHOP (5,17,11,6) robots — unpadded map cells
	// MapOrigin (4000,-9900), CS=300
	const FVector Origin(4000.f, -9900.f, 0.f);
	const float CS = 300.f;
	auto InRect = [&](const FVector& P, int32 X0, int32 Y0, int32 W, int32 H) -> bool
	{
		const float MinX = Origin.X + X0 * CS;
		const float MinY = Origin.Y + Y0 * CS;
		const float MaxX = Origin.X + (X0 + W) * CS;
		const float MaxY = Origin.Y + (Y0 + H) * CS;
		return P.X >= MinX && P.X <= MaxX && P.Y >= MinY && P.Y <= MaxY;
	};

	// Heal leader in medbay
	if (InRect(GetActorLocation(), 26, 26, 9, 7))
	{
		HealAmount(4.f * DeltaSeconds); // HEAL_RATE 4/s
		if (GEngine) GEngine->AddOnScreenDebugMessage(8840, 0.05f, FColor(1.f, 0.4f, 0.5f), TEXT("MEDBAY"));
	}

	// Repair controlled ally robots / all allies in workshop via GameMode tick preferred —
	// also heal any ally standing in zones
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	for (AActor* A : Bots)
	{
		ARDBot* B = Cast<ARDBot>(A);
		if (!B || B->IsDead()) continue;
		const FVector P = B->GetActorLocation();
		if (B->IsRobot())
		{
			if (InRect(P, 5, 17, 11, 6))
			{
				B->SetCurrentHP(FMath::Min(B->MaxHP, B->GetHP() + 6.f * DeltaSeconds));
			}
		}
		else if (InRect(P, 26, 26, 9, 7))
		{
			B->SetCurrentHP(FMath::Min(B->MaxHP, B->GetHP() + 4.f * DeltaSeconds));
		}
	}
}

void ARDCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ARDCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ARDCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ARDCharacter::Turn);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &ARDCharacter::LookUp);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ARDCharacter::OnJump);
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &ARDCharacter::OnFire);
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Repeat, this, &ARDCharacter::OnFire);
	PlayerInputComponent->BindAction(TEXT("Order1"), IE_Pressed, this, &ARDCharacter::OrderFollow);
	PlayerInputComponent->BindAction(TEXT("Order2"), IE_Pressed, this, &ARDCharacter::OrderAttack);
	PlayerInputComponent->BindAction(TEXT("Order3"), IE_Pressed, this, &ARDCharacter::OrderHold);
	PlayerInputComponent->BindAction(TEXT("Order4"), IE_Pressed, this, &ARDCharacter::OrderStorm);
	PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &ARDCharacter::OnInteract);
	PlayerInputComponent->BindAction(TEXT("CycleControl"), IE_Pressed, this, &ARDCharacter::OnCycleControl);
}

void ARDCharacter::MoveForward(float Value)
{
	MoveAxisF = Value;
	if (bInBuggy || IsValid(ControlledAlly)) return;
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Value);
	}
}

void ARDCharacter::MoveRight(float Value)
{
	MoveAxisR = Value;
	if (bInBuggy || IsValid(ControlledAlly)) return;
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Value);
	}
}

void ARDCharacter::Turn(float Value) { AddControllerYawInput(Value); }
void ARDCharacter::LookUp(float Value) { AddControllerPitchInput(Value); }
void ARDCharacter::OnJump()
{
	if (!bInBuggy && !IsValid(ControlledAlly)) Jump();
}

void ARDCharacter::GatherLivingAllies(TArray<ARDBot*>& Out) const
{
	Out.Reset();
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	for (AActor* A : Bots)
	{
		if (ARDBot* B = Cast<ARDBot>(A))
		{
			if (B->Team == ERDBotTeam::Ally && !B->IsDead()) Out.Add(B);
		}
	}
}

void ARDCharacter::OnCycleControl()
{
	if (bDead || bInBuggy) return;

	TArray<ARDBot*> Allies;
	GatherLivingAllies(Allies);

	// Release previous
	if (IsValid(ControlledAlly))
	{
		ControlledAlly->SetPlayerDriven(false);
	}

	// Cycle: self (-1) → ally0 → ally1 → … → self
	ControlIndex++;
	if (ControlIndex >= Allies.Num())
	{
		ControlIndex = -1;
		ControlledAlly = nullptr;
		if (FirstPersonCamera)
		{
			FirstPersonCamera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			FirstPersonCamera->AttachToComponent(GetCapsuleComponent(),
				FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
			FirstPersonCamera->bUsePawnControlRotation = true;
		}
		if (GEngine) GEngine->AddOnScreenDebugMessage(8850, 1.5f, FColor::Green, TEXT("CONTROL: LEADER (you)"));
		return;
	}

	ControlledAlly = Allies[ControlIndex];
	ControlledAlly->SetPlayerDriven(true);
	UpdateControlCamera();
	const FString Label = ControlledAlly->UnitName.IsEmpty()
		? TEXT("ALLY")
		: ControlledAlly->UnitName;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(8850, 1.5f, FColor::Cyan,
			FString::Printf(TEXT("CONTROL: %s%s"), *Label, ControlledAlly->IsRobot() ? TEXT(" [ROBOT]") : TEXT("")));
	}
}

void ARDCharacter::UpdateControlCamera()
{
	if (!IsValid(ControlledAlly) || !FirstPersonCamera) return;
	FirstPersonCamera->AttachToComponent(ControlledAlly->GetRootComponent(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	FirstPersonCamera->SetRelativeLocation(FVector(30.f, 0.f, 70.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
}

void ARDCharacter::OnInteract()
{
	if (bDead) return;
	if (bInBuggy && IsValid(CurrentBuggy))
	{
		CurrentBuggy->Disembark();
		return;
	}
	// Find nearby buggy
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDMoonBuggy::StaticClass(), Found);
	ARDMoonBuggy* Best = nullptr;
	float BestD = 320.f;
	for (AActor* A : Found)
	{
		if (ARDMoonBuggy* B = Cast<ARDMoonBuggy>(A))
		{
			const float D = FVector::Dist(GetActorLocation(), B->GetActorLocation());
			if (D < BestD) { BestD = D; Best = B; }
		}
	}
	if (Best)
	{
		Best->TryBoard(this);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(8831, 1.2f, FColor::White, TEXT("No buggy nearby (E)"));
	}
}

void ARDCharacter::NotifyBoardedBuggy(ARDMoonBuggy* Buggy)
{
	bInBuggy = true;
	CurrentBuggy = Buggy;
	// Drop ally control
	ControlledAlly = nullptr;
	ControlIndex = -1;
	if (FirstPersonCamera)
	{
		FirstPersonCamera->AttachToComponent(GetCapsuleComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		FirstPersonCamera->SetRelativeLocation(FVector(40.f, 0.f, 50.f));
	}
}

void ARDCharacter::NotifyLeftBuggy()
{
	bInBuggy = false;
	CurrentBuggy = nullptr;
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	}
}

FString ARDCharacter::GetControlLabel() const
{
	if (bInBuggy) return TEXT("BUGGY");
	if (IsValid(ControlledAlly))
	{
		return ControlledAlly->UnitName.IsEmpty() ? TEXT("ALLY") : ControlledAlly->UnitName;
	}
	return LeaderName.IsEmpty() ? TEXT("TRUMP") : LeaderName.ToUpper();
}

void ARDCharacter::HealAmount(float Amount)
{
	if (bDead || Amount <= 0.f) return;
	HP = FMath::Min(MaxHP, HP + Amount);
}

void ARDCharacter::ApplyDamage(float Amount)
{
	if (bDead || Amount <= 0.f) return;
	if (bSpectatorInvulnerable) return;
	if (bInBuggy) Amount *= 0.65f;

	HP -= Amount;
	HitFlash = 0.25f;
	if (GetWorld())
	{
		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 120),
			FString::Printf(TEXT("YOU %.0f"), HP), nullptr, FColor::Red, 0.5f, true, 1.3f);
	}
	if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->RaiseAlarm(GetActorLocation());
	}
	if (HP <= 0.f)
	{
		Die();
	}
}

void ARDCharacter::Die()
{
	if (bDead) return;
	bDead = true;
	if (bInBuggy && IsValid(CurrentBuggy)) CurrentBuggy->Disembark();
	ControlledAlly = nullptr;
	GetCharacterMovement()->DisableMovement();
	if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->NotifyLeaderKilled();
	}
}

void ARDCharacter::IssueOrder(ERDSquadOrder Order)
{
	CurrentOrder = Order;
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	int32 N = 0;
	for (AActor* A : Bots)
	{
		if (ARDBot* B = Cast<ARDBot>(A))
		{
			if (B->Team == ERDBotTeam::Ally && !B->IsDead())
			{
				// Don't override the unit we are manually controlling — still set order for when we leave
				B->SetOrder(Order);
				++N;
			}
		}
	}

	FString Msg = TEXT("FOLLOW");
	switch (Order)
	{
	case ERDSquadOrder::Attack: Msg = TEXT("ATTACK"); break;
	case ERDSquadOrder::Hold: Msg = TEXT("HOLD"); break;
	case ERDSquadOrder::Storm: Msg = TEXT("STORM ISAAC"); break;
	default: break;
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(7701, 2.0f, FColor::Green,
			FString::Printf(TEXT("SQUAD: %s (%d)"), *Msg, N));
	}
}

void ARDCharacter::OrderFollow() { IssueOrder(ERDSquadOrder::Follow); }
void ARDCharacter::OrderAttack() { IssueOrder(ERDSquadOrder::Attack); }
void ARDCharacter::OrderHold() { IssueOrder(ERDSquadOrder::Hold); }
void ARDCharacter::OrderStorm() { IssueOrder(ERDSquadOrder::Storm); }

FVector ARDCharacter::GetFireStart() const
{
	if (IsValid(ControlledAlly) && FirstPersonCamera)
	{
		return FirstPersonCamera->GetComponentLocation();
	}
	if (FirstPersonCamera) return FirstPersonCamera->GetComponentLocation();
	return GetActorLocation() + FVector(0.f, 0.f, 64.f);
}

FVector ARDCharacter::GetFireForward() const
{
	if (Controller) return Controller->GetControlRotation().Vector();
	if (FirstPersonCamera) return FirstPersonCamera->GetForwardVector();
	return GetActorForwardVector();
}

void ARDCharacter::ApplyShotAt(const FVector& Start, const FVector& End)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Draw + sound first (beam stops at first hit below)

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RDLaser), false, this);
	if (IsValid(ControlledAlly)) Params.AddIgnoredActor(ControlledAlly);

	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	const FVector BeamEnd = bHit ? Hit.ImpactPoint : End;
	RDLaserFX::DrawBeam(World, Start, BeamEnd, FColor(40, 230, 255), 18.f, 0.3f);
	RDLaserFX::PlayZap(World, Start, true);

	if (!bHit) return;
	AActor* HitActor = Hit.GetActor();
	if (!HitActor) return;

	if (ARDRoomDoor* Door = Cast<ARDRoomDoor>(HitActor))
	{
		Door->TakeDamageAmount(LaserDamage);
		return;
	}
	if (ARDWreckage* Wreck = Cast<ARDWreckage>(HitActor))
	{
		Wreck->TakeDamageAmount(LaserDamage);
		return;
	}
	if (ARDDestructible* Prop = Cast<ARDDestructible>(HitActor))
	{
		Prop->TakeDamageAmount(LaserDamage);
		if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
			GM->RaiseAlarm(Hit.ImpactPoint);
		return;
	}
	if (ARDMoonBuggy* Bug = Cast<ARDMoonBuggy>(HitActor))
	{
		// don't shoot own buggy while riding
		if (!bInBuggy) Bug->TakeDamageAmount(LaserDamage * 0.5f);
		return;
	}
	if (AActor* HitOwner = HitActor->GetOwner())
	{
		if (ARDRoomDoor* Door = Cast<ARDRoomDoor>(HitOwner)) { Door->TakeDamageAmount(LaserDamage); return; }
		if (ARDWreckage* Wreck = Cast<ARDWreckage>(HitOwner)) { Wreck->TakeDamageAmount(LaserDamage); return; }
		if (ARDDestructible* Prop = Cast<ARDDestructible>(HitOwner)) { Prop->TakeDamageAmount(LaserDamage); return; }
	}

	if (ARDIsaacNode* Isaac = Cast<ARDIsaacNode>(HitActor))
	{
		Isaac->TakeDamageAmount(LaserDamage);
		if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
			GM->RaiseAlarm(Hit.ImpactPoint);
		return;
	}

	if (ARDBot* Bot = Cast<ARDBot>(HitActor))
	{
		// Don't teamkill allies
		if (Bot->Team == ERDBotTeam::Defender) Bot->ApplyDamage(LaserDamage);
		return;
	}

	if (HitActor->ActorHasTag(FName(TEXT("RD_Breach"))))
	{
		HitActor->Destroy();
		if (ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(this)))
			GM->RaiseAlarm(Hit.ImpactPoint);
	}
}

void ARDCharacter::OnFire()
{
	AgentFire();
}

void ARDCharacter::AgentFire()
{
	if (bDead || FireReady > 0.f || bInBuggy || !GetWorld()) return; // unarmed in buggy
	FireReady = FireCooldown;

	const FVector Start = GetFireStart();
	const FVector End = Start + GetFireForward() * LaserRange;
	ApplyShotAt(Start, End);
}
