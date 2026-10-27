// Override only the outputs of the game's appearance calculation. Never
// substitute CharStats, even temporarily. Requests from Present are consumed
// by the game's own appearance update; no OGRE calls run on the UI thread.
// Entries belong to an AppearanceHuman lifetime and are erased on destruction.
#include <Debug.h>
#include <core/Functions.h>
#include <kenshi/Character.h>
#include <kenshi/Appearance.h>
#include <kenshi/RaceData.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <float.h>
#include <cstdio>
#include <map>
#include "PhysiqueControl.h"

struct PhysiqueEntry
{
    Character* owner;
    PhysiqueValues values;
    bool enabled;
    bool pending;
    unsigned int revision;
};

static SRWLOCK g_physiqueMutex = SRWLOCK_INIT;
static std::map<AppearanceHuman*, PhysiqueEntry> g_physiques;
static volatile LONG g_physiqueReady = 0;
static volatile LONG g_physiqueFaultLogged = 0;
static unsigned int g_physiqueRevision = 0;
static void (*g_origMuscleMults)(AppearanceHuman*) = NULL;
static void (*g_origFromStats)(AppearanceHuman*) = NULL;
static void (*g_origDestroyAppearance)(AppearanceBase*) = NULL;

class PhysiqueGuard
{
public:
    PhysiqueGuard() { AcquireSRWLockExclusive(&g_physiqueMutex); }
    ~PhysiqueGuard() { ReleaseSRWLockExclusive(&g_physiqueMutex); }
private:
    PhysiqueGuard(const PhysiqueGuard&);
    PhysiqueGuard& operator=(const PhysiqueGuard&);
};

bool PhysiqueControl_Ready()
{
    return InterlockedCompareExchange(&g_physiqueReady, 0, 0) != 0;
}

static bool ValidValues(const PhysiqueValues& values)
{
    return _finite(values.bulk) && values.bulk >= 0.0f && values.bulk <= 1.0f &&
        _finite(values.muscle) && values.muscle >= 0.0f && values.muscle <= 1.0f &&
        _finite(values.skinny) && values.skinny >= 0.0f && values.skinny <= 1.0f;
}

