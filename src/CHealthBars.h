#include "stdafx.h"

struct playerInfo_s {
    char *szWeapon;
    char *szModel;
    char *szName;
    int team;
    int hp;
    int sequence;
    bool kitbomb;
    int kills;
    int deaths;
    int roundKills;
    bool isObserved;
};

class CHealthBars {
public:
    static void Draw();
};
