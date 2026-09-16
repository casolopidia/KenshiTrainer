p = "src/KenshiTrainer.cpp"
s = open(p, "r", encoding="ascii").read()

old = '''static int SafeDataType(const GameData* g)'''
new = '''static bool SafeGetRefVals(const GameDataReference* r, int* a, int* b, int* c)
{
    __try { *a = r->values.value[0]; *b = r->values.value[1]; *c = r->values.value[2]; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static int SafeDataType(const GameData* g)'''
assert old in s
s = s.replace(old, new, 1)

old2 = '''    int t = SafeDataType(gd);
    if (t == WEAPON)
    {
        // weapons reference nothing: manufacturers and models are global lists
        EnsureGearCaches();
        g_companies = g_allManufacturers;
        g_variants = g_allWeaponModels;
    }
    // armour: variants = its material collection refs (filled by ClassifyRefs)
}'''
new2 = '''    int t = SafeDataType(gd);
    if (t == WEAPON)
    {
        // weapons reference nothing: manufacturers and models are global lists
        EnsureGearCaches();
        g_companies = g_allManufacturers;
        g_variants = g_allWeaponModels;
    }
    // armour: log its material collection entries + their level-range values,
    // to confirm the grade<->level mapping on this game build
    if (t == ARMOUR)
    {
        for (size_t i = 0; i < g_variants.size(); ++i)
        {
            GameData* col = g_variants[i];
            if (!col || SafeDataType(col) != MATERIAL_SPECS_COLLECTION)
                continue;
            std::string dbg = "KenshiTrainer material collection ";
            dbg += col->name;
            dbg += ": ";
            const Ogre::vector<GameDataReference>::type* refs = NULL;
            __try { refs = col->getReferenceList("specs"); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            if (!refs)
                __try { refs = col->getReferenceList("material specs"); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            if (refs)
            {
                for (size_t j = 0; j < refs->size(); ++j)
                {
                    GameData* e = SafeRefPtr(&(*refs)[j], &ou->gamedata);
                    int a = 0, b = 0, c = 0;
                    SafeGetRefVals(&(*refs)[j], &a, &b, &c);
                    char eb[160];
                    sprintf_s(eb, "[%s vals=%d/%d/%d] ", e ? e->name.c_str() : "?",
                        a, b, c);
                    dbg += eb;
                }
            }
            DebugLog(dbg);
        }
    }
}'''
assert old2 in s
s = s.replace(old2, new2)

open(p, "w", encoding="ascii").write(s)
print("diag ok")