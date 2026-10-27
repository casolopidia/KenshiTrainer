// XP entry points supply the character; increaseStat supplies the XP amount.
// A thread-local scope joins them without changing shared race/global data.
// See docs/03-RE_Kenshi injection notes for the verified 1.0.65 call graph.
#include <Debug.h>
#include <core/Functions.h>
#include <kenshi/Globals.h>
#include <kenshi/CharStats.h>
#include <kenshi/Character.h>
#include <kenshi/Faction.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <float.h>
#include <cstdio>
#include "ExperienceMultiplier.h"

static volatile LONG g_xpConfig = 100 | (1 << 14);
static volatile LONG g_xpReady = 0;
static volatile LONG g_xpScopeLogged[2] = { 0, 0 };
static volatile LONG g_xpReadFailureLogged = 0;
static __declspec(thread) float g_xpMultiplier = 1.0f;

bool ExperienceMultiplier_Ready()
{
    return InterlockedCompareExchange(&g_xpReady, 0, 0) != 0;
}

void ExperienceMultiplier_Configure(int percent, bool player, bool ai)
{
    if (percent < 0) percent = 0;
    if (percent > 100000) percent = 100000;
    LONG value = percent | (player ? (1 << 28) : 0) | (ai ? (1 << 29) : 0);
    if (InterlockedExchange(&g_xpConfig, value) != value)
    {
        InterlockedExchange(&g_xpScopeLogged[0], 0);
        InterlockedExchange(&g_xpScopeLogged[1], 0);
        char text[160];
        sprintf_s(text, "KenshiTrainer: XP config multiplier=%.2f player=%d ai=%d",
            percent / 100.0f, player ? 1 : 0, ai ? 1 : 0);
        DebugLog(text);
    }
}

