$version = [Environment]::OSVersion.Version
if ($version.Major -ne 10 -or $version.Build -lt 17134) {
	throw "This package requires Windows 10 v1803 or later."
}

if (-not [Environment]::Is64BitOperatingSystem) {
	throw "This package requires a 64-bit Windows."
}

Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux

$packageName = 'lxrunoffline'
$url = 'https://github.com/DDoSolitary/LxRunOffline/releases/download/v3.4.1/LxRunOffline-v3.4.1-msvc.zip'
$unzipLocation = Join-Path (Get-ToolsLocation) $packageName
$shellExtPath = Join-Path $unzipLocation 'LxRunOfflineShellExt.dll'
if (Test-Path $unzipLocation) {
	if (Test-Path $shellExtPath) {
		regsvr32 /s /u $shellExtPath
	}
	rm -Recurse $unzipLocation
}
Install-ChocolateyZipPackage -PackageName $packageName -Url $url -UnzipLocation $unzipLocation -Checksum '56015afe2cb1ed0e5daa935fd1142c44f8837b4b0e0c7f1ff9f5aa184767b480' -ChecksumType 'sha256'
Install-ChocolateyPath -PathToInstall $unzipLocation -PathType 'Machine'

regsvr32 /s $shellExtPath
