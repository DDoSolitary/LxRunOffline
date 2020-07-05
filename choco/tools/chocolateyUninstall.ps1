$pkgDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$shellExtPath = Join-Path $pkgDir 'LxRunOfflineShellExt.dll'
regsvr32 /s /u $shellExtPath
