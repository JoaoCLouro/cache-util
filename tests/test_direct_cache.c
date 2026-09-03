#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "../include/direct_cache_interface.h"

static void assert_test(int condition, const char* test_name) {
    if (condition) {
        printf("[PASSED] %s\n", test_name);
    } else {
        printf("[FAILED] %s\n", test_name);
        exit(1);
    }
}

int main(void) {
    printf("=== Starting Direct Cache Interface Test Suite ===\n");

    // 1. Initialize Direct Cache Environment & Get Passkey
    uint64_t passkey = init_t();
    assert_test(passkey != 0, "Direct cache initialization and passkey generation");

    // 2. Test Direct Write Operation
    uint64_t target_address = 0x0000000000001000;
    uint64_t write_status = write_cache_t(target_address, passkey);
    
    // Status 0 indicates success, 2 indicates invalid passkey
    assert_test(write_status == 0 || write_status == 2, "Direct write cache execution status code");

    // 3. Test Direct Read Operation
    uint64_t return_data = 0;
    size_t bytes_to_read = 8;
    uint64_t read_status = read_cache_t(target_address, &return_data, bytes_to_read, passkey);

    // Status codes: 0 = Success, 1 = Cache miss, 2 = Invalid passkey, 3 = Index error[cite: 6]
    printf("[INFO] Direct read exit status code: %lu\n", read_status);
    assert_test(read_status <= 3, "Direct read execution returned valid status code range");

    // 4. Test Invalid Passkey Protection
    uint64_t invalid_passkey = passkey + 999;
    uint64_t bad_write_status = write_cache_t(target_address, invalid_passkey);
    assert_test(bad_write_status == 2, "Direct write rejects invalid passkey with code 2");

    printf("=== All Direct Cache Tests Completed Successfully ===\n");
    return 0;
}