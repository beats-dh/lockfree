/**
 * LockFree Object Pool - A high-performance, thread-safe object pool implementation
 * Copyright (©) 2025 Daniel <daniel15042015@gmail.com>
 * Repository: https://github.com/beats-dh/lockfree
 * License: https://github.com/beats-dh/lockfree/blob/main/LICENSE
 * Contributors: https://github.com/beats-dh/lockfree/graphs/contributors
 * Website:
 */

#pragma once

#include "atomic_queue/atomic_queue.h"
#include "lib/thread_pool.hpp"
#include "parallel_hashmap/phmap.h"
#include <span>
#include <chrono>
#include <memory_resource>
#include <expected>
#include <version>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable : 4996) // Deprecated functions
	#pragma warning(disable : 4324) // Structure padded due to alignment specifier

	#ifndef LOCKFREE_GNU_ATTRIBUTES_DEFINED
		#define LOCKFREE_GNU_ATTRIBUTES_DEFINED
		#define GNUHOT
		#define GNUCOLD
		#define GNUALWAYSINLINE inline
		#define GNUFLATTEN
		#define GNUNOINLINE __declspec(noinline)
	#endif

	#if _MSC_VER >= 1929
		#define LOCKFREE_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
	#else
		#define LOCKFREE_NO_UNIQUE_ADDRESS
	#endif
#else
	#define GNUHOT [[gnu::hot]]
	#define GNUCOLD [[gnu::cold]]
	#define GNUALWAYSINLINE [[gnu::always_inline]] inline
	#define GNUFLATTEN [[gnu::flatten]]
	#define GNUNOINLINE [[gnu::noinline]]

	#if __has_cpp_attribute(no_unique_address) >= 201803L
		#define LOCKFREE_NO_UNIQUE_ADDRESS [[no_unique_address]]
	#else
		#define LOCKFREE_NO_UNIQUE_ADDRESS
	#endif
#endif

#ifdef __AVX2__
	#include <immintrin.h>
#endif

#ifdef __cpp_lib_hardware_interference_size
// Usa o tamanho da linha de cache de hardware definido pelo padrão C++17 para evitar "false sharing".
constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
// Fallback para o tamanho de linha de cache mais comum (64 bytes) se o padrão não estiver disponível.
constexpr size_t CACHE_LINE_SIZE = 64;
#endif

namespace lockfree_config {
	using namespace std::chrono_literals;
	constexpr size_t DEFAULT_POOL_SIZE = 1024;
	constexpr size_t DEFAULT_LOCAL_CACHE_SIZE = 32;
	constexpr size_t PREWARM_BATCH_SIZE = 32;
	constexpr size_t CLEANUP_BATCH_SIZE = 64;
}

// Conceitos para garantir que os tipos T tenham as interfaces necessárias.
template <typename T, typename... Args>
concept HasReset = requires(T t, Args &&... args) {
	{ t.reset(std::forward<Args>(args)...) } -> std::same_as<void>;
};

template <typename T, typename... Args>
concept HasBuild = requires(T t, Args &&... args) {
	{ t.build(std::forward<Args>(args)...) } -> std::same_as<void>;
};

template <typename T>
concept HasDestroy = requires(T t) {
	{ t.destroy() } -> std::same_as<void>;
};

template <typename T>
concept HasThreadId = requires(T t) {
	{ static_cast<const decltype(t) &>(t).threadId } -> std::convertible_to<int16_t>;
};

template <typename T>
concept Poolable = std::is_default_constructible_v<T> || HasReset<T> || HasBuild<T>;

// Enum para erros explícitos, usado com std::expected (C++23).
enum class PoolError {
	Shutdown,
	AllocationFailed
};

