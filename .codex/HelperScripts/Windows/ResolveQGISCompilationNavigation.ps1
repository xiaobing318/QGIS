[CmdletBinding()]
param()

function Test-CodexQGISCompilationNavigation {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string[]]$RequiredChildren
    )

    $resolvedPath = $Path
    if (Test-Path -LiteralPath $Path -PathType Container) {
        $resolvedPath = (Resolve-Path -LiteralPath $Path).Path
    }

    $rootExists = Test-Path -LiteralPath $resolvedPath -PathType Container
    $children = foreach ($child in $RequiredChildren) {
        $childPath = Join-Path $resolvedPath $child
        @{
            Name = $child
            Path = $childPath
            TestPath = Test-Path -LiteralPath $childPath -PathType Container
        }
    }

    $allChildrenExist = $true
    foreach ($childResult in $children) {
        if (-not $childResult.TestPath) {
            $allChildrenExist = $false
            break
        }
    }

    @{
        Path = $resolvedPath
        Source = $Source
        TestPath = ($rootExists -and $allChildrenExist)
        RootExists = $rootExists
        RequiredChildren = @($children)
    }
}

function Get-CodexDefaultWindowsSearchRoots {
    [CmdletBinding()]
    param()

    Get-PSDrive -PSProvider FileSystem |
        Where-Object { $_.Root -and (Test-Path -LiteralPath $_.Root -PathType Container) } |
        ForEach-Object { $_.Root }
}

