# Repository Guidelines

Contributor guide for the KenshiTrainer project — a RE_Kenshi/KenshiLib plugin DLL that renders an in-game trainer (stats editor, money, rename, item spawner, medical/prosthetics editor, building-placement bypass) via a Dear ImGui overlay on the game's D3D11 swapchain. UI text is Chinese + English, selected at runtime from the game locale; docs under `docs/` are in Chinese.

## Project Structure & Module Organization

- `src/` — the plugin (Release|x64, MSVC v100 only).
  - `KenshiTrainer.cpp` — plugin entry `startPlugin()`, game-locale detection, `data\mods.cfg` self-gate, stat offset calibration, item database + spawner, and the whole ImGui UI (`DrawTrainerUI`).
  - `Overlay.cpp/.h` — DXGI/D3D11 hooks, Present shadow-vtable, WndProc queue, ImGui init/frame pump, crash text + minidump reports. Dust coexistence: contested dxgi creation hooks are skipped and the game swapchain is found by pure-read polling of OGRE internals (`OgreSwapChainWatch`: `WindowEventUtilities::_msWindows` / Root singleton -> `D3D11RenderWindowHwnd::mpSwapChain`; offsets documented in `docs/03/Dust共存问题-移交文档.md`). Self-contained: no Dust, no MyGUI widgets.
  - `BuildBypass.cpp/.h` — building placement-validity bypass (`placementVerification` hook). Separate translation unit because `kenshi/Building/Building.h`'s global `BuildingDesignation` enum clashes with `kenshi/Platoon.h`.
  - `ChineseStrings.h`, `StatNames.h` — generated pure-ASCII tables (`TR::*` CN/EN pairs, `TR_STATS` = 38 stat entries). Regenerate with a scratch Python script; never hand-edit the escapes or put raw Chinese in source.
  - `KenshiTrainer.vcxproj` — the only project file. Toolset v100 + LTCG, compiles ImGui 1.86 sources from `reference/imgui-1.86`.
- `mods/KenshiTrainer/` — shippable mod package (`KenshiTrainer.dll`, `RE_Kenshi.json` PreloadPlugin declaration, `KenshiTrainer.mod`). The DLL is tracked in git, so every build dirties the tree; `tools/build.ps1` copies the DLL here.
- `docs/` — Chinese design/API/install docs, grouped by folder: `01-总览与路线图/`, `02-FCS官方模组工具/`, `03-RE_Kenshi注入框架/`, `04-游戏数据参考/`, `06-资料链接索引/`, plus `05-构建与安装说明.md` (verified local toolchain setup + crash post-mortems), `07-创意工坊介绍.md` and `08-创意工坊更新清单与发布说明.md` (workshop release checklist).
- `reference/` — local-only upstream material (gitignored, contains nested git clones). Treat as read-only: `KenshiLib_include`, `KenshiLib_Examples_deps` (KenshiLib libs + Boost 1.60), `KenshiLib_Examples*`, `RE_Kenshi_repo`, `离线API参考/ReKenshi_KLIB_Navigator.html`, and `imgui-1.86`.
  - `reference/imgui-1.86` is **not vanilla**: `backends/imgui_impl_dx11.cpp` dynamically loads `D3DCompiler_47/43.dll` (SDK 7.1 has no `d3dcompiler.h`). Never re-download or replace this tree.
- `Dust-main/` — untracked copy of the upstream **Dust** rendering framework, a separate project with its own build/tests. It is a reference for D3D11 hooking and ImGui overlay patterns only; nothing in it is compiled into KenshiTrainer.
- `tools/` — build, reverse-engineering and vendored-toolchain material; see the map below.
- `logs/` — runtime `RE_Kenshi_log.txt` copied in from the user's game. Never commit.

## Build, Test, and Development Commands

