// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterHUD.h"

void ABlasterHUD::DrawHUD()
{
    Super::DrawHUD();

    FVector2D ViewportSize;
    if(GEngine){
        GEngine->GameViewport->GetViewportSize(ViewportSize);
        const FVector2D ViewportCenter(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);

        if(HUDPackage.CrosshairCenter){
            DrawCrosshair(HUDPackage.CrosshairCenter, ViewportCenter);
        }
        if(HUDPackage.CrosshairLeft){
            DrawCrosshair(HUDPackage.CrosshairLeft, ViewportCenter);
        }
        if(HUDPackage.CrosshairRight){
            DrawCrosshair(HUDPackage.CrosshairRight, ViewportCenter);
        }
        if(HUDPackage.CrosshairTop){
            DrawCrosshair(HUDPackage.CrosshairTop, ViewportCenter);
        }
        if(HUDPackage.CrosshairBottom){
            DrawCrosshair(HUDPackage.CrosshairBottom, ViewportCenter);
        }
    }
}

void ABlasterHUD::DrawCrosshair(UTexture2D *Texture, FVector2D ViewportCenter)
{
    //this will draw the texture in the center of the HUD
    //by default, drawing a texture2d will draw it to the top left of the center, so it will look odd without this
    //the textures themselves have offsets, so drawing them in the middle will place them in the Top, Bottom, Left, Right, or Center
    //depending on the offset of the texture being drawn
    const float TextureWidth = Texture->GetSizeX();
    const float TextureHeight = Texture->GetSizeY();
    const FVector2D TextureDrawPoint(ViewportCenter.X - (TextureWidth / 2.f), ViewportCenter.Y - (TextureHeight / 2.f));

    DrawTexture(Texture, TextureDrawPoint.X, TextureDrawPoint.Y, TextureWidth, TextureHeight, 0.f, 0.f, 1.f, 1.f, FLinearColor::White);
}
