#include "../include/direct_cache_interface.h"
#include <stdint.h>

// ============================================================================
// Extern Functions
// ============================================================================

extern uint64_t init (void);
extern uint64_t read_cache (uint64_t read_address, void* return_buffer, uint64_t bytes_to_read, uint64_t passkey);
extern uint64_t write_cache (uint64_t write_address, uint64_t passkey);

// ============================================================================
// Core Functions Implementations
// ============================================================================

uint64_t init_t (void)
{
    return init();
}

uint64_t read_cache_t (const uint64_t address, const void *return_buffer, const size_t bytes_to_read, const uint64_t passkey)
{
    return read_cache(address, (void *) return_buffer, (size_t) bytes_to_read, passkey);
}

uint64_t write_cache_t (const uint64_t address_to_write, const uint64_t passkey)
{
    return write_cache(address_to_write, passkey);
}
