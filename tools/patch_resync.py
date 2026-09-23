# -*- coding: utf-8 -*-
import io

path = r"src\Overlay.cpp"
with io.open(path, "rb") as f:
    data = f.read()

def rep(name, old, new):
    global data
    n = data.count(old)
    assert n == 1, "%s matched %d times" % (name, n)
    data = data.replace(old, new)

# 1. extend SCRec + forward-declare the resync
rep("struct", 
    b"struct SCRec { IDXGISwapChain* sc; void** shadowVt; void* origPresent; void* origPresent1; };\r\n",
    b"struct SCRec\r\n"
    b"{\r\n"
    b"    IDXGISwapChain* sc;\r\n"
    b"    void**          shadowVt;\r\n"
    b"    void*           origPresent;\r\n"
    b"    void*           origPresent1;\r\n"
    b"    void**          liveVt;   // game's live vtable at hook time (drift target)\r\n"
    b"    void*           live8;    // last-seen live slot values\r\n"
    b"    void*           live13;\r\n"
    b"    void*           live22;\r\n"
    b"};\r\n"
    b"static void ResyncShadowVtable(SCRec* rec);\r\n")

# 2. capture the rec during lookup
rep("lookup",
    b"    void* origFn = NULL;\r\n"
    b"    for (int i = 0; i < g_scRecCount; ++i)\r\n"
    b"        if (g_scRecs[i].sc == sc)\r\n"
    b"        {\r\n"
    b"            origFn = isPresent1 ? g_scRecs[i].origPresent1 : g_scRecs[i].origPresent;\r\n"
    b"            break;\r\n"
    b"        }\r\n",
    b"    SCRec* rec = NULL;\r\n"
    b"    void* origFn = NULL;\r\n"
    b"    for (int i = 0; i < g_scRecCount; ++i)\r\n"
    b"        if (g_scRecs[i].sc == sc)\r\n"
    b"        {\r\n"
    b"            rec = &g_scRecs[i];\r\n"
    b"            origFn = isPresent1 ? g_scRecs[i].origPresent1 : g_scRecs[i].origPresent;\r\n"
    b"            break;\r\n"
    b"        }\r\n")

# 3. resync under the frame lock, before rendering
rep("callsite",
    b"        __try { RenderOverlay(sc); }\r\n",
    b"        __try { ResyncShadowVtable(rec); RenderOverlay(sc); }\r\n")

# 4. resync implementation after Present1_Detour
rep("impl",
    b"    return PresentCommon(sc, sync, flags, params, true);\r\n}\r\n",
    b"    return PresentCommon(sc, sync, flags, params, true);\r\n}\r\n"
    b"\r\n"
    b"// Detect hookers that patched the LIVE vtable after our shadow swap (Dust\r\n"
    b"// writes its detours into the live table at device-capture time, which is\r\n"
    b"// later than our CreateSwapChain-time shadow install; without this its\r\n"
    b"// Present never fires and its overlay silently dies). Resync = re-copy the\r\n"
    b"// live table into our shadow and chain our hook to whatever the live slot\r\n"
    b"// holds now, so both load orders compose into a single chain.\r\n"
    b"static void ResyncShadowVtable(SCRec* rec)\r\n"
    b"{\r\n"
    b"    if (!rec || !rec->liveVt)\r\n"
    b"        return;\r\n"
    b"    void* live8 = NULL;\r\n"
    b"    void* live13 = NULL;\r\n"
    b"    void* live22 = NULL;\r\n"
    b"    __try\r\n"
    b"    {\r\n"
    b"        live8 = rec->liveVt[8];\r\n"
    b"        live13 = rec->liveVt[13];\r\n"
    b"        live22 = rec->liveVt[22];\r\n"
    b"    }\r\n"
    b"    __except (EXCEPTION_EXECUTE_HANDLER) { return; }\r\n"
    b"\r\n"
    b"    if (live8 == rec->live8 && live13 == rec->live13 && live22 == rec->live22)\r\n"
    b"        return; // no drift\r\n"
    b"\r\n"
    b"    rec->origPresent = live8;\r\n"
    b"    rec->origPresent1 = live22;\r\n"
    b"    __try\r\n"
    b"    {\r\n"
    b"        memcpy(rec->shadowVt, rec->liveVt, sizeof(void*) * 32);\r\n"
    b"        rec->shadowVt[8] = (void*)&Present_Detour;\r\n"
    b"        rec->shadowVt[22] = (void*)&Present1_Detour;\r\n"
    b"    }\r\n"
    b"    __except (EXCEPTION_EXECUTE_HANDLER) { return; }\r\n"
    b"    rec->live8 = live8;\r\n"
    b"    rec->live13 = live13;\r\n"
    b"    rec->live22 = live22;\r\n"
    b"    DebugLog(\"KenshiTrainer: shadow vtable resynced (live-vtable hooker detected)\");\r\n"
    b"}\r\n")

# 5. snapshot the live table at hook time
rep("snapshot",
    b"    g_scRecs[g_scRecCount].origPresent = shadow[8];\r\n"
    b"    g_scRecs[g_scRecCount].origPresent1 = shadow[22];\r\n",
    b"    g_scRecs[g_scRecCount].origPresent = shadow[8];\r\n"
    b"    g_scRecs[g_scRecCount].origPresent1 = shadow[22];\r\n"
    b"    g_scRecs[g_scRecCount].liveVt = origVt;\r\n"
    b"    g_scRecs[g_scRecCount].live8 = shadow[8];\r\n"
    b"    g_scRecs[g_scRecCount].live13 = shadow[13];\r\n"
    b"    g_scRecs[g_scRecCount].live22 = shadow[22];\r\n")

with io.open(path, "wb") as f:
    f.write(data)
print("Overlay.cpp: 5 resync edits applied")