/**
 * @brief Pool de objetos lock-free ultrarrápida com limpeza otimizada e cache local por thread.
 * @details Esta classe fornece uma implementação de pool de objetos de alta performance e segura para threads,
 * usando operações atômicas lock-free e caches locais para minimizar a contenção.
 * Os objetos são reciclados eficientemente com construção otimizada para SIMD e ordenação
 * LIFO para localidade de cache ótima.
 *
 * @tparam T Tipo do objeto a ser armazenado na pool (deve satisfazer o conceito Poolable).
 * @tparam PoolSize Capacidade máxima da pool global (deve ser uma potência de dois para melhor performance).
 * @tparam EnableStats Habilita a coleta de estatísticas para monitoramento de performance.
 * @tparam Allocator Tipo do alocador (suporta alocadores PMR).
 * @tparam LocalCacheSize Máximo de objetos no cache por thread para reduzir contenção.
 */
template <
	typename T,
	size_t PoolSize = lockfree_config::DEFAULT_POOL_SIZE,
	bool EnableStats = false,
	typename Allocator = std::pmr::polymorphic_allocator<T>,
	size_t LocalCacheSize = lockfree_config::DEFAULT_LOCAL_CACHE_SIZE>
class OptimizedObjectPool {
public:
	using pointer = T*;
	using PoolResult = std::expected<pointer, PoolError>;

	struct alignas(CACHE_LINE_SIZE) PoolStatistics {
		size_t acquires = 0, releases = 0, creates = 0, cross_thread_ops = 0,
			   same_thread_hits = 0, in_use = 0, current_pool_size = 0,
			   cache_hits = 0, batch_operations = 0;
	};

	/**
	 * @brief Construtor padrão que utiliza o recurso de memória padrão.
	 */
	OptimizedObjectPool() :
		OptimizedObjectPool(Allocator(std::pmr::get_default_resource())) { }

	/**
	 * @brief Constrói a pool com um alocador customizado.
	 * @param alloc Instância do alocador customizado para alocação de objetos.
	 * @details Inicializa a pool, a registra para limpeza segura e opcionalmente
	 * pré-aquece a pool com objetos para melhor performance inicial.
	 */
	explicit OptimizedObjectPool(const Allocator &alloc) :
		m_allocator(alloc), m_shutdown_flag(false), m_queue() {
		get_active_instances().emplace(this, std::chrono::steady_clock::now());
		if constexpr (std::is_default_constructible_v<T>) {
			prewarm(PoolSize / 2uz);
		}
	}

	/**
	 * @brief Destrutor com limpeza segura multi-threaded.
	 * @details Garante a ordem correta de desligamento: sinaliza o desligamento, aguarda por operações
	 * em andamento, limpa os caches das threads, remove a instância da lista de ativas e
	 * limpa a fila global. Esta ordem previne "race conditions" durante a destruição da pool.
	 */
	~OptimizedObjectPool() {
		m_shutdown_flag.store(true, std::memory_order_release);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		cleanup_all_caches();
		get_active_instances().erase(this);
		cleanup_global_queue();
	}

	/**
	 * @brief Retorna um objeto de forma segura para a pool global, com verificação de desligamento.
	 * @param obj Ponteiro para o objeto a ser retornado.
	 * @return true se o objeto foi retornado com sucesso, false caso contrário.
	 * @details Tenta inserir o objeto na fila atômica global. A lógica de recuperação de race condition
	 * foi removida para simplificação, pois o destrutor principal já garante a limpeza.
	 */
	bool safe_return_to_global(pointer obj) noexcept {
		if (m_shutdown_flag.load(std::memory_order_acquire)) [[unlikely]] {
			return false;
		}
		return m_queue.try_push(obj);
	}

