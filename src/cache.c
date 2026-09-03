#include "../include/cache.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>


// ============================================================================
// Internal Structs and Types Definitions
// ============================================================================

/**
 * @brief Enum defining the type of operation being executed on the cache.
 */
enum OperationType {
    READ,
    WRITE
};

/**
 * @brief Thread-safe stack implementation to hold available thread indexes or memory addresses.
 * * Uses uint64_t to safely store either a thread index or a 64-bit hardware address without truncation.
 * * Manipulations of this stack should be protected by a mutex in multi-threaded contexts.
 */
typedef struct stack {
    uint64_t value;
    struct stack* next;
} stack;

/**
 * @brief Struct holding the error information for the cache library.
 * * Tracks the error code, the operation type that failed, and a stack of addresses that caused failures.
 */
typedef struct error {
    int8_t code;   
    enum OperationType type;
    uint8_t is_fatal;
    stack* s; // Failed accesses addresses stack
} error;

/**
 * @brief Private buffer holding the pending accesses to be executed on the next flush call.
 * * Contains buffers for read/write addresses, return destinations, and tracks current buffer capacities.
 */
typedef struct cache_accesses_buffer {
    uint64_t* read_buffer;
    uint8_t r_buffer_count;
    uint64_t* write_buffer;
    uint8_t w_buffer_count;
    uint64_t** return_buffers;
    uint8_t return_buffers_count;
    error* e;
} cache_accesses_buffer;

/**
 * @brief Struct holding the core configuration and active buffers for the cache library instance.
 */
struct Definition {
    uint8_t thread_count;
    uint8_t max_buffer_size;
    uint64_t passkey;
    cache_accesses_buffer* accesses_buffer;
};

/**
 * @brief Thread-local payload holding arguments for read and write concurrent worker threads.
 * * Securely scoped per-thread to prevent data races. Contains local pointers to sync mutexes and stacks.
 */
typedef struct access_args {
    uint64_t address;
    uint64_t* return_buffer;
    uint64_t passkey;
    uint8_t thread_idx;
    uint8_t exit_code;
    stack** local_stack;
    pthread_mutex_t* stack_mutex;
} access_args;


// ==============
// Prototypes
// ==============

static uint8_t address_compatibility_check(const uint64_t address, const uint64_t *base_buffer, int base_buffer_count);
static uint8_t fill_write_buffer(Definition *def, const uint64_t *write_buffer, int count);
static uint8_t flush_write(Definition *def);
static void *write_thread_func(void *arg);
static uint8_t fill_read_buffer(Definition* def, const uint64_t* read_addresses, const uint8_t address_count, const size_t* byte_counts, const void** return_buffers);
static uint8_t flush_read(Definition *def);
static void* read_thread_func(void *arg);
void push(stack** s, pthread_mutex_t* mutex, uint64_t value);
int64_t pop(stack** s, pthread_mutex_t* mutex);


// ============================================================================
// Init, Getters and Setters Functions
// ============================================================================

/**
 * @brief Initializes the cache environment and sets up the internal buffering system.
 * @param passkey The verification key for cache access.
 * @return Definition* Pointer to the fully allocated struct holding configuration values.
 */
Definition* m_init_t (const uint64_t passkey)
{
    Definition* def = malloc(sizeof(Definition));
    def->thread_count = 1;
    def->max_buffer_size = def->thread_count * 2; 
    def->passkey = passkey;
    
    def->accesses_buffer = calloc(1, sizeof(cache_accesses_buffer));
    def->accesses_buffer->read_buffer = calloc(def->max_buffer_size, sizeof(uint64_t));
    def->accesses_buffer->write_buffer = calloc(def->max_buffer_size, sizeof(uint64_t));
    def->accesses_buffer->return_buffers = calloc(def->max_buffer_size, sizeof(uint64_t*));
    def->accesses_buffer->e = calloc(1, sizeof(error));
    
    return def;
}

