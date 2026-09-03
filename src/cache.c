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



// ==============
// Error struct
// ==============

/**
 * @brief Struct holding the error information for the cache library.
 * * This struct is used to hold the error information for the cache library,
 * * such as the error code, the type of operation being executed, and whether the error is fatal or not.
 */
typedef struct error {
    uint8_t code;
    enum OperationType type;
    uint8_t is_fatal;
} error;



// ======================
// Stack Implementation
// ======================

/**
 * @brief Simple stack implementation to hold the available thread indexes for multi-threaded execution.
 * * This stack is used to hold the available thread indexes for multi-threaded execution.
 * * It is used to keep track of which threads are currently executing and which are available for new accesses.
 * * The stack is initialized with the number of threads to use at once, and
 * * * the available thread indexes are pushed onto the stack.
 * * When a thread is created to execute an access, it pops an index from the stack
 * * * and uses it to execute the access. When the access is finished, the index is pushed back onto the stack to be used by another access.
 * * The stack is implemented as a linked list, with each node containing a value and a pointer to the next node.
 */
typedef struct stack {
    uint8_t value;
    struct stack* next;
} stack;

stack* s = NULL;

// ==================
// Stack Prototypes
// ==================

void init_stack_with_thread_count (stack* s, uint8_t count);
void clear_stack (stack* s);
void push (stack* s, uint8_t value);
uint8_t pop (stack* s);



// =======================
// Cache accesses buffer
// =======================

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
    error* e;
} cache_accesses_buffer;

struct Definition {
    uint8_t thread_count;
    uint8_t max_buffer_size;
    uint64_t passkey;
    cache_accesses_buffer* accesses_buffer;
};


// =================
// Args definition
// =================

/**
 * @brief Struct holding the arguments for the write and read functions.
 * * This struct is used to hold the arguments for the write and read functions,
 * * such as the address to be accessed, the passkey for the cache library, and the index of the thread executing the access.
 */
typedef struct access_args {
    uint64_t address;
    uint64_t* return_buffer;
    uint64_t passkey;
    uint8_t thread_idx;
    uint8_t exit_code;      // Only for read
} access_args;



// ==============
// Prototypes
// ==============

// Write

static uint8_t fill_write_buffer(Definition *def, const uint64_t *write_buffer, int count);
static void flush_write(Definition *def);
static void *write(void *arg);
static uint8_t address_compatibility_check(const uint64_t address, const uint64_t *base_buffer, int base_buffer_count);

// Read

static uint8_t fill_read_buffer (Definition* def, const uint64_t* read_addresses, const uint8_t address_count, const size_t* byte_counts);
static void flush_read(Definition *def);
static void* read (void *arg);



// ============================================================================
// Init, Getters and Setters Functions
// ============================================================================

Definition* m_init_t (const uint64_t passkey)
{
    Definition* def = malloc(sizeof(Definition));
    def->thread_count = 1;
    def->max_buffer_size = def->thread_count * 2; // Each thread can have at most 2 pending accesses (1 read and 1 write)
    def->passkey = passkey;
    // calloc for a clean allocation, not garbage values
    def->accesses_buffer = calloc(1, sizeof(cache_accesses_buffer));
    return def;
}

uint8_t set_thread_count (Definition* def, const uint8_t thread_count)
{
    if (def == NULL)
    {
        return 0;
    }
    else if (thread_count == 0 || thread_count > 64) {
        return -1;
    }
    def->thread_count = thread_count;
    return 1;
}

uint8_t get_thread_count (const Definition* def)
{
    if (def == NULL)
    {
        return 0;
    }
    return def->thread_count;
}

uint8_t set_max_wait_size (Definition* def, const uint8_t size)
{
    if (def == NULL)
    {
        return 0;
    }
    else if (size == 0 || size > 128) {
        return -1;
    }
    def->max_buffer_size = size;
    return 1;
}

uint8_t get_max_wait_size (const Definition* def)
{
    if (def == NULL)
    {
        return 0;
    }
    return def->max_buffer_size;
}

// ============================================================================
// Cache Operations Interface
// ============================================================================

void clean (Definition* def)
{
    if (def == NULL)
    {
        return;
    }
    // Free all the pending accesses buffers and allocate new ones
    free(def->accesses_buffer->read_buffer);
    free(def->accesses_buffer->write_buffer);
    for (int i = 0; i < def->accesses_buffer->return_buffers_count; i++) {
        free(def->accesses_buffer->return_buffers[i]);
    }
    free(def->accesses_buffer);
    def->accesses_buffer = calloc(1, sizeof(cache_accesses_buffer));
}

