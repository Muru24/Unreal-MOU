#pragma once
#include "Data/CustomizationTypes.h"
#include "CustomizationValidation.h"
namespace MOUCustomization
{
inline MOU::CharacterCustomization ToWire(const FCharacterCustomizationData& D)
{
 MOU::CharacterCustomization W;
 W.BodyColor[0] = D.BodyColor.R;
 W.BodyColor[1] = D.BodyColor.G;
 W.BodyColor[2] = D.BodyColor.B;
 W.BodyColor[3] = D.BodyColor.A;
 W.DecalsColor[0] = D.DecalsColor.R;
 W.DecalsColor[1] = D.DecalsColor.G;
 W.DecalsColor[2] = D.DecalsColor.B;
 W.DecalsColor[3] = D.DecalsColor.A;
 W.Metallic = D.Metallic;
 W.RoughnessB = D.RoughnessB;
 W.RoughnessA = D.RoughnessA;
 W.DecalIndex = D.DecalIndex;
 W.TilingX = D.TilingX;
 W.TilingY = D.TilingY;
 return W;
}
inline FCharacterCustomizationData FromWire(const MOU::CharacterCustomization& W)
{
 FCharacterCustomizationData D;
 D.BodyColor = FLinearColor(W.BodyColor[0], W.BodyColor[1], W.BodyColor[2], W.BodyColor[3]);
 D.DecalsColor = FLinearColor(W.DecalsColor[0], W.DecalsColor[1], W.DecalsColor[2], W.DecalsColor[3]);
 D.Metallic = W.Metallic;
 D.RoughnessB = W.RoughnessB;
 D.RoughnessA = W.RoughnessA;
 D.DecalIndex = W.DecalIndex;
 D.TilingX = W.TilingX;
 D.TilingY = W.TilingY;
 return D;
}
}
