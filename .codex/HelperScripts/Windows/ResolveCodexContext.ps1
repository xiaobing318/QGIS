Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$defaultConfigPath = Join-Path $scriptRoot 'ResolveCodexContext.json'
$platformInfoScriptName = 'ResolvePlatformInfo.ps1'
$qgisCompilationNavigationScriptName = 'ResolveQGISCompilationNavigation.ps1'
$defaultEnabledScripts = @(
    $platformInfoScriptName,
    $qgisCompilationNavigationScriptName
)
$allowedEnabledScripts = New-Object 'System.Collections.Generic.HashSet[string]' -ArgumentList ([System.StringComparer]::Ordinal)
[void]$allowedEnabledScripts.Add($platformInfoScriptName)
[void]$allowedEnabledScripts.Add($qgisCompilationNavigationScriptName)

. (Join-Path $scriptRoot $platformInfoScriptName)
. (Join-Path $scriptRoot $qgisCompilationNavigationScriptName)

function Show-CodexContextHelp {
    @'
ResolveCodexContext.ps1

Usage:
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .codex\HelperScripts\Windows\ResolveCodexContext.ps1 [--help]
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .codex\HelperScripts\Windows\ResolveCodexContext.ps1 [--config <path>] [--outputJsonPath <path>]

Parameters:
  --help                  Show this help message and exit.
  --config <path>         Read the specified JSON config file. Defaults to ResolveCodexContext.json next to this script.
  --outputJsonPath <path> Write the JSON result to the specified path. Existing files are overwritten. Defaults to terminal output only.

Config:
  EnabledScripts controls which discovery scripts run. Allowed values:
    ResolvePlatformInfo.ps1
    ResolveQGISCompilationNavigation.ps1

'@ | Write-Output
}

function Resolve-CodexUserPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $PathValue))
}

function Write-CodexJson {
    param(
        [Parameter(Mandatory = $true)]
        [System.Collections.IDictionary]$Context,

        [string]$OutputJsonPath = ''
    )

    $json = $Context | ConvertTo-Json -Depth 30
    if (-not [string]::IsNullOrWhiteSpace($OutputJsonPath)) {
        $resolvedOutputPath = Resolve-CodexUserPath -PathValue $OutputJsonPath
        $outputDirectory = Split-Path -Parent $resolvedOutputPath
        if (-not [string]::IsNullOrWhiteSpace($outputDirectory) -and
            -not (Test-Path -LiteralPath $outputDirectory -PathType Container)) {
            New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
        }

        $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
        [System.IO.File]::WriteAllText($resolvedOutputPath, $json, $utf8NoBom)
    }

    $json
}

function New-CodexInvalidInputContext {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Code,

        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    [ordered]@{
        Status = 'InvalidInput'
        Messages = @(
            [ordered]@{
                Level = 'Error'
                Code = $Code
                Text = $Message
            }
        )
    }
}

function Write-CodexInvalidInputAndExit {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Code,

        [Parameter(Mandatory = $true)]
        [string]$Message,

        [string]$OutputJsonPath = ''
    )

    $context = New-CodexInvalidInputContext -Code $Code -Message $Message
    Write-CodexJson -Context $context -OutputJsonPath $OutputJsonPath
    exit 2
}

function Assert-CodexPathArgument {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        throw "$Name requires a non-empty value."
    }

    if ($Value -like '--*') {
        throw "$Name requires a value, but received another parameter name."
    }

    $invalidChars = [System.IO.Path]::GetInvalidPathChars()
    if ($Value.IndexOfAny($invalidChars) -ge 0) {
        throw "$Name contains invalid path characters."
    }
}

