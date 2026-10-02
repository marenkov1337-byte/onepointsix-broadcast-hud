#include "stdafx.h"

#pragma once

class CMinimap {
public:
    static void Draw();
    static void OnTextureDeleted(GLuint texture);

private:
    static void LoadForCurrentMap();

    static hud_texture_s s_texture;
    static char s_szLoadedMap[64];
    static bool s_bLoadFailed;

    // Calibration parsed from overviews/<map>.txt
    static float s_fZoom;
    static float s_fOriginX;
    static float s_fOriginY;
    static bool s_bRotated;
};
