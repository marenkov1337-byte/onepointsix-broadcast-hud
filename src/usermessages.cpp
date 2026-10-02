#include "stdafx.h"

// the complete struct is probably in the SDK somewhere, bleh
struct UserMessageList_s {
    int whatisthis;
    int idonteven;
    char pszFuncName[16];
    UserMessageList_s *pNext;
    pfnUserMsgHook pfnUserMessage;
};

// inspired by stev3s post on addCommand hooking
pfnUserMsgHook HookUserMsg(char *szMsgName, void *pfnUserMessage) {
    UserMessageList_s *pList = *(UserMessageList_s **) ((DWORD) g_hHW_DLL + g_dwUserMessages);
    while (pList != nullptr) {
        if (strcmp(pList->pszFuncName, szMsgName) == 0) {
            // Already hooked to us (e.g. re-invoked on a later HUD_VidInit while the
            // engine kept the same node) -> do nothing, or we'd self-reference and
            // recurse forever the next time this message fires.
            if (pList->pfnUserMessage == reinterpret_cast<pfnUserMsgHook>(pfnUserMessage)) {
                return nullptr;
            }
            pfnUserMsgHook ret = pList->pfnUserMessage;
            pList->pfnUserMessage = reinterpret_cast<pfnUserMsgHook>(pfnUserMessage);
            return ret;
        }
        pList = pList->pNext;
    }
    return nullptr;
}

void DumpUserMessages() {
    UserMessageList_s *pList = *(UserMessageList_s **) ((DWORD) g_hHW_DLL + g_dwUserMessages);
    CONPRINT("UserMsg list:\r\n");
    while (pList != nullptr) {
        CONPRINTF("%s (Located at: %X \r\n", pList->pszFuncName, pList->pfnUserMessage);
        pList = pList->pNext;
    }
}

// === Hooked usermessages below ===
// Team score is updated
// Kept as separate standalone globals (not part of hud_globals_s) to avoid
// any risk of disturbing that shared struct's memory layout for other code.
int g_iRoundHalfScore[2] = {0, 0}; // raw per-half score, as reported by the server
int g_iScoreOffset[2] = {0, 0};    // manually set base carried over via hltv_set_round_score
pfnUserMsgHook oTeamScore;
auto TeamScore(const char *szMsgName, int iSize, void *pbuf) -> int {
    BEGIN_READ(pbuf, iSize);
    char *teamName = READ_STRING();
    int score = READ_BYTE();

    if (strcmp(teamName, "TERRORIST") == 0) {
        g_iRoundHalfScore[0] = score;
        g_HUD_Vars.iTeamScores[0] = score + g_iScoreOffset[0];
    } else if (strcmp(teamName, "CT") == 0) {
        g_iRoundHalfScore[1] = score;
        g_HUD_Vars.iTeamScores[1] = score + g_iScoreOffset[1];
    }

    return oTeamScore(szMsgName, iSize, pbuf);
}

// HLTV sends 0,0 at the beginning of a new CS round, before freezetime.
// Reset per-round HUD data here so round-kill badges disappear immediately
// when the new round starts instead of waiting for the post-freezetime RoundTime jump.
pfnUserMsgHook oHLTV;
auto HLTVMsg(const char *szMsgName, int iSize, void *pbuf) -> int {
    BEGIN_READ(pbuf, iSize);
    const int first = READ_BYTE();
    const int second = READ_BYTE();

    if (first == 0 && second == 0) {
        BroadcastResetRoundData();
    }

    return oHLTV ? oHLTV(szMsgName, iSize, pbuf) : 0;
}

// Round time is updated
pfnUserMsgHook oRoundTime;
auto RoundTime(const char *szMsgName, int iSize, void *pbuf) -> int {
    BEGIN_READ(pbuf, iSize);
    int time = READ_SHORT();

    // Uppdate round time
    g_HUD_Vars.iRoundTime = time;
    // A fresh full round timer marks the start of a round for our per-round counters.
    static int lastRoundTime = 0;

    if (lastRoundTime > 0 && time > lastRoundTime + 5) {
        BroadcastResetRoundData();
    } else if (lastRoundTime == 0) {
        // Premier round observé : initialise simplement les compteurs.
        BroadcastResetRoundData();
    }

    lastRoundTime = time;
    // Do not clear bBombPlanted here: RoundTime repeats for the whole round; clearing it made the top bar alternate
    // between DrawTexture and pfnSPR_Draw, which broke sprite rendering for the rest of the HUD redraw.

    return oRoundTime(szMsgName, iSize, pbuf);
}

