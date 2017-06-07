# LxRunOffline

[![Build status](https://ci.appveyor.com/api/projects/status/wnqrsjk1rdc1cmpn?svg=true)](https://ci.appveyor.com/project/ddosolitary/lxrunoffline)

An offline installer for Bash on Windows.

# Features
- **No Internet access required when installing!**
- **Install any version of Ubuntu!**

# How to use
1. Enable WSL.  
  Run `OptionalFeatures.exe`, check `Windows Subsystem for Linux (Beta)`, click `OK`.
2. Download the binaries. ([stable](https://github.com/DDoSolitary/LxRunOffline/releases)/[latest](https://ci.appveyor.com/project/ddosolitary/lxrunoffline/build/artifacts))  
  **Important: You have to [unblock](http://stackoverflow.com/a/8268636/323899) the zip file before you extract it because of a security mechanism of Windows!**
3. [Prepare an Ubuntu pre-installed disk image.](https://github.com/DDoSolitary/LxRunOffline/wiki/Guide-for-fetching-Ubuntu-pre-installed-images)
4. Download the icon file: https://wsldownload.azureedge.net/ubuntu.ico.
5. Run `LxRunOffline /install`. Or use other arguments you need, just like you're using `LxRun.exe`.
6. Enter the paths of the two files you prepared when prompted.
7. Finish the installation.

# How it works
`LxRun.exe` uses `InternetOpenUrl()` in `wininet.dll` to download the files required. This application uses [EasyHook](https://easyhook.github.io) to inject code to its process , and hooks this function and some others related. Then the download requests can be redirected to local files.
