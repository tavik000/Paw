// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawBubbleHiderCapture.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Paw/Character/Player/PawCharacter.h"


APawBubbleHiderCapture::APawBubbleHiderCapture()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APawBubbleHiderCapture::BeginPlay()
{
	Super::BeginPlay();
}

void APawBubbleHiderCapture::Break_Implementation()
{
	Super::Break_Implementation();
	GEngine->AddOnScreenDebugMessage(-1, 45.0f, FColor::Green, TEXT("Bubble Hider Capture Break"));
	if (HasAuthority())
	{
		GEngine->AddOnScreenDebugMessage(-1, 45.0f, FColor::Green, TEXT("Bubble Hider Capture Break Authority"));
		ServerReleaseHider();
		Destroy();
	}
}

void APawBubbleHiderCapture::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawBubbleHiderCapture::MulticastSetHiderFloatingEnable_Implementation(APawCharacter* Hider, bool bEnable)
{
	UE_LOG(LogTemp, Warning, TEXT("MulticastSetHiderFloatingEnable, bEnable: %d"), bEnable);
	Hider->SetActorEnableCollision(!bEnable);
	Hider->GetCharacterMovement()->GravityScale = !bEnable ? 1.75f : 0.0f;
}

void APawBubbleHiderCapture::ServerCaptureHider_Implementation(APawCharacter* Hider)
{
	if (!HasAuthority())
	{
		return;
	}
	if (!IsValid(Hider))
	{
		return;
	}
	CapturedHider = Hider;
	MulticastSetHiderFloatingEnable(Hider, true);

	// Attach it to BubbleMesh
	CapturedHider->AttachToComponent(BubbleMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
	CapturedHider->SetActorRelativeLocation(FVector::ZeroVector);

	CapturedHider->ServerSetCaptured(true);
	GEngine->AddOnScreenDebugMessage(-1, 45.0f, FColor::Green, TEXT("Hider Captured"));
}

void APawBubbleHiderCapture::ServerReleaseHider_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}
	if (!CapturedHider.IsValid())
	{
		return;
	}


	// Detach it from BubbleMesh
	if (HasAuthority())
	{
		CapturedHider->ServerSetCaptured(false);
		GEngine->AddOnScreenDebugMessage(-1, 45.0f, FColor::Green, TEXT("Hider Released"));
	}
	CapturedHider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	MulticastSetHiderFloatingEnable(CapturedHider.Get(), false);
	CapturedHider.Reset();
}