	/**
	 * @brief Destrói e desaloca um objeto de forma segura, com tratamento de exceções.
	 * @param obj Ponteiro para o objeto a ser destruído e desalocado.
	 * @details Executa a sequência de destruição correta: chama o destrutor (se não for trivial)
	 * e depois desaloca a memória. Todas as exceções são capturadas e ignoradas para
	 * garantir uma limpeza segura em destrutores e caminhos de erro.
	 */
	void safe_destroy_and_deallocate(pointer obj) noexcept {
		if (!obj) [[unlikely]] {
			return;
		}
		try {
			if constexpr (!std::is_trivially_destructible_v<T>) {
				std::destroy_at(obj);
			}
			m_allocator.deallocate(obj, 1);
		} catch (...) { }
	}

	/**
	 * @brief Retorna múltiplos objetos para a pool global de forma eficiente.
	 * @param objects Um `std::span` de ponteiros de objetos a serem retornados.
	 * @details Operação em lote otimizada que tenta retornar todos os objetos para a pool global.
	 * Se a pool estiver em processo de desligamento, destrói todos os objetos.
	 * Atualiza as estatísticas de operações em lote quando habilitado.
	 */
	void batch_return_to_global(std::span<pointer> objects) noexcept {
		if (m_shutdown_flag.load(std::memory_order_acquire)) [[unlikely]] {
			for (auto obj : objects) {
				safe_destroy_and_deallocate(obj);
			}
			return;
		}
		if constexpr (EnableStats) {
			m_stats.batch_operations.fetch_add(1, std::memory_order_relaxed);
		}
		for (auto obj : objects) {
			if (!safe_return_to_global(obj)) {
				safe_destroy_and_deallocate(obj);
			}
		}
	}

	/**
	 * @brief Adquire um objeto da pool, com argumentos opcionais para o construtor.
	 * @tparam Args Tipos dos argumentos do construtor.
	 * @param args Argumentos a serem encaminhados para o construtor do objeto ou método `reset`.
	 * @return Um `std::expected` contendo um ponteiro para o objeto adquirido, ou um `PoolError` em caso de falha.
	 * @details Aquisição de alta performance com cache de múltiplos níveis: verifica primeiro o cache local
	 * da thread (LIFO para localidade de cache), depois a fila atômica global, e por último
	 * cria um novo objeto. Inclui "prefetching hints" e construção otimizada com SIMD.
	 */
	template <typename... Args>
	[[nodiscard]] GNUHOT PoolResult acquire(Args &&... args) {
		if (m_shutdown_flag.load(std::memory_order_acquire)) [[unlikely]] {
			return std::unexpected(PoolError::Shutdown);
		}

		if constexpr (EnableStats) {
			m_stats.acquires.fetch_add(1, std::memory_order_relaxed);
			m_stats.in_use.fetch_add(1, std::memory_order_relaxed);
		}

		auto &cache = local_cache();
		if (cache.size > 0uz) [[likely]] {
			if (cache.is_valid()) [[likely]] {
				pointer obj = cache.data[--cache.size];
				if constexpr (EnableStats) {
					m_stats.same_thread_hits.fetch_add(1, std::memory_order_relaxed);
					m_stats.cache_hits.fetch_add(1, std::memory_order_relaxed);
				}
				construct_or_reset(obj, std::forward<Args>(args)...);
				return obj;
			}
		}

		pointer obj = nullptr;
		if (m_queue.try_pop(obj)) [[likely]] {
			if constexpr (EnableStats) {
				m_stats.cross_thread_ops.fetch_add(1, std::memory_order_relaxed);
			}
			construct_or_reset(obj, std::forward<Args>(args)...);
			return obj;
		}
		return create_new(std::forward<Args>(args)...);
	}

