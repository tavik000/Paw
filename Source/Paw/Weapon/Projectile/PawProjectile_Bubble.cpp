// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawProjectile_Bubble.h"

#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Paw/Character/Player/PawCharacter.h"
#include "Paw/Environment/GameplayElement/Bubble/PawBubbleHiderCapture.h"


// Sets default values
APawProjectile_Bubble::APawProjectile_Bubble()
{
	ProjectileMovement->bShouldBounce = false;
	BubbleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BubbleMesh"));
	BubbleMesh->SetupAttachment(RootComponent);
	BubbleMesh->SetCollisionProfileName(TEXT("NoCollision"));
	BubbleMesh->SetIsReplicated(true);
	BubbleMesh->SetVisibility(true);
}

void APawProjectile_Bubble::BeginPlay()
{
	Super::BeginPlay();
}

void APawProjectile_Bubble::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawProjectile_Bubble::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                  FVector NormalImpulse, const FHitResult& Hit)
{
	// if (!HasAuthority())
	// {
	// 	return;
	// }


	if (!BubbleHiderCaptureClass)
	{
		return;
	}

	APawCharacter* HitHider = Cast<APawCharacter>(OtherActor);
	if (!IsValid(HitHider))
	{
		Destroy();
		return;
	}
	
	FVector HitHiderLocation = HitHider->GetActorLocation();
	FVector BubbleLocation = FVector(HitHiderLocation.X, HitHiderLocation.Y, HitHiderLocation.Z + BubbleHeightOffset);

	// Spawn Actor from class BubbleHideCaptureClass
	APawBubbleHiderCapture* BubbleHiderCapture = GetWorld()->SpawnActor<APawBubbleHiderCapture>(
		BubbleHiderCaptureClass, BubbleLocation, FRotator::ZeroRotator);
	if (IsValid(BubbleHiderCapture))
	{
		BubbleHiderCapture->SetOwner(HitHider);
		BubbleHiderCapture->SetReplicates(true);
		BubbleHiderCapture->SetReplicateMovement(true);
		BubbleHiderCapture->CaptureHider(HitHider);
	}

	Destroy();
}
