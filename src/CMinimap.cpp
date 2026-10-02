#include "stdafx.h"

hud_texture_s CMinimap::s_texture = {0, 0, 0};
char CMinimap::s_szLoadedMap[64] = "";
bool CMinimap::s_bLoadFailed = false;
float CMinimap::s_fZoom = 1.F;
float CMinimap::s_fOriginX = 0.F;
float CMinimap::s_fOriginY = 0.F;
bool CMinimap::s_bRotated = false;

namespace {

// CS overview files are commonly uppercase (ZOOM/ORIGIN/ROTATED), while some
// custom maps use lowercase. Accept both.
bool ParseOverviewTxt(const char *path, float *zoom, float *originX, float *originY, bool *rotated) {
    FILE *fp = fopen(path, "r");
    if (fp == nullptr) return false;

    char token[128];
    bool gotZoom = false;
    bool gotOrigin = false;
    while (fscanf(fp, "%127s", token) == 1) {
        if (_stricmp(token, "zoom") == 0) {
            if (fscanf(fp, "%f", zoom) == 1) gotZoom = true;
        } else if (_stricmp(token, "origin") == 0) {
            float dummyZ = 0.F;
            if (fscanf(fp, "%f %f %f", originX, originY, &dummyZ) == 3) gotOrigin = true;
        } else if (_stricmp(token, "rotated") == 0) {
            int r = 0;
            if (fscanf(fp, "%d", &r) == 1) *rotated = (r != 0);
        }
    }
    fclose(fp);
    return gotZoom && gotOrigin;
}

int GetPlayerTeam(int slot) {
    char *model = CHelpers::szGetPlayerModel(slot);
    int team = model ? CHelpers::iGetTeam(model) : 0;
    if (team == 0) {
        const int tid = g_ScoreboardData[slot].teamId;
        if (tid == 1) team = 1;
        else if (tid == 2 || tid == 3) team = 2;
    }
    return team;
}

// Reproduces the world extents used by CS 1.6's CHudSpectator::DrawOverviewLayer.
// Returns normalized texture coordinates in the overview BMP (0..1).
bool WorldToOverview(float worldX, float worldY, float zoom, float originX, float originY,
                     bool rotated, float *u, float *v) {
    if (zoom <= 0.F) return false;
    const float aspect = 4.F / 3.F;

    if (rotated) {
        const float halfX = 4096.F / zoom;
        const float halfY = 4096.F / (zoom * aspect);
        *u = (worldX - (originX - halfX)) / (2.F * halfX);
        *v = ((originY + halfY) - worldY) / (2.F * halfY);
    } else {
        // Match GoldSrc CHudSpectator::DrawOverviewLayer exactly for ROTATED 0.
        // The overview texture axes are transposed relative to world space:
        // texture U spans world Y (full 8192/zoom range), while texture V
        // spans world X (8192/(zoom*4/3)). Both run in the negative world
        // direction from the overview origin.
        const float halfU = 4096.F / zoom;
        const float halfV = 4096.F / (zoom * aspect);
        *u = ((originY + halfU) - worldY) / (2.F * halfU);
        *v = ((originX + halfV) - worldX) / (2.F * halfV);
    }

    return *u >= 0.F && *u <= 1.F && *v >= 0.F && *v <= 1.F;
}

GLuint g_minimapCircleMask = 0;
GLuint g_minimapArrowMask = 0;

bool PointInPolygon(float x, float y, const float pts[][2], int count) {
    bool inside = false;
    for (int i = 0, j = count - 1; i < count; j = i++) {
        const float xi = pts[i][0], yi = pts[i][1];
        const float xj = pts[j][0], yj = pts[j][1];
        const bool crosses = ((yi > y) != (yj > y)) &&
            (x < (xj - xi) * (y - yi) / ((yj - yi) + 0.000001F) + xi);
        if (crosses) inside = !inside;
    }
    return inside;
}

GLuint CreateSmoothMaskTexture(bool arrow) {
    // Render the tiny minimap shapes into a supersampled alpha mask once, then let
    // GL_LINEAR downsample them in-game. This avoids the stair-stepped edges produced
    // by immediate-mode GL polygons at 14-20 screen pixels.
    const int texSize = 64;
    const int ss = 4;
    std::vector<unsigned char> alpha(texSize * texSize, 0);

        // v4.89: keep the same wide triangular base as v4.88, but pull only the tip
    // roughly 30% back toward the marker. This preserves the integrated/outlined
    // look while making the visible pointer noticeably shorter and less needle-like.
    // The circle is drawn afterwards and hides the inner base of the triangle.
    const float arrowPts[3][2] = {
        {-0.82F, -0.66F},
        { 0.44F,  0.00F},
        {-0.82F,  0.66F}
    };

    for (int py = 0; py < texSize; ++py) {
        for (int px = 0; px < texSize; ++px) {
            int covered = 0;
            for (int sy = 0; sy < ss; ++sy) {
                for (int sx = 0; sx < ss; ++sx) {
                    const float fx = ((px + (sx + 0.5F) / ss) / texSize) * 2.F - 1.F;
                    const float fy = ((py + (sy + 0.5F) / ss) / texSize) * 2.F - 1.F;
                    bool hit = false;
                    if (arrow) {
                        hit = PointInPolygon(fx, fy, arrowPts, 3);
                    } else {
                        hit = (fx * fx + fy * fy) <= 0.94F * 0.94F;
                    }
                    if (hit) ++covered;
                }
            }
            alpha[py * texSize + px] = (unsigned char)((covered * 255) / (ss * ss));
        }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, texSize, texSize, 0, GL_ALPHA, GL_UNSIGNED_BYTE, alpha.data());
    return tex;
}

void EnsureSmoothMarkerTextures() {
    if (g_minimapCircleMask == 0 || glIsTexture(g_minimapCircleMask) == GL_FALSE)
        g_minimapCircleMask = CreateSmoothMaskTexture(false);
    if (g_minimapArrowMask == 0 || glIsTexture(g_minimapArrowMask) == GL_FALSE)
        g_minimapArrowMask = CreateSmoothMaskTexture(true);
}

void DrawSmoothCircle(GLuint tex, float cx, float cy, float radius, int r, int g, int b, int a) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4ub((GLubyte)r, (GLubyte)g, (GLubyte)b, (GLubyte)a);
    // The mask's filled circle has radius .94 inside the texture, compensate so the
    // requested screen-space radius remains exact.
    const float h = radius / 0.94F;
    glBegin(GL_QUADS);
    glTexCoord2f(0.F, 0.F); glVertex2f(cx - h, cy - h);
    glTexCoord2f(1.F, 0.F); glVertex2f(cx + h, cy - h);
    glTexCoord2f(1.F, 1.F); glVertex2f(cx + h, cy + h);
    glTexCoord2f(0.F, 1.F); glVertex2f(cx - h, cy + h);
    glEnd();
}

