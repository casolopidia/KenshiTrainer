p = "src/KenshiTrainer.cpp"
s = open(p, "r", encoding="ascii").read()

# helper: protected getReferenceList
old = '''static bool SafeGetRefVals(const GameDataReference* r, int* a, int* b, int* c)'''
new = '''static const Ogre::vector<GameDataReference>::type* SafeGetRefList(GameData* g, const std::string* list)
{
    __try { return g->getReferenceList(*list); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

static bool SafeGetRefVals(const GameDataReference* r, int* a, int* b, int* c)'''
assert old in s
s = s.replace(old, new, 1)

# move the armour dump out of SelectItem into its own function
i = s.find("    // armour: log its material collection entries")
j = s.find("\n}", i)
assert i > 0 and j > i
block = s[i:j]
s = s[:i] + "    if (t == ARMOUR)\n        DumpArmourCollections();" + s[j:]

# insert DumpArmourCollections before SelectItem
anchor = "// called when the selected item changes"
fn = '''// diagnostic: armour grade <-> level mapping, read from the game's data
static void DumpArmourCollections()
{
    if (!ou)
        return;
    for (size_t i = 0; i < g_variants.size(); ++i)
    {
        GameData* col = g_variants[i];
        if (!col || SafeDataType(col) != MATERIAL_SPECS_COLLECTION)
            continue;
        std::string dbg = "KenshiTrainer material collection ";
        dbg += col->name;
        dbg += ": ";
        static const std::string specsKey("specs");
        static const std::string msKey("material specs");
        const Ogre::vector<GameDataReference>::type* refs = SafeGetRefList(col, &specsKey);
        if (!refs)
            refs = SafeGetRefList(col, &msKey);
        if (refs)
        {
            for (size_t k = 0; k < refs->size(); ++k)
            {
                GameData* e = SafeRefPtr(&(*refs)[k], &ou->gamedata);
                int a = 0, b = 0, c = 0;
                SafeGetRefVals(&(*refs)[k], &a, &b, &c);
                char eb[200];
                sprintf_s(eb, "[%s vals=%d/%d/%d] ", e ? e->name.c_str() : "?", a, b, c);
                dbg += eb;
            }
        }
        DebugLog(dbg);
    }
}

''' + anchor
assert anchor in s
s = s.replace(anchor, fn, 1)

open(p, "w", encoding="ascii").write(s)
print("restructured")