// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawBubbleHiderCapture.h"

#include "GameFramework/Character.h"
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
		ReleaseHider();
		Destroy();
	}
}

void APawBubbleHiderCapture::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawBubbleHiderCapture::CaptureHider(APawCharacter* Hider)
{
	if (!IsValid(Hider))
	{
		return;
	}
	CapturedHider = Hider;
	CapturedHider->SetActorEnableCollision(false);

	// Attach it to BubbleMesh
	CapturedHider->AttachToComponent(BubbleMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
	CapturedHider->SetActorRelativeLocation(FVector::ZeroVector);
	GEngine->AddOnScreenDebugMessage(-1, 45.0f, FColor::Green, TEXT("Hider Captured"));
	CapturedHider->GetMesh()->SetEnableGravity(false);
	if (HasAuthority())
	{
		CapturedHider->ServerSetCaptured(true);
	}
}

void APawBubbleHiderCapture::ReleaseHider()
{
	if (!CapturedHider.IsValid())
	{
		return;
	}

	GEngine->AddOnScreenDebugMessage(-1, 45.0f, FColor::Green, TEXT("Hider Released"));

	// Detach it from BubbleMesh
	if (HasAuthority())
	{
		CapturedHider->ServerSetCaptured(false);
	}
	CapturedHider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	CapturedHider->SetActorEnableCollision(false);
	CapturedHider->GetMesh()->SetEnableGravity(true);
	CapturedHider.Reset();
}
