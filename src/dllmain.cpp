// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

// HLTV HUD
// Sk�ll
// http://www.gamedeception.net/threads/22277-Healthbars?p=163659#post163659


// === Globals ===
// Offsets
DWORD g_dwHLTV = 0x12F464; // client.dll (start of info arr) -- verified via disassembly of the confirmed-working build
// --- Offsets below patched for build 1.1.2.7/Stdio (Aug 3 2020, 8684) ---
// Found via static analysis of hw.dll (see conversation notes). Confidence noted per line.
DWORD g_dwCommandList = 0x2E3D60;  // HIGH confidence: node-insertion fn matched via known engine commands
DWORD g_dwUserMessages = 0x16D27C; // HIGH confidence: matched via pfnHookUserMsg trampoline -> impl
DWORD g_dwEngine = 0x136260;       // HIGH confidence: matched via client.dll Initialize() call site (push 7 = iVersion)
DWORD g_dwClient = 0x122ED60;      // HIGH confidence: matched via GetProcAddress loading sequence, field order verified
DWORD g_dwStudio = 0x153248;       // HIGH confidence: matched via HUD_GetStudioModelInterface call site (validated: all struct fields point into .text)
DWORD g_dwInterface = 0x15330C;    // Unused by any currently active feature (only g_pStudio is used)
// OpenGL table
DWORD g_dw_glDeleteTextures = 0x11C274; // CONFIRMED via PE import table (OPENGL32.dll!glDeleteTextures IAT slot)

HANDLE g_hHW_DLL = nullptr;
HANDLE g_hClient = nullptr;

cl_enginefuncs_s g_oEngine;
cl_enginefuncs_s *g_pEngine;

cl_clientfuncs_s g_oClient;
cl_clientfuncs_s *g_pClient;

engine_studio_api_s *g_pStudio;
r_studio_interface_s *g_pInterface;

hud_globals_s g_HUD_Vars;
char g_szPluginPath[MAX_PATH];
sprite_data_s *g_Sprites = nullptr;
char **g_pszPlayerNames = nullptr;

void InitializeHack();

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(InitializeHack), nullptr, 0, nullptr);
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

void InitializeHack() {
    do {
        g_hHW_DLL = GetModuleHandleA("hw.dll");
        g_hClient = GetModuleHandleA("client.dll");
        Sleep(10);
    } while (g_hHW_DLL == nullptr);

    // get the Counter-Strike path
    char szPath[256];
    int len = static_cast<int>(GetModuleFileNameA(nullptr, szPath, sizeof(szPath)));
    szPath[len - 6] = 0; // remove "hl.exe"
    sprintf(g_szPluginPath, "%sonepointsix_broadcast\\", szPath);

    // struct pointers
    g_pEngine = (cl_enginefuncs_s *) ((DWORD) g_hHW_DLL + g_dwEngine);
    g_pClient = (cl_clientfuncs_s *) ((DWORD) g_hHW_DLL + g_dwClient);
    g_pStudio = (engine_studio_api_s *) ((DWORD) g_hHW_DLL + g_dwStudio);
    g_pInterface = (r_studio_interface_s *) ((DWORD) g_hHW_DLL + g_dwInterface);


    // copies for later calling the original functions from our hooked ones
    memcpy(&g_oEngine, g_pEngine, sizeof(g_oEngine));
    memcpy(&g_oClient, g_pClient, sizeof(g_oClient));

    // HLTV variables & positions
    // not sure if properly intialized, hence
    ZeroMemory(&g_HUD_Vars, sizeof(g_HUD_Vars));
    ZeroMemory(&g_Positions, sizeof(g_Positions));
    ZeroMemory(&g_ScoreboardData, sizeof(g_ScoreboardData));

    // names are later copied every now and then for use with the strip tag function
    g_pszPlayerNames = new char *[32];
    for (int i = 0; i < 32; i++) {
        g_pszPlayerNames[i] = new char[64];
    }

    CONPRINTF(
            "\r\n================================================\r\nOnePointSix Broadcast HUD v1.0.0\r\n"
            "For help, type hltv_help\r\n"
            "================================================\r\n\r\n");

    Hook(); // client functions
    HookUserMessages();
    RegisterCommands();
    LoadSprites();

    // TODO check if files actually exist
    if (!CHelpers::bDirectoryExists(g_szPluginPath)) {
        CONPRINTF("** WARNING ** The plugin directory could not be found:\r\n");
        CONPRINTF("\"%s\"", g_szPluginPath);
    }
}