void DrawSmoothArrow(GLuint tex, float cx, float cy, float dirX, float dirY,
                     float halfSize, float centerDist, int r, int g, int b, int a) {
    const float sideX = -dirY;
    const float sideY = dirX;
    const float ox = cx + dirX * centerDist;
    const float oy = cy + dirY * centerDist;

    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4ub((GLubyte)r, (GLubyte)g, (GLubyte)b, (GLubyte)a);
    glBegin(GL_QUADS);
    glTexCoord2f(0.F, 0.F); glVertex2f(ox - dirX * halfSize - sideX * halfSize, oy - dirY * halfSize - sideY * halfSize);
    glTexCoord2f(1.F, 0.F); glVertex2f(ox + dirX * halfSize - sideX * halfSize, oy + dirY * halfSize - sideY * halfSize);
    glTexCoord2f(1.F, 1.F); glVertex2f(ox + dirX * halfSize + sideX * halfSize, oy + dirY * halfSize + sideY * halfSize);
    glTexCoord2f(0.F, 1.F); glVertex2f(ox - dirX * halfSize + sideX * halfSize, oy - dirY * halfSize + sideY * halfSize);
    glEnd();
}

void DrawPlayerMarker(int cx, int cy, int number, int r, int g, int b, bool followed, bool bombCarrier, float yaw, bool overviewRotated) {
    const float radius = followed ? 9.F : 7.F;
    const float borderWidth = followed ? 3.0F : 1.0F;
    if (bombCarrier) { r = 242; g = 146; b = 32; }

    EnsureSmoothMarkerTextures();

    const float yawRad = yaw * (3.14159265358979323846F / 180.F);
    float dirX = 0.F, dirY = 0.F;
    if (overviewRotated) {
        dirX = cosf(yawRad);
        dirY = -sinf(yawRad);
    } else {
        dirX = -sinf(yawRad);
        dirY = -cosf(yawRad);
    }

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    GLint oldTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_ALPHA_TEST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    const float outerRadius = radius + borderWidth;

    // v4.88: pure triangular pointer, with its inner base hidden by the disk. For normal players,
    // draw a slightly larger white arrow first, then the team-coloured arrow on top.
    // The disk is drawn last and hides the rear corners, leaving one continuous white
    // outline around circle + pointer like the broadcast reference.
    if (followed) {
        const float arrowHalfSize = 9.2F;
        const float arrowCenterDist = outerRadius + 3.8F;
        DrawSmoothArrow(g_minimapArrowMask, (float)cx, (float)cy, dirX, dirY,
                        arrowHalfSize, arrowCenterDist, 255, 255, 255, 255);
    } else {
        const float outerArrowHalfSize = 7.2F;
        const float innerArrowHalfSize = 6.25F;
        const float arrowCenterDist = outerRadius + 3.0F;

        // White antialiased outline around the visible pointer.
        DrawSmoothArrow(g_minimapArrowMask, (float)cx, (float)cy, dirX, dirY,
                        outerArrowHalfSize, arrowCenterDist, 255, 255, 255, 255);
        // Team/C4 colour inset, leaving a fine white edge on both diagonals and tip.
        DrawSmoothArrow(g_minimapArrowMask, (float)cx, (float)cy, dirX, dirY,
                        innerArrowHalfSize, arrowCenterDist - 0.15F, r, g, b, 255);
    }

    // Smooth antialiased white ring, then the team/C4 disk. Drawing these last fuses
    // the arrow into the circumference and masks its rear corners completely.
    DrawSmoothCircle(g_minimapCircleMask, (float)cx, (float)cy, outerRadius, 255, 255, 255, 255);
    DrawSmoothCircle(g_minimapCircleMask, (float)cx, (float)cy, radius, r, g, b, 250);

    glBindTexture(GL_TEXTURE_2D, (GLuint)oldTexture);
    glPopAttrib();

    // Keep the established spectator hotkey numbering. The surrounding marker is now
    // filtered/antialiased; text rendering can be upgraded separately if the engine's
    // console glyphs remain visibly coarse at this very small size.
    char text[4];
    _snprintf(text, sizeof(text), "%d", number);
    text[sizeof(text) - 1] = '\0';

    int w = 0, h = 0;
    ENGINE.pfnDrawConsoleStringLen(text, &w, &h);
    const int tx = cx - w / 2;
    const int ty = cy - h / 2;
    ENGINE.pfnDrawSetTextColor(1.F, 1.F, 1.F);
    ENGINE.pfnDrawConsoleString(tx, ty, text);
}

