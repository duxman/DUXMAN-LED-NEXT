param(
  [ValidateSet("esp32c3supermini", "esp32dev", "esp32s3")]
  [string]$Profile = "esp32c3supermini",

  [ValidateSet("build", "upload", "upload-monitor", "monitor")]
  [string]$Action = "upload",

  [string]$Port,

  [int]$Baud = 115200,

  [switch]$ListPorts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-RepoRoot {
  $scriptDir = Split-Path -Parent $PSCommandPath
  return Split-Path -Parent $scriptDir
}

function Get-PlatformIoWorkingDirectory {
  $repoRoot = Get-RepoRoot
  $rootIni = Join-Path $repoRoot "platformio.ini"
  if (Test-Path $rootIni) {
    return $repoRoot
  }

  $firmwarePath = Join-Path $repoRoot "firmware"
  $firmwareIni = Join-Path $firmwarePath "platformio.ini"
  if (Test-Path $firmwareIni) {
    return $firmwarePath
  }

  throw "No se encontro platformio.ini ni en la raiz ni en firmware/."
}

function Get-SerialPorts {
  $ports = @(Get-CimInstance Win32_SerialPort |
    Select-Object DeviceID, Name, Description)

  return $ports
}

function Get-AutoPort {
  $ports = Get-SerialPorts
  if (-not $ports -or $ports.Count -eq 0) {
    throw "No se encontraron puertos serie disponibles."
  }

  $usbLike = @($ports | Where-Object {
    $_.DeviceID -match "^COM\\d+$" -and
    $_.DeviceID -ne "COM1" -and
    ($_.Name -match "USB|CP210|CH340|CH910|UART|Silicon Labs|FTDI" -or $_.Description -match "USB|UART")
  })

  if ($usbLike.Count -eq 1) {
    return $usbLike[0].DeviceID
  }

  $nonCom1 = @($ports | Where-Object { $_.DeviceID -match "^COM\\d+$" -and $_.DeviceID -ne "COM1" })
  if ($nonCom1.Count -eq 1) {
    return $nonCom1[0].DeviceID
  }

  $allCom = @($ports | Where-Object { $_.DeviceID -match "^COM\\d+$" })
  if ($allCom.Count -eq 1) {
    return $allCom[0].DeviceID
  }

  $portList = ($ports | ForEach-Object { $_.DeviceID }) -join ", "
  throw "No se pudo detectar automaticamente un puerto unico. Usa -Port COMx. Puertos: $portList"
}

function Invoke-Pio {
  param(
    [string[]]$CommandArgs,
    [string]$WorkingDirectory
  )

  Write-Host "[flash] pio $($CommandArgs -join ' ')" -ForegroundColor Cyan
  Push-Location $WorkingDirectory
  try {
    & pio @CommandArgs
    if ($LASTEXITCODE -ne 0) {
      throw "PlatformIO devolvio codigo $LASTEXITCODE"
    }
  }
  finally {
    Pop-Location
  }
}

if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
  throw "No se encontro el comando pio en PATH."
}

if ($ListPorts) {
  $ports = Get-SerialPorts
  if (-not $ports -or $ports.Count -eq 0) {
    Write-Host "No se encontraron puertos serie." -ForegroundColor Yellow
    exit 0
  }

  $ports | Format-Table -AutoSize
  exit 0
}

$platformIoPath = Get-PlatformIoWorkingDirectory

switch ($Action) {
  "build" {
    Invoke-Pio -CommandArgs @("run", "-e", $Profile) -WorkingDirectory $platformIoPath
  }

  "upload" {
    if ([string]::IsNullOrWhiteSpace($Port)) {
      $Port = Get-AutoPort
    }

    Write-Host "[flash] Perfil: $Profile" -ForegroundColor Green
    Write-Host "[flash] Puerto: $Port" -ForegroundColor Green
    Invoke-Pio -CommandArgs @("run", "-e", $Profile, "-t", "upload", "--upload-port", $Port) -WorkingDirectory $platformIoPath
  }

  "upload-monitor" {
    if ([string]::IsNullOrWhiteSpace($Port)) {
      $Port = Get-AutoPort
    }

    Write-Host "[flash] Perfil: $Profile" -ForegroundColor Green
    Write-Host "[flash] Puerto: $Port" -ForegroundColor Green
    Invoke-Pio -CommandArgs @("run", "-e", $Profile, "-t", "upload", "--upload-port", $Port) -WorkingDirectory $platformIoPath
    Invoke-Pio -CommandArgs @("device", "monitor", "-p", $Port, "-b", $Baud.ToString()) -WorkingDirectory $platformIoPath
  }

  "monitor" {
    if ([string]::IsNullOrWhiteSpace($Port)) {
      $Port = Get-AutoPort
    }

    Write-Host "[flash] Perfil: $Profile" -ForegroundColor Green
    Write-Host "[flash] Puerto: $Port" -ForegroundColor Green
    Invoke-Pio -CommandArgs @("device", "monitor", "-p", $Port, "-b", $Baud.ToString()) -WorkingDirectory $platformIoPath
  }
}
