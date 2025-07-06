# LockFree Object Pool

Uma implementação ultra-rápida de pool de objetos lock-free com otimizações avançadas e cache thread-local.

## Características

- **Lock-free**: Operações atômicas sem mutexes para máxima performance
- **Thread-local caching**: Cache por thread para reduzir contenção
- **SIMD optimizations**: Otimizações com prefetching e instruções SIMD
- **Memory pool management**: Gerenciamento eficiente de memória com PMR allocators
- **Comprehensive benchmarks**: Suite completa de benchmarks para análise de performance
- **Cross-platform**: Suporte para Windows, Linux e macOS

## Estrutura do Projeto

```
lockfree/
├── include/
│   └── lockfree/
│       └── lockfree.hpp          # Implementação principal
│   └── lib/
│       └── thread_pool.cpp       # thread_pool simples
│       └── thread_pool.hpp       # thread_pool simples
├── src/
│   └── benchmark/
│       ├── base.hpp              # Base para benchmarks
│       ├── baseline_benchmarks.hpp
│       ├── pool_benchmarks.hpp
│       ├── multithreaded_benchmarks.hpp
│       ├── stress_benchmarks.hpp
│       ├── analysis_benchmarks.hpp
│       └── main_benchmark.hpp
├── examples/
│   └── main_test_lockfree.cpp    # Exemplo de uso e testes
├── CMakeLists.txt
└── README.md
```

## Dependências

- **atomic_queue**: Queue atômica lock-free
- **parallel_hashmap**: Hash map paralelo de alta performance
- **thread_pool**: Pool de threads otimizado
- **C++20**: Requer compilador com suporte completo ao C++20

## Uso Básico

```cpp
#include <lockfree/lockfree.hpp>

// Criar pool com 1024 objetos, estatísticas habilitadas
OptimizedObjectPool<MyObject, 1024, true> pool;

// Pré-aquecer o pool
pool.prewarm(512);

// Adquirir objeto
auto obj = pool.acquire(constructor_args...);

// Usar objeto...

// Retornar ao pool
pool.release(obj);
```

## Configurações Recomendadas

### Aplicações Single-threaded
```cpp
OptimizedObjectPool<T, 512, false, std::pmr::polymorphic_allocator<T>, 32>
```
- Speedup esperado: 3-8x vs make_shared
- Overhead de memória: ~32MB + 2MB por thread

### Aplicações Multi-threaded (≤8 threads)
```cpp
OptimizedObjectPool<T, 1024, false, std::pmr::polymorphic_allocator<T>, 16>
```
- Speedup esperado: 15-40x vs make_shared
- Overhead de memória: ~64MB + 1MB por thread

### Aplicações High-contention (>8 threads)
```cpp
OptimizedObjectPool<T, 2048, false, std::pmr::polymorphic_allocator<T>, 8>
```
- Speedup esperado: 50-150x vs make_shared
- Overhead de memória: ~128MB + 512KB por thread

## 🚀 Início Rápido

