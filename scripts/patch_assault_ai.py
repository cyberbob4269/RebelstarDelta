from pathlib import Path

path = Path(r"C:\UE\RebelstarDelta\Source\RebelstarDelta\RDBot.cpp")
text = path.read_text(encoding="utf-8")

if "RDDestructible.h" not in text:
    text = text.replace(
        '#include "RDCharacter.h"\n',
        '#include "RDCharacter.h"\n#include "RDDestructible.h"\n',
    )

start = text.find(
    "void ARDBot::MoveToward(const FVector& WorldTarget, float DeltaSeconds, float SpeedScale)"
)
end = text.find("AActor* ARDBot::FindNearestEnemy() const")
if start < 0 or end < 0:
    raise SystemExit(f"MoveToward markers not found start={start} end={end}")

new_move = r"""FVector ARDBot::ComputeSeparation() const
{
	FVector Push = FVector::ZeroVector;
	const float SepR = bIsRobot ? 160.f : 130.f;
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	for (AActor* A : Bots)
	{
		const ARDBot* O = Cast<ARDBot>(A);
		if (!O || O == this || O->IsDead() || O->Team != Team) continue;
		FVector D = GetActorLocation() - O->GetActorLocation();
		D.Z = 0.f;
		const float L = D.Size();
		if (L < 1.f || L > SepR) continue;
		Push += (D / L) * (SepR - L);
	}
	return Push;
}

bool ARDBot::ShouldYieldAtChoke(const FVector& ChokeLoc, float Radius) const
{
	const float MyD = FVector::Dist2D(GetActorLocation(), ChokeLoc);
	if (MyD > Radius + 80.f) return false;
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	int32 Closer = 0;
	for (AActor* A : Bots)
	{
		const ARDBot* O = Cast<ARDBot>(A);
		if (!O || O == this || O->IsDead() || O->Team != Team) continue;
		const float OD = FVector::Dist2D(O->GetActorLocation(), ChokeLoc);
		if (OD + 25.f < MyD && OD < Radius)
		{
			++Closer;
			if (!bIsRobot && O->IsRobot()) return true;
		}
	}
	const int32 MaxInChoke = bIsRobot ? 2 : 1;
	return Closer >= MaxInChoke;
}

int32 ARDBot::CountAlliesNear(const FVector& Loc, float Radius) const
{
	int32 N = 0;
	TArray<AActor*> Bots;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDBot::StaticClass(), Bots);
	for (AActor* A : Bots)
	{
		const ARDBot* O = Cast<ARDBot>(A);
		if (!O || O->IsDead() || O->Team != Team) continue;
		if (FVector::Dist2D(O->GetActorLocation(), Loc) < Radius) ++N;
	}
	return N;
}

void ARDBot::MoveToward(const FVector& WorldTarget, float DeltaSeconds, float SpeedScale)
{
	FVector To = WorldTarget - GetActorLocation();
	To.Z = 0.f;
	const float Dist = To.Size();
	if (Dist < 70.f)
	{
		const FVector Sep = ComputeSeparation();
		if (Sep.Size() > 8.f)
		{
			FVector Nudge = GetActorLocation() + Sep.GetSafeNormal() * MoveSpeed * 0.6f * DeltaSeconds;
			Nudge.Z = FMath::Clamp(GetActorLocation().Z, 100.f, 160.f);
			FHitResult H;
			SetActorLocation(Nudge, true, &H);
		}
		return;
	}

	FVector Dir = To / Dist;
	const FVector Sep = ComputeSeparation();
	if (Sep.Size() > 1.f)
	{
		const float SepW = FMath::Clamp(Sep.Size() / 140.f, 0.f, 1.35f);
		Dir = (Dir + Sep.GetSafeNormal() * SepW * 1.1f).GetSafeNormal();
	}

	const int32 Crowd = CountAlliesNear(GetActorLocation(), 200.f);
	float CrowdSlow = 1.f;
	if (Crowd >= 3) CrowdSlow = 0.55f;
	else if (Crowd == 2) CrowdSlow = 0.75f;

	const float Step = MoveSpeed * SpeedScale * CrowdSlow * DeltaSeconds;
	FVector Desired = GetActorLocation() + Dir * FMath::Min(Step, Dist);
	Desired.Z = FMath::Clamp(GetActorLocation().Z, 100.f, 160.f);

	FHitResult Hit;
	SetActorLocation(Desired, true, &Hit);
	if (Hit.bBlockingHit)
	{
		const FVector Slide = FVector::VectorPlaneProject(Dir, Hit.Normal).GetSafeNormal() * Step * 0.95f;
		FVector SlidePos = GetActorLocation() + Slide + Sep.GetSafeNormal() * Step * 0.35f;
		SlidePos.Z = Desired.Z;
		FHitResult Hit2;
		SetActorLocation(SlidePos, true, &Hit2);
		if (Hit2.bBlockingHit)
		{
			const FVector Side(-Dir.Y, Dir.X, 0.f);
			for (float S : { 1.f, -1.f })
			{
				FVector Alt = GetActorLocation() + Side * S * Step * 0.95f + Dir * Step * 0.2f;
				Alt.Z = Desired.Z;
				FHitResult Hit3;
				SetActorLocation(Alt, true, &Hit3);
				if (!Hit3.bBlockingHit) break;
			}
		}
	}

	if (!IsLocationClear(GetActorLocation()))
	{
		ResolveWallPenetration();
	}
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Dir.Rotation(), DeltaSeconds, 12.f));
	}
}

"""
text = text[:start] + new_move + text[end:]

