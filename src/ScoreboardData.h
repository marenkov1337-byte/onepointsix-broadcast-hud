#pragma once

#define SCOREBOARD_MAX_INDEX 32

struct scoreboard_entry_s {
    int id;
    int score;
    int frags;
    int deaths;
    int headshots;
    int teamId;
};

extern scoreboard_entry_s g_ScoreboardData[SCOREBOARD_MAX_INDEX + 1];
