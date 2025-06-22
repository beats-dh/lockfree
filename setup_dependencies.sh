#!/bin/bash

# Setup script for LockFree Object Pool dependencies
# This script installs dependencies via vcpkg package manager

set -e  # Exit on any error

echo -e "\033[32mLockFree Object Pool - Dependency Setup\033[0m"
echo -e "\033[32m=========================================\033[0m"
echo ""

# Check if vcpkg is available
if [ -z "$VCPKG_ROOT" ]; then
    echo -e "\033[31mError: VCPKG_ROOT environment variable is not set\033[0m"
    echo -e "\033[31mPlease install vcpkg and set VCPKG_ROOT environment variable\033[0m"
    echo -e "\033[33mSee VCPKG.md for detailed instructions\033[0m"
    exit 1
fi

VCPKG_PATH="$VCPKG_ROOT/vcpkg"
if [ ! -f "$VCPKG_PATH" ]; then
    echo -e "\033[31mError: vcpkg not found at $VCPKG_PATH\033[0m"
    echo -e "\033[31mPlease ensure vcpkg is properly installed\033[0m"
    exit 1
fi

echo -e "\033[36mUsing vcpkg at: $VCPKG_ROOT\033[0m"
echo ""

# Function to install vcpkg package
install_vcpkg_package() {
    local package_name="$1"
    local triplet="${2:-x64-linux}"
    
    echo -e "\033[36mInstalling $package_name...\033[0m"
    
    if "$VCPKG_PATH" install "$package_name:$triplet" 2>/dev/null; then
        echo -e "  \033[32m✓ $package_name installed successfully\033[0m"
        return 0
    else
        echo -e "  \033[31m✗ Failed to install $package_name\033[0m"
        return 1
    fi
}

# Read dependencies from vcpkg.json
if [ ! -f "vcpkg.json" ]; then
    echo -e "\033[31mError: vcpkg.json not found\033[0m"
    exit 1
fi

echo -e "\033[33mReading dependencies from vcpkg.json...\033[0m"
# Extract dependencies using grep and sed (more portable than jq)
dependencies=($(grep -A 20 '"dependencies"' vcpkg.json | grep '"' | sed 's/.*"\([^"]*\)".*/\1/' | grep -v dependencies))
echo -e "\033[37mFound ${#dependencies[@]} dependencies\033[0m"
echo ""

# Install dependencies
success=true
triplet="${VCPKG_DEFAULT_TRIPLET:-x64-linux}"

# Detect OS and set appropriate triplet
if [[ "$OSTYPE" == "darwin"* ]]; then
    triplet="${VCPKG_DEFAULT_TRIPLET:-x64-osx}"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    triplet="${VCPKG_DEFAULT_TRIPLET:-x64-windows}"
fi

echo -e "\033[37mInstalling dependencies for triplet: $triplet\033[0m"
echo ""

for i in "${!dependencies[@]}"; do
    dep="${dependencies[$i]}"
    echo -e "\033[37m$((i + 1)). Installing $dep...\033[0m"
    if ! install_vcpkg_package "$dep" "$triplet"; then
        success=false
    fi
    echo ""
done

echo ""
if [ "$success" = true ]; then
    echo -e "\033[32m✓ All dependencies installed successfully!\033[0m"
    
    # Verify vcpkg integration
    echo ""
    echo -e "\033[33mVerifying vcpkg integration...\033[0m"
    
    declare -a checks=(
        "atomic-queue"
        "parallel-hashmap"
        "bshoshany-thread-pool"
        "benchmark"
        "gtest"
        "spdlog"
    )
    
    all_found=true
    for check in "${checks[@]}"; do
        if "$VCPKG_PATH" list "$check" 2>/dev/null | grep -q "$check"; then
            echo -e "  \033[32m✓ $check is installed\033[0m"
        else
            echo -e "  \033[31m✗ $check not found\033[0m"
            all_found=false
        fi
    done
    
    if [ "$all_found" = true ]; then
        echo ""
        echo -e "\033[32m✓ All dependencies are properly installed!\033[0m"
        echo -e "\033[37mYou can now build the project with:\033[0m"
        echo -e "  \033[36mcmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake\033[0m"
        echo -e "  \033[36mcmake --build build --config Release\033[0m"
    else
        echo ""
        echo -e "\033[33m⚠ Some dependencies may not be properly installed\033[0m"
        echo -e "\033[33mPlease check the vcpkg installation manually\033[0m"
    fi
else
    echo -e "\033[31m✗ Some dependencies failed to install\033[0m"
    echo -e "\033[31mPlease check the errors above and try again\033[0m"
    echo -e "\033[33mYou may need to run: $VCPKG_PATH integrate install\033[0m"
    exit 1
fi

echo ""
echo -e "\033[32mSetup completed!\033[0m"
echo -e "\033[36mNote: This project now uses vcpkg for dependency management\033[0m"
echo -e "\033[36mSee VCPKG.md for more information about vcpkg usage\033[0m"