/**
 * @brief Sets the maximum number of concurrent threads to use during batched operations.
 * @param def Pointer to the Definition configuration struct.
 * @param thread_count Desired number of threads (1 to 64).
 * @return int8_t 1 on success, 0 if null, -1 if out of bounds.
 */
int8_t set_thread_count (Definition* def, const uint8_t thread_count)
{
    if (def == NULL) return 0;
    if (thread_count == 0 || thread_count > 64) return -1;
    def->thread_count = thread_count;
    return 1;
}

/**
 * @brief Retrieves the current max thread count configuration.
 * @param def Pointer to the Definition configuration struct.
 * @return uint8_t Current thread count, or 0 if def is null.
 */
uint8_t get_thread_count (const Definition* def)
{
    if (def == NULL) return 0;
    return def->thread_count;
}

/**
 * @brief Sets the capacity trigger limit for the pending batch operations buffer.
 * @param def Pointer to the Definition configuration struct.
 * @param size Desired maximum buffer size before automatic flush occurs (1 to 128).
 * @return int8_t 1 on success, 0 if null, -1 if out of bounds.
 */
int8_t set_max_wait_size (Definition* def, const uint8_t size)
{
    if (def == NULL) return 0;
    if (size == 0 || size > 128) return -1;
    def->max_buffer_size = size;
    return 1;
}

/**
 * @brief Retrieves the current capacity trigger limit.
 * @param def Pointer to the Definition configuration struct.
 * @return uint8_t Current max buffer size, or 0 if def is null.
 */
uint8_t get_max_wait_size (const Definition* def)
{
    if (def == NULL) return 0;
    return def->max_buffer_size;
}

// ============================================================================
// Cache Operations Interface
// ============================================================================

/**
 * @brief Safely clears and reallocates all internal accesses buffers, dropping unexecuted requests.
 * @param def Pointer to the Definition configuration struct.
 * @warning Any pending writes in the buffer will be permanently lost without executing.
 */
void clean (Definition* def)
{
    if (def == NULL) return;
    
    free(def->accesses_buffer->read_buffer);
    free(def->accesses_buffer->write_buffer);
    free(def->accesses_buffer->return_buffers);
    free(def->accesses_buffer->e);
    free(def->accesses_buffer);
    
    def->accesses_buffer = calloc(1, sizeof(cache_accesses_buffer));
    def->accesses_buffer->read_buffer = calloc(def->max_buffer_size, sizeof(uint64_t));
    def->accesses_buffer->write_buffer = calloc(def->max_buffer_size, sizeof(uint64_t));
    def->accesses_buffer->return_buffers = calloc(def->max_buffer_size, sizeof(uint64_t*));
    def->accesses_buffer->e = calloc(1, sizeof(error));
}

/**
 * @brief Manually triggers the multi-threaded execution of all currently pending read and write operations.
 * @param def Pointer to the Definition configuration struct.
 * @return enum Error_Type Status of the execution (SUCCESS, CACHE_MISS, INVALID_PASSKEY, etc).
 */
enum Error_Type flush (Definition* def)
{
    if (def == NULL) return IMPLEMENTATION_ERROR;
    
    if (flush_read(def) != 0 || flush_write(def) != 0)
    {
        switch (def->accesses_buffer->e->code)
        {
            case -1: return NO_RETURN_BUFFER;
            case 1:  return CACHE_MISS;
            case 2:  return INVALID_PASSKEY;
            case 3:  return IMPLEMENTATION_ERROR;
            default: return IMPLEMENTATION_ERROR;
        }
    }
    return SUCCESS;
}

/**
 * @brief Executes batch-mode sequence reads against multiple cache targets returning a Result type.
 * @param def Pointer to the Definition configuration struct.
 * @param read_addresses Array of 64-bit hardware addresses to look up.
 * @param address_count Total number of lookups.
 * @param byte_counts Array containing execution read lengths.
 * @param return_buffers Null terminated array of destination memory addresses.
 * @return CacheResult Tagged union containing either total ops/bytes or error code and failing address.
 */
