#include "stdafx.h"
#include <cstdarg>
#include <cstring>

namespace {
// Steam GoldSrc build 8684, verified against the user's exact hw.dll.
static const DWORD kQueueEventRva       = 0x0001E910;
static const DWORD kSvcEventCallRva     = 0x0001EB7E;
static const DWORD kSvcReliableCallRva  = 0x0001EC64;

// Internal sound path in the same exact hw.dll build.
// RVA 0x1F5FC is a verified direct CALL to S_StartDynamicSound (RVA 0x8C210).
// This sits below the engine-function sound hooks used in v4.67, which is why it
// can observe sounds decoded from demo/HLTV network playback.
static const DWORD kStartDynamicSoundRva = 0x0008C210;
static const DWORD kStartStaticSoundRva  = 0x0008C4C0;
// Exact svc_sound handler (service id 6) in the user's build-8684 hw.dll is RVA 0x198F0.
// Its verified calls into the sound mixer are:
//   RVA 0x1AA4C -> S_StartStaticSound (RVA 0x8C4C0)
//   RVA 0x1AA6F -> S_StartDynamicSound (RVA 0x8C210)
// v4.77/v4.78 incorrectly patched an unrelated sound wrapper at 0x1F5D5/0x1F5FC.
static const DWORD kNetworkStaticSoundCallRva = 0x0001AA4C;
static const DWORD kNetworkSoundCallRva       = 0x0001AA6F;

using QueueEventFn = void (__cdecl *)(int flags, unsigned short eventIndex, float delay, event_args_t *args);
using StartDynamicSoundFn = void (__cdecl *)(int entnum, int entchannel, void *sfx, float *origin,
                                             float volume, float attenuation, int flags, int pitch);
using StartStaticSoundFn = StartDynamicSoundFn;

struct CallPatch {
    DWORD rva;
    BYTE original[5];
    bool installed;
};

CallPatch g_eventPatch    = { kSvcEventCallRva,    {0}, false };
CallPatch g_reliablePatch = { kSvcReliableCallRva, {0}, false };
CallPatch g_soundPatch    = { kNetworkSoundCallRva,{0}, false };
CallPatch g_staticSoundPatch = { kNetworkStaticSoundCallRva,{0}, false };
QueueEventFn g_originalQueueEvent = nullptr;
StartDynamicSoundFn g_originalStartDynamicSound = nullptr;
StartStaticSoundFn g_originalStartStaticSound = nullptr;

bool g_wasEnabled = false;
bool g_eventHooksInstalled = false;
bool g_soundHookInstalled = false;
bool g_staticSoundHookInstalled = false;
FILE *g_log = nullptr;

struct PendingSmoke {
    bool active;
    unsigned short eventIndex;
    float origin[3];
    double clientTime;
};
static const int kMaxPendingSmokes = 8;
PendingSmoke g_pendingSmokes[kMaxPendingSmokes] = {};

struct TempSeen {
    const tempent_s *ptr;
    const void *modelPtr;
    float die;
    double lastSeen;
};
TempSeen g_tempSeen[1024] = {};

static const int kMaxVisualEvents = 24;
GrenadeVisualEvent g_visualEvents[kMaxVisualEvents] = {};
int g_visualWrite = 0;

bool Near(float a, float b, float eps = 0.6f) { return fabs(a - b) <= eps; }
bool IsZero3(const float *v, float eps = 0.01f) {
    return Near(v[0], 0.0f, eps) && Near(v[1], 0.0f, eps) && Near(v[2], 0.0f, eps);
}
bool Same3(const float *a, const float *b, float eps = 0.6f) {
    return Near(a[0], b[0], eps) && Near(a[1], b[1], eps) && Near(a[2], b[2], eps);
}
float DistSq3(const float *a, const float *b) {
    const float dx=a[0]-b[0], dy=a[1]-b[1], dz=a[2]-b[2];
    return dx*dx+dy*dy+dz*dz;
}
double ClientTimeNow() { return ENGINE.GetClientTime ? ENGINE.GetClientTime() : 0.0; }
bool Enabled() { return CConVars::getConVarFloat((char *)"grenade_probe") != 0.F; }

void OpenLog() {
    if (g_log) return;
    const char *dir = ENGINE.pfnGetGameDirectory ? ENGINE.pfnGetGameDirectory() : nullptr;
    char path[MAX_PATH] = {0};
    if (dir && *dir) _snprintf(path, sizeof(path)-1, "%s\\hltv_grenade_probe.log", dir);
    else strncpy(path, "hltv_grenade_probe.log", sizeof(path)-1);
    g_log = fopen(path, "a");
}

void Log(const char *fmt, ...) {
    if (!Enabled()) return;
    char buf[2048];
    va_list ap; va_start(ap, fmt); _vsnprintf(buf, sizeof(buf)-1, fmt, ap); va_end(ap);
    buf[sizeof(buf)-1] = '\0';
    CONPRINTF("%s", buf);
    OpenLog();
    if (g_log) { fputs(buf, g_log); fflush(g_log); }
}

bool FirstTempSeen(const tempent_s *p, double now) {
    if (!p) return false;
    const void *modelPtr = p->entity.model;
    const float die = p->die;
    int freeSlot = -1;
    for (int i=0;i<1024;i++) {
        if (g_tempSeen[i].ptr == p) {
            // TEMPENTs come from a recycled pool. A pointer can represent blood, muzzleflash,
            // then an explosion later. Pointer-only dedup (v4.77/v4.78) therefore suppressed
            // legitimate HE explosions. Treat a changed model or lifetime as a new tempent.
            const bool sameInstance =
                g_tempSeen[i].modelPtr == modelPtr && fabs(g_tempSeen[i].die - die) <= 0.001f;
            g_tempSeen[i].modelPtr = modelPtr;
            g_tempSeen[i].die = die;
            g_tempSeen[i].lastSeen = now;
            return !sameInstance;
        }
        if (!g_tempSeen[i].ptr && freeSlot < 0) freeSlot = i;
        if (g_tempSeen[i].ptr && now - g_tempSeen[i].lastSeen > 5.0 && freeSlot < 0) freeSlot = i;
    }
    if (freeSlot < 0) freeSlot = ((uintptr_t)p >> 4) % 1024;
    g_tempSeen[freeSlot].ptr = p;
    g_tempSeen[freeSlot].modelPtr = modelPtr;
    g_tempSeen[freeSlot].die = die;
    g_tempSeen[freeSlot].lastSeen = now;
    return true;
}

const char *TempModelName(const tempent_s *p) {
    if (!p || !p->entity.model) return nullptr;
    return p->entity.model->name;
}

bool AddVisualEvent(int type, const float *origin, double now, double duration) {
    if (!origin) return false;
    // Collapse multiple low-level signals that belong to one logical grenade detonation.
    // Keep the first position as the stable minimap anchor; later paired/recycled effects
    // only extend its lifetime. This is especially important for HE eexplo/fexplo pairs.
    for (int i=0;i<kMaxVisualEvents;i++) {
        GrenadeVisualEvent &e = g_visualEvents[i];
        if (e.type != type || e.endTime <= now) continue;
        if (fabs(e.startTime - now) <= 0.35 && DistSq3(e.origin, origin) <= 128.0f*128.0f) {
            if (e.endTime < now + duration) e.endTime = now + duration;
            return false;
        }
    }

    GrenadeVisualEvent &e = g_visualEvents[g_visualWrite];
    g_visualWrite = (g_visualWrite + 1) % kMaxVisualEvents;
    e.type = type;
    e.origin[0]=origin[0]; e.origin[1]=origin[1]; e.origin[2]=origin[2];
    e.startTime = now;
    e.endTime = now + duration;
    return true;
}

void ExpirePendingSmokes(double now) {
    for (int i=0;i<kMaxPendingSmokes;i++) {
        if (g_pendingSmokes[i].active && now > 0.0 && g_pendingSmokes[i].clientTime > 0.0 &&
            now - g_pendingSmokes[i].clientTime > 1.0)
            g_pendingSmokes[i].active = false;
    }
}

void AddSmokeCandidate(unsigned short eventIndex, const event_args_t *args, double now) {
    int slot=-1;
    for (int i=0;i<kMaxPendingSmokes;i++) if (!g_pendingSmokes[i].active) { slot=i; break; }
    if (slot < 0) slot=0;
    PendingSmoke &p=g_pendingSmokes[slot];
    p.active=true; p.eventIndex=eventIndex; p.clientTime=now;
    memcpy(p.origin,args->origin,sizeof(p.origin));
}

bool WriteCallTarget(CallPatch &patch, void *target) {
    BYTE *site = reinterpret_cast<BYTE *>((DWORD)g_hHW_DLL + patch.rva);
    if (!patch.installed) {
        if (site[0] != 0xE8) return false;
        memcpy(patch.original, site, sizeof(patch.original));
    }
    DWORD oldProtect=0;
    if (!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&oldProtect)) return false;
    const intptr_t rel = reinterpret_cast<BYTE *>(target) - (site + 5);
    site[0]=0xE8;
    *reinterpret_cast<int32_t *>(site+1)=static_cast<int32_t>(rel);
    FlushInstructionCache(GetCurrentProcess(),site,5);
    DWORD ignored=0; VirtualProtect(site,5,oldProtect,&ignored);
    patch.installed=true;
    return true;
}

