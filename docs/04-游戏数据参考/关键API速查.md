# 修改器关键 API 速查

> 全部签名摘自本地 `reference/KenshiLib_include/`（KenshiLib `RE_Kenshi_mods` 分支，
> 对应 Kenshi 1.0.65）。RVA 地址已随头文件保存，KenshiLib 运行时自动解析。

## 一、选中角色

```cpp
#include <kenshi/Globals.h>        // ou 全局对象
#include <kenshi/PlayerInterface.h>

Character* c = ou->player->selectedCharacter.getCharacter();
if (c) { /* ... */ }               // 必须判空：未选中/在主菜单时为 null
```

## 二、属性修改（CharStats.h）

```cpp
CharStats* stats = c->stats;                        // Character 内的属性对象
float& str = stats->getStatRef(STAT_STRENGTH);      // 取引用，直接赋值即修改
str = 100.0f;
float cur = stats->getStat(STAT_STRENGTH, true);    // 读取（unmodified=true 取基础值）
std::string name = CharStats::getStatName(STAT_STRENGTH);  // 属性名（用于 UI 显示）
```

`StatsEnumerated` 枚举（Enums.h，0 起连续，可循环遍历）：

| 枚举 | 含义 | 枚举 | 含义 |
|---|---|---|---|
| STAT_STRENGTH | 力量 | STAT_TOUGHNESS | 韧性 |
| STAT_DEXTERITY | 敏捷 | STAT_MELEE_ATTACK | 近战攻击 |
| STAT_MELEE_DEFENCE | 近战防御 | STAT_MARTIALARTS | 武术 |
| STAT_DODGE | 躲闪 | STAT_STEALTH | 潜行 |
| STAT_THIEVING | 偷窃 | STAT_LOCKPICKING | 撬锁 |
| STAT_ASSASSINATION | 暗杀 | STAT_ATHLETICS | 奔跑 |
| STAT_SWIMMING | 游泳 | STAT_PERCEPTION | 知觉 |
| STAT_TURRETS | 炮塔 | STAT_CROSSBOWS | 十字弩 |
| STAT_KATANAS | 武士刀 | STAT_SABRES | 军刀 |
| STAT_HACKERS | 砍刀 | STAT_HEAVYWEAPONS | 重型武器 |
| STAT_BLUNT | 钝器 | STAT_POLEARMS | 长柄武器 |
| STAT_MEDIC | 战地医生 | STAT_SCIENCE | 科学 |
| STAT_ENGINEERING | 工程 | STAT_ROBOTICS | 机器人学 |
| STAT_SMITHING_WEAPON | 武器锻造 | STAT_SMITHING_ARMOUR | 护甲锻造 |
| STAT_SMITHING_BOW | 弩锻造 | STAT_LABOURING | 劳动 |
| STAT_FARMING | 耕作 | STAT_COOKING | 烹饪 |
| STAT_SURVIVAL | 生存 | STAT_WEAPONS | 武器（综合）|
| STAT_MASSCOMBAT | 群体战斗 | STAT_FRIENDLY_FIRE | 误伤规避 |

`STAT_END` 之后的衍生属性（`_MaxCarryWeight`、`_MaxRunSpeed`、`_DamageResistance` 等）
为计算值，**不适合直接写**。UI 遍历范围用 `for (int i = STAT_NONE+1; i < STAT_END; ++i)`。

## 三、物品生成

### 3.1 枚举全部物品定义（含 mod）

```cpp
#include <kenshi/GameDataManager.h>

lektor<GameData*> list;
GameDataManager* gdm = /* GameWorld 内的 gameDataManager，见 Globals/GameWorld */;
gdm->getDataOfType(list, itemType/*如武器/护甲/物品类型*/);
// getDataOfType 遍历的是运行时合并数据库——所有启用 mod 的定义都在里面
```

相关查询接口：

```cpp
GameData* getData(const std::string& sid);                    // 按 StringID
GameData* getData(const std::string& sid, itemType category); // 按 SID+类别
GameData* getDataByName(const std::string& name, itemType);   // 按显示名
void      getDataOfType(lektor<GameData*>&, itemType);        // 按类别枚举
```

### 3.2 创建物品实例（RootObjectFactory.h）

```cpp
// 完整版（可指定武器材质/等级/派系制服）
Item* createItem(GameData* gd, const hand& handle, GameData* weaponMesh,
                 GameData* matData, int levelOverride, Faction* flagUniform);
// 简化版
Item* createItem(GameData* itemState);
```

### 3.3 放入选中角色背包（Inventory.h）

```cpp
Item* backpack = /* 在 c->inventory 的物品里找 ITEMTYPE_BACKPACK 的 Item */;
Inventory* inv = backpack ? backpack->inventory /*背包内部栏位*/ : c->inventory;

inv->addItem(item, quantity);                          // 自动找位置堆叠
// 或指定坐标：inv->_addItem(item, x, y);
// 兜底版：addItem(item, qty, dropOnFail=true, destroyOnFail=false) 放不下了掉落而非消失
```

> ⚠️ 注意：Inventory.h 里有两组 addItem（不同子类/签名），编译时按实际拿到的
> Inventory* 类型选用。生成装备（武器/护甲）时建议同时验证 createItem 的
> weaponMesh/matData/levelOverride 参数对品质（刃行者/铭刃等）的控制效果。

## 四、其他常用类

| 类 | 用途 | 头文件 |
|---|---|---|
| `GameData` | 一切静态数据定义（物品/角色/建筑模板） | GameData.h |
| `GameDataManager` | 全局数据合并库 | GameDataManager.h |
| `Item` / `Gear` | 物品实例 / 装备实例 | Item.h / Gear.h |
| `MedicalSystem` / `Damages` | 生命值/部位伤害（KillButton 用它杀角色） | MedicalSystem.h / Damages.h |
| `RaceData` | 种族数据（属性乘数） | RaceData.h |
| `ModInfo` | mod 信息 | ModInfo.h |
| `gui/MainBarGUI` | 主界面底部栏（左上角按钮可挂此界面初始化） | gui/MainBarGUI.h |
| `gui/CharacterStatsWindow` | 原生属性面板（可参考其数据绑定） | gui/CharacterStatsWindow.h |
| `gui/InventoryGUI` | 原生背包界面 | gui/InventoryGUI.h |

## 五、待验证事项（开发时实测清单）

- [ ] `getStatRef` 写入后是否被升级/经验系统回写（是否需要 hook 经验结算）
- [ ] GameDataManager 全局实例的稳妥获取路径（`ou->` 链或 GameWorld 成员）
- [ ] createItem 简化版对武器/护甲的品质默认值；levelOverride 取值范围
- [ ] 背包满时 addItem 的失败行为；建议 UI 提示"背包已满，已掉落地面"
- [ ] MyGUI 在 1.0.68 兼容 exe 下的皮肤名是否一致
