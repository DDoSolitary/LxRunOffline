# LxRunOffline

[![Build status](https://img.shields.io/appveyor/ci/ddosolitary/lxrunoffline/master.svg)](https://ci.appveyor.com/project/ddosolitary/lxrunoffline)
[![Chocolatey](https://img.shields.io/chocolatey/v/lxrunoffline.svg)](https://chocolatey.org/packages/lxrunoffline)

A full-featured utility for *Windows Subsystem for Linux (WSL)*.

# Features

- No Internet access required when installing.
- Install any Linux distros to any location on your disk.
- Moving existing installations to other locations.
- Duplicating existing installations. (To create backups.)
- Register existing installation directories. (For portable usage.)
- Running arbitrary Linux commands on installations.
- Configure default user, environment variables and various configuration flags.

# Install

You can install via Chocolatey `choco install lxrunoffline`, or download the binaries directly:
- Latest releases: https://github.com/DDoSolitary/LxRunOffline/releases
- Development builds: https://ci.appveyor.com/project/DDoSolitary/lxrunoffline/build/artifacts

# Usage

See the [Wiki](https://github.com/DDoSolitary/LxRunOffline/wiki) for where to download tar files for different distros, which is required by the `LxRunOffline install` command.

Run `LxRunOffline` for the command line interface.

# Build

### Visual Studio

Visual Studio 2017 and Windows SDK 10.0.17134 are required.

Install vcpkg and its VS integration (if you haven't) and install dependencies.

```
vcpkg install --triplet x64-windows libarchive boost-program-options
```

Then build with Visual Studio or MSBuild.

### MSYS2

Open the "MSYS2 MinGW 64-bit" shell, and install necessary packages.

```
pacman -Sy --needed --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-libarchive mingw-w64-x86_64-boost
```

Then run `make`.

# Compatibility

- **v1.x**: All Windows 10 versions with the Linux subsystem support.
- **v2.x**: Only Windows 10 Fall Creators Update (v1709) or later.
- **v3.x**: Only Windows 10 April 2018 Update (v1803) or later.

It is strongly recommended to install the April 2018 Update and use v3.x releases.
