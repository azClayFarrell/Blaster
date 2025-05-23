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
#include "Blaster/HUD/Announcement.h"
#include "Kismet/GameplayStatics.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Blaster/GameState/BlasterGameState.h"

void ABlasterPlayerController::BeginPlay()
{
    Super::BeginPlay();

    BlasterHUD = Cast<ABlasterHUD>(GetHUD());
    ServerCheckMatchState();
}

void ABlasterPlayerController::SetHUDTime()
{
    float TimeLeft = 0.f;

    if(MatchState == MatchState::WaitingToStart) {TimeLeft = WarmupTime - GetServerTime() + LevelStartingTime;}
    else if(MatchState == MatchState::InProgress) {TimeLeft = WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;}
    else if(MatchState == MatchState::Cooldown) {TimeLeft = CooldownTime + WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;}

    uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);

    if(HasAuthority()){
        BlasterGameMode = BlasterGameMode == nullptr ? Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this)) : BlasterGameMode;
        if(BlasterGameMode){
            SecondsLeft = FMath::CeilToInt(BlasterGameMode->GetCountdownTime() + LevelStartingTime);
        }
    }

    if(CountdownInt != SecondsLeft){
        if(MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown){
            SetHUDAnnouncementCountdown(TimeLeft);
        }
        if(MatchState == MatchState::InProgress){
            SetHUDMatchCountdown(TimeLeft);
        }
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
                
                ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
				if (BlasterCharacter && BlasterCharacter->GetCombat())
				{
					SetHUDGrenades(BlasterCharacter->GetCombat()->GetGrenades());
				}
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

void ABlasterPlayerController::ServerCheckMatchState_Implementation()
{
    ABlasterGameMode* GameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
    if(GameMode){
        WarmupTime = GameMode->WarmupTime;
        MatchTime = GameMode->MatchTime;
        CooldownTime = GameMode->CooldownTime;
        LevelStartingTime = GameMode->LevelStartingTime;
        MatchState = GameMode->GetMatchState();
        ClientJoinMidgame(MatchState, WarmupTime, MatchTime, CooldownTime, LevelStartingTime);
    }
}

void ABlasterPlayerController::ClientJoinMidgame_Implementation(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartTime)
{
    WarmupTime = Warmup;
    MatchTime = Match;
    LevelStartingTime = StartTime;
    MatchState = StateOfMatch;
    CooldownTime = Cooldown;
    OnMatchStateSet(MatchState);
    //if we join midgame during the in progress state, we will not add the announcement to the client screen
    if(BlasterHUD && MatchState == MatchState::WaitingToStart){
        BlasterHUD->AddAnnouncement();
    }
}

void ABlasterPlayerController::OnRep_MatchState()
{
    if(MatchState == MatchState::InProgress){
        HandleMatchHasStarted();
    }
    else if(MatchState == MatchState::Cooldown){
        HandleCooldown();
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

        if(CountdownTime < 0.f){
            BlasterHUD->CharacterOverlay->MatchCountdownText->SetText(FText());
            return;
        }

        int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
        int32 Seconds = CountdownTime - Minutes * 60;

		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->CharacterOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::SetHUDAnnouncementCountdown(float CountdownTime)
{
    BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
    bool bHUDValid = BlasterHUD &&
		BlasterHUD->Announcement &&
		BlasterHUD->Announcement->WarmupTime;
	if (bHUDValid)
	{
        if(CountdownTime < 0.f){
            BlasterHUD->Announcement->WarmupTime->SetText(FText());
            return;
        }

        int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
        int32 Seconds = CountdownTime - Minutes * 60;

		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->Announcement->WarmupTime->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::SetHUDGrenades(int32 Grenades)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->GrenadesText;
	if (bHUDValid)
	{
		FString GrenadesText = FString::Printf(TEXT("%d"), Grenades);
		BlasterHUD->CharacterOverlay->GrenadesText->SetText(FText::FromString(GrenadesText));
	}
	else
	{
		HUDGrenades = Grenades;
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
        HandleMatchHasStarted();
    }
    else if(MatchState == MatchState::Cooldown){
        HandleCooldown();
    }
}

void ABlasterPlayerController::HandleMatchHasStarted()
{
    BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	if (BlasterHUD)
	{
        if(BlasterHUD->CharacterOverlay == nullptr) {BlasterHUD->AddCharacterOverlay();}
        if(BlasterHUD->Announcement){
            BlasterHUD->Announcement->SetVisibility(ESlateVisibility::Hidden);
        }
	}
}

void ABlasterPlayerController::HandleCooldown()
{
    BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
    if(BlasterHUD){
        BlasterHUD->CharacterOverlay->RemoveFromParent();
        bool bHUDValid = BlasterHUD->Announcement && BlasterHUD->Announcement->AnnouncementText && BlasterHUD->Announcement->InfoText;
        if(bHUDValid){
            BlasterHUD->Announcement->SetVisibility(ESlateVisibility::Visible);
            FString AnnouncementText("New Match Starts In:");
            BlasterHUD->Announcement->AnnouncementText->SetText(FText::FromString(AnnouncementText));

            //everything below here is for getting the win info
            ABlasterGameState* BlasterGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
            ABlasterPlayerState* BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
            if(BlasterGameState && BlasterPlayerState){
                TArray<ABlasterPlayerState*> TopPlayers = BlasterGameState->TopScoringPlayers;
                FString InfoTextString;
                if(TopPlayers.Num() == 0){
                    InfoTextString = FString("There is no winner.");
                }
                else if(TopPlayers.Num() == 1 && TopPlayers[0] == BlasterPlayerState){
                    InfoTextString = FString("You are the winner!");
                }
                else if(TopPlayers.Num() == 1){
                    InfoTextString = FString::Printf(TEXT("Winner:\n%s"), *TopPlayers[0]->GetPlayerName());
                }
                else if(TopPlayers.Num() > 1){
                    InfoTextString = FString("Players tied for the win:\n");
                    for(auto TiedPlayer : TopPlayers){
                        InfoTextString.Append(FString::Printf(TEXT("%s\n"), *TiedPlayer->GetPlayerName()));
                    }
                }
                BlasterHUD->Announcement->InfoText->SetText(FText::FromString(InfoTextString));
            }
        }
    }

    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
    if(BlasterCharacter && BlasterCharacter->GetCombat()){
        BlasterCharacter->bDisableGameplay = true;
        BlasterCharacter->GetCombat()->FireButtonPressed(false);
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
