# KenshiTrainer - Kenshi 内置修改器 / In-game Trainer

[中文](#中文) | [English](#english)

---

## 中文

基于 [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) + [KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib) 的 Kenshi 游戏内置修改器插件，使用 Dear ImGui 在游戏画面上直接渲染界面，**自动跟随游戏语言显示中文或英文**。

### 功能

- **属性编辑**：选中角色 38 项属性读写（含 mod 新增属性），带写入校验回读；"全部拉满"一键满属性
- **改名 / 金钱**：直接修改选中角色姓名与携带金钱
- **物品生成**：枚举游戏全部物品定义（**自动包含所有启用的 mod**），按类别（武器/护甲/物品/弩/背包容器）过滤 + 关键字搜索
  - 武器：制造商 / 模型联动选择，等级或品质档（中级救助~铭物）驱动
  - 护甲/弩：六档品质（模本~杰作）+ 等级微调
  - 生成到选中角色背包，背包已满时自动掉落地面不丢物品
- **建筑放置验证开关**：关闭放置有效性检查（坡度/重叠/室内等不再拦截），预览颜色仍显示真实有效性，违规位置也能放下
- **中英双语界面**：根据游戏当前语言自动切换

### 环境要求

- Kenshi **1.0.65**（Steam / GOG）
- [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) v0.3.5 或更新（必须先安装）

### 安装

1. 按 [RE_Kenshi 说明](https://github.com/BFrizzleFoShizzle/RE_Kenshi) 安装并确认主菜单显示 RE_Kenshi 版本号
2. 将 `mods/KenshiTrainer/` 整个文件夹复制到 `Kenshi/mods/` 目录
3. 在 Kenshi 启动器的 Mods 页勾选 KenshiTrainer
4. 进入游戏，点击左上角 **修改器** 按钮打开面板（可拖动）

### 从源码编译

需要 Visual Studio 2019+（安装 v100 x64 工具链）、Boost 1.60.0、KenshiLib 依赖（见 `reference/`）。

```powershell
powershell -ExecutionPolicy Bypass -File toolsuild.ps1
```

产物自动复制到 `mods/KenshiTrainer/`。仅支持 Release|x64。

### 故障排查

- 插件是否加载：查看游戏目录 `RE_Kenshi_log.txt` 中的 `KenshiTrainer:` 日志行
- 崩溃报告：DLL 同目录 `KT_*.txt`
- Windows Smart App Control 可能阻止插件加载，需临时关闭

### 许可证

GPL-3.0（使用 KenshiLib 的插件须以 GPLv3 发布）。

### 致谢

- [RE_Kenshi / KenshiLib](https://github.com/BFrizzleFoShizzle/RE_Kenshi) by BFrizzleFoShizzle
- [Dear ImGui](https://github.com/ocornut/imgui)

---

## English

An in-game trainer plugin for Kenshi built on [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) + [KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib), rendered with Dear ImGui. **The UI follows the game language (Chinese / English automatically).**

### Features

- **Stats editor**: read/write 38 stats of the selected character (modded stats included) with write-verify readback; one-click max all
- **Rename / money**: edit the selected character's name and money
- **Item spawner**: enumerates every item definition in the game (**including all enabled mods**), filter by category (weapons / armour / items / crossbows / backpacks & containers) with keyword search
  - Weapons: linked manufacturer/model pickers, numeric level or quality grade (mid-grade .. meitou)
  - Armour/crossbows: six quality tiers (prototype .. masterwork) plus fine-grained level
  - Spawns into the selected character's backpack; drops on the ground when full (never lost)
- **Building placement bypass**: disables the placement validity check (slope / overlap / indoors no longer block); preview colour stays truthful so you can place anywhere
- **Bilingual UI**: follows the current game language

### Requirements

- Kenshi **1.0.65** (Steam / GOG)
- [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) v0.3.5 or newer (must be installed first)

### Install

1. Install RE_Kenshi per its README and confirm the version shows on the main menu
2. Copy the `mods/KenshiTrainer/` folder into `Kenshi/mods/`
3. Enable KenshiTrainer in the launcher Mods page
4. In game, click the **Trainer** button at the top-left (draggable)

### Building from source

Visual Studio 2019+ with the v100 x64 toolset, Boost 1.60.0 and the KenshiLib deps (see `reference/`).

```powershell
powershell -ExecutionPolicy Bypass -File toolsuild.ps1
```

Output is copied to `mods/KenshiTrainer/`. Release|x64 only.

### Troubleshooting

- Check `RE_Kenshi_log.txt` in the game folder for `KenshiTrainer:` lines
- Crash reports: `KT_*.txt` next to the DLL
- Windows Smart App Control may block the plugin

### License

GPL-3.0 (plugins using KenshiLib must be GPLv3).

### Credits

- [RE_Kenshi / KenshiLib](https://github.com/BFrizzleFoShizzle/RE_Kenshi) by BFrizzleFoShizzle
- [Dear ImGui](https://github.com/ocornut/imgui)
