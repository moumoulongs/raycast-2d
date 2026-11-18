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
| A / D | 向左 / 向右转 |
| Q 或 Esc | 退出 |

请在交互式终端中运行，而不是在 IDE 的只读输出面板中运行。

## 字号与画面精度

在支持程序调整字体的 Windows 控制台中，游戏启动时会尝试把字符高度缩小约 20%（不低于 10 像素，已经更小的保持不变），正常退出时恢复原字号。游戏按启动时可见的行列数绘制。若终端自行管理字号（例如 Windows Terminal），请在启动游戏前先将终端字号调小，再运行游戏。

## 手动构建

```sh
cmake -S . -B out/play -DCMAKE_BUILD_TYPE=Release
cmake --build out/play --config Release --target raycast-2d
```

Visual Studio 通常会生成 `out/play/Release/raycast-2d.exe`，单配置生成器通常会生成 `out/play/raycast-2d`（Windows 加 `.exe`）。只需运行时，直接启动对应的可执行文件即可。

如果 CMake 报告缓存中的源码路径或生成器不匹配，请将两条手动构建命令中的 `out/play` 换成同一个新的空目录，然后从新目录启动。
