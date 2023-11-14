// Fill out your copyright notice in the Description page of Project Settings.


#include "HitScanWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "Kismet/KismetMathLibrary.h"
#include "WeaponTypes.h"

void AHitScanWeapon::Fire(const FVector &HitTarget)
{
    Super::Fire(HitTarget);

    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if(OwnerPawn == nullptr) return;
    AController* InstigatorController = OwnerPawn->GetController(); //controller for the character firing the weapon

    const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
    if(MuzzleFlashSocket){
        FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
        FVector Start = SocketTransform.GetLocation();

        FHitResult FireHit;
        UWorld* World = GetWorld();
        WeaponTraceHit(Start, HitTarget, FireHit);
        ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor());
        if (BlasterCharacter && HasAuthority() && InstigatorController)
        {
            UGameplayStatics::ApplyDamage(BlasterCharacter,	Damage,	InstigatorController, this, UDamageType::StaticClass());
        }
        if (ImpactParticles)
        {
            UGameplayStatics::SpawnEmitterAtLocation(World, ImpactParticles, FireHit.ImpactPoint, FireHit.ImpactNormal.Rotation());
        }
        if(HitSound){
            UGameplayStatics::PlaySoundAtLocation(this, HitSound, FireHit.ImpactPoint);
        }
        if(MuzzleFlash){
            UGameplayStatics::SpawnEmitterAtLocation(World, MuzzleFlash, SocketTransform);
        }
        if(FireSound){
            UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
        }
    }
}

FVector AHitScanWeapon::TraceEndWithScatter(const FVector &TraceStart, const FVector &HitTarget)
{
    //this points to the hit target, but we want to set the length of it directly so it needs to be normalized
    FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
    FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;

    //selecting random locations within the sphere to use for line traces
    FVector RandVec = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius);
    FVector EndLoc = SphereCenter + RandVec;

    //line to the end
	FVector ToEndLoc = EndLoc - TraceStart;

    // //showing what the shots and line traces look like
	// DrawDebugSphere(GetWorld(), SphereCenter, SphereRadius, 12, FColor::Red, true);
	// DrawDebugSphere(GetWorld(), EndLoc, 4.f, 12, FColor::Orange, true);
	// DrawDebugLine(GetWorld(), TraceStart, FVector(TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size()), FColor::Cyan, true);

	return FVector(TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size());
}

void AHitScanWeapon::WeaponTraceHit(const FVector &TraceStart, const FVector &HitTarget, FHitResult &OutHit)
{
    UWorld* World = GetWorld();
	if (World)
	{
		FVector End = bUseScatter ? TraceEndWithScatter(TraceStart, HitTarget) : TraceStart + (HitTarget - TraceStart) * 1.25f;

		World->LineTraceSingleByChannel(
			OutHit,
			TraceStart,
			End,
			ECollisionChannel::ECC_Visibility
		);
		FVector BeamEnd = End;
		if (OutHit.bBlockingHit)
		{
			BeamEnd = OutHit.ImpactPoint;
		}
		if (BeamParticles)
		{
			UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
				World,
				BeamParticles,
				TraceStart,
				FRotator::ZeroRotator,
				true
			);
			if (Beam)
			{
				Beam->SetVectorParameter(FName("Target"), BeamEnd);
			}
		}
	}
}
