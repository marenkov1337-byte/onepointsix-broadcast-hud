#include "stdafx.h"

void ReinitializeFonts();

auto Hook_HUD_Redraw(float flArg, int iArg) -> int;

extern CHealthBars *hud;
extern CTopBar *topbar;
extern bool g_bReinitializeFonts;

// Delayed spectator handoff after a followed player dies.
extern int g_iLastDeathVictim;
extern int g_iLastDeathKiller;
extern double g_flLastDeathTime;
