#include "stdafx.h"

auto commands = new CCommands();
auto cvars = new CConVars();

// Lists all commands & convars that we've registered
void PrintHelp() {
    CONPRINTF("Commands:\r\n");
    commands->printHelp();
    CONPRINTF("Cvars:\r\n");
    cvars->printHelp();
}

// reload everything not accessed directly through a cvar
void cmd_HUD_Reload() {
    delete g_Positions.iArrPosCT;
    delete g_Positions.iArrPosT;

    g_Positions.iArrPosCT = new pos_s[5];
    g_Positions.iArrPosT = new pos_s[5];

    g_Positions.team_1_name_pos.x = static_cast<int>(CConVars::getConVarFloat("team_1_name_xpos"));
    g_Positions.team_1_name_pos.y = static_cast<int>(CConVars::getConVarFloat("team_1_name_ypos"));

    g_Positions.team_2_name_pos.x = static_cast<int>(CConVars::getConVarFloat("team_2_name_xpos"));
    g_Positions.team_2_name_pos.y = static_cast<int>(CConVars::getConVarFloat("team_2_name_ypos"));

    g_Positions.topbar_pos.x = static_cast<int>(CConVars::getConVarFloat("topbar_xpos"));
    g_Positions.topbar_pos.y = static_cast<int>(CConVars::getConVarFloat("topbar_ypos"));



    g_Positions.roundTimer_pos.x = static_cast<int>(CConVars::getConVarFloat("roundtimer_xpos"));
    g_Positions.roundTimer_pos.y = static_cast<int>(CConVars::getConVarFloat("roundtimer_ypos"));

    g_Positions.team_1_clanScore_pos.x = static_cast<int>(CConVars::getConVarFloat("team_1_clanscore_xpos"));
    g_Positions.team_1_clanScore_pos.y = static_cast<int>(CConVars::getConVarFloat("team_1_clanscore_ypos"));

    g_Positions.team_2_clanScore_pos.x = static_cast<int>(CConVars::getConVarFloat("team_2_clanscore_xpos"));
    g_Positions.team_2_clanScore_pos.y = static_cast<int>(CConVars::getConVarFloat("team_2_clanscore_ypos"));

    g_Positions.team_1_teamScore_pos.x = static_cast<int>(CConVars::getConVarFloat("team_1_teamscore_xpos"));
    g_Positions.team_1_teamScore_pos.y = static_cast<int>(CConVars::getConVarFloat("team_1_teamscore_ypos"));

    g_Positions.team_2_teamScore_pos.x = static_cast<int>(CConVars::getConVarFloat("team_2_teamscore_xpos"));
    g_Positions.team_2_teamScore_pos.y = static_cast<int>(CConVars::getConVarFloat("team_2_teamscore_ypos"));


    char buffer[64];
    for (int i = 0; i < 5; i++) {
        sprintf(buffer, "ct_%d_xpos", i + 1);
        g_Positions.iArrPosCT[i].x = static_cast<int>(CConVars::getConVarFloat(buffer));

        sprintf(buffer, "ct_%d_ypos", i + 1);
        g_Positions.iArrPosCT[i].y = static_cast<int>(CConVars::getConVarFloat(buffer));

        sprintf(buffer, "t_%d_xpos", i + 1);
        g_Positions.iArrPosT[i].x = static_cast<int>(CConVars::getConVarFloat(buffer));

        sprintf(buffer, "t_%d_ypos", i + 1);
        g_Positions.iArrPosT[i].y = static_cast<int>(CConVars::getConVarFloat(buffer));
    }

    g_Positions.minimap_pos.x = static_cast<int>(CConVars::getConVarFloat("minimap_xpos"));
    g_Positions.minimap_pos.y = static_cast<int>(CConVars::getConVarFloat("minimap_ypos"));

    g_HUD_Vars.szClanNames[0] = CConVars::getConVarString("team_1_name");
    g_HUD_Vars.szClanNames[1] = CConVars::getConVarString("team_2_name");

    g_fontGeneral.SetFont(CConVars::getConVarString("general_font"));
    g_fontGeneral.SetSize(static_cast<int>(CConVars::getConVarFloat("general_font_size")));

    g_fontHealthBar.SetFont(CConVars::getConVarString("healthbar_font"));
    g_fontHealthBar.SetSize(static_cast<int>(CConVars::getConVarFloat("healthbar_font_size")));

    g_fontRoundTimer.SetFont(CConVars::getConVarString("roundtimer_font"));
    g_fontRoundTimer.SetSize(static_cast<int>(CConVars::getConVarFloat("roundtimer_font_size")));

    g_fontTeamScore.SetFont(CConVars::getConVarString("teamscore_font"));
    g_fontTeamScore.SetSize(static_cast<int>(CConVars::getConVarFloat("teamscore_font_size")));

    g_fontTeamNames.SetFont(CConVars::getConVarString("teamnames_font"));
    g_fontTeamNames.SetSize(static_cast<int>(CConVars::getConVarFloat("teamnames_font_size")));

    g_fontClanScore.SetFont(CConVars::getConVarString("clanscore_font"));
    g_fontClanScore.SetSize(static_cast<int>(CConVars::getConVarFloat("clanscore_font_size")));


    ReinitializeFonts();
    InitializeSprites();

    CONPRINTF("HUD reloaded.\r\n");
}

