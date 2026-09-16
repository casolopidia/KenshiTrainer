// ============================================================================
// Overlay.cpp - DXGI/D3D11 hooks + ImGui overlay for KenshiTrainer
//
// Hook strategy mirrors the proven Dust mod (DustBoot.dll + Dust.dll):
//   * KenshiLib::AddHook on d3d11!D3D11CreateDeviceAndSwapChain
//   * a temp IDXGIFactory is created just to READ its vtable (never modified);
//     the CreateSwapChain / CreateSwapChainForHwnd *functions* are then hooked
//     with KenshiLib::AddHook (proper multi-plugin chaining)
//   * every swapchain born through those functions is captured; the largest one
//     is considered the game's
//   * Present is hooked by a PER-INSTANCE shadow vtable swap on the selected
//     swapchain only (Dust.dll does exactly this: "Present vtable-hooked on
//     swap chain %p") - other swapchains and the shared class vtable stay
//     untouched, nothing else in the process is affected
//
// Safety: message queue (ImGui is not thread-safe), render-thread frame lock,
// Present reentry circuit-breaker, SEH everywhere, fast text crash reports.
// ============================================================================
#include <Debug.h>
#include <core/Functions.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

// SDK 7.1 has no dxgi1_2.h; we only need these IIDs for QueryInterface.
static const GUID KT_IID_IDXGIFactory2 =
    { 0x50c431a1, 0x5e12, 0x4c0a, { 0x98, 0x4b, 0x0d, 0xa0, 0x0e, 0x32, 0x82, 0xdd } };

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include <string>

#include "Overlay.h"

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

static IDXGISwapChain* g_gameSC = NULL;      // swapchain we render to (largest seen)
static IDXGISwapChain* g_imguiSC = NULL;     // swapchain ImGui was initialised with
static bool            g_imguiReady = false;
static ID3D11Device*   g_dev = NULL;
static ID3D11DeviceContext* g_ctx = NULL;
static HWND            g_hwnd = NULL;
static WNDPROC         g_origWndProc = NULL;
static char            g_imguiIni[MAX_PATH] = { 0 };
static float           g_uiScale = 1.0f;
static LONG            g_frameLock = 0;
static LONG            g_inPresent = 0;
static bool            g_overlayEnabled = true;
static int             g_crashCount = 0;

float Overlay_GetUiScale() { return g_uiScale; }

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ---------------------------------------------------------------------------
// crash reports: fast text report (survives process death), minidump for fatal
// ---------------------------------------------------------------------------

typedef BOOL(WINAPI* MiniDumpWriteDump_t)(HANDLE, DWORD, HANDLE, int, void*, void*, void*);
static char g_dumpPath[MAX_PATH] = { 0 };

static bool SafeReadPtr(const DWORD_PTR* p, DWORD_PTR* out)
{
    __try { *out = *p; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

struct KT_EXCEPTION_POINTERS { void* ExceptionRecord; void* ContextRecord; };
struct KT_MINIDUMP_EXCEPTION_INFO { DWORD ThreadId; KT_EXCEPTION_POINTERS* ExceptionPointers; int ClientPointers; };

static void WriteCrashText(const char* tag, EXCEPTION_POINTERS* ep)
{
    if (!g_dumpPath[0] || !ep || !ep->ExceptionRecord || !ep->ContextRecord)
        return;
    std::string p = std::string(g_dumpPath) + tag + ".txt";
    HANDLE f = CreateFileA(p.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    std::string out;
    char buf[512];
    EXCEPTION_RECORD* er = ep->ExceptionRecord;
    CONTEXT* ctx = ep->ContextRecord;
    sprintf_s(buf, "code=%08X addr=%p rip=%p rsp=%p\r\n",
        er->ExceptionCode, er->ExceptionAddress, (void*)ctx->Rip, (void*)ctx->Rsp);
    out += buf;

    HMODULE mod = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)ctx->Rip, &mod) && mod)
    {
        char mp[MAX_PATH];
        GetModuleFileNameA(mod, mp, MAX_PATH);
        sprintf_s(buf, "crash module: %s + 0x%llX\r\n", mp,
            (unsigned long long)(ctx->Rip - (DWORD_PTR)mod));
        out += buf;
    }
    sprintf_s(buf, "rax=%p rbx=%p rcx=%p rdx=%p rsi=%p rdi=%p r8=%p r9=%p\r\nstack:\r\n",
        (void*)ctx->Rax, (void*)ctx->Rbx, (void*)ctx->Rcx, (void*)ctx->Rdx,
        (void*)ctx->Rsi, (void*)ctx->Rdi, (void*)ctx->R8, (void*)ctx->R9);
    out += buf;

    DWORD_PTR* sp = (DWORD_PTR*)ctx->Rsp;
    for (int i = 0; i < 512; ++i)
    {
        DWORD_PTR v = 0;
        if (!SafeReadPtr(&sp[i], &v))
            break;
        HMODULE m2 = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)v, &m2) && m2)
        {
            char mp[MAX_PATH];
            GetModuleFileNameA(m2, mp, MAX_PATH);
            const char* base = strrchr(mp, '\\');
            sprintf_s(buf, "  [rsp+%03X] %s+0x%llX\r\n", i * 8,
                base ? base + 1 : mp, (unsigned long long)(v - (DWORD_PTR)m2));
            out += buf;
        }
    }
    DWORD written = 0;
    WriteFile(f, out.c_str(), (DWORD)out.size(), &written, NULL);
    CloseHandle(f);
}

