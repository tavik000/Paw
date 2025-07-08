// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawBubbleHiderCapture.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Paw/Character/Player/PawCharacter.h"
#include "Paw/Character/Player/PawPlayerHider.h"


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
	if (HasAuthority())
	{
		ServerReleaseHider();
		Destroy();
	}
}

void APawBubbleHiderCapture::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawBubbleHiderCapture::MulticastSetHiderFloatingEnable_Implementation(APawPlayerHider* Hider, bool bEnable)
{
	if (!IsValid(Hider))
	{
		UE_LOG(LogTemp, Warning, TEXT("APawBubbleHiderCapture::MulticastSetHiderFloatingEnable - Invalid Hider"));
		return;
	}
	Hider->SetActorEnableCollision(!bEnable);
	Hider->GetCharacterMovement()->GravityScale = !bEnable ? Hider->GetDefaultGravityScale() : 0.0f;
}

void APawBubbleHiderCapture::ServerCaptureHider_Implementation(APawPlayerHider* Hider)
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
	CapturedHider->GetRootComponent()->SetAbsolute(false, true, false);
	CapturedHider->GetRootComponent()->AttachToComponent(BubbleMesh->GetAttachmentRoot(),
	                                                     FAttachmentTransformRules(
		                                                     EAttachmentRule::SnapToTarget,
		                                                     EAttachmentRule::KeepRelative,
		                                                     EAttachmentRule::KeepRelative, true));
	CapturedHider->SetActorRelativeLocation(FVector::ZeroVector);

	CapturedHider->ServerSetCaptured_Implementation(true);
	CapturedHider->OnDestroyed.AddDynamic(this, &APawBubbleHiderCapture::OnCapturedHiderDestroy);
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
		CapturedHider->ServerSetCaptured_Implementation(false);
	}
	CapturedHider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	MulticastSetHiderFloatingEnable(CapturedHider.Get(), false);
	CapturedHider.Reset();
}

void APawBubbleHiderCapture::OnCapturedHiderDestroy(AActor* DestroyedActor)
{
	if (HasAuthority())
	{
		Destroy();
	}
}
