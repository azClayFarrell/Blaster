// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
ABlasterCharacter::ABlasterCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 500.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
}

// Called when the game starts or when spawned
void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called to bind functionality to input
void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);\

	PlayerInputComponent->BindAxis("MoveForward", this, &ThisClass::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ThisClass::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &ThisClass::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ThisClass::LookUp);

	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ThisClass::AimButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &ThisClass::AimButtonReleased);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ThisClass::CrouchButtonPressed);
	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &ThisClass::EquipButtonPressed);

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
}

void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if(Combat){
		Combat->Character = this;
	}
}

void ABlasterCharacter::MoveForward(float Value)
{
	if(Controller && Value != 0.f){
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		//just know this gets the direction of the controller
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		//This does not determine speed, only direction. The speed is handled by the character movement component
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::MoveRight(float Value)
{
	
	if(Controller && Value != 0.f){
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		//just know this gets the direction of the controller
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		//This does not determine speed, only direction. The speed is handled by the character movement component
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::Turn(float Value)
{
	AddControllerYawInput(Value);
}

void ABlasterCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}

void ABlasterCharacter::EquipButtonPressed()
{
	//this will be called on every connected client, so we will have to perform checks to make sure the server is equipping
	//weapons since the server has authority over the weapons
	if(Combat){
		if(HasAuthority()){
			Combat->EquipWeapon(OverlappingWeapon);
		}
		else{
			//we do not need to have the _Implementation here, this is just for when we are defining the function
			ServerEquipButtonPressed();
		}
	}
}

void ABlasterCharacter::CrouchButtonPressed()
{
	//this is a function on the character class, which passes off a lot of behavior on the character movement component
	//this is replicated to clients as specified in the Character.h file
	if(bIsCrouched){
		UnCrouch();
	}
	else{
		Crouch();
	}
}

void ABlasterCharacter::AimButtonPressed()
{
	if(Combat){
		Combat->SetAiming(true);
	}
}

void ABlasterCharacter::AimButtonReleased()
{
	if(Combat){
		Combat->SetAiming(false);
	}
}

void ABlasterCharacter::AimOffset(float DeltaTime)
{
	//we are only concerned with aim offsets if we have a weapon, so return out if there isn't one equipped
	if(Combat && Combat->EquippedWeapon == nullptr){
		return;
	}
	//gets the speed of the character and if they are falling
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	float Speed = Velocity.Size();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	//if the character is standing still and not jumping
	if(Speed == 0.f && !bIsInAir){
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		//order in which you pass the rotators here is important. If you pass the rotators in the other way, the yaw direction will be flipped the wrong way
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		bUseControllerRotationYaw = false;
	}

	//if character is running or jumping
	if(Speed > 0.f || bIsInAir){
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
	}

	AO_Pitch = GetBaseAimRotation().Pitch;

	// //this was used to debug the problem we were having
	// UE_LOG(LogTemp, Warning, TEXT("AO_Pitch: %f"), AO_Pitch);
	// //this was used to check the client controlled pawn values as they are on the server, which is the root of the issue
	// if(!HasAuthority() && !IsLocallyControlled()){
	// 	UE_LOG(LogTemp, Warning, TEXT("AO_Pitch: %f"), AO_Pitch);
	// }

	//this is the fix for the buggy aiming. Since we know it does not occer on pawns we are controlling, we only need to
	//adjust for pawns we are not locally controlling
	if(AO_Pitch > 90.f && !IsLocallyControlled()){
		//map pitch from [270, 360) to [-90, 0)
		//this will fix the jerky up and down aiming that we saw when using the aim offsets in multiplayer
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		//this is the method that actually does the mapping of one range to another
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	if(OverlappingWeapon){
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if(LastWeapon){
		LastWeapon->ShowPickupWidget(false);
	}
}

void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	//this will be called on every connected client, so we will have to perform checks to make sure the server is equipping
	//weapons since the server has authority over the weapons
	if(Combat){
		Combat->EquipWeapon(OverlappingWeapon);
	}
}

void ABlasterCharacter::SetOverlappingWeapon(AWeapon *Weapon)
{
	if(OverlappingWeapon){
		OverlappingWeapon->ShowPickupWidget(false);
	}

	OverlappingWeapon = Weapon;

	if(IsLocallyControlled()){
		if(OverlappingWeapon){
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

bool ABlasterCharacter::IsWeaponEquipped()
{
    return (Combat && Combat->EquippedWeapon);
}

bool ABlasterCharacter::IsAiming()
{
    return (Combat && Combat->bAiming);
}

AWeapon *ABlasterCharacter::GetEquippedWeapon()
{
	if(Combat == nullptr){
	    return nullptr;
	}
	return Combat->EquippedWeapon;
}

// Called every frame
void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AimOffset(DeltaTime);
}