// ============================================================================
// KenshiTrainer - Kenshi in-game trainer plugin (RE_Kenshi + KenshiLib)
// UI: Dear ImGui rendered over the game's D3D11 swapchain (see Overlay.cpp).
// Self-contained: no Dust, no MyGUI widgets.
//
// Features: stats editor, rename, money, item spawner (weapons incl. modded,
// with manufacturer/model; armour with material/quality), all through the
// game's own data + factory/ctor calls.
// ============================================================================
#include <Debug.h>
#include <core/Functions.h>

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Character.h>
#include <kenshi/CharStats.h>
#include <kenshi/Enums.h>
#include <kenshi/GameData.h>
#include <kenshi/GameDataManager.h>
#include <kenshi/RootObjectFactory.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/Gear.h>
#include <kenshi/Faction.h>
#include <kenshi/util/hand.h>
#include <kenshi/util/lektor.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Platoon.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mygui/MyGUI_PointerManager.h>
#include <mygui/MyGUI_Gui.h>

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "StatNames.h"      // TR_STATS (needs Enums.h first)
#include "ChineseStrings.h" // TR::* UTF-8 hex-escaped strings
#include "Overlay.h"
#include "BuildBypass.h"

// ----------------------------------------------------------------------------
// Small helpers
// ----------------------------------------------------------------------------

static std::string ToLower(const std::string& s)
{
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i)
        r[i] = (char)tolower((unsigned char)r[i]);
    return r;
}

static Character* GetSelectedCharacterImpl()
{
    if (!ou || !ou->player)
        return NULL;
    return ou->player->selectedCharacter.getCharacter();
}

