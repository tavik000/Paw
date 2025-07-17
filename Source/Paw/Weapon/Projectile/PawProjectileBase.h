// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"

#include "PawProjectileBase.generated.h"

class UPawProjectileMovementComponent;
class USphereComponent;

UCLASS()
class PAW_API APawProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	APawProjectileBase();
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                   FVector NormalImpulse,
	                   const FHitResult& Hit);

	USphereComponent* GetCollisionComp() const { return CollisionComp; }
	UPawProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }

protected:
	virtual void BeginPlay() override;

protected:
	virtual void PostNetReceiveLocationAndRotation() override;

protected:
	UPROPERTY(VisibleDefaultsOnly, Category = Projectile)
	USphereComponent* CollisionComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	UPawProjectileMovementComponent* ProjectileMovement;
};