CacheResult multi_read_cache_t(Definition* def, const uint64_t* read_addresses, const uint8_t address_count, const size_t* byte_counts, const void** return_buffers)
{
    CacheResult result = { .is_ok = 1, .value.ok = {0, 0} };
    
    if (def == NULL) {
        result.is_ok = 0;
        result.value.err.code = IMPLEMENTATION_ERROR;
        result.value.err.failed_address = 0;
        return result;
    }

    uint8_t read = fill_read_buffer(def, read_addresses, address_count, byte_counts, return_buffers);
    
    size_t current_bytes = 0;
    for (int i = 0; i < read; i++) {
        current_bytes += byte_counts[i];
    }
    
    if (read != address_count)
    {
        enum Error_Type flush_err = flush(def);
        if (flush_err == SUCCESS) {
            CacheResult next = multi_read_cache_t(def, read_addresses + read, address_count - read, byte_counts + read, return_buffers + read);
            
            if (next.is_ok) {
                result.value.ok.operations_completed = read + next.value.ok.operations_completed;
                result.value.ok.total_bytes = current_bytes + next.value.ok.total_bytes;
            } else {
                return next;
            }
            return result;
        } else {
            result.is_ok = 0;
            result.value.err.code = flush_err;
            result.value.err.failed_address = (uint64_t)pop(&(def->accesses_buffer->e->s), NULL);
            return result;
        }
    }
    
    result.value.ok.operations_completed = read;
    result.value.ok.total_bytes = current_bytes;
    return result;
}

/**
 * @brief Executes batch-mode sequence allocations into the cache space returning a Result type.
 * @param def Pointer to the Definition configuration struct.
 * @param write_buffers Array of pointers containing the block entries to be written.
 * @param write_count Number of elements to write.
 * @return CacheResult Tagged union containing either total ops or error code and failing address.
 */
CacheResult multi_write_cache_t(Definition* def, const uint64_t* write_buffers, int write_count)
{
    CacheResult result = { .is_ok = 1, .value.ok = {0, 0} };
    
    if (def == NULL) {
        result.is_ok = 0;
        result.value.err.code = IMPLEMENTATION_ERROR;
        result.value.err.failed_address = 0;
        return result;
    }
    
    uint8_t written = fill_write_buffer(def, write_buffers, write_count);

    if (written != write_count)
    {
        enum Error_Type flush_err = flush(def);
        if (flush_err == SUCCESS) {
            CacheResult next = multi_write_cache_t(def, write_buffers + written, write_count - written);
            if (next.is_ok) {
                result.value.ok.operations_completed = written + next.value.ok.operations_completed;
            } else {
                return next; 
            }
            return result;
        } else {
            result.is_ok = 0;
            result.value.err.code = flush_err;
            result.value.err.failed_address = (uint64_t)pop(&(def->accesses_buffer->e->s), NULL);
            return result;
        }
    }
    
    result.value.ok.operations_completed = written;
    return result;
}

/**
 * @brief Evaluates structural cache alignment invariants between two memory blocks.
 * @param address_1 First 64-bit target address.
 * @param address_2 Second 64-bit target address.
 * @return uint8_t 1 if both addresses map to the exact same cache set index boundary, 0 otherwise.
 */
uint8_t multi_operation_compatible_t(const uint64_t address_1, const uint64_t address_2)
{
    uint64_t index_mask = (1ULL << CACHE_INDEX_BITS) - 1;
    uint64_t ad1 = (address_1 >> CACHE_OFFSET_BITS) & index_mask;
    uint64_t ad2 = (address_2 >> CACHE_OFFSET_BITS) & index_mask;
    return (ad1 != ad2) ? 1 : 0;
}


// ======================================================================================
//  Helper methods
// ======================================================================================

/**
 * @brief Fills the write buffer, checking for cache-line conflicts with existing queued read/write operations.
 * @param def Pointer to the Definition struct.
 * @param write_buffer Pointer to the array of addresses to be queued for writing.
 * @param count Number of addresses to queue.
 * @return uint8_t Number of addresses successfully queued before hitting capacity or conflict.
 */