void DrawGrenadeMarker(int cx, int cy, int type, double age) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (type == GRENADE_VIS_SMOKE) {
        // Compact cloud: three overlapping lobes + a flat lower body.
        glColor4ub(165, 168, 170, 235);
        const float lobes[3][3] = {
            {-5.0F,  0.0F, 4.5F},
            { 0.0F, -2.5F, 6.0F},
            { 5.0F,  0.0F, 4.5F}
        };
        for (int l = 0; l < 3; ++l) {
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx + lobes[l][0], cy + lobes[l][1]);
            for (int i = 0; i <= 16; ++i) {
                const float a = (2.F * 3.14159265358979323846F * i) / 16.F;
                glVertex2f(cx + lobes[l][0] + cosf(a) * lobes[l][2],
                           cy + lobes[l][1] + sinf(a) * lobes[l][2]);
            }
            glEnd();
        }
        glBegin(GL_QUADS);
        glVertex2f(cx - 7.F, cy); glVertex2f(cx + 7.F, cy);
        glVertex2f(cx + 6.F, cy + 5.F); glVertex2f(cx - 6.F, cy + 5.F);
        glEnd();
    } else {
        // Flash/HE: sharp explosion/star glyph. Same shape, event-specific colour.
        if (type == GRENADE_VIS_FLASH) glColor4ub(255, 255, 255, 245);
        else                           glColor4ub(235, 65, 55, 245);

        const float outer = 9.0F;
        const float inner = 3.6F;
        const int rays = 8;
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f((float)cx, (float)cy);
        for (int i = 0; i <= rays * 2; ++i) {
            const float a = -3.14159265358979323846F / 2.F +
                            (3.14159265358979323846F * i) / rays;
            const float radius = (i & 1) ? inner : outer;
            glVertex2f(cx + cosf(a) * radius, cy + sinf(a) * radius);
        }
        glEnd();
    }

    glPopAttrib();
}

