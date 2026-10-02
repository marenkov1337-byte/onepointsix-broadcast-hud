#include "stdafx.h"

SCREENINFO g_ScreenInfo;


auto Hook_HUD_VidInit() -> int {
    // update screen width&height
    g_ScreenInfo.iSize = sizeof(g_ScreenInfo);
    ENGINE.pfnGetScreenInfo(&g_ScreenInfo);

    // Let the native client rebuild/initialize its HUD first. In particular,
    // keep CHudDeathNotice's native DeathMsg path intact for the stock kill feed.
    g_bReinitializeFonts = true;
    const int ret = CLIENT.HUD_VidInit();

    // Some builds rebuild the user-message list during HUD_VidInit. Hook only
    // afterwards so our stats hook chains to the final native DeathMsg handler.
    HookUserMessages();

    InitializeSprites();
    return ret;
}
