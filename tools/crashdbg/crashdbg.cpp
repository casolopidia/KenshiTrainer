// crashdbg.cpp - minimal user-mode debugger: launch a process, report the
// first-chance exception (code/address) + context + call stack + modules.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

static HANDLE gProc;
static DWORD  gPid;

static void Log(const char* fmt, ...)
{
    char buf[2048];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    OutputDebugStringA(buf);
    printf("%s", buf);
    fflush(stdout);
}

static void DumpStack(HANDLE hThread, CONTEXT& ctx)
{
    STACKFRAME64 sf = {};
    sf.AddrPC.Offset = ctx.Rip; sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrFrame.Offset = ctx.Rbp; sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Offset = ctx.Rsp; sf.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 64; ++i)
    {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, gProc, hThread, &sf, &ctx,
                NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
            break;
        if (sf.AddrPC.Offset == 0)
            break;
        DWORD64 disp = 0;
        char symbuf[sizeof(SYMBOL_INFO) + 512] = {};
        SYMBOL_INFO* si = (SYMBOL_INFO*)symbuf;
        si->SizeOfStruct = sizeof(SYMBOL_INFO);
        si->MaxNameLen = 512;
        const char* name = "?";
        if (SymFromAddr(gProc, sf.AddrPC.Offset, &disp, si))
            name = si->Name;
        char mod[MAX_PATH] = "?";
        DWORD64 base = SymGetModuleBase64(gProc, sf.AddrPC.Offset);
        if (base)
        {
            IMAGEHLP_MODULE64 mi = {}; mi.SizeOfStruct = sizeof(mi);
            if (SymGetModuleInfo64(gProc, sf.AddrPC.Offset, &mi))
                _snprintf(mod, sizeof(mod), "%s", mi.ModuleName);
        }
        Log("  %2d: %s!%s+0x%llX  (0x%llX)\n", i, mod, name,
            (unsigned long long)disp, (unsigned long long)sf.AddrPC.Offset);
        if (sf.AddrReturn.Offset == 0)
            break;
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2) { printf("usage: crashdbg <exe> [args]\n"); return 2; }
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);

    STARTUPINFOW si = {}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    wchar_t cmd[4096] = {};
    for (int i = 1; i < argc; ++i)
    {
        wcscat_s(cmd, argv[i]); wcscat_s(cmd, L" ");
    }
    DWORD flags = DEBUG_PROCESS;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi))
    {
        printf("CreateProcess failed %lu\n", GetLastError());
        return 1;
    }
    gProc = pi.hProcess; gPid = pi.dwProcessId;
    Log("debugging pid=%lu\n", gPid);
    SymInitialize(gProc, NULL, TRUE);

    bool exceptionSeen = false;
    DEBUG_EVENT ev = {};
    bool seenException = false;
    while (WaitForDebugEvent(&ev, INFINITE))
    {
        DWORD cont = DBG_CONTINUE;
        switch (ev.dwDebugEventCode)
        {
        case CREATE_PROCESS_DEBUG_EVENT:
            Log("[ev] create process pid=%lu", ev.dwProcessId);
            CloseHandle(ev.u.CreateProcessInfo.hFile);
            break;
        case LOAD_DLL_DEBUG_EVENT:
            CloseHandle(ev.u.LoadDll.hFile);
            break;
        case EXCEPTION_DEBUG_EVENT:
        {
            const EXCEPTION_RECORD& er = ev.u.Exception.ExceptionRecord;
            static long firstChanceCpp = 0;
            bool isCpp = (er.ExceptionCode == 0xE06D7363);
            if (ev.u.Exception.dwFirstChance)
            {
                if (isCpp)
                    ++firstChanceCpp;
                else
                    Log("[fc] first-chance 0x%08X at %p (params %p %p)\n",
                        er.ExceptionCode, er.ExceptionAddress,
                        er.ExceptionInformation[0], er.ExceptionInformation[1]);
                break; // let the app handle it normally
            }
            bool benign = (er.ExceptionCode == 0x40010006) ||
                (er.ExceptionCode == 0x40010008) ||
                (er.ExceptionCode == 0x80000003) ||
                (er.ExceptionCode == 0x80000004);
            bool interesting = !seenException && !benign;
            if (interesting)
            {
                seenException = true;
                HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, ev.dwThreadId);
                CONTEXT ctx = {}; ctx.ContextFlags = CONTEXT_FULL;
                GetThreadContext(hThread, &ctx);
                char mod[MAX_PATH] = "";
                DWORD64 base = SymGetModuleBase64(gProc, (DWORD64)er.ExceptionAddress);
                DWORD64 disp = (DWORD64)er.ExceptionAddress - base;
                IMAGEHLP_MODULE64 mi = {}; mi.SizeOfStruct = sizeof(mi);
                if (base && SymGetModuleInfo64(gProc, (DWORD64)er.ExceptionAddress, &mi))
                    _snprintf(mod, sizeof(mod), "%s", mi.ModuleName);
                Log("\n==== EXCEPTION 0x%08X at %s+0x%llX (0x%p) thread=%lu ====\n",
                    er.ExceptionCode, mod[0] ? mod : "?",
                    (unsigned long long)disp, er.ExceptionAddress, ev.dwThreadId);
                Log("RIP=%p RSP=%p RAX=%p RBX=%p RCX=%p RDX=%p\n",
                    (void*)ctx.Rip, (void*)ctx.Rsp, (void*)ctx.Rax,
                    (void*)ctx.Rbx, (void*)ctx.Rcx, (void*)ctx.Rdx);
                Log("[info] total first-chance C++ exceptions before death: %ld\n", firstChanceCpp);
                DumpStack(hThread, ctx);
                TerminateProcess(gProc, 1);
                CloseHandle(hThread);
                ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, cont);
                goto done;
            }
            break;
        }
        case EXIT_PROCESS_DEBUG_EVENT:
            Log("[ev] exit process pid=%lu code=%lu", ev.dwProcessId,
                ev.u.ExitProcess.dwExitCode);
            break;
        default:
            break;
        }
        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, cont);
        if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
        {
            Log("process exited code=%lu\n", ev.u.ExitProcess.dwExitCode);
            break;
        }
    }
    { DWORD werr = GetLastError(); Log("[ev] WaitForDebugEvent ended err=%lu\n", werr); }
done:
    SymCleanup(gProc);
    return seenException ? 3 : 0;
}