void BuildPlayerNumbers(int numbers[33]) {
    for (int i = 0; i <= 32; ++i) numbers[i] = -1;

    // Stable spectator hotkey numbering: T = 1..5, CT = 6..9,0.
    // Include dead players while assigning numbers so a death does not renumber teammates.
    int nextT = 1;
    int nextCT = 6;
    for (int i = 1; i <= 32; ++i) {
        hud_player_info_t pi{};
        ENGINE.pfnGetPlayerInfo(i, &pi);
        if (pi.name == nullptr || pi.name[0] == '\0' || pi.spectator != 0) continue;

        const int team = GetPlayerTeam(i);
        if (team == 1 && nextT <= 5) {
            numbers[i] = nextT++;
        } else if (team == 2 && nextCT <= 10) {
            // Spectator hotkeys are 6,7,8,9,0 (10 is displayed as key 0).
            numbers[i] = (nextCT == 10) ? 0 : nextCT;
            ++nextCT;
        }
    }
}

} // namespace

void CMinimap::OnTextureDeleted(GLuint texture) {
    if (texture != 0 && texture == s_texture.texID) {
        s_texture.texID = 0;
        s_szLoadedMap[0] = '\0';
    }
}

void CMinimap::LoadForCurrentMap() {
    if (s_texture.texID != 0 && glIsTexture(s_texture.texID) == GL_FALSE) {
        s_texture.texID = 0;
        s_szLoadedMap[0] = '\0';
    }

    const char *level = ENGINE.pfnGetLevelName();
    if (level == nullptr || level[0] == '\0') return;

    const char *start = strrchr(level, '/');
    start = start ? start + 1 : level;
    char map[64];
    strncpy(map, start, sizeof(map) - 1);
    map[sizeof(map) - 1] = '\0';
    char *ext = strstr(map, ".bsp");
    if (ext) *ext = '\0';

    if (strcmp(map, s_szLoadedMap) == 0) return;
    strncpy(s_szLoadedMap, map, sizeof(s_szLoadedMap) - 1);
    s_szLoadedMap[sizeof(s_szLoadedMap) - 1] = '\0';
    s_bLoadFailed = false;

    if (s_texture.texID != 0) {
        glDeleteTextures(1, &s_texture.texID);
        s_texture.texID = 0;
    }

    char txt[MAX_PATH], bmp[MAX_PATH];
    _snprintf(txt, sizeof(txt), "overviews/%s.txt", map);
    _snprintf(bmp, sizeof(bmp), "overviews/%s.bmp", map);
    txt[sizeof(txt) - 1] = '\0';
    bmp[sizeof(bmp) - 1] = '\0';

    // The process working directory is not guaranteed to be cstrike. Try the
    // traditional relative path first, then resolve against GoldSrc's actual
    // game directory (normally .../Half-Life/cstrike).
    if (!CHelpers::bFileExists(txt) || !CHelpers::bFileExists(bmp)) {
        const char *gameDir = ENGINE.pfnGetGameDirectory ? ENGINE.pfnGetGameDirectory() : nullptr;
        if (gameDir != nullptr && gameDir[0] != '\0') {
            _snprintf(txt, sizeof(txt), "%s/overviews/%s.txt", gameDir, map);
            _snprintf(bmp, sizeof(bmp), "%s/overviews/%s.bmp", gameDir, map);
            txt[sizeof(txt) - 1] = '\0';
            bmp[sizeof(bmp) - 1] = '\0';
        }
    }

    bool txtExists = CHelpers::bFileExists(txt);
    bool bmpExists = CHelpers::bFileExists(bmp);

    // Fast-downloaded/custom maps are stored by Steam under the sibling
    // cstrike_downloads directory. If the normal game overview is absent,
    // try that directory before giving up.
    if (!txtExists || !bmpExists) {
        const char *gameDir = ENGINE.pfnGetGameDirectory ? ENGINE.pfnGetGameDirectory() : nullptr;
        if (gameDir != nullptr && gameDir[0] != '\0') {
            char downloadsDir[MAX_PATH];
            strncpy(downloadsDir, gameDir, sizeof(downloadsDir) - 1);
            downloadsDir[sizeof(downloadsDir) - 1] = '\0';

            char *lastSlash = strrchr(downloadsDir, '/');
            char *lastBackslash = strrchr(downloadsDir, '\\');
            char *separator = lastSlash;
            if (lastBackslash != nullptr && (separator == nullptr || lastBackslash > separator))
                separator = lastBackslash;

            char *gameFolder = separator != nullptr ? separator + 1 : downloadsDir;
            if (_stricmp(gameFolder, "cstrike") == 0) {
                strcpy(gameFolder, "cstrike_downloads");
            } else {
                // pfnGetGameDirectory normally ends in cstrike. Keep a useful
                // fallback for unusual launch layouts where it returns another value.
                _snprintf(downloadsDir, sizeof(downloadsDir), "cstrike_downloads");
                downloadsDir[sizeof(downloadsDir) - 1] = '\0';
            }

            _snprintf(txt, sizeof(txt), "%s/overviews/%s.txt", downloadsDir, map);
            _snprintf(bmp, sizeof(bmp), "%s/overviews/%s.bmp", downloadsDir, map);
            txt[sizeof(txt) - 1] = '\0';
            bmp[sizeof(bmp) - 1] = '\0';

            txtExists = CHelpers::bFileExists(txt);
            bmpExists = CHelpers::bFileExists(bmp);
        }
    }

    if (!txtExists || !bmpExists) {
        s_bLoadFailed = true;
        CONPRINTF("Minimap: overview missing for %s (txt=%d bmp=%d; checked cstrike + cstrike_downloads)\r\n",
                  map, txtExists ? 1 : 0, bmpExists ? 1 : 0);
        return;
    }

    if (!ParseOverviewTxt(txt, &s_fZoom, &s_fOriginX, &s_fOriginY, &s_bRotated) || s_fZoom <= 0.F) {
        s_bLoadFailed = true;
        CONPRINTF("Minimap: failed to parse %s\r\n", txt);
        return;
    }

    LOAD_TEXTURE(s_texture, bmp);
    if (s_texture.texID == 0) {
        s_bLoadFailed = true;
        CONPRINTF("Minimap: failed to load %s\r\n", bmp);
        return;
    }

    CONPRINTF("Minimap: loaded %s (%dx%d) zoom=%.2f origin=%.0f,%.0f rotated=%d\r\n",
              map, s_texture.width, s_texture.height, s_fZoom, s_fOriginX, s_fOriginY, s_bRotated ? 1 : 0);
}

