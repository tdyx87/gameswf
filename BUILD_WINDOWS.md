# GameSWF Windows 构建指南

本文档介绍如何在 Windows 系统上编译 GameSWF 并生成可执行文件。

## 前置要求

1. **Visual Studio 2022** (或 2019)
   - 需要安装 "使用 C++ 的桌面开发" 工作负载
   - 下载地址: https://visualstudio.microsoft.com/

2. **CMake** (3.16 或更高版本)
   - 下载地址: https://cmake.org/download/
   - 安装时选择 "Add CMake to the system PATH"

3. **Git** (用于安装 vcpkg)
   - 下载地址: https://git-scm.com/download/win

## 快速开始

### 方法 1: 使用自动脚本 (推荐)

1. **安装依赖项**
   ```cmd
   setup_dependencies.bat
   ```
   这个脚本会自动：
   - 安装 vcpkg (如果尚未安装)
   - 安装所需的库 (SDL2, zlib, libpng, libjpeg-turbo)

2. **编译项目**
   ```cmd
   # 打开 "x64 Native Tools Command Prompt for VS 2022"
   # 然后运行：
   build.bat
   ```

3. **运行程序**
   ```cmd
   gameswf_test_ogl.exe gameswf\samples\gameswf_logo.swf
   ```

### 方法 2: 手动构建

#### 步骤 1: 安装 vcpkg 和依赖项

```cmd
# 克隆 vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
bootstrap-vcpkg.bat

# 设置环境变量
setx VCPKG_ROOT "C:\vcpkg"

# 安装依赖项
vcpkg install sdl2:x64-windows zlib:x64-windows libpng:x64-windows libjpeg-turbo:x64-windows
```

#### 步骤 2: 编译 GameSWF

```cmd
# 打开 "x64 Native Tools Command Prompt for VS 2022"
cd d:\Downloads\gameswf-master

# 创建构建目录
mkdir build
cd build

# 配置
cmake .. -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release

# 编译
nmake
```

#### 步骤 3: 运行

```cmd
cd bin
gameswf_test_ogl.exe ..\gameswf\samples\gameswf_logo.swf
```

## 构建选项

### Debug 构建

```cmd
build.bat debug
```

或手动：
```cmd
cmake .. -DCMAKE_BUILD_TYPE=Debug
nmake
```

### 使用 Visual Studio 项目文件

如果你想生成 Visual Studio 解决方案：

```cmd
mkdir build_vs
cd build_vs
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
```

然后打开 `build_vs\gameswf.sln` 进行编译。

## 输出文件

编译成功后，可执行文件位于：
- `build_windows\bin\gameswf_test_ogl.exe`
- `gameswf_test_ogl.exe` (复制到项目根目录)

## 使用方法

```cmd
gameswf_test_ogl.exe [选项] movie.swf
```

### 常用选项

| 选项 | 说明 |
|------|------|
| `-h` | 显示帮助 |
| `-v` | 详细输出 |
| `-w WxH` | 设置窗口大小，如 `-w 1024x768` |
| `-1` | 播放一次后退出 |
| `-f` | 强制实时帧率 |

### 键盘控制

| 按键 | 功能 |
|------|------|
| ESC / Ctrl+Q | 退出 |
| Ctrl+P | 暂停/继续 |
| Ctrl+[ / Ctrl+] | 上一帧/下一帧 |
| Ctrl+A | 切换抗锯齿 |
| Ctrl+T | 显示线框模式 |

### 示例

```cmd
# 播放示例文件
gameswf_test_ogl.exe gameswf\samples\gameswf_logo.swf

# 以 1024x768 分辨率播放
gameswf_test_ogl.exe -w 1024x768 gameswf\samples\test_gradients_alpha.swf

# 播放一次后退出
gameswf_test_ogl.exe -1 gameswf\samples\test_shape_tweening.swf
```

## 故障排除

### 错误: "Visual Studio compiler not found!"

**解决方案**: 确保从 "x64 Native Tools Command Prompt for VS 2022" 运行脚本，而不是普通命令提示符。

### 错误: "CMake configuration failed!"

**解决方案**: 
1. 检查 vcpkg 是否正确安装
2. 确认依赖项已安装: `vcpkg list | findstr sdl2`
3. 检查 `VCPKG_ROOT` 环境变量是否设置

### 错误: "SDL2 not found"

**解决方案**:
```cmd
vcpkg install sdl2:x64-windows
```

### 运行时缺少 DLL

如果运行时提示缺少 SDL2.dll 等文件，可以：
1. 将 `C:\vcpkg\installed\x64-windows\bin` 中的 DLL 文件复制到可执行文件目录
2. 或将该目录添加到系统 PATH

## 文件说明

| 文件 | 说明 |
|------|------|
| `build.bat` | Windows 构建脚本 |
| `setup_dependencies.bat` | 依赖项安装脚本 |
| `CMakeLists.txt` | CMake 配置文件 (已添加 Windows 支持) |
| `CMakeLists.windows.txt` | Windows 专用 CMake 配置 (备用) |

## 技术细节

- 使用 **CMake** 作为构建系统
- 使用 **vcpkg** 管理依赖项
- 支持 **Visual Studio 2019/2022**
- 编译为 **64 位** 可执行文件
- 使用 **静态链接** 运行时库 (MSVC)
