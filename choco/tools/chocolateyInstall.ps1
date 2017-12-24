$osVersion = [Environment]::OSVersion.Version.Major
$osRelId = (gp 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion').ReleaseId
if ($osVersion -ne 10 -or $osRelId -lt 1703) {
	throw "This package requires Windows 10 Fall Creators Update or later."
}

$packageName = 'lxrunoffline'
$url = 'https://github.com/DDoSolitary/LxRunOffline/releases/download/v{VERSION}/LxRunOffline-v{VERSION}.zip'
$unzipLocation = Join-Path $Env:ChocolateyToolsLocation $packageName
if (Test-Path $unzipLocation) {
	rm -Recurse $unzipLocation
}
Install-ChocolateyZipPackage -PackageName $packageName -Url $url -UnzipLocation $unzipLocation -Checksum '{CHECKSUM}' -ChecksumType 'sha256'
Install-ChocolateyPath -PathToInstall $unzipLocation -PathType 'Machine'
