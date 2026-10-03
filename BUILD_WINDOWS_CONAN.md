# GameSWF Windows 构建指南 (使用 Conan)

本文档介绍如何使用 **Conan** 包管理器在 Windows 系统上编译 GameSWF。

## 前置要求

1. **Visual Studio 2022** (或 2019)
   - 需要安装 "使用 C++ 的桌面开发" 工作负载
   - 下载地址: https://visualstudio.microsoft.com/

2. **CMake** (3.16 或更高版本)
   - 下载地址: https://cmake.org/download/
   - 安装时选择 "Add CMake to the system PATH"

3. **Conan** (2.0 或更高版本)
   - 安装命令: `pip install conan`
   - 或下载: https://conan.io/downloads

4. **Python** (3.8+，用于运行 Conan)
   - 下载地址: https://www.python.org/downloads/

## 快速开始

### 1. 安装 Conan

```cmd
pip install conan
```

### 2. 配置 Conan (首次使用)

```cmd
# 检测默认配置
conan profile detect --force

# 添加官方仓库
conan remote add conancenter https://center.conan.io
```

### 3. 编译项目

打开 **"x64 Native Tools Command Prompt for VS 2022"**，然后：

```cmd
cd d:\Downloads\gameswf-master

# 运行构建脚本
build_conan.bat
```

或手动构建：

```cmd
# 1. 安装依赖
conan install . --output-folder=build_conan --build=missing --settings=build_type=Release

# 2. 配置 CMake
cd build_conan
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_PREFIX_PATH="%CD%"

# 3. 编译
nmake
```

### 4. 运行程序

```cmd
gameswf_test_ogl.exe gameswf\samples\gameswf_logo.swf
```

## 构建选项

### Debug 构建

```cmd
build_conan.bat debug
```

或手动：
```cmd
conan install . --output-folder=build_conan --build=missing --settings=build_type=Debug
cd build_conan
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
nmake
```

### 使用 Visual Studio 项目文件

```cmd
conan install . --output-folder=build_vs --build=missing

cd build_vs
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
```

然后打开 `build_vs\gameswf.sln` 进行编译。

## 文件说明

| 文件 | 说明 |
|------|------|
| `conanfile.txt` | Conan 依赖配置文件 |
| `CMakeLists.conan.txt` | 配合 Conan 使用的 CMake 配置 |
| `build_conan.bat` | 使用 Conan 的自动构建脚本 |
| `BUILD_WINDOWS_CONAN.md` | 本文档 |

## Conan 配置说明

### conanfile.txt

```ini
[requires]
sdl/2.30.0
zlib/1.3.1
libpng/1.6.43
libjpeg/9e

[generators]
CMakeDeps
CMakeToolchain

[options]
sdl:shared=False
zlib:shared=False
libpng:shared=False
libjpeg:shared=False
```

- **requires**: 定义依赖包及其版本
- **generators**: 生成 CMake 所需的配置文件
- **options**: 设置为静态链接库

### 修改依赖版本

编辑 `conanfile.txt` 中的版本号，例如：
```ini
sdl/2.28.0
zlib/1.2.13
```

然后重新运行 `conan install`。

## 故障排除

### 错误: "Conan not found"

**解决方案**:
```cmd
pip install conan
```

### 错误: "Package not found"

**解决方案**:
```cmd
conan remote add conancenter https://center.conan.io
```

### 错误: "CMake configuration failed"

**解决方案**:
1. 确认 Conan 安装步骤已完成
2. 检查 `conan_toolchain.cmake` 是否生成
3. 确保在正确的目录运行 CMake

### 清理构建

```cmd
# 删除构建目录
rmdir /s /q build_conan

# 重新构建
build_conan.bat
```

### 更新依赖

```cmd
# 更新所有包到最新版本
conan install . --output-folder=build_conan --build=missing --update
```

## 与 vcpkg 对比

| 特性 | Conan | vcpkg |
|------|-------|-------|
| 包版本控制 | 精确版本指定 | 基于 commit 的版本 |
| 二进制缓存 | 支持 | 支持 |
| 配置复杂度 | 中等 | 简单 |
| 跨平台 | 优秀 | 良好 |
| 社区生态 | 活跃 | 活跃 |

Conan 更适合：
- 需要精确控制依赖版本
- 团队共享配置
- 复杂的构建场景

## 使用方法

编译成功后，使用方式与 vcpkg 版本相同：

```cmd
# 播放 SWF 文件
gameswf_test_ogl.exe movie.swf

# 设置窗口大小
gameswf_test_ogl.exe -w 1024x768 movie.swf

# 播放一次后退出
gameswf_test_ogl.exe -1 movie.swf
```

详细用法请参考 `BUILD_WINDOWS.md` 中的"使用方法"章节。
