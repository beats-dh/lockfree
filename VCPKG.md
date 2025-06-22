# Usando vcpkg com LockFree Object Pool

Este projeto usa o sistema de manifesto do vcpkg para gerenciar dependências automaticamente.

## Pré-requisitos

1. **Instalar vcpkg**:
   ```bash
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat  # Windows
   # ou
   ./bootstrap-vcpkg.sh   # Linux/macOS
   ```

2. **Integrar vcpkg com CMake**:
   ```bash
   .\vcpkg integrate install  # Windows
   # ou
   ./vcpkg integrate install  # Linux/macOS
   ```

## Configuração do Projeto

### Opção 1: Usando CMake Toolchain (Recomendado)

```bash
# Configure o projeto
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[caminho-para-vcpkg]/scripts/buildsystems/vcpkg.cmake

# Compile
cmake --build build --config Release
```

### Opção 2: Usando vcpkg integrate

Se você já executou `vcpkg integrate install`:

```bash
# Configure o projeto
cmake -B build -S .

# Compile
cmake --build build --config Release
```

### Opção 3: Usando variável de ambiente

```bash
# Windows
set VCPKG_ROOT=C:\caminho\para\vcpkg
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

# Linux/macOS
export VCPKG_ROOT=/caminho/para/vcpkg
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

## Dependências Gerenciadas

O arquivo `vcpkg.json` define as seguintes dependências:

- **atomic-queue** (≥1.5.0): Filas lock-free de alta performance
- **parallel-hashmap** (≥1.3.12): Hash maps paralelos otimizados
- **thread-pool** (≥4.1.0): Pool de threads para processamento paralelo
- **benchmark** (≥1.8.0): Framework para benchmarks de performance
- **gtest** (≥1.14.0): Framework de testes unitários

## Instalação Manual das Dependências

Se preferir instalar as dependências manualmente:

```bash
# Instalar todas as dependências
vcpkg install atomic-queue parallel-hashmap thread-pool benchmark gtest

# Ou instalar para uma triplet específica
vcpkg install atomic-queue:x64-windows parallel-hashmap:x64-windows thread-pool:x64-windows benchmark:x64-windows gtest:x64-windows
```

## Verificação da Instalação

Para verificar se as dependências foram instaladas corretamente:

```bash
# Listar pacotes instalados
vcpkg list

# Verificar informações de um pacote específico
vcpkg search atomic-queue
```

## Troubleshooting

### Erro: "Could not find a package configuration file"

1. Verifique se o vcpkg está integrado:
   ```bash
   vcpkg integrate install
   ```

2. Use o toolchain file explicitamente:
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[caminho-vcpkg]/scripts/buildsystems/vcpkg.cmake
   ```

### Erro: "Package not found"

1. Atualize o vcpkg:
   ```bash
   git pull
   .\bootstrap-vcpkg.bat  # Windows
   ```

2. Instale as dependências manualmente:
   ```bash
   vcpkg install atomic-queue parallel-hashmap thread-pool benchmark gtest
   ```

### Problemas de Triplet

Para especificar a arquitetura:

```bash
# Windows 64-bit
cmake -B build -S . -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake

# Linux 64-bit
cmake -B build -S . -DVCPKG_TARGET_TRIPLET=x64-linux -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
```

## Vantagens do vcpkg

1. **Gerenciamento automático**: As dependências são instaladas automaticamente
2. **Versionamento**: Controle preciso das versões das dependências
3. **Multiplataforma**: Funciona no Windows, Linux e macOS
4. **Integração CMake**: Integração nativa com CMake
5. **Reprodutibilidade**: Builds consistentes entre diferentes ambientes

## Migração do third_party

Se você estava usando o diretório `third_party` anteriormente:

1. O diretório `third_party` não é mais necessário
2. Os scripts `setup_dependencies.ps1` e `setup_dependencies.sh` podem ser removidos
3. O arquivo `DEPENDENCIES.md` pode ser atualizado para referenciar este arquivo

## Exemplo de Build Completo

```bash
# Clone o projeto
git clone <seu-repositorio>
cd lockfree-object-pool

# Configure com vcpkg
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake

# Compile
cmake --build build --config Release

# Execute os exemplos
.\build\bin\Release\simple_example.exe     # Windows
.\build\bin\Release\lockfree_benchmark.exe # Windows

# ou
./build/bin/simple_example     # Linux/macOS
./build/bin/lockfree_benchmark # Linux/macOS
```