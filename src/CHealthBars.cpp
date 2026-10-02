#include "stdafx.h"

// client.dll authoritative spectator target. This is the same g_iUser2
// location already used by FollowPlayerIndex() in commands.cpp.
static int GetObservedPlayerIndex() {
    if (g_hClient == nullptr) return 0;
    const DWORD clientBase = reinterpret_cast<DWORD>(g_hClient);
    const int target = *reinterpret_cast<int *>(clientBase + 0x1012AC);
    return (target >= 1 && target <= 32) ? target : 0;
}

void CHealthBars::Draw() {
    if ((g_Positions.iArrPosCT == nullptr) || (g_Positions.iArrPosT == nullptr)) {
        return;
    }

    int addedT = 0;
    int addedCT = 0;
    const int observedPlayer = GetObservedPlayerIndex();

    // Max players in GoldSrc is 32, but index 0 is world, so 1-32.
    for (int i = 1; i <= 32; i++) {
        cl_entity_s *ent = ENGINE.GetEntityByIndex(i);

        if (!CHelpers::bIsValidEnt(ent)) {
            continue;
        }

        // Populate player info
        playerInfo_s info = {}; // Initialize to zero

        info.szName = CHelpers::szGetPlayerName(i);
        info.szModel = CHelpers::szGetPlayerModel(i);
        info.szWeapon = CHelpers::szGetWeaponName(ent->curstate.weaponmodel);

        // Null checks
        if (info.szName == nullptr) {
            info.szName = "unknown";
        }
        if (info.szModel == nullptr) {
            info.szModel = "unknown";
        }
        if (info.szWeapon == nullptr) {
            info.szWeapon = "unknown";
        }

        info.team = CHelpers::iGetTeam(info.szModel);
        info.hp = CHelpers::iGetPlayerHP(i);
        info.sequence = CHelpers::iTranslateSequence(ent->curstate.sequence);
        const bool entityHasKitOrBomb = CHelpers::bHasKitOrBomb(ent);

        // HLTV does not expose the T bomb flag consistently while another weapon is selected.
        // Once a T is positively identified as carrying C4 (body flag or C4 weapon), remember
        // that carrier until the bomb is dropped/planted or a new round begins.
        if (info.team == 1 && (entityHasKitOrBomb || strcmp(info.szWeapon, "c4") == 0)) {
            g_iBombCarrier = i;
        }
        info.kitbomb = (info.team == 1) ? (g_iBombCarrier == i) : entityHasKitOrBomb;
        info.kills = g_ScoreboardData[i].frags;
        info.deaths = g_ScoreboardData[i].deaths;
        info.roundKills = g_BroadcastState[i].roundKills;
        info.isObserved = (i == observedPlayer);

        // Position and Draw
        if (info.team == 1 && addedT < 5) {
            CClassicHealthBar card(g_Positions.iArrPosT[addedT].x, g_Positions.iArrPosT[addedT].y - 15, 116, 111, info);
            card.Draw();
            addedT++;
        } else if (info.team == 2 && addedCT < 5) {
            CClassicHealthBar card(g_Positions.iArrPosCT[addedCT].x, g_Positions.iArrPosCT[addedCT].y - 15, 116, 111, info);
            card.Draw();
            addedCT++;
        }
    }
}
