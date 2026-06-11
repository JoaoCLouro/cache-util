#include "../include/cache.h"
#include <stdint.h>


// ============================================================================
// Internal Structs and Types Definitions
// ============================================================================

struct Definition {
    uint8_t thread_count;
    uint8_t max_buffer_size;
    uint64_t passkey;
    cache_accesses_buffer* accesses_buffer;
};



/**
 * @brief Private buffer holding the pending accesses to be executed on the next flush call.
 * * This buffer is used to store the pending accesses to be executed on the next flush call.
 * * It contains a buffer of addresses to be read and a buffer of addresses to be written,
 * * * as well as the return buffers for the read operations and counters for each of them.
 * * As soon as the total accesses pending count reaches the max buffer size,
 * * * the flush function is called to execute all the pending accesses.
 * * The flush function will try to multi-thread the current accesses to speed up execution time.
 * * The thread-count const defined on this file must be set to the number of threads the in use cpu has or bellow.
 * ** the clean function can be used to delete all accesses that are on hold, so they will never be executed.
 */
typedef struct cache_accesses_buffer {
    uint64_t* read_buffer;
    uint8_t r_buffer_count;
    uint64_t* write_buffer;
    uint8_t w_buffer_count;
    uint64_t** return_buffers;
    uint8_t return_buffers_count;
} cache_accesses_buffer;


// ============================================================================
// Init, Getters and Setters Functions
// ============================================================================

Definition* init_cache (uint64_t passkey)
{
    Definition* def = malloc(sizeof(Definition));
    def->thread_count = 1;
    def->max_buffer_size = def->thread_count * 2; // Each thread can have at most 2 pending accesses (1 read and 1 write)
    def->passkey = passkey;
    def->accesses_buffer = malloc(sizeof(cache_accesses_buffer));
    return def;
}

uint8_t set_thread_count (Definition* def, uint8_t thread_count)
{
    if (def == NULL) {
        return 0;
    }
    else if (thread_count == 0 || thread_count > 64) {
        return -1;
    }
    def->thread_count = thread_count;
    return 1;
}

uint8_t get_thread_count (Definition* def)
{
    if (def == NULL) {
        return 0;
    }
    return def->thread_count;
}

uint8_t set_max_wait_size (Definition* def, uint8_t size)
{
    if (def == NULL) {
        return 0;
    }
    else if (size == 0 || size > 128) {
        return -1;
    }
    def->max_buffer_size = size;
    return 1;
}

uint8_t get_max_wait_size (Definition* def)
{
    if (def == NULL) {
        return 0;
    }
    return def->max_buffer_size;
}

// ============================================================================
// Cache Operations Interface
// ============================================================================

void clean (Definition* def)
{
    if (def == NULL) {
        return;
    }
    // Free all the pending accesses buffers and allocate new ones
    free(def->accesses_buffer->read_buffer);
    free(def->accesses_buffer->write_buffer);
    for (int i = 0; i < def->accesses_buffer->return_buffers_count; i++) {
        free(def->accesses_buffer->return_buffers[i]);
    }
    free(def->accesses_buffer);
    def->accesses_buffer = malloc(sizeof(cache_accesses_buffer));
}

