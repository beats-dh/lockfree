# Dependências do LockFree Object Pool

Este projeto utiliza o **vcpkg** para gerenciar dependências automaticamente através do sistema de manifesto.

## 🚀 Configuração Rápida com vcpkg

Para usar este projeto, você precisa apenas do vcpkg configurado. Veja o arquivo [VCPKG.md](VCPKG.md) para instruções detalhadas.

```bash
# Configure o projeto com vcpkg
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[caminho-vcpkg]/scripts/buildsystems/vcpkg.cmake

# Compile (as dependências são instaladas automaticamente)
cmake --build build --config Release
```

## Dependências Principais

### 1. atomic_queue
**Repositório**: https://github.com/max0x7ba/atomic_queue  
**Versão**: ≥1.5.0 (gerenciada pelo vcpkg)  
**Descrição**: Implementação de filas lock-free de alta performance

### 2. parallel_hashmap
**Repositório**: https://github.com/greg7mdp/parallel-hashmap  
**Versão**: ≥1.3.12 (gerenciada pelo vcpkg)  
**Descrição**: Hash maps paralelos com excelente performance

### 3. Thread Pool
**Repositório**: https://github.com/bshoshany/thread-pool  
**Versão**: ≥4.1.0 (gerenciada pelo vcpkg)  
**Descrição**: Pool de threads moderno e eficiente para C++17/20

### 4. Google Benchmark
**Repositório**: https://github.com/google/benchmark  
**Versão**: ≥1.8.0 (gerenciada pelo vcpkg)  
**Descrição**: Framework para benchmarks de performance

### 5. Google Test
**Repositório**: https://github.com/google/googletest  
**Versão**: ≥1.14.0 (gerenciada pelo vcpkg)  
**Descrição**: Framework de testes unitários

## Migração do Sistema Anterior

Se você estava usando o sistema anterior com `third_party/`:

1. **Remova o diretório third_party** (não é mais necessário):
   ```bash
   rm -rf third_party/  # Linux/macOS
   rmdir /s third_party  # Windows
   ```

2. **Configure o vcpkg** seguindo as instruções em [VCPKG.md](VCPKG.md)

3. **Recompile o projeto**:
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```

## Vantagens do vcpkg

- ✅ **Instalação automática**: Dependências instaladas automaticamente durante o build
- ✅ **Versionamento preciso**: Controle exato das versões das dependências
- ✅ **Multiplataforma**: Funciona no Windows, Linux e macOS
- ✅ **Integração CMake**: Integração nativa sem configuração adicional
- ✅ **Reprodutibilidade**: Builds consistentes entre diferentes ambientes
- ✅ **Atualizações fáceis**: Atualize o `vcpkg.json` e recompile

## Verificação da Configuração

Para verificar se tudo está funcionando:

```bash
# Configure e compile
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# Execute os exemplos
./build/bin/simple_example      # Linux/macOS
./build/bin/lockfree_benchmark  # Linux/macOS

# Windows
.\build\bin\Release\simple_example.exe
.\build\bin\Release\lockfree_benchmark.exe
```

## Solução de Problemas

Veja o arquivo [VCPKG.md](VCPKG.md) para troubleshooting detalhado do vcpkg.

### Problemas Comuns

1. **vcpkg não encontrado**: Configure a variável `CMAKE_TOOLCHAIN_FILE`
2. **Dependências não instaladas**: Execute `vcpkg install` manualmente
3. **Erro de triplet**: Especifique a arquitetura correta (x64-windows, x64-linux, etc.)

## Informações Técnicas

Todas as dependências são header-only ou compiladas automaticamente pelo vcpkg:

- **atomic_queue**: Headers em `<atomic_queue/atomic_queue.h>`
- **parallel_hashmap**: Headers em `<parallel_hashmap/phmap.h>`
- **thread_pool**: Headers em `<BS_thread_pool.hpp>`
- **benchmark**: Biblioteca linkada automaticamente
- **gtest**: Biblioteca linkada automaticamente

## Versões e Compatibilidade

O vcpkg garante compatibilidade entre as versões das dependências:

- **atomic_queue**: ≥1.5.0
- **parallel_hashmap**: ≥1.3.12
- **thread_pool**: ≥4.1.0
- **benchmark**: ≥1.8.0
- **gtest**: ≥1.14.0
- **CMake**: 3.20+
- **C++ Standard**: C++20

## Atualizações

Para atualizar as dependências:

1. **Atualize o vcpkg**:
   ```bash
   cd [caminho-vcpkg]
   git pull
   .\bootstrap-vcpkg.bat  # Windows
   ./bootstrap-vcpkg.sh   # Linux/macOS
   ```

2. **Atualize as versões no vcpkg.json** (se necessário)

3. **Recompile o projeto**:
   ```bash
   cmake --build build --config Release
   ```

## Notas Importantes

1. **Gerenciamento automático**: vcpkg cuida de todas as dependências
2. **Compatibilidade**: Versões testadas e compatíveis entre si
3. **Performance**: As bibliotecas são otimizadas para alta performance
4. **Padrão C++**: Requer C++20 para funcionalidades modernas
5. **Threading**: Suporte nativo para programação multi-threaded
6. **Memory Management**: Implementações lock-free para máxima eficiência
7. **Multiplataforma**: Funciona em Windows, Linux e macOS

## Licenças das Dependências

- **atomic_queue**: MIT License
- **parallel_hashmap**: Apache License 2.0
- **thread_pool**: MIT License

Todas as dependências são compatíveis com a licença MIT deste projeto.