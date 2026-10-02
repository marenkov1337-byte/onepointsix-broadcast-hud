#include "stdafx.h"

// v4.18: three visually distinct card zones; upper zone is the HP meter.
const BarColors COLOR_T = {154, 55, 43, 245};
const BarColors COLOR_CT = {48, 88, 145, 245};
const BarColors COLOR_LOST_HP = {68, 72, 78, 165};
const BarColors COLOR_DEAD = {13, 13, 13, 225};
const BarColors COLOR_ACTION = {255, 166, 45, 255};

const int CARD_PAD = 6;
const int NAME_HEIGHT = 22;
const int STATS_HEIGHT = 18;

// Small per-round kill badge font. Kept separate from HP/KD fonts so the
// badge stays intentionally discreet.
static CFont g_fontRoundKillBadge((char *)"Verdana", 12, 700);
static bool g_roundKillBadgeFontReady = false;

static void DrawRoundKillBadge(int x, int y, int width, int height, int hp, int team, int roundKills) {
    if (roundKills <= 0) return;

    const int badgeW = 16;
    const int badgeH = 16;
    const int badgeGap = 5;
    const int badgeX = x + width - badgeW;

    // Alive: float above the card with a clear gap from the observed-player
    // highlight (which extends a few pixels outside the card).
    // Dead: the upper HP/weapon area disappears, so keep the round-kill badge
    // attached just above the dead player's name strip instead.
    const int nameY = y + height - STATS_HEIGHT - NAME_HEIGHT;
    const int badgeY = (hp > 0)
        ? (y - badgeH - badgeGap)
        : (nameY - badgeH - 2);

    if (hp > 0) {
        const BarColors teamColor = (team == 1) ? COLOR_T : COLOR_CT;
        fillrgba(badgeX, badgeY, badgeW, badgeH, teamColor.r, teamColor.g, teamColor.b, 245);
    } else {
        // Keep the round-kill count visible after death, but clearly muted.
        fillrgba(badgeX, badgeY, badgeW, badgeH, 68, 72, 78, 225);
    }

    if (!g_roundKillBadgeFontReady) {
        g_fontRoundKillBadge.InitText();
        g_roundKillBadgeFontReady = true;
    }

    char kills[8];
    sprintf(kills, "%d", roundKills);
    g_fontRoundKillBadge.Print(badgeX + badgeW / 2, badgeY + badgeH / 2 - 1,
                              255, 255, 255, 255, FL_CENTER_X | FL_CENTER_Y, 0, kills);
}

