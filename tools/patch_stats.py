p = "src/KenshiTrainer.cpp"
s = open(p, "r", encoding="ascii").read()

old1 = 'static float       g_statVals[64] = { 0 };'
new1 = 'static float       g_statVals[64] = { 0 };\nstatic char        g_statEdit[64][16] = { 0 };'
assert old1 in s, "old1"
s = s.replace(old1, new1)

old2 = '''    for (int i = 0; i < TR_STATS_COUNT; ++i)
        g_statVals[i] = (c && c->stats) ? ReadStatFast(c->stats, i) : 0.0f;'''
new2 = '''    for (int i = 0; i < TR_STATS_COUNT; ++i)
    {
        g_statVals[i] = (c && c->stats) ? ReadStatFast(c->stats, i) : 0.0f;
        sprintf_s(g_statEdit[i], "%.0f", g_statVals[i]);
    }'''
assert old2 in s, "old2"
s = s.replace(old2, new2)

old3 = '''        if (ImGui::InputFloat("##val", &g_statVals[i], 0.0f, 0.0f, "%.0f"))
            ApplyStat(i);
        ImGui::SameLine();
        if (ImGui::SmallButton(TR::BTN_SET))
            ApplyStat(i);'''
new3 = '''        // plain text edit: typing only changes the string; the game is touched
        // only on Enter / clicking away / the set button
        ImGui::InputText("##val", g_statEdit[i], sizeof(g_statEdit[i]),
            ImGuiInputTextFlags_CharsDecimal);
        bool apply = ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SameLine();
        if (ImGui::SmallButton(TR::BTN_SET))
            apply = true;
        if (apply)
        {
            float v = (float)atof(g_statEdit[i]);
            g_statVals[i] = v;
            ApplyStat(i);
            sprintf_s(g_statEdit[i], "%.0f", g_statVals[i]);
        }'''
assert old3 in s, "old3"
s = s.replace(old3, new3)

old4 = '''        if (g_selQuality > 0)
            g_gearLevel = g_selQuality - 1;'''
new4 = '''        if (g_selQuality > 0)
            g_gearLevel = (g_selQuality - 1) * 20; // grade -> level_0_100 scale'''
assert old4 in s, "old4"
s = s.replace(old4, new4)

old5 = '''        GameData* m = CurrentVariant();
        if (m && weaponLike)
        {
            int lv = -1;
            if (SafeGetIData(m, &g_lvlKey, &lv))
                g_weaponLevel = lv;
        }'''
new5 = '''        GameData* m = CurrentVariant();
        if (!m && weaponLike && !g_variants.empty())
            m = g_variants[0]; // default model: show its level right away
        if (m && weaponLike)
        {
            int lv = -1;
            if (SafeGetIData(m, &g_lvlKey, &lv))
                g_weaponLevel = lv;
        }'''
assert old5 in s, "old5"
s = s.replace(old5, new5)

open(p, "w", encoding="ascii").write(s)
print("patched ok")