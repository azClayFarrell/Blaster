// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Casing.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/BlasterComponents/CombatComponent.h"

// Sets default values
AWeapon::AWeapon()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	//this makes the server have authority over all the weapons
	bReplicates = true;

	//this is important because we need the overlap events to be consistent and the clients can see the location of the weapon
	// in the same area as the server so that the clients can pick up the weapons
	SetReplicateMovement(true);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

	//this is for dropping the weapon on the ground. The first line is so that the weapon collides with the ground and walls
	WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	//the second line is so that the weapon does NOT collide or otherwise block player movement
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	//This is so the player can walk through it to be picked up. It's like Halo by default
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	EnableCustomDepth(true);
	WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_BLUE);
	WeaponMesh->MarkRenderStateDirty();


	//this is what we will use to detect overlaps with characters, so we can then display a pickup widget among other things
	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
	AreaSphere->SetupAttachment(RootComponent);

	//we are disabling collision here for all players. Then we will Enable it on the server side so that the server has
	//authority over the collision and pickup events and such
	AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(RootComponent);
}

void AWeapon::EnableCustomDepth(bool bEnable)
{
	if (WeaponMesh)
	{
		WeaponMesh->SetRenderCustomDepth(bEnable);
	}
}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeapon, WeaponState);
	DOREPLIFETIME(AWeapon, Ammo);
}

void AWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();

	if(Owner == nullptr){
		BlasterOwnerCharacter = nullptr;
		BlasterOwnerController = nullptr;
	}
	else{
		SetHUDAmmo();
	}
}

void AWeapon::SetHUDAmmo()
{
	BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;
	if(BlasterOwnerCharacter){
		BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(BlasterOwnerCharacter->Controller) : BlasterOwnerController;
		if(BlasterOwnerController){
			BlasterOwnerController->SetHUDWeaponAmmo(Ammo);
		}
	}
}

bool AWeapon::IsEmpty()
{
    return Ammo <= 0;
}

bool AWeapon::IsFull()
{
    return Ammo == MagCapacity;
}

void AWeapon::Fire(const FVector &HitTarget)
{
	if(FireAnimation){
		//we don't want to loop the animation we are wanting to play, so we will pass false for looping
		WeaponMesh->PlayAnimation(FireAnimation, false);
	}

	if(CasingClass){
		const USkeletalMeshSocket* AmmoEjectSocket = WeaponMesh->GetSocketByName(FName("AmmoEject"));
		if(AmmoEjectSocket){
			FTransform SocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh);

			UWorld* World = GetWorld();
			if(World){
				World->SpawnActor<ACasing>(CasingClass, SocketTransform.GetLocation(), SocketTransform.GetRotation().Rotator());
			}
		}
	}
	SpendRound();
}

// Called when the game starts or when spawned
void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	//this check is equivalent shorthand for (GetLocalRole() == ENetRole::ROLE_Authority)
	if(HasAuthority()){
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
		AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnSphereOverlap);
		AreaSphere->OnComponentEndOverlap.AddDynamic(this, &ThisClass::OnSphereEndOverlap);
	}
	
	if(PickupWidget){
		//we want the widget to be off by default on game start
		PickupWidget->SetVisibility(false);
	}
}

void AWeapon::OnSphereOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	if(BlasterCharacter){
		//this is now replicated across server and client
		BlasterCharacter->SetOverlappingWeapon(this);
	}
}

void AWeapon::OnSphereEndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	if(BlasterCharacter){
		//This will be used by the Blaster character Rep Notify function and will check against nullptr to remove the widget
		BlasterCharacter->SetOverlappingWeapon(nullptr);
	}
}

// Called every frame
void AWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWeapon::ShowPickupWidget(bool bShowWidget)
{
	if(PickupWidget){
		PickupWidget->SetVisibility(bShowWidget);
	}
}

void AWeapon::SetWeaponState(EWeaponState State)
{
	WeaponState = State;

	switch(WeaponState){
		case EWeaponState::EWS_Equipped:
			//once the weapon has been picked up, we can disable the widget
			ShowPickupWidget(false);
			
			//disable collisions for the area sphere that the weapon has
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			//we have to do these three lines or else attaching the gun to another players hand socket won't work
			WeaponMesh->SetSimulatePhysics(false);
			WeaponMesh->SetEnableGravity(false);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			if(WeaponType == EWeaponType::EWT_SubmachineGun){
				WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				WeaponMesh->SetEnableGravity(true);
				WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
			}
			EnableCustomDepth(false);
			break;
		
		case EWeaponState::EWS_Dropped:
			if(HasAuthority()){
				AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			}
			WeaponMesh->SetSimulatePhysics(true);
			WeaponMesh->SetEnableGravity(true);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
			WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
			WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);

			WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_BLUE);
			WeaponMesh->MarkRenderStateDirty();
			EnableCustomDepth(true);
			break;
	}
}

void AWeapon::OnRep_WeaponState()
{
	switch(WeaponState){
		case EWeaponState::EWS_Equipped:
			ShowPickupWidget(false);

			//we have to do these three lines or else attaching the gun to another players hand socket won't work
			WeaponMesh->SetSimulatePhysics(false);
			WeaponMesh->SetEnableGravity(false);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if(WeaponType == EWeaponType::EWT_SubmachineGun){
				WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				WeaponMesh->SetEnableGravity(true);
				WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
			}
			EnableCustomDepth(false);
			break;
		
		case EWeaponState::EWS_Dropped:
			WeaponMesh->SetSimulatePhysics(true);
			WeaponMesh->SetEnableGravity(true);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
			WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
			WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);

			WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_BLUE);
			WeaponMesh->MarkRenderStateDirty();
			EnableCustomDepth(true);
			break;
	}
}

void AWeapon::OnRep_Ammo()
{
	BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;
	if (BlasterOwnerCharacter && BlasterOwnerCharacter->GetCombat() && IsFull())
	{
		BlasterOwnerCharacter->GetCombat()->JumpToShotgunEnd();
	}
	SetHUDAmmo();
}

void AWeapon::SpendRound()
{
	Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
	SetHUDAmmo();
}

void AWeapon::Dropped()
{
	SetWeaponState(EWeaponState::EWS_Dropped);
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	WeaponMesh->DetachFromComponent(DetachRules);
	SetOwner(nullptr);

	BlasterOwnerCharacter = nullptr;
	BlasterOwnerController = nullptr;
}

void AWeapon::AddAmmo(int32 AmmoToAdd)
{
	Ammo = FMath::Clamp(Ammo - AmmoToAdd, 0, MagCapacity);
	SetHUDAmmo();
}
