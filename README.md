# KenshiTrainer - Kenshi 内置修改器 / In-game Trainer

[中文](#中文) | [English](#english)

> 当前构建：2026-10-27 · `KenshiTrainer.dll` 519,680 字节 · 目标游戏版本 Kenshi 1.0.65

基于 [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) + [KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib) 的 Kenshi 游戏内置修改器插件，使用 Dear ImGui 在游戏画面上直接渲染界面，**自动跟随游戏语言显示中文或英文**。

---

## 中文

### 功能

主面板分五个页签：属性 / 物品生成 / 建筑 / 医疗 / 经验倍率。

- **属性编辑**：选中角色 38 项属性读写（含 mod 新增属性），写入后回读校验；「全部拉满」一键满属性
- **改名 / 金钱**：直接修改选中角色姓名与携带金钱
- **物品生成**：枚举游戏全部物品定义（**自动包含所有启用的 mod**），按类别（全部/武器/护甲/物品/弩/容器）过滤 + 关键字搜索
  - 武器：制造商 / 模型联动选择，等级或品质档（中级救助~铭物）驱动
  - 护甲/弩：六档品质（模本~杰作）+ 等级微调
  - 生成到选中角色背包，背包已满时自动掉落地面不丢物品

**医疗**（三个折叠区）

- **全身血液与部位血量**
  - 全身血液：实时显示当前 / 上限，输入目标值后「设定」或直接「回满」；保留原版负血值语义，失血昏迷与死亡判定照常
  - 部位血量：按角色实际 anatomy 列出头、胸、腹、四肢及 mod 新增部位。「当前 / 上限」实时读取，目标值是草稿，只有「设定」「回满」才写入
  - 上限按 `maxHealth()` 反算，种族/年龄倍率与义肢磨损继续生效，不会把所有种族一律设成 100
  - 「回满」清除该部位钝伤与临时修补、把绷带量限制在剩余伤口内；**不修改其他部位、全身血液、饥饿、死亡标志与昏迷计时器**，回满一只手不会自动补血或复活角色
  - 断肢、粉碎肢体不能直接改数值，需先恢复原生肢体或安装义肢；非法/越界/非有限输入拒绝写入
- **形体调节与保持**
  - 「锁定当前外形」捕获当前三个外观倍率，或用滑条（体格 / 肌肉 / 消瘦，0~100，Ctrl+点击可输入）调节后「应用数值并保持」
  - 只覆盖由属性与营养驱动的**外观倍率**，不改力量、敏捷、技能、饥饿等任何属性；取消锁定后恢复游戏自动计算
  - 锁定按角色外观对象记录：可给多人分别设置，切换选中或关闭面板仍然生效；角色卸载 / 读档 / 重启后失效（**不写入存档**）。动物不提供此功能
- **截肢与义肢**
  - 四个肢体分别显示状态，可截肢 / 安装 / 恢复原生肢体（满血恢复）
  - 可直接替换已装义肢，**无需先截肢**；换装或卸下的旧义肢自动退回背包，背包满则落地
  - 每个肢体下拉只列明确对应该部位的型号，安装前再次校验；无法识别左右与臂腿的模组义肢不会出现在下拉中（日志记录 `prosthetic compatibility unknown`）
  - 义肢品质沿用护甲生成器六档量表，默认「自动」，也可直接填等级

- **经验倍率**（独立页签）
  - 倍率范围 **0.00~1000.00**，精度 0.01；「玩家派系」「AI 派系」可分别勾选，也可同时勾选或都不勾选
  - 只作用于今后获得的技能经验，不追溯修改已有属性。1 倍为原速，0 倍停止所选范围的正常正向经验增长，都不勾选时双方保持原速
  - 派系归属取经验获得者本身，友好 NPC、雇佣盟友与敌对 NPC 都按实际派系归类；玩家派系动物同样属于玩家范围
  - 嵌套经验入口替换而非叠乘上下文，`xpMelee -> xpDodgeEvent -> increaseStat` 这类调用链不会把 2 倍变成 4 倍；伤害、速度、敌人技能等原始参数不改变
  - **不会赋予 AI 原版没有的经验来源**（例如非玩家角色的跑步经验）；训练条件、种族倍率与等级上限仍由游戏决定
  - 12 个经验钩子全部安装成功才启用；任一失败则全部保持原速，界面显示不可用
- **建筑放置验证开关**：关闭放置有效性检查（坡度 / 重叠 / 室内等不再拦截），预览颜色仍显示真实有效性，违规位置也能放下（**存在未修复 bug，酌情使用，建议先存档**）
- **中英双语界面**：根据游戏当前语言自动切换
- **兼容性**：可与 Dust 画质增强 mod 及其他 RE_Kenshi 插件双开同用

### 界面与设置

- 入口是视口**右上角**的「打开面板 / 关闭面板」小窗口（可拖动标题栏），主面板首次打开位于默认位置，超出视口时自动移回
- 主面板原生支持标题栏拖动、X 关闭、滚动条、输入框与下拉框；打开时游戏忽略世界输入
- DLL 同目录 `KenshiTrainer.ini` 的 `[Features]` 节保存功能偏好：建筑开关、物品分类、生成数量、武器/护甲/义肢等级、经验倍率与两个派系勾选。改动在当帧保存，写入失败每 5 秒重试，文件缺失或数值越界时回退默认值
- 窗口布局保存在同目录 `KenshiTrainer_imgui.ini`，与功能设置分开管理
- 更新 DLL 时**保留这两个 ini**；上传创意工坊时**不要包含**（属于本机设置）

### 已知限制

- 建筑放置绕过仍有未修复 bug，预览有效性与真实放置结果可能不一致，建议操作前存档
- 形体锁定只在本次外观对象生命周期内有效，读档 / 重启 / 角色重载后需重新设置
- 全身血液与部位血量互不治疗：回血不治部位伤，部位回满不补血，也不解除昏迷计时
- 断肢、粉碎肢体不能直接编辑血量，需先恢复原生肢体或安装义肢
- 无法识别左右与臂腿的模组义肢不会出现在对应肢体的下拉中，也不允许安装
- 经验倍率只覆盖游戏原有的经验入口，完全绕开这些入口的其他模组不受影响

### 环境要求

- Kenshi **1.0.65**（Steam / GOG）
- [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) v0.3.5 或更新（必须先安装）

### 安装

1. 按 [RE_Kenshi 说明](https://github.com/BFrizzleFoShizzle/RE_Kenshi) 安装并确认主菜单显示 RE_Kenshi 版本号
2. 将 `mods/KenshiTrainer/` 整个文件夹复制到 `Kenshi/mods/` 目录
3. 在 Kenshi 启动器的 Mods 页勾选 KenshiTrainer
4. 进入游戏，点击右上角「打开面板」打开主面板

### 从源码编译

需要 Visual Studio 2019+（安装 v100 x64 工具链）、Boost 1.60.0、KenshiLib 依赖（见 `reference/`）。

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

产物自动复制到 `mods/KenshiTrainer/`。仅支持 Release|x64。

### 故障排查

- 插件是否加载：查看游戏目录 `RE_Kenshi_log.txt` 中的 `KenshiTrainer:` 日志行。关键行包括 `feature settings loaded`、`physique hooks ready (3/3)`、`part health ... result=ok/failed`、经验钩子安装数量
- 崩溃报告：DLL 同目录 `KT_*.txt` 与 minidump
- 定向关闭功能排查（环境变量）：`KT_NOHOOK` 整体禁用插件、`KT_NOXP` 只跳经验钩子、`KT_NOPHYSIQUE` 只跳形体钩子
- 从 2026-10-08 之前的版本升级：旧版没有功能设置持久化，需要在设置面板里重新设置一次
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

The main panel has five tabs: Stats / Item Spawner / Building / Medical / XP Multiplier.

- **Stats editor**: read/write 38 stats of the selected character (modded stats included) with write-verify readback; one-click max all
- **Rename / money**: edit the selected character's name and money
- **Item spawner**: enumerates every item definition in the game (**including all enabled mods**), filter by category (all / weapons / armour / items / crossbows / containers) with keyword search
  - Weapons: linked manufacturer/model pickers, numeric level or quality grade (mid-grade .. meitou)
  - Armour/crossbows: six quality tiers (prototype .. masterwork) plus fine-grained level
  - Spawns into the selected character's backpack; drops on the ground when full (never lost)

**Medical** (three collapsible sections)

- **Total blood and body-part health**
  - Total blood: live current / max; enter a target and press Set, or press Full. Negative values keep the base game's bleeding-out and death semantics
  - Body parts: lists the character's real anatomy (head, chest, stomach, limbs and modded parts). Current / Max are live reads; entered targets are drafts and only Set / Full write to the game
  - Max is derived through `maxHealth()`, so race/age multipliers and prosthetic wear stay effective - the trainer never forces every race to 100
  - Full clears that part's blunt damage and jury-rigging and clamps bandage amounts to the remaining wound. It **never touches other parts, total blood, hunger, death flags or the KO timer**, so healing a hand will not refill blood or revive a character
  - Severed or crushed limbs cannot be edited directly - restore the original limb or install a prosthetic first. Invalid, non-finite or out-of-range input is rejected
- **Physique adjustment and lock**
  - "Freeze current physique" captures the three appearance multipliers, or drag the bulk / muscle / skinny sliders (0-100, Ctrl+click to type) and press "Apply values and lock"
  - Overrides only the **appearance multipliers** driven by stats and nutrition - strength, dexterity, skills and hunger are untouched. Unchecking the lock returns appearance to automatic game calculation
  - The lock is recorded per character appearance object: several characters can be locked independently and the lock survives panel close and character switching, but is lost on character unload / reload / restart (**never written to saves**). Not available for animals
- **Amputation and prosthetics**
  - Each of the four limbs shows its state; amputate / install / restore the original limb (restored at full health)
  - Direct replacement works **without amputating first**; the replaced or removed prosthetic returns to the backpack (drops on the ground when full)
  - Each limb dropdown lists only models that map to that limb, and compatibility is re-checked before install; prosthetics whose side or arm/leg type cannot be identified are not offered (logged as `prosthetic compatibility unknown`)
  - Prosthetic quality uses the same six-tier scale as the armour spawner, defaults to Auto, and can be typed as a numeric level

- **XP multiplier** (dedicated tab)
  - Multiplier range **0.00-1000.00** in 0.01 steps; "player faction" and "AI factions" can be selected separately, together, or not at all
  - Affects future skill XP only - existing stats are not retroactively changed. 1x is normal speed, 0x stops positive XP gain in the selected scope, and selecting neither keeps everything at normal speed
  - Scope follows the faction of the character gaining XP, so friendly NPCs, hired allies and hostile NPCs all count by their real faction; player-faction animals count as player
  - Nested XP entries replace rather than stack the context, so `xpMelee -> xpDodgeEvent -> increaseStat` never turns 2x into 4x. Damage, speed and enemy-skill parameters are untouched
  - **Never grants AI XP sources the base game would not give** (e.g. running for non-player factions); training conditions, race multipliers and level caps remain controlled by the game
  - Enabled only when all 12 XP hooks install successfully; if any fails, everything stays at normal speed and the tab reports it as unavailable
- **Building placement bypass**: disables the placement validity check (slope / overlap / indoors no longer block); preview colour stays truthful so you can place anywhere (**known unfixed bugs - back up your save first**)
- **Bilingual UI**: follows the current game language
- **Compatibility**: runs alongside the Dust graphics mod and other RE_Kenshi plugins

### UI and settings

- The entry point is an **Open/Close Panel** window anchored at the top-right of the viewport (draggable); the main panel opens at its default position and is clamped back into the viewport if it ends up off-screen
- The main panel supports title-bar drag, X close, scrollbars, text inputs and dropdowns; while open, the game ignores world input
- Feature preferences are saved in `KenshiTrainer.ini` next to the DLL under `[Features]`: building bypass, item category, spawn quantity, weapon/armour/prosthetic level, XP multiplier and both faction switches. Changes are written in the same frame, failed writes retry every 5 seconds, and missing or out-of-range values fall back to defaults
- Window layout is kept separately in `KenshiTrainer_imgui.ini` in the same folder
- **Keep both ini files** when updating the DLL, and **never ship them** to the workshop (they are local settings)

### Known limitations

- The building-placement bypass still has unfixed bugs where the preview and the real placement result can disagree - back up your save first
- The physique lock lives only for the current appearance object; reload, restart or character reload requires setting it again
- Total blood and body-part health do not heal each other: blood does not repair parts, part healing does not refill blood, and neither clears the KO timer
- Severed or crushed limbs cannot have their health edited directly
- Prosthetics whose side and arm/leg type cannot be identified are not offered for any limb
- The XP multiplier only covers the game's existing XP entry points; other mods that bypass them entirely are unaffected

### Requirements

- Kenshi **1.0.65** (Steam / GOG)
- [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) v0.3.5 or newer (must be installed first)

### Install

1. Install RE_Kenshi per its README and confirm the version shows on the main menu
2. Copy the `mods/KenshiTrainer/` folder into `Kenshi/mods/`
3. Enable KenshiTrainer in the launcher Mods page
4. In game, click **Open Panel** at the top-right to open the trainer

### Building from source

Visual Studio 2019+ with the v100 x64 toolset, Boost 1.60.0 and the KenshiLib deps (see `reference/`).

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

Output is copied to `mods/KenshiTrainer/`. Release|x64 only.

### Troubleshooting

- Check `RE_Kenshi_log.txt` in the game folder for `KenshiTrainer:` lines. Useful ones include `feature settings loaded`, `physique hooks ready (3/3)`, `part health ... result=ok/failed` and the XP hook install count
- Crash reports: `KT_*.txt` and the minidump next to the DLL
- Targeted diagnostics via environment variables: `KT_NOHOOK` disables the whole plugin, `KT_NOXP` skips only the XP hooks, `KT_NOPHYSIQUE` skips only the physique hooks
- Upgrading from a build older than 2026-10-08: those versions had no feature-settings persistence, so set your preferences once in the new build
- Windows Smart App Control may block the plugin

### License

GPL-3.0 (plugins using KenshiLib must be GPLv3).

### Credits

- [RE_Kenshi / KenshiLib](https://github.com/BFrizzleFoShizzle/RE_Kenshi) by BFrizzleFoShizzle
- [Dear ImGui](https://github.com/ocornut/imgui)
