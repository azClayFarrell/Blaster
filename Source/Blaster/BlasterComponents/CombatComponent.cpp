// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Sound/SoundCue.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Weapon/Projectile.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	BaseWalkSpeed = 600.f;
	AimWalkSpeed = 450.f;
}

void UCombatComponent::EquipWeapon(AWeapon *WeaponToEquip)
{
	//if there is no valid character to equip weapons to or the Weapon to Equip is not valid then we will return out
	if (Character == nullptr || WeaponToEquip == nullptr) return;
	if (CombatState != ECombatState::ECS_Unoccupied) return;

	DropEquippedWeapon();
	EquippedWeapon = WeaponToEquip;
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	AttachActorToRightHand(EquippedWeapon);
	EquippedWeapon->SetOwner(Character);
	EquippedWeapon->SetHUDAmmo();
	UpdateCarriedAmmo();
	PlayEquipWeaponSound();
	ReloadEmptyWeapon();

	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;
}

void UCombatComponent::DropEquippedWeapon()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->Dropped();
	}
}

void UCombatComponent::AttachActorToRightHand(AActor* ActorToAttach)
{
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr) return;
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (HandSocket)
	{
		HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
	EquippedWeapon->SetOwner(Character);
	EquippedWeapon->SetHUDAmmo();
}

void UCombatComponent::AttachActorToLeftHand(AActor* ActorToAttach)
{
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr || EquippedWeapon == nullptr) return;
	bool bUsePistolSocket = 
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Pistol ||
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SubmachineGun;
	FName SocketName = bUsePistolSocket ? FName("PistolSocket") : FName("LeftHandSocket");
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(SocketName);
	if (HandSocket)
	{
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::UpdateCarriedAmmo()
{
	if (EquippedWeapon == nullptr) return;
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller)
	{
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
}

void UCombatComponent::PlayEquipWeaponSound()
{
	if (Character && EquippedWeapon && EquippedWeapon->EquipSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			EquippedWeapon->EquipSound,
			Character->GetActorLocation()
		);
	}
}

void UCombatComponent::ReloadEmptyWeapon()
{
	if (EquippedWeapon && EquippedWeapon->IsEmpty())
	{
		Reload();
	}
}

void UCombatComponent::Reload()
{
	if(CarriedAmmo > 0 && CombatState == ECombatState::ECS_Unoccupied && EquippedWeapon && !EquippedWeapon->IsFull()){
		ServerReload();
	}
}

void UCombatComponent::FinishReloading()
{
	if(Character == nullptr) {return;}
	if(Character->HasAuthority()){
		CombatState = ECombatState::ECS_Unoccupied;
		UpdateAmmoValues();
	}
	if(bFireButtonPressed){
		Fire();
	}
}

void UCombatComponent::HandleReload()
{
	Character->PlayReloadMontage();
}

int32 UCombatComponent::AmountToReload()
{
	if(EquippedWeapon == nullptr) {return 0;}
	int32 RoomInMag = EquippedWeapon->GetMagCapacity() - EquippedWeapon->GetAmmo();

	if(CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())){
		int32 AmountCarried = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
		int32 Least = FMath::Min(RoomInMag, AmountCarried);
		return FMath::Clamp(RoomInMag, 0, Least);
	}
	return 0;
}

void UCombatComponent::ThrowGrenade()
{
	if(Grenades == 0) {return;}
	if (CombatState != ECombatState::ECS_Unoccupied || EquippedWeapon == nullptr) return;
	CombatState = ECombatState::ECS_ThrowingGrenade;
	if (Character)
	{
		Character->PlayThrowGrenadeMontage();
		//this is the line I missed last lecture
		AttachActorToLeftHand(EquippedWeapon);

		ShowAttachedGrenade(true);
	}
	if (Character && !Character->HasAuthority())
	{
		ServerThrowGrenade();
	}
	if(Character && Character->HasAuthority()){
		Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
		UpdateHUDGrenades();
	}
}

