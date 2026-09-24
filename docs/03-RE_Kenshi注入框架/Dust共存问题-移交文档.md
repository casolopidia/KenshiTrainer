# Dust 共存问题 · 移交文档

日期：2026-09-24。状态：进程级共存已解决并验证；**双开时修改器 ImGui 界面可用**未解决，下一步走 OGRE 内部路线（见文末行动清单）。

## 问题定义

KenshiTrainer（本仓库，RE_Kenshi/KenshiLib 插件）与 Dust（创意工坊 id 3703458265，v0.7.5.3，安装于 `D:\Program Files (x86)\Steam\steamapps\workshop\content\233860\3703458265`）同时启用时：

- 原状：游戏启动 2~3 秒内无声死亡（无 WER、无崩溃转储、退出码 0）
- 目标：双开稳定 **且** 修改器界面照常可用

## 已验证的事实（游戏内实验矩阵，全部有日志存档）

| # | 配置 | 结果 |
|---|------|------|
| 1 | 只装 Dust（重命名我们的 RE_Kenshi.json） | 正常。DustBoot 三钩全装（452~650 字节日志），Dust F11 GUI 就绪 |
| 2 | 我们 DLL 加载但 startPlugin 立即返回（KT_NOHOOK=1） | 正常 → 排除"DLL 存在即崩" |
| 3 | 只装 4 个游戏函数钩（bypass，KT_NOBYPASS 未设 + KT_NOOVERLAY=1） | 正常 |
| 4 | 只钩 D3D11CreateDevice（Dust 不碰的导出） | 正常 |
| 5 | 只钩 D3D11CreateDeviceAndSwapChain（Dust 同样钩） | **必崩**，死在 DustBoot 的第一个 AddHook（日志止于工厂地址打印） |
| 6 | 配置 5 + 调试器（crashdbg） | 不崩，可跑 150 秒+ |

**根因结论**：两个模块通过 KenshiLib 0.5.0 `AddHook` 对**同一 dxgi 导出**下钩即触发链/trampoline 损坏；并发 D3D 初始化期时序决定何时爆发（时序敏感型 Heisenbug，调试器拖慢即可隐藏）。KenshiLib AddHook 的反汇编（`KenshiLib.dll` RVA 0x2390→0x14978）显示其支持同目标链式挂钩，但双钩实际损坏，属 KenshiLib 缺陷。另注意：游戏创建其 DXGI 工厂**早于**一切插件下钩时机（OGRE RenderSystem 初始化在前），所以 swapchain 创建期捕获在双开下不可用。

## 已落地代码（当前 HEAD = 0ce698b，全部已提交）

- `LateHookDecision`（Overlay.cpp）：启动只钩无争议导出（D3D11CreateDevice + CreateDXGIFactory1/v1）；首次触发或游戏窗口出现（WindowWatchdog 线程）时检测 Dust：
  - **无 Dust** → `InstallLegacyCreationHooks()` 装经典三钩（D3D11CDSC + CreateSwapChain + CreateSwapChainForHwnd），单机行为与原版一致（已验证"legacy creation hooks installed"）
  - **有 Dust** → 跳过争议钩，修改器界面休眠（日志"Dust detected - ... overlay dormant"）
- `ShadowHookPresent` + `ResyncShadowVtable`（Overlay.cpp）：swapchain 实例 vptr 重定向到 shadow vtable（挂 Present/Present1）+ 活表 8/13/22 槽漂移重同步（处理 Dust 后装/先装两种顺序）。**拿到 swapchain 指针后这套机制直接可用**。
- `OnSwapChain(sc)`：shadow-hook + 按面积选游戏主 swapchain（g_gameSC）
- 诊断门控：KT_NOHOOK / KT_NOBYPASS（KenshiTrainer.cpp startPlugin）、KT_NOOVERLAY

**双开现状（已验证 90 秒）**：游戏稳定、Dust 全功能（F11 GUI 就绪）、修改器休眠。
**单开现状（已验证）**：legacy 钩安装成功，界面全功能（含义肢品质/等级、断肢不受伤、恢复满血——见提交 38a377c、3f38424）。

## 已尝试并放弃的路线（勿重做）

