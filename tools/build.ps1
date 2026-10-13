# ============================================================
# 腕部驱动设备上位机 — 一键构建脚本
#
# 作用：用 Qt 6.8.2 + MinGW 13.1 编译 src/ 下的源码，
#       并把 Qt / MinGW 运行库部署到 exe 同目录，使其可直接双击运行。
#
# 背景：Qt 的 moc.exe 无法处理含中文的路径（会把 UTF-8 路径按 GBK 解析，
#       导致 “Cannot create ... No such file or directory”）。
#       本脚本通过一个 ASCII 路径的目录联接（junction）指向项目目录来构建，
#       不复制、不移动任何源码，也不影响直接运行 exe。
#
# 用法：在 PowerShell 中执行   .\build.ps1
# ============================================================

$ErrorActionPreference = 'Stop'

# ── 工具链路径 ──────────────────────────────────────────────
$qtDir     = 'C:\Qt\6.8.2\mingw_64'
$mingwDir  = 'C:\Qt\Tools\mingw1310_64'
$cmakeExe  = 'C:\Qt\Tools\CMake_64\bin\cmake.exe'
$ninjaExe  = 'C:\Qt\Tools\Ninja\ninja.exe'
$deployExe = Join-Path $qtDir 'bin\windeployqt.exe'

$kitName = 'Desktop_Qt_6_8_2_MinGW_64_bit_Release'

# ── 目录 ────────────────────────────────────────────────────
# 项目根目录 = 本脚本所在目录(tools)的上一级
$root = Split-Path -Parent $PSScriptRoot
# ASCII 联接路径，用于绕过 moc 的中文路径问题
$link = 'C:\Users\Lenovo\uart_wrist'

# ── 检查工具链 ──────────────────────────────────────────────
$required = @($cmakeExe, $ninjaExe, $deployExe,
              (Join-Path $mingwDir 'bin\g++.exe'),
              (Join-Path $qtDir 'lib\cmake\Qt6Charts\Qt6ChartsConfig.cmake'),
              (Join-Path $qtDir 'lib\cmake\Qt6SerialPort\Qt6SerialPortConfig.cmake'))
foreach ($p in $required) {
    if (-not (Test-Path $p)) {
        throw "缺少必要组件：$p`n请用 Qt MaintenanceTool 安装 Qt Charts 与 Qt Serial Port。"
    }
}

# ── 确保 ASCII 联接存在 ─────────────────────────────────────
if (-not (Test-Path $link)) {
    New-Item -ItemType Junction -Path $link -Target $root | Out-Null
    Write-Host "[目录联接] 已创建 $link  ->  $root"
}

$srcAscii   = Join-Path $link 'src'
$buildAscii = Join-Path $link ('build\' + $kitName)
if (-not (Test-Path (Join-Path $srcAscii 'CMakeLists.txt'))) {
    throw "找不到源码：$srcAscii\CMakeLists.txt（目录联接是否失效？）"
}

# 把 Qt 与 MinGW 的 DLL 目录加入 PATH，供构建与部署使用
$env:Path = "$qtDir\bin;$mingwDir\bin;" + $env:Path

# ── 配置 ────────────────────────────────────────────────────
$fwdQt     = $qtDir    -replace '\\', '/'
$fwdGcc    = "$mingwDir\bin\gcc.exe" -replace '\\', '/'
$fwdGxx    = "$mingwDir\bin\g++.exe" -replace '\\', '/'
$fwdNinja  = $ninjaExe -replace '\\', '/'

$cfgArgs = @(
    '-S', $srcAscii,
    '-B', $buildAscii,
    '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_PREFIX_PATH=$fwdQt",
    "-DCMAKE_C_COMPILER=$fwdGcc",
    "-DCMAKE_CXX_COMPILER=$fwdGxx",
    "-DCMAKE_MAKE_PROGRAM=$fwdNinja"
)

Write-Host "[1/3] 配置 CMake ..."
& $cmakeExe @cfgArgs
if ($LASTEXITCODE -ne 0) { throw '  CMake 配置失败' }

# ── 编译 ────────────────────────────────────────────────────
Write-Host "[2/3] 编译 ..."
& $cmakeExe --build $buildAscii
if ($LASTEXITCODE -ne 0) { throw '  编译失败' }

# ── 部署运行库 ──────────────────────────────────────────────
Write-Host "[3/3] 部署 Qt / MinGW 运行库 ..."
$exe = Join-Path $buildAscii 'Uart_controller.exe'
if (-not (Test-Path $exe)) { throw "  未生成可执行文件：$exe" }
& $deployExe --release --no-translations --compiler-runtime $exe
if ($LASTEXITCODE -ne 0) { throw '  运行库部署失败' }

Write-Host ''
Write-Host '构建完成，可直接运行：'
Write-Host "  $exe"
