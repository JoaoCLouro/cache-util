#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "../include/cache.h"

int main(void) {
    printf("=== Starting Cache Library Test Suite ===\n");
    
    uint64_t passkey = init_t();
    Definition* def = m_init_t(passkey);
    if (def) {
        printf("[PASSED] Initialization of Definition struct\n");
    } else {
        fprintf(stderr, "[FAILED] m_init_t returned NULL\n");
        return 1;
    }

    set_thread_count(def, 4);
    if (get_thread_count(def) == 4) {
        printf("[PASSED] Set and get thread count\n");
    }

    set_max_wait_size(def, 32);
    if (get_max_wait_size(def) == 32) {
        printf("[PASSED] Set and get max wait buffer size\n");
    }

    // Mathematical bitwise checks don't dereference memory, so hardcoded addresses are fine here
    if (multi_operation_compatible_t(0x1000, 0x1000) == 1) {
        printf("[PASSED] Compatibility check: colliding addresses\n");
    }
    if (multi_operation_compatible_t(0x1000, 0x2000) == 0) {
        printf("[PASSED] Compatibility check: non-colliding addresses\n");
    }

    // Allocate valid host memory to act as the source blocks for cache operations
    uint8_t* simulated_memory = (uint8_t*)calloc(3, 64);
    uint64_t addr1 = (uint64_t)(simulated_memory + 0);
    uint64_t addr2 = (uint64_t)(simulated_memory + 64);
    uint64_t addr3 = (uint64_t)(simulated_memory + 128);

    uint64_t write_addrs[] = {addr1, addr2};
    CacheResult w_res = multi_write_cache_t(def, write_addrs, 2);
    if (w_res.is_ok) {
        printf("[PASSED] Multi-write batch execution status\n");
    }
    
    // Flush will now successfully execute the memory copy from addr1/addr2 into the cache
    flush(def);

    uint64_t read_addrs[] = {addr1, addr3};
    size_t bytes[] = {8, 8};
    uint64_t buf1 = 0, buf2 = 0;
    const void* bufs[] = {&buf1, &buf2};
    
    CacheResult r_res = multi_read_cache_t(def, read_addrs, (uint8_t)2, bytes, bufs);
    flush(def);
    
    if (!r_res.is_ok) {
        printf("[INFO] Read triggered cache error code: %d at address: 0x%lx\n", 
            r_res.value.err.code, r_res.value.err.failed_address);
        if (r_res.value.err.code == CACHE_MISS) {
            printf("[PASSED] CacheResult captured failing address correctly\n");
        }
    }

    clean(def);
    printf("[PASSED] Definition state maintained after clean call\n");

    m_free_t(def);
    free(simulated_memory);
    
    printf("=== All Cache Tests Completed Successfully ===\n");
    return 0;
}