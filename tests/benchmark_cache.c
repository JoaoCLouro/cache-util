#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "../include/cache.h"

#define ITERATIONS 500000
#define BATCH_SIZE 16
#define ADDRESS_POOL_SIZE 256

typedef struct CacheStats {
    uint64_t total_accesses;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t invalid_passkeys;
    uint64_t other_errors;
} CacheStats;

int main(void) {
    printf("=== Starting Heavy Stress-Test Cache Benchmark (%d Iterations, Batch Size: %d) ===\n", ITERATIONS, BATCH_SIZE);

    uint64_t passkey = init_t();
    Definition* def = m_init_t(passkey);
    if (!def) {
        fprintf(stderr, "Failed to initialize cache definition.\n");
        return 1;
    }

    // Configure higher thread count and buffer capacity to stress concurrency and alignment checks
    set_thread_count(def, 8);
    set_max_wait_size(def, 64);

    CacheStats stats = {0};
    
    // Generate a sparse, realistic 64-bit address space pool to trigger conflict and capacity misses
    uint64_t address_pool[ADDRESS_POOL_SIZE];
    for (int i = 0; i < ADDRESS_POOL_SIZE; i++) {
        // High-range 64-bit virtual memory addresses with stride patterns
        address_pool[i] = 0x7FFF00000000ULL + ((uint64_t)i * 0x40ULL);
    }

    size_t byte_counts[BATCH_SIZE];
    uint64_t return_bufs[BATCH_SIZE];
    const void* return_ptrs[BATCH_SIZE];
    
    for (int b = 0; b < BATCH_SIZE; b++) {
        byte_counts[b] = 8;
        return_ptrs[b] = &return_bufs[b];
    }

    // Pre-populate a portion of the pool to simulate working-set residency
    for (int i = 0; i < ADDRESS_POOL_SIZE; i += BATCH_SIZE) {
        uint64_t priming_batch[BATCH_SIZE];
        for (int b = 0; b < BATCH_SIZE; b++) {
            priming_batch[b] = address_pool[i + b];
        }
        multi_write_cache_t(def, priming_batch, BATCH_SIZE);
        flush(def);
    }

    clock_t start_time = clock();

    for (int i = 0; i < ITERATIONS; i++) {
        // Pseudo-random stride across the 64-bit pool to maximize cache churn
        int pool_idx = (i * 7) % (ADDRESS_POOL_SIZE - BATCH_SIZE);
        
        uint64_t batch_addrs[BATCH_SIZE];
        for (int b = 0; b < BATCH_SIZE; b++) {
            batch_addrs[b] = address_pool[pool_idx + b];
        }

        // Mix in writes dynamically to test concurrency between read/write workers
        if (i % 3 == 0) {
            multi_write_cache_t(def, batch_addrs, BATCH_SIZE);
        }

        CacheResult res = multi_read_cache_t(def, batch_addrs, BATCH_SIZE, byte_counts, return_ptrs);
        flush(def);

        stats.total_accesses += BATCH_SIZE;

        if (res.is_ok) {
            stats.cache_hits += res.value.ok.operations_completed;
        } else {
            switch (res.value.err.code) {
                case CACHE_MISS:
                    stats.cache_misses += BATCH_SIZE; // Count remaining failed batch entries as misses
                    break;
                case INVALID_PASSKEY:
                    stats.invalid_passkeys++;
                    clean(def);
                    free(def);
                    passkey = init_t();
                    def = m_init_t(passkey);
                    if (!def) {
                        fprintf(stderr, "Failed to reinitialize cache definition.\n");
                        return 1;
                    }
                    set_thread_count(def, 8);
                    set_max_wait_size(def, 64);
                    break;
                default:
                    stats.other_errors++;
                    break;
            }
        }
    }

    clock_t end_time = clock();
    double elapsed_seconds = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    double miss_pct = stats.total_accesses > 0 ? ((double)stats.cache_misses / stats.total_accesses) * 100.0 : 0.0;
    double hit_pct  = stats.total_accesses > 0 ? ((double)stats.cache_hits / stats.total_accesses) * 100.0 : 0.0;

    printf("\n=== Heavy Stress-Test Results ===\n");
    printf("Total Iterations      : %d\n", ITERATIONS);
    printf("Batch Size            : %d\n", BATCH_SIZE);
    printf("Total Accesses        : %lu\n", stats.total_accesses);
    printf("Cache Hits            : %lu (%.2f%%)\n", stats.cache_hits, hit_pct);
    printf("Cache Misses          : %lu (%.2f%%)\n", stats.cache_misses, miss_pct);
    printf("Invalid Passkeys      : %lu\n", stats.invalid_passkeys);
    printf("Other System Errors   : %lu\n", stats.other_errors);
    printf("Execution Time        : %.4f seconds\n", elapsed_seconds);
    printf("Throughput            : %.2f accesses/sec\n", (double)stats.total_accesses / elapsed_seconds);
    printf("===================================\n");

    clean(def);
    free(def);
    return 0;
}