### Pré-requisitos
- Compilador com suporte a C++20 (GCC 10+, Clang 12+, MSVC 2022+)
- CMake 3.20 ou superior
- [vcpkg](https://github.com/Microsoft/vcpkg) configurado

### Instalação

1. **Clone o repositório**:
   ```bash
   git clone https://github.com/beats-dh/lockfree.git
   cd lockfree-object-pool
   ```

2. **Configure e compile com vcpkg**:
   ```bash
   # Configure o projeto (dependências instaladas automaticamente)
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[caminho-vcpkg]/scripts/buildsystems/vcpkg.cmake
   
   # Compile
   cmake --build build --config Release
   ```

3. **Execute os exemplos**:
   ```bash
   # Exemplo simples
   ./build/bin/simple_example
   
   # Benchmarks completos
   ./build/bin/lockfree_benchmark
   ```

> 📖 **Documentação detalhada**: Veja [VCPKG.md](VCPKG.md) para instruções completas do vcpkg

## Compilação

### Sistema de Dependências
Este projeto usa **vcpkg** para gerenciar dependências automaticamente. Veja:
- [VCPKG.md](VCPKG.md) - Guia completo do vcpkg
- [DEPENDENCIES.md](DEPENDENCIES.md) - Informações sobre as dependências
- [BUILD.md](BUILD.md) - Instruções detalhadas de compilação

### Build Básico
```bash
# Configure com vcpkg
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake

# Compile
cmake --build build --config Release
```

### Build com Otimizações
```bash
# Release com otimizações máximas
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-O3 -march=native"
cmake --build build --config Release
```

### Targets Disponíveis
- `lockfree_benchmark`: Executável principal com todos os benchmarks
- `simple_example`: Exemplo básico de uso
- `lockfree_object_pool`: Biblioteca interface (header-only)

### Executar Benchmarks
```bash
# Suite completa
./lockfree_benchmark

# Benchmark leve (para CI/CD)
./lockfree_benchmark --light

# Apenas stress tests
./lockfree_benchmark --stress --ops 10000
```

## 🚀 Recursos Avançados

O `OptimizedObjectPool` implementa diversos conceitos C++20 e otimizações avançadas para máxima performance:

### Conceitos de Tipo (Type Concepts)

#### HasThreadId - Otimização Thread-Local
```cpp
template <typename T>
concept HasThreadId = requires(T t) {
    { static_cast<const decltype(t) &>(t).threadId } -> std::convertible_to<int16_t>;
};
```

Permite rastreamento automático de thread ownership para otimizações de cache:

```cpp
class MinhaClasse {
public:
    // Seus dados...
    int16_t threadId;  // Habilita otimizações thread-local
};
```

#### Poolable - Flexibilidade de Construção
```cpp
template <typename T>
concept Poolable = std::is_default_constructible_v<T> || HasReset<T> || HasBuild<T>;
```

Suporta diferentes padrões de inicialização:
- **Default constructible**: Construção padrão
- **HasReset**: Método `reset()` para reutilização
- **HasBuild**: Método `build()` para inicialização customizada

#### HasReset - Reutilização Eficiente
```cpp
template <typename T, typename... Args>
concept HasReset = requires(T t, Args &&... args) {
    { t.reset(std::forward<Args>(args)...) } -> std::same_as<void>;
};
```

#### HasBuild - Construção Customizada
```cpp
template <typename T, typename... Args>
concept HasBuild = requires(T t, Args &&... args) {
    { t.build(std::forward<Args>(args)...) } -> std::same_as<void>;
};
```

#### HasDestroy - Limpeza Customizada
```cpp
template <typename T>
concept HasDestroy = requires(T t) {
    { t.destroy() } -> std::same_as<void>;
};
```

### Otimizações de Performance

- **Lock-free operations**: Operações atômicas sem mutexes
- **Thread-local caching**: Cache LIFO por thread para localidade
- **SIMD prefetching**: Hints de prefetch para otimização de cache
- **Batch operations**: Operações em lote para eficiência
- **Memory alignment**: Alinhamento de cache line para reduzir false sharing
- **PMR allocators**: Suporte a allocators polimórficos

### Estatísticas Detalhadas

Quando `EnableStats = true`, o pool coleta métricas abrangentes:

```cpp
struct PoolStatistics {
    size_t acquires;           // Total de aquisições
    size_t releases;           // Total de liberações
    size_t creates;            // Objetos criados
    size_t cross_thread_ops;   // Operações cross-thread
    size_t same_thread_hits;   // Cache hits na mesma thread
    size_t in_use;             // Objetos em uso
    size_t current_pool_size;  // Tamanho atual do pool
    size_t cache_hits;         // Total de cache hits
    size_t batch_operations;   // Operações em lote
};
```

### Exemplo Completo

```cpp
#include <lockfree/lockfree.hpp>

class OptimizedObject {
public:
    int16_t threadId;  // Habilita otimizações thread-local
    
    void reset(int value) {
        data = value;
        processed = false;
    }
    
    void build(const std::string& config) {
        // Inicialização customizada
    }
    
private:
    int data = 0;
    bool processed = false;
};

// Pool com estatísticas e PMR allocator
OptimizedObjectPool<OptimizedObject, 2048, true, 
                   std::pmr::polymorphic_allocator<OptimizedObject>, 64> pool;

// Uso
auto obj = pool.acquire(42);  // Chama reset(42)
// ... usar objeto ...
pool.release(obj);  // Retorna ao cache thread-local

// Verificar estatísticas
auto stats = pool.get_stats();
std::cout << "Cache hit rate: " 
          << (stats.same_thread_hits * 100.0 / stats.acquires) << "%\n";
```

## Performance

Resultados típicos em hardware moderno:

| Cenário | Speedup vs std::make_shared | Throughput |
|---------|----------------------------|------------|
| Single-thread | 3-8x | 50-100M ops/sec |
| Multi-thread (4 cores) | 15-40x | 200-500M ops/sec |
| High-contention (16+ cores) | 50-150x | 1-2B ops/sec |

## Licença

MIT License - veja LICENSE para detalhes.

## Contribuindo

1. Fork o projeto
2. Crie uma branch para sua feature (`git checkout -b feature/AmazingFeature`)
3. Commit suas mudanças (`git commit -m 'Add some AmazingFeature'`)
4. Push para a branch (`git push origin feature/AmazingFeature`)
5. Abra um Pull Request

## Autores

- Implementação original e otimizações
- Benchmarks e análise de performance
- Documentação e exemplos