void RestoreCall(CallPatch &patch) {
    if (!patch.installed) return;
    BYTE *site = reinterpret_cast<BYTE *>((DWORD)g_hHW_DLL + patch.rva);
    DWORD oldProtect=0;
    if (VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&oldProtect)) {
        memcpy(site,patch.original,sizeof(patch.original));
        FlushInstructionCache(GetCurrentProcess(),site,5);
        DWORD ignored=0; VirtualProtect(site,5,oldProtect,&ignored);
    }
    patch.installed=false;
}

void __cdecl Hook_QueueEvent(int flags, unsigned short eventIndex, float delay, event_args_t *args) {
    if (args) {
        const double now=ClientTimeNow();
        ExpirePendingSmokes(now);
        const bool initialSmokeSignature =
            args->entindex==2047 && IsZero3(args->angles) &&
            Near(args->fparam1,0.0f,0.001f) && Near(args->fparam2,0.0f,0.001f) &&
            args->iparam1==0 && args->iparam2==1 && args->bparam2==0;
        if (initialSmokeSignature) {
            AddSmokeCandidate(eventIndex,args,now);
            Log("[GRENADE][SMOKE_CANDIDATE] index=%u xyz=(%.1f %.1f %.1f) time=%.3f\r\n",
                (unsigned)eventIndex,args->origin[0],args->origin[1],args->origin[2],now);
        }
        const bool puff = args->entindex==2047 && args->iparam2==4 && args->bparam2!=0;
        if (puff) {
            for (int i=0;i<kMaxPendingSmokes;i++) {
                PendingSmoke &p=g_pendingSmokes[i];
                if (!p.active || p.eventIndex!=eventIndex || !Same3(args->angles,p.origin)) continue;
                const double age=(now>0.0 && p.clientTime>0.0)?now-p.clientTime:0.0;
                AddVisualEvent(GRENADE_VIS_SMOKE,p.origin,now,20.0);
                Log("[GRENADE][SMOKE] DETONATE index=%u xyz=(%.1f %.1f %.1f) dt=%.3f\r\n",
                    (unsigned)eventIndex,p.origin[0],p.origin[1],p.origin[2],age);
                p.active=false;
                break;
            }
        }
    }
    if (g_originalQueueEvent) g_originalQueueEvent(flags,eventIndex,delay,args);
}