	/**
	 * @brief Libera um objeto de volta para a pool com otimização de afinidade de thread.
	 * @param obj Ponteiro para o objeto sendo liberado.
	 * @details Caminho de liberação otimizado: limpa o objeto, prefere o cache local da thread para
	 * liberações na mesma thread, e recorre à pool global para operações entre threads.
	 * Rastreia estatísticas entre threads com precisão e lida com o overflow do cache.
	 */
	GNUHOT void release(pointer obj) noexcept {
		if (!obj) [[unlikely]] {
			return;
		}

		if constexpr (EnableStats) {
			this->m_stats.releases.fetch_add(1, std::memory_order_relaxed);
			this->m_stats.in_use.fetch_sub(1, std::memory_order_relaxed);
		}

		bool same_thread = true;
		if constexpr (HasThreadId<T>) {
			same_thread = obj->threadId == ThreadPool::getThreadId();
		}

		this->cleanup_object_optimized(obj);

		if (same_thread) [[likely]] {
			if (!this->m_shutdown_flag.load(std::memory_order_relaxed)) [[likely]] {
				auto &cache = this->local_cache();
				if (cache.is_valid() && cache.size < LocalCacheSize) [[likely]] {
					cache.data[cache.size++] = obj;
					return;
				}
			}
		}

		if (!this->safe_return_to_global(obj)) {
			this->safe_destroy_and_deallocate(obj);
		}
		if constexpr (EnableStats) {
			if (!same_thread) {
				this->m_stats.cross_thread_ops.fetch_add(1, std::memory_order_relaxed);
			}
		}
	}

	/**
	 * @brief Pré-popula a pool com objetos prontos para uso.
	 * @param count Número de objetos a serem criados e adicionados à pool.
	 * @details Cria objetos em lote e os adiciona à pool global para melhorar a performance inicial.
	 * Usa um tamanho de lote configurável para eficiência de memória e para na primeira
	 * falha de alocação ou se a pool estiver cheia.
	 */
	void prewarm(size_t count) {
		if (m_shutdown_flag.load(std::memory_order_acquire)) {
			return;
		}

		count = std::min(count, PoolSize - m_queue.was_size());
		if (count == 0uz) {
			return;
		}

		std::array<pointer, lockfree_config::PREWARM_BATCH_SIZE> batch;
		while (count > 0uz) {
			const size_t n = std::min(count, batch.size());
			size_t allocated = 0;
			for (size_t i = 0; i < n; ++i) {
				if ((batch[i] = allocate_and_construct())) {
					++allocated;
				} else {
					break;
				}
			}
			if (allocated == 0uz) {
				return;
			}
			for (size_t i = 0; i < allocated; ++i) {
				if (!m_queue.try_push(batch[i])) {
					for (size_t j = i; j < allocated; ++j) {
						safe_destroy_and_deallocate(batch[j]);
					}
					return;
				}
			}
			count -= allocated;
		}
	}

	/**
	 * @brief Descarrega o cache local da thread atual para a pool global.
	 * @details Força o retorno de todos os objetos em cache para a pool global usando uma operação em lote.
	 * Útil para balanceamento de carga ou antes do término de uma thread para prevenir a
	 * perda de objetos e melhorar a disponibilidade de objetos entre threads.
	 */
	void flush_local_cache() noexcept {
		auto &cache = local_cache();
		if (cache.size > 0uz) {
			batch_return_to_global(std::span(cache.data).first(cache.size));
			cache.size = 0;
		}
	}

	/**
	 * @brief Reduz o tamanho da pool destruindo objetos em excesso.
	 * @param max Número máximo de objetos a serem destruídos.
	 * @return O número de objetos que foram realmente destruídos.
	 * @details Encolhe a pool removendo objetos da fila global em lotes. Primeiro descarrega o cache local,
	 * depois destrói objetos da pool global até o limite especificado. Útil em situações de
	 * pressão de memória.
	 */
	[[nodiscard]] size_t shrink(size_t max = PoolSize) {
		flush_local_cache();
		size_t released = 0;
		constexpr size_t BATCH_SIZE = 16;
		std::array<pointer, BATCH_SIZE> batch;
		while (released < max) {
			size_t batch_count = 0;
			const size_t target = std::min(max - released, BATCH_SIZE);
			for (size_t i = 0; i < target && m_queue.try_pop(batch[i]); ++i) {
				++batch_count;
			}
			if (batch_count == 0uz) {
				break;
			}
			for (auto obj : std::span(batch).first(batch_count)) {
				safe_destroy_and_deallocate(obj);
			}
			released += batch_count;
		}
		return released;
	}