// increment/decrement cvar, helpers for setting the scores
void cmd_Increment() {
    if (ARGC != 2) {
        CONPRINTF("Usage %s <ConVar>\r\n", ARGV(0));
        return;
    }
    ENGINE.Cvar_SetValue(ARGV(1), ENGINE.pfnGetCvarFloat(ARGV(1)) + 1);
}
void cmd_Decrement() {
    if (ARGC != 2) {
        CONPRINTF("Usage %s <ConVar>\r\n", ARGV(0));
        return;
    }
    ENGINE.Cvar_SetValue(ARGV(1), ENGINE.pfnGetCvarFloat(ARGV(1)) - 1);
}
// toggle cvar 0/1
void cmd_Toggle() {
    if (ARGC != 2) {
        CONPRINTF("Usage %s <ConVar>\r\n", ARGV(0));
        return;
    }
    ENGINE.Cvar_SetValue(ARGV(1), static_cast<float>(static_cast<int>(ENGINE.pfnGetCvarFloat(ARGV(1))) ^ 1));
}



// switch the scores around
// positions swapped through the cfg
extern int g_iRoundHalfScore[2];
extern int g_iScoreOffset[2];

// Manually carry the score over to the next half. Because the round score
// is driven by the server's own per-half TERRORIST/CT counters (which reset
// to 0 after a side switch), a plain overwrite would get clobbered by the
// very next round. Instead this sets an offset that keeps being added to the
// server's live count, so it survives for the rest of the half.
// Usage: hltv_set_round_score <team1_score> <team2_score>
void cmd_SetRoundScore() {
    if (ARGC != 3) {
        CONPRINTF("Usage: %s <team1_score> <team2_score>\r\n", ARGV(0));
        return;
    }
    g_iScoreOffset[0] = atoi(ARGV(1)) - g_iRoundHalfScore[0];
    g_iScoreOffset[1] = atoi(ARGV(2)) - g_iRoundHalfScore[1];
    g_HUD_Vars.iTeamScores[0] = g_iRoundHalfScore[0] + g_iScoreOffset[0];
    g_HUD_Vars.iTeamScores[1] = g_iRoundHalfScore[1] + g_iScoreOffset[1];
    CONPRINTF("Round score set to %d - %d\r\n", g_HUD_Vars.iTeamScores[0], g_HUD_Vars.iTeamScores[1]);
}


static char g_szMatchTeam1[128] = "Team 1";
static char g_szMatchTeam2[128] = "Team 2";

