#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "../include/cache.h"
#include "../include/direct_cache_interface.h"

#define BATCH_SIZE 16
#define POOL_SIZE_BLOCKS 2048 // Expanded to exceed CACHE_SIZE and trigger capacity misses[cite: 2]

typedef struct {
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t hits;
    uint64_t misses;
    uint64_t invalid_passkeys;
    uint64_t other_errors;
    double time_elapsed;
} BenchmarkStats;

void run_workload_phase(const char* phase_name, Definition* def, int iterations, int workload_type, uint64_t* address_pool, size_t pool_count) {
    BenchmarkStats stats = {0};
    
    uint8_t* safe_host_memory = (uint8_t*)calloc(BATCH_SIZE, CACHE_CELL_SIZE); //[cite: 2]
    const void* return_ptrs[BATCH_SIZE];
    size_t byte_counts[BATCH_SIZE];
    
    for (int b = 0; b < BATCH_SIZE; b++) {
        return_ptrs[b] = safe_host_memory + (b * CACHE_CELL_SIZE); //[cite: 2]
        byte_counts[b] = 8;
    }

    clock_t start = clock();

    for (int i = 0; i < iterations; i++) {
        uint64_t batch_addrs[BATCH_SIZE];
        
        for (int b = 0; b < BATCH_SIZE; b++) {
            size_t idx = 0;
            switch (workload_type) {
                case 0: // Sequential scan
                    idx = (i * BATCH_SIZE + b) % pool_count;
                    break;
                case 1: // Pure random scatter
                    idx = rand() % pool_count;
                    break;
                case 2: // Hotspot (80/20 rule)
                    if (rand() % 100 < 80) {
                        idx = rand() % (pool_count / 5);
                    } else {
                        idx = rand() % pool_count;
                    }
                    break;
            }
            batch_addrs[b] = address_pool[idx];
        }

        // 20% writes, 80% reads mix
        if (i % 5 == 0) {
            multi_write_cache_t(def, batch_addrs, BATCH_SIZE); //[cite: 1]
            stats.total_writes += BATCH_SIZE;
        } else {
            CacheResult res = multi_read_cache_t(def, batch_addrs, (uint8_t)BATCH_SIZE, byte_counts, return_ptrs); //[cite: 1]
            stats.total_reads += BATCH_SIZE;
            if (res.is_ok) {
                stats.hits += res.value.ok.operations_completed;
            } else {
                if (res.value.err.code == CACHE_MISS) { //[cite: 1]
                    stats.misses += BATCH_SIZE;
                } else if (res.value.err.code == INVALID_PASSKEY) { //[cite: 1]
                    stats.invalid_passkeys++;
                } else {
                    stats.other_errors++;
                }
            }
        }
        flush(def); //[cite: 1]
    }

    stats.time_elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    uint64_t total_operations = stats.total_reads + stats.total_writes;
    double hit_rate = (stats.total_reads > 0) ? ((double)stats.hits / stats.total_reads) * 100.0 : 0.0;
    double throughput = total_operations / stats.time_elapsed;

    printf("\n=== Phase: %s ===\n", phase_name);
    printf("Total Reads : %lu\n", stats.total_reads);
    printf("Total Writes: %lu\n", stats.total_writes);
    printf("Read Hits   : %lu (%.2f%%)\n", stats.hits, hit_rate);
    printf("Read Misses : %lu\n", stats.misses);
    printf("Passkey Err : %lu\n", stats.invalid_passkeys);
    printf("Throughput  : %.2f operations/sec (%.4fs)\n", throughput, stats.time_elapsed);

    free(safe_host_memory);
}

int main(void) {
    srand(time(NULL));
    printf("=== Starting Realistic Cache Benchmark Suite ===\n");
    printf("Cache Capacity : %d bytes[cite: 2]\n", CACHE_SIZE);
    printf("Block Size     : %d bytes[cite: 2]\n", CACHE_BLOCK_SIZE);

    size_t backing_store_size = POOL_SIZE_BLOCKS * CACHE_BLOCK_SIZE; //[cite: 2]
    uint8_t* backing_store = (uint8_t*)calloc(1, backing_store_size);
    if (!backing_store) {
        fprintf(stderr, "Failed to allocate memory backing store.\n");
        return 1;
    }

    uint64_t address_pool[POOL_SIZE_BLOCKS];
    for (size_t i = 0; i < POOL_SIZE_BLOCKS; i++) {
        address_pool[i] = (uint64_t)(backing_store + (i * CACHE_BLOCK_SIZE)); //[cite: 2]
    }

    uint64_t passkey = init_t(); //[cite: 2]
    Definition* def = m_init_t(passkey); //[cite: 1]
    if (!def) {
        fprintf(stderr, "Failed to initialize cache definition struct[cite: 1].\n");
        free(backing_store);
        return 1;
    }

    set_thread_count(def, 8); //[cite: 1]
    set_max_wait_size(def, BATCH_SIZE * 4); //[cite: 1]

    for (size_t i = 0; i < POOL_SIZE_BLOCKS; i += BATCH_SIZE) {
        uint64_t priming_batch[BATCH_SIZE];
        for (int b = 0; b < BATCH_SIZE; b++) {
            priming_batch[b] = address_pool[i + b];
        }
        multi_write_cache_t(def, priming_batch, BATCH_SIZE); //[cite: 1]
        flush(def); //[cite: 1]
    }

    run_workload_phase("Sequential Scan Workload", def, 100000, 0, address_pool, POOL_SIZE_BLOCKS);
    run_workload_phase("Random Scatter Workload", def, 100000, 1, address_pool, POOL_SIZE_BLOCKS);
    run_workload_phase("Hotspot (80/20 Locality) Workload", def, 100000, 2, address_pool, POOL_SIZE_BLOCKS);

    clean(def); //[cite: 1]
    m_free_t(def); //[cite: 1]
    free(backing_store);

    printf("\n=== Benchmark Suite Completed Successfully ===\n");
    return 0;
}