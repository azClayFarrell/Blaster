// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterPlayerController.h"
#include "Blaster/HUD/BlasterHUD.h"
#include "Blaster/HUD/CharacterOverlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"

void ABlasterPlayerController::BeginPlay()
{
    Super::BeginPlay();

    BlasterHUD = Cast<ABlasterHUD>(GetHUD());
}

void ABlasterPlayerController::SetHUDTime()
{
    uint32 SecondsLeft = FMath::CeilToInt(MatchTime - GetServerTime());
    if(CountdownInt != SecondsLeft){
        SetHUDMatchCountdown(MatchTime - GetServerTime());
    }
    CountdownInt = SecondsLeft;
}

void ABlasterPlayerController::PollInit()
{
    //this is needing to be done because the overlay does not yet exist when we transition into the map
    if(CharacterOverlay == nullptr){
        if(BlasterHUD && BlasterHUD->CharacterOverlay){
            CharacterOverlay = BlasterHUD->CharacterOverlay;
            if(CharacterOverlay){
                SetHUDHealth(HUDCurrentHealth, HUDMaxHealth);
                SetHUDScore(HUDScore);
                SetHUDDefeats(HUDDefeats);
            }
        }
    }
}

void ABlasterPlayerController::ServerRequestServerTime_Implementation(float TimeOfClientRequest)
{
    //this function is called from the client that is connecting

    float ServerTimeOfReceipt = GetWorld()->GetTimeSeconds();

    //this function is calling an RPC on the client to tell them what the time is on the server
    ClientReportServerTime(TimeOfClientRequest, ServerTimeOfReceipt);
}

void ABlasterPlayerController::ClientReportServerTime_Implementation(float TimeOfClientRequest, float TimeServerReceivedClientRequest)
{
    //this function is being called by the server because this client has requested to know what the time on the server is

    float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;
    float CurrentServerTime = TimeServerReceivedClientRequest + (0.5f * RoundTripTime);
    ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds();
}

void ABlasterPlayerController::CheckTimeSync(float DeltaTime)
{
    TimeSyncRunningTime += DeltaTime;
    if(IsLocalController() && TimeSyncRunningTime > TimeSyncFrequency){
        //this is being called every so often to resync with the server to avoid time drift
        ServerRequestServerTime(GetWorld()->GetTimeSeconds());
        TimeSyncRunningTime = 0.f;
    }
}

void ABlasterPlayerController::OnRep_MatchState()
{
    if(MatchState == MatchState::InProgress){
        BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
        if(BlasterHUD){
            BlasterHUD->AddCharacterOverlay();
        }
    }
}

void ABlasterPlayerController::SetHUDHealth(float CurrentHealth, float MaxHealth)
{
    //making sure that we are trying to get the blaster hud before using it
    BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;

    //checking if the hud, overlay, and all it's components are valid
    bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay && BlasterHUD->CharacterOverlay->HealthBar && BlasterHUD->CharacterOverlay->HealthText;

    if(bHUDValid){
        const float HealthPercent = CurrentHealth / MaxHealth;
        BlasterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
        FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(CurrentHealth), FMath::CeilToInt(MaxHealth));
        BlasterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
    }
    else{
        bInitializeCharacterOverlay = true;
        HUDCurrentHealth = CurrentHealth;
        HUDMaxHealth = MaxHealth;
    }
}

void ABlasterPlayerController::SetHUDScore(float Score)
{
    //making sure that we are trying to get the blaster hud before using it
    BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;

    //checking if the hud, overlay, and all it's components are valid
    bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay && BlasterHUD->CharacterOverlay->ScoreAmount;
    if(bHUDValid){
        FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
        BlasterHUD->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
    }
    else{
        bInitializeCharacterOverlay = true;
        HUDScore = Score;
    }
}

void ABlasterPlayerController::SetHUDDefeats(int32 Defeats)
{
    BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;

    bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay && BlasterHUD->CharacterOverlay->DefeatsAmount;
    if(bHUDValid){
        FString DefeatsText = FString::Printf(TEXT("%d"), Defeats);
        BlasterHUD->CharacterOverlay->DefeatsAmount->SetText(FText::FromString(DefeatsText));
    }
    else{
        bInitializeCharacterOverlay = true;
        HUDDefeats = Defeats;
    }
}

void ABlasterPlayerController::SetHUDWeaponAmmo(int32 Ammo)
{
    BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->WeaponAmmoAmount;
	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		BlasterHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	}
}

void ABlasterPlayerController::SetHUDCarriedAmmo(int32 Ammo)
{
    BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->CarriedAmmoAmount;
	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		BlasterHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(AmmoText));
	}
}

void ABlasterPlayerController::SetHUDMatchCountdown(float CountdownTime)
{
    BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->MatchCountdownText;
	if (bHUDValid)
	{
        int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
        int32 Seconds = CountdownTime - Minutes * 60;

		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->CharacterOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::OnPossess(APawn *InPawn)
{
    Super::OnPossess(InPawn);

    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(InPawn);
    if(BlasterCharacter){
        SetHUDHealth(BlasterCharacter->GetHealth(), BlasterCharacter->GetMaxHealth());
    }
}

float ABlasterPlayerController::GetServerTime()
{
    if(HasAuthority()) {return GetWorld()->GetTimeSeconds();}
    else {return GetWorld()->GetTimeSeconds() + ClientServerDelta;}
}

void ABlasterPlayerController::ReceivedPlayer()
{
    Super::ReceivedPlayer();

    if(IsLocalController()){
        //this is the earliest a client can request to be synced with the server that it connects to
        ServerRequestServerTime(GetWorld()->GetTimeSeconds());
    }
}

void ABlasterPlayerController::OnMatchStateSet(FName State)
{
    MatchState = State;

    if(MatchState == MatchState::InProgress){
        BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
        if(BlasterHUD){
            BlasterHUD->AddCharacterOverlay();
        }
    }
}

void ABlasterPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    SetHUDTime();
    CheckTimeSync(DeltaTime);
    PollInit();
}

void ABlasterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABlasterPlayerController, MatchState);
}