static void CopyMatchTeamName(char *dst, size_t dstSize, const char *src) {
    if (!src) src = "";
    _snprintf(dst, dstSize, "%s", src);
    dst[dstSize - 1] = '\0';
}

static int ParseBestOf(const char *value) {
    if (!value || value[0] == '\0') return 1;
    if (_stricmp(value, "bo1") == 0 || strcmp(value, "1") == 0) return 1;
    if (_stricmp(value, "bo3") == 0 || strcmp(value, "3") == 0) return 3;
    if (_stricmp(value, "bo5") == 0 || strcmp(value, "5") == 0) return 5;
    return 0;
}

static void ApplyLogicalTeamOrder() {
    if (!g_bBroadcastSidesSwapped) {
        g_HUD_Vars.szClanNames[0] = g_szMatchTeam1;
        g_HUD_Vars.szClanNames[1] = g_szMatchTeam2;
    } else {
        g_HUD_Vars.szClanNames[0] = g_szMatchTeam2;
        g_HUD_Vars.szClanNames[1] = g_szMatchTeam1;
    }

    char cmd[384];
    _snprintf(cmd, sizeof(cmd), "hltv_team_1_name \"%s\"; hltv_team_2_name \"%s\"\n",
              g_HUD_Vars.szClanNames[0], g_HUD_Vars.szClanNames[1]);
    cmd[sizeof(cmd) - 1] = '\0';
    ENGINE.pfnClientCmd(cmd);
}

// Usage: hltv_match_start "Team A" "Team B" [bo1|bo3|bo5]
// BO1 is the default. This deliberately does NOT exec a cfg.
void cmd_MatchStart() {
    if (ARGC != 3 && ARGC != 4) {
        CONPRINTF("Usage: %s \"team 1\" \"team 2\" [bo1|bo3|bo5]\r\n", ARGV(0));
        return;
    }

    const int bestOf = (ARGC == 4) ? ParseBestOf(ARGV(3)) : 1;
    if (bestOf == 0) {
        CONPRINTF("Invalid mode. Use bo1, bo3 or bo5.\r\n");
        return;
    }

    CopyMatchTeamName(g_szMatchTeam1, sizeof(g_szMatchTeam1), ARGV(1));
    CopyMatchTeamName(g_szMatchTeam2, sizeof(g_szMatchTeam2), ARGV(2));
    g_iBestOf = bestOf;
    g_iMapWins[0] = g_iMapWins[1] = 0;
    g_iBroadcastMapHistoryCount = 0;
    memset(g_BroadcastMapHistory, 0, sizeof(g_BroadcastMapHistory));
    g_bBroadcastSidesSwapped = false;
    g_iRoundHalfScore[0] = g_iRoundHalfScore[1] = 0;
    g_iScoreOffset[0] = g_iScoreOffset[1] = 0;
    g_HUD_Vars.iTeamScores[0] = g_HUD_Vars.iTeamScores[1] = 0;
    ApplyLogicalTeamOrder();

    CONPRINTF("Match started: %s vs %s (%s)\r\n", g_szMatchTeam1, g_szMatchTeam2,
              bestOf == 1 ? "BO1" : (bestOf == 3 ? "BO3" : "BO5"));
}

void cmd_Halftime() {
    const int leftTotal = g_HUD_Vars.iTeamScores[0];
    const int rightTotal = g_HUD_Vars.iTeamScores[1];

    g_bBroadcastSidesSwapped = !g_bBroadcastSidesSwapped;
    g_iRoundHalfScore[0] = g_iRoundHalfScore[1] = 0;
    g_iScoreOffset[0] = rightTotal;
    g_iScoreOffset[1] = leftTotal;
    g_HUD_Vars.iTeamScores[0] = rightTotal;
    g_HUD_Vars.iTeamScores[1] = leftTotal;
    ApplyLogicalTeamOrder();

    CONPRINTF("Halftime: %s %d - %d %s\r\n", g_HUD_Vars.szClanNames[0],
              g_HUD_Vars.iTeamScores[0], g_HUD_Vars.iTeamScores[1], g_HUD_Vars.szClanNames[1]);
}

