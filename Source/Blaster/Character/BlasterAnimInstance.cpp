// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterAnimInstance.h"
#include "BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

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

    bIsCrouched = BlasterCharacter->bIsCrouched;

    bAiming = BlasterCharacter->IsAiming();

    //this is a global rotation and is not based on the character
    FRotator AimRotation = BlasterCharacter->GetBaseAimRotation();
    //this is also global rotation
    FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(BlasterCharacter->GetVelocity());

    //this will get the delta between two rotators, and we are going to use the resulting rotator to interp to a location
    FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);

    //this function does interp for rotators using the shortest path possible (will go from -180 to 180 directly rather than traveling through 0 to get there)
    DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaTime, 15.f);
    YawOffset = DeltaRotation.Yaw;

    // //if this character is a client and is not locally controlled. i.e this is a simulated proxy
    // if(!BlasterCharacter->HasAuthority() && !BlasterCharacter->IsLocallyControlled()){
    //     //this is a log message that was used to understand what was happening with the directions
    //     UE_LOG(LogTemp, Warning, TEXT("AimRotaton Yaw: %f"), AimRotation.Yaw);
    //     UE_LOG(LogTemp, Warning, TEXT("Movement Yaw: %f"), MovementRotation.Yaw);
    // }

    //We need these to determine the delta of the character Lean between frames
    CharacterRotationLastFrame = CharacterRotation;
    CharacterRotation = BlasterCharacter->GetActorRotation();

    //we get the delta, scale it to DeltaTime, Interpolate to the Target to stop the character whipping around
    const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame);
    const float Target = Delta.Yaw / DeltaTime;
    const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.f);
    Lean = FMath::Clamp(Interp, -90.f, 90.f);
}
