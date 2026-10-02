#include "stdafx.h"

struct pos_s {
    int x;
    int y;
};

struct positions_s {
    pos_s topbar_pos, roundTimer_pos, team_1_clanScore_pos, team_1_teamScore_pos,
            team_2_clanScore_pos, team_2_teamScore_pos, team_1_name_pos, team_2_name_pos, minimap_pos;

    pos_s *iArrPosCT;
    pos_s *iArrPosT;
};

extern positions_s g_Positions;