static void GetCurrentMapName(char *out, size_t outSize) {
    out[0] = '\0';
    const char *level = ENGINE.pfnGetLevelName();
    if (!level || !level[0]) return;
    const char *slash = strrchr(level, '/');
    const char *backslash = strrchr(level, '\\');
    const char *start = slash;
    if (backslash && (!start || backslash > start)) start = backslash;
    start = start ? start + 1 : level;
    _snprintf(out, outSize, "%s", start);
    out[outSize - 1] = '\0';
    char *ext = strstr(out, ".bsp");
    if (ext) *ext = '\0';
}

// Usage: hltv_map_winner <1|2>. Team number is the permanent series identity.
void cmd_MapWinner() {
    if (ARGC != 2) {
        CONPRINTF("Usage: %s <1|2>\r\n", ARGV(0));
        return;
    }
    const int winner = atoi(ARGV(1));
    if (winner != 1 && winner != 2) {
        CONPRINTF("Winner must be 1 or 2.\r\n");
        return;
    }
    const int needed = (g_iBestOf + 1) / 2;
    if (g_iMapWins[winner - 1] >= needed) {
        CONPRINTF("Team %d already has the maximum map wins for this series.\r\n", winner);
        return;
    }

    int logicalScore1 = g_bBroadcastSidesSwapped ? g_HUD_Vars.iTeamScores[1] : g_HUD_Vars.iTeamScores[0];
    int logicalScore2 = g_bBroadcastSidesSwapped ? g_HUD_Vars.iTeamScores[0] : g_HUD_Vars.iTeamScores[1];
    if (g_iBroadcastMapHistoryCount < BROADCAST_MAX_MAPS) {
        broadcast_map_result_s &result = g_BroadcastMapHistory[g_iBroadcastMapHistoryCount++];
        GetCurrentMapName(result.mapName, sizeof(result.mapName));
        result.team1Score = logicalScore1;
        result.team2Score = logicalScore2;
        result.winner = winner;
    }
    ++g_iMapWins[winner - 1];
    CONPRINTF("Map recorded: Team %d wins. Series %d-%d.\r\n", winner, g_iMapWins[0], g_iMapWins[1]);
}

// Prepares HUD state for the next map; it does not changelevel.
void cmd_NextMap() {
    g_bBroadcastSidesSwapped = false;
    g_iRoundHalfScore[0] = g_iRoundHalfScore[1] = 0;
    g_iScoreOffset[0] = g_iScoreOffset[1] = 0;
    g_HUD_Vars.iTeamScores[0] = g_HUD_Vars.iTeamScores[1] = 0;
    ApplyLogicalTeamOrder();
    BroadcastResetRoundData();
    CONPRINTF("HUD ready for map %d.\r\n", g_iBroadcastMapHistoryCount + 1);
}

extern int g_iDesiredSpecTarget;

// Directly force the spectator target via client.dll's own internal globals
// (found via disassembly: g_iUser2 at client.dll+0x1012AC, and a "jump to
// spectator target now" flag at client.dll+0x145B08 that the native
// FindNextPlayer-equivalent code sets to 1 right after positioning the
// camera). This bypasses the local player's curstate.iuser2 entirely, since
// that field gets overwritten by the client and isn't the authoritative
// source during demo/HLTV playback.
void FollowPlayerIndex(int index) {
    auto clientBase = reinterpret_cast<DWORD>(g_hClient);
    auto *pIUser2 = reinterpret_cast<int *>(clientBase + 0x1012AC);
    auto *pJumpSpectator = reinterpret_cast<int *>(clientBase + 0x145B08);

    *pIUser2 = index;
    *pJumpSpectator = 1;
}