static uint8_t fill_write_buffer(Definition *def, const uint64_t *write_buffer, int count)
{
    uint8_t max = def->max_buffer_size;
    uint64_t *base_write = def->accesses_buffer->write_buffer;
    uint64_t write_current = def->accesses_buffer->w_buffer_count;
    uint64_t *base_read = def->accesses_buffer->read_buffer;
    uint64_t read_current = def->accesses_buffer->r_buffer_count;
    uint8_t written = 0;

    while (write_current + written <= max && written < count)
    {
        if (address_compatibility_check(write_buffer[written], base_write, write_current) != 0 ||
            address_compatibility_check(write_buffer[written], base_read, read_current) != 0)
        {
            return written;
        }
        base_write[write_current + written] = write_buffer[written];
        written++;
    }
    def->accesses_buffer->w_buffer_count += written;
    return written;
}

/**
 * @brief Dispatches POSIX threads to execute all pending write operations concurrently.
 * @param def Pointer to the Definition struct.
 * @return uint8_t 0 on success, 1 if any child thread reported a fatal error.
 */
static uint8_t flush_write(Definition *def)
{
    uint8_t count = def->accesses_buffer->w_buffer_count;
    if (count == 0) return 0;

    uint8_t thread_count = (count > def->thread_count) ? def->thread_count : count;
    
    pthread_t *thread_array = calloc(thread_count, sizeof(pthread_t));
    uint8_t *thread_active = calloc(thread_count, sizeof(uint8_t));
    access_args **thread_args = calloc(thread_count, sizeof(access_args*));

    stack* local_stack = NULL;
    pthread_mutex_t local_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t error_stack_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    for (int i = thread_count - 1; i >= 0; i--) {
        push(&local_stack, &local_mutex, i);
    }

    uint8_t error_occurred = 0;

    for (int i = 0; i < count; i++)
    {
        int64_t idx = pop(&local_stack, &local_mutex);
        if (idx != -1)
        {
            if (thread_active[idx]) {
                pthread_join(thread_array[idx], NULL);
                if (thread_args[idx]->exit_code != 0) {
                    def->accesses_buffer->e->code = thread_args[idx]->exit_code;
                    def->accesses_buffer->e->type = WRITE;
                    def->accesses_buffer->e->is_fatal = 1;
                    push(&(def->accesses_buffer->e->s), &error_stack_mutex, thread_args[idx]->address);
                    error_occurred = 1;
                }
                free(thread_args[idx]);
                thread_active[idx] = 0;
            }

            if (error_occurred) {
                push(&local_stack, &local_mutex, idx);
                break;
            }

            access_args *t_args = calloc(1, sizeof(access_args));
            t_args->address = def->accesses_buffer->write_buffer[i];
            t_args->passkey = def->passkey;
            t_args->thread_idx = (uint8_t)idx;
            t_args->local_stack = &local_stack;
            t_args->stack_mutex = &local_mutex;
            
            thread_args[idx] = t_args;
            thread_active[idx] = 1;
            
            pthread_create(&thread_array[idx], NULL, write_thread_func, t_args);
        } 
        else
        {
            i--; 
        }
    }

    for (int i = 0; i < thread_count; i++) {
        if (thread_active[i]) {
            pthread_join(thread_array[i], NULL);
            if (thread_args[i]->exit_code != 0 && !error_occurred) {
                def->accesses_buffer->e->code = thread_args[i]->exit_code;
                def->accesses_buffer->e->type = WRITE;
                def->accesses_buffer->e->is_fatal = 1;
                push(&(def->accesses_buffer->e->s), &error_stack_mutex, thread_args[i]->address);
                error_occurred = 1;
            }
            free(thread_args[i]);
        }
    }

    while (pop(&local_stack, NULL) != -1);
    free(thread_array);
    free(thread_active);
    free(thread_args);

    def->accesses_buffer->w_buffer_count = 0;
    return error_occurred ? 1 : 0;
}

