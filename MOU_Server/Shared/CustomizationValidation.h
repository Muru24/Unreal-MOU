#pragma once
#include "ChatProtocol.h"
#include <cmath>
namespace MOU
{
inline bool IsValidCustomization(const CharacterCustomization& D)
{
    auto Unit = [](float V) { return std::isfinite(V) && V >= 0 && V <= 1; };
    for (int I = 0; I < 4; ++I)
        if (!Unit(D.BodyColor[I]) || !Unit(D.DecalsColor[I])) return false;
    return Unit(D.Metallic) && Unit(D.RoughnessA) && Unit(D.RoughnessB)
        && D.DecalIndex >= 0 && D.DecalIndex < 6
        && std::isfinite(D.TilingX) && D.TilingX >= 0.01f && D.TilingX <= 20
        && std::isfinite(D.TilingY) && D.TilingY >= 0.01f && D.TilingY <= 20;
}
}
