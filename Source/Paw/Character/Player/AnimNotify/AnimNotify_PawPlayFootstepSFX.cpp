// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "AnimNotify_PawPlayFootstepSFX.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Paw/Character/Player/PawPlayerHider.h"

void UAnimNotify_PawPlayFootstepSFX::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                            const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	const auto* Hider = Cast<APawPlayerHider>(MeshComp->GetOwner());
	if (!IsValid(Hider))
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayFootStepSFX Hider is not valid!"));
		return;
	}

	if (FMath::Floor(Hider->GetMovementComponent()->Velocity.Length()) <= Hider->GetSlowWalkSpeed())
	{
		return;
	}

	if (!IsValid(FootstepSound))
	{
		UE_LOG(LogTemp, Error, TEXT("FootstepSound is not valid!"));
		return;
	}

	if (!IsValid(MeshComp->GetWorld()))
	{
		return;
	}
	

	UGameplayStatics::PlaySoundAtLocation(MeshComp->GetWorld(), FootstepSound, MeshComp->GetComponentLocation(),
	                                      1.f, 1.f);
}
