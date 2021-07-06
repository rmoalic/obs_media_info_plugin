# https://emsea.github.io/2018/07/13/lib-from-dll/

function New-LibFromDll([string]$modulePath, [string]$machine) {
    if(!(Test-Path $modulePath -PathType Leaf)) {
        Write-Warning "$modulePath is an invalid module path"
        return
    }

    $exports = (dumpbin /nologo /exports $modulePath) | Out-String
    $tabStart = $exports.IndexOf("ordinal hint")
    $exportsTab = $exports.Substring($tabStart) -split "\s+"

    $numImports = 0
    $def = @("EXPORTS")
    for($i = 4; $exportsTab[$i] -ne "Summary"; $i += 4) {
        $def += $exportsTab[$i + 3]
        $numImports++
    }

    $moduleFileInfo = New-Object System.IO.FileInfo($modulePath)

    $defFileName = $moduleFileInfo.BaseName + ".def"
    ($def -join "`r`n") | Out-File $defFileName

    $libMachine = if($machine) {" /machine:$machine"} else {""}
    $libFileName = $moduleFileInfo.BaseName + ".lib"
    iex ("lib /nologo /def:$defFileName /out:$libFileName" + $libMachine)

    if(Test-Path $libFileName -PathType Leaf) {
        Write-Host "`r`nCreated $libFileName importing $numImports exports`r`n"
        Write-Host "#pragma comment(lib, `"$libFileName`")"
    } else {
        Write-Warning "$libFileName was unable to be created"
    }
}