void ObserveFlashSound(const char *pathTag, int entnum, void *sfx, float *origin) {
    const char *sample = reinterpret_cast<const char *>(sfx);
    if (!sample || !origin) return;
    if (strstr(sample,"weapons/flashbang-1.wav") || strstr(sample,"weapons/flashbang-2.wav")) {
        const double now = ClientTimeNow();
        if (AddVisualEvent(GRENADE_VIS_FLASH,origin,now,1.0)) {
            Log("[GRENADE][FLASH] DETONATE path=%s sample=%s xyz=(%.1f %.1f %.1f) ent=%d\r\n",
                pathTag,sample,origin[0],origin[1],origin[2],entnum);
        }
    }
}

void __cdecl Hook_StartDynamicSound(int entnum, int entchannel, void *sfx, float *origin,
                                    float volume, float attenuation, int flags, int pitch) {
    ObserveFlashSound("dynamic",entnum,sfx,origin);
    if (g_originalStartDynamicSound)
        g_originalStartDynamicSound(entnum,entchannel,sfx,origin,volume,attenuation,flags,pitch);
}

void __cdecl Hook_StartStaticSound(int entnum, int entchannel, void *sfx, float *origin,
                                   float volume, float attenuation, int flags, int pitch) {
    ObserveFlashSound("static",entnum,sfx,origin);
    if (g_originalStartStaticSound)
        g_originalStartStaticSound(entnum,entchannel,sfx,origin,volume,attenuation,flags,pitch);
}
}

void GrenadeProbeReset() {
    if (g_log) { fclose(g_log); g_log=nullptr; }
    g_wasEnabled=false;
    memset(g_pendingSmokes,0,sizeof(g_pendingSmokes));
    memset(g_tempSeen,0,sizeof(g_tempSeen));
    memset(g_visualEvents,0,sizeof(g_visualEvents));
    g_visualWrite=0;
}

void GrenadeProbeObserveEntity(cl_entity_s *, const char *) {}

