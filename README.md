# LxRunOffline

[![Build status](https://ci.appveyor.com/api/projects/status/wnqrsjk1rdc1cmpn?svg=true)](https://ci.appveyor.com/project/ddosolitary/lxrunoffline)

An offline installer for *Windows Subsystem for Linux (WSL)*.

# Features

- No Internet access required when installing.
- Install any Linux distros that you like.
- Install the Linux filesystem to any directory.

# How to use

Binaries can be found at:
- Latest releases: https://github.com/DDoSolitary/LxRunOffline/releases
- Development builds: https://ci.appveyor.com/project/DDoSolitary/lxrunoffline/build/artifacts

See the [Wiki](https://github.com/DDoSolitary/LxRunOffline/wiki) for ways to create the ".tar.gz" files for different distros, which is required by the `LxRunOffline install` command.

---

```
LxRunOffline install <name> <path to .tar.gz> <target installation directory>
```
`name` is used to identify this installation when running other commands.
---
```
LxRunOffline uninstall <name>
```
---
```
LxRunOffline info <name>
```
---
```
LxRunOffline config <name> <UID of the default user> <WSL_DISTRIBUTION_FLAGS_ENABLE_INTEROP> <WSL_DISTRIBUTION_FLAGS_APPEND_NT_PATH> <WSL_DISTRIBUTION_FLAGS_ENABLE_DRIVE_MOUNTING>
```
The last three options are for the [WSL_DISTRIBUTION_FLAGS](https://msdn.microsoft.com/en-us/library/windows/desktop/mt826872(v=vs.85).aspx). They should be integers and zero(0) means enabled while non-zero means disabled.
---
```
LxRunOffline run <name> [command]
```
if `command` is not provided, `/bin/bash --login` will be used default. The exit code of the command will be used as the exit code of current process.
---
```
LxRunOffline list
```
List the names of all installations of current user.
