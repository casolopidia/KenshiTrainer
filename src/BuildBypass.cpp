// ============================================================================
// BuildBypass.cpp - "disable building placement validity check" feature.
//
// PreviewBuilding::placementVerification() (kenshi/Building/Building.h,
// RVA 0x4DCAC0, vtable slot 0xC8) is the game's master gate for building
// placement: placementVerification_recurse() runs the slope/overlap/indoors/
// ground checks and sets the flag members that drive the red/green preview
// material. No subclass overrides it (walls/farms only differ by type()).
//
// The hook calls the original first so flags and preview colour stay
// truthful, then forces an allow while the toggle is on. The hook calls no
// game API itself, so no SafeXxx/SEH wrapper is needed.
// ============================================================================
#include <Debug.h>
#include <core/Functions.h>

#include <kenshi/Building/Building.h>

static bool g_buildBypass = false;

static bool (*g_origPlacementVerification)(PreviewBuilding*) = NULL;

static bool PlacementVerification_Hook(PreviewBuilding* self)
{
    bool ok = g_origPlacementVerification(self);
    if (g_buildBypass)
        return true;
    return ok;
}

bool BuildBypass_Enabled()
{
    return g_buildBypass;
}

void BuildBypass_Set(bool enabled)
{
    if (g_buildBypass != enabled)
    {
        g_buildBypass = enabled;
        DebugLog(std::string("KenshiTrainer: build placement bypass = ")
                 + (enabled ? "on" : "off"));
    }
}

void BuildBypass_Install()
{
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&PreviewBuilding::placementVerification),
            &PlacementVerification_Hook, &g_origPlacementVerification))
        DebugLog("KenshiTrainer: placementVerification hook FAILED");
    else
        DebugLog("KenshiTrainer: placementVerification hook installed");
}
