// Rebelstar Delta — large, minimal HUD

#include "RDHUD.h"
#include "RDCharacter.h"
#include "RDGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"

void ARDHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine) return;

	ARDGameMode* GM = Cast<ARDGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	ARDCharacter* PC = Cast<ARDCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	// Prefer larger engine fonts; scale further for big monitors
	UFont* Font = GEngine->GetLargeFont();
	if (!Font) Font = GEngine->GetMediumFont();
	if (!Font) Font = GEngine->GetSmallFont();

	const float Scale = 2.8f;
	const float LineH = 52.f;
	const float X = 48.f;
	float Y = 40.f;

	auto DrawLine = [&](const FString& Text, const FLinearColor& Col)
	{
		FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Col);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		Item.Scale = FVector2D(Scale, Scale);
		Canvas->DrawItem(Item);
		Y += LineH;
	};

	FString Line1 = TEXT("REBELSTAR DELTA");
	FString Line2;
	FString Line3;
	FString Line4;

	if (GM)
	{
		const int32 Alive = GM->GetIsaacAliveCount();
		const int32 Total = FMath::Max(1, GM->GetIsaacTotalCount());
		const int32 Sec = FMath::CeilToInt(GM->GetTimeLeft());
		const bool bAlarm = GM->IsAlarmRaised();

		Line1 = FString::Printf(TEXT("%02d:%02d     ISAAC  %d / %d"),
			Sec / 60, Sec % 60, Alive, Total);

		if (GM->IsEndingCinematic() || (GM->IsMatchOver() && GM->DidAttackersWin()))
		{
			Line2 = TEXT("ISAAC OFFLINE");
			Line3 = TEXT("MISSION COMPLETE");
		}
		else if (GM->IsMatchOver())
		{
			Line2 = TEXT("MISSION FAILED");
			Line3 = GM->GetStatusMessage();
		}
		else
		{
			Line2 = bAlarm ? TEXT("ALARM") : TEXT("STEALTH");
			FString Status = GM->GetStatusMessage();
			if (Status.Len() > 42) Status = Status.Left(40) + TEXT("...");
			Line3 = Status;
		}
	}

	if (GM && GM->IsAIVsAIMode() && !GM->IsMatchOver())
	{
		FString CamName = TEXT("TRUMP");
		// Status often carries "CAM: following X" after succession; keep roster line clear
		Line4 = FString::Printf(TEXT("US %d k%d  vs  UK %d k%d   R%d  W %d-%d   CAM follows succession"),
			GM->GetAliveAllies(), GM->GetAttackerKills(),
			GM->GetAliveDefenders(), GM->GetDefenderKills(),
			GM->GetRoundIndex(),
			GM->GetAttackerWins(), GM->GetDefenderWins());
	}
	else if (PC && GM && !GM->IsMatchOver() && !GM->IsEndingCinematic())
	{
		const int32 HPi = FMath::CeilToInt(PC->GetHP());
		const int32 MaxHPi = FMath::CeilToInt(PC->GetMaxHP());
		FString OrderName = TEXT("FOLLOW");
		switch (PC->GetCurrentOrder())
		{
		case ERDSquadOrder::Attack: OrderName = TEXT("ATTACK"); break;
		case ERDSquadOrder::Hold: OrderName = TEXT("HOLD"); break;
		case ERDSquadOrder::Storm: OrderName = TEXT("STORM"); break;
		default: break;
		}
		Line4 = FString::Printf(TEXT("HP %d/%d     %s     SQUAD %s"),
			HPi, MaxHPi, *PC->GetControlLabel(), *OrderName);
	}

	DrawLine(Line1, FLinearColor(0.95f, 0.98f, 1.f));
	if (!Line2.IsEmpty())
	{
		const FLinearColor C2 = Line2.Equals(TEXT("ALARM"))
			? FLinearColor(1.f, 0.25f, 0.15f)
			: (Line2.Contains(TEXT("OFFLINE")) || Line2.Contains(TEXT("COMPLETE"))
				? FLinearColor(0.3f, 1.f, 0.55f)
				: FLinearColor(0.75f, 0.9f, 1.f));
		DrawLine(Line2, C2);
	}
	if (!Line3.IsEmpty())
	{
		DrawLine(Line3, FLinearColor(1.f, 0.88f, 0.45f));
	}
	if (!Line4.IsEmpty())
	{
		const float HpFrac = PC ? (PC->GetMaxHP() > 0.f ? PC->GetHP() / PC->GetMaxHP() : 0.f) : 1.f;
		const FLinearColor HpCol = HpFrac > 0.4f
			? FLinearColor(0.35f, 1.f, 0.45f)
			: FLinearColor(1.f, 0.3f, 0.2f);
		DrawLine(Line4, HpCol);
	}
}
