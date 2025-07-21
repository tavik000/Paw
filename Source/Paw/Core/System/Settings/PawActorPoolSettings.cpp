// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#include "PawActorPoolSettings.h"
#include "Paw/Weapon/Projectile/PawProjectile_Bubble.h"

UPawActorPoolSettings::UPawActorPoolSettings()
{
	// Set default configuration with bubble projectile
	DefaultPoolSize = 50;
	
	// Add default projectile to the pool configuration
	FActorPoolConfig DefaultProjectileConfig;
	DefaultProjectileConfig.ActorClass = TSoftClassPtr<AActor>(FSoftObjectPath("/Game/Main/Characters/Players/Seekers/Weapons/BP_Projectile_Bubble.BP_Projectile_Bubble_C"));
	DefaultProjectileConfig.PoolSize = 50;
	
	ActorPools.Add(DefaultProjectileConfig);
}