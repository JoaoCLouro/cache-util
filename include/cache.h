#ifndef CACHELIB_H
#define CACHELIB_H

// ============================================================================
// About
// ============================================================================

// Higher implementation interface that provides not only single accesses to the cache
// but also simultaneous accesses.
// Will use the direct interface for the single calls and this interface for multi access calls

// ============================================================================
// Included Libs
// ============================================================================

// Provides all the direct access functions to the cache, as well as the passkey management
#include "direct_cache_interface.h"

// ============================================================================
//  Structures Prototypes
// ============================================================================

/**
 * @brief Structure containing module constraints such as number of threads available and active and the accesses buffer
 */
typedef struct Definition Definition;

/**
 * @brief Structure containing the pending accesses to be executed on the next flush call.
 */
typedef enum Error_Type {
    NO_RETURN_BUFFER = -1,
    SUCCESS = 0,
    CACHE_MISS = 1,
    INVALID_PASSKEY = 2,
    IMPLEMENTATION_ERROR = 3
} Error_Type;

// ============================================================================
// Cache Result type (A rust Result<T, E> like type)
// ============================================================================

/**
 * @brief Struct representing the result of a cache operation, similar to Rust's Result<T, E> type.
 * * Contains either a successful outcome with operation details or an error with specific failure information.
 * * This struct is designed to provide a clear and structured way to handle the results of cache operations, including both read and write actions.
 * * The `is_ok` field indicates whether the operation was successful (1) or if an error occurred (0).
 * * The `value` union holds the details of the successful operation or the error information, depending on the value of `is_ok`.
 * @usage
 * CacheResult result = multi_read_cache_t(def, read_addresses, address_count, byte_counts, return_buffers);
 * if (result.is_ok) {
 *     // Handle successful read operation
 *     uint8_t completed = result.value.ok.operations_completed;
 *     size_t bytes_read = result.value.ok.total_bytes;
 * } else {
 *     // Handle error
 *     enum Error_Type error_code = result.value.err.code;
 *     uint64_t failed_address = result.value.err.failed_address;
 * }
 */
typedef struct CacheResult {
    uint8_t is_ok;          // 0 if error, 1 if ok
    union {
        // if is_ok == 1
        struct{
            uint8_t operations_completed; // Number of cache accesses successfully queued/executed
            size_t total_bytes;           // Total bytes read (0 for write operations)
        } ok;
        
        // if is_ok == 0
        struct {
            enum Error_Type code;         // Specific error that occurred
            uint64_t failed_address;      // Exact hardware address that triggered the failure
        } err;
    } value;
} CacheResult;

// ============================================================================
// Core Functions Interface
// ============================================================================

    /**
     * @brief Initializes the cache environment and sets up a randomized passkey.
     * @param passkey           The passkey to set for cache access.
     * @return                  Definition* Pointer to the struct holding the configuration values for the cache library.
     */
    Definition* m_init_t (const uint64_t passkey);

    /**
     * @brief Executes batch-mode sequence reads against multiple cache targets.
     * @param def               Pointer to the definition struct holding the configuration values for the cache library.
     * @param read_addresses    Array of 64-bit hardware address lines to look up.
     * @param address_count     Total size of the input pointer arrays (bounded by a uint8_t capacity).
     * @param byte_counts       Array containing execution read lengths matching each sequential lookup index.
     * @param return_buffers    Null terminated array of destination memory addresses receiving mapped data chunks.
     * @return CacheResult      Result of the read operations.
     * @warning The maximum number of threads must be set before calling this function, otherwise the default value of 1 will be used.
     */
    CacheResult multi_read_cache_t(Definition* def, const uint64_t* read_addresses, const uint8_t address_count, const size_t* byte_counts, const void** return_buffers);

    /**
     * @brief Executes batch-mode sequence allocations into the cache space.
     * * Sequential iterations terminate gracefully upon encountering a null array pointer wrapper boundary.
     * @param def               Pointer to the definition struct holding the configuration values for the cache library.
     * @param write_buffers     Array of pointers containing the block entries to be written.
     * @param write_count       Number of elements in the `write_buffer`
     * @return CacheResult       Result of the write operations.
     * @warning The maximum number of threads must be set before calling this function, otherwise the default value of 1 will be used.
     */
    CacheResult multi_write_cache_t(Definition* def, const uint64_t* write_buffers, int write_count);
    
    /**
     * @brief Structural compatibility check to evaluate alignment invariants.
     * * Checks if two target memory blocks resolve to identical structural cache row lines (sets).
     * @param address_1         First 64-bit data address segment pointer context.
     * @param address_2         Second 64-bit data address segment pointer context.
     * @return uint8_t 1        If both addresses map to the exact same cache set index boundary.
     * @return uint8_t 0        Otherwise.
     */
    uint8_t multi_operation_compatible_t(const uint64_t address_1, const uint64_t address_2);

    /**
     * @brief Tells the cache to execute the pending operations concurrently.
     * * Will try to multi-thread the current accesses to speed up execution time.
     * @param def               Pointer to the definition struct holding the configuration values for the cache library.
     * @return enum Error_Type  0 If all accesses were compatible and executed successfully, 
     *                          -1 If the definition struct is null,
     *                          1 If there is no return buffer for the read operations,
     *                          2 If the passkey is invalid,
     *                          3 If there is an implementation error.
     * @warning The thread-count const defined on this file must be set to the number of threads the in use cpu has or bellow.
     */
    enum Error_Type flush (Definition* def);
    
    /**
     * @brief Deletes all accesses that are on hold.
     * * This accesses will never be executed
     * @param def           Pointer to the definition struct holding the configuration values for the cache library.
     * @warning Use with caution, as it can cause data loss if there are pending write accesses on hold.
     */
    void clean (Definition* def);


// ============================================================================
// Constraint Values Definitions
// ============================================================================

/**
 * @brief Struct holding the configuration values for the cache library.
 * * This struct is used to hold the configuration values for the cache library,
 * * such as the number of threads to use and the max number of accesses to have waiting before flushing.
 */
typedef struct Definition Definition;

// ============================================================================
// Configurations Interface
// ============================================================================

    /**
     * @brief Setter for the max number of different threads to use at once.
     * @param def           Pointer to the definition struct holding the configuration values for the cache library.
     * @param thread_count  Max number of threads to run concurrently
     * @return int8_t       1 if the thread count was set successfully,
     *                      0 if the definition struct is null,
     *                      -1 if the thread count is invalid (0 or above 64)
     */
    int8_t set_thread_count (Definition* def, const uint8_t thread_count);

    /**
     * @brief Getter for the max number of different threads to use at once.
     * @param def            Pointer to the definition struct holding the configuration values for the cache library.
     * @return uint8_t       Max number of threads to run concurrently or 0 if the definition struct is null
     */
    uint8_t get_thread_count (const Definition* def);
    
    /**
     * @brief Setter for the max number of accesses pending buffer.
     * * If accesses buffer ever match the set nu,ber will automaticaly flush the buffers
     * @param def           Pointer to the definition struct holding the configuration values for the cache library.
     * @param size          Max number of accesses to have waiting
     * @return int8_t       1 if the max wait size was set successfully, 0 if the definition struct is null and -1 if the size is invalid (0 or above 128)
     */
    int8_t set_max_wait_size (Definition* def, const uint8_t size);

    /**
     * @brief Getter for the max number of accesses pending buffer.
     * @param def           Pointer to the definition struct holding the configuration values for the cache library.
     * @return uint8_t      Max number of accesses to have waiting or 0 if the definition struct is null
     */ 
    uint8_t get_max_wait_size (const Definition* def);




#endif // CACHELIB_H
