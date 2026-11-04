// Copyright Epic Games, Inc. All Rights Reserved.

#include "MAL.h"
#include "Modules/ModuleManager.h"
#include "Tags/MALGameplayTags.h"

class FMALModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// Force early initialization of gameplay tags before DoneAddingNativeTags
		UMALGameplayTags::Get();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(
	FMALModule,
	MAL,
	"MAL"
);
