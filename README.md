# raycast-2d

一个在终端中运行的 C++17 光线投射演示。在二维地图里移动和转向，终端会用 ASCII 字符绘制第一人称视图。

## 一键构建并运行

需要 **CMake 3.16 或更高版本**和支持 C++17 的编译器。Windows 可安装 Visual Studio 的“使用 C++ 的桌面开发”工作负载，Linux 可使用 GCC 或 Clang，macOS 可使用 Xcode Command Line Tools。先在终端运行 `cmake --version`，确认 CMake 已加入 PATH。

| 平台 | 构建并启动 | 仅构建，不进入游戏 |
| --- | --- | --- |
| Windows | 双击 [run.cmd](run.cmd)，或执行 `.\run.cmd` | `.\run.cmd --build-only` |
| Linux / macOS | `sh ./run.sh` | `sh ./run.sh --build-only` |

以上命令从项目根目录执行。脚本会自动选择本机 CMake 生成器，以 Release 配置构建 `raycast-2d`。首次运行会创建 `out/play/`，之后会复用该目录进行增量构建。编译失败时不会启动旧版本。

## 操作

| 按键 | 动作 |
| --- | --- |
| W / S | 前进 / 后退 |
| A / D | 向左 / 向右平移 |
| 单击左键（Windows） | 锁定并隐藏鼠标；移动鼠标转向或俯仰，Esc 释放 |
| 空格 | 跳跃；按住 W 再点空格可向前跃过缺口 |
| Q | 退出 |

默认地图 **Skyway** 是折返跳台：从起点向东越过几组悬台，在远端箱子处向南转，再沿另一排平台向西跳到信标。失足掉入深坑会回到出生点。原来的工业场景仍可在配置中选择 `maps/foundry.map`。

请在交互式终端中运行，而不是在 IDE 的只读输出面板中运行。

## 字号与画面精度

默认字符高度为 8 像素。在 `assets/game.cfg` 中修改 `font_height`（4–32，0 保持原字号）。支持调整字体的 Windows 控制台只缩小字号，正常退出时恢复；自行管理字体的终端可在启动前手动缩小字号。

## 编辑地图和贴图

启动脚本读取 `assets/game.cfg`；`map` 和 `textures` 路径相对于配置文件。复制配置后可用 `.\run.cmd --config assets/other.cfg` 启动。直接运行 exe 则默认读取 exe 旁边的 `assets/game.cfg`。修改资源后重启即可，不需要重新编译。

- `assets/maps/foundry.map`：`tile 符号 地面高度 墙贴图ID 地面贴图ID`（墙 ID 为 0 表示可行走）。`row` 后面是一行等宽地图。`spawn x y dx dy` 设置出生点和朝向；`prop 贴图ID x y 宽 高` 把物品放到当地地面上。高度以一格为单位，单步最多上升 0.26 格。
- `assets/textures/industrial.atlas`：每张纹理以 `ID ANSI颜色 宽 高` 开头，后接指定行数的等宽 ASCII 图案；颜色为 0–15，物品的空格透明。编辑时不要裁掉图案行尾的空格。
- 出生点正前方的 `a → b → c` 是三级台阶；`d` 是低于普通地面的小坑。

## 手动构建

```sh
cmake -S . -B out/play -DCMAKE_BUILD_TYPE=Release
cmake --build out/play --config Release --target raycast-2d
```

Visual Studio 通常会生成 `out/play/Release/raycast-2d.exe`，单配置生成器通常会生成 `out/play/raycast-2d`（Windows 加 `.exe`）。只需运行时，直接启动对应的可执行文件即可。

如果 CMake 报告缓存中的源码路径或生成器不匹配，请将两条手动构建命令中的 `out/play` 换成同一个新的空目录，然后从新目录启动。
