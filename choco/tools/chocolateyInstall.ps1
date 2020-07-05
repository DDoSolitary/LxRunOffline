$version = [Environment]::OSVersion.Version
if ($version.Major -ne 10 -or $version.Build -lt 17134) {
	throw "This package requires Windows 10 v1803 or later."
}

if (-not [Environment]::Is64BitOperatingSystem) {
	throw "This package requires a 64-bit Windows."
}

$unzipLocation = Join-Path (Get-ToolsLocation) $packageName
$legacyShellExtPath = Join-Path $unzipLocation 'LxRunOfflineShellExt.dll'
if (Test-Path $unzipLocation) {
	if (Test-Path $legacyShellExtPath) {
		regsvr32 /s /u $shellExtPath
	}
	rm -Recurse $unzipLocation
}

$pkgDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$shellExtPath = Join-Path $pkgDir 'LxRunOfflineShellExt.dll'
regsvr32 /s $shellExtPath