function Resolve-CodexArguments {
    param(
        [string[]]$ArgumentList
    )

    $resolved = [ordered]@{
        Help = $false
        Config = $defaultConfigPath
        OutputJsonPath = ''
    }

    $seenConfig = $false
    $seenOutputJsonPath = $false
    for ($index = 0; $index -lt $ArgumentList.Count; $index++) {
        $argument = $ArgumentList[$index]
        switch ($argument) {
            '--help' {
                $resolved.Help = $true
            }
            '--config' {
                if ($seenConfig) {
                    throw 'Duplicate --config parameter.'
                }
                if (($index + 1) -ge $ArgumentList.Count) {
                    throw '--config requires a value.'
                }
                $value = $ArgumentList[$index + 1]
                Assert-CodexPathArgument -Name '--config' -Value $value
                $resolved.Config = $value
                $seenConfig = $true
                $index++
            }
            '--outputJsonPath' {
                if ($seenOutputJsonPath) {
                    throw 'Duplicate --outputJsonPath parameter.'
                }
                if (($index + 1) -ge $ArgumentList.Count) {
                    throw '--outputJsonPath requires a value.'
                }
                $value = $ArgumentList[$index + 1]
                Assert-CodexPathArgument -Name '--outputJsonPath' -Value $value
                $resolved.OutputJsonPath = $value
                $seenOutputJsonPath = $true
                $index++
            }
            default {
                throw "Unsupported parameter or positional argument: $argument"
            }
        }
    }

    if ($resolved.Help -and $ArgumentList.Count -gt 1) {
        throw '--help cannot be combined with other parameters.'
    }

    return $resolved
}

