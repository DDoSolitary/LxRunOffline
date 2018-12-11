$version = [Environment]::OSVersion.Version
if ($version.Major -ne 10 -or $version.Build -lt 17134) {
	throw "This package requires Windows 10 v1803 or later."
}

if (-not [Environment]::Is64BitOperatingSystem) {
	throw "This package requires a 64-bit Windows."
}

Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux

$packageName = 'lxrunoffline'
$url = 'https://github.com/DDoSolitary/LxRunOffline/releases/download/v{VERSION}/LxRunOffline-v{VERSION}.zip'
$unzipLocation = Join-Path (Get-ToolsLocation) $packageName
if (Test-Path $unzipLocation) {
	rm -Recurse $unzipLocation
}
Install-ChocolateyZipPackage -PackageName $packageName -Url $url -UnzipLocation $unzipLocation -Checksum '{CHECKSUM}' -ChecksumType 'sha256'
Install-ChocolateyPath -PathToInstall $unzipLocation -PathType 'Machine'
