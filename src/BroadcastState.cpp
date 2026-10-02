#include "stdafx.h"
#include "BroadcastState.h"

broadcast_player_state_s g_BroadcastState[BROADCAST_MAX_PLAYERS + 1] = {};
int g_iBombCarrier = 0;
int g_iBestOf = 1;
int g_iMapWins[2] = {0, 0};
bool g_bBroadcastSidesSwapped = false;
broadcast_map_result_s g_BroadcastMapHistory[BROADCAST_MAX_MAPS] = {};
int g_iBroadcastMapHistoryCount = 0;

void BroadcastResetRoundData() {
    g_iBombCarrier = 0;
    for (int i = 1; i <= BROADCAST_MAX_PLAYERS; ++i) {
        g_BroadcastState[i].roundKills = 0;
    }
}
