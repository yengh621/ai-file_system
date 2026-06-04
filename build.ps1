# PowerShell 构建脚本
$ErrorActionPreference = "Continue"
$buildErrors = @()

# ==========自动加载MSYS2 MinGW64环境==========
$mingwBin = "C:\msys64\mingw64\bin"
if($env:PATH -notcontains $mingwBin){
    $env:PATH = "$mingwBin;$env:PATH"
}
# ===========================================

Write-Host "Building filesystem..." -ForegroundColor Green

# 清空+重建build，防止旧垃圾文件
if (Test-Path "build") {
    Remove-Item build -Recurse -Force
}
New-Item -ItemType Directory -Path "build" | Out-Null

# 编译核心模块
Write-Host "Compiling core modules..." -ForegroundColor Yellow
$files = @(
    "src/core/main.c",
    "src/core/disk.c",
    "src/core/inode.c",
    "src/core/block.c",
    "src/core/directory.c",
    "src/core/file.c",
    "src/core/user.c",
    "src/core/permission.c",
    "src/core/process_lock.c",
    "src/core/ai.c"
)

foreach ($file in $files) {
    $objName = "build/" + [System.IO.Path]::GetFileNameWithoutExtension($file) + ".o"
    Write-Host "  Compiling $file -> $objName" -NoNewline
    $output = gcc -Wall -Wextra -Iinclude -c $file -o $objName 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host " [FAILED]" -ForegroundColor Red
        $buildErrors += "Failed to compile $file"
        Write-Host $output -ForegroundColor Red
    } else {
        Write-Host " [OK]" -ForegroundColor Green
    }
}

# 编译创新模块
Write-Host "Compiling innovation modules..." -ForegroundColor Yellow
$innovationFiles = @(
    "src/innovations/kfs.c",
    "src/innovations/io_optimizer.c",
    "src/innovations/security.c",
    "src/innovations/link.c",
    "src/innovations/file_ops.c"
)

foreach ($file in $innovationFiles) {
    $objName = "build/" + [System.IO.Path]::GetFileNameWithoutExtension($file) + ".o"
    Write-Host "  Compiling $file -> $objName" -NoNewline
    $output = gcc -Wall -Wextra -Iinclude -c $file -o $objName 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host " [FAILED]" -ForegroundColor Red
        $buildErrors += "Failed to compile $file"
        Write-Host $output -ForegroundColor Red
    } else {
        Write-Host " [OK]" -ForegroundColor Green
    }
}

# 链接
Write-Host "Linking..." -ForegroundColor Yellow
$output = gcc -Wall -Wextra -o filesystem.exe build/*.o 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host " [FAILED]" -ForegroundColor Red
    $buildErrors += "Failed to link"
    Write-Host $output -ForegroundColor Red
} else {
    Write-Host " [OK]" -ForegroundColor Green
}

# 结果汇总
Write-Host "`n========================================" -ForegroundColor Cyan
if ($buildErrors.Count -eq 0) {
    Write-Host "Build complete!" -ForegroundColor Green
    Write-Host "Run: .\filesystem.exe" -ForegroundColor Cyan
} else {
    Write-Host "Build failed with $($buildErrors.Count) error(s):" -ForegroundColor Red
    foreach ($err in $buildErrors) {
        Write-Host "  - $err" -ForegroundColor Red
    }
}
Write-Host "========================================" -ForegroundColor Cyan
