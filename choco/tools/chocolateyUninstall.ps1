$unzipLocation = Join-Path $Env:ChocolateyToolsLocation 'lxrunoffline'
if (Test-Path $unzipLocation) {
	rm -Recurse $unzipLocation
}