void UCombatComponent::ServerThrowGrenade_Implementation()
{
	if(Grenades == 0) {return;}
	CombatState = ECombatState::ECS_ThrowingGrenade;
	if (Character)
	{
		Character->PlayThrowGrenadeMontage();
		AttachActorToLeftHand(EquippedWeapon);
		ShowAttachedGrenade(true);
	}
	Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
	UpdateHUDGrenades();
}

void UCombatComponent::UpdateHUDGrenades()
{
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller)
	{
		Controller->SetHUDGrenades(Grenades);
	}
}

void UCombatComponent::ShowAttachedGrenade(bool bShowGrenade)
{
	if (Character && Character->GetAttachedGrenade())
	{
		Character->GetAttachedGrenade()->SetVisibility(bShowGrenade);
	}
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, bAiming);
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	//server will only replicate the CarriedAmmo count to the client that it pertains to, which is the owner
	DOREPLIFETIME_CONDITION(UCombatComponent, CarriedAmmo, COND_OwnerOnly);

	DOREPLIFETIME(UCombatComponent, CombatState);
	DOREPLIFETIME(UCombatComponent, Grenades);
}

void UCombatComponent::ShotgunShellReload()
{
	if (Character && Character->HasAuthority())
	{
		UpdateShotgunAmmoValues();
	}

}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if(Character){
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

		if(Character->GetFollowCamera()){
			DefaultFOV = Character->GetFollowCamera()->FieldOfView;
			CurrentFOV = DefaultFOV;
		}
		if(Character->HasAuthority()){
			InitializeCarriedAmmo();
		}
	}
}

void UCombatComponent::SetAiming(bool bIsAiming)
{
	if(Character == nullptr || EquippedWeapon == nullptr) {return;}
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
	if(Character->IsLocallyControlled() && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle){
		Character->ShowSniperScopeWidget(bAiming);
	}
}

void UCombatComponent::PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount)
{
	if(CarriedAmmoMap.Contains(WeaponType)){
		CarriedAmmoMap[WeaponType] = FMath::Clamp(CarriedAmmoMap[WeaponType] + AmmoAmount, 0, MaxCarriedAmmo);
		UpdateCarriedAmmo();
	}

	//if the weapon is empty, reload when ammo is picked up
	if(EquippedWeapon && EquippedWeapon->IsEmpty() && EquippedWeapon->GetWeaponType() == WeaponType){
		Reload();
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
		
		EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
		AttachActorToRightHand(EquippedWeapon);
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;
		PlayEquipWeaponSound();
	}
}

void UCombatComponent::FireButtonPressed(bool bPressed)
{
	//We don't want to replicate this variable to the server so we are going to use Multicast RPCs to tell clients to fire the weapons
	bFireButtonPressed = bPressed;

	// we are now checking if we have a weapon in the CanFire function so we don't need to check it here anymore
	if(bFireButtonPressed){
		Fire();
	}
}

void UCombatComponent::Fire()
{
	if(CanFire()){
		bCanFire = false;
		ServerFire(HitTarget);
		if(EquippedWeapon){
			CrosshairShootingFactor = 0.75f;
		}
		StartFireTimer();
	}
}

void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	//calling this Multicast RPC will make the server pawn fire and propagate the firing of the weapon to all clients
	MulticastFire(TraceHitTarget);
}

void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget){
	//we need to check this since we are wanting to call the fire function on the Weapon class
	if(EquippedWeapon == nullptr) {return;}

	if (Character && CombatState == ECombatState::ECS_Reloading && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun)
	{
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
		CombatState = ECombatState::ECS_Unoccupied;
		return;
	}

	if(Character && CombatState == ECombatState::ECS_Unoccupied){
		//bAiming is replicated, so all clients will know if we are aiming or not
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
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

		if(Character){
			float DistanceToCharacter = (Character->GetActorLocation() - Start).Size();
			Start += OutCrosshairWorldDirection * (DistanceToCharacter + 100.f);
			// // this is a debug sphere to see where the start location would be for the line trace
			// DrawDebugSphere(GetWorld(), Start, 16.f, 12, FColor::Red, false);
		}

		FVector End = Start + OutCrosshairWorldDirection * TRACE_LENGTH;

		GetWorld()->LineTraceSingleByChannel(TraceHitResult, Start, End, ECollisionChannel::ECC_Visibility);

		//if we hit nothing in the hit result, then we will just set the impact point as the End point
		//if we do not have this with our current implementation, the bullet will fly to the world origin if we hit nothing
		if(!TraceHitResult.bBlockingHit){
			TraceHitResult.ImpactPoint = End;
		}

		if(TraceHitResult.GetActor() && TraceHitResult.GetActor()->Implements<UInteractWithCrosshairsInterface>()){
			HUDPackage.CrosshairColor = FLinearColor::Red;
		}
		else{
			HUDPackage.CrosshairColor = FLinearColor::White;
		}
	}
}

