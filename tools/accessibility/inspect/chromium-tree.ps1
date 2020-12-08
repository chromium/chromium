# Powershell script to dump accessibility tree for Chromium. Takes optional first argument with part of window title to disambiguate the desired window.
$all = ps | where {$_.ProcessName -eq 'chrome'} |where MainWindowTitle -like "*$($args[0])*Chromium" | select MainWindowHandle, MainWindowTitle
echo $all
echo ""
If (@($all).length -gt 1) {
  echo "Multiple matching windows, please disambuate: include part of the desired window's title as a first argument."
  exit
}

$hwnd = Get-Process | where {$_.ProcessName -eq 'chrome'} | where MainWindowTitle -like "*$($args[0])*Chromium*" | select MainWindowHandle -ExpandProperty MainWindowHandle | Out-String
$hwnd_arg = "--pid=" + $hwnd
$exe = ".\ax_dump_tree.exe"
& $exe $hwnd_arg
