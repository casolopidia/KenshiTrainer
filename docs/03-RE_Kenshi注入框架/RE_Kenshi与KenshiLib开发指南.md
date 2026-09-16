# RE_Kenshi 与 KenshiLib 开发指南

> 来源：BFrizzleFoShizzle/RE_Kenshi、BFrizzleFoShizzle/KenshiLib、KenshiLib_Examples 仓库 README（2026-09 抓取）

## 一、RE_Kenshi 是什么

通过代码注入扩展 Kenshi 功能的 mod 框架。以 Ogre 渲染插件身份被游戏加载
（`Plugins_x64.cfg` 中 `Plugin=RE_Kenshi`，排在 `RenderSystem_Direct3D11_x64` 之前），
随后扫描 `Kenshi/mods/*/RE_Kenshi.json` 并加载其中声明的插件 DLL。

- 当前版本：**v0.3.5**（2026-08-26 发布）
- 基于 Kenshi **1.0.65** 逆向；**1.0.68** 由安装器生成兼容 exe（放在 `RE_Kenshi` 目录）
- 用 `--norekenshi` 启动参数可运行原版 1.0.68（禁用 RE_Kenshi）
- 设置菜单入口：主菜单或游戏内 Esc → Options → Mods → RE_Kenshi Settings
- 安装成功的标志：主菜单界面显示 RE_Kenshi 版本号

### 安装（推荐安装器）

1. 下载 `RE_Kenshi_v0.3.5.zip`，解压运行其中的安装器 exe
2. 选择 Kenshi 安装目录，按提示完成
3. 正常启动 Kenshi，确认主菜单出现 RE_Kenshi 版本号

### 手动安装（1.0.65 Steam/GOG）

把 loose 包 `install` 目录下的以下内容复制到 Kenshi 根目录：
`RE_Kenshi.dll`、`KenshiLib.dll`、`CompressToolsLib.dll`、完整 `RE_Kenshi/` 目录
（含 `locale`、`RVAs`、`game_speed_tutorial.png`），
然后在 `Plugins_x64.cfg` 的 `Plugin=RenderSystem_Direct3D11_x64` **之前**加一行 `Plugin=RE_Kenshi`。

### 卸载

运行安装器选 Uninstall；手动卸载从 `Plugins_x64.cfg` 删除 `Plugin=RE_Kenshi` 即可。

## 二、KenshiLib 是什么

逆向重建 Kenshi 内部类结构与方法地址的 SDK，导出**版本无关**的 API：
读变量、调函数、挂 hook。插件开发者**不需要**自己编译 RE_Kenshi/KenshiLib，
用预编译版即可。

## 三、插件开发环境搭建

1. 安装 **Visual Studio 2019 或更新** + **Visual C++ 2010 x64 编译器（v100 工具链，强制）**
   （VS2010 及 SP1 可从 Wayback Machine 获取；新项目 IDE 里把 Platform Toolset 设为 v100）
2. 安装 **DirectX SDK June 2010**（编译 RE_Kenshi 本体需要；写插件一般用不到）
3. **Boost 1.60.0**——直接用 KenshiLib_Examples_deps 仓库里的预编译版
4. 最简单的起步方式：克隆 **KenshiLib_Examples**（含全部示例工程 +
   Examples_deps 依赖仓库，含 KenshiLib 头文件与预编译 lib：KenshiLib/OgreMain/Boost 等）
5. 工程设置：
   - 附加包含目录：KenshiLib 的 `Include/` + Boost 1.60.0 头文件
   - 链接器输入：`KenshiLib.lib` + `OgreMain.lib`（都在 Examples_deps 里）
6. **只用 RELEASE 模式编译**（DEBUG 当前是坏的）

### 插件最佳实践（官方强调）

- ✅ **必须**用 KenshiLib 自带的 `AddHook` hook 系统——专为多插件 hook 同一函数设计
- ❌ **不要**用第三方 hook/detour 库（MinHook 等）——多插件会互相冲突
- ❌ **不要**在非 UI 线程访问 UI——MyGUI 大多函数非线程安全，会引发偶发崩溃
- 若插件要向其他插件导出函数，应做成 **Preload Plugin** 保证先加载

