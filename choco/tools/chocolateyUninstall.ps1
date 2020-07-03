$unzipLocation = Join-Path $Env:ChocolateyToolsLocation 'lxrunoffline'
$shellExtPath = Join-Path $unzipLocation 'LxRunOfflineShellExt.dll'
if (Test-Path $unzipLocation) {
	if (Test-Path $shellExtPath) {
		regsvr32 /s /u $shellExtPath
	}
	rm -Recurse $unzipLocation
}
