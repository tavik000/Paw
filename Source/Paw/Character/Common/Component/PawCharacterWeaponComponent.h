// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"

#include "PawCharacterWeaponComponent.generated.h"


class APawWeaponBase;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PAW_API UPawCharacterWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPawCharacterWeaponComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable)
	APawWeaponBase* GetEquipWeapon() const;

	template <class T>
	T* GetEquipWeapon() const { return Cast<T>(GetEquipWeapon()); }

	void CreateDefaultWeapon();

	void CreateEquipWeapon(UClass* WeaponClass);

	void DeleteEquipWeapon();

	void ChangeEquipWeapon(UClass* ChangeWeaponClass);

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<APawWeaponBase> DefaultWeaponClass;

private:
	TWeakObjectPtr<APawWeaponBase> EquippedWeapon;
};