void UCombatComponent::SetHUDCrosshairs(float DeltaTime)
{
	if(Character == nullptr || Character->Controller == nullptr){
		return;
	}
	//we will try to cast the character controller if the value is currently null
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if(Controller){
		//we will attempt to cast the HUD to the type we want to use if the current value is null
		HUD = HUD == nullptr ? Cast<ABlasterHUD>(Controller->GetHUD()) : HUD;
		if(HUD){
			
			//making the FHUDPackage using this weapons information about the crosshairs it uses
			if(EquippedWeapon){
				HUDPackage.CrosshairCenter = EquippedWeapon->CrosshairCenter;
				HUDPackage.CrosshairLeft = EquippedWeapon->CrosshairLeft;
				HUDPackage.CrosshairRight = EquippedWeapon->CrosshairRight;
				HUDPackage.CrosshairTop = EquippedWeapon->CrosshairTop;
				HUDPackage.CrosshairBottom = EquippedWeapon->CrosshairBottom;
			}
			//if we do not have a weapon equipped we should not draw any crosshairs
			else{
				HUDPackage.CrosshairCenter = nullptr;
				HUDPackage.CrosshairLeft = nullptr;
				HUDPackage.CrosshairRight = nullptr;
				HUDPackage.CrosshairTop = nullptr;
				HUDPackage.CrosshairBottom = nullptr;
			}

			//Calculate crosshair spread

			//map character movement speed [0, 600] to [0, 1]
			FVector2D WalkSpeedRange(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);
			FVector2D VelocityMultiplierRange(0.f, 1.f);
			FVector Velocity = Character->GetVelocity();
			Velocity.Z = 0.f;

			//does the actual conversion
			CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());

			if(Character->GetCharacterMovement()->IsFalling()){
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
			}
			else{
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
			}

			if(bAiming){
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.58f, DeltaTime, 30.f);
			}
			else{
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);
			}

			CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 40.f);

			HUDPackage.CrosshairSpread = 0.5f + CrosshairVelocityFactor + CrosshairInAirFactor - CrosshairAimFactor + CrosshairShootingFactor;

			HUD->SetHUDPackage(HUDPackage);
		}
	}
}

void UCombatComponent::ServerReload_Implementation()
{
	if(Character == nullptr || EquippedWeapon == nullptr) { return; }
	CombatState = ECombatState::ECS_Reloading;
	HandleReload();
}

void UCombatComponent::InterpFOV(float DeltaTime)
{
	if(EquippedWeapon == nullptr){
		return;
	}

	//determing the current value of the FOV based on if we are aiming or not and what the weapons FOV settings are
	if(bAiming){
		CurrentFOV = FMath::FInterpTo(CurrentFOV, EquippedWeapon->GetZoomedFOV(), DeltaTime, EquippedWeapon->GetZoomInterpSpeed());
	}
	else{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, ZoomInterpSpeed);
	}

	//updating the camera after you determine the FOV to be set
	if(Character && Character->GetFollowCamera()){
		Character->GetFollowCamera()->SetFieldOfView(CurrentFOV);
	}
}

void UCombatComponent::StartFireTimer()
{
	if(EquippedWeapon == nullptr || Character == nullptr){
		return;
	}

	Character->GetWorldTimerManager().SetTimer(FireTimer, this, &ThisClass::FireTimerFinished, EquippedWeapon->FireDelay);
}

