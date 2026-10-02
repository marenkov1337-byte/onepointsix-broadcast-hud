#include "stdafx.h"

using pfn_glDeleteTextures = void(WINAPI *)(GLsizei n, const GLuint *textures);
pfn_glDeleteTextures original = nullptr;

void WINAPI Hook_glDeleteTextures(GLsizei n, const GLuint *textures) {
    for (int i = 0; i < n; i++) {
        CONPRINTF("Freeing texture %d\r\n", textures[i]);
        CMinimap::OnTextureDeleted(textures[i]);
    }
    original(n, textures);
}

void Hook() {
    g_pClient->HUD_Frame = hook_hud_frame;
    g_pClient->HUD_Redraw = Hook_HUD_Redraw;
    g_pClient->HUD_VidInit = Hook_HUD_VidInit;
    g_pClient->HUD_AddEntity = Hook_HUD_AddEntity;
    g_pClient->HUD_TempEntUpdate = Hook_HUD_TempEntUpdate;
    GrenadeProbeInstallEngineHooks();

    auto *offset = (DWORD *) ((DWORD) g_hHW_DLL + g_dw_glDeleteTextures);
    original = (pfn_glDeleteTextures) *offset;

    DWORD oldProtect;
    VirtualProtect(offset, sizeof(DWORD), PAGE_READWRITE, &oldProtect);
    *offset = (DWORD) &Hook_glDeleteTextures;
    VirtualProtect(offset, sizeof(DWORD), oldProtect, &oldProtect);
}

void Unhook() {
    g_pClient->HUD_Frame = CLIENT.HUD_Frame;
    g_pClient->HUD_Redraw = CLIENT.HUD_Redraw;
    g_pClient->HUD_VidInit = CLIENT.HUD_VidInit;
    g_pClient->HUD_AddEntity = CLIENT.HUD_AddEntity;
    g_pClient->HUD_TempEntUpdate = CLIENT.HUD_TempEntUpdate;
    GrenadeProbeRemoveEngineHooks();

    auto *offset = (DWORD *) ((DWORD) g_hHW_DLL + g_dw_glDeleteTextures);
    DWORD oldProtect;
    VirtualProtect(offset, sizeof(DWORD), PAGE_READWRITE, &oldProtect);
    *offset = (DWORD) original;
    VirtualProtect(offset, sizeof(DWORD), oldProtect, &oldProtect);
}
