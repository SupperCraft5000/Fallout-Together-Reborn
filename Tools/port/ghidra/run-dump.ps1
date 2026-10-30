param($Which)
$env:GHIDRA_HEADLESS_MAXMEM="20G"
$env:JAVA_HOME=$env:FTR_JAVA_HOME
$env:PATH="$env:JAVA_HOME\bin;$env:PATH"
$g="$env:FTR_GHIDRA\support\analyzeHeadless.bat"
$s=$PSScriptRoot
if ($Which -eq 'old') { $exe="$env:FTR_WORK\exe-1.10.163\Fallout4.analysis.exe" } else { $exe="$env:FTR_WORK\exe-1.11.240\Fallout4.exe" }
& $g "$env:FTR_WORK\ghidra\proj-$Which" fo4 -import $exe -noanalysis -scriptPath $s -postScript DumpAll.java "$env:FTR_WORK/ghidra/dump/$Which.txt" fn -scriptlog "$env:FTR_WORK\ghidra\$Which.script.log" -log "$env:FTR_WORK\ghidra\$Which.log" *> "$env:FTR_WORK\ghidra\$Which.console.log"
