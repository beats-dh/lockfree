# Guia de Compilação - LockFree Object Pool

Este documento fornece instruções detalhadas para compilar e testar o LockFree Object Pool usando vcpkg.

## Pré-requisitos

### Compiladores Suportados
- **GCC**: 10.0 ou superior
- **Clang**: 12.0 ou superior  
- **MSVC**: Visual Studio 2022 (v143) ou superior
- **Apple Clang**: 13.0 ou superior (macOS)

### Ferramentas Necessárias
- **CMake**: 3.20 ou superior
- **vcpkg**: Para gerenciamento de dependências
- **Git**: Para clonar o projeto
- **Make/Ninja**: Sistema de build (opcional, mas recomendado)

### Padrão C++
- **C++20**: Obrigatório para todas as funcionalidades

## Configuração do vcpkg

Antes de compilar, configure o vcpkg seguindo as instruções em [VCPKG.md](VCPKG.md).

## Configuração Rápida

### 1. Clone o Repositório
```bash
# Clone o repositório
git clone https://github.com/beats-dh/lockfree.git
cd lockfree-object-pool
```

### 2. Configure e Compile com vcpkg
```bash
# Configure o build (dependências instaladas automaticamente)
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=[caminho-vcpkg]/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release

# Compile
cmake --build build --config Release --parallel
```

### 3. Execute
```bash
# Teste básico
./build/bin/simple_example

# Benchmarks completos
./build/bin/lockfree_benchmark
```

> **Nota**: Substitua `[caminho-vcpkg]` pelo caminho real da sua instalação do vcpkg.

## Building with CMake

### Prerequisites
- CMake 3.20 or higher
- vcpkg (see [VCPKG.md](VCPKG.md) for setup)
- C++20 compatible compiler (MSVC 2019+, GCC 10+, Clang 10+)

### Build Steps

#### Option 1: Using CMakePresets (Recommended)

1. **Configure vcpkg** (if not already done):
   ```bash
   # Set VCPKG_ROOT environment variable
   export VCPKG_ROOT=/path/to/vcpkg  # Linux/macOS
   set VCPKG_ROOT=C:\path\to\vcpkg   # Windows
   ```

2. **List available presets**:
   ```bash
   cmake --list-presets
   ```

3. **Configure with preset**:
   ```bash
   # Windows
   cmake --preset windows-release
   
   # Linux
   cmake --preset linux-release
   
   # macOS
   cmake --preset macos-release
   ```

4. **Build with preset**:
   ```bash
   cmake --build --preset windows-release  # Windows
   cmake --build --preset linux-release    # Linux
   cmake --build --preset macos-release    # macOS
   ```

5. **Run tests with preset**:
   ```bash
   ctest --preset windows-release  # Windows
   ctest --preset linux-release    # Linux
   ctest --preset macos-release    # macOS
   ```

#### Option 2: Manual CMake Configuration

1. **Configure vcpkg** (if not already done):
   ```bash
   # Set VCPKG_ROOT environment variable
   export VCPKG_ROOT=/path/to/vcpkg  # Linux/macOS
   set VCPKG_ROOT=C:\path\to\vcpkg   # Windows
   ```

2. **Configure the project**:
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
   ```

3. **Build**:
   ```bash
   cmake --build build --config Release
   ```

4. **Run tests** (optional):
   ```bash
   cd build
   ctest --output-on-failure
   ```

## Build Detalhado

### Configuração do CMake

#### Opções Básicas
```bash
# Build padrão (Release)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build com debug
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Especificar compilador
cmake -B build -S . -DCMAKE_CXX_COMPILER=g++-11
```

#### Opções Avançadas
```bash
# Habilitar otimizações específicas
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-march=native -mtune=native"

# Build com sanitizers (Debug)
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fsanitize=undefined"

# Build com profiling
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS="-pg -fno-omit-frame-pointer"
```

### Compilação

#### Build Paralelo
```bash
# Usar todos os cores disponíveis
cmake --build build --config Release --parallel

# Especificar número de jobs
cmake --build build --config Release --parallel 8
```

#### Targets Específicos
```bash
# Compilar apenas a biblioteca
cmake --build build --target lockfree_object_pool

# Compilar apenas os benchmarks
cmake --build build --target lockfree_benchmark

# Gerar documentação (se Doxygen estiver disponível)
cmake --build build --target docs
```

## Executando Testes

### Testes Básicos
```bash
# Executar teste de integração
./build/bin/lockfree_benchmark --integration

# Executar suite completa
./build/bin/lockfree_benchmark --complete