void GrenadeProbeFrame(double clientTime) {
    const bool enabled=Enabled();
    if (enabled && !g_wasEnabled) {
        g_wasEnabled=true; OpenLog();
        Log("\r\n[GRENADE][PROBE] ===== v4.80 STABLE MINIMAP GRENADE EVENTS ENABLED clientTime=%.3f =====\r\n",clientTime);
        Log("[GRENADE][PROBE] smoke=%d flash_dynamic=%d flash_static=%d; logical-event dedup enabled; trajectory probes removed.\r\n",
            g_eventHooksInstalled?1:0,g_soundHookInstalled?1:0,g_staticSoundHookInstalled?1:0);
    } else if (!enabled && g_wasEnabled) {
        g_wasEnabled=false;
        if (g_log) { fputs("[GRENADE][PROBE] ===== DISABLED =====\r\n",g_log); fclose(g_log); g_log=nullptr; }
    }
}

int GrenadeProbeGetVisualEvents(GrenadeVisualEvent *outEvents, int maxEvents, double clientTime) {
    if (!outEvents || maxEvents<=0) return 0;
    int count=0;
    for (int i=0;i<kMaxVisualEvents && count<maxEvents;i++) {
        const GrenadeVisualEvent &e=g_visualEvents[i];
        if (e.type==0 || e.endTime<=clientTime) continue;
        outEvents[count++]=e;
    }
    return count;
}

int Hook_HUD_AddEntity(int type, cl_entity_s *ent, const char *modelname) {
    return CLIENT.HUD_AddEntity(type,ent,modelname);
}

void Hook_HUD_TempEntUpdate(double frametime, double client_time, double cl_gravity,
                            tempent_s **ppTempEntFree, tempent_s **ppTempEntActive,
                            int (*Callback_AddVisibleEntity)(cl_entity_s *pEntity),
                            void (*Callback_TempEntPlaySound)(tempent_s *pTemp, float damp)) {
    // svc_temp_entity has already created active TEMPENTs before this callback. Scan the
    // active list before the client advances/recycles it. The dedup key is recycle-aware.
    if (ppTempEntActive) {
        int count=0;
        for (tempent_s *p=*ppTempEntActive; p && count<512; p=p->next,++count) {
            const char *tm=TempModelName(p);
            if (!tm || !FirstTempSeen(p,client_time)) continue;
            if (strstr(tm,"eexplo.spr") || strstr(tm,"fexplo.spr")) {
                if (AddVisualEvent(GRENADE_VIS_HE,p->entity.origin,client_time,1.0)) {
                    Log("[GRENADE][HE] DETONATE tempent=%s xyz=(%.1f %.1f %.1f)\r\n",
                        tm,p->entity.origin[0],p->entity.origin[1],p->entity.origin[2]);
                }
            }
        }
    }

    CLIENT.HUD_TempEntUpdate(frametime,client_time,cl_gravity,ppTempEntFree,ppTempEntActive,
                             Callback_AddVisibleEntity,Callback_TempEntPlaySound);
}

void GrenadeProbeInstallEngineHooks() {
    if (!g_hHW_DLL) return;
    if (!g_eventHooksInstalled) {
        g_originalQueueEvent=reinterpret_cast<QueueEventFn>((DWORD)g_hHW_DLL+kQueueEventRva);
        const bool a=WriteCallTarget(g_eventPatch,reinterpret_cast<void *>(&Hook_QueueEvent));
        const bool b=WriteCallTarget(g_reliablePatch,reinterpret_cast<void *>(&Hook_QueueEvent));
        if (!a || !b) {
            RestoreCall(g_eventPatch); RestoreCall(g_reliablePatch);
            g_originalQueueEvent=nullptr;
        } else g_eventHooksInstalled=true;
    }
    if (!g_soundHookInstalled) {
        g_originalStartDynamicSound=reinterpret_cast<StartDynamicSoundFn>((DWORD)g_hHW_DLL+kStartDynamicSoundRva);
        if (WriteCallTarget(g_soundPatch,reinterpret_cast<void *>(&Hook_StartDynamicSound)))
            g_soundHookInstalled=true;
        else g_originalStartDynamicSound=nullptr;
    }
    if (!g_staticSoundHookInstalled) {
        g_originalStartStaticSound=reinterpret_cast<StartStaticSoundFn>((DWORD)g_hHW_DLL+kStartStaticSoundRva);
        if (WriteCallTarget(g_staticSoundPatch,reinterpret_cast<void *>(&Hook_StartStaticSound)))
            g_staticSoundHookInstalled=true;
        else g_originalStartStaticSound=nullptr;
    }
}

void GrenadeProbeRemoveEngineHooks() {
    RestoreCall(g_eventPatch); RestoreCall(g_reliablePatch); RestoreCall(g_soundPatch); RestoreCall(g_staticSoundPatch);
    g_originalQueueEvent=nullptr; g_originalStartDynamicSound=nullptr; g_originalStartStaticSound=nullptr;
    g_eventHooksInstalled=false; g_soundHookInstalled=false; g_staticSoundHookInstalled=false;
}
