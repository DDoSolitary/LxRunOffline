# LxRunOffline

[![Build status](https://ci.appveyor.com/api/projects/status/wnqrsjk1rdc1cmpn?svg=true)](https://ci.appveyor.com/project/sqc1999/lxrunoffline)

An offline installer for Bash on Windows.

# Features
- **No Internet access required when installing!**
- **Install any version of Ubuntu!**

# How to use
1. Prepare an Ubuntu pre-installed disk image.  
  You have several choices.
  - Download an image from Ubuntu Cloud Image: https://cloud-images.ubuntu.com/releases/.  
    Note: Download the files that are named as `xx-server-cloudimg-amd64-root.tar.gz`.
  - Download the Ubuntu 14.04 image from Microsoft: https://wsldownload.azureedge.net/trusty-server-cloudimg-amd64-root.tar.gz.
  - Download the Ubuntu 16.04 image from Microsoft: https://wsldownload.azureedge.net/16.4.1-server-cloudimg-amd64-root.tar.gz.
  - Use an image from somewhere else if you want to.
2. Download the icon file: https://wsldownload.azureedge.net/ubuntu.ico. (You can use another one too!)
3. Run `LxRunOffline /install`. Or use other arguments you need, just like you're using `LxRun.exe`.
4. Enter the paths of the two files you prepared when prompted. (**Note: You should use absolute paths, relative ones are not supported**)
5. Finish the installation.

# How it works
`LxRun.exe` uses `InternetOpenUrl()` in `wininet.dll` to download the files required. This application uses [EasyHook](https://easyhook.github.io) to inject code to its process , and hooks this function and some others related. Then the download requests can be redirected to local files.