static void WriteCrashDumpEx(const char* tag, void* excPtrs)
{
    WriteCrashText(tag, (EXCEPTION_POINTERS*)excPtrs);
    HMODULE dbg = LoadLibraryA("dbghelp.dll");
    if (!dbg || !g_dumpPath[0])
        return;
    MiniDumpWriteDump_t fn = (MiniDumpWriteDump_t)GetProcAddress(dbg, "MiniDumpWriteDump");
    if (!fn)
        return;
    std::string p = std::string(g_dumpPath) + tag + ".dmp";
    HANDLE f = CreateFileA(p.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    KT_MINIDUMP_EXCEPTION_INFO mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = (KT_EXCEPTION_POINTERS*)excPtrs;
    mei.ClientPointers = 0;
    fn(GetCurrentProcess(), GetCurrentProcessId(), f, 0 /*MiniDumpNormal*/, excPtrs ? &mei : NULL, NULL, NULL);
    CloseHandle(f);
}

static int DumpOnException(const char* tag, EXCEPTION_POINTERS* ep)
{
    WriteCrashText(tag, ep);
    return EXCEPTION_EXECUTE_HANDLER;
}

static LPTOP_LEVEL_EXCEPTION_FILTER g_prevUEF = NULL;

static LONG WINAPI TrainerUEF(EXCEPTION_POINTERS* ep)
{
    WriteCrashDumpEx("KT_fatal", ep); // dump first, then defer to RE_Kenshi's UEF
    if (g_prevUEF)
        return g_prevUEF(ep);
    return EXCEPTION_CONTINUE_SEARCH;
}

// ---------------------------------------------------------------------------
// input queue: window messages arrive on the window thread, ImGui runs on the
// render thread. Queue here, drain before ImGui::NewFrame.
// ---------------------------------------------------------------------------

struct KTMsg { HWND h; UINT m; WPARAM w; LPARAM l; };
static KTMsg g_msgQueue[256];
static volatile LONG g_msgHead = 0;
static volatile LONG g_msgTail = 0;

static void QueueMsg(HWND h, UINT m, WPARAM w, LPARAM l)
{
    LONG head = g_msgHead;
    LONG next = (head + 1) & 255;
    if (next == g_msgTail)
        return; // queue full: drop
    g_msgQueue[head].h = h;
    g_msgQueue[head].m = m;
    g_msgQueue[head].w = w;
    g_msgQueue[head].l = l;
    InterlockedExchange(&g_msgHead, next); // publish with full fence
}

static LRESULT WndProc_Inner(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ImGuiIO& io = ImGui::GetIO();
    bool mouseMsg = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL;
    bool keyMsg = (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR ||
                   msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP);
    if (mouseMsg || keyMsg)
        QueueMsg(hWnd, msg, wParam, lParam); // delivered to ImGui on the render thread
    if (mouseMsg && io.WantCaptureMouse)
        return 0;
    if (keyMsg && io.WantCaptureKeyboard)
        return 0;
    return CallWindowProcW(g_origWndProc, hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProc_Hook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_imguiReady && g_overlayEnabled)
    {
        __try { return WndProc_Inner(hWnd, msg, wParam, lParam); }
        __except (DumpOnException("KT_wndproc", GetExceptionInformation())) {}
    }
    return CallWindowProcW(g_origWndProc, hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// ImGui init/shutdown
// ---------------------------------------------------------------------------

static void ShutdownImGui()
{
    if (!g_imguiReady)
        return;
    __try
    {
        if (g_hwnd && g_origWndProc)
            SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    g_imguiReady = false;
    g_origWndProc = NULL;
    g_hwnd = NULL;
    if (g_dev) { g_dev->Release(); g_dev = NULL; }
    if (g_ctx) { g_ctx->Release(); g_ctx = NULL; }
}

static bool InitImGui(IDXGISwapChain* sc)
{
    if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&g_dev)) || !g_dev)
        return false;
    g_dev->GetImmediateContext(&g_ctx);
    if (!g_ctx)
        return false;

    DXGI_SWAP_CHAIN_DESC desc;
    if (FAILED(sc->GetDesc(&desc)))
        return false;
    g_hwnd = desc.OutputWindow;
    if (!g_hwnd)
        return false;

    g_uiScale = desc.BufferDesc.Height / 1080.0f;
    if (g_uiScale < 1.0f) g_uiScale = 1.0f;
    if (g_uiScale > 1.6f) g_uiScale = 1.6f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    if (g_imguiIni[0])
        io.IniFilename = g_imguiIni;

    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc",
        16.0f * g_uiScale, NULL, io.Fonts->GetGlyphRangesChineseFull());
    if (!font)
        font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simsun.ttc",
            16.0f * g_uiScale, NULL, io.Fonts->GetGlyphRangesChineseFull());
    if (!font)
        io.Fonts->AddFontDefault();

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(g_uiScale);
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.10f, 0.13f, 0.96f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.15f, 0.20f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.30f, 0.48f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.32f, 0.50f, 0.85f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.45f, 0.70f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.55f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.32f, 0.50f, 0.55f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.45f, 0.70f, 0.8f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.16f, 0.21f, 1.0f);

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);

    g_origWndProc = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)&WndProc_Hook);

    DebugLog("KenshiTrainer: imgui ready");
    return true;
}

