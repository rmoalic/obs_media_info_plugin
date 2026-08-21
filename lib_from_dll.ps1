function New-LibFromDll([string]$modulePath, [string]$machine) {
    if(!(Test-Path $modulePath -PathType Leaf)) {
        Write-Warning "$modulePath is an invalid module path"
        return
    }

    $exports = dumpbin /nologo /exports $modulePath

    $def = @("EXPORTS")
    $numImports = 0
    $inTable = $false

    foreach ($line in $exports) {
        if ($line -match "ordinal\s+hint") {
            $inTable = $true
            continue
        }
        if (-not $inTable) { continue }
        if ($line -match "^\s*Summary") { break }
        if ([string]::IsNullOrWhiteSpace($line)) { continue }

        if ($line -match "^\s*\d+\s+[0-9A-Fa-f]*\s+[0-9A-Fa-f]+\s+([^\s(]+)") {
            $name = $matches[1]
            $def += $name
            $numImports++
        }
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