1. **GetParent 拿工厂重定向 vtable**：游戏工厂不是 GetParent 返回的那个隐式单例，捕获不到。
2. **钩 CreateDXGIFactory1/v1 + 工厂实例 vptr 重定向**：下钩后唯一一次触发（1.96s）是 Dust 临时设备的；游戏工厂在插件加载前已创建，永远追不上。
3. **RenderSystem_Direct3D11_x64.dll IAT 补丁**（CreateDXGIFactory1）：8 字节写与游戏线程并发读 → 真速崩；改为挂起全部线程再写 → VirtualProtect 与挂起线程持有的 MM 锁死锁。
4. **HeapWalk 堆扫描找工厂对象**：与无序列化堆并发 → 堆损坏，进程直接终止。
5. **VirtualQuery 全内存扫描找 swapchain**：调试器下 85 秒扫不完一回合（Sleep 分片后更慢）；且"创建扫描线程"与真速死亡强相关（KT_scan.log 从未创建=线程疑似从未执行，原因未查明），已整体回退（0ce698b）。

## 下一步：OGRE 内部路线（行动清单）

核心思路：**不钩任何争议函数，纯读游戏内存拿到游戏自己持有的 swapchain 指针**，交给现有 OnSwapChain。

1. **定位 `Ogre::Root` 单例**：OgreMain_x64.dll 导出 `??0?$Singleton@VRoot@Ogre@@@Ogre@@QEAA@XZ`（RVA 0x4C090）。反汇编找 `mov [rip+disp], rcx` 形态的 ms_Singleton 写入 → 得静态地址 → `Root* root = *(Root**)msSingletonAddr`。
   - 工具：dumpbin（VS BuildTools 14.51 路径在 tools 历史命令里）、system python + capstone（已装）、PE 解析参考 `$env:TEMP\disasm2.py` 模式。
2. **确定 Root 布局**：本机**无 OgreRoot.h**（deps 只有 ogre 基础头 + RenderSystems\Direct3D11\OgreD3D11RenderWindow.h、OgreMonitorInfo.h）。需对照 Ogre 开源版本（先定版本：Kenshi 2013 年构建 ≈ Ogre 1.8/1.9，可从 OgreMain 字符串/导出核实）。找 `mRenderSystems`（vector<RenderSystem*>）偏移：可从导出的 `??0Root@Ogre@@QEAA@...`（RVA 0x2A2A60）构造函数据成员初始化反推。
3. **拿 RenderWindow → mpSwapChain**：`reference\KenshiLib_Examples_deps\KenshiLib\Include\ogre\RenderSystems\Direct3D11\OgreD3D11RenderWindow.h` 有 `D3D11RenderWindowSwapChainBased`，含 `_getSwapChain()`（返回 `IDXGISwapChainN* mpSwapChain`）。从 D3D11RenderSystem 的 mRenderWindows 列表或 mPrimaryWindow 成员到达（布局同样需反汇编 RenderSystem_Direct3D11_x64.dll 或对照开源头）。
4. **轮询读取**：游戏窗口创建后 swapchain 才存在；开线程每 500ms 纯读指针链（8 字节对齐读天然原子），拿到非空 `IDXGISwapChain*` → 调 `OnSwapChain(sc)` → Present shadow + resync 自动接管 UI。**这一步成功即达成目标**。
5. 全程不新增 KenshiLib AddHook（或只用已验证无争议的导出）；注意 C2712：__try 块不得持有 std::string/vector 局部。

## 验证方法