	/**
	 * @brief Obtém as estatísticas de performance atuais da pool.
	 * @return Uma struct `PoolStatistics` com os contadores atuais.
	 * @details Retorna um snapshot atômico das métricas de performance da pool, incluindo contagens de
	 * aquisição/liberação, taxas de acerto de cache e operações entre threads. As estatísticas
	 * só são coletadas quando o parâmetro de template `EnableStats` é verdadeiro.
	 */
	[[nodiscard]] PoolStatistics get_stats() const {
		PoolStatistics stats {};
		if constexpr (EnableStats) {
			stats.acquires = this->m_stats.acquires.load(std::memory_order_relaxed);
			stats.releases = this->m_stats.releases.load(std::memory_order_relaxed);
			stats.creates = this->m_stats.creates.load(std::memory_order_relaxed);
			stats.cross_thread_ops = this->m_stats.cross_thread_ops.load(std::memory_order_relaxed);
			stats.same_thread_hits = this->m_stats.same_thread_hits.load(std::memory_order_relaxed);
			stats.in_use = this->m_stats.in_use.load(std::memory_order_relaxed);
			stats.current_pool_size = this->m_queue.was_size();
			stats.cache_hits = this->m_stats.cache_hits.load(std::memory_order_relaxed);
			stats.batch_operations = this->m_stats.batch_operations.load(std::memory_order_relaxed);
		}
		return stats;
	}

	/**
	 * @brief Obtém a capacidade da pool em tempo de compilação.
	 * @return O tamanho máximo da pool especificado em tempo de compilação.
	 */
	[[nodiscard]] static constexpr size_t capacity() noexcept {
		return PoolSize;
	}

private:
	LOCKFREE_NO_UNIQUE_ADDRESS Allocator m_allocator;
	std::atomic<bool> m_shutdown_flag;

	struct alignas(CACHE_LINE_SIZE) StatsBlock {
		std::atomic<size_t> acquires { 0 }, releases { 0 }, creates { 0 }, cross_thread_ops { 0 },
			same_thread_hits { 0 }, in_use { 0 }, cache_hits { 0 }, batch_operations { 0 };
	};
	LOCKFREE_NO_UNIQUE_ADDRESS std::conditional_t<EnableStats, StatsBlock, std::monostate> m_stats;

	alignas(CACHE_LINE_SIZE) atomic_queue::AtomicQueue<pointer, PoolSize> m_queue;

	/**
	 * @brief Obtém uma referência para o mapa de instâncias ativas com ordem de inicialização garantida.
	 * @return Uma referência para um `parallel_flat_hash_map_m` seguro para threads.
	 * @details Usa o padrão singleton de Meyer para garantir que o mapa seja inicializado sob demanda,
	 * evitando o "static initialization order fiasco". O mapa de hash é seguro para threads
	 * com mutexes internos e otimizado para acesso concorrente.
	 */
	static auto &get_active_instances() {
		static phmap::parallel_flat_hash_map_m<OptimizedObjectPool*, std::chrono::steady_clock::time_point> instances;
		return instances;
	}

	struct alignas(CACHE_LINE_SIZE) ThreadCache {
		size_t size = 0;
		std::atomic<bool> valid { true };
		alignas(CACHE_LINE_SIZE) pointer data[LocalCacheSize];
		bool is_valid() const noexcept {
			return valid.load(std::memory_order_acquire);
		}
		void invalidate() noexcept {
			valid.store(false, std::memory_order_release);
		}
		~ThreadCache() noexcept;
	};