void CMinimap::Draw() {
    LoadForCurrentMap();
    if (s_bLoadFailed || s_texture.texID == 0) return;

    int size = static_cast<int>(CConVars::getConVarFloat("minimap_size"));
    const int px = g_Positions.minimap_pos.x;
    const int py = g_Positions.minimap_pos.y;
    if (size <= 32) return;

    // Never let the minimap run into the top scoreboard. 720_broadcast.cfg
    // starts the top bar at x=350, so with x=10 this naturally caps at 330px.
    // This also keeps custom layouts safe if the top bar is moved.
    const int topbarX = g_Positions.topbar_pos.x;
    if (topbarX > px + 64) {
        const int maxWidth = topbarX - px - 10;
        if (size > maxWidth) size = maxWidth;
    }

    // Preserve the overview's native 4:3 aspect ratio instead of stretching it square.
    const int mapW = size;
    const int mapH = static_cast<int>(size * (static_cast<float>(s_texture.height) / s_texture.width));

    // Dark frame behind the overview.
    ENGINE.pfnFillRGBA(px - 3, py - 3, mapW + 6, mapH + 6, 12, 14, 16, 190);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s_texture.texID);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.F, 1.F, 1.F, 0.92F);
    glBegin(GL_QUADS);
    glTexCoord2f(0.F, 0.F); glVertex2f((float) px,        (float) py);
    glTexCoord2f(1.F, 0.F); glVertex2f((float) px+mapW,   (float) py);
    glTexCoord2f(1.F, 1.F); glVertex2f((float) px+mapW,   (float) py+mapH);
    glTexCoord2f(0.F, 1.F); glVertex2f((float) px,        (float) py+mapH);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glPopAttrib();

    // Series/map context. The current map name is read directly from the engine,
    // so loser-picks-next-map requires no preconfigured map list.
    const int rowH = 16;
    int infoY = py + mapH + 5;
    char line[160];

    // Completed maps stay visible with their final logical Team1-Team2 score.
    if (g_iBestOf > 1) {
        for (int m = 0; m < g_iBroadcastMapHistoryCount; ++m) {
            const broadcast_map_result_s &r = g_BroadcastMapHistory[m];
            _snprintf(line, sizeof(line), "MAP %d  %s  %d-%d", m + 1,
                      r.mapName[0] ? r.mapName : "unknown", r.team1Score, r.team2Score);
            line[sizeof(line) - 1] = '\0';
            ENGINE.pfnFillRGBA(px - 3, infoY - 1, mapW + 6, rowH, 12, 14, 16, 185);
            ENGINE.pfnDrawSetTextColor(0.72F, 0.72F, 0.72F);
            ENGINE.pfnDrawConsoleString(px + 4, infoY, line);
            infoY += rowH;
        }
    }

    const bool currentAlreadyRecorded = (g_iBroadcastMapHistoryCount > 0 &&
        _stricmp(g_BroadcastMapHistory[g_iBroadcastMapHistoryCount - 1].mapName, s_szLoadedMap) == 0);
    if (!currentAlreadyRecorded) {
        if (g_iBestOf > 1)
            _snprintf(line, sizeof(line), "MAP %d  %s  LIVE", g_iBroadcastMapHistoryCount + 1, s_szLoadedMap);
        else
            _snprintf(line, sizeof(line), "%s", s_szLoadedMap);
        line[sizeof(line) - 1] = '\0';
        ENGINE.pfnFillRGBA(px - 3, infoY - 1, mapW + 6, rowH + 1, 12, 14, 16, 220);
        ENGINE.pfnDrawSetTextColor(1.F, 1.F, 1.F);
        ENGINE.pfnDrawConsoleString(px + 4, infoY, line);
    }

    // Grenade detonation markers. Draw before players so player markers remain on top.
    GrenadeVisualEvent grenadeEvents[24];
    const double grenadeNow = ENGINE.GetClientTime ? ENGINE.GetClientTime() : 0.0;
    const int grenadeCount = GrenadeProbeGetVisualEvents(grenadeEvents, 24, grenadeNow);
    for (int i = 0; i < grenadeCount; ++i) {
        const GrenadeVisualEvent &e = grenadeEvents[i];
        float u = 0.F, v = 0.F;
        if (!WorldToOverview(e.origin[0], e.origin[1], s_fZoom, s_fOriginX, s_fOriginY, s_bRotated, &u, &v)) continue;
        const int gx = px + static_cast<int>(u * mapW);
        const int gy = py + static_cast<int>(v * mapH);
        const double age = grenadeNow > e.startTime ? grenadeNow - e.startTime : 0.0;
        DrawGrenadeMarker(gx, gy, e.type, age);
    }

    // HLTV/demo: use client.dll's authoritative spectator target.
    int followedSlot = 0;
    if (g_hClient != nullptr) {
        const DWORD clientBase = reinterpret_cast<DWORD>(g_hClient);
        followedSlot = *reinterpret_cast<int *>(clientBase + 0x1012AC);
    }

    int playerNumbers[33];
    BuildPlayerNumbers(playerNumbers);

    for (int i = 1; i <= 32; ++i) {
        cl_entity_s *ent = ENGINE.GetEntityByIndex(i);
        if (!CHelpers::bIsValidEnt(ent)) continue;
        if (CHelpers::iGetPlayerHP(i) <= 0) continue;

        hud_player_info_t pi{};
        ENGINE.pfnGetPlayerInfo(i, &pi);
        if (pi.name == nullptr || pi.name[0] == '\0' || pi.spectator != 0) continue;

        const int team = GetPlayerTeam(i);
        if (team != 1 && team != 2) continue;

        float u = 0.F, v = 0.F;
        if (!WorldToOverview(ent->origin[0], ent->origin[1], s_fZoom, s_fOriginX, s_fOriginY, s_bRotated, &u, &v)) continue;

        const int x = px + static_cast<int>(u * mapW);
        const int y = py + static_cast<int>(v * mapH);
        const bool followed = (i == followedSlot);

        const int r = team == 1 ? 225 : 70;
        const int g = team == 1 ? 75  : 145;
        const int b = team == 1 ? 60  : 235;

        const int number = playerNumbers[i];
        if (number < 0) continue;

        const bool bombCarrier = (team == 1 && i == g_iBombCarrier);
        DrawPlayerMarker(x, y, number, r, g, b, followed, bombCarrier, ent->angles[1], s_bRotated);
    }
}
