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

## 修复方案（待批准）

给 Overlay 加 **shadow vtable 漂移重同步**：SCRec 记录活 vtable 指针，每次 Present（持帧锁）比对活表 8/13/22 槽与上次快照；发现外部改写（Dust 后装）则把活表重新拷入 shadow、重定向我们的槽、orig 更新为 late hooker 的 detour。效果：任意安装顺序都能链成 外层→内层→真实Present，Dust 不再静默失效。约 20 行，只动 Overlay.cpp。

## 建议验证流程

同时启用两 mod 进游戏：看 `RE_Kenshi_log.txt` 有无 `KenshiTrainer:` 帧阶段日志、Dust 日志有无 `Present #N` 递增（在 Dust.ini 开日志）。若 Dust GUI 不出现而本 mod 正常，即命中场景 (3)。