static Character* GetSelectedCharacter()
{
    __try { return GetSelectedCharacterImpl(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

// ----------------------------------------------------------------------------
// SEH-protected game calls. __try functions must not have locals with
// non-trivial destructors (C2712) - keep them POD-only, pass objects by pointer.
// ----------------------------------------------------------------------------

static bool SafeGetDataOfType(GameDataManager* gdm, itemType t, lektor<GameData*>* out)
{
    __try { gdm->getDataOfType(*out, t); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static GameData* SafeRefPtr(const GameDataReference* r, GameDataContainer* c)
{
    __try { if (r->ptr) return r->ptr; return r->getPtr(c); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

static const Ogre::vector<GameDataReference>::type* SafeGetRefList(GameData* g, const std::string* list)
{
    __try { return g->getReferenceList(*list); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

static bool SafeGetRefVals(const GameDataReference* r, int* a, int* b, int* c)
{
    __try { *a = r->values.value[0]; *b = r->values.value[1]; *c = r->values.value[2]; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static int SafeDataType(const GameData* g)
{
    __try { return (int)g->type; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

static bool SafeGetIData(const GameData* g, const std::string* key, int* out)
{
    __try
    {
        boost::unordered::unordered_map<std::string, int, boost::hash<std::string>,
            std::equal_to<std::string>, Ogre::STLAllocator<std::pair<const std::string, int>,
            Ogre::GeneralAllocPolicy> >::const_iterator it = g->idata.find(*key);
        if (it != g->idata.end()) { *out = it->second; return true; }
        return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeGetFData(const GameData* g, const std::string* key, float* out)
{
    __try
    {
        boost::unordered::unordered_map<std::string, float, boost::hash<std::string>,
            std::equal_to<std::string>, Ogre::STLAllocator<std::pair<const std::string, float>,
            Ogre::GeneralAllocPolicy> >::const_iterator it = g->fdata.find(*key);
        if (it != g->fdata.end()) { *out = it->second; return true; }
        return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void GetNameImpl(Character* c, std::string* out)
{
    *out = c->getName(); // by-value return temp can't live inside __try (C2712)
}

static bool SafeGetName(Character* c, std::string* out)
{
    __try { GetNameImpl(c, out); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// createItem(gd, handle, weaponMesh, matData, levelOverride, flagUniform)
static Item* SafeCreateWeapon(const hand& owner, GameData* gd, GameData* model, GameData* company)
{
    __try { return ou->theFactory->createItem(gd, owner, model, company, -1, NULL); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

static Item* SafeCreateArmour(const hand& owner, GameData* gd, GameData* material, int level)
{
    __try { return ou->theFactory->createItem(gd, owner, NULL, material, level, NULL); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

static Item* SafeCreatePlainItem(const hand& owner, GameData* gd)
{
    __try { return ou->theFactory->createItem(gd, owner, NULL, NULL, -1, NULL); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

static bool SafeAddItem(Inventory* inv, Item* item, int qty)
{
    // dropOnFail=true: full backpack -> item drops on the ground, not deleted
    __try { return inv->addItem(item, qty, true, false); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void SafeGetQualityInfo(Item* item, std::string* mName, std::string* matName, int* lvl)
{
    __try
    {
        *lvl = -1;
        if (item->manufacturerData) *mName = item->manufacturerData->name;
        if (item->materialData) *matName = item->materialData->name;
        Gear* g = item->isGear();
        if (g) *lvl = g->getLevel();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static bool SafeSetStat(CharStats* st, int id, float v)
{
    __try { st->getStatRef((StatsEnumerated)id) = v; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static float SafeGetStat(CharStats* st, int id)
{
    __try { return st->getStat((StatsEnumerated)id, true); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
}

static bool SafeSetName(Character* c, const std::string* name)
{
    __try { c->setName(*name); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Money is faction-wide in Kenshi (CheatMenu uses getFaction + Ownerships too)
static Faction* GetPlayerFaction()
{
    if (!ou || !ou->player)
        return NULL;
    __try { return ou->player->getFaction(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

static bool SafeSetMoney(int amount)
{
    Faction* f = GetPlayerFaction();
    if (!f)
        return false;
    __try { f->factionOwnerships->setMoney(amount); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static int SafeGetMoney()
{
    Faction* f = GetPlayerFaction();
    if (!f)
        return -1;
    __try { return f->factionOwnerships->getMoney(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

// ----------------------------------------------------------------------------
// Item catalogue (runtime merged DB -> all enabled mods included)
// ----------------------------------------------------------------------------

static std::vector<GameData*> g_itemsCache;    // current category
static std::vector<GameData*> g_visibleItems;  // after search filter
static int                    g_curCat = 0;    // 0=all

static void GetCategoryTypes(int cat, itemType* out, int* outCount)
{
    switch (cat)
    {
    case 1: out[0] = WEAPON;    *outCount = 1; return;
    case 2: out[0] = ARMOUR;    *outCount = 1; return;
    case 3: out[0] = ITEM;      *outCount = 1; return;
    case 4: out[0] = CROSSBOW;  *outCount = 1; return;
    case 5: out[0] = CONTAINER; *outCount = 1; return;
    default:
        out[0] = WEAPON; out[1] = ARMOUR; out[2] = ITEM; out[3] = CROSSBOW; out[4] = CONTAINER;
        *outCount = 5;
        return;
    }
}

static bool GameDataNameLess(GameData* a, GameData* b)
{
    return _stricmp(a->name.c_str(), b->name.c_str()) < 0;
}

static void EnsureGearCaches(); // fwd

static void RebuildItemCache()
{
    g_itemsCache.clear();
    if (!ou)
        return;
    EnsureGearCaches();
    itemType types[8];
    int count = 0;
    GetCategoryTypes(g_curCat, types, &count);
    for (int t = 0; t < count; ++t)
    {
        lektor<GameData*> list;
        if (SafeGetDataOfType(&ou->gamedata, types[t], &list))
        {
            for (uint32_t i = 0; i < list.size(); ++i)
            {
                GameData* gd = list[i];
                if (gd && !gd->name.empty())
                    g_itemsCache.push_back(gd);
            }
        }
    }
    std::sort(g_itemsCache.begin(), g_itemsCache.end(), GameDataNameLess);
}

static void RefreshVisibleItems(const char* filterLower)
{
    g_visibleItems.clear();
    size_t shown = 0;
    for (size_t i = 0; i < g_itemsCache.size(); ++i)
    {
        GameData* gd = g_itemsCache[i];
        if (filterLower[0] && ToLower(gd->name).find(filterLower) == std::string::npos)
            continue;
        g_visibleItems.push_back(gd);
        if (++shown >= 1000) // cap: keep the UI responsive with huge modpacks
            break;
    }
}

// ----------------------------------------------------------------------------
// Manufacturer / model / quality variant selection
// ----------------------------------------------------------------------------

static GameData*              g_selItem = NULL;
static std::vector<GameData*> g_companies;      // WEAPON_MANUFACTURER
static std::vector<GameData*> g_variants;       // models (weapons) / material collections (armour)
static int                    g_selCompany = 0; // 0 = auto
static int                    g_selVariant = 0; // 0 = auto
static int                    g_weaponLevel = 0;  // weapon grade 0..100, 0 = game default
static int                    g_gearLevel = -1;   // armour/crossbow grade 0..100, -1 = auto

static std::vector<GameData*> g_allManufacturers;
static std::vector<GameData*> g_allWeaponModels;
static bool                   g_gearCacheBuilt = false;

// Weapon<->model association lives on the WEAPON_MANUFACTURER entries:
// each manufacturer holds a weapons list (WEAPON refs) and a models list
// (MATERIAL_SPECS_WEAPON refs); any weapon x model combination of that
// manufacturer can occur in game. Built from the runtime merged DB, so
// mod weapons/mod models are covered automatically.
// per-reference vals (TripleInt) from the data files (see fcs.def):
//   "weapon types"  WEAPON refs:               value[0] = chance
//   "weapon models" MATERIAL_SPECS_WEAPON refs: value[0] = weapon level [1-100],
//                                              value[1] = chance
struct ManufacturerInfo
{
    GameData*              gd;
    std::vector<GameData*> weapons;      // WEAPON refs ("weapon types" lists)
    std::vector<int>       weaponChance; // spawn chance per weapons ref
    std::vector<GameData*> models;       // MATERIAL_SPECS_WEAPON refs ("weapon models" lists)
    std::vector<int>       modelLevel;   // weapon level per model ref
    std::vector<int>       modelChance;  // spawn chance per model ref
};
static std::vector<ManufacturerInfo> g_mfrInfos;

static bool ListHas(const std::vector<GameData*>& v, GameData* g)
{
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i] == g)
            return true;
    return false;
}

template<typename MapT>
static void ClassifyRefs(const MapT& m, GameDataContainer* src,
    std::vector<GameData*>* companies, std::vector<GameData*>* variants, std::string* dbg)
{
    for (typename MapT::const_iterator it = m.begin(); it != m.end(); ++it)
    {
        const Ogre::vector<GameDataReference>::type& vec = it->second;
        for (size_t i = 0; i < vec.size(); ++i)
        {
            GameData* ref = SafeRefPtr(&vec[i], src);
            if (!ref)
                continue;
            int t = SafeDataType(ref);
            char buf[64];
            sprintf_s(buf, "[%s -> type %d] ", it->first.c_str(), t);
            *dbg += buf;
            if (t == WEAPON_MANUFACTURER)
            {
                if (!ListHas(*companies, ref))
                    companies->push_back(ref);
            }
            else if (t == MATERIAL_SPECS_WEAPON || t == MATERIAL_SPECS_CLOTHING ||
                t == MATERIAL_SPEC || t == MATERIAL_SPECS_COLLECTION)
            {
                if (!ListHas(*variants, ref))
                    variants->push_back(ref);
            }
        }
    }
}

static std::string g_dbgRefs; // global to keep __try functions free of unwindable locals

static bool SafeClassifyRefs(GameData* gd, GameDataContainer* src)
{
    __try { ClassifyRefs(gd->objectReferences, src, &g_companies, &g_variants, &g_dbgRefs); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool ModelLevelLess(GameData* a, GameData* b)
{
    static const std::string lvlKey("level");
    int la = 0, lb = 0;
    SafeGetIData(a, &lvlKey, &la);
    SafeGetIData(b, &lvlKey, &lb);
    if (la != lb) return la < lb;
    return _stricmp(a->name.c_str(), b->name.c_str()) < 0;
}

static bool SafeBuildMfrInfo(ManufacturerInfo* mi); // defined below

static void EnsureGearCachesImpl()
{
    lektor<GameData*> l1;
    if (SafeGetDataOfType(&ou->gamedata, WEAPON_MANUFACTURER, &l1))
        for (uint32_t i = 0; i < l1.size(); ++i)
            if (l1[i] && !l1[i]->name.empty())
                g_allManufacturers.push_back(l1[i]);
    lektor<GameData*> l2;
    if (SafeGetDataOfType(&ou->gamedata, MATERIAL_SPECS_WEAPON, &l2))
        for (uint32_t i = 0; i < l2.size(); ++i)
            if (l2[i] && !l2[i]->name.empty())
                g_allWeaponModels.push_back(l2[i]);
    std::sort(g_allManufacturers.begin(), g_allManufacturers.end(), GameDataNameLess);
    std::sort(g_allWeaponModels.begin(), g_allWeaponModels.end(), ModelLevelLess);
    for (size_t i = 0; i < g_allManufacturers.size(); ++i)
    {
        ManufacturerInfo mi;
        mi.gd = g_allManufacturers[i];
        g_mfrInfos.push_back(mi);
    }
    for (size_t i = 0; i < g_mfrInfos.size(); ++i)
        SafeBuildMfrInfo(&g_mfrInfos[i]);
    char buf[96];
    sprintf_s(buf, "KenshiTrainer gear caches: manufacturers=%d models=%d",
        (int)g_allManufacturers.size(), (int)g_allWeaponModels.size());
    DebugLog(buf);
}

// classify one manufacturer's ref lists by element type; list key names are
// data-driven (lang-dependent), element types are not
template<typename MapT>
static void BuildMfrInfoImpl(ManufacturerInfo* mi, const MapT& m, GameDataContainer* src)
{
    for (typename MapT::const_iterator it = m.begin(); it != m.end(); ++it)
    {
        const Ogre::vector<GameDataReference>::type& vec = it->second;
        for (size_t i = 0; i < vec.size(); ++i)
        {
            GameData* ref = SafeRefPtr(&vec[i], src);
            int t = SafeDataType(ref);
            int a = 0, b = 0, c = 0;
            SafeGetRefVals(&vec[i], &a, &b, &c);
            if (t == WEAPON)
            {
                if (!ListHas(mi->weapons, ref))
                {
                    mi->weapons.push_back(ref);
                    mi->weaponChance.push_back(a);
                }
            }
            else if (t == MATERIAL_SPECS_WEAPON)
            {
                if (!ListHas(mi->models, ref))
                {
                    mi->models.push_back(ref);
                    // fcs.def: value[0] = level [1-100]; if it looks wrong,
                    // fall back to the first sane value in the triple
                    int lvl = a;
                    if (lvl < 1 || lvl > 100)
                        lvl = (b >= 1 && b <= 100) ? b : ((c >= 1 && c <= 100) ? c : 0);
                    mi->modelLevel.push_back(lvl);
                    mi->modelChance.push_back(b);
                }
            }
        }
    }
}

static void SafeBuildMfrInfoImpl(ManufacturerInfo* mi)
{
    BuildMfrInfoImpl(mi, mi->gd->objectReferences, &ou->gamedata);
}

static bool SafeBuildMfrInfo(ManufacturerInfo* mi)
{
    __try { SafeBuildMfrInfoImpl(mi); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static ManufacturerInfo* FindMfr(GameData* gd)
{
    for (size_t i = 0; i < g_mfrInfos.size(); ++i)
        if (g_mfrInfos[i].gd == gd)
            return &g_mfrInfos[i];
    return NULL;
}

static bool MfrMakesWeapon(ManufacturerInfo* mi, GameData* w)
{
    return mi && w && ListHas(mi->weapons, w);
}

// the weapon level a manufacturer assigns to a model (fcs.def "weapon models"
// value[0]); searches the current manufacturer scope for the model
static int MfrModelLevel(GameData* model)
{
    if (!model)
        return -1;
    ManufacturerInfo* scope = NULL;
    if (g_selCompany > 0 && g_selCompany <= (int)g_companies.size())
        scope = FindMfr(g_companies[g_selCompany - 1]);
    for (int pass = 0; pass < 2; ++pass)
    {
        if (pass == 1)
            scope = NULL; // second pass: search every manufacturer
        for (size_t i = 0; i < g_mfrInfos.size(); ++i)
        {
            ManufacturerInfo* mi = &g_mfrInfos[i];
            if (scope && mi != scope)
                continue;
            for (size_t k = 0; k < mi->models.size(); ++k)
                if (mi->models[k] == model)
                    return mi->modelLevel[k];
        }
    }
    return -1;
}

// rebuild the model combo for the current weapon + manufacturer selection
static void RebuildWeaponVariants()
{
    g_variants.clear();
    g_selVariant = 0;
    ManufacturerInfo* mi = NULL;
    if (g_selCompany > 0 && g_selCompany <= (int)g_companies.size())
        mi = FindMfr(g_companies[g_selCompany - 1]);
    if (mi && !mi->models.empty())
    {
        g_variants = mi->models;
    }
    else
    {
        // union of models from all manufacturers that make this weapon
        for (size_t i = 0; i < g_mfrInfos.size(); ++i)
        {
            if (!MfrMakesWeapon(&g_mfrInfos[i], g_selItem))
                continue;
            for (size_t k = 0; k < g_mfrInfos[i].models.size(); ++k)
                if (!ListHas(g_variants, g_mfrInfos[i].models[k]))
                    g_variants.push_back(g_mfrInfos[i].models[k]);
        }
        if (g_variants.empty())
            g_variants = g_allWeaponModels; // no association found: show everything
    }
    std::sort(g_variants.begin(), g_variants.end(), ModelLevelLess);
}

static void EnsureGearCaches()
{
    if (g_gearCacheBuilt || !ou)
        return;
    g_gearCacheBuilt = true;
    __try { EnsureGearCachesImpl(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { ErrorLog("KenshiTrainer: EnsureGearCaches crashed"); }
}

// ---------------------------------------------------------------------------
// one-shot data diagnostics: dump raw GameData property maps so the log shows
// where weapon levels / armour grades actually live on this game build
// ---------------------------------------------------------------------------
static std::string g_dbgDump;

template<typename MapT>
static void DumpIntMap(const MapT& m, const char* tag, std::string* dbg)
{
    char buf[384];
    sprintf_s(buf, "%s{", tag);
    *dbg += buf;
    for (typename MapT::const_iterator it = m.begin(); it != m.end(); ++it)
    {
        sprintf_s(buf, "%s=%d ", it->first.c_str(), it->second);
        *dbg += buf;
    }
    *dbg += "} ";
}

template<typename MapT>
static void DumpFloatMap(const MapT& m, const char* tag, std::string* dbg)
{
    char buf[384];
    sprintf_s(buf, "%s{", tag);
    *dbg += buf;
    for (typename MapT::const_iterator it = m.begin(); it != m.end(); ++it)
    {
        sprintf_s(buf, "%s=%.2f ", it->first.c_str(), it->second);
        *dbg += buf;
    }
    *dbg += "} ";
}

template<typename MapT>
static void DumpStrMap(const MapT& m, const char* tag, std::string* dbg)
{
    char buf[512];
    sprintf_s(buf, "%s{", tag);
    *dbg += buf;
    for (typename MapT::const_iterator it = m.begin(); it != m.end(); ++it)
    {
        sprintf_s(buf, "%s=%.100s ", it->first.c_str(), it->second.c_str());
        *dbg += buf;
    }
    *dbg += "} ";
}

template<typename MapT>
static void DumpRefLists(const MapT& m, GameDataContainer* src, std::string* dbg)
{
    char buf[512];
    for (typename MapT::const_iterator it = m.begin(); it != m.end(); ++it)
    {
        sprintf_s(buf, "list[%s]=%d: ", it->first.c_str(), (int)it->second.size());
        *dbg += buf;
        size_t n = it->second.size() < 4 ? it->second.size() : 4;
        for (size_t k = 0; k < n; ++k)
        {
            GameData* r = SafeRefPtr(&it->second[k], src);
            int a = 0, b = 0, c = 0;
            SafeGetRefVals(&it->second[k], &a, &b, &c);
            sprintf_s(buf, "%s(%d/%d/%d) ", r ? r->name.c_str() : "?", a, b, c);
            *dbg += buf;
        }
    }
}

static void DumpOneGameData(GameData* m, const char* what, GameDataContainer* src, bool withStr)
{
    if (!m)
        return;
    g_dbgDump += what;
    g_dbgDump += " ";
    g_dbgDump += m->name;
    g_dbgDump += ": ";
    DumpIntMap(m->idata, "idata", &g_dbgDump);
    DumpFloatMap(m->fdata, "fdata", &g_dbgDump);
    if (withStr)
        DumpStrMap(m->sdata, "sdata", &g_dbgDump);
    DumpRefLists(m->objectReferences, src, &g_dbgDump);
    DebugLog(g_dbgDump);
    g_dbgDump.clear();
}

static void DumpGearSampleImpl()
{
    if (!ou)
        return;
    DumpOneGameData(g_selItem, "armour", &ou->gamedata, true);
    size_t n = g_variants.size() < 3 ? g_variants.size() : 3;
    for (size_t k = 0; k < n; ++k)
        DumpOneGameData(g_variants[k], "gear-material", &ou->gamedata, false);
}

static void DumpGearSample()
{
    __try { DumpGearSampleImpl(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { ErrorLog("KenshiTrainer: gear sample dump crashed"); }
}

static bool g_weaponDumpDone = false;

static void DumpWeaponSampleImpl()
{
    if (!ou)
        return;
    // one compact line per manufacturer so the log shows whether the
    // weapons/models association parsed correctly on this build
    for (size_t i = 0; i < g_mfrInfos.size(); ++i)
    {
        ManufacturerInfo* mi = &g_mfrInfos[i];
        g_dbgDump = "KenshiTrainer mfr '";
        g_dbgDump += mi->gd->name;
        g_dbgDump += "' weapons=";
        char buf[256];
        sprintf_s(buf, "%d(", (int)mi->weapons.size());
        g_dbgDump += buf;
        for (size_t k = 0; k < mi->weapons.size() && k < 3; ++k)
        {
            g_dbgDump += mi->weapons[k]->name;
            g_dbgDump += ";";
        }
        sprintf_s(buf, ") models=%d(", (int)mi->models.size());
        g_dbgDump += buf;
        for (size_t k = 0; k < mi->models.size() && k < 3; ++k)
        {
            g_dbgDump += mi->models[k]->name;
            sprintf_s(buf, "[lv%d/ch%d];", mi->modelLevel[k], mi->modelChance[k]);
            g_dbgDump += buf;
        }
        g_dbgDump += ")";
        DebugLog(g_dbgDump);
        g_dbgDump.clear();
    }
}

static void DumpWeaponSample()
{
    if (g_weaponDumpDone)
        return;
    g_weaponDumpDone = true;
    __try { DumpWeaponSampleImpl(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { ErrorLog("KenshiTrainer: weapon sample dump crashed"); }
}

// called when the selected item changes
static void SelectItem(GameData* gd)
{
    g_selItem = gd;
    g_companies.clear();
    g_variants.clear();
    g_selCompany = 0;
    g_selVariant = 0;
    g_weaponLevel = 0;   // weapon grade 0..100, 0 = game default
    g_gearLevel = -1;    // armour/crossbow grade 0..100, -1 = auto
    if (!gd || !ou)
        return;

    g_dbgRefs = "KenshiTrainer refs of ";
    g_dbgRefs += gd->name;
    g_dbgRefs += ": ";
    if (!SafeClassifyRefs(gd, &ou->gamedata))
        g_dbgRefs += "<crash>";
    DebugLog(g_dbgRefs);

    int t = SafeDataType(gd);
    if (t == WEAPON)
    {
        EnsureGearCaches();
        // manufacturers claiming this weapon = its weapons list contains it.
        // A manufacturer with models but an empty weapons list (e.g. Cross /
        // 铭物 - unique weapons are placed directly, never random-spawned)
        // is universal: any weapon x its model combination is valid.
        g_companies.clear();
        for (size_t i = 0; i < g_mfrInfos.size(); ++i)
        {
            bool claims = !g_mfrInfos[i].weapons.empty() && ListHas(g_mfrInfos[i].weapons, gd);
            bool universal = g_mfrInfos[i].weapons.empty() && !g_mfrInfos[i].models.empty();
            if (claims || universal)
                g_companies.push_back(g_mfrInfos[i].gd);
        }
        if (g_companies.empty())
        {
            char buf[256];
            sprintf_s(buf, "KenshiTrainer: no manufacturer claims weapon '%.80s', showing all",
                gd->name.c_str());
            DebugLog(buf);
            g_companies = g_allManufacturers;
        }
        RebuildWeaponVariants();
        DumpWeaponSample();
    }
    if (t == ARMOUR)
        DumpGearSample();
}

// ----------------------------------------------------------------------------
// Item creation
// ----------------------------------------------------------------------------

static GameData* CurrentCompany()
{
    return (g_selCompany > 0 && g_selCompany <= (int)g_companies.size())
        ? g_companies[g_selCompany - 1] : NULL;
}

static GameData* CurrentVariant()
{
    return (g_selVariant > 0 && g_selVariant <= (int)g_variants.size())
        ? g_variants[g_selVariant - 1] : NULL;
}

// weapons fail with null manufacturer/model: resolve safe defaults from the
// current association scope (companies/variants already filtered per weapon)
static GameData* DefaultCompany()
{
    const std::vector<GameData*>& pool =
        g_companies.empty() ? g_allManufacturers : g_companies;
    if (pool.empty())
        return NULL;
    for (size_t i = 0; i < pool.size(); ++i)
        if (ToLower(pool[i]->name).find("homemade") != std::string::npos)
            return pool[i];
    // prefer a manufacturer that actually has models for this weapon
    for (size_t i = 0; i < pool.size(); ++i)
    {
        ManufacturerInfo* mi = FindMfr(pool[i]);
        if (mi && !mi->models.empty())
            return pool[i];
    }
    return pool[0];
}

static GameData* DefaultModel()
{
    return g_variants.empty() ? NULL : g_variants[0]; // lowest level = safest
}

// Direct construction - this is how CheatMenu.dll spawns graded weapons:
// Sword(baseData, companyData=manufacturer, materialData=model, handle, level)
static Sword* NewSwordImpl(GameData* gd, GameData* company, GameData* material, hand h, int level)
{
    return new Sword(gd, company, material, h, level); // 'new' can't live inside __try (C2712)
}

static Sword* SafeCreateSword(GameData* gd, GameData* company, GameData* material, const hand* h, int level)
{
    __try { return NewSwordImpl(gd, company, material, *h, level); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

static Item* CreateWeaponRobust(const hand& owner, GameData* gd, GameData* model, GameData* company)
{
    if (!model) model = DefaultModel();
    if (!company) company = DefaultCompany();
    int lvl = g_weaponLevel;
    if (lvl < 0) lvl = 0; // CheatMenu semantics: 0 = game default grade
    if (lvl > 100) lvl = 100;
    Item* it = SafeCreateSword(gd, company, model, &owner, lvl);
    if (!it) it = SafeCreateWeapon(owner, gd, model, company); // factory path
    if (!it) it = SafeCreateWeapon(owner, gd, company, model); // swapped mapping
    if (!it) it = SafeCreatePlainItem(owner, gd);              // bare fallback
    return it;
}

// Matches CheatMenu.dll's proven paths exactly:
//   weapons:   new Sword(gd, manufacturer, model, nullHand, level)
//   armour/xbow: createItem(gd, nullHand, NULL, NULL, gradeLevel, NULL)
//   plain:     createItem(gd, nullHand, NULL, NULL, -1, NULL)
static Item* CreateConfiguredItem(GameData* gd)
{
    hand nullHand;
    int t = SafeDataType(gd);
    if (t == WEAPON)
        return CreateWeaponRobust(nullHand, gd, CurrentVariant(), CurrentCompany());
    if (t == CROSSBOW || t == ARMOUR)
        return SafeCreateArmour(nullHand, gd, NULL, g_gearLevel); // -1 = auto
    return SafeCreatePlainItem(nullHand, gd);
}

// ----------------------------------------------------------------------------
// UI state + actions
// ----------------------------------------------------------------------------

static bool        g_showMenu = false;
static std::string g_status;
static int         g_qty = 1;
static char        g_searchBuf[128] = { 0 };
static char        g_nameBuf[128] = { 0 };
static int         g_moneyVal = 0;
static float       g_statVals[64] = { 0 };
static char        g_statEdit[64][16] = { 0 };
static bool        g_statsDirty = true; // refill g_statVals on next draw
static int         g_headerTimer = 0;
static bool        g_cursorHidden = false;

static void SetStatus(const std::string& msg)
{
    g_status = msg;
    DebugLog(std::string("KenshiTrainer: ") + msg);
}

static void DrawBuildTab()
{
    bool bypass = BuildBypass_Enabled();
    if (ImGui::Checkbox(TR::LBL_BUILD_BYPASS, &bypass))
    {
        BuildBypass_Set(bypass);
        g_status = TR::LBL_BUILD_BYPASS;
    }
    ImGui::TextUnformatted(TR::HINT_BUILD_BYPASS);
}

bool Trainer_MenuOpen() { return g_showMenu; }

void DrawTrainerUI()
{
    // while our menu is open: game ignores world input (its own modal flag)
    // and the game's MyGUI cursor is hidden so the OS/ImGui cursor is usable
    __try
    {
        if (key)
            key->controlEnabled = !g_showMenu;
        if (g_cursorHidden != g_showMenu)
        {
            g_cursorHidden = g_showMenu;
            MyGUI::PointerManager::getInstance().setVisible(!g_showMenu);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    // entry window: draggable by title, click button toggles the panel
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(TR::APP_NAME, NULL,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoCollapse);
    if (ImGui::Button(g_showMenu ? "\xE5\x85\xB3\xE9\x97\xAD\xE9\x9D\xA2\xE6\x9D\xBF" : "\xE5\xBC\x80\xE5\x90\xAF\xE9\x9D\xA2\xE6\x9D\xBF"))
    {
        g_showMenu = !g_showMenu;
        if (g_showMenu)
            g_statsDirty = true;
    }
    ImGui::End();

    if (!g_showMenu)
        return;

    ImGui::SetNextWindowSize(ImVec2(620.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(220.0f, 80.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(TR::WINDOW_TITLE, &g_showMenu, 0))
    {
        if (ImGui::BeginTabBar("##tabs"))
        {
            if (ImGui::BeginTabItem(TR::TAB_STATS))
            {
                DrawStatsTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(TR::TAB_ITEMS))
            {
                DrawItemsTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(TR::TAB_BUILD))
            {
                DrawBuildTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::Separator();
        ImGui::TextUnformatted(g_status.c_str());
    }
    ImGui::End();
}

// ----------------------------------------------------------------------------
// Plugin entry
// ----------------------------------------------------------------------------

__declspec(dllexport) void startPlugin()
{
    BuildBypass_Install();
    Overlay_Init();
    DebugLog("KenshiTrainer plugin started");
}