// ---------------------------------------------------------------------------
// Present detour: per-instance shadow vtable swap (Dust.dll style)
// ---------------------------------------------------------------------------

typedef HRESULT(STDMETHODCALLTYPE* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* Present1_t)(IDXGISwapChain*, UINT, UINT, const void*);

struct SCRec { IDXGISwapChain* sc; void** shadowVt; void* origPresent; void* origPresent1; };
static SCRec g_scRecs[8];
static int   g_scRecCount = 0;

static void RenderOverlay(IDXGISwapChain* sc)
{
    if (g_imguiReady && sc != g_imguiSC)
        ShutdownImGui(); // device/context/hwnd changed with the swapchain
    if (!g_imguiReady)
    {
        g_imguiReady = InitImGui(sc);
        if (g_imguiReady)
            g_imguiSC = sc;
    }
    if (!g_imguiReady)
        return;

    static int stageLog = 0;
    while (g_msgTail != g_msgHead) // drain queued input on the render thread
    {
        KTMsg m = g_msgQueue[g_msgTail];
        g_msgTail = (g_msgTail + 1) & 255;
        ImGui_ImplWin32_WndProcHandler(m.h, m.m, m.w, m.l);
    }
    // game hides the OS cursor and draws its own (which we hide while the menu
    // is open) - draw ImGui's cursor sprite so the user can see the pointer
    ImGui::GetIO().MouseDrawCursor = Trainer_MenuOpen();
    if (stageLog == 0) { DebugLog("KenshiTrainer: frame stage: init done"); }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    if (stageLog == 0) { DebugLog("KenshiTrainer: frame stage: backend newframe ok"); }
    ImGui::NewFrame();
    DrawTrainerUI();
    if (stageLog == 0) { DebugLog("KenshiTrainer: frame stage: draw ok"); }
    ImGui::Render();
    if (stageLog == 0) { DebugLog("KenshiTrainer: frame stage: imgui render ok"); }
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    if (stageLog == 0) { DebugLog("KenshiTrainer: frame stage: dx11 render ok"); stageLog = 1; }
}