marker = "AActor* ARDBot::FindNearestFriendlyRobot() const"
idx = text.find(marker)
if idx < 0:
    raise SystemExit("FindNearestFriendlyRobot not found")
brace = text.find("{", idx)
depth = 0
i = brace
while i < len(text):
    if text[i] == "{":
        depth += 1
    elif text[i] == "}":
        depth -= 1
        if depth == 0:
            i += 1
            break
    i += 1

insert = r"""

AActor* ARDBot::FindNearestBreachable() const
{
	TArray<AActor*> Props;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARDDestructible::StaticClass(), Props);
	AActor* Best = nullptr;
	float BestD = 2200.f;
	for (AActor* A : Props)
	{
		const ARDDestructible* D = Cast<ARDDestructible>(A);
		if (!D || D->IsDestroyed()) continue;
		const bool bDoorish = D->Kind.Contains(TEXT("door"), ESearchCase::IgnoreCase)
			|| D->Kind.Contains(TEXT("Sealed"), ESearchCase::IgnoreCase)
			|| A->ActorHasTag(FName(TEXT("RD_Breach")));
		const float Dist = FVector::Dist(GetActorLocation(), A->GetActorLocation());
		const float Score = Dist * (bDoorish ? 0.55f : 1.f);
		if (Score < BestD) { BestD = Score; Best = A; }
	}
	return Best;
}

void ARDBot::TryBreachTarget(AActor* Target, float DamageMul)
{
	if (!Target || !GetWorld() || FireCD > 0.f || bDead) return;
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 70.f);
	const FVector Aim = Target->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const float Dist = FVector::Dist(Start, Aim);
	if (Dist > WeaponRange * 1.05f) return;

	const bool bClose = Dist < 650.f;
	if (!bClose)
	{
		FHitResult LosHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RDBreachLos), false, this);
		if (GetWorld()->LineTraceSingleByChannel(LosHit, Start, Aim, ECC_Visibility, Params))
		{
			if (LosHit.GetActor() != Target && LosHit.GetActor() != Target->GetOwner())
			{
				return;
			}
		}
	}

	FireCD = FireInterval * (bIsRobot ? 0.45f : 0.55f);
	const FColor Beam = (Team == ERDBotTeam::Ally)
		? (bIsRobot ? FColor(40, 230, 255) : FColor(60, 255, 90))
		: FColor(255, 40, 30);
	RDLaserFX::DrawBeam(GetWorld(), Start, Aim, Beam, bIsRobot ? 20.f : 15.f, 0.3f);
	RDLaserFX::PlayZap(GetWorld(), Start, Team == ERDBotTeam::Ally);

	const float Dmg = ShotDamage * DamageMul * (bIsRobot ? 1.25f : 1.f);
	if (ARDRoomDoor* Door = Cast<ARDRoomDoor>(Target))
	{
		Door->TakeDamageAmount(Dmg * 2.2f);
	}
	else if (ARDWreckage* W = Cast<ARDWreckage>(Target))
	{
		W->TakeDamageAmount(Dmg * 1.8f);
	}
	else if (ARDDestructible* Prop = Cast<ARDDestructible>(Target))
	{
		Prop->TakeDamageAmount(Dmg * 1.6f);
	}
}
"""
text = text[:i] + insert + text[i:]

