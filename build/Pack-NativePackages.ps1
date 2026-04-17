[CmdletBinding()]
param(
    [ValidateSet('MSFS2020', 'MSFS2024')]
    [string[]]$Simulator = @('MSFS2020', 'MSFS2024'),

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$PackageVersion = '0.2.0',

    [string]$PackageOutputPath = '',

    [string]$MSFS2020SdkRoot = 'H:\MSFS 2020 SDK',

    [string]$MSFS2024SdkRoot = 'H:\MSFS 2024 SDK',

    [switch]$Push,

    [string]$Source = 'https://api.nuget.org/v3/index.json',

    [string]$ApiKey = $env:NUGET_API_KEY
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ToolPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "Required tool '$Name' was not found on PATH."
    }

    return $command.Source
}

function Normalize-SdkRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        throw "Simulator SDK root '$Path' does not exist."
    }

    $resolved = (Resolve-Path $Path).Path
    if (-not $resolved.EndsWith('\')) {
        $resolved += '\'
    }

    return $resolved
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$interopProject = Join-Path $repoRoot 'CsSimConnectInterOp.vcxproj'
$packageProject = Join-Path $repoRoot 'nuget\CsSimConnect.Native.Package\CsSimConnect.Native.Package.csproj'

if ([string]::IsNullOrWhiteSpace($PackageOutputPath)) {
    $PackageOutputPath = Join-Path $repoRoot 'artifacts\packages'
}

$msbuild = Get-ToolPath -Name 'msbuild'
$dotnet = Get-ToolPath -Name 'dotnet'

$simulatorConfig = @{
    'MSFS2020' = @{
        PackageId = 'CsSimConnect.Native.MSFS2020'
        SdkRoot = $MSFS2020SdkRoot
        Description = 'Native CsSimConnect InterOp DLL built against the Microsoft Flight Simulator 2020 SimConnect SDK.'
        NativeAssetSubdir = 'MSFS2020'
    }
    'MSFS2024' = @{
        PackageId = 'CsSimConnect.Native.MSFS2024'
        SdkRoot = $MSFS2024SdkRoot
        Description = 'Native CsSimConnect InterOp DLL built against the Microsoft Flight Simulator 2024 SimConnect SDK.'
        NativeAssetSubdir = 'MSFS2024'
    }
}

$results = @()
$originalMsfsSdk = $env:MSFS_SDK

try {
    New-Item -ItemType Directory -Path $PackageOutputPath -Force | Out-Null

    foreach ($sim in $Simulator) {
        $config = $simulatorConfig[$sim]
        $sdkRoot = Normalize-SdkRoot -Path $config.SdkRoot
        $buildOutput = Join-Path $repoRoot ("artifacts\build\{0}\{1}" -f $sim, $Configuration)
        $intermediateOutput = Join-Path $repoRoot ("artifacts\obj\{0}\{1}\" -f $sim, $Configuration)

        New-Item -ItemType Directory -Path $buildOutput, $intermediateOutput -Force | Out-Null

        Write-Host "Building $sim against $sdkRoot" -ForegroundColor Cyan

        $env:MSFS_SDK = $sdkRoot

        $msbuildArgs = @(
            $interopProject
            '/t:Build'
            "/p:Configuration=$Configuration"
            '/p:Platform=x64'
            "/p:OutDir=$buildOutput\"
            "/p:IntDir=$intermediateOutput"
            '/nologo'
            '/verbosity:minimal'
        )

        & $msbuild @msbuildArgs
        if ($LASTEXITCODE -ne 0) {
            throw "MSBuild failed for $sim."
        }

        Write-Host "Packing $($config.PackageId) $PackageVersion" -ForegroundColor Cyan

        $packArgs = @(
            'pack'
            $packageProject
            '--configuration'
            'Release'
            '--output'
            $PackageOutputPath
            '--nologo'
            "-p:NativeBinaryDir=$buildOutput"
            "-p:PackageId=$($config.PackageId)"
            "-p:PackageVersion=$PackageVersion"
            "-p:PackageDescription=$($config.Description)"
            "-p:NativeAssetSubdir=$($config.NativeAssetSubdir)"
        )

        & $dotnet @packArgs
        if ($LASTEXITCODE -ne 0) {
            throw "dotnet pack failed for $sim."
        }

        $packagePath = Join-Path $PackageOutputPath ("{0}.{1}.nupkg" -f $config.PackageId, $PackageVersion)
        if (-not (Test-Path $packagePath)) {
            throw "Expected package '$packagePath' was not created."
        }

        if ($Push) {
            if ([string]::IsNullOrWhiteSpace($ApiKey)) {
                throw 'NuGet push requested, but no API key was provided through -ApiKey or NUGET_API_KEY.'
            }

            & $dotnet nuget push $packagePath --api-key $ApiKey --source $Source --skip-duplicate
            if ($LASTEXITCODE -ne 0) {
                throw "dotnet nuget push failed for $packagePath."
            }
        }

        $results += [pscustomobject]@{
            Simulator = $sim
            PackageId = $config.PackageId
            PackagePath = $packagePath
            BuildOutput = $buildOutput
            SdkRoot = $sdkRoot
        }
    }
}
finally {
    $env:MSFS_SDK = $originalMsfsSdk
}

$results | Format-Table -AutoSize
