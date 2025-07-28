// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawBubbleHiderCapture.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "Paw/Character/Player/PawPlayerHider.h"


APawBubbleHiderCapture::APawBubbleHiderCapture()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APawBubbleHiderCapture::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APawBubbleHiderCapture, CapturedHider);
}

void APawBubbleHiderCapture::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(CaptureSound))
	{
		UGameplayStatics::SpawnSoundAtLocation(GetWorld(), CaptureSound, GetActorLocation());
	}
}

void APawBubbleHiderCapture::Break_Implementation()
{
	if (!CapturedHider.IsValid())
	{
		return;
	}
	if (!CapturedHider->IsAlive())
	{
		return;
	}
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

void APawBubbleHiderCapture::MulticastSetHiderFloatingEnable_Implementation(
	APawPlayerHider* Hider, bool bEnable)
{
	if (!IsValid(Hider))
	{
		UE_LOG(LogTemp, Warning, TEXT("APawBubbleHiderCapture::MulticastSetHiderFloatingEnable - Invalid Hider"));
		return;
	}

	UCharacterMovementComponent* MovementComp = Hider->GetCharacterMovement();
	if (!IsValid(MovementComp))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("APawBubbleHiderCapture::MulticastSetHiderFloatingEnable - Invalid Movement Component"));
		return;
	}

	if (bEnable)
	{
		// Capturing: Disable all movement physics
		UE_LOG(LogTemp, Log,
		       TEXT("APawBubbleHiderCapture::MulticastSetHiderFloatingEnable - Disabling movement for %s"),
		       *Hider->GetName());

		// Store original movement mode
		OriginalMovementMode = MovementComp->MovementMode;

		// Disable collision
		Hider->SetActorEnableCollision(false);

		// Clear any existing velocity
		MovementComp->Velocity = FVector::ZeroVector;

		// Disable all movement physics
		MovementComp->SetMovementMode(MOVE_None);
		MovementComp->GravityScale = 0.0f;
	}
	else
	{
		// Releasing: Restore normal movement physics
		UE_LOG(LogTemp, Log,
		       TEXT("APawBubbleHiderCapture::MulticastSetHiderFloatingEnable - Restoring movement for %s"),
		       *Hider->GetName());

		// Restore collision
		Hider->SetActorEnableCollision(true);
		// Restore original movement mode
		MovementComp->SetMovementMode(OriginalMovementMode);


		// Restore gravity
		MovementComp->GravityScale = Hider->GetDefaultGravityScale();


		// Clear velocity to prevent sudden movements
		MovementComp->Velocity = FVector::ZeroVector;
	}
}

void APawBubbleHiderCapture::MulticastAttachHiderToBubble_Implementation(APawPlayerHider* Hider)
{
	if (!IsValid(Hider) || !IsValid(BubbleMesh))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("APawBubbleHiderCapture::MulticastAttachHiderToBubble - Invalid Hider or BubbleMesh"));
		return;
	}

	// Attach hider to BubbleMesh with more stable rules
	Hider->GetRootComponent()->SetAbsolute(false, true, false);
	bool bAttachmentSuccessful = Hider->GetRootComponent()->AttachToComponent(
		BubbleMesh,
		FAttachmentTransformRules(
			EAttachmentRule::KeepWorld,
			EAttachmentRule::KeepWorld,
			EAttachmentRule::KeepWorld,
			true
		)
	);

	if (!bAttachmentSuccessful)
	{
		UE_LOG(LogTemp, Error, TEXT("APawBubbleHiderCapture::MulticastAttachHiderToBubble - Attachment failed for %s"),
		       *Hider->GetName());
		return;
	}

	// Verify attachment succeeded
	if (Hider->GetRootComponent()->GetAttachParent() != BubbleMesh)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("APawBubbleHiderCapture::MulticastAttachHiderToBubble - Attachment verification failed for %s"),
		       *Hider->GetName());
		return;
	}

	// Now set the relative position to center the hider in the bubble
	Hider->SetActorRelativeLocation(FVector::ZeroVector);

	UE_LOG(LogTemp, Log,
	       TEXT("APawBubbleHiderCapture::MulticastAttachHiderToBubble - Successfully attached %s to bubble"),
	       *Hider->GetName());
}

void APawBubbleHiderCapture::MulticastDetachHiderFromBubble_Implementation(APawPlayerHider* Hider)
{
	if (!IsValid(Hider))
	{
		UE_LOG(LogTemp, Warning, TEXT("APawBubbleHiderCapture::MulticastDetachHiderFromBubble - Invalid Hider"));
		return;
	}

	// Detach hider from BubbleMesh
	Hider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
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

	UE_LOG(LogTemp, Log, TEXT("APawBubbleHiderCapture::ServerCaptureHider - Starting capture for %s"),
	       *Hider->GetName());

	CapturedHider = Hider;

	// First, disable physics and collision on all clients
	MulticastSetHiderFloatingEnable(Hider, true);

	// Single frame delay to ensure physics changes are processed before attachment
	GetWorld()->GetTimerManager().SetTimerForNextTick([this, Hider]()
	{
		if (IsValid(Hider) && IsValid(this))
		{
			// Then attach to bubble
			MulticastAttachHiderToBubble(Hider);

			// Set captured state
			Hider->ServerSetCaptured(true);
			Hider->OnDeathStarted.AddDynamic(this, &APawBubbleHiderCapture::OnCapturedHiderDeathStarted);

			UE_LOG(LogTemp, Log, TEXT("APawBubbleHiderCapture::ServerCaptureHider - Capture complete for %s"),
			       *Hider->GetName());
		}
	});
}

void APawBubbleHiderCapture::ServerReleaseHider_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	CapturedHider->OnDeathStarted.RemoveDynamic(this, &APawBubbleHiderCapture::OnCapturedHiderDeathStarted);
	APawPlayerHider* HiderToRelease = CapturedHider.Get();

	MulticastDetachHiderFromBubble(HiderToRelease);
	MulticastSetHiderFloatingEnable(HiderToRelease, false);
	HiderToRelease->ServerSetCaptured(false);

	CapturedHider.Reset();
}

void APawBubbleHiderCapture::MulticastSpawnCaptureBurstEffect_Implementation()
{
	if (IsValid(CapturedBurstSound))
	{
		UGameplayStatics::SpawnSoundAtLocation(GetWorld(), CapturedBurstSound, GetActorLocation());
	}

	if (!IsValid(BreakEffect))
	{
		BreakEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BreakEffectAsset.Get(),
		                                                             GetActorLocation(),
		                                                             GetActorRotation(),
		                                                             FVector::One() * BreakEffectScale, true, true,
		                                                             ENCPoolMethod::AutoRelease,
		                                                             true);
	}

	Deactivate();

	if (HasAuthority())
	{
		Destroy();
	}
}

void APawBubbleHiderCapture::OnCapturedHiderDeathStarted()
{
	if (HasAuthority())
	{
		APawPlayerHider* HiderToRelease = CapturedHider.Get();
		MulticastDetachHiderFromBubble(HiderToRelease);
		MulticastSetHiderFloatingEnable(HiderToRelease, false);
		MulticastSpawnCaptureBurstEffect();
	}
}