static HRESULT PresentCommon(IDXGISwapChain* sc, UINT sync, UINT flags,
    const void* presentParams, bool isPresent1)
{
    // reentrancy circuit-breaker: if any layer calls Present through the
    // (shadow) vtable from inside a Present detour, break the cycle here
    if (InterlockedCompareExchange(&g_inPresent, 1, 0) != 0)
        return S_OK;

    void* origFn = NULL;
    for (int i = 0; i < g_scRecCount; ++i)
        if (g_scRecs[i].sc == sc)
        {
            origFn = isPresent1 ? g_scRecs[i].origPresent1 : g_scRecs[i].origPresent;
            break;
        }

    HRESULT hr = S_OK;
    if (!origFn)
    {
        InterlockedExchange(&g_inPresent, 0);
        return hr; // unknown instance: don't chain, just acknowledge
    }

    if (g_overlayEnabled && sc == g_gameSC && InterlockedCompareExchange(&g_frameLock, 1, 0) == 0)
    {
        __try { RenderOverlay(sc); }
        __except (DumpOnException("KT_frame", GetExceptionInformation()))
        {
            ErrorLog("KenshiTrainer: overlay frame crashed");
            if (++g_crashCount >= 3)
            {
                g_overlayEnabled = false;
                ErrorLog("KenshiTrainer: overlay disabled after repeated crashes");
            }
        }
        g_frameLock = 0;
    }

    __try
    {
        if (isPresent1)
            hr = ((Present1_t)origFn)(sc, sync, flags, presentParams);
        else
            hr = ((Present_t)origFn)(sc, sync, flags);
    }
    __except (DumpOnException("KT_present", GetExceptionInformation()))
    {
        ErrorLog("KenshiTrainer: original Present crashed");
    }
    InterlockedExchange(&g_inPresent, 0);
    return hr;
}

static HRESULT STDMETHODCALLTYPE Present_Detour(IDXGISwapChain* sc, UINT sync, UINT flags)
{
    return PresentCommon(sc, sync, flags, NULL, false);
}

static HRESULT STDMETHODCALLTYPE Present1_Detour(IDXGISwapChain* sc, UINT sync, UINT flags, const void* params)
{
    return PresentCommon(sc, sync, flags, params, true);
}

// swap in a per-instance shadow vtable with Present (idx 8) and Present1
// (idx 22, if the vtable has one) redirected. Only this swapchain instance is
// affected; the shared class vtable stays untouched.
static void ShadowHookPresent(IDXGISwapChain* sc)
{
    if (!sc || g_scRecCount >= 8)
        return;
    for (int i = 0; i < g_scRecCount; ++i)
        if (g_scRecs[i].sc == sc)
            return;

    void** origVt = NULL;
    __try { origVt = *(void***)sc; } __except (EXCEPTION_EXECUTE_HANDLER) { origVt = NULL; }
    if (!origVt)
        return;

    void** shadow = (void**)malloc(sizeof(void*) * 32);
    if (!shadow)
        return;
    __try { memcpy(shadow, origVt, sizeof(void*) * 32); }
    __except (EXCEPTION_EXECUTE_HANDLER) { free(shadow); return; }

    g_scRecs[g_scRecCount].sc = sc;
    g_scRecs[g_scRecCount].shadowVt = shadow;
    g_scRecs[g_scRecCount].origPresent = shadow[8];
    g_scRecs[g_scRecCount].origPresent1 = shadow[22];
    shadow[8] = (void*)&Present_Detour;
    shadow[22] = (void*)&Present1_Detour;

    __try { *(void***)sc = shadow; }
    __except (EXCEPTION_EXECUTE_HANDLER) { free(shadow); return; }

    g_scRecCount++;
    DebugLog("KenshiTrainer: Present shadow-hooked on swapchain");
}