	static thread_local ThreadCache thread_cache;
	ThreadCache &local_cache() {
		return thread_cache;
	}
	static void cleanup_all_caches() {
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	/**
	 * @brief Limpa os objetos restantes da fila global durante a destruição.
	 * @details Processa a fila atômica global em lotes para destruir todos os objetos restantes
	 * de forma segura durante a destruição da pool. Usa um tamanho de lote configurável
	 * para eficiência de memória.
	 */
	void cleanup_global_queue() noexcept {
		constexpr size_t BATCH_SIZE = lockfree_config::CLEANUP_BATCH_SIZE;
		std::array<pointer, BATCH_SIZE> batch;
		while (true) {
			size_t batch_count = 0;
			for (size_t i = 0; i < BATCH_SIZE && m_queue.try_pop(batch[i]); ++i) {
				++batch_count;
			}
			if (batch_count == 0uz) {
				break;
			}
			for (auto obj : std::span(batch).first(batch_count)) {
				safe_destroy_and_deallocate(obj);
			}
		}
	}

	/**
	 * @brief Cria um novo objeto com inicialização adequada e rastreamento de thread.
	 * @tparam Args Tipos dos argumentos do construtor.
	 * @param args Argumentos a serem encaminhados para o construtor do objeto.
	 * @return Um `std::expected` contendo um ponteiro para o novo objeto, ou `PoolError` em caso de falha.
	 * @details Aloca memória, constrói o objeto com os argumentos fornecidos e define o ID da thread,
	 * se suportado. Atualiza as estatísticas de criação quando habilitado.
	 * Fornece segurança de exceção adequada com limpeza em caso de falha.
	 */
	template <typename... Args>
	GNUCOLD GNUNOINLINE PoolResult create_new(Args &&... args) {
		if constexpr (EnableStats) {
			m_stats.creates.fetch_add(1, std::memory_order_relaxed);
		}
		pointer obj = nullptr;
		try {
			obj = m_allocator.allocate(1);
			if constexpr (HasBuild<T, Args...>) {
				std::construct_at(obj);
				obj->build(std::forward<Args>(args)...);
			} else {
				std::construct_at(obj, std::forward<Args>(args)...);
			}
			if constexpr (HasThreadId<T>) {
				obj->threadId = ThreadPool::getThreadId();
			}
			return obj;
		} catch (const std::bad_alloc &) {
			if (obj) {
				m_allocator.deallocate(obj, 1);
			}
			if constexpr (EnableStats) {
				m_stats.in_use.fetch_sub(1, std::memory_order_relaxed);
			}
			return std::unexpected(PoolError::AllocationFailed);
		} catch (...) {
			if (obj) {
				m_allocator.deallocate(obj, 1);
			}
			if constexpr (EnableStats) {
				m_stats.in_use.fetch_sub(1, std::memory_order_relaxed);
			}
			throw;
		}
	}

	/**
	 * @brief Aloca e constrói um objeto padrão para o pré-aquecimento da pool.
	 * @return Ponteiro para o objeto alocado, ou nullptr em caso de falha.
	 * @details Alocação simplificada para pré-aquecimento que apenas constrói objetos com o construtor padrão.
	 * Usa operações `nothrow` quando possível e define o ID da thread. Retorna nullptr em qualquer
	 * falha, em vez de lançar exceções.
	 */
	GNUALWAYSINLINE pointer allocate_and_construct() noexcept {
		try {
			pointer obj = m_allocator.allocate(1);
			if (!obj) {
				return nullptr;
			}
			std::construct_at(obj);
			if constexpr (HasThreadId<T>) {
				obj->threadId = ThreadPool::getThreadId();
			}
			return obj;
		} catch (...) {
			return nullptr;
		}
	}

	/**
	 * @brief Limpa/reseta um objeto para reutilização com segurança de exceção.
	 * @param obj Ponteiro para o objeto a ser limpo.
	 * @details Tenta resetar o estado do objeto usando o método `reset()` se disponível,
	 * caso contrário, usa o método `destroy()`. Otimizado para previsão de desvio
	 * do caso comum com `reset()` como caminho principal.
	 */
	GNUHOT GNUALWAYSINLINE void cleanup_object_optimized(pointer obj) noexcept {
		if (!obj) [[unlikely]] {
			return;
		}
		if constexpr (HasReset<T>) {
			try {
				obj->reset();
			} catch (...) { }
		} else if constexpr (HasDestroy<T>) {
			try {
				obj->destroy();
			} catch (...) { }
		}
	}

	/**
	 * @brief Constrói ou reseta um objeto de forma otimizada com otimizações SIMD.
	 * @tparam Args Tipos dos argumentos do construtor.
	 * @param obj Ponteiro para o objeto a ser inicializado.
	 * @param args Argumentos para construção/reset.
	 * @details Inicialização de objetos com múltiplas estratégias: usa `reset()` para eficiência da pool,
	 * `build()` para inicialização complexa, cópia otimizada com SIMD para tipos triviais,
	 * e reconstrução completa para tipos complexos.
	 */
	template <typename... Args>
	GNUHOT GNUALWAYSINLINE void construct_or_reset(pointer obj, Args &&... args) noexcept {
		if (!obj) [[unlikely]] {
			return;
		}
		if constexpr (HasReset<T, Args...>) {
			try {
				obj->reset(std::forward<Args>(args)...);
			} catch (...) { }
		} else if constexpr (HasBuild<T, Args...>) {
			try {
				obj->build(std::forward<Args>(args)...);
			} catch (...) { }
		} else if constexpr (!std::is_trivially_destructible_v<T> || sizeof...(Args) > 0) {
			try {
				if constexpr (!std::is_trivially_destructible_v<T>) {
					std::destroy_at(obj);
				}
				std::construct_at(obj, std::forward<Args>(args)...);
			} catch (...) { }
		}
	}
};

/**
 * @brief Definição da variável de template estática `thread_local`.
 * @details Esta linha garante que cada thread tenha sua própria instância de `ThreadCache`.
 * É o mecanismo central que permite o cache local por thread, eliminando a contenção
 * na maioria das operações de `acquire` e `release`.
 */
template <typename T, size_t P, bool E, typename A, size_t L>
thread_local typename OptimizedObjectPool<T, P, E, A, L>::ThreadCache OptimizedObjectPool<T, P, E, A, L>::thread_cache;

/**
 * @brief Implementação do destrutor de `ThreadCache`.
 * @details Este destrutor é uma rede de segurança crucial que é chamada automaticamente quando uma thread termina.
 * Ele garante que nenhum objeto seja vazado. Para cada objeto restante no cache da thread, ele tenta
 * retorná-lo a uma das pools ativas no programa.
 * O fallback para `delete` foi removido para evitar comportamento indefinido com alocadores PMR.
 * Em um cenário de desligamento raro, é preferível vazar o objeto a corromper a memória.
 */
template <typename T, size_t P, bool E, typename A, size_t L>
OptimizedObjectPool<T, P, E, A, L>::ThreadCache::~ThreadCache() noexcept {
	invalidate();
	if (size == 0) {
		return;
	}
	auto &instances = OptimizedObjectPool<T, P, E, A, L>::get_active_instances();
	for (auto obj : std::span(data).first(size)) {
		if (!obj) {
			continue;
		}
		bool returned = false;
		for (const auto &[pool, timestamp] : instances) {
			if (pool && !pool->m_shutdown_flag.load(std::memory_order_acquire)) {
				if (pool->safe_return_to_global(obj)) {
					returned = true;
					break;
				}
			}
		}
		// O bloco 'if (!returned)' foi removido para evitar a chamada incorreta de 'delete'.
	}
}

/**
 * @brief Wrapper de `std::shared_ptr` para a pool de objetos com gerenciamento automático RAII.
 * @details Fornece uma interface de `std::shared_ptr` enquanto utiliza a pool de objetos para alocação.
 * Os objetos são retornados automaticamente para a pool quando a contagem de referências do
 * `shared_ptr` chega a zero. Combina a performance da pool com a semântica familiar do `shared_ptr`.
 *
 * @tparam T Tipo do objeto a ser armazenado na pool.
 * @tparam PoolSize Capacidade máxima da pool subjacente.
 * @tparam EnableStats Habilita a coleta de estatísticas.
 * @tparam Allocator Tipo do alocador para a pool.
 * @tparam LocalCacheSize Tamanho do cache local por thread.
 */
template <
	typename T,
	size_t PoolSize = lockfree_config::DEFAULT_POOL_SIZE,
	bool EnableStats = false,
	typename Allocator = std::pmr::polymorphic_allocator<T>,
	size_t LocalCacheSize = lockfree_config::DEFAULT_LOCAL_CACHE_SIZE>
class SharedOptimizedObjectPool {
public:
	using PoolResult = std::expected<std::shared_ptr<T>, PoolError>;

