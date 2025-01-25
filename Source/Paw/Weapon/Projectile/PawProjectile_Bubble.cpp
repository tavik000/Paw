// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawProjectile_Bubble.h"

#include "Components/SphereComponent.h"
#include "Engine/AssetManager.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Paw/Character/Player/PawCharacter.h"
#include "Paw/Environment/GameplayElement/Bubble/PawBubbleHiderCapture.h"


// Sets default values
APawProjectile_Bubble::APawProjectile_Bubble()
{
	BubbleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BubbleMesh"));
	BubbleMesh->SetupAttachment(RootComponent);
	BubbleMesh->SetCollisionProfileName(TEXT("NoCollision"));
	BubbleMesh->SetIsReplicated(true);
	BubbleMesh->SetVisibility(true);

	InitialLifeSpan = 0.0f;
}

void APawProjectile_Bubble::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &APawProjectile_Bubble::SelfBreak, BubbleLifeTime,
		                                       false);
	}
	LoadBreakEffect();
}

void APawProjectile_Bubble::Break_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}
	IPawCollideBreakableInterface::Break_Implementation();
	MulticastSpawnBreakEffect();

	// Wait 5 sec and destroy
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &APawProjectile_Bubble::SelfDestroy, 5.0f, false);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BubbleMesh->SetVisibility(false);
}

void APawProjectile_Bubble::SelfBreak()
{
	if (IPawCollideBreakableInterface* CollideBreakableInterface = Cast<IPawCollideBreakableInterface>(this))
	{
		CollideBreakableInterface->Break_Implementation();
	}
}

void APawProjectile_Bubble::LoadBreakEffect()
{
	UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(BreakEffectAsset.ToSoftObjectPath(),
	                                                             FStreamableDelegate::CreateUObject(
		                                                             this,
		                                                             &APawProjectile_Bubble::OnBreakEffectLoaded));
}

void APawProjectile_Bubble::OnBreakEffectLoaded()
{
}

void APawProjectile_Bubble::MulticastSpawnBreakEffect_Implementation()
{
	if (IsValid(BreakEffectAsset.Get()))
	{
		BreakEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BreakEffectAsset.Get(),
		                                                             GetActorLocation(),
		                                                             GetActorRotation(),
		                                                             FVector::One() * BreakEffectScale, true, true,
		                                                             ENCPoolMethod::AutoRelease,
		                                                             true);
	}
}

void APawProjectile_Bubble::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawProjectile_Bubble::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                  FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}


	if (!BubbleHiderCaptureClass)
	{
		return;
	}

	APawCharacter* HitHider = Cast<APawCharacter>(OtherActor);
	if (!IsValid(HitHider))
	{
		// Only add impulse and destroy projectile if we hit a physics
		if ((OtherActor != nullptr) && (OtherActor != this) && (OtherComp != nullptr) && OtherComp->
			IsSimulatingPhysics())
		{
			OtherComp->AddImpulseAtLocation(GetVelocity() * PushForce, GetActorLocation());
		}
		return;
	}

	FVector HitHiderLocation = HitHider->GetActorLocation();
	FVector BubbleSpawnLocation = FVector(HitHiderLocation.X, HitHiderLocation.Y,
	                                      HitHiderLocation.Z + BubbleHeightOffset);
	HitHider->SetActorLocation(BubbleSpawnLocation);

	// Spawn Actor from class BubbleHideCaptureClass
	APawBubbleHiderCapture* BubbleHiderCapture = GetWorld()->SpawnActor<APawBubbleHiderCapture>(
		BubbleHiderCaptureClass, BubbleSpawnLocation, FRotator::ZeroRotator);
	if (IsValid(BubbleHiderCapture))
	{
		BubbleHiderCapture->SetOwner(this);
		BubbleHiderCapture->SetReplicates(true);
		BubbleHiderCapture->SetReplicateMovement(true);
		BubbleHiderCapture->ServerCaptureHider(HitHider);
	}

	Destroy();
}

void APawProjectile_Bubble::SelfDestroy()
{
	if (!HasAuthority())
	{
		return;
	}
	Destroy();
}