function Get-CodexComponentProperty {
    param(
        [object]$Component,
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    if ($null -eq $Component) {
        return $null
    }

    if ($Component -is [System.Collections.IDictionary]) {
        if ($Component.Contains($Name)) {
            return $Component[$Name]
        }
        return $null
    }

    $property = $Component.PSObject.Properties[$Name]
    if ($null -ne $property) {
        return $property.Value
    }

    return $null
}

function Get-CodexObjectProperty {
    param(
        [object]$Object,
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [object]$DefaultValue = $null
    )

    if ($null -eq $Object) {
        return $DefaultValue
    }

    if ($Object -is [System.Collections.IDictionary]) {
        if ($Object.Contains($Name)) {
            return $Object[$Name]
        }
        return $DefaultValue
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -ne $property) {
        return $property.Value
    }

    return $DefaultValue
}

function Get-CodexStringArrayProperty {
    param(
        [object]$Object,
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [string[]]$DefaultValue = @()
    )

    $value = Get-CodexObjectProperty -Object $Object -Name $Name
    if ($null -eq $value) {
        return @($DefaultValue)
    }

    if ($value -is [string]) {
        return @($value)
    }

    $items = @()
    foreach ($item in @($value)) {
        $text = [string]$item
        if ([string]::IsNullOrWhiteSpace($text)) {
            throw "$Name contains an empty value."
        }
        $items += $text
    }

    return $items
}

function Get-CodexEnabledScripts {
    param(
        [object]$ConfigObject,
        [Parameter(Mandatory = $true)]
        [string]$ConfigText
    )

    $enabledScriptsProperty = $ConfigObject.PSObject.Properties['EnabledScripts']
    if ($enabledScriptsProperty -and
        ($ConfigText -notmatch '(?s)"EnabledScripts"\s*:\s*\[')) {
        throw 'EnabledScripts must be an array of strings.'
    }

    if ($enabledScriptsProperty) {
        $scripts = @()
        $rawScripts = $enabledScriptsProperty.Value
        if ($null -ne $rawScripts) {
            if ($rawScripts -is [string]) {
                $scripts = @($rawScripts)
            }
            else {
                foreach ($item in @($rawScripts)) {
                    $text = [string]$item
                    if ([string]::IsNullOrWhiteSpace($text)) {
                        throw 'EnabledScripts contains an empty value.'
                    }
                    $scripts += $text
                }
            }
        }
    }
    else {
        $scripts = @($defaultEnabledScripts)
    }

    $seen = New-Object 'System.Collections.Generic.HashSet[string]' -ArgumentList ([System.StringComparer]::Ordinal)
    foreach ($scriptName in $scripts) {
        if (-not $allowedEnabledScripts.Contains($scriptName)) {
            throw "Unsupported EnabledScripts value: $scriptName"
        }
        if (-not $seen.Add($scriptName)) {
            throw "Duplicate EnabledScripts value: $scriptName"
        }
    }

    return @($scripts)
}

function Test-CodexScriptEnabled {
    param(
        [string[]]$EnabledScripts,
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    return $EnabledScripts -contains $Name
}

function Test-CodexSupportedPlatform {
    param(
        [Parameter(Mandatory = $true)]
        [object]$PlatformInfo
    )

    $platformKey = '{0}/{1}' -f $PlatformInfo.OS, $PlatformInfo.ISA
    return $platformKey -in @(
        'Windows/AMD64',
        'Windows/ARM64',
        'Linux/AMD64',
        'Linux/ARM64',
        'macOS/ARM64'
    )
}

function Assert-CodexOutputJsonPath {
    param(
        [string]$OutputJsonPath
    )

    if ([string]::IsNullOrWhiteSpace($OutputJsonPath)) {
        return
    }

    try {
        $resolvedOutputJsonPath = Resolve-CodexUserPath -PathValue $OutputJsonPath
        if (Test-Path -LiteralPath $resolvedOutputJsonPath -PathType Container) {
            throw "--outputJsonPath must be a file path, not a directory: $resolvedOutputJsonPath"
        }
        $outputDirectory = Split-Path -Parent $resolvedOutputJsonPath
        if (-not [string]::IsNullOrWhiteSpace($outputDirectory) -and
            (Test-Path -LiteralPath $outputDirectory) -and
            -not (Test-Path -LiteralPath $outputDirectory -PathType Container)) {
            throw "--outputJsonPath parent exists but is not a directory: $outputDirectory"
        }
    }
    catch {
        throw $_.Exception.Message
    }
}

function Get-CodexNavigationConfig {
    param(
        [object]$ConfigObject
    )

    $navigationConfig = Get-CodexObjectProperty -Object $ConfigObject -Name 'QGISCompilationNavigation'

    $searchRoots = @()
    $configuredSearchRoots = Get-CodexObjectProperty -Object $navigationConfig -Name 'SearchRoots'
    if ($configuredSearchRoots) {
        $searchRoots = @($configuredSearchRoots)
    }

    $requiredChildren = @(
        'CommonMaterials',
        'QGIS-Linux-AMD64',
        'QGIS-Linux-ARM64',
        'QGIS-macOS-ARM64',
        'QGIS-Windows-AMD64',
        'QGIS-Windows-ARM64'
    )
    $configuredRequiredChildren = Get-CodexObjectProperty -Object $navigationConfig -Name 'RequiredChildren'
    if ($configuredRequiredChildren) {
        $requiredChildren = @($configuredRequiredChildren)
    }

    $pruneDirectoryNames = @(
        '$Recycle.Bin',
        'System Volume Information',
        'Recovery',
        'Config.Msi',
        '.git',
        'node_modules'
    )
    $configuredPruneDirectoryNames = Get-CodexObjectProperty -Object $navigationConfig -Name 'PruneDirectoryNames'
    if ($configuredPruneDirectoryNames) {
        $pruneDirectoryNames = @($configuredPruneDirectoryNames)
    }

    [ordered]@{
        SearchRoots = $searchRoots
        RequiredChildren = $requiredChildren
        PruneDirectoryNames = $pruneDirectoryNames
        DefaultSearchMode = [string](Get-CodexObjectProperty -Object $navigationConfig -Name 'DefaultSearchMode' -DefaultValue 'AllFixedDrives')
        MaxCandidates = [int](Get-CodexObjectProperty -Object $navigationConfig -Name 'MaxCandidates' -DefaultValue 20)
    }
}

try {
    $arguments = Resolve-CodexArguments -ArgumentList $args
}
catch {
    Write-CodexInvalidInputAndExit -Code 'ParameterValidationFailed' -Message $_.Exception.Message
}

if ($arguments.Help) {
    Show-CodexContextHelp
    exit 0
}

$resolvedConfigPath = Resolve-CodexUserPath -PathValue $arguments.Config
if (-not (Test-Path -LiteralPath $resolvedConfigPath -PathType Leaf)) {
    Write-CodexInvalidInputAndExit `
        -Code 'ConfigNotFound' `
        -Message "Config file not found: $resolvedConfigPath" `
        -OutputJsonPath $arguments.OutputJsonPath
}

try {
    Assert-CodexOutputJsonPath -OutputJsonPath $arguments.OutputJsonPath
}
catch {
    Write-CodexInvalidInputAndExit -Code 'OutputPathInvalid' -Message $_.Exception.Message
}

try {
    $configText = Get-Content -LiteralPath $resolvedConfigPath -Raw -Encoding UTF8
    $configObject = $configText | ConvertFrom-Json
}
catch {
    Write-CodexInvalidInputAndExit `
        -Code 'ConfigInvalidJson' `
        -Message "Config file is not valid JSON: $resolvedConfigPath. $($_.Exception.Message)" `
        -OutputJsonPath $arguments.OutputJsonPath
}

try {
    $enabledScripts = @(Get-CodexEnabledScripts -ConfigObject $configObject -ConfigText $configText)
}
catch {
    Write-CodexInvalidInputAndExit `
        -Code 'EnabledScriptsInvalid' `
        -Message $_.Exception.Message `
        -OutputJsonPath $arguments.OutputJsonPath
}

$messages = New-Object System.Collections.Generic.List[object]
$context = [ordered]@{
    Status = 'Ok'
}

$platformInfo = $null
$isPlatformInfoEnabled = Test-CodexScriptEnabled -EnabledScripts $enabledScripts -Name $platformInfoScriptName
$isNavigationEnabled = Test-CodexScriptEnabled -EnabledScripts $enabledScripts -Name $qgisCompilationNavigationScriptName

if ($isPlatformInfoEnabled) {
    $platformInfo = Resolve-CodexPlatformInfo
    $context['PlatformInfo'] = $platformInfo
    if (-not (Test-CodexSupportedPlatform -PlatformInfo $platformInfo)) {
        $context['Status'] = 'Unsupported'
    }
}

if ($isNavigationEnabled) {
    $navigationConfig = Get-CodexNavigationConfig -ConfigObject $configObject
    $navigation = Resolve-CodexQGISCompilationNavigation `
        -ExplicitPath '' `
        -SearchRoots $navigationConfig.SearchRoots `
        -DefaultSearchMode $navigationConfig.DefaultSearchMode `
        -RequiredChildren $navigationConfig.RequiredChildren `
        -PruneDirectoryNames $navigationConfig.PruneDirectoryNames `
        -MaxCandidates $navigationConfig.MaxCandidates

    $navigationStatus = Get-CodexComponentProperty -Component $navigation -Name 'Status'
    if ($context['Status'] -eq 'Ok') {
        $context['Status'] = if ($navigationStatus) { $navigationStatus } else { 'NotFound' }
    }

    $placeholder = [ordered]@{
        Path = [string](Get-CodexComponentProperty -Component $navigation -Name 'Path')
        TestPath = [bool](Get-CodexComponentProperty -Component $navigation -Name 'TestPath')
        Source = [string](Get-CodexComponentProperty -Component $navigation -Name 'Source')
    }
    $context['Placeholders'] = [ordered]@{
        '__QGISCompilationNavigation__' = $placeholder
    }

    if ($context['Status'] -ne 'Ok') {
        $navigationCandidates = Get-CodexComponentProperty -Component $navigation -Name 'Candidates'
        if ($navigationCandidates) {
            $context['Candidates'] = @($navigationCandidates)
        }

        $navigationMessages = Get-CodexComponentProperty -Component $navigation -Name 'Messages'
        if ($navigationMessages) {
            foreach ($message in @($navigationMessages)) {
                $messages.Add($message)
            }
        }
    }
}

if ($enabledScripts.Count -eq 0) {
    $messages.Add([ordered]@{
        Level = 'Info'
        Code = 'NoDiscoveryScriptsEnabled'
        Text = 'EnabledScripts is empty; no discovery scripts were executed.'
    })
}

if ($messages.Count -gt 0) {
    $context['Messages'] = $messages.ToArray()
}

Write-CodexJson -Context $context -OutputJsonPath $arguments.OutputJsonPath
