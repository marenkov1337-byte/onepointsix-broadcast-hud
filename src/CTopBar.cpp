#include "stdafx.h"

CTopBar::CTopBar() = default;

namespace {
    CFont g_fontBroadcastTimer;
    CFont g_fontBroadcastScore;
    bool g_bBroadcastFontsReady = false;

    void EnsureBroadcastFonts() {
        if (g_bBroadcastFontsReady) return;

        g_fontBroadcastTimer.SetFont((char *) "Verdana");
        g_fontBroadcastTimer.SetSize(23);
        g_fontBroadcastTimer.SetWeight(700);
        g_fontBroadcastTimer.InitText();

        g_fontBroadcastScore.SetFont((char *) "Verdana");
        g_fontBroadcastScore.SetSize(34);
        g_fontBroadcastScore.SetWeight(700);
        g_fontBroadcastScore.InitText();

        g_bBroadcastFontsReady = true;
    }

    int ClampMapWins(int value, int stars) {
        if (value < 0) return 0;
        if (value > stars) return stars;
        return value;
    }

    void DrawTinyStar(int cx, int cy, bool won) {
        // Compact 9x9 five-point star, drawn explicitly for GoldSrc HUD rendering.
        const int c = won ? 255 : 145;
        const int a = won ? 255 : 225;
        fillrgba(cx,     cy - 4, 1, 2, c,c,c,a);
        fillrgba(cx - 1, cy - 2, 3, 2, c,c,c,a);
        fillrgba(cx - 4, cy,     9, 1, c,c,c,a);
        fillrgba(cx - 3, cy + 1, 7, 1, c,c,c,a);
        fillrgba(cx - 2, cy + 2, 5, 1, c,c,c,a);
        fillrgba(cx - 3, cy + 3, 2, 2, c,c,c,a);
        fillrgba(cx + 2, cy + 3, 2, 2, c,c,c,a);
    }

    void DrawMapStars(int x, int topY, int wins, int stars) {
        wins = ClampMapWins(wins, stars);
        for (int i = 0; i < stars; ++i)
            DrawTinyStar(x, topY + i * 11, wins >= (i + 1));
    }

    void CountAlivePlayers(int &tAlive, int &ctAlive) {
        tAlive = 0;
        ctAlive = 0;
        for (int i = 1; i <= 32; ++i) {
            cl_entity_s *ent = ENGINE.GetEntityByIndex(i);
            if (!CHelpers::bIsValidEnt(ent)) continue;
            if (CHelpers::iGetPlayerHP(i) <= 0) continue;

            char *model = CHelpers::szGetPlayerModel(i);
            int team = model ? CHelpers::iGetTeam(model) : 0;
            if (team == 0) {
                const int tid = g_ScoreboardData[i].teamId;
                if (tid == 1) team = 1;
                else if (tid == 2 || tid == 3) team = 2;
            }
            if (team == 1) ++tAlive;
            else if (team == 2) ++ctAlive;
        }
    }

    void DrawRoundStatus() {
        const int x = g_Positions.topbar_pos.x;
        const int y = g_Positions.topbar_pos.y;
        const int h = (int) CConVars::getConVarFloat("topbar_height");
        const int centerX = x + 290;

        int tAlive = 0, ctAlive = 0;
        CountAlivePlayers(tAlive, ctAlive);

        // Round number: small and centered below the timer, but still inside the top bar.
        const int roundNumber = g_HUD_Vars.iTeamScores[0] + g_HUD_Vars.iTeamScores[1] + 1;
        char round[32];
        sprintf(round, "Round %d", roundNumber);
        g_fontHealthBar.Print(centerX, y + h - 4, 205,205,205,255,
                              FL_CENTER_X | FL_BACKDROP, 0, round);

        // Alive status below the top bar, matching the scoreboard orientation: T left, CT right.
        const int statusY = y + h;
        fillrgba(centerX - 38, statusY, 76, 16, 15,18,22,225);

        char ct[8], tt[8];
        sprintf(ct, "%d", ctAlive);
        sprintf(tt, "%d", tAlive);
        g_fontHealthBar.Print(centerX - 22, statusY + 12, 225,75,60,255,
                              FL_CENTER_X | FL_BACKDROP, 0, tt);
        g_fontHealthBar.Print(centerX, statusY + 12, 215,215,215,255,
                              FL_CENTER_X | FL_BACKDROP, 0, "vs");
        g_fontHealthBar.Print(centerX + 22, statusY + 12, 80,150,235,255,
                              FL_CENTER_X | FL_BACKDROP, 0, ct);
    }
}

