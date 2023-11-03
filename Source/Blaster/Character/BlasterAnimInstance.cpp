// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterAnimInstance.h"
#include "BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void UBlasterAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
}

void UBlasterAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
    Super::NativeUpdateAnimation(DeltaTime);

    if(BlasterCharacter == nullptr){
        BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
    }

    if(!BlasterCharacter){
        return;
    }

    //all this to get the speed
    FVector Velocity = BlasterCharacter->GetVelocity();
    Velocity.Z = 0.f;
    Speed = Velocity.Size();

    //this will get if we are airborne
    bIsInAir = BlasterCharacter->GetCharacterMovement()->IsFalling();

    //if we are adding inputs to the controller (holding W key for example) then this will be true
    bIsAccelerating = BlasterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f ? true : false;

    bWeaponEquipped = BlasterCharacter->IsWeaponEquipped();
}
