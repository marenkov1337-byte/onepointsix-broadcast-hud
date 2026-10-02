#include "stdafx.h"

CTopBar *topbar = new CTopBar();
CHealthBars *healthbars = new CHealthBars();

void ReinitializeFonts() {
    g_fontGeneral.InitText();
    g_fontHealthBar.InitText();
    g_fontRoundTimer.InitText();
    g_fontTeamScore.InitText();
    g_fontTeamNames.InitText();
    g_fontClanScore.InitText();
    g_bReinitializeFonts = false;

    CONPRINTF("Fonts initialized \r\n");
}

bool g_bReinitializeFonts = true;
int g_iDesiredSpecTarget = 0; // kept for cmd_SpecFree compatibility; camera targeting now goes via FollowPlayerIndex()
int g_iLastDeathVictim = 0;
int g_iLastDeathKiller = 0;
double g_flLastDeathTime = -1.0;

static void AutoAdvanceDeadSpectatorTarget() {
    if (g_hClient == nullptr) return;

    const DWORD clientBase = reinterpret_cast<DWORD>(g_hClient);
    const int followed = *reinterpret_cast<int *>(clientBase + 0x1012AC);
    if (followed < 1 || followed > 32) return;

    cl_entity_s *followedEnt = ENGINE.GetEntityByIndex(followed);
    if (followedEnt == nullptr || followedEnt->player == 0) return;

    // Nothing to do while the followed player is alive.
    if (CHelpers::iGetPlayerHP(followed) > 0) return;

    // Keep the death on screen briefly instead of switching on the very first
    // frame where HP reaches zero. If DeathMsg tells us who killed the player,
    // prefer that killer once the 0.5 s hold has elapsed.
    if (g_iLastDeathVictim == followed && g_flLastDeathTime >= 0.0) {
        const double now = ENGINE.GetClientTime ? ENGINE.GetClientTime() : 0.0;
        if (now - g_flLastDeathTime < 0.5) return;

        const int killer = g_iLastDeathKiller;
        if (killer >= 1 && killer <= 32 && killer != followed) {
            cl_entity_s *killerEnt = ENGINE.GetEntityByIndex(killer);
            if (killerEnt != nullptr && killerEnt->player != 0 && CHelpers::iGetPlayerHP(killer) > 0) {
                FollowPlayerIndex(killer);
                g_iLastDeathVictim = 0;
                g_iLastDeathKiller = 0;
                g_flLastDeathTime = -1.0;
                return;
            }
        }
    }

    const int followedTeam = CHelpers::iGetTeam(CHelpers::szGetPlayerModel(followed));

    // If the killer is unavailable (world/self kill/dead killer), prefer the
    // next living teammate. If that team is wiped, fall back to anyone alive.
    for (int pass = 0; pass < 2; ++pass) {
        for (int step = 1; step <= 32; ++step) {
            const int candidate = ((followed - 1 + step) % 32) + 1;
            cl_entity_s *ent = ENGINE.GetEntityByIndex(candidate);
            if (ent == nullptr || ent->player == 0) continue;
            if (CHelpers::iGetPlayerHP(candidate) <= 0) continue;

            char *model = CHelpers::szGetPlayerModel(candidate);
            const int team = CHelpers::iGetTeam(model);
            if (team != 1 && team != 2) continue;
            if (pass == 0 && followedTeam != 0 && team != followedTeam) continue;

            FollowPlayerIndex(candidate);
            g_iLastDeathVictim = 0;
            g_iLastDeathKiller = 0;
            g_flLastDeathTime = -1.0;
            return;
        }
    }
}
auto Hook_HUD_Redraw(float flArg, int iArg) -> int {
    AutoAdvanceDeadSpectatorTarget();

    // Let the native GoldSrc/CS HUD render first (including sniper scope).
    // Our broadcast HUD is then composited on top so the scope can no longer
    // cover the minimap, top bar or player cards.
    const int nativeResult = CLIENT.HUD_Redraw(flArg, iArg);

    if (g_Sprites != nullptr && g_iSpriteCount > 0 && g_Sprites[0].hspr == 0) {
        InitializeSprites();
    }

    if (g_bReinitializeFonts) {
        ReinitializeFonts();
    }

    if (CConVars::getConVarFloat("draw_topbar") == 1.F) {
        CTopBar::Draw();
    }

    if (CConVars::getConVarFloat("draw_healthbars") == 1.F) {
        CHealthBars::Draw();
    }


    if (CConVars::getConVarFloat("draw_minimap") == 1.F) {
        CMinimap::Draw();
    }

    return nativeResult;
}
