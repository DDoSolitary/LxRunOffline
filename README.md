# LxRunOffline

![Build status](https://github.com/DDoSolitary/LxRunOffline/workflows/.github/workflows/build.yml/badge.svg)
[![Chocolatey](https://img.shields.io/chocolatey/v/lxrunoffline.svg)](https://chocolatey.org/packages/lxrunoffline)

A full-featured utility for managing *Windows Subsystem for Linux (WSL)*.

# Donation

It would be greatly appreciated if you could make a donation to support development of this project.

PayPal [![](https://www.paypalobjects.com/en_US/i/btn/btn_donate_LG.gif)](https://www.paypal.me/ddosolitary)

Alipay

![Alipay](https://image.ibb.co/kkxV99/1537608529099_20180922174914623.jpg)

# Features

- Install any Linux distro to any directory on your computer.
- Move an existing installation to another directory.
- Duplicate(copy) an existing installation.
- Register an existing installation directory. This enables you to install to a USB stick and use it on different computers.
- Run arbitrary Linux commands in a specified installation.
- Configure default user, environment variables and [various flags](https://docs.microsoft.com/en-us/previous-versions/windows/desktop/api/wslapi/ne-wslapi-wsl_distribution_flags).
- Export configuration to an XML file and import from the file.
- Export an installation to a tar file.

# Install

You can install via Chocolatey `choco install lxrunoffline`, Scoop `scoop bucket add extras`, `scoop install lxrunoffline`, or download the binaries directly:
- Latest releases: https://github.com/DDoSolitary/LxRunOffline/releases
- Development builds: https://ddosolitary-builds.sourceforge.io/LxRunOffline/

### Shell extension

The right-click menu feature requires the shell extension DLL to be properly registered. This is automatically done if you used Chocolatey to install this project. However, if you downloaded the binaries directly, you need to run `regsvr32 LxRunOfflineShellExt.dll` manually to register the DLL file.

# Usage

See the [Wiki](https://github.com/DDoSolitary/LxRunOffline/wiki) to download tar files for different distros, which are used by the `LxRunOffline install` command.

Run `LxRunOffline` for available actions and run `LxRunOffline <action-name>` for available arguments/flags.

# Build

This project uses CMake as its build system. MinGW GCC and Visual C++ compilers are supported.

### Visual C++

1. Install Visual Studio 2019, latest Windows SDK, CMake and [vcpkg](https://github.com/Microsoft/vcpkg).

2. Install dependencies.

```
vcpkg install --triplet x64-windows-static libarchive boost-program-options boost-format boost-algorithm boost-test tinyxml2
```

3. Open "x64 Native Tools Command Prompt" from Start Menu and build.

```cmd
mkdir build
cd build
cmake .. ^
    -G "NMake Makefiles" ^
    -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg-dir>/scripts/buildsystems/vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-static
nmake
```

### MinGW

1. Install MSYS2.

2. Open the "MSYS2 MinGW 64-bit" shell from Start menu, and install dependencies.

```bash
pacman -Sy --needed --noconfirm base-devel git mingw-w64-x86_64-{toolchain,cmake,libarchive,boost,tinyxml2}
```

3. Build.

```bash
mkdir build
cd build
cmake .. -G "MSYS Makefiles"
make
```

### Notes

- Other CMake generators like Visual Studio and Ninja may also work, but they're neither tested nor officially supported by this project.
- Static linking is used by default. However, you can define `-DLXRUNOFFLINE_STATIC=OFF` to switch to dynamic linking. If you're building with Visual C++, you also need to change vcpkg's triplet to `x64-windows` when installing dependencies and invoking CMake.
- The build script in [CI configuration](https://github.com/DDoSolitary/LxRunOffline/blob/master/.github/workflows/build.yml) can be used as an example of how to build this project.
- The shell extension uses ATL, which is not supported by MinGW, so it will only be built when using Visual C++.

# Compatibility

- **v1.x**: Only Windows 10 Fall Creators Update (v1709) or earlier.
- **v2.x**: Only Windows 10 Fall Creators Update (v1709) or later.
- **v3.x**: Only Windows 10 April 2018 Update (v1803) or later.

It is strongly recommended to install the April 2018 Update or later and use v3.x releases.