static AppearanceHuman* SafeHumanAppearance(Character* character, PhysiqueValues* values)
{
    __try
    {
        if (!character || !character->isHuman())
            return NULL;
        AppearanceHuman* appearance = (AppearanceHuman*)character->getAppearance();
        // These values belong to AppearanceHuman, not the streamed OGRE body.
        // Keep the editor available while a body is loading or supplied by a
        // modded humanoid; the normal game update applies bones when ready.
        if (!appearance)
            return NULL;
        values->bulk = appearance->bulkMult;
        values->muscle = appearance->muscleMult;
        values->skinny = appearance->skinnyMult;
        // Mods may drive values outside the vanilla range; still expose the
        // controls so the user can replace them with a valid 0..1 setting.
        return _finite(values->bulk) && _finite(values->muscle) &&
            _finite(values->skinny) ? appearance : NULL;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

static bool ReadEntry(AppearanceHuman* appearance, PhysiqueEntry* entry)
{
    PhysiqueGuard guard;
    std::map<AppearanceHuman*, PhysiqueEntry>::const_iterator it = g_physiques.find(appearance);
    if (it == g_physiques.end())
        return false;
    *entry = it->second;
    return true;
}

bool PhysiqueControl_Read(Character* character, PhysiqueValues* values,
    bool* locked, AppearanceHuman** identity)
{
    *locked = false;
    *identity = SafeHumanAppearance(character, values);
    if (!*identity)
        return false;
    PhysiqueEntry entry;
    if (ReadEntry(*identity, &entry) && entry.owner == character && entry.enabled)
    {
        *locked = true;
    }
    return true;
}

bool PhysiqueControl_Set(Character* character, const PhysiqueValues& values, bool locked)
{
    if (!PhysiqueControl_Ready() || !ValidValues(values))
        return false;
    PhysiqueValues current;
    AppearanceHuman* appearance = SafeHumanAppearance(character, &current);
    if (!appearance)
        return false;
    {
        PhysiqueGuard guard;
        PhysiqueEntry& entry = g_physiques[appearance];
        entry.owner = character;
        entry.values = values;
        entry.enabled = locked;
        entry.pending = true;
        entry.revision = ++g_physiqueRevision;
    }
    char text[180];
    sprintf_s(text, "KenshiTrainer: physique queued character=%p appearance=%p lock=%d bulk=%.3f muscle=%.3f skinny=%.3f",
        character, appearance, locked ? 1 : 0, values.bulk, values.muscle, values.skinny);
    DebugLog(text);
    return true;
}

static void LogPhysiqueFault()
{
    InterlockedExchange(&g_physiqueReady, 0);
    if (InterlockedCompareExchange(&g_physiqueFaultLogged, 1, 0) == 0)
        DebugLog("KenshiTrainer: physique update fault; overrides disabled");
}

static void SafeMuscleMults(AppearanceHuman* self)
{
    __try
    {
        g_origMuscleMults(self);
        if (!PhysiqueControl_Ready())
            return;
        PhysiqueEntry entry;
        if (ReadEntry(self, &entry) && entry.enabled && self->me == entry.owner)
        {
            // Steam 1.0.65: bulk@1A0, muscle@1A4, skinny@1A8.
            // The multipliers are valid even while the body is streaming.
            // The caller next consumes these for textures and proportions.
            self->bulkMult = entry.values.bulk;
            self->muscleMult = entry.values.muscle;
            self->skinnyMult = entry.values.skinny;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { LogPhysiqueFault(); }
}

static void FinishRefresh(AppearanceHuman* self, unsigned int revision)
{
    PhysiqueGuard guard;
    std::map<AppearanceHuman*, PhysiqueEntry>::iterator it = g_physiques.find(self);
    if (it == g_physiques.end() || it->second.revision != revision)
        return;
    if (it->second.enabled)
        it->second.pending = false;
    else
        g_physiques.erase(it);
}

static void SafeFromStats(AppearanceHuman* self)
{
    __try
    {
        PhysiqueEntry request;
        bool refresh = PhysiqueControl_Ready() && ReadEntry(self, &request) &&
            request.pending && self->me == request.owner;
        g_origFromStats(self);
        if (refresh && PhysiqueControl_Ready() && self->body)
        {
            // The original refreshes bones automatically only for the player
            // faction. Explicitly refresh a pending edit for any selected human.
            self->updateProportions();
            self->forceUpdateAnimationTransforms();
            FinishRefresh(self, request.revision);
            char text[120];
            sprintf_s(text, "KenshiTrainer: physique applied appearance=%p revision=%u lock=%d",
                self, request.revision, request.enabled ? 1 : 0);
            DebugLog(text);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { LogPhysiqueFault(); }
}

static void ForgetAppearance(AppearanceHuman* self)
{
    PhysiqueGuard guard;
    if (g_physiques.erase(self))
        DebugLog("KenshiTrainer: physique released on appearance destruction");
}

static void SafeDestroyAppearance(AppearanceBase* self)
{
    // The base destructor body does not free this allocation; its caller
    // does. Clear even on unwinding, before the address can be reused.
    // Casting is only used as a map key: no derived members are dereferenced.
    __try { g_origDestroyAppearance(self); }
    __finally { ForgetAppearance((AppearanceHuman*)self); }
}

void PhysiqueControl_Install()
{
    bool ok = true;
    if (KenshiLib::AddHook(KenshiLib::GetRealAddress(&AppearanceHuman::updatePhysiqueMuscleMults),
        &SafeMuscleMults, &g_origMuscleMults) != KenshiLib::SUCCESS)
        ok = false;
    if (KenshiLib::AddHook(KenshiLib::GetRealAddress(&AppearanceHuman::updatePysiqueFromStats),
        &SafeFromStats, &g_origFromStats) != KenshiLib::SUCCESS)
        ok = false;
    // The Human deleting destructor inlines its trivial derived destructor
    // and calls the BASE body directly (verified in Steam 1.0.65). Hooking
    // AppearanceHuman::_DESTRUCTOR misses virtual deletion and leaks entries.
    if (KenshiLib::AddHook(KenshiLib::GetRealAddress(&AppearanceBase::_DESTRUCTOR),
        &SafeDestroyAppearance, &g_origDestroyAppearance) != KenshiLib::SUCCESS)
        ok = false;
    InterlockedExchange(&g_physiqueReady, ok ? 1 : 0);
    DebugLog(ok ? "KenshiTrainer: physique hooks ready (3/3)"
        : "KenshiTrainer: physique hooks failed; overrides disabled");
}
