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

Run `LxRunOffline help` for the command line interface.

Set the environment variable `LXRUNOFFLINE_PROMPT_ANSWER` to `y` or `n` to answer confirmations automatically. Set `LXRUNOFFLINE_VERBOSE` to `1` to enable verbose output.

# Compatibility

- **v1.x**: All Windows 10 versions with the Linux subsystem support.
- **v2.x**: Only Windows 10 Fall Creators Update or later.

It is strongly recommended to install the Creators Update and use v2.x releases.
