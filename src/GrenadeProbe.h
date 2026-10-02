#pragma once

struct cl_entity_s;
struct tempent_s;

enum GrenadeVisualType {
    GRENADE_VIS_SMOKE = 1,
    GRENADE_VIS_FLASH = 2,
    GRENADE_VIS_HE = 3
};

struct GrenadeVisualEvent {
    int type;
    float origin[3];
    double startTime;
    double endTime;
};

void GrenadeProbeReset();
void GrenadeProbeObserveEntity(struct cl_entity_s *ent, const char *modelname);
void GrenadeProbeFrame(double clientTime);
int GrenadeProbeGetVisualEvents(GrenadeVisualEvent *outEvents, int maxEvents, double clientTime);

int Hook_HUD_AddEntity(int type, struct cl_entity_s *ent, const char *modelname);
void Hook_HUD_TempEntUpdate(double frametime, double client_time, double cl_gravity,
                            struct tempent_s **ppTempEntFree, struct tempent_s **ppTempEntActive,
                            int (*Callback_AddVisibleEntity)(struct cl_entity_s *pEntity),
                            void (*Callback_TempEntPlaySound)(struct tempent_s *pTemp, float damp));

void GrenadeProbeInstallEngineHooks();
void GrenadeProbeRemoveEngineHooks();
