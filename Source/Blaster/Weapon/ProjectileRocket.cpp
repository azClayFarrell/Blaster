// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileRocket.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraComponent.h"
#include "Sound/SoundCue.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"

AProjectileRocket::AProjectileRocket(){
    RocketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rocket Mesh"));
    RocketMesh->SetupAttachment(RootComponent);
    RocketMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AProjectileRocket::Destroyed()
{
    //we don't want to call Super::Destroy because we are handling the spawning of the sounds and particles in here
    //differently than we would for a normal projectile
}

void AProjectileRocket::OnHit(UPrimitiveComponent *HitComp, AActor *OtherActor, UPrimitiveComponent *OtherComp, FVector NormalImpulse, const FHitResult &Hit)
{
    APawn* FiringPawn = GetInstigator();
    //we have to add the HasAuthority here so that we are allowed to generate hit events on client and server, but the
    //damage only gets applied on the server
    if(FiringPawn && HasAuthority()){
        AController* FiringController = FiringPawn->GetController();
        if(FiringController){
            UGameplayStatics::ApplyRadialDamageWithFalloff(
                this, //world context object
                Damage, //BaseDamage
                10.f, //MinimumDamage
                GetActorLocation(), //Origin
                200.f, //DamageInnerRadius
                500.f, //DamageOuterRadius
                1.f, //DamageFalloff
                UDamageType::StaticClass(), //DamageTypeClass
                TArray<AActor*>(), //IgnoreActors
                this, //DamageCauser
                FiringController //InstigatorController
            );
        }
    }
    //we don't want to destroy OnHit, since we want the smoke trail to linger after the rocket explodes

    GetWorldTimerManager().SetTimer(DestroyTimer, this, &AProjectileRocket::DestroyTimerFinished, DestroyTime);

	if (ImpactParticles)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, GetActorTransform());
	}
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}
	if (RocketMesh)
	{
		RocketMesh->SetVisibility(false);
	}
	if (CollisionBox)
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
    //this is different from the course github commit
	if (TrailSystemComponent && TrailSystemComponent->GetSystemInstanceController())
	{
        //this is different from the course github commit, the API in the lecture is outdated at this point
		TrailSystemComponent->GetSystemInstanceController()->Deactivate();
	}
	if (ProjectileLoopComponent && ProjectileLoopComponent->IsPlaying())
	{
		ProjectileLoopComponent->Stop();
	}
}

void AProjectileRocket::BeginPlay()
{
    Super::BeginPlay();

	if (!HasAuthority())
	{
        //since this is already bound on the server, which is happening in the Projectile Super::BeginPlay(), we can bind
        //all the client projectile OnHit functions so we will have hit events for rockets on all machines
		CollisionBox->OnComponentHit.AddDynamic(this, &ThisClass::OnHit);
	}

	if (TrailSystem)
	{
		TrailSystemComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			TrailSystem,
			GetRootComponent(),
			FName(),
			GetActorLocation(),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition,
			false
		);
	}
	if (ProjectileLoop && LoopingSoundAttenuation)
	{
		ProjectileLoopComponent = UGameplayStatics::SpawnSoundAttached(
			ProjectileLoop,
			GetRootComponent(),
			FName(),
			GetActorLocation(),
			EAttachLocation::KeepWorldPosition,
			false, // we want to stop the sound manually, so this needs to be false
			1.f,
			1.f,
			0.f,
			LoopingSoundAttenuation,
			(USoundConcurrency*)nullptr,
			false //this is an AutoDestroy boolean, and since we want this to be false, we have to specify the USoundConcurrency* as a nullptr
		);
	}
}

void AProjectileRocket::DestroyTimerFinished()
{
    Destroy();
}
