// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	BaseWalkSpeed = 600.f;
	AimWalkSpeed = 450.f;
}

void UCombatComponent::EquipWeapon(AWeapon *WeaponToEquip)
{
	//if there is no valid character to equip weapons to or the Weapon to Equip is not valid then we will return out
	if(!Character || !WeaponToEquip){
		return;
	}

	EquippedWeapon = WeaponToEquip;
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if(HandSocket){
		HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
	}
	EquippedWeapon->SetOwner(Character);

	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, bAiming);
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if(Character){
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	}
}

void UCombatComponent::SetAiming(bool bIsAiming)
{
	//this is just "drawing" the client as aiming without waiting for the server to replicate the request back to it. Immediate feedback
	bAiming = bIsAiming;

	//SOLUTION 1: for getting the character aim to be replicated across server and client
	// if(!Character->HasAuthority()){
	// 	ServerSetAiming(bIsAiming);
	// }

	//FINAL SOLUTION: see notes as to why with link to reference graph in documentation
	ServerSetAiming(bIsAiming);

	//this is to set the max walk speed when aiming
	if(Character){
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
}

void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming)
{
	bAiming = bIsAiming;

	//this is to set the max walk speed when aiming on the server so that the server doesn't try to adjust our position
	if(Character){
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
}

void UCombatComponent::OnRep_EquippedWeapon(){
	if(EquippedWeapon && Character){
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;
	}
}

void UCombatComponent::FireButtonPressed(bool bPressed)
{
	//We don't want to replicate this variable to the server so we are going to use Multicast RPCs to tell clients to fire the weapons
	bFireButtonPressed = bPressed;

	if(bFireButtonPressed){
		// call the fire function on the server if it's true that the client pressed the fire button;
		// if only this function call is included and the logic for firing was in this function alone, 
		// the firing client and all other clients will not see the weapon fire, but the server will.
		// However, we moved the code from the body of this function into the Multicast function, and
		// we are calling the multicast function from the body of ServerFire;
		ServerFire();
	}
}

void UCombatComponent::ServerFire_Implementation()
{
	//calling this Multicast RPC will make the server pawn fire and propagate the firing of the weapon to all clients
	MulticastFire();
}

void UCombatComponent::MulticastFire_Implementation(){
	//we need to check this since we are wanting to call the fire function on the Weapon class
	if(EquippedWeapon == nullptr){
		return;
	}

	if(Character){
		//bAiming is replicated, so all clients will know if we are aiming or not
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(HitTarget);
	}
}

void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult){
	//this will get the size of the view port so that we can find the middle of the screen
	FVector2D ViewportSize;
	if(GEngine && GEngine->GameViewport){
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	//set the Crosshair location as the center of the viewport in world space
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector OutCrosshairWorldPosition;
	FVector OutCrosshairWorldDirection;
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(UGameplayStatics::GetPlayerController(this, 0), CrosshairLocation, OutCrosshairWorldPosition, OutCrosshairWorldDirection);

	if(bScreenToWorld){
		FVector Start = OutCrosshairWorldPosition;
		FVector End = Start + OutCrosshairWorldDirection * TRACE_LENGTH;

		GetWorld()->LineTraceSingleByChannel(TraceHitResult, Start, End, ECollisionChannel::ECC_Visibility);

		//if we hit nothing in the hit result, then we will just set the impact point as the End point
		if(!TraceHitResult.bBlockingHit){
			TraceHitResult.ImpactPoint = End;
			//making sure our new FVector for HitTarget isn't null
			HitTarget = End;
		}
		else{
			HitTarget = TraceHitResult.ImpactPoint;
			DrawDebugSphere(GetWorld(), TraceHitResult.ImpactPoint, 12.f, 12, FColor::Red);
		}
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FHitResult OutHitResult;
	TraceUnderCrosshairs(OutHitResult);
}

