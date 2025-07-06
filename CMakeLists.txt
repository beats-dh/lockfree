cmake_minimum_required(VERSION 3.20)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "")
endif()

if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET} AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}" CACHE STRING "")
endif()

project(LockFreeObjectPool VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find dependencies via vcpkg
# atomic-queue and parallel-hashmap are header-only libraries
find_path(ATOMIC_QUEUE_INCLUDE_DIRS "atomic_queue/atomic_queue.h")
find_path(PARALLEL_HASHMAP_INCLUDE_DIRS "parallel_hashmap/btree.h")
find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")
find_package(spdlog CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
    add_compile_options(/arch:AVX2)  # Enable AVX2 if available
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-march=native -mtune=native)  # Enable native optimizations
endif()

# Build type specific optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(NDEBUG)
    if(MSVC)
        # Maximum optimization for MSVC Release
        add_compile_options(/O2 /Ob2 /Oi /Ot /Oy /GL)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
        # Enable function-level linking
        add_compile_options(/Gy)
        # Additional MSVC release optimizations
        add_compile_options(/fp:fast /GS-)
    else()
        # Maximum optimization for GCC/Clang Release
        add_compile_options(-O3 -DNDEBUG -flto -ffast-math)
        add_compile_options(-funroll-loops -finline-functions)
        add_link_options(-flto)
        # Use gold linker only on Linux (not available on macOS)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            add_link_options(-fuse-ld=gold)
        endif()
        # Additional GCC/Clang release optimizations
        add_compile_options(-fomit-frame-pointer -fno-stack-protector)
    endif()
endif()

# Configurar codificação UTF-8 para MSVC
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Para outros compiladores
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# Add include directories
include_directories(include)

# Find required packages
find_package(Threads REQUIRED)

# Header-only library target
add_library(lockfree_object_pool INTERFACE)
target_include_directories(lockfree_object_pool INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(lockfree_object_pool INTERFACE cxx_std_20)
# Include directories for header-only libraries
target_include_directories(lockfree_object_pool INTERFACE
    $<BUILD_INTERFACE:${ATOMIC_QUEUE_INCLUDE_DIRS}>
    $<BUILD_INTERFACE:${PARALLEL_HASHMAP_INCLUDE_DIRS}>
    $<BUILD_INTERFACE:${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS}>
    $<INSTALL_INTERFACE:include>
)

# Link dependencies to the interface library
target_link_libraries(lockfree_object_pool INTERFACE
    Threads::Threads
    spdlog::spdlog_header_only
)

# Create main benchmark executable
if(WIN32)
    # Add Windows resource file for icon
    add_executable(lockfree_benchmark examples/main_test_lockfree.cpp resources/app.rc)
else()
    add_executable(lockfree_benchmark examples/main_test_lockfree.cpp)
endif()

target_link_libraries(lockfree_benchmark PRIVATE 
    lockfree_object_pool
    benchmark::benchmark
    spdlog::spdlog_header_only
)
target_compile_features(lockfree_benchmark PRIVATE cxx_std_20)

# Performance-specific optimizations for benchmark
target_compile_definitions(lockfree_benchmark PRIVATE
    BENCHMARK_STATIC_DEFINE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF  # Disable logging in release
)

# Platform-specific optimizations
if(WIN32)
    target_compile_definitions(lockfree_benchmark PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00  # Windows 10+
    )
    # Windows-specific optimizations
else()
    # Linux/Unix specific optimizations
    target_compile_definitions(lockfree_benchmark PRIVATE
        _GNU_SOURCE
    )
endif()

# Set runtime output directory for better organization
set_target_properties(lockfree_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

# Enable testing
enable_testing()
add_test(NAME lockfree_integration_test
    COMMAND lockfree_benchmark --integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation
install(TARGETS lockfree_object_pool
    EXPORT lockfree_object_pool_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT lockfree_object_pool_targets
    FILE lockfree_object_pool_targets.cmake
    NAMESPACE lockfree::
    DESTINATION lib/cmake/lockfree_object_pool
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    lockfree_object_pool_config_version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lockfree_object_pool_config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/lockfree_object_pool_config.cmake
    INSTALL_DESTINATION lib/cmake/lockfree_object_pool
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lockfree_object_pool_config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/lockfree_object_pool_config_version.cmake
    DESTINATION lib/cmake/lockfree_object_pool
)

# Documentation target (optional)
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
    set(DOXYGEN_PROJECT_NAME "LockFree Object Pool")
    set(DOXYGEN_PROJECT_BRIEF "Ultra-fast lock-free object pool implementation")
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_LATEX NO)
    
    doxygen_add_docs(docs
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        COMMENT "Generating documentation"
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "LockFree Object Pool Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")