// Centered text
pfnUserMsgHook oTextMsg;
auto TextMsg(const char *szMsgName, int iSize, void *pbuf) -> int {
    BEGIN_READ(pbuf, iSize);
    int destinationType = READ_BYTE();
    char *message = READ_STRING();
    if (strcmp(message, "#Bomb_Planted") == 0) {
        g_iBombCarrier = 0;
        g_HUD_Vars.bBombPlanted = true;
        g_HUD_Vars.iRoundTime = CConVars::getConVarFloat("c4timer");
    } else if (strcmp(message, "#Game_bomb_drop") == 0 || strcmp(message, "#Bomb_Dropped") == 0) {
        g_iBombCarrier = 0;
    } else if (strcmp(message, "#Bomb_Defused") == 0 || strcmp(message, "#Target_Bombed") == 0 ||
               strcmp(message, "#CTs_Win") == 0 || strcmp(message, "#Terrorists_Win") == 0 ||
               strcmp(message, "#Round_Draw") == 0 || strcmp(message, "#Game_Commencing") == 0) {
        g_HUD_Vars.bBombPlanted = false;
    }

    return oTextMsg(szMsgName, iSize, pbuf);
}

pfnUserMsgHook oScoreInfo;
auto try_scoreinfo(void *pbuf, int iSize, int skipBytes, int *playerID, int *frags, int *deaths, int *classID,
                   int *teamID) -> bool {
    BEGIN_READ(pbuf, iSize);
    for (int s = 0; s < skipBytes; s++) {
        READ_BYTE();
    }
    *playerID = READ_BYTE();
    *frags = READ_SHORT();
    *deaths = READ_SHORT();
    *classID = READ_SHORT();
    *teamID = READ_SHORT();
    return READ_OK() != 0 && *playerID >= 1 && *playerID <= SCOREBOARD_MAX_INDEX;
}

// HL/CS ScoreInfo: byte index + 4 shorts (see halflife dlls/player.cpp). Some paths pass an extra leading byte
// (e.g. iSize 10); try skip 0/1 so reads stay aligned with the game scoreboard.
auto ScoreInfo(const char *szMsgName, int iSize, void *pbuf) -> int {
    int playerID = 0;
    int frags = 0;
    int deaths = 0;
    int classID = 0;
    int teamID = 0;
    bool ok = false;

    if (iSize >= 9) {
        for (int skip = 0; skip <= 1 && !ok; skip++) {
            if (try_scoreinfo(pbuf, iSize, skip, &playerID, &frags, &deaths, &classID, &teamID)) {
                ok = true;
            }
        }
    }

    if (ok) {
        if (frags == 0 && deaths == 0) {
            g_ScoreboardData[playerID].headshots = 0;
        }

        g_ScoreboardData[playerID].id = playerID;
        g_ScoreboardData[playerID].frags = frags;
        g_ScoreboardData[playerID].score = frags;
        g_ScoreboardData[playerID].deaths = deaths;
        g_ScoreboardData[playerID].teamId = teamID;
        (void) classID;
    }

    return oScoreInfo(szMsgName, iSize, pbuf);
}

pfnUserMsgHook oDeathMsg;
auto DeathMsg(const char *szMsgName, int iSize, void *pbuf) -> int {
    BEGIN_READ(pbuf, iSize);

    int KillerID = READ_BYTE();
    int VictimID = READ_BYTE();
    int IsHeadshot = READ_BYTE();
    char *TruncatedWeaponName = READ_STRING();
    (void) TruncatedWeaponName;

    // Remember the exact death event for spectator handoff. The redraw hook
    // holds the victim for 0.5 s, then follows the killer when possible.
    if (VictimID >= 1 && VictimID <= SCOREBOARD_MAX_INDEX) {
        g_iLastDeathVictim = VictimID;
        g_iLastDeathKiller = KillerID;
        g_flLastDeathTime = ENGINE.GetClientTime ? ENGINE.GetClientTime() : 0.0;
    }

    if (KillerID >= 1 && KillerID <= SCOREBOARD_MAX_INDEX && KillerID != VictimID) {
        g_BroadcastState[KillerID].roundKills++;
        if (IsHeadshot == 1) g_ScoreboardData[KillerID].headshots++;
    }

    return oDeathMsg(szMsgName, iSize, pbuf);
}


void HookUserMessages() {
    if (auto ret = HookUserMsg("ScoreInfo", ScoreInfo)) oScoreInfo = ret;
    if (auto ret = HookUserMsg("TeamScore", TeamScore)) oTeamScore = ret;
    if (auto ret = HookUserMsg("HLTV", HLTVMsg)) oHLTV = ret;
    if (auto ret = HookUserMsg("RoundTime", RoundTime)) oRoundTime = ret;
    if (auto ret = HookUserMsg("TextMsg", TextMsg)) oTextMsg = ret;
    if (auto ret = HookUserMsg("DeathMsg", DeathMsg)) oDeathMsg = ret;
}

void UnhookUserMessages() {
    HookUserMsg("ScoreInfo", oScoreInfo);
    HookUserMsg("TeamScore", oTeamScore);
    if (oHLTV) HookUserMsg("HLTV", oHLTV);
    HookUserMsg("RoundTime", oRoundTime);
    HookUserMsg("TextMsg", oTextMsg);
    HookUserMsg("DeathMsg", oDeathMsg);
}
