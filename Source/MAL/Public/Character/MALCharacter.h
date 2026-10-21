// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "MALCharacter.generated.h"

class UGameplayEffect;
class UMALClassDefinition;

UCLASS()
class MAL_API AMALCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMALCharacter();

	/** Class definition that determines which abilities this character has */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Class")
	TObjectPtr<UMALClassDefinition> ClassDefinition;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "MAL|Attribute")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttribute;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,  Category = "MAL|Attribute")
	TSubclassOf<UGameplayEffect> DefaultSecondaryAttribute;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,  Category = "MAL|Attribute")
	TSubclassOf<UGameplayEffect> DefaultStandaloneAttribute;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void PossessedBy(AController* NewController) override;
};
