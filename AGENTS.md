# Repository Guidelines

Contributor guide for the KenshiTrainer project — a RE_Kenshi/KenshiLib plugin DLL that renders an in-game trainer (stats editor, money, rename, item spawner) via a Dear ImGui overlay on the game's D3D11 swapchain. UI text is Chinese; docs under `docs/` are in Chinese.

## Project Structure & Module Organization

- `src/` — plugin source. `KenshiTrainer.cpp` (game logic + ImGui UI), `Overlay.cpp/.h` (DXGI/D3D11 hooks, Present shadow-vtable, WndProc queue, crash reports), `KenshiTrainer.vcxproj` (MSBuild project), `ChineseStrings.h`/`StatNames.h` (generated UTF-8-hex string tables — regenerate with Python, never edit by hand).
- `mods/KenshiTrainer/` — shippable mod package (`KenshiTrainer.dll`, `RE_Kenshi.json`, `.mod`). The build script copies the DLL here.
- `tools/` — build/debug scripts (`build.ps1`, crash-dump analysis, PE/disasm helpers).
- `docs/` — design docs, API notes, install/troubleshooting guide.
- `reference/` — vendored dependencies (KenshiLib headers/libs, ImGui 1.86, example repos, Dust/CheatMenu binaries for reverse engineering). Do not modify; treat as read-only upstream.
- `logs/` — runtime logs from the user's game (copied in for debugging). Never commit.

## Build, Test, and Development Commands

- `powershell -ExecutionPolicy Bypass -File tools\build.ps1` — the only build command. Compiles `src\KenshiTrainer.vcxproj` (Release|x64) with MSBuild and copies the DLL into `mods\KenshiTrainer\`.
- Toolchain is fixed: **MSVC v100 (VS2010) x64, Release only** (Kenshi ABI requirement). ImGui is pinned to 1.86 (last version compatible with v100).
- No automated tests exist. Verification is in-game: check `RE_Kenshi_log.txt` for `KenshiTrainer:` lines and crash reports `KT_*.txt` next to the DLL.

## Coding Style & Naming Conventions

- C++ in a single style: 4-space indent, Allman braces, `static` file-scope globals prefixed `g_`, SEH wrappers named `SafeXxx` (POD-only locals inside `__try` — MSVC C2712 forbids unwindable objects there).
- All game API calls go through `SafeXxx` wrappers; never call game code bare.
- Game-facing string literals live in `ChineseStrings.h`/`StatNames.h` as `\xNN` UTF-8 hex escapes (v100 source encoding). Add strings by regenerating the header with Python, not by editing escapes manually.

## Testing Guidelines

- Manual in-game testing only. Test matrix: stats write/readback, money, rename, item spawn per category (weapon manufacturer/model/level, armour grade/level).
- Always ship diagnostic logging via `DebugLog()` for new game interactions; ask testers for `RE_Kenshi_log.txt` and `KT_*.txt`.

## Commit & Pull Request Guidelines

- Commit messages are short imperative summaries of the fix round (e.g. "fix stat input apply-on-enter, armour level scale x20"). Reference the symptom and root cause in the body when non-obvious.
- PRs should state: what changed, how it was verified (log lines / in-game behavior), and remaining known risks (e.g. unverified grade mapping).