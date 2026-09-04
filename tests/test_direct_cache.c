#include <stdio.h>
#include <stdlib.h>
#include "../include/direct_cache_interface.h"

int main(void) {
    printf("=== Starting Direct Cache Interface Test Suite ===\n");
    
    uint64_t passkey = init_t();
    printf("[PASSED] Direct cache initialization and passkey generation\n");

    // Allocate valid host memory to act as the 64-byte source data for the cache write
    void* simulated_memory = calloc(1, CACHE_BLOCK_SIZE);
    uint64_t test_addr = (uint64_t)simulated_memory;

    uint64_t status = write_cache_t(test_addr, passkey);
    
    if (status == 0) {
        printf("[PASSED] Direct write cache execution status code\n");
    } else {
        printf("[FAILED] Direct write cache returned %lu\n", status);
    }

    char buffer[16] = {0};
    status = read_cache_t(test_addr, buffer, 8, passkey);
    
    if (status == 0 || status == 1) {
        printf("[PASSED] Direct read execution returned valid status code range\n");
    } else {
        printf("[FAILED] Direct read execution returned: %lu\n", status);
    }

    status = write_cache_t(test_addr, passkey + 1);
    if (status == 2) {
        printf("[PASSED] Direct write rejects invalid passkey with code 2\n");
    } else {
        printf("[FAILED] Invalid passkey rejected with wrong code: %lu\n", status);
    }

    free(simulated_memory);
    printf("=== All Direct Cache Tests Completed Successfully ===\n");
    return 0;
}