static int SwapChainArea(IDXGISwapChain* sc)
{
    DXGI_SWAP_CHAIN_DESC d;
    __try
    {
        if (FAILED(sc->GetDesc(&d)))
            return 0;
        return (int)d.BufferDesc.Width * (int)d.BufferDesc.Height;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static void OnSwapChain(IDXGISwapChain* sc)
{
    if (!sc)
        return;
    ShadowHookPresent(sc);

    // the game swapchain is the largest (fullscreen); the launcher's is small
    if (sc == g_gameSC)
        return;
    int newArea = SwapChainArea(sc);
    int curArea = g_gameSC ? SwapChainArea(g_gameSC) : 0;
    if (newArea >= curArea && newArea > 0)
    {
        if (g_gameSC && sc != g_gameSC)
            DebugLog("KenshiTrainer: switching to newer swapchain");
        g_gameSC = sc;
    }
}

// ---------------------------------------------------------------------------
// creation hooks (function-code hooks via KenshiLib - the DustBoot layer)
// ---------------------------------------------------------------------------

typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChain_t)(
    IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
static CreateSwapChain_t g_origCreateSwapChain = NULL;

static HRESULT STDMETHODCALLTYPE CreateSwapChain_Hook(
    IDXGIFactory* self, IUnknown* dev, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out)
{
    HRESULT hr = g_origCreateSwapChain(self, dev, desc, out);
    if (SUCCEEDED(hr) && out && *out)
        OnSwapChain(*out);
    return hr;
}

typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainForHwnd_t)(
    IUnknown*, IUnknown*, HWND, const void*, const void*, IUnknown*, void**);
static CreateSwapChainForHwnd_t g_origCSCFH = NULL;

static HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd_Hook(
    IUnknown* self, IUnknown* dev, HWND hwnd, const void* desc,
    const void* fsDesc, IUnknown* output, void** out)
{
    HRESULT hr = g_origCSCFH(self, dev, hwnd, desc, fsDesc, output, out);
    if (SUCCEEDED(hr) && out && *out)
        OnSwapChain((IDXGISwapChain*)*out); // IDXGISwapChain1 derives IDXGISwapChain
    return hr;
}

typedef HRESULT(WINAPI* D3D11CDSC_t)(
    IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT,
    UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**,
    D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
static D3D11CDSC_t g_origD3D11CDSC = NULL;

static HRESULT WINAPI D3D11CDSC_Hook(
    IDXGIAdapter* adapter, D3D_DRIVER_TYPE dt, HMODULE sw, UINT flags,
    const D3D_FEATURE_LEVEL* levels, UINT numLevels, UINT sdk,
    const DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** scOut,
    ID3D11Device** devOut, D3D_FEATURE_LEVEL* flOut, ID3D11DeviceContext** ctxOut)
{
    HRESULT hr = g_origD3D11CDSC(adapter, dt, sw, flags, levels, numLevels, sdk,
        desc, scOut, devOut, flOut, ctxOut);
    if (SUCCEEDED(hr) && scOut && *scOut)
        OnSwapChain(*scOut);
    return hr;
}

// create a temp factory purely to READ the vtable (DustBoot does the same via
// a temp swapchain + GetParent); hook the two creation functions found there.
static void HookFactoryFunctions()
{
    typedef HRESULT(WINAPI* CF1_t)(REFIID, void**);
    HMODULE dxgi = GetModuleHandleA("dxgi.dll");
    if (!dxgi) dxgi = LoadLibraryA("dxgi.dll");
    if (!dxgi)
        return;
    CF1_t cf1 = (CF1_t)GetProcAddress(dxgi, "CreateDXGIFactory1");
    if (!cf1)
        return;
    IDXGIFactory* factory = NULL;
    if (FAILED(cf1(__uuidof(IDXGIFactory), (void**)&factory)) || !factory)
        return;

    void** vt = *(void***)factory;
    void* createSwapChainFn = vt[10];

    void* cscfhFn = NULL;
    IUnknown* f2 = NULL;
    if (SUCCEEDED(factory->QueryInterface(KT_IID_IDXGIFactory2, (void**)&f2)) && f2)
    {
        void** vt2 = *(void***)f2;
        cscfhFn = vt2[15];
        f2->Release();
    }
    factory->Release();

    if (createSwapChainFn)
    {
        if (KenshiLib::SUCCESS != KenshiLib::AddHook(createSwapChainFn, &CreateSwapChain_Hook, (void**)&g_origCreateSwapChain))
            ErrorLog("KenshiTrainer: failed to hook CreateSwapChain");
    }
    if (cscfhFn)
    {
        if (KenshiLib::SUCCESS != KenshiLib::AddHook(cscfhFn, &CreateSwapChainForHwnd_Hook, (void**)&g_origCSCFH))
            ErrorLog("KenshiTrainer: failed to hook CreateSwapChainForHwnd");
    }
    DebugLog("KenshiTrainer: factory function hooks installed");
}

void Overlay_Init()
{
    // ini + crash report paths next to the DLL
    HMODULE self = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&Overlay_Init, &self))
    {
        char path[MAX_PATH];
        if (GetModuleFileNameA(self, path, MAX_PATH))
        {
            std::string p = path;
            size_t slash = p.find_last_of("\\/");
            if (slash != std::string::npos)
                p.erase(slash + 1);
            strcpy_s(g_imguiIni, (p + "KenshiTrainer_imgui.ini").c_str());
            strcpy_s(g_dumpPath, p.c_str());
        }
    }

    HMODULE d3d11 = GetModuleHandleA("d3d11.dll");
    if (!d3d11) d3d11 = LoadLibraryA("d3d11.dll");
    void* cdsc = d3d11 ? (void*)GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain") : NULL;
    if (cdsc)
    {
        if (KenshiLib::SUCCESS != KenshiLib::AddHook(cdsc, &D3D11CDSC_Hook, (void**)&g_origD3D11CDSC))
            ErrorLog("KenshiTrainer: failed to hook D3D11CreateDeviceAndSwapChain");
    }

    HookFactoryFunctions();

    g_prevUEF = SetUnhandledExceptionFilter(&TrainerUEF);
    DebugLog("KenshiTrainer: overlay hooks installed");
}