	/**
	 * @brief Construtor padrão que utiliza o recurso de memória padrão.
	 */
	SharedOptimizedObjectPool() :
		m_pool(Allocator(std::pmr::get_default_resource())) { }

	/**
	 * @brief Constrói com um alocador customizado.
	 * @param allocator Instância do alocador customizado a ser usado pela pool subjacente.
	 */
	explicit SharedOptimizedObjectPool(const Allocator &allocator) :
		m_pool(allocator) { }

	/**
	 * @brief Adquire um objeto encapsulado em um `shared_ptr` com um deleter customizado.
	 * @tparam Args Tipos dos argumentos do construtor.
	 * @param args Argumentos a serem encaminhados para o construtor do objeto.
	 * @return Um `std::expected` contendo um `shared_ptr` que gerencia o objeto da pool, ou `PoolError` em caso de falha.
	 * @details Cria um `shared_ptr` com um deleter customizado que retorna o objeto para a pool
	 * quando a contagem de referências chega a zero. Fornece semântica RAII mantendo
	 * os benefícios de performance da pool.
	 */
	template <typename... Args>
	[[nodiscard]] PoolResult acquire(Args &&... args) {
		auto result = m_pool.acquire(std::forward<Args>(args)...);
		if (!result) [[unlikely]] {
			return std::unexpected(result.error());
		}
		return std::shared_ptr<T>(result.value(), [this](T* ptr) noexcept {
			if (ptr) {
				m_pool.release(ptr);
			}
		});
	}