# Executar testes leves (para CI/CD)
./build/bin/lockfree_benchmark --light
```

### Testes Específicos
```bash
# Apenas benchmarks baseline
./build/bin/lockfree_benchmark --baseline

# Apenas testes multi-threaded
./build/bin/lockfree_benchmark --multithread --threads 8

# Stress tests com configuração customizada
./build/bin/lockfree_benchmark --stress --ops 100000
```

### Testes com CTest
```bash
# Executar todos os testes registrados
cd build
ctest

# Executar com output verboso
ctest --verbose

# Executar testes específicos
ctest -R integration
```

## Configurações de Performance

### Otimizações de Compilador

#### GCC/Clang
```bash
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -flto -ffast-math"
```

#### MSVC
```bash
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="/O2 /Ob2 /arch:AVX2 /GL"
```

### Profile-Guided Optimization (PGO)

#### GCC
```bash
# 1. Build com instrumentação
cmake -B build-pgo -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-fprofile-generate"
cmake --build build-pgo

# 2. Executar benchmarks para coletar dados
./build-pgo/bin/lockfree_benchmark --complete

# 3. Build final com otimização
cmake -B build-optimized -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-fprofile-use -fprofile-correction"
cmake --build build-optimized
```

## Troubleshooting

### Problemas Comuns

#### Erro: "C++20 features not supported"
**Solução**: Atualize o compilador ou use flags específicas:
```bash
# GCC
cmake -B build -S . -DCMAKE_CXX_FLAGS="-std=c++20"

# Clang
cmake -B build -S . -DCMAKE_CXX_FLAGS="-std=c++20 -stdlib=libc++"
```

#### Erro: "atomic_queue.h not found"
**Solução**: Execute o script de dependências:
```bash
./setup_dependencies.sh  # Linux/macOS
.\setup_dependencies.ps1  # Windows
```

#### Erro: "Linking failed"
**Solução**: Verifique se todas as dependências estão instaladas:
```bash
# Ubuntu/Debian
sudo apt install build-essential cmake git

# CentOS/RHEL
sudo yum install gcc-c++ cmake git

# macOS
brew install cmake
```

#### Performance baixa
**Verificações**:
1. Certifique-se de estar usando build Release
2. Verifique se as otimizações estão habilitadas
3. Use `-march=native` para otimizações específicas do CPU
4. Considere usar PGO para workloads específicos

### Debug de Performance

#### Profiling com perf (Linux)
```bash
# Compilar com símbolos de debug
cmake -B build -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build

# Executar com profiling
perf record ./build/bin/lockfree_benchmark --stress
perf report
```

#### Profiling com Instruments (macOS)
```bash
# Executar com Instruments
instruments -t "Time Profiler" ./build/bin/lockfree_benchmark --stress
```

#### Profiling com Visual Studio (Windows)
1. Abra o projeto no Visual Studio
2. Debug → Performance Profiler
3. Selecione "CPU Usage"
4. Execute o benchmark

## Integração Contínua

### GitHub Actions
```yaml
# .github/workflows/ci.yml
name: CI
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    - name: Setup dependencies
      run: ./setup_dependencies.sh
    - name: Configure
      run: cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    - name: Build
      run: cmake --build build --parallel
    - name: Test
      run: ./build/bin/lockfree_benchmark --light
```

### Docker Build
```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    build-essential cmake git
COPY . /app
WORKDIR /app
RUN ./setup_dependencies.sh
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --parallel
CMD ["./build/bin/lockfree_benchmark", "--light"]
```

## Instalação

### Instalação Local
```bash
# Configurar com prefixo de instalação
cmake -B build -S . -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

### Criação de Pacote
```bash
# Criar pacote DEB (Ubuntu/Debian)
cpack -G DEB

# Criar pacote RPM (CentOS/RHEL)
cpack -G RPM

# Criar pacote ZIP
cpack -G ZIP
```

## Uso como Dependência

### CMake find_package
```cmake
find_package(lockfree_object_pool REQUIRED)
target_link_libraries(my_target PRIVATE lockfree::lockfree_object_pool)
```

### CMake FetchContent
```cmake
include(FetchContent)
FetchContent_Declare(
  lockfree_object_pool
  GIT_REPOSITORY https://github.com/beats-dh/lockfree.git
  GIT_TAG main
)
FetchContent_MakeAvailable(lockfree_object_pool)
target_link_libraries(my_target PRIVATE lockfree::lockfree_object_pool)
```

```cmake
add_subdirectory(third_party/lockfree)
target_link_libraries(my_target PRIVATE lockfree_object_pool)
```