- 构建：`powershell -ExecutionPolicy Bypass -File tools\build.ps1`（唯一构建命令，MSVC v100）
- **部署：构建只写工作区 `mods\KenshiTrainer\`，必须手动 `Copy-Item` 到游戏目录 `D:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\KenshiTrainer\`**（多次调试坑）
- 双开测试：直接启动 kenshi_x64.exe；禁用 Dust 测试法 = 重命名 workshop 的 RE_Kenshi.json（测完恢复）
- 日志：游戏目录 `RE_Kenshi_log.txt`（找 `KenshiTrainer:` 行）；workshop `logs\`（DustBoot_*.log 4 行截断=死，452+ 字节=活；Dust_*.log 看 Present #N 递增）
- 调试器：`tools\crashdbg\crashdbg.exe <exe>`（自写迷你调试器，抓二次异常+栈；注意调试器会改变时序，Heisenbug 可能消失）
- 崩溃转储：游戏目录 `mods\KenshiTrainer\KT_*.txt`（我们的 UEF 写）

## 项目约束（必读）

- 工具链固定 MSVC v100 + /GL + LTCG；钩虚函数用 `_NV_` 桩；`__try` 内禁可析构局部
- UI 字符串一律走 `tools\gen_medical_strings.py` 模式生成进 ChineseStrings.h，源码禁中文
- Dust 的源码副本在项目 `Dust-main\`（≈ workshop v0.7.5.3，上游 github.com/Bazouz660/Dust）；RE_Kenshi 源码副本在 `reference\RE_Kenshi_repo\`
- 测试是游戏内手动验证；每轮改动定期 commit

## 2026-09-24 补充：OGRE 路线逆向结果（全部完成，代码已落地 a3b122c）

**版本澄清**：游戏 OGRE 是 2.x（导出含 CompositorManager2 / InstancingThreadedCullingMethod），非移交文档此前猜测的 1.8/1.9。本机 KenshiLib deps 的头文件集（OgreRoot.h/OgreRenderTarget.h/OgreD3D11RenderWindow.h 等齐全）与游戏 DLL 类名经 RTTI 比对一致，可用作布局参考。

**已验证偏移链**（每条至少两处独立证据）：

| 项 | 位置 | 证据 |
|----|------|------|
| `Singleton<Root>::msSingleton` | OgreMain_x64.dll RVA **0x909B20** | 构造器 0x4C090 的 `mov [rip+..],rcx`；getSingletonPtr 0x2A2A40 的读取；导出表 `?msSingleton@?$Singleton@VRoot@Ogre@@@Ogre@@` 直接指向 0x909B20 |
| `Root::mRenderSystems` | Root+**0x08**（vector） | getAvailableRenderers 0x2A4E50：`lea rax,[rcx+8]` |
| `Root::mActiveRenderSystem` | Root+**0x28** | getRenderSystem 0x2A5080 |
| `Root::mAutoWindow` | Root+**0x168** | getAutoCreatedWindow 0x2A7590 |
| `WindowEventUtilities::_msWindows` | OgreMain RVA **0x8D9F40**（begin）/0x8D9F48（end）/0x8D9F50（cap） | 导出表 + `_addRenderWindow` push_back 实现 + `_removeRenderWindow` 遍历均引用此三地址 |
| `D3D11RenderWindowHwnd` vtable | RenderSystem_Direct3D11_x64.dll RVA **0x7C548** | RTTI COL 遍历（`.?AVD3D11RenderWindowHwnd@Ogre@@`） |
| `mpSwapChain` | 窗口对象+**0x130** | 3 处：destroy（SetFullscreenState+Release）、GetBuffer(0) 回读、swapBuffers 尾部 `call [rax+0x40]`=Present |

**实现**（Overlay.cpp，commit a3b122c）：Dust 分支不再休眠，启动 `OgreSwapChainWatch` 线程（500ms 轮询）：
1. 主路径：`_msWindows` 找 vptr == RenderSystem 基址+0x7C548 的窗口 → +0x130 取 mpSwapChain
2. 后备：msSingleton → Root+0x168 mAutoWindow → 同上
3. 候选过现有 `ValidateSwapChainCandidate`（dxgi vtable 范围 + GetDesc + 窗口归属 + 排除 DustBootDummy）
4. 命中 → `OnSwapChain(sc)`（shadow vtable + 漂移重同步机制原样复用，与 Dust 活表钩任意顺序兼容）
5. 持续轮询以捕获 swapchain 重建（全屏切换等）

预期日志序列（双开）：`Dust detected - ... using OGRE-internal swapchain discovery` → `OGRE swapchain watch started` → `swapchain found via OGRE internals: 0x...` → `Present shadow-hooked on swapchain`（可能出现 `shadow vtable resynced`）。

**待游戏内验证**：双开时修改器界面可用且 Dust 全功能；单开回归（legacy 路径未动）。验证通过后更新 docs/08 发布说明。
