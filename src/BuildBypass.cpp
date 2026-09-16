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

// one-shot diagnostic: log the first bytes at a resolved game address so the
// exact build-specific code can be analysed offline (see docs workflow).
static bool SafeReadCode(void* addr, unsigned char* buf) // POD-only for SEH
{
    __try
    {
        memcpy(buf, addr, 64);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void DumpCodeAt(const char* name, void* addr)
{
    if (!addr)
    {
        DebugLog(std::string("KenshiTrainer: code@") + name + " = NULL");
        return;
    }
    unsigned char buf[64];
    if (!SafeReadCode(addr, buf))
    {
        DebugLog(std::string("KenshiTrainer: code@") + name + " read failed");
        return;
    }
    char tmp[8];
    std::string hex;
    char addrbuf[24];
    sprintf_s(addrbuf, "%p ", addr);
    hex = addrbuf;
    for (int i = 0; i < (int)sizeof(buf); ++i)
    {
        sprintf_s(tmp, "%02X", buf[i]);
        hex += tmp;
    }
    DebugLog(std::string("KenshiTrainer: code@") + name + " " + hex);
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
    // placementVerification is VIRTUAL: &Class::placementVerification makes the
    // compiler synthesize a local vtable-dispatch thunk, so GetRealAddress gets
    // an address in our own module and asserts (Functions.cpp:174). The _NV_
    // variant is a plain non-virtual import stub for the same RVA (0x4DCAC0);
    // with LTCG enabled its address folds to the KenshiLib import. Same x64
    // calling convention: rcx = this, bool returned in al.
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&PreviewBuilding::_NV_placementVerification),
            &PlacementVerification_Hook, &g_origPlacementVerification))
        DebugLog("KenshiTrainer: placementVerification hook FAILED");
    else
        DebugLog("KenshiTrainer: placementVerification hook installed");
        // diagnostics: dump the real build-specific code around the placement
        // entry points (helps locate the separate town-distance refusal check)
        DumpCodeAt("placementVerification", (void*)g_origPlacementVerification);
        DumpCodeAt("buildingPlacementUpdate",
                   (void*)KenshiLib::GetRealAddress(&PreviewBuilding::_NV_buildingPlacementUpdate));
        DumpCodeAt("placeFinalPreviewBuilding",
                   (void*)KenshiLib::GetRealAddress(&PreviewBuilding::_NV_placeFinalPreviewBuilding));
        DumpCodeAt("figureOutWhichTown",
                   (void*)KenshiLib::GetRealAddress(&PreviewBuilding::figureOutWhichTown));
}