- `powershell -ExecutionPolicy Bypass -File tools\build.ps1` — the only build command. It points `KENSHILIB_DIR`/`KENSHILIB_DEPS_DIR`/`BOOST_*` at `reference\KenshiLib_Examples_deps`, builds `src\KenshiTrainer.vcxproj` (Release|x64) with the VS2022 BuildTools MSBuild host, and copies the DLL into `mods\KenshiTrainer\`.
- Toolchain is fixed: **MSVC v100 (VS2010) x64, Release only** (Kenshi ABI requirement). ImGui is pinned to 1.86, the last version compatible with v100.
- Two vcxproj workarounds are load-bearing — do not "clean them up": `TrackFileAccess=false` (new MSBuild trackers break v100) and `GenerateManifest=false` (Mt task path error).
- `/GL` + LTCG must stay enabled: hook targets use the `_NV_` non-virtual import stubs so `KenshiLib::GetRealAddress` folds to the real import; without it the address lands inside our own DLL and KenshiLib asserts (`Functions.cpp:174`).
- `tools\fixreg.ps1` (or `fixreg.bat`, needs admin) repairs the HKLM keys that register the VS2010 v100 toolset and Windows SDK 7.1 when MSBuild cannot find them.
- No automated tests exist. Verification is in-game: check `RE_Kenshi_log.txt` for `KenshiTrainer:` lines and crash reports `KT_*.txt` next to the DLL.

## tools/ Map

- `build.ps1` — the build entry point (above).
- `fixreg.bat` / `fixreg.ps1` — HKLM registration fixups for the v100 toolchain.
- `patch_diag.py`, `patch_diag2.py`, `patch_stats.py` — one-off source rewrites from earlier fix rounds, already applied and asserting on old text. Historical records: do not re-run blindly.
- `cdb_script*.txt`, `cdb_all.txt`, `cdb_frame.txt` — CDB/WinDbg command scripts.
- `allstacks.txt`, `build_diag.log`, `vc_std*.log`, `winsdk_amd64.log`, `cheatmenu_disasm.txt`, `dustboot_disasm.txt`, `crashdump/`, `crashdump2/` — captured evidence (crash dumps, game logs, disassembly) for reverse engineering. Read-only.
- `wiki_fetch/` — scraped Kenshi wiki articles (Chinese `.txt`) plus `fetch.py`/`batch*.py`.
- `winsdk_extract/`, `GRMSDKX_EN_DVD.iso`, `imgui186.zip` — setup media for the v100 toolchain.
- `ogre-sdk/` + `ogre-sdk.zip`, `blender2ogre/` — large vendored toolchains plus ad-hoc Blender mesh/rig experiment scripts. Not part of the plugin build.

## Coding Style & Naming Conventions

- C++ in a single style: 4-space indent, Allman braces, `static` file-scope globals prefixed `g_`, SEH wrappers named `SafeXxx`.
- Inside `__try` use POD locals only — MSVC C2712 forbids objects needing unwinding; put the real work in an `XxxImpl` function and wrap that.
- All game API calls go through a `SafeXxx` wrapper; never call game code bare.
- Hook through `KenshiLib::AddHook` (multi-plugin compatible). Never take the address of a virtual member function (`&Class::method` synthesizes a local thunk) — use the `_NV_` stub for the same RVA.
- Game-facing string literals live in `ChineseStrings.h`/`StatNames.h` as `\xNN` UTF-8 hex escapes (v100 source encoding). Add strings by regenerating the header with Python, not by editing escapes manually.

## Testing Guidelines

- Manual in-game testing only. Test matrix: stats write/readback (including modded stats), money, rename, item spawn per category (weapon manufacturer/model/level, armour grade/level), building-placement bypass, and CN/EN locale switching.
- Always ship diagnostic logging via `DebugLog()` for new game interactions; ask testers for `RE_Kenshi_log.txt` and `KT_*.txt`.
- Launch-time safety: the plugin self-gates on `data\mods.cfg` (stays dormant when the mod is unchecked) and creates its main panel lazily on first click; keep it that way.
- Reverse-engineering note: the exe that actually runs is `RE_Kenshi\Kenshi_x64.exe` (the 1.0.65 compatibility copy swapped in by the RE_Kenshi installer), NOT the root `kenshi_x64.exe`. KenshiLib header RVA comments are stale for it; the real mapping is stub-index (FUNC_BEGIN=0x15B46 in KenshiLib.dll) -> `RE_Kenshi\RVAs\Steam_1.0.65.br` (raw uint32 array).

## Commit & Pull Request Guidelines

- Commit messages are short imperative summaries of the fix round (e.g. "fix stat input apply-on-enter, armour level scale x20"). Reference the symptom and root cause in the body when non-obvious.
- PRs should state: what changed, how it was verified (log lines / in-game behavior), and remaining known risks (e.g. unverified grade mapping).
- Never commit `reference/`, `logs/`, `Dust-main/`, `src/x64/`, or the runtime files the plugin writes next to the DLL (`KenshiTrainer_imgui.ini`, `KT_*.txt`/minidumps). `docs/08` lists exactly what belongs in a workshop upload.