void flush (Definition* def)
{
    if (def == NULL){
        return;
    }
    flush_read(def);
    flush_write(def);
}

uint8_t multi_read_cache_t(Definition* def, const uint64_t* read_addresses, const uint8_t address_count, const size_t* byte_counts, const void** return_buffers)
{
    if (def == NULL) 
    {
        return 0;
    }

    def ->accesses_buffer->return_buffers = (uint64_t**) return_buffers;
    int8_t read = fill_read_buffer(def, read_addresses, address_count, byte_counts);
    
    // Address incompatibility
    if (read == -1)
    {
        flush(def);
        multi_read_cache_t(def, read_addresses, address_count, byte_counts, return_buffers);
        println("Buffers flushed!");
    }
    return (uint8_t) read;
}

uint8_t multi_write_cache_t(Definition* def, const uint64_t* write_buffers, int write_count)
{
    if (def == NULL) 
    {
        return -1;
    }
    
    // Calculates and writes to the buffer the possible compatible addresses
    uint8_t written = fill_write_buffer(def, write_buffers, write_count);

    // If at least one was incompatible flush the buffer and start over
    if (written != write_count)
    {
        // Triggers execution
        flush(def);
        multi_write_cache_t(def, (write_buffers + (written - 1) * sizeof(uint64_t)), write_count - written);
    }
    return 0;
}

uint8_t multi_operation_compatible_t(const uint64_t address_1, const uint64_t address_2)
{
    // Shifts to only contain index bits and compare them
    uint64_t ad1 = (address_1 << CACHE_TAG_BITS) >> CACHE_TAG_BITS >> CACHE_OFFSET_BITS;
    uint64_t ad2 = (address_2 << CACHE_TAG_BITS) >> CACHE_TAG_BITS >> CACHE_OFFSET_BITS;
    return (ad1 != ad2) ? 1 : 0;
}


// ======================================================================================
//  Helper methods
// ======================================================================================

/**
 * @brief Fills the write buffer with the given addresses, checking for compatibility with existing addresses in the read and write buffers.
 * @param def Pointer to the definition struct holding the configuration values for the cache library.
 * @param write_buffer Pointer to the array of addresses to be written.
 * @param count Number of addresses in the write_buffer.
 * @return uint8_t Number of addresses successfully written to the buffer.
 */
static uint8_t fill_write_buffer(Definition *def, const uint64_t *write_buffer, int count)
{
    uint8_t max = def->max_buffer_size;
    uint64_t *base_write = def->accesses_buffer->write_buffer;
    uint64_t write_current = def->accesses_buffer->w_buffer_count;
    uint64_t *base_read = def->accesses_buffer->read_buffer;
    uint64_t read_current = def->accesses_buffer->r_buffer_count;
    uint8_t written = 0;

    while (write_current + written <= max || written < count)
    {
        if (address_compatibility_check(write_buffer[written], base_write, write_current) != 0)
        {
            return written;
        }
        if (address_compatibility_check(write_buffer[written], base_read, read_current) != 0)
        {
            return written;
        }
        base_write[write_current + written] = write_buffer[written];
        written++;
    }

    return written;
}

/**
 * @brief Flushes the write buffer by executing all pending write operations concurrently using multiple threads.
 * @param def Pointer to the definition struct holding the configuration values for the cache library.
 * @warning The thread-count const defined on this file must be set to the number of threads the in use cpu has or bellow.
 */
static void flush_write(Definition *def)
{
    uint8_t count = def->accesses_buffer->w_buffer_count;
    // Multithreaded sync

    // Thread count definition
    // Note: Thread count is not limited to the number of threads available by the CPU for maximum compatibility with the cache library,
    // but it is recommended to set it to the number of threads available by the CPU or below for optimal performance.
    uint8_t thread_count = (count > def->thread_count)? def->thread_count : count;
    init_stack_with_thread_count(s,thread_count);

    pthread_t thread_array[thread_count];

    
    // writing loop
    for (int i = 0; i < count; i++)
    {
        int8_t idx = pop(s);
        if (idx != -1)
        {
            // Creates and populates the args struct
            access_args *args = malloc(sizeof(*args));
            args->address = def->accesses_buffer->write_buffer[i];
            args->passkey = def->passkey;
            args->thread_idx = idx;
            // Creates the thread to execute the write operation
            pthread_create(&thread_array[idx], NULL, write, args);
            if (args->exit_code == 2)
            {
                println("Write operation failed due to cache error.");
            }

        } 
        else
        {
            // Keeps the loop on the same access
            i--;
        }
    }
    clear_stack(s);
}