## 四、插件的构成与加载

一个插件 mod 目录（放在 `Kenshi/mods/` 下）：

```
mods/MyPlugin/
├── RE_Kenshi.json      ← { "Plugins": [ "MyPlugin.dll" ] }
├── MyPlugin.mod        ← FCS 生成的（空）mod 文件，让它出现在 Mods 列表里被勾选
└── MyPlugin.dll        ← 编译产物（导出 startPlugin 函数）
```

DLL 入口（HelloWorld 示例全文）：

```cpp
#include <Debug.h>

__declspec(dllexport) void startPlugin()
{
    DebugLog("Hello world!");
}
```

安装后运行 RE_Kenshi，在 Kenshi 启动器的 Mods 页勾选启用。

## 五、Hook 与 UI 创建的标准模式（KillButton 示例）

完整源码已存本地 `reference/KenshiLib_Examples/KillButton/KillButton.cpp`，要点：

```cpp
// 1. 保存原函数指针
TitleScreen* (*TitleScreen_orig)(TitleScreen*) = NULL;

// 2. hook 函数：先调原函数，再在 UI 线程里安全创建控件
TitleScreen* TitleScreen_hook(TitleScreen* thisptr)
{
    TitleScreen* titleScreen = TitleScreen_orig(thisptr);
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    MyGUI::Window* window = gui->createWidgetReal<MyGUI::Window>(
        "Kenshi_WindowCX", 0.25, 0.25, 0.30, 0.33,
        MyGUI::Align::Center, "Window", "KillWindow");
    window->setCaption("Kill Button");
    MyGUI::Button* btn = window->getClientWidget()->createWidgetReal<MyGUI::Button>(
        "Kenshi_Button1", 0.1, 0.1, 0.8, 0.8, MyGUI::Align::Center, "KillButton");
    btn->eventMouseButtonClick += MyGUI::newDelegate(OnButtonPress);
    return titleScreen;
}

// 3. startPlugin 里安装 hook
__declspec(dllexport) void startPlugin()
{
    KenshiLib::AddHook(KenshiLib::GetRealAddress(&TitleScreen::_CONSTRUCTOR),
                       TitleScreen_hook, &TitleScreen_orig);
}
```

- 取选中角色：`Character* c = ou->player->selectedCharacter.getCharacter();`（用前判空）
- MyGUI 控件皮肤用游戏自带的 `Kenshi_WindowCX` / `Kenshi_Button1`，视觉与原生一致

## 六、示例插件清单（KenshiLib_Examples 仓库）

| 示例 | 内容 | 对本项目的价值 |
|---|---|---|
| HelloWorld | 最小插件骨架 | 工程模板 |
| **KillButton** | MyGUI 窗口+按钮，杀选中角色 | **UI 创建/入口按钮/选中角色——直接可抄** |
| Dialogue Extensions | 自定义对话条件/效果 + GameData 自定义属性 | 进阶 |
| World States Variables | FCS 自定义数据 + 存档持久化 | 持久化参考 |
| Plugin import/export | 插件间通信、PreloadPlugin | 架构参考 |
| Character Highlight | 材质/shader 属性修改 | 渲染参考 |

## 七、辅助工具：FCS_extended

BFrizzleFoShizzle/FCS_extended——给官方 FCS 编辑器打补丁，支持从 mod 目录加载插件和
def 文件补丁（仅 mod 开发者需要）。运行 `FCS_extended.exe` 启动。Wiki 有
Def file patches / Mod def files / Plugins 四页文档。

## 八、离线 API 参考（重要）

`ReKenshi_KenshiLib_Swagger`（regulareverydaynormalmazaf 维护）：
单文件离线 HTML（`ReKenshi_KLIB_Navigator.html`），收录 Kenshi 1.0.65 逆向头文件的
全部函数/字段声明、RVA 地址、vtable 槽位、值域（枚举范围）、hook 骨架与真实代码示例。
开发时**必备**——下载后双击即可用，无需联网。