storm_fn = r"""
void ARDBot::TickAllyStormAssault(float DeltaSeconds, AActor* Enemy, AActor* Isaac, const FVector& AssaultRally)
{
	const FVector Me = GetActorLocation();
	// 0=BREACH 1=SUPPORT 2=DISTRACT  (avoid name Role — shadows AActor::Role)
	int32 SquadRole = FormationSlot % 3;
	if (bIsRobot || UnitName.Equals(TEXT("Trump"), ESearchCase::IgnoreCase)) SquadRole = 0;
	else if (UnitName.Contains(TEXT("Haley")) || UnitName.Contains(TEXT("Rubio"))) SquadRole = 2;

	AActor* DoorOrWreck = FindNearestRoomDoor();
	AActor* BreachProp = FindNearestBreachable();
	AActor* PrimaryBreach = DoorOrWreck ? DoorOrWreck : BreachProp;

	const float ApproachGateX = 5600.f;
	if (Me.X < ApproachGateX)
	{
		FVector Way = AssaultRally;
		Way.Y += (FormationSlot - 1) * 140.f;
		if (SquadRole == 2)
		{
			Way.Y += (UnitName.Contains(TEXT("Haley")) ? -550.f : 550.f);
		}
		if (!bIsRobot)
		{
			if (AActor* Tank = FindNearestFriendlyRobot())
			{
				const FVector T = Tank->GetActorLocation();
				if (T.X > Me.X - 80.f && SquadRole != 2)
				{
					Way = T + FVector(-260.f, (FormationSlot - 1) * 90.f, 0.f);
				}
			}
		}
		if (ShouldYieldAtChoke(AssaultRally, 380.f) && SquadRole != 0)
		{
			const FVector Hold = AssaultRally + FVector(-400.f, (FormationSlot - 1) * 180.f, 0.f);
			MoveToward(Hold, DeltaSeconds, 0.7f);
			if (Enemy) EngageTactically(Enemy, DeltaSeconds);
			return;
		}
		MoveToward(Way, DeltaSeconds, bIsRobot ? 1.55f : (SquadRole == 2 ? 1.45f : 1.25f));
		if (Enemy && FVector::Dist(Me, Enemy->GetActorLocation()) < 1200.f)
		{
			EngageTactically(Enemy, DeltaSeconds);
		}
		if (BreachProp && FVector::Dist(Me, BreachProp->GetActorLocation()) < 900.f)
		{
			TryBreachTarget(BreachProp, 1.4f);
		}
		return;
	}

	if (SquadRole == 2 && PrimaryBreach)
	{
		const FVector DoorL = PrimaryBreach->GetActorLocation();
		const FVector Flank = DoorL + FVector(-200.f, (UnitName.Contains(TEXT("Haley")) ? -700.f : 700.f), 0.f);
		if (FVector::Dist2D(Me, Flank) > 200.f)
		{
			MoveToward(Flank, DeltaSeconds, 1.3f);
		}
		if (Enemy) EngageTactically(Enemy, DeltaSeconds);
		else TryBreachTarget(PrimaryBreach, 1.1f);
		return;
	}

	if (SquadRole == 1 && PrimaryBreach)
	{
		const FVector DoorL = PrimaryBreach->GetActorLocation();
		const FVector Support = DoorL + FVector(-450.f, (FormationSlot - 1) * 160.f, 0.f);
		if (ShouldYieldAtChoke(DoorL, 500.f) || FVector::Dist2D(Me, Support) > 180.f)
		{
			MoveToward(Support, DeltaSeconds, ShouldYieldAtChoke(DoorL, 500.f) ? 0.85f : 1.15f);
		}
		if (Enemy && FVector::Dist(Me, Enemy->GetActorLocation()) < 1600.f)
		{
			EngageTactically(Enemy, DeltaSeconds);
		}
		TryBreachTarget(PrimaryBreach, 1.5f);
		if (BreachProp && BreachProp != PrimaryBreach) TryBreachTarget(BreachProp, 1.2f);
		return;
	}

	if (PrimaryBreach)
	{
		const FVector DoorL = PrimaryBreach->GetActorLocation();
		const float D = FVector::Dist(Me, DoorL);
		if (ShouldYieldAtChoke(DoorL, 400.f) && !bIsRobot)
		{
			const FVector Wait = DoorL + FVector(-380.f, (FormationSlot - 1) * 120.f, 0.f);
			MoveToward(Wait, DeltaSeconds, 0.7f);
			TryBreachTarget(PrimaryBreach, 1.3f);
			if (Enemy) EngageTactically(Enemy, DeltaSeconds);
			return;
		}
		if (D > 220.f)
		{
			FVector DoorGoal = DoorL + FVector(0.f, bIsRobot ? 0.f : 40.f * (FormationSlot - 1), 0.f);
			MoveToward(DoorGoal, DeltaSeconds, bIsRobot ? 1.5f : 1.2f);
		}
		TryBreachTarget(PrimaryBreach, bIsRobot ? 2.4f : 1.9f);
		if (Enemy && FVector::Dist(Me, Enemy->GetActorLocation()) < 700.f)
		{
			if (bIsRobot) EngageTactically(Enemy, DeltaSeconds);
			else if (const ARDBot* EB = Cast<ARDBot>(Enemy))
			{
				if (!EB->IsRobot()) EngageTactically(Enemy, DeltaSeconds);
				else TryFireAt(Enemy);
			}
		}
		return;
	}

	if (Isaac)
	{
		const float D = FVector::Dist(Me, Isaac->GetActorLocation());
		if (ShouldYieldAtChoke(Isaac->GetActorLocation(), 350.f) && SquadRole != 0)
		{
			MoveToward(Isaac->GetActorLocation() + FVector(-300.f, (FormationSlot - 1) * 150.f, 0.f), DeltaSeconds, 0.9f);
		}
		else if (D > 300.f)
		{
			MoveToward(Isaac->GetActorLocation(), DeltaSeconds, 1.35f);
		}
		TryFireAt(Isaac);
		if (Enemy && D < 1100.f) EngageTactically(Enemy, DeltaSeconds);
		return;
	}

	if (Enemy) EngageTactically(Enemy, DeltaSeconds);
	else MoveToward(AssaultRally + FVector(2000.f, 0.f, 0.f), DeltaSeconds, 1.2f);
}

"""

tick = text.find("void ARDBot::TickAlly(float DeltaSeconds)")
if tick < 0:
    raise SystemExit("TickAlly not found")
text = text[:tick] + storm_fn + text[tick:]

old_storm_start = text.find("case ERDSquadOrder::Storm:")
follow = text.find("case ERDSquadOrder::Follow:", old_storm_start)
if old_storm_start < 0 or follow < 0:
    raise SystemExit(f"storm case markers bad {old_storm_start} {follow}")
replacement = """case ERDSquadOrder::Storm:
		TickAllyStormAssault(DeltaSeconds, Enemy, Isaac, AssaultRally);
		break;

	"""
text = text[:old_storm_start] + replacement + text[follow:]

path.write_text(text, encoding="utf-8")
print("Patched OK, bytes", path.stat().st_size)
assert "ComputeSeparation" in text
assert "TickAllyStormAssault" in text
assert "TryBreachTarget" in text
assert "int32 Role" not in text
print("sanity checks passed")
