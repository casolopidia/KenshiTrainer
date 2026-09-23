# 与 Dust 同时启用的冲突分析

日期：2026-09-23。状态：源码级排查完成；修复方案待批准。

## 两边 hook 架构对比

| 层 | KenshiTrainer | Dust |
|----|---------------|------|
| D3D11 入口 | `AddHook` 链 hook `D3D11CreateDeviceAndSwapChain` + 工厂 `CreateSwapChain*` | 临时设备取地址后 `AddHook` 链 hook 大量设备上下文函数（Draw*/Map/OMSetRT/PSSetSRV/RSSetVP/ClearState 等） |
| Present | **实例 vptr 重定向到 shadow vtable**（拷贝当时的活 vtable，只改 8/22 槽） | **直接写活 vtable** 的 8/13/22 槽（VirtualProtect），保存旧值为 original |
| WndProc | 独自子类化游戏窗口（SetWindowLongPtr） | 不碰 WndProc，走游戏 MyGUI |
| 异常过滤 | `TrainerUEF`，链到上一个 | `DustShutdownExceptionFilter`，链到上一个 |
| ImGui | 私有 imgui 1.86 副本，独立上下文 | 自带 imgui 副本，独立上下文 |

## 冲突结论

**硬崩溃风险：低。** 所有 hook 都经 KenshiLib::AddHook 链或保存 original 后链接，无互踩内存、无 WndProc 争夺、两个 ImGui 上下文互相隔离、UEF 双向链接。

**功能冲突：有，且取决于安装先后顺序：**

1. 我们在 **CreateSwapChain 时**就给游戏 swapchain 装上 shadow vtable（vptr 重定向）。
2. Dust 在**设备被首次调用捕获后**才把 detour 写进**活 vtable**。
3. 时序上 (1) 几乎必然早于 (2) → 对象 vptr 已指向我们的 shadow，Dust 写活 vtable 永远不触发 → **Dust 的 Present/ResizeBuffers hook 失效，Dust GUI 不初始化、帧边界逻辑停摆**（Dust 的 TryRecoverPresent 会反复"成功"重写活 vtable，但调用永远到不了）。
4. 反过来（Dust 先写活 vtable，我们后拷 shadow）则完全兼容：shadow 拷贝里已含 Dust detour，形成 我们→Dust→真实Present 的链，两个 GUI 共存。

**次级风险（两 GUI 共存时）：** 我们的 ImGui 绘制流量（Map/DrawIndexed/VSSetShader 等）会穿过 Dust 的设备上下文 hook。Dust 按游戏着色器签名做分发，我们的 UI 绘制不匹配签名应 mostly 透传；但其逐绘制统计/调查记录会被污染，个别效果（如相机 CB 提取）理论上可能误判。Dust 对自己的 GUI 绘制有豁免，对外部 ImGui 没有。

## 实测根因（2026-09-23 深夜，多轮游戏内二分定位）

给 Overlay 加 **shadow vtable 漂移重同步**：SCRec 记录活 vtable 指针，每次 Present（持帧锁）比对活表 8/13/22 槽与上次快照；发现外部改写（Dust 后装）则把活表重新拷入 shadow、重定向我们的槽、orig 更新为 late hooker 的 detour。效果：任意安装顺序都能链成 外层→内层→真实Present，Dust 不再静默失效。已落地：Overlay.cpp 的 SCRec 记录活 vtable 指针与 8/13/22 槽快照，每次 Present（持帧锁）比对，漂移则重拷活表入 shadow、orig 更新为后装者 detour。日志出现 "shadow vtable resynced" 即命中场景 (3) 并被修复。

## 建议验证流程

同时启用两 mod 进游戏：看 `RE_Kenshi_log.txt` 有无 `KenshiTrainer:` 帧阶段日志、Dust 日志有无 `Present #N` 递增（在 Dust.ini 开日志）。若 Dust GUI 不出现而本 mod 正常，即命中场景 (3)。

## 最终结论与落地方案

**根因（游戏内二分 + Heisenbug 特征证实）**：两个模块通过 KenshiLib AddHook 对**同一 dxgi 导出**
（D3D11CreateDeviceAndSwapChain / CreateSwapChain / CreateSwapChainForHwnd）下钩即崩溃。证据链：

- 只装 Dust：游戏正常，DustBoot 三钩全部成功（650 字节日志）。
- 我们的 DLL 完全惰性（startPlugin 立即返回）：正常 —— 排除"DLL 存在即崩"。
- 只开 bypass（4 个游戏函数钩）：正常 —— 游戏函数地址不与 Dust 冲突。
- 只开 D3D11CreateDevice 钩（Dust 不碰的导出）：正常。
- 只开 D3D11CreateDeviceAndSwapChain 钩（Dust 同样要钩）：必崩，死在 DustBoot 的第一个 AddHook。
- 同一必崩配置挂在调试器下（拖慢初始化时序）：不崩 —— 时序敏感型链/trampoline 损坏。

**尝试过的替代捕获方案（均放弃）**：
- 从 device GetParent 拿工厂重定向 vtable：游戏工厂不是这个对象，捕获不到。
- 钩 CreateDXGIFactory1/CreateDXGIFactory 重定向工厂实例 vtable：1.96s 那次创建是 Dust 临时设备的，游戏的工厂在我们下钩之前就已创建，永远追不上。
- RenderSystem IAT 补丁：真实速度下崩（写 8 字节期间游戏线程并发读 IAT）；挂起全部线程后再写又会与 VirtualProtect 死锁（MM 锁被挂起线程持有）。
- 堆扫描找工厂对象：HeapWalk 与无锁堆并发触发堆损坏，进程直接终止。

**落地（已提交）**：Overlay 启动只钩无争议的 D3D11CreateDevice + CreateDXGIFactory(1)；首次触发或游戏窗口出现（看门线程）时做延迟决策：
- 无 Dust → 装经典创建钩（原行为，单机已验证"legacy creation hooks installed"）。
- 有 Dust → 跳过争议钩，修改器静默休眠，Dust 全功能。代价：与 Dust 同启时修改器界面不可用；游戏稳定不崩。

TODO（后续可研究）：从 OGRE/KenshiLib 侧拿游戏 RenderSystem 的 mSwapChain 指针（游戏内全局），或与 Dust 作者协同（上游修 KenshiLib 双钩同一导出的问题）。
