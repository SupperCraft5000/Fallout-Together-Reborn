param($Which, $In, $Out)
$env:GHIDRA_HEADLESS_MAXMEM="20G"
$env:JAVA_HOME=$env:FTR_JAVA_HOME
$env:PATH="$env:JAVA_HOME\bin;$env:PATH"
$g="$env:FTR_GHIDRA\support\analyzeHeadless.bat"
$s=$PSScriptRoot
$n=if ($Which -eq 'old') {'Fallout4.analysis.exe'} else {'Fallout4.exe'}
& $g "$env:FTR_WORK\ghidra\proj-$Which" fo4 -process $n -noanalysis -readOnly -scriptPath $s -postScript Decomp.java $In $Out *> "$env:FTR_WORK\ghidra\decomp-$Which.console.log"
