// Rebelstar Delta — simple canvas HUD (Phase 0)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RDHUD.generated.h"

UCLASS()
class REBELSTARDELTA_API ARDHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
