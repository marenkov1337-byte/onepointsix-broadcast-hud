#pragma once

#define BROADCAST_MAX_PLAYERS 32
#define BROADCAST_MAX_MAPS 5

struct broadcast_player_state_s {
    int roundKills;
};

struct broadcast_map_result_s {
    char mapName[64];
    int team1Score;
    int team2Score;
    int winner; // logical team: 1 or 2
};

extern broadcast_player_state_s g_BroadcastState[BROADCAST_MAX_PLAYERS + 1];
extern int g_iBombCarrier;

// Match-series state. Team 1/2 are permanent identities for the whole series;
// the display sides can swap at halftime.
extern int g_iBestOf;                 // 1, 3 or 5
extern int g_iMapWins[2];             // logical Team 1 / Team 2
extern bool g_bBroadcastSidesSwapped;
extern broadcast_map_result_s g_BroadcastMapHistory[BROADCAST_MAX_MAPS];
extern int g_iBroadcastMapHistoryCount;

void BroadcastResetRoundData();