static void *write(void *arg)
{
    access_args *args = arg;
    if (write_cache_t(args->address, args->passkey) == 2)
    {
        args->exit_code = 2;
    };
    push(s, args->thread_idx);
    return NULL;
}



static uint8_t fill_read_buffer (Definition* def, const uint64_t* read_addresses, const uint8_t address_count, const size_t* byte_counts)
{
    uint8_t max = def->max_buffer_size;
    uint64_t *base_read = def->accesses_buffer->read_buffer;
    uint64_t read_current = def->accesses_buffer->r_buffer_count;
    uint64_t *base_write = def->accesses_buffer->write_buffer;
    uint64_t write_current = def->accesses_buffer->w_buffer_count;
    uint8_t read = 0;

    while (read_current + read <= max || read < address_count)
    {
        if (address_compatibility_check(read_addresses[read], base_read, read_current) != 0)
        {
            return read;
        }
        if (address_compatibility_check(read_addresses[read], base_write, write_current) != 0)
        {
            return read;
        }
        base_read[read_current + read] = read_addresses[read];
        read++;
    }

    return read;
}

static void flush_read(Definition *def)
{
    if (def->accesses_buffer->return_buffers == NULL)
    {
        return;
    }

    uint8_t count = def->accesses_buffer->r_buffer_count;
    // Multithreaded sync

    // Thread count definition
    // Note: Thread count is not limited to the number of threads available by the CPU for maximum compatibility with the cache library,
    // but it is recommended to set it to the number of threads available by the CPU or below for optimal performance.
    uint8_t thread_count = (count > def->thread_count)? def->thread_count : count;
    init_stack_with_thread_count(s,thread_count);

    pthread_t thread_array[thread_count];

    
    // writing loop
    for (int i = 0; i < count; i++)
    {
        int8_t idx = pop(s);
        if (idx != -1)
        {
            // Creates and populates the args struct
            access_args *args = malloc(sizeof(*args));
            args->address = def->accesses_buffer->read_buffer[i];
            args->return_buffer = def->accesses_buffer->return_buffers[i];
            args->passkey = def->passkey;
            args->thread_idx = idx;
            // Creates the thread to execute the read operation
            pthread_create(&thread_array[idx], NULL, read, args);
            if (args->exit_code == 1)
            {
                println("Read operation failed due to cache error.");
            }
        } 
        else
        {
            // Keeps the loop on the same access
            i--;
        }
    }
    clear_stack(s);
}

static void* read (void *arg)
{
    access_args *args = arg;
    uint64_t code = read_cache_t(args->address, args->passkey, 8, args->thread_idx);
    if (code == 0)
    {
        push(s, args->thread_idx);
        free(args);
        return NULL;
    }
    args->exit_code = code;
    push(s, args->thread_idx);
}


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
// Simple stack implementation
// ===================================

/**
 * @brief Pushes all numbers from \count - 1 to 0 to the stack
 * @param s Pointer to the stack root struct
 * @param counter Biggest number to push
 * @warning pushes all numbers from \counter - 1 to 0 to the stack
 */
void init_stack_with_thread_count (stack* s, uint8_t count)
{
    for (int i = count - 1; i >= 0; i--)
    {
        push(s, i);
    }
}

/**
 * @brief Clear the stack of all its elements
 * @param s Pointer to the stack root struct
 * @warning Will loose all values on the stack
 */
void clear_stack (stack* s)
{
    while (s != NULL)
    {
        pop(s);
    }
}

/**
* @brief Adds an element on to the stack or initializes if it is not yet initialized
* @param s Pointer to the stack root struct
* @param value Void pointer to the value push
* @warning  value should not be null
*
*/
void push (stack* s, uint8_t value)
{
    stack* tmp = calloc(1, sizeof(stack));
    tmp->value = value;
    tmp->next = s;
    s = tmp;
}

/**
* @brief Removes an element from the top of the stack or returns -1 if empty
* @param s Pointer to the stack root struct
* @returns the value at the top of the stack if possible
* @warning Returns -1 if the stack is empty
*/
uint8_t pop (stack* s)
{
    if (s == NULL)
    {
        return -1;
    }

    uint8_t tmp = s->value;
    stack* tmp_stack = s->next;
    free(s);
    s = tmp_stack;
    return tmp;
}

