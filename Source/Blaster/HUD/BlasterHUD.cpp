// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterHUD.h"
#include "GameFramework/PlayerController.h"
#include "CharacterOverlay.h"

void ABlasterHUD::DrawHUD()
{
    Super::DrawHUD();

    FVector2D ViewportSize;
    if(GEngine){
        GEngine->GameViewport->GetViewportSize(ViewportSize);
        const FVector2D ViewportCenter(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);

        float SpreadScaled = CrosshairSpreadMax * HUDPackage.CrosshairSpread;

        if(HUDPackage.CrosshairCenter){
            FVector2D Spread(0.f, 0.f);
            DrawCrosshair(HUDPackage.CrosshairCenter, ViewportCenter, Spread, HUDPackage.CrosshairColor);
        }
        if(HUDPackage.CrosshairLeft){
            FVector2D Spread(-SpreadScaled, 0.f);
            DrawCrosshair(HUDPackage.CrosshairLeft, ViewportCenter, Spread, HUDPackage.CrosshairColor);
        }
        if(HUDPackage.CrosshairRight){
            FVector2D Spread(SpreadScaled, 0.f);
            DrawCrosshair(HUDPackage.CrosshairRight, ViewportCenter, Spread, HUDPackage.CrosshairColor);
        }
        if(HUDPackage.CrosshairTop){
            FVector2D Spread(0.f, -SpreadScaled);
            DrawCrosshair(HUDPackage.CrosshairTop, ViewportCenter, Spread, HUDPackage.CrosshairColor);
        }
        if(HUDPackage.CrosshairBottom){
            FVector2D Spread(0.f, SpreadScaled);
            DrawCrosshair(HUDPackage.CrosshairBottom, ViewportCenter, Spread, HUDPackage.CrosshairColor);
        }
    }
}

void ABlasterHUD::BeginPlay()
{
    Super::BeginPlay();

    AddCharacterOverlay();
}

void ABlasterHUD::AddCharacterOverlay()
{
    APlayerController* PlayerController = GetOwningPlayerController();
    if(PlayerController && CharacterOverlayClass){
        CharacterOverlay = CreateWidget<UCharacterOverlay>(PlayerController, CharacterOverlayClass);
        CharacterOverlay->AddToViewport();
    }
}

void ABlasterHUD::DrawCrosshair(UTexture2D *Texture, FVector2D ViewportCenter, FVector2D Spread, FLinearColor CrosshairColor)
{
    //this will draw the texture in the center of the HUD
    //by default, drawing a texture2d will draw it to the top left of the center, so it will look odd without this
    //the textures themselves have offsets, so drawing them in the middle will place them in the Top, Bottom, Left, Right, or Center
    //depending on the offset of the texture being drawn
    const float TextureWidth = Texture->GetSizeX();
    const float TextureHeight = Texture->GetSizeY();
    const FVector2D TextureDrawPoint(ViewportCenter.X - (TextureWidth / 2.f) + Spread.X, ViewportCenter.Y - (TextureHeight / 2.f) + Spread.Y);

    DrawTexture(Texture, TextureDrawPoint.X, TextureDrawPoint.Y, TextureWidth, TextureHeight, 0.f, 0.f, 1.f, 1.f, CrosshairColor);
}