void CTopBar::Draw() {
    EnsureBroadcastFonts();
    drawBackground();
    drawRoundTimer(g_HUD_Vars.iRoundTime);
    drawTeam1(g_HUD_Vars.szClanNames[0]);
    drawTeam2(g_HUD_Vars.szClanNames[1]);
    drawTeam1Score(g_HUD_Vars.iTeamScores[0], CConVars::getConVarFloat("team_1_score"));
    drawTeam2Score(g_HUD_Vars.iTeamScores[1], CConVars::getConVarFloat("team_2_score"));

    if (g_iBestOf == 3 || g_iBestOf == 5) {
        const int y = g_Positions.topbar_pos.y;
        const int stars = (g_iBestOf + 1) / 2;
        const int leftLogical = g_bBroadcastSidesSwapped ? 1 : 0;
        const int rightLogical = g_bBroadcastSidesSwapped ? 0 : 1;
        DrawMapStars(g_Positions.topbar_pos.x + 195, y + 11, g_iMapWins[leftLogical], stars);
        DrawMapStars(g_Positions.topbar_pos.x + 385, y + 11, g_iMapWins[rightLogical], stars);
    }

    DrawRoundStatus();
}

void CTopBar::drawRoundTimer(int time) {
    if (CConVars::getConVarFloat("enable_roundtimer") != 1) return;

    char szRoundTime[32];
    sprintf(szRoundTime, "%d:%02d", time / 60, time % 60);

    const int centerX = g_Positions.topbar_pos.x + 290;
    const int timerY = g_Positions.roundTimer_pos.y;

    // Timer is text-only: no clock/C4 icon. Bomb-planted state is still shown by red timer text.
    if (g_HUD_Vars.bBombPlanted) {
        g_fontBroadcastTimer.Print(centerX, timerY, 168,24,4,255,
                               FL_CENTER_X | FL_BACKDROP, 0, szRoundTime);
    } else {
        g_fontBroadcastTimer.Print(centerX, timerY, 255,255,255,255,
                               FL_CENTER_X | FL_BACKDROP, 0, szRoundTime);
    }
}

void CTopBar::drawTeam1(char *szTeam) {
    if (szTeam != nullptr)
        g_fontTeamNames.Print(g_Positions.team_1_name_pos.x, g_Positions.team_1_name_pos.y, 255,255,255,255,
                              FL_CENTER_X | FL_BACKDROP, 0, szTeam);
}
void CTopBar::drawTeam2(char *szTeam) {
    if (szTeam != nullptr)
        g_fontTeamNames.Print(g_Positions.team_2_name_pos.x, g_Positions.team_2_name_pos.y, 255,255,255,255,
                              FL_CENTER_X | FL_BACKDROP, 0, szTeam);
}

void CTopBar::drawTeam1Score(int teamScore, int clanScore) {
    char szTeamScore[16], szClanScore[16];
    sprintf(szClanScore, "%d", clanScore);
    sprintf(szTeamScore, "%d", teamScore);
    // Large score immediately beside the timer. One score only: use the largest HUD score font.
    const int scoreX = g_Positions.topbar_pos.x + 232;
    const int scoreY = g_Positions.roundTimer_pos.y + 4;
    g_fontBroadcastScore.Print(scoreX, scoreY, 255,255,255,255,
                          FL_CENTER_X | FL_BACKDROP, 0, szTeamScore);
}
void CTopBar::drawTeam2Score(int teamScore, int clanScore) {
    char szTeamScore[16], szClanScore[16];
    sprintf(szClanScore, "%d", clanScore);
    sprintf(szTeamScore, "%d", teamScore);
    // Symmetric CT score, kept tight to the timer.
    const int scoreX = g_Positions.topbar_pos.x + 348;
    const int scoreY = g_Positions.roundTimer_pos.y + 4;
    g_fontBroadcastScore.Print(scoreX, scoreY, 255,255,255,255,
                          FL_CENTER_X | FL_BACKDROP, 0, szTeamScore);
}

void CTopBar::drawBackground() {
    const int x = g_Positions.topbar_pos.x;
    const int y = g_Positions.topbar_pos.y;
    const int h = (int) CConVars::getConVarFloat("topbar_height");

    // Team-name blocks.
    fillrgba(x,       y, 180, h, 154, 55, 43, 225); // T
    fillrgba(x + 400, y, 180, h, 48, 88, 145, 225); // CT

    // One continuous dark block for stars + scores + timer.
    // No separators between score and timer: they visually belong together.
    fillrgba(x + 180, y, 220, h, 24, 27, 31, 240);

    // Strong team-coloured separators between team names and the central score area.
    // They are intentionally wider than the old internal dividers.
    fillrgba(x + 177, y, 7, h, 235, 72, 55, 255);   // T: brighter + wider
    fillrgba(x + 396, y, 7, h, 70, 145, 235, 255);  // CT: brighter + wider
}
