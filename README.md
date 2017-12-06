# LxRunOffline

[![Build status](https://ci.appveyor.com/api/projects/status/wnqrsjk1rdc1cmpn?svg=true)](https://ci.appveyor.com/project/ddosolitary/lxrunoffline)

An offline installer for *Windows Subsystem for Linux (WSL)*.

# Features

- No Internet access required when installing.
- Install any Linux distros that you like.
- Install the Linux filesystem to any directory.

# Install

You can install via Chocolatey `choco install lxrunoffline`, or download the binaries directly:
- Latest releases: https://github.com/DDoSolitary/LxRunOffline/releases
- Development builds: https://ci.appveyor.com/project/DDoSolitary/lxrunoffline/build/artifacts

# Usage

See the [Wiki](https://github.com/DDoSolitary/LxRunOffline/wiki) for ways to create the ".tar.gz" files for different distros, which is required by the `LxRunOffline install` command.

Run `LxRunOffline help` for the command line interface.

# Compatibility

- *v1.x*: All Windows 10 versions with the Linux subsystem support.
- *v2.x*: Only Windows 10 Fall Creators Update or later.

It is strongly recommended to install the Creators Update and use v2.x releases.
