// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PawProjectileTestHelper.generated.h"

class APawProjectileBase;
class UStaticMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class PAW_API APawProjectileTestHelper : public AActor
{
	GENERATED_BODY()

public:
	APawProjectileTestHelper();

	UFUNCTION(BlueprintCallable, Category = "Test")
	void SetupBasicWallTest();

	UFUNCTION(BlueprintCallable, Category = "Test")
	void FireTestProjectiles(int32 NumProjectiles = 10, float FireInterval = 0.5f);

	UFUNCTION(BlueprintCallable, Category = "Test")
	void CreatePerfectWall(const FVector& Location = FVector(500, 0, 0), const FVector& Scale = FVector(1, 10, 5));

	UFUNCTION(BlueprintCallable, Category = "Test")
	void EnableAllProjectileLogging(bool bEnable = true);

	UFUNCTION(BlueprintCallable, Category = "Test")
	void ClearAllProjectiles();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void FireSingleProjectile();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	TSubclassOf<APawProjectileBase> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	FVector FireLocation = FVector(0, 0, 100);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	FVector FireDirection = FVector(1, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	float ProjectileSpeed = 1500.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test")
	UStaticMeshComponent* TestWall;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test")
	TArray<TObjectPtr<APawProjectileBase>> ActiveProjectiles;

private:
	FTimerHandle FireTimerHandle;
	int32 ProjectilesToFire = 0;
};