// Jump directly to a specific player, like number-key spectating in CS2/CSGO.
// Since CS 1.6 has no native "spectate slot N" command, we walk the entity
// list ourselves (skipping invalid/empty slots) to find the Nth player -
// slots 1-5 are Terrorists, slots 6-10 are Counter-Terrorists, matching a
// typical 5v5 - then issue the engine's own "follow" command with their
// name, the same command the native scoreboard uses when you click a name.
// Usage: hltv_spec_slot <1-10>  (bind number keys to this: 1-9 then 0=10)
void cmd_SpecSlot() {
    if (ARGC != 2) {
        CONPRINTF("Usage: %s <slot number 1-10>\r\n", ARGV(0));
        return;
    }

    const int wantedSlot = atoi(ARGV(1));
    if (wantedSlot < 1) {
        return;
    }
    const int wantedTeam = (wantedSlot <= 5) ? 1 : 2; // 1=T, 2=CT
    const int wantedPosInTeam = (wantedSlot <= 5) ? wantedSlot : (wantedSlot - 5);

    int found = 0;
    for (int i = 1; i <= 32; i++) {
        cl_entity_s *ent = ENGINE.GetEntityByIndex(i);
        if (!CHelpers::bIsValidEnt(ent)) {
            continue;
        }
        char *szModel = CHelpers::szGetPlayerModel(i);
        if (CHelpers::iGetTeam(szModel) != wantedTeam) {
            continue;
        }

        found++;
        if (found == wantedPosInTeam) {
            FollowPlayerIndex(i);
            CONPRINTF("Following %s (slot %d)\r\n", CHelpers::szGetPlayerName(i), wantedSlot);
            return;
        }
    }

    CONPRINTF("No player in slot %d.\r\n", wantedSlot);
}

void cmd_SpecFree() {
    g_iDesiredSpecTarget = 0;
    CONPRINTF("Spectator target lock released.\r\n");
}

// Manually triggered (bind to a key, press once after connecting) to hide the
// native spectator status line, which otherwise duplicates our own round score
// in a small font near the top of the screen. Deliberately NOT auto-run from
// any engine hook (e.g. HUD_VidInit) - doing that too early, before these
// native cvars are registered by client.dll, previously caused unrelated
// regressions (health bars / weapon icons breaking).
void cmd_HideNativeHud() {
    ENGINE.Cvar_SetValue((char *) "spec_drawstatus", 0.F);
    ENGINE.Cvar_SetValue((char *) "spec_drawstatus_internal", 0.F);
    ENGINE.Cvar_SetValue((char *) "spec_drawnames", 0.F);
    ENGINE.Cvar_SetValue((char *) "spec_drawnames_internal", 0.F);
    CONPRINTF("Native spectator HUD hidden.\r\n");
}

void cmd_Swap() {
    float score1 = CConVars::getConVarFloat("team_1_score");
    float score2 = CConVars::getConVarFloat("team_2_score");
    CConVars::setValue("team_1_score", score2);
    CConVars::setValue("team_2_score", score1);

    swap(g_HUD_Vars.iClanScores[0], g_HUD_Vars.iClanScores[1]);
    swap(g_HUD_Vars.iTeamScores[0], g_HUD_Vars.iTeamScores[1]);
    swap(g_iRoundHalfScore[0], g_iRoundHalfScore[1]);
    swap(g_iScoreOffset[0], g_iScoreOffset[1]);
    swap(g_HUD_Vars.szClanNames[0], g_HUD_Vars.szClanNames[1]);

    CONPRINTF("Teams swapped.\r\n");
}

// adds a substr to be stripped
void cmd_StripTag() {
    if (ARGC != 2) {
        CONPRINTF("Usage %s <String>\r\n", ARGV(0));
        return;
    }
    AddTag(ARGV(1));
    CONPRINTF("Tag added\r\n");
}
// clears tags, restores names
void cmd_ClearTags() {
    ClearTags();
    CONPRINTF("All tags cleared\r\n");
}

// just added this to prevent others and myself from getting tempted by game invites or to otherwise wander off to
// VAC-servers
void Hook_Connect() { CONPRINTF("**Use the plugin-specific command to connect**\r\n"); }

// Display available fonts
char fontBuffer[128];
int CALLBACK EnumFontFamiliesExProc(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, int FontType, LPARAM lParam) {
    if ((strcmp(fontBuffer, reinterpret_cast<char *>(lpelfe->elfFullName)) == 0) || lpelfe->elfFullName[0] == '@') {
        return 1;
    }

    CONPRINTF("%s\n", lpelfe->elfFullName);
    sprintf(fontBuffer, reinterpret_cast<char *>(lpelfe->elfFullName));
    return 1;
}
void PrintAvailableFonts() {
    CONPRINTF("Available fonts (use them exactly as they appear on each line)\r\n");

    ZeroMemory(&fontBuffer, sizeof(fontBuffer));

    HDC hDC = GetDC(nullptr);

    LOGFONT lf = {0, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, 0};
    EnumFontFamiliesEx(hDC, &lf, reinterpret_cast<FONTENUMPROC>(EnumFontFamiliesExProc), 0, 0);
    ReleaseDC(nullptr, hDC);
}

void Debug() {
    CONPRINTF("unhooking\n");
    UnhookUserMessages();
}