	/**
	 * @brief Pré-popula a pool subjacente com objetos.
	 * @param count Número de objetos a serem criados e adicionados à pool.
	 */
	void prewarm(size_t count) {
		m_pool.prewarm(count);
	}

	/**
	 * @brief Descarrega o cache da thread atual para a pool global.
	 */
	void flush_local_cache() {
		m_pool.flush_local_cache();
	}

	/**
	 * @brief Encolhe a pool destruindo objetos em excesso.
	 * @param max Número máximo de objetos a serem destruídos.
	 * @return O número de objetos que foram realmente destruídos.
	 */
	[[nodiscard]] size_t shrink(size_t max = PoolSize) {
		return m_pool.shrink(max);
	}

	/**
	 * @brief Obtém as estatísticas de performance da pool subjacente.
	 * @return `PoolStatistics` com as métricas de performance atuais.
	 */
	[[nodiscard]] auto get_stats() const {
		return m_pool.get_stats();
	}

	/**
	 * @brief Obtém a capacidade da pool em tempo de compilação.
	 * @return O tamanho máximo da pool.
	 */
	[[nodiscard]] static constexpr size_t capacity() noexcept {
		return PoolSize;
	}

private:
	OptimizedObjectPool<T, PoolSize, EnableStats, Allocator, LocalCacheSize> m_pool;
};