void UCombatComponent::FireTimerFinished()
{
	if(EquippedWeapon == nullptr){
		return;
	}

	bCanFire = true;
	if(bFireButtonPressed && EquippedWeapon->bAutomatic){
		Fire();
	}
	ReloadEmptyWeapon();
}

bool UCombatComponent::CanFire()
{
    if(EquippedWeapon == nullptr) {return false;}
	if (!EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Reloading && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun) return true;
	return !EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Unoccupied;
}

void UCombatComponent::OnRep_CarriedAmmo()
{
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if(Controller){
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	bool bJumpToShotgunEnd = 
		CombatState == ECombatState::ECS_Reloading &&
		EquippedWeapon != nullptr &&
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun &&
		CarriedAmmo == 0;
	if (bJumpToShotgunEnd)
	{
		JumpToShotgunEnd();
	}
}

void UCombatComponent::InitializeCarriedAmmo()
{
	CarriedAmmoMap.Emplace(EWeaponType::EWT_AssaultRifle, StartingARAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_RocketLauncher, StartingRocketAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Pistol, StartingPistolAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SubmachineGun, StartingSMGAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Shotgun, StartingShotgunAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SniperRifle, StartingSniperAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_GrenadeLauncher, StartingGrenadeLauncherAmmo);
}

void UCombatComponent::OnRep_CombatState()
{
	switch(CombatState){
		case ECombatState::ECS_Reloading:
			HandleReload();
			break;
		case ECombatState::ECS_Unoccupied:
			if(bFireButtonPressed){
				Fire();
			}
			break;
		case ECombatState::ECS_ThrowingGrenade:
			if (Character && !Character->IsLocallyControlled())
			{
				Character->PlayThrowGrenadeMontage();
				AttachActorToLeftHand(EquippedWeapon);
				ShowAttachedGrenade(true);
			}
			break;
	}
}

void UCombatComponent::UpdateAmmoValues()
{
	if(Character == nullptr || EquippedWeapon == nullptr) {return;}
	int32 ReloadAmount = AmountToReload();
	if(CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())){
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= ReloadAmount;
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if(Controller){
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	EquippedWeapon->AddAmmo(-ReloadAmount);
}

void UCombatComponent::UpdateShotgunAmmoValues()
{
	if (Character == nullptr || EquippedWeapon == nullptr) return;

	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= 1;
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller)
	{
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	EquippedWeapon->AddAmmo(-1);
	bCanFire = true;
	if (EquippedWeapon->IsFull() || CarriedAmmo == 0)
	{
		JumpToShotgunEnd();
	}
}

void UCombatComponent::JumpToShotgunEnd()
{
	// Jump to ShotgunEnd section
	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (AnimInstance && Character->GetReloadMontage())
	{
		AnimInstance->Montage_JumpToSection(FName("ShotgunEnd"));
	}
}

void UCombatComponent::ThrowGrenadeFinished()
{
	CombatState = ECombatState::ECS_Unoccupied;
	AttachActorToRightHand(EquippedWeapon);
}

void UCombatComponent::LaunchGrenade()
{
	ShowAttachedGrenade(false);
	if (Character && Character->IsLocallyControlled())
	{
		ServerLaunchGrenade(HitTarget);
	}
}

void UCombatComponent::ServerLaunchGrenade_Implementation(const FVector_NetQuantize & Target)
{
	if(Character && GrenadeClass && Character->GetAttachedGrenade()){
		const FVector StartingLocation = Character->GetAttachedGrenade()->GetComponentLocation();
		FVector ToTarget = Target - StartingLocation;
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Character;
		SpawnParams.Instigator = Character;
		UWorld* World = GetWorld();
		if (World)
		{
			World->SpawnActor<AProjectile>(GrenadeClass, StartingLocation, ToTarget.Rotation(), SpawnParams);
		}
	}
}

void UCombatComponent::OnRep_Grenades()
{
	UpdateHUDGrenades();
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);


	//we are doing this for drawing debug lines to see how far off our muzzle is from being on target
	if(Character && Character->IsLocallyControlled()){
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		HitTarget = HitResult.ImpactPoint;

		SetHUDCrosshairs(DeltaTime);
		InterpFOV(DeltaTime);
	}
}