void RegisterCommands() {
    // Fonts
    cvars->addConVar("healthbar_font_yoffset", "0", "font vertical offset");
    cvars->addConVar("healthbar_use_engine_font", "1", "use engine font or not");
    cvars->addConVar("general_font", "Verdana", "font");
    cvars->addConVar("healthbar_font", "Verdana", "font");
    cvars->addConVar("roundtimer_font", "Verdana", "font");
    cvars->addConVar("teamscore_font", "Verdana", "font");
    cvars->addConVar("teamnames_font", "Verdana", "font");
    cvars->addConVar("clanscore_font", "Verdana", "font");

    // Sizes
    cvars->addConVar("general_font_size", "22", "font size");
    cvars->addConVar("healthbar_font_size", "18", "font size");
    cvars->addConVar("roundtimer_font_size", "22", "font size");
    cvars->addConVar("teamscore_font_size", "26", "font size");
    cvars->addConVar("teamnames_font_size", "38", "font size");
    cvars->addConVar("clanscore_font_size", "26", "font size");

    commands->addCommand("debug_cmd", "debug", Debug);
    cvars->addConVar("debug_cvar", "debug", "debug");

    // Rename connect => dj_connect, let connect display a message
    void *pfnConnect = CCommands::hookCommand("connect", Hook_Connect);
    commands->addCommand("connect", "Renamed connect command", pfnConnect);

    commands->addCommand("help", "Displays this help", PrintHelp);
    commands->addCommand("reload", "Reloads HUD settings from ConVars", cmd_HUD_Reload);
    commands->addCommand("hide_native_hud", "Hides the native spectator status line (bind to a key, press once after connecting)", cmd_HideNativeHud);
    commands->addCommand("set_round_score", "Manually sets the round score for both teams: <team1_score> <team2_score>", cmd_SetRoundScore);
    commands->addCommand("match_start", "Starts match: <team1> <team2> [bo1|bo3|bo5]", cmd_MatchStart);
    commands->addCommand("halftime", "Swaps sides and carries the first-half score", cmd_Halftime);
    commands->addCommand("map_winner", "Records map result and adds a series win: <1|2>", cmd_MapWinner);
    commands->addCommand("next_map", "Resets round/side HUD state for the next map", cmd_NextMap);
    commands->addCommand("spec_slot", "Spectates the Nth connected player (like number-key spectating in CS2/CSGO): <slot number>", cmd_SpecSlot);
    commands->addCommand("spec_free", "Releases the forced spectator target set by spec_slot", cmd_SpecFree);

    commands->addCommand("strip_tag", "Strips specified tag from all player names", cmd_StripTag);
    commands->addCommand("strip_clear_tags", "Clear any added tags", cmd_ClearTags);
    commands->addCommand("swap", "Swaps teams around", cmd_Swap);

    commands->addCommand("increment", "Increment specified cvar", cmd_Increment);
    commands->addCommand("decrement", "Decrement specified cvar", cmd_Decrement);
    commands->addCommand("toggle", "Toggle specified cvar between 0/1", cmd_Toggle);

    commands->addCommand("print_available_fonts", "Print installed fonts", PrintAvailableFonts);

    cvars->addConVar("c4timer", "30", "Set to equal the server's mp_c4timer");

    cvars->addConVar("enable_roundtimer", "1",
                     "Enable/disable separate roundtimer (bomb notification is shown anyway)");
    cvars->addConVar("draw_topbar", "0", "Enable/disable drawing the top bar");
    cvars->addConVar("draw_healthbars", "0", "Enable/disable drawing health bars");
    cvars->addConVar("draw_minimap", "0", "Enable/disable drawing the minimap");
    cvars->addConVar("grenade_probe", "0", "Log grenade projectile type and last known XYZ for minimap research");
    cvars->addConVar("minimap_xpos", "10", "Minimap X position");
    cvars->addConVar("minimap_ypos", "8", "Minimap Y position");
    cvars->addConVar("minimap_size", "330", "Minimap width in pixels (auto-capped before top bar)");


    // team names
    cvars->addConVar("team_1_name", "Team 1", "Team 1 name");
    cvars->addConVar("team_2_name", "Team 2", "Team 2 name");

    // team scores (not the T/CT, but the clans or whatchamacallit)
    cvars->addConVar("team_1_score", "0", "Team 1 team score");
    cvars->addConVar("team_2_score", "0", "Team 2 team score");

    // positions
    cvars->addConVar("team_1_name_xpos", "0", "team 1 name position");
    cvars->addConVar("team_1_name_ypos", "0", "team 1 name position");

    cvars->addConVar("team_2_name_xpos", "0", "team 2 name position");
    cvars->addConVar("team_2_name_ypos", "0", "team 2 name position");

    cvars->addConVar("topbar_xpos", "0", "topbar position");
    cvars->addConVar("topbar_ypos", "0", "topbar position");
    cvars->addConVar("topbar_width", "0", "topbar width");
    cvars->addConVar("topbar_height", "0", "topbar height");


    cvars->addConVar("roundtimer_xpos", "0", "round timer position");
    cvars->addConVar("roundtimer_ypos", "0", "round timer position");

    cvars->addConVar("team_1_clanscore_xpos", "0", "clan score position");
    cvars->addConVar("team_1_clanscore_ypos", "0", "clan score position");

    cvars->addConVar("team_2_clanscore_xpos", "0", "clan score position");
    cvars->addConVar("team_2_clanscore_ypos", "0", "clan score position");

    cvars->addConVar("team_1_teamscore_xpos", "0", "team score position");
    cvars->addConVar("team_1_teamscore_ypos", "0", "team score position");

    cvars->addConVar("team_2_teamscore_xpos", "0", "team score position");
    cvars->addConVar("team_2_teamscore_ypos", "0", "team score position");

    // HP bar positions
    for (int i = 0; i < 5; i++) {
        char *temp = new char[64];
        sprintf(temp, "ct_%d_xpos", i + 1);
        cvars->addConVar(temp, "0", "Health bar position");

        temp = new char[64];
        sprintf(temp, "ct_%d_ypos", i + 1);
        cvars->addConVar(temp, "0", "Health bar position");

        temp = new char[64];
        sprintf(temp, "t_%d_xpos", i + 1);
        cvars->addConVar(temp, "0", "Health bar position");

        temp = new char[64];
        sprintf(temp, "t_%d_ypos", i + 1);
        cvars->addConVar(temp, "0", "Health bar position");
    }
}
