function Invoke-Environment { # https://github.com/majkinetor/posh/blob/master/MM_Admin/Invoke-Environment.ps1
    param (
        # Any cmd shell command, normally a configuration batch file.
        [Parameter(Mandatory = $true)]
        [string] $Command
    )
    $Command = "`"" + $Command + "`""
    cmd /c "$Command >nul 2>&1 && set" | . { process {
        if ($_ -match '^([^=]+)=(.*)') {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
        }
    }}
}

$VisualStudioPath = 'C:\Program Files\Microsoft Visual Studio\2022\Community'

if ($null -eq $Env:VSCMD_VER) {
    Invoke-Environment "$VisualStudioPath\VC\Auxiliary\Build\vcvars64.bat"
}

$Platforms = 'Win32', 'x64'
$Configurations = 'Debug', 'Release' # Development Release Trace?

$BasePath = $PSScriptRoot
$RepositoryPath = "$BasePath"
$BuildDirectoryName = 'bin'

$BuildBinary = $true
$BuildSetup = $true

$Release = $true
$Date = Get-Date -Format "yyyyMMdd"
$Zip = "C:\Program Files\7-Zip\7z"

$Type = "Rebuild" # Build, Rebuild

if (!(Test-Path "$RepositoryPath\$BuildDirectoryName")) {
    New-Item -Path $RepositoryPath -Name $BuildDirectoryName -ItemType Directory
}

if($BuildBinary) {
    $Platforms | ForEach-Object {
        $Platform = $_
        $Configurations | ForEach-Object {
            $Configuration = $_
            Write-Host "Configuration $Configuration, Platform $Platform"
            $Build = Start-Process "msbuild.exe" -ArgumentList "`"mp4.sln`" /t:$Type /p:Configuration=`"$Configuration`";Platform=`"$Platform`" /nodeReuse:false" -WorkingDirectory $RepositoryPath -PassThru -NoNewWindow -Wait
            $ExitCode = $Build.ExitCode
            Write-Host "Build ExitCode $ExitCode"
            if($ExitCode -ne 0) {
                throw "Failed to build Configuration $Configuration, Platform $Platform"
            }
        }
    }
}

if($BuildSetup) {
    $Build = Start-Process "C:\Program Files (x86)\NSIS\makensisw.exe" -ArgumentList "`"$RepositoryPath\install\Setup.nsi`"" -WorkingDirectory $RepositoryPath -PassThru -NoNewWindow -Wait
    $ExitCode = $Build.ExitCode
    Write-Host "Makensisw ExitCode $ExitCode"
    if($ExitCode -ne 0) {
        throw "Failed to build setup"
    }
}