static int SafeXpFaction(CharStats* stats)
{
    __try
    {
        if (!stats || !stats->me)
            return -1;
        Faction* faction = stats->me->getFaction();
        if (!faction)
            return -1;
        // Matches vanilla xpRunning's player-faction check, not selection,
        // allegiance, allied status or a character's current AI task.
        return faction->isPlayer ? 0 : 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

static float XpMultiplierFor(CharStats* stats)
{
    if (!ExperienceMultiplier_Ready())
        return 1.0f;
    LONG config = InterlockedCompareExchange(&g_xpConfig, 0, 0);
    int percent = config & 0x0fffffff;
    if (percent == 100 || (config & 0x30000000) == 0)
        return 1.0f;
    int faction = SafeXpFaction(stats);
    if (faction < 0)
    {
        if (InterlockedCompareExchange(&g_xpReadFailureLogged, 1, 0) == 0)
            DebugLog("KenshiTrainer: XP faction unavailable; keeping vanilla rate");
        return 1.0f;
    }
    if ((config & (1 << (28 + faction))) == 0)
        return 1.0f;
    if (InterlockedCompareExchange(&g_xpScopeLogged[faction], 1, 0) == 0)
        DebugLog(faction == 0 ? "KenshiTrainer: XP scope matched player faction"
            : "KenshiTrainer: XP scope matched AI faction");
    return percent / 100.0f;
}

// Only positive, finite XP is scaled. The original function still owns its
// level curve, training limit and invalid-amount checks.
static float ScaleXp(float amount, float multiplier)
{
    if (multiplier == 1.0f || !(amount > 0.0f) || !_finite(amount))
        return amount;
    float scaled = amount * multiplier;
    return _finite(scaled) ? scaled : amount;
}

static void (*g_origIncreaseStat)(float&, float, float) = NULL;
static void SafeIncreaseStat(float& stat, float amount, float limit)
{
    // Suspend the scope while invoking the downstream chain. A nested call
    // to the helper must not apply the multiplier for the same XP twice.
    float previous = g_xpMultiplier;
    g_xpMultiplier = 1.0f;
    __try { g_origIncreaseStat(stat, ScaleXp(amount, previous), limit); }
    __finally { g_xpMultiplier = previous; }
}

// POD-only scopes: restore context on normal return and SEH unwinding. Nested
// XP entry points REPLACE the multiplier, rather than multiplying it again.
#define XP_SCOPE_HOOK(name, signature, arguments) \
    static void (*g_orig##name) signature = NULL; \
    static void Safe##name signature \
    { \
        float previous = g_xpMultiplier; \
        g_xpMultiplier = XpMultiplierFor(self); \
        __try { g_orig##name arguments; } \
        __finally { g_xpMultiplier = previous; } \
    }

XP_SCOPE_HOOK(xpGeneral, (CharStats* self, float time, float mult, StatsEnumerated stat), (self, time, mult, stat))
XP_SCOPE_HOOK(xpTraining, (CharStats* self, float time, float mult, float& stat, float limit, StatsEnumerated which), (self, time, mult, stat, limit, which))
XP_SCOPE_HOOK(xpEngineering, (CharStats* self, float time), (self, time))
XP_SCOPE_HOOK(xpLockpicking, (CharStats* self, int level, bool success), (self, level, success))
XP_SCOPE_HOOK(xpDodgeEvent, (CharStats* self, float enemySkill, bool successful), (self, enemySkill, successful))
XP_SCOPE_HOOK(xpToughness_RagdollEvent, (CharStats* self), (self))
XP_SCOPE_HOOK(xpToughness_GetUpEvent, (CharStats* self), (self))
XP_SCOPE_HOOK(xpToughness_PunchSomething, (CharStats* self, HitMaterialType material), (self, material))
XP_SCOPE_HOOK(xpMelee, (CharStats* self, CharStats::DeadTimeState what, Character* target, const Damages& damage), (self, what, target, damage))
#undef XP_SCOPE_HOOK

// First aid adds XP directly; time is solely an XP factor in this function,
// not the healing operation. Keep the patient's identity and stat unchanged.
static void (*g_origFirstAid)(CharStats*, Character*, float, StatsEnumerated) = NULL;
static void SafeFirstAid(CharStats* self, Character* patient, float time, StatsEnumerated stat)
{
    float previous = g_xpMultiplier;
    float multiplier = XpMultiplierFor(self);
    g_xpMultiplier = 1.0f;
    __try { g_origFirstAid(self, patient, ScaleXp(time, multiplier), stat); }
    __finally { g_xpMultiplier = previous; }
}

static bool SafeReadMassCombat(CharStats* self, float* value)
{
    __try { *value = self->massCombat; return _finite(*value) != 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void SafeScaleMassCombat(CharStats* self, float before, float multiplier)
{
    __try
    {
        float after = self->massCombat;
        if (_finite(after) && after > before)
        {
            float adjusted = (float)(before + ((double)after - before) * multiplier);
            if (_finite(adjusted))
                self->massCombat = adjusted;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// This legacy field bypasses increaseStat and has no amount argument.
static void (*g_origMassCombat)(CharStats*) = NULL;
static void SafeMassCombat(CharStats* self)
{
    float previous = g_xpMultiplier;
    float multiplier = XpMultiplierFor(self);
    float before = 0.0f;
    bool adjust = multiplier != 1.0f && SafeReadMassCombat(self, &before);
    g_xpMultiplier = 1.0f;
    __try
    {
        g_origMassCombat(self);
        if (adjust)
            SafeScaleMassCombat(self, before, multiplier);
    }
    __finally { g_xpMultiplier = previous; }
}

void ExperienceMultiplier_Install()
{
    bool ok = true;
    // No literal RVAs: all targets resolve through the installed KenshiLib.
#define INSTALL_XP(target, detour, original) \
    if (KenshiLib::SUCCESS != KenshiLib::AddHook( \
        KenshiLib::GetRealAddress(target), detour, original)) \
    { \
        ok = false; \
        DebugLog("KenshiTrainer: XP hook failed: " #target); \
    }
#define INSTALL_SCOPE(name) INSTALL_XP(&CharStats::name, &Safe##name, &g_orig##name)
    INSTALL_XP(&increaseStat, &SafeIncreaseStat, &g_origIncreaseStat)
    INSTALL_SCOPE(xpGeneral)
    INSTALL_SCOPE(xpTraining)
    INSTALL_SCOPE(xpEngineering)
    INSTALL_SCOPE(xpLockpicking)
    INSTALL_SCOPE(xpDodgeEvent)
    INSTALL_SCOPE(xpToughness_RagdollEvent)
    INSTALL_SCOPE(xpToughness_GetUpEvent)
    INSTALL_SCOPE(xpToughness_PunchSomething)
    INSTALL_SCOPE(xpMelee)
    INSTALL_XP(&CharStats::xpFirstAid, &SafeFirstAid, &g_origFirstAid)
    INSTALL_XP(&CharStats::xpMassCombat, &SafeMassCombat, &g_origMassCombat)
#undef INSTALL_SCOPE
#undef INSTALL_XP
    // Partially installed hooks remain pass-through, never partial coverage.
    InterlockedExchange(&g_xpReady, ok ? 1 : 0);
    DebugLog(ok ? "KenshiTrainer: XP hooks ready (12/12)"
        : "KenshiTrainer: XP multiplier disabled after hook failure");
}