function Resolve-CodexQGISCompilationNavigation {
    [CmdletBinding()]
    param(
        [string]$ExplicitPath,
        [string[]]$SearchRoots,
        [string]$DefaultSearchMode = 'AllFixedDrives',
        [string[]]$RequiredChildren,
        [string[]]$PruneDirectoryNames,
        [int]$MaxCandidates = 20
    )

    $messages = New-Object System.Collections.Generic.List[object]
    $candidates = New-Object System.Collections.Generic.List[object]
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' -ArgumentList ([System.StringComparer]::OrdinalIgnoreCase)

    function New-NavigationResult {
        param(
            [Parameter(Mandatory = $true)][string]$Status,
            [object]$SelectedCandidate
        )

        $selectedPath = ''
        $selectedSource = ''
        $selectedRequiredChildren = @()
        $selectedTestPath = $false
        if ($SelectedCandidate) {
            $selectedPath = $SelectedCandidate['Path']
            $selectedSource = $SelectedCandidate['Source']
            $selectedRequiredChildren = @($SelectedCandidate['RequiredChildren'])
            $selectedTestPath = [bool]$SelectedCandidate['TestPath']
        }

        @{
            Status = $Status
            Path = $selectedPath
            Source = $selectedSource
            TestPath = $selectedTestPath
            RequiredChildren = $selectedRequiredChildren
            Candidates = $candidates.ToArray()
            Messages = $messages.ToArray()
        }
    }

    function Add-Candidate {
        param(
            [Parameter(Mandatory = $true)][string]$CandidatePath,
            [Parameter(Mandatory = $true)][string]$Source
        )

        if (-not $CandidatePath) {
            return $null
        }

        $candidate = Test-CodexQGISCompilationNavigation -Path $CandidatePath -Source $Source -RequiredChildren $RequiredChildren
        if (-not $seen.Contains($candidate.Path)) {
            [void]$seen.Add($candidate.Path)
            $candidates.Add($candidate)
        }
        return $candidate
    }

    if ($ExplicitPath) {
        $explicitCandidate = Add-Candidate -CandidatePath $ExplicitPath -Source 'Parameter'
        if ($explicitCandidate.TestPath) {
            return New-NavigationResult -Status 'Ok' -SelectedCandidate $explicitCandidate
        }

        $messages.Add([ordered]@{
            Level = 'Error'
            Code = 'ExplicitNavigationInvalid'
            Text = "Explicit QGISCompilationNavigation path is missing or incomplete: $ExplicitPath"
        })
        return New-NavigationResult -Status 'NotFound'
    }

    if ($env:QGIS_COMPILATION_NAVIGATION) {
        $environmentCandidate = Add-Candidate -CandidatePath $env:QGIS_COMPILATION_NAVIGATION -Source 'Environment:QGIS_COMPILATION_NAVIGATION'
        if ($environmentCandidate.TestPath) {
            return New-NavigationResult -Status 'Ok' -SelectedCandidate $environmentCandidate
        }

        $messages.Add([ordered]@{
            Level = 'Warning'
            Code = 'EnvironmentNavigationInvalid'
            Text = "QGIS_COMPILATION_NAVIGATION is missing or incomplete: $($env:QGIS_COMPILATION_NAVIGATION)"
        })
    }

    if (-not $SearchRoots -or $SearchRoots.Count -eq 0) {
        switch ($DefaultSearchMode) {
            'None' {
                $SearchRoots = @()
            }
            'AllFixedDrives' {
                $SearchRoots = @(Get-CodexDefaultWindowsSearchRoots)
            }
            default {
                $messages.Add([ordered]@{
                    Level = 'Warning'
                    Code = 'UnknownDefaultSearchMode'
                    Text = "Unknown DefaultSearchMode '$DefaultSearchMode'; falling back to AllFixedDrives."
                })
                $SearchRoots = @(Get-CodexDefaultWindowsSearchRoots)
            }
        }
    }

    $queue = New-Object 'System.Collections.Generic.Queue[string]'
    foreach ($root in $SearchRoots) {
        if (Test-Path -LiteralPath $root -PathType Container) {
            $queue.Enqueue((Resolve-Path -LiteralPath $root).Path)
        }
        else {
            $messages.Add([ordered]@{
                Level = 'Warning'
                Code = 'SearchRootMissing'
                Text = "Search root is missing: $root"
            })
        }
    }

    while ($queue.Count -gt 0 -and $candidates.Count -lt $MaxCandidates) {
        $current = $queue.Dequeue()

        try {
            $item = Get-Item -LiteralPath $current -Force -ErrorAction Stop
        }
        catch {
            $messages.Add([ordered]@{
                Level = 'Warning'
                Code = 'SearchPathUnreadable'
                Text = "Directory is unreadable: $current"
            })
            continue
        }

        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            continue
        }

        if ($item.Name -eq 'QGISCompilationNavigation') {
            [void](Add-Candidate -CandidatePath $item.FullName -Source 'FilesystemScan')
            continue
        }

        if ($PruneDirectoryNames -contains $item.Name) {
            continue
        }

        try {
            $children = Get-ChildItem -LiteralPath $item.FullName -Directory -Force -ErrorAction Stop
        }
        catch {
            continue
        }

        foreach ($child in $children) {
            if (($child.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                continue
            }
            if ($PruneDirectoryNames -contains $child.Name) {
                continue
            }
            $queue.Enqueue($child.FullName)
        }
    }

    if ($candidates.Count -ge $MaxCandidates) {
        $messages.Add([ordered]@{
            Level = 'Warning'
            Code = 'CandidateLimitReached'
            Text = "Candidate limit $MaxCandidates reached; remaining directories were not scanned."
        })
    }

    $validCandidates = New-Object System.Collections.Generic.List[object]
    foreach ($candidate in $candidates) {
        if ($candidate.TestPath) {
            $validCandidates.Add($candidate)
        }
    }
    $status = if ($validCandidates.Count -eq 1) {
        'Ok'
    }
    elseif ($validCandidates.Count -gt 1) {
        'Ambiguous'
    }
    else {
        'NotFound'
    }

    $selected = if ($validCandidates.Count -eq 1) { $validCandidates[0] } else { $null }

    $selectedPath = ''
    $selectedSource = ''
    $selectedRequiredChildren = @()
    if ($selected) {
        $selectedPath = $selected['Path']
        $selectedSource = $selected['Source']
        $selectedRequiredChildren = @($selected['RequiredChildren'])
    }
    $selectedTestPath = $false
    if ($null -ne $selected) {
        $selectedTestPath = $true
    }
    $candidateArray = $candidates.ToArray()
    $messageArray = $messages.ToArray()

    @{
        Status = $status
        Path = $selectedPath
        Source = $selectedSource
        TestPath = $selectedTestPath
        RequiredChildren = $selectedRequiredChildren
        Candidates = $candidateArray
        Messages = $messageArray
    }
}
