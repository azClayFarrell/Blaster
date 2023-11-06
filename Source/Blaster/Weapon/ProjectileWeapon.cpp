// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Projectile.h"

void AProjectileWeapon::Fire(const FVector &HitTarget)
{
    Super::Fire(HitTarget);

    //this is to make sure we are only spawning the projectile on the server
    if(!HasAuthority()){
        return;
    }

    //this might be the ProjectileWeapon?
    APawn* InstigatorPawn = Cast<APawn>(GetOwner());
    //GetWeaponMesh is a parent function
    const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
    if(MuzzleFlashSocket){
        //GetSocketTransform needs the mesh that the socket is on as a param
        FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
        //From the muzzle flash socket to hit location from TraceUnderCrosshairs
        FVector ToTarget = HitTarget - SocketTransform.GetLocation();
        FRotator TargetRotation = ToTarget.Rotation();
        if(ProjectileClass && InstigatorPawn){
            UWorld* World = GetWorld();
            if(World){
                //params for making a projectile
                FActorSpawnParameters SpawnParams;
                //Sets the projectiles owner to the weapons owner
                SpawnParams.Owner = GetOwner();
                SpawnParams.Instigator = InstigatorPawn;
                World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
            }
        }
    }
}
