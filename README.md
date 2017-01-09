# LxRunOffline
An offline installer for Bash on Windows.

# Features
- **No Internet access required when installing!**
- **Install any version of Ubuntu!**

# How to use
1. Download a Ubuntu pre-installed disk image.
  - Download an image from Ubuntu Cloud Image: https://cloud-images.ubuntu.com/releases/.<br/>
    Note: Download the files that are named as xx-server-cloudimg-amd64-root.tar.gz.
  - Download the Ubuntu 14.04 image from Microsoft: https://wsldownload.azureedge.net/trusty-server-cloudimg-amd64-root.tar.gz.
  - Download the Ubuntu 16.04 image from Microsoft: https://wsldownload.azureedge.net/16.4.1-server-cloudimg-amd64-root.tar.gz.
2. Download the icon file: https://go.microsoft.com/fwlink/?LinkID=747853.
3. Run `LxRunOffline /install` or use other arguments you need to be passed to `LxRun.exe`.
4. Enter the paths of the two files you downloaded when prompted. (**Note: You should use absolute paths, relative ones are not supported**)
5. Complete the installation.

# How it works
`LxRun.exe` uses `InternetOpenUrl()` in `wininet.dll` to download the files required. This application uses [EasyHook](https://easyhook.github.io) to inject code to its process and hooks this function and some others related. Then the download requests can be redirected to local files.
