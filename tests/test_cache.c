#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "../include/cache.h"

// Helper function to verify test assertions with clear output messages
static void assert_test(int condition, const char* test_name) {
    if (condition) {
        printf("[PASSED] %s\n", test_name);
    } else {
        printf("[FAILED] %s\n", test_name);
        exit(1);
    }
}

int main(void) {
    printf("=== Starting Cache Library Test Suite ===\n");

    // 1. Initialize Cache Environment
    uint64_t passkey = init_t();
    Definition* def = m_init_t(passkey);
    assert_test(def != NULL, "Initialization of Definition struct");

    // 2. Test Configuration Getters and Setters
    int8_t thread_res = set_thread_count(def, 4);
    assert_test(thread_res == 1 && get_thread_count(def) == 4, "Set and get thread count");

    int8_t wait_res = set_max_wait_size(def, 16);
    assert_test(wait_res == 1 && get_max_wait_size(def) == 16, "Set and get max wait buffer size");

    // 3. Test Structural Address Compatibility (Set Collision Checking)
    // Addresses mapping to the same set index should return 0 (incompatible for same-batch execution)
    // Addresses mapping to different sets should return 1 (compatible)
    uint64_t addr1 = 0x0000000000000040; // Set index 1 (assuming 6 offset bits)
    uint64_t addr2 = 0x0000000000000040; // Same address, conflict
    uint64_t addr3 = 0x0000000000000140; // Different set index, compatible

    uint8_t same_compat = multi_operation_compatible_t(addr1, addr2);
    uint8_t diff_compat = multi_operation_compatible_t(addr1, addr3);
    
    assert_test(same_compat == 0, "Compatibility check: colliding addresses");
    assert_test(diff_compat == 1, "Compatibility check: non-colliding addresses");

    // 4. Test Multi-Write Operations with Result Handling
    uint64_t write_addresses[2] = { 0x1000, 0x2000 };
    CacheResult write_result = multi_write_cache_t(def, write_addresses, 2);
    
    assert_test(write_result.is_ok == 1, "Multi-write batch execution status");
    assert_test(write_result.value.ok.operations_completed == 2, "Multi-write operations completed count");

    // 5. Test Multi-Read Operations with Result Handling
    uint64_t read_addresses[2] = { 0x1000, 0x2000 };
    size_t byte_counts[2] = { 8, 8 };
    uint64_t return_buf_1 = 0;
    uint64_t return_buf_2 = 0;
    const void* return_buffers[2] = { &return_buf_1, &return_buf_2 };

    CacheResult read_result = multi_read_cache_t(def, read_addresses, 2, byte_counts, return_buffers);
    
    // Note: Depending on whether the ASM back-end simulates a cache hit or miss for unpopulated lines, 
    // we verify the Result framework correctly parses the outcome structure.
    if (read_result.is_ok) {
        assert_test(read_result.value.ok.operations_completed == 2, "Multi-read operations completed count");
        assert_test(read_result.value.ok.total_bytes == 16, "Multi-read total bytes count");
    } else {
        // If it resulted in a cache miss (exit code 1), check that the failed address tracking works
        printf("[INFO] Read triggered cache error code: %d at address: 0x%lX\n", 
               read_result.value.err.code, 
               read_result.value.err.failed_address);
        assert_test(read_result.value.err.failed_address != 0, "CacheResult captured failing address");
    }

    // 6. Test Clean Functionality
    clean(def);
    assert_test(get_max_wait_size(def) == 16, "Definition state maintained after clean call");

    // Free resources
    free(def);
    printf("=== All Cache Tests Completed Successfully ===\n");
    return 0;
}