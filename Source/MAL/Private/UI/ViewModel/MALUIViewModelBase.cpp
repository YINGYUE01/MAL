// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ViewModel/MALUIViewModelBase.h"

#include "AbilitySystemComponent.h"

void UMALUIViewModelBase::SetAbilitySystemComponent(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (GetAbilitySystemComponent() == InAbilitySystemComponent)
	{
		return;
	}

	UnbindAllAttributes();
	OnAbilitySystemReleased();

	CachedAbilitySystemComponent = InAbilitySystemComponent;

	if (InAbilitySystemComponent)
	{
		OnAbilitySystemReady(InAbilitySystemComponent);
	}
}

UAbilitySystemComponent* UMALUIViewModelBase::GetAbilitySystemComponent() const
{
	return CachedAbilitySystemComponent.Get();
}

void UMALUIViewModelBase::BindAttribute(const FGameplayAttribute& Attribute, TFunction<void(const FOnAttributeChangeData&)>&& OnChanged)
{
	UAbilitySystemComponent* ASC = CachedAbilitySystemComponent.Get();
	if (!ASC || !Attribute.IsValid() || !OnChanged)
	{
		return;
	}

	// Drop an existing listener for the same attribute first: rebinding (respawn, re-possession)
	// must not leave two callbacks pushing into the same FieldNotify property.
	for (int32 Index = AttributeBindings.Num() - 1; Index >= 0; --Index)
	{
		if (AttributeBindings[Index].Attribute == Attribute)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(AttributeBindings[Index].Handle);
			AttributeBindings.RemoveAtSwap(Index, EAllowShrinking::No);
		}
	}

	FMALAttributeBinding& Binding = AttributeBindings.AddDefaulted_GetRef();
	Binding.Attribute = Attribute;
	Binding.Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UMALUIViewModelBase::HandleAttributeChanged);
	Binding.Callback = MoveTemp(OnChanged);

	// Prime through the same dispatch path as a real change so a handler registering more
	// attributes is safe (the dispatcher copies the callback before invoking it).
	FOnAttributeChangeData InitialValue;
	InitialValue.Attribute = Attribute;
	InitialValue.OldValue = ASC->GetNumericAttribute(Attribute);
	InitialValue.NewValue = InitialValue.OldValue;

	HandleAttributeChanged(InitialValue);
}

void UMALUIViewModelBase::UnbindAllAttributes()
{
	if (UAbilitySystemComponent* ASC = CachedAbilitySystemComponent.Get())
	{
		for (const FMALAttributeBinding& Binding : AttributeBindings)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Binding.Attribute).Remove(Binding.Handle);
		}
	}

	AttributeBindings.Reset();
}

void UMALUIViewModelBase::HandleAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	// Copy the callback out before invoking it: a handler that rebinds attributes would
	// reallocate AttributeBindings and invalidate the loop.
	TFunction<void(const FOnAttributeChangeData&)> Callback;
	for (const FMALAttributeBinding& Binding : AttributeBindings)
	{
		if (Binding.Attribute == ChangeData.Attribute)
		{
			Callback = Binding.Callback;
			break;
		}
	}

	if (Callback)
	{
		Callback(ChangeData);
	}
}
