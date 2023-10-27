// Copyright Epic Games, Inc. All Rights Reserved.

#include "UE_TPSProjectGameMode.h"
#include "UE_TPSProjectCharacter.h"
#include "UObject/ConstructorHelpers.h"

AUE_TPSProjectGameMode::AUE_TPSProjectGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