static GLuint CreateHudIconTexture(const unsigned char *alpha, int w, int h) {
    std::vector<unsigned char> rgba((size_t)w * (size_t)h * 4, 255);
    for (int i = 0; i < w * h; ++i) rgba[(size_t)i * 4 + 3] = alpha[i];
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

static void DrawEmbeddedHudIcon(GLuint texture, int x, int y, int w, int h, int shade) {
    if (texture == 0) return;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4ub((UCHAR)shade, (UCHAR)shade, (UCHAR)shade, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f((GLfloat)x,(GLfloat)y);
    glTexCoord2f(1,0); glVertex2f((GLfloat)(x+w),(GLfloat)y);
    glTexCoord2f(1,1); glVertex2f((GLfloat)(x+w),(GLfloat)(y+h));
    glTexCoord2f(0,1); glVertex2f((GLfloat)x,(GLfloat)(y+h));
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glPopAttrib();
}

static void DrawKillIcon(int x, int y, int shade) {
    static const unsigned char alpha[24 * 24] = {
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1, 16, 18,  4,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  1,  0, 15,217,235, 59,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  1,  0, 17,241,255, 66,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  2,  4, 17,235,254, 64,  3,  5,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  3,  0,  0, 11,237,255, 62,  0,  0,  3,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  2,  0, 22,125,199,250,251,215,151, 44,  0,  1,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  2,  0, 71,227,255,255,255,255,255,255,249,112,  1,  3,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  3,  0, 68,255,255,184, 79, 67, 67, 68,154,255,255,127,  0,  3,  0,  0,  0,  0,
          0,  1,  1,  2,  2, 19,230,255,121,  0,  0,  0,  0,  0,  0, 73,245,254, 62,  1,  4,  1,  1,  0,
          0,  0,  0,  0,  0,124,255,186,  0,  4,  0,  6, 14,  0,  7,  0,128,255,187,  0,  0,  0,  0,  0,
          1, 15, 17, 18, 19,205,254, 73,  1,  1, 52,212,230, 97,  0,  2, 21,244,249, 41, 16, 18, 16,  4,
         17,218,236,235,238,252,255, 64,  0, 11,218,255,255,253, 43,  0, 18,237,254,241,236,235,236, 65,
         19,250,255,255,255,255,255, 67,  0, 16,239,254,249,255, 58,  0, 21,238,255,255,255,255,255, 74,
          5, 59, 65, 65, 68,220,254, 62,  1,  0, 96,250,255,149,  2,  1, 14,241,252, 89, 64, 65, 64, 18,
          0,  0,  0,  0,  0,150,255,153,  0,  7,  0, 43, 58,  0,  7,  0, 91,253,211,  0,  0,  0,  0,  0,
          0,  3,  3,  5,  3, 40,250,250, 73,  0,  0,  0,  0,  0,  0, 29,226,255, 93,  2,  6,  3,  3,  1,
          0,  0,  0,  0,  3,  0,111,255,247,128, 29, 20, 20, 20, 95,225,255,171,  3,  3,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  2,  0,127,255,255,248,238,239,242,255,255,168, 18,  1,  1,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  1,  0, 67,183,246,255,255,252,207, 96,  0,  0,  1,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  3,  0,  0, 40,239,253, 88,  2,  0,  1,  1,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  4,  1, 10,235,255, 58,  0,  6,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  1,  0, 17,235,254, 65,  1,  3,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  1,  0, 16,242,255, 66,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4, 65, 70, 18,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0
    };
    static GLuint texture = 0;
    if (texture == 0) texture = CreateHudIconTexture(alpha, 24, 24);
    DrawEmbeddedHudIcon(texture, x, y, 13, 13, shade);
}

static void DrawDeathIcon(int x, int y, int shade) {
    static const unsigned char alpha[24 * 24] = {
          0,  0,  0,  0,  0,  0,  0,  0,  2,  4,  1,  0,  0,  0,  4,  3,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  3,  0,  0,  0, 16, 16,  3,  0,  0,  2,  1,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  1,  2,  0, 44,140,202,239,239,214,159, 70,  0,  0,  2,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  1,  1,  0,127,249,255,255,255,255,255,255,255,167, 18,  0,  2,  0,  0,  0,  0,
          0,  0,  0,  0,  3,  1,154,255,254,251,253,254,254,253,251,252,255,202, 19,  1,  1,  0,  0,  0,
          0,  0,  0,  3,  0,127,255,249,254,254,255,255,255,255,254,254,249,255,187,  2,  2,  0,  0,  0,
          0,  0,  2,  0, 41,250,252,252,255,255,252,255,255,253,255,255,252,250,255, 96,  0,  3,  0,  0,
          0,  0,  4,  0,141,255,250,255,252,244,255,254,255,255,252,244,255,251,254,203,  2,  2,  0,  0,
          0,  0,  1,  4,205,254,255,203, 43, 25,158,255,254,203, 43, 25,158,255,254,244, 30,  0,  2,  0,
          0,  1,  0, 17,238,255,245, 37,  0,  0,  2,214,253, 35,  0,  0,  2,213,254,255, 66,  0,  3,  0,
          0,  1,  0, 18,239,255,240, 16,  1,  6,  0,200,252, 12,  1,  6,  0,198,255,255, 68,  0,  3,  0,
          0,  0,  1,  8,218,255,255,160,  5,  0,105,255,255,159,  5,  0,106,253,254,249, 42,  0,  2,  0,
          0,  0,  3,  0,161,254,251,255,214,196,255,201,162,255,211,199,255,253,254,218,  5,  1,  0,  0,
          0,  0,  3,  0, 66,255,252,253,255,255,255, 85, 22,248,255,255,254,251,255,128,  0,  4,  0,  0,
          0,  0,  0,  3,  0,170,255,252,250,253,219, 10,  0,164,255,248,253,255,221, 15,  2,  1,  0,  0,
          0,  0,  0,  1,  1, 18,214,254,255,254, 83,  0,  0, 27,245,255,253,243, 56,  0,  2,  0,  0,  0,
          0,  0,  0,  0,  4,  0,129,255,235,248,192,174,186,187,224,248,255,193,  0,  3,  0,  0,  0,  0,
          0,  0,  0,  0,  4,  1,127,255, 16,195,255, 31,208,255, 21,186,255,182,  2,  2,  0,  0,  0,  0,
          0,  0,  0,  0,  3,  0, 75,253,  0,189,252,  0,186,252,  0,180,255,129,  0,  4,  0,  0,  0,  0,
          0,  0,  0,  0,  3,  0, 66,248,  3,188,253,  2,188,253,  3,180,253,118,  0,  4,  0,  0,  0,  0,
          0,  0,  0,  0,  1,  0, 25,236,  8,198,255,  1,199,255,  2,191,255, 69,  0,  3,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  5, 57,  2, 49, 66,  0, 49, 66,  1, 47, 66, 15,  0,  1,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  3,  0,  2,  3,  0,  2,  3,  0,  2,  3,  1,  0,  0,  0,  0,  0,  0
    };
    static GLuint texture = 0;
    if (texture == 0) texture = CreateHudIconTexture(alpha, 24, 24);
    DrawEmbeddedHudIcon(texture, x, y, 13, 13, shade);
}

static void DrawObservedHighlight(int x, int y, int width, int height) {
    // POV selection lives OUTSIDE the card so it never steals pixels from
    // the HP/name/KD content. Two-pixel solid white frame + subtle outer halo.
    fillrgba(x - 3, y - 3, width + 6, 1, 255,255,255,85);
    fillrgba(x - 3, y + height + 2, width + 6, 1, 255,255,255,85);
    fillrgba(x - 3, y - 3, 1, height + 6, 255,255,255,85);
    fillrgba(x + width + 2, y - 3, 1, height + 6, 255,255,255,85);

    fillrgba(x - 2, y - 2, width + 4, 2, 255,255,255,255);
    fillrgba(x - 2, y + height, width + 4, 2, 255,255,255,255);
    fillrgba(x - 2, y, 2, height, 255,255,255,255);
    fillrgba(x + width, y, 2, height, 255,255,255,255);
}

void CClassicHealthBar::Draw() {
    DrawBackground();

    // Per-round kills: small tab above the card's top-right corner. It remains
    // visible when the player dies, but switches to a muted grey background.
    DrawRoundKillBadge(x, y, width, height, info.hp, info.team, info.roundKills);

    // Dead players deliberately have no HP/weapon area at all.
    if (info.hp > 0) {
        BarColors activeColor = {235,235,235,255};
        bool isDoingAction = false;
        if (info.sequence == SEQUENCE_RELOAD || info.sequence == SEQUENCE_ARM_C4) {
            activeColor = COLOR_ACTION;
            isDoingAction = true;
        }
        DrawHP();
        DrawWeapon(activeColor, isDoingAction);
    }

    DrawPlayerName();

    // K/D strip remains team-coloured even for the observed player.
    const int statsY = y + height - STATS_HEIGHT;
    const int iconY = statsY + 4;
    const int shade = (info.hp > 0) ? 245 : 210;
    DrawKillIcon(x + 8, iconY, shade);
    char value[16];
    sprintf(value, "%d", info.kills);
    g_fontHealthBar.Print(x + 24, statsY + 13, shade,shade,shade,255, FL_NONE | FL_BACKDROP, 18, value);
    DrawDeathIcon(x + 59, iconY, shade);
    sprintf(value, "%d", info.deaths);
    g_fontHealthBar.Print(x + 75, statsY + 13, shade,shade,shade,255, FL_NONE | FL_BACKDROP, 18, value);

    if (info.isObserved) DrawObservedHighlight(x, y, width, height);
}

void CClassicHealthBar::DrawBackground() {
    const BarColors team = (info.team == 1) ? COLOR_T : COLOR_CT;
    const int statsY = y + height - STATS_HEIGHT;
    const int nameY = statsY - NAME_HEIGHT;

    // The name and K/D are deliberately darker than the live HP colour so the
    // three zones read as separate HUD bands at 640x480.
    const int nameR = (team.r * 66) / 100;
    const int nameG = (team.g * 66) / 100;
    const int nameB = (team.b * 66) / 100;
    const int statsR = (team.r * 48) / 100;
    const int statsG = (team.g * 48) / 100;
    const int statsB = (team.b * 48) / 100;

    if (info.hp <= 0) {
        // Dead = literally nothing above the name strip.
        fillrgba(x, nameY, width, NAME_HEIGHT, nameR, nameG, nameB, 235);
        fillrgba(x, statsY, width, STATS_HEIGHT, statsR, statsG, statsB, 235);
        fillrgba(x, statsY, width, 1, 255, 255, 255, 28);
        return;
    }

    // Upper rectangle is the HP meter. Remaining health grows upward from the
    // name strip; lost health is the dark-grey part at the top.
    const int hpAreaHeight = nameY - y;
    int hp = info.hp;
    if (hp < 0) hp = 0;
    if (hp > 100) hp = 100;
    const int aliveHeight = (hpAreaHeight * hp) / 100;
    const int lostHeight = hpAreaHeight - aliveHeight;

    if (lostHeight > 0)
        fillrgba(x, y, width, lostHeight, COLOR_LOST_HP.r, COLOR_LOST_HP.g, COLOR_LOST_HP.b, COLOR_LOST_HP.a);
    if (aliveHeight > 0)
        fillrgba(x, y + lostHeight, width, aliveHeight, team.r, team.g, team.b, 255);

    // Dedicated name strip. Only THIS strip becomes white in first person.
    if (info.isObserved)
        fillrgba(x, nameY, width, NAME_HEIGHT, 245, 245, 245, 255);
    else
        fillrgba(x, nameY, width, NAME_HEIGHT, nameR, nameG, nameB, 255);

    // Dedicated K/D strip, darker again than the name strip.
    fillrgba(x, statsY, width, STATS_HEIGHT, statsR, statsG, statsB, 255);

    // Fine separators make the zones unambiguous without adding another bar.
    fillrgba(x, nameY, width, 1, 255, 255, 255, 38);
    fillrgba(x, statsY, width, 1, 255, 255, 255, 30);
}

void CClassicHealthBar::DrawHPBar() {
    // Intentionally unused in v4.17: the whole upper card is now the HP bar.
}

void CClassicHealthBar::DrawPlayerName() {
    char buffer[64];
    StripTags(buffer, info.szName);
    const int nameY = y + height - STATS_HEIGHT - NAME_HEIGHT;
    const int shade = (info.isObserved && info.hp > 0) ? 38 : 255;

    // Use the dedicated 32px team-name font: the player name is the primary HUD information.
    // Its baseline stays inside the dedicated name strip.
    const int nameBaseline = nameY + 18;
    g_fontTeamNames.Print(x + 6, nameBaseline, shade, shade, shade, 255,
                        (info.isObserved ? FL_NONE : FL_BACKDROP),
                        width - 12, buffer);
}

void CClassicHealthBar::DrawHP() {
    if (info.hp <= 0) return;
    char hp[16];
    sprintf(hp, "%d", info.hp);
    // HP sits at the bottom-right of the live-health zone, immediately above the name strip.
    const int nameY = y + height - STATS_HEIGHT - NAME_HEIGHT;
    g_fontTeamScore.Print(x + width - CARD_PAD, nameY - 4, 255,255,255,255,
                          FL_RIGHT | FL_BACKDROP, 0, hp);
}

void CClassicHealthBar::DrawWeapon(BarColors color, bool isDoingAction) {
    if (info.hp <= 0) return;
    const int nameY = y + height - STATS_HEIGHT - NAME_HEIGHT;
    // Align the active weapon with the HP value near the bottom of the live-health zone.
    const int weaponY = nameY - 12;
    CHelpers::drawSprite(x + 26, weaponY, info.szWeapon, false, nullptr, color.r,color.g,color.b);
    const bool isHoldingC4 = (strcmp(info.szWeapon, "c4") == 0);
    if (info.kitbomb && !isHoldingC4) {
        char *icon = (info.team == 1) ? (char *)"c4" : (char *)"defuser";
        // Keep the existing GoldSrc defuser/C4 sprite. The CT defuse-kit (pliers)
        // is forced white; the T bomb icon keeps the team tint. Raise it slightly
        // so it remains clearly above the HP number.
        if (info.team == 2) {
            CHelpers::drawSprite(x + 16, nameY - 34, icon, false, nullptr, 255,255,255);
        } else {
            // Bomb ownership is match information: always render the C4 marker in white,
            // regardless of which weapon the carrier currently has selected.
            CHelpers::drawSprite(x + width - 18, nameY - 34, icon, false, nullptr, 255,255,255);
        }
    }
}
