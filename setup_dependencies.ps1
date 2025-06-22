# Setup script for LockFree Object Pool dependencies
# This script installs dependencies via vcpkg package manager

Write-Host "LockFree Object Pool - Dependency Setup" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""

# Check if vcpkg is available
if (-not $env:VCPKG_ROOT) {
    Write-Host "Error: VCPKG_ROOT environment variable is not set" -ForegroundColor Red
    Write-Host "Please install vcpkg and set VCPKG_ROOT environment variable" -ForegroundColor Red
    Write-Host "See VCPKG.md for detailed instructions" -ForegroundColor Yellow
    exit 1
}

$vcpkgPath = Join-Path $env:VCPKG_ROOT "vcpkg.exe"
if (-not (Test-Path $vcpkgPath)) {
    Write-Host "Error: vcpkg.exe not found at $vcpkgPath" -ForegroundColor Red
    Write-Host "Please ensure vcpkg is properly installed" -ForegroundColor Red
    exit 1
}

Write-Host "Using vcpkg at: $env:VCPKG_ROOT" -ForegroundColor Cyan
Write-Host ""

# Function to install vcpkg package
function Install-VcpkgPackage {
    param(
        [string]$PackageName,
        [string]$Triplet = "x64-windows"
    )
    
    Write-Host "Installing $PackageName..." -ForegroundColor Cyan
    
    $result = & $vcpkgPath install "$PackageName`:$Triplet" 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ $PackageName installed successfully" -ForegroundColor Green
        return $true
    } else {
        Write-Host "  ✗ Failed to install $PackageName" -ForegroundColor Red
        Write-Host "  Error: $result" -ForegroundColor Red
        return $false
    }
}

# Read dependencies from vcpkg.json
if (Test-Path "vcpkg.json") {
    Write-Host "Reading dependencies from vcpkg.json..." -ForegroundColor Yellow
    $vcpkgConfig = Get-Content "vcpkg.json" | ConvertFrom-Json
    $dependencies = $vcpkgConfig.dependencies
    Write-Host "Found $($dependencies.Count) dependencies" -ForegroundColor White
    Write-Host ""
} else {
    Write-Host "Error: vcpkg.json not found" -ForegroundColor Red
    exit 1
}

# Install dependencies
$success = $true
$triplet = if ($env:VCPKG_DEFAULT_TRIPLET) { $env:VCPKG_DEFAULT_TRIPLET } else { "x64-windows" }

Write-Host "Installing dependencies for triplet: $triplet" -ForegroundColor White
Write-Host ""

for ($i = 0; $i -lt $dependencies.Count; $i++) {
    $dep = $dependencies[$i]
    Write-Host "$($i + 1). Installing $dep..." -ForegroundColor White
    $success = $success -and (Install-VcpkgPackage $dep $triplet)
    Write-Host ""
}

Write-Host ""
if ($success) {
    Write-Host "✓ All dependencies installed successfully!" -ForegroundColor Green
    
    # Verify vcpkg integration
    Write-Host ""
    Write-Host "Verifying vcpkg integration..." -ForegroundColor Yellow
    
    $checks = @(
        @{Command="$vcpkgPath list atomic-queue"; Name="atomic-queue"},
        @{Command="$vcpkgPath list parallel-hashmap"; Name="parallel-hashmap"},
        @{Command="$vcpkgPath list bshoshany-thread-pool"; Name="bshoshany-thread-pool"},
        @{Command="$vcpkgPath list benchmark"; Name="benchmark"},
        @{Command="$vcpkgPath list gtest"; Name="gtest"},
        @{Command="$vcpkgPath list spdlog"; Name="spdlog"}
    )
    
    $allFound = $true
    foreach ($check in $checks) {
        $result = Invoke-Expression $check.Command 2>$null
        if ($result -and $result.Length -gt 0) {
            Write-Host "  ✓ $($check.Name) is installed" -ForegroundColor Green
        } else {
            Write-Host "  ✗ $($check.Name) not found" -ForegroundColor Red
            $allFound = $false
        }
    }
    
    if ($allFound) {
        Write-Host ""
        Write-Host "✓ All dependencies are properly installed!" -ForegroundColor Green
        Write-Host "You can now build the project with:" -ForegroundColor White
        Write-Host "  cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -ForegroundColor Cyan
        Write-Host "  cmake --build build --config Release" -ForegroundColor Cyan
    } else {
        Write-Host ""
        Write-Host "⚠ Some dependencies may not be properly installed" -ForegroundColor Yellow
        Write-Host "Please check the vcpkg installation manually" -ForegroundColor Yellow
    }
} else {
    Write-Host "✗ Some dependencies failed to install" -ForegroundColor Red
    Write-Host "Please check the errors above and try again" -ForegroundColor Red
    Write-Host "You may need to run: vcpkg integrate install" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "Setup completed!" -ForegroundColor Green
Write-Host "Note: This project now uses vcpkg for dependency management" -ForegroundColor Cyan
Write-Host "See VCPKG.md for more information about vcpkg usage" -ForegroundColor Cyan