/**
 * @brief Thread entry point for executing direct write instructions.
 * @param arg Pointer to thread-local access_args configuration payload.
 * @return void* Always NULL.
 */
static void *write_thread_func(void *arg)
{
    access_args *args = (access_args *)arg;
    uint64_t code = write_cache_t(args->address, args->passkey);
    args->exit_code = code;
    push(args->local_stack, args->stack_mutex, args->thread_idx);
    return NULL;
}

/**
 * @brief Fills the read buffer, checking for cache-line conflicts with existing queued read/write operations.
 * @param def Pointer to the Definition struct.
 * @param read_addresses Pointer to array of addresses to read.
 * @param address_count Number of addresses to queue.
 * @param byte_counts Expected byte retrieval length per request.
 * @param return_buffers Array of destination memory addresses for output data.
 * @return uint8_t Number of reads successfully queued before hitting capacity or conflict.
 */
static uint8_t fill_read_buffer (Definition* def, const uint64_t* read_addresses, const uint8_t address_count, const size_t* byte_counts, const void** return_buffers)
{
    uint8_t max = def->max_buffer_size;
    uint64_t *base_read = def->accesses_buffer->read_buffer;
    uint64_t read_current = def->accesses_buffer->r_buffer_count;
    uint64_t *base_write = def->accesses_buffer->write_buffer;
    uint64_t write_current = def->accesses_buffer->w_buffer_count;
    uint8_t read = 0;

    while (read_current + read <= max && read < address_count)
    {
        if (address_compatibility_check(read_addresses[read], base_read, read_current) != 0 ||
            address_compatibility_check(read_addresses[read], base_write, write_current) != 0)
        {
            return read;
        }
        base_read[read_current + read] = read_addresses[read];
        def->accesses_buffer->return_buffers[read_current + read] = (uint64_t*)return_buffers[read]; 
        read++;
    }
    
    def->accesses_buffer->r_buffer_count += read;
    return read;
}

/**
 * @brief Dispatches POSIX threads to execute all pending read operations concurrently.
 * @param def Pointer to the Definition struct.
 * @return uint8_t 0 on success, 1 if any child thread reported a fatal error or cache miss.
 */
static uint8_t flush_read(Definition *def)
{
    uint8_t count = def->accesses_buffer->r_buffer_count;
    if (count == 0) return 0;

    uint8_t thread_count = (count > def->thread_count) ? def->thread_count : count;
    
    pthread_t *thread_array = calloc(thread_count, sizeof(pthread_t));
    uint8_t *thread_active = calloc(thread_count, sizeof(uint8_t));
    access_args **thread_args = calloc(thread_count, sizeof(access_args*));

    stack* local_stack = NULL;
    pthread_mutex_t local_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t error_stack_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    for (int i = thread_count - 1; i >= 0; i--) {
        push(&local_stack, &local_mutex, i);
    }

    uint8_t error_occurred = 0;

    for (int i = 0; i < count; i++)
    {
        int64_t idx = pop(&local_stack, &local_mutex);
        if (idx != -1)
        {
            if (thread_active[idx]) {
                pthread_join(thread_array[idx], NULL);
                
                if (thread_args[idx]->exit_code != 0) {
                    def->accesses_buffer->e->code = thread_args[idx]->exit_code;
                    def->accesses_buffer->e->type = READ;
                    def->accesses_buffer->e->is_fatal = 1;
                    push(&(def->accesses_buffer->e->s), &error_stack_mutex, thread_args[idx]->address);
                    error_occurred = 1;
                }
                free(thread_args[idx]);
                thread_active[idx] = 0;
            }
            
            if (error_occurred) {
                push(&local_stack, &local_mutex, idx);
                break;
            }

            access_args *t_args = calloc(1, sizeof(access_args));
            t_args->address = def->accesses_buffer->read_buffer[i];
            t_args->return_buffer = def->accesses_buffer->return_buffers[i];
            t_args->passkey = def->passkey;
            t_args->thread_idx = (uint8_t)idx;
            t_args->local_stack = &local_stack;
            t_args->stack_mutex = &local_mutex;
            
            thread_args[idx] = t_args;
            thread_active[idx] = 1;
            
            pthread_create(&thread_array[idx], NULL, read_thread_func, t_args);
        } 
        else
        {
            i--;
        }
    }

    for (int i = 0; i < thread_count; i++) {
        if (thread_active[i]) {
            pthread_join(thread_array[i], NULL);
            if (thread_args[i]->exit_code != 0) {
                def->accesses_buffer->e->code = thread_args[i]->exit_code;
                def->accesses_buffer->e->type = READ;
                def->accesses_buffer->e->is_fatal = 1;
                push(&(def->accesses_buffer->e->s), &error_stack_mutex, thread_args[i]->address);
                error_occurred = 1;
            }
            free(thread_args[i]);
        }
    }

    while (pop(&local_stack, NULL) != -1);
    free(thread_array);
    free(thread_active);
    free(thread_args);

    def->accesses_buffer->r_buffer_count = 0;
    return error_occurred ? 1 : 0;
}

/**
 * @brief Thread entry point for executing direct read instructions.
 * @param arg Pointer to thread-local access_args configuration payload.
 * @return void* Always NULL.
 */
static void* read_thread_func(void *arg)
{
    access_args *args = (access_args *)arg;
    uint64_t code = read_cache_t(args->address, args->return_buffer, 8, args->passkey);
    args->exit_code = code;
    
    push(args->local_stack, args->stack_mutex, args->thread_idx);
    return NULL;
}

/**
 * @brief Checks if an address clashes with any address inside a pre-populated execution buffer.
 * @param address The target address to test.
 * @param base_buffer Array of already-queued addresses.
 * @param base_buffer_count Number of active elements in the base_buffer.
 * @return uint8_t 0 if compatible, -1 if a cache set conflict exists.
 */
static uint8_t address_compatibility_check(const uint64_t address, const uint64_t *base_buffer, int base_buffer_count)
{
    for (int i = 0; i < base_buffer_count; i++)
    {
        const uint64_t base_address = base_buffer[i];
        if (multi_operation_compatible_t(address, base_address) == 0)
        {
            return -1;
        }
    }
    return 0;
}


// ===================================
// Safe Stack Implementation
// ===================================

/**
 * @brief Pushes a value onto a linked-list stack, safely locking if a mutex is provided.
 * @param s Double pointer to the head of the target stack.
 * @param mutex Pointer to a pthread_mutex_t for thread-safety (can be NULL for single-threaded init).
 * @param value The uint64_t value to push onto the stack.
 */
void push(stack** s, pthread_mutex_t* mutex, uint64_t value)
{
    stack* tmp = calloc(1, sizeof(stack));
    tmp->value = value;
    
    if (mutex) pthread_mutex_lock(mutex);
    tmp->next = *s;
    *s = tmp;
    if (mutex) pthread_mutex_unlock(mutex);
}

/**
 * @brief Pops a value from a linked-list stack, safely locking if a mutex is provided.
 * @param s Double pointer to the head of the target stack.
 * @param mutex Pointer to a pthread_mutex_t for thread-safety (can be NULL for single-threaded cleanup).
 * @return int64_t The popped value, or -1 if the stack is currently empty.
 */
int64_t pop(stack** s, pthread_mutex_t* mutex)
{
    if (mutex) pthread_mutex_lock(mutex);
    
    if (*s == NULL)
    {
        if (mutex) pthread_mutex_unlock(mutex);
        return -1;
    }

    uint64_t tmp = (*s)->value;
    stack* tmp_stack = (*s)->next;
    free(*s);
    *s = tmp_stack;
    
    if (mutex) pthread_mutex_unlock(mutex);
    return (int64_t)tmp;
}