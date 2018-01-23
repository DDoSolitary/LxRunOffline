$osVersion = [Environment]::OSVersion.Version.Major
$osRelId = (gp 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion').ReleaseId
if ($osVersion -ne 10 -or $osRelId -lt 1703) {
	throw "This package requires Windows 10 Fall Creators Update or later."
}

if ($Env:PROCESSOR_ARCHITECTURE -ne 'AMD64') {
	throw "This package requires a 64-bit Windows."
}

New-ItemProperty -Force -Path 'HKLM:\System\CurrentControlSet\Control\Session Manager\Kernel' -Name 'obcaseinsensitive' -Value 0 -PropertyType Dword
echo 'Please restart your system to let registry changes take effect.'

$packageName = 'lxrunoffline'
$url = 'https://github.com/DDoSolitary/LxRunOffline/releases/download/v{VERSION}/LxRunOffline-v{VERSION}.zip'
$unzipLocation = Join-Path $Env:ChocolateyToolsLocation $packageName
if (Test-Path $unzipLocation) {
	rm -Recurse $unzipLocation
}
Install-ChocolateyZipPackage -PackageName $packageName -Url $url -UnzipLocation $unzipLocation -Checksum '{CHECKSUM}' -ChecksumType 'sha256'
Install-ChocolateyPath -PathToInstall $unzipLocation -PathType 'Machine'
