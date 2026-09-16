# FCS（Forgotten Construction Set）使用指南

> 来源：orukoharin.com 图文教程、Kenshi 中文维基（灰机 wiki）Mod 制作系列、3DM 汉化讨论帖。
> 对本项目的作用：① 创建修改器插件需要的空 `.mod` 文件；② 查阅物品/角色/装备的字段含义与合法取值。

## 一、FCS 是什么

Lo-Fi Games 官方随游戏附带的 mod 制作工具。不需要编程即可：调整数值、添加物品、
添加开局剧本、改角色属性模板等。所有修改只写入 mod 文件，不影响游戏本体数据。

### 启动方式

- Steam 库中点 Kenshi「启动」→ 选 **Modding tool**；或
- 直接运行游戏目录下的 forgotten construction set
  （默认 `C:\Program Files (x86)\Steam\steamapps\common\Kenshi`）

### 汉化

编辑器界面仅英文。3DM 论坛有 fcs.def / fcs_layout.def 汉化文件
（放入 Kenshi 目录即可，链接见《链接索引》）；GitHub 上有 1875 行的 fcs_chs.def
中文定义文件可作字段对照表用。

## 二、基本操作流程

1. **打开 mod**：启动后窗口上段 4 个是游戏基础数据（gamedata.base 等），
   下段是 mod 列表。点 **Create new mod files** 创建新 mod，命名后 DONE 进入编辑。
2. **编辑时只勾选自己的 mod**：右下角按钮取消其他文件勾选——
   否则会产生依赖关联，单独使用 mod 时弹警告。
3. **主界面**：左侧是分类（Items / Characters / Races / Weapons / Armour …），
   选中分类后右侧列出该分类全部对象，双击进入编辑面板。
4. **复制对象**：做新物品/角色时，通常先 Duplicate 一个相近的原版对象再改，省去逐项配置。

## 三、与本项目直接相关的字段知识（角色属性）

摘自中文维基《Mod制作：角色（FCS）》：

- **stats**（优先级最高）：直接指定角色各项属性数值，覆盖下列所有快捷字段；
  `stats randomise`（属性随机值）在使用 stats 后依然生效。
- **combat stats**：批量决定战斗相关技能——力量、韧性、敏捷、暗杀(×0.5)、近战攻防、
  躲闪(×0.1)、武术(×0.1)、奔跑、游泳、武器技能、战地医生、炮塔、精准射击。
- **ranged stats**：覆盖远程相关——知觉、炮塔、十字弩、精准射击。
- **stealth stats**：覆盖潜行相关——敏捷、潜行、撬锁、偷窃、暗杀、战地医生。
- **unarmed stats**：覆盖徒手——敏捷、躲闪、武术。
- **strength**：单独覆盖力量。
- 优先级：stats > strength/ranged/stealth/unarmed > combat stats。

### 角色引用字段（与背包生成相关）

- **backpack**：角色生成时携带的背包（引用背包物品）。
- **weapons** + **weapon level**：weapon level 选「制造者」（如刃行者），
  游戏在该制造者的模型中随机——**无法直接指定「刃之三」**；weapons 有值但
  weapon level 为空时制造者默认为「古人」。
- **blueprints**：角色物品栏中的蓝图（引用研究项目，而非装备）。
- **death items**：从存活/昏迷角色身上移除即致死的物品。

### 护甲品质枚举（armour grade）

`GEAR_PROTOTYPE` 模板 → `GEAR_CHEAP` 赝品 → `GEAR_STANDARD` 标准 →
`GEAR_GOOD` 高 → `GEAR_QUALITY` 专家 → `GEAR_MASTER` 杰作。

## 四、为本项目创建空 .mod 文件

修改器插件需要一个 `.mod` 文件才能出现在 Kenshi 启动器 Mods 列表：

1. FCS → Create new mod files → 命名（如 `KenshiTrainer`）→ DONE
2. 不做任何数据修改，直接保存退出
3. 生成的 `.mod` 文件位置：Kenshi 安装目录下 `mods/KenshiTrainer/` 或
   `Kenshi/mods/`（FCS 版本不同位置略异），把它和 `RE_Kenshi.json`、插件 DLL 放同一目录

## 五、进阶：FCS_extended

BFrizzleFoShizzle 的 FCS 补丁，允许从 mod 目录加载插件（.dll/.cs）和 def 文件补丁，
用来给 FCS 编辑器本身加功能。仅当需要自定义 GameData 类型（如修改器配置持久化到
存档/到 FCS 数据）时才需要。运行方式：解压到 Kenshi 目录后用 `FCS_extended.exe` 启动。

## 六、配套工具：OpenConstructionSet（C#）

如需离线解析 `.mod` 文件（例如做一个"物品目录浏览器"桌面工具辅助设计），
可用 NuGet 包 `OpenConstructionSet`：

- 定位 Steam/GOG 安装目录、Workshop 目录
- 读取启用的 mod 列表与加载顺序
- `ModDataContext` 加载多个基础/激活 mod 进行编辑保存（类似 FCS 的数据上下文）

> 注意：修改器本体**不需要**它——运行时 GameDataManager 已合并全部启用 mod 的数据，
> 直接枚举即可（见《关键API速查》）。OCS 仅用于开发期的离线分析工具。
