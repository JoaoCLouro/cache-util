#ifndef DIRECTCACHELIB_H
#define DIRECTCACHELIB_H

// ============================================================================
// About
// ============================================================================

// Interface for the functions that talk directly to the ASM code.
// This is the base interface for the others provided in this lib.
// Could be used to implement other costume interfaces.

// ============================================================================
// Included Libs
// ============================================================================

#include <stdint.h>
#include <stddef.h>

// ============================================================================
// Cache Configuration Constants
// ============================================================================

#define SYSTEM_ADDRESS_SIZE 64                                // bits
#define CACHE_BLOCK_SIZE    64                                // bytes
#define CACHE_WAYS          4
#define CACHE_SIZE          65536                             // bytes

#define CACHE_LINES         (CACHE_SIZE / CACHE_BLOCK_SIZE)   // 1024 lines
#define CACHE_CELL_SIZE     (CACHE_BLOCK_SIZE / CACHE_WAYS)   // 16 bytes

#define CACHE_TAG_BITS      50                                // SYSTEM_ADDRESS_SIZE - CACHE_INDEX_BITS - CACHE_OFFSET_BITS
#define CACHE_INDEX_BITS    8                                 // log2(CACHE_LINES)
#define CACHE_OFFSET_BITS   6                                 // log2(CACHE_BLOCK_SIZE)

// ============================================================================
// Provided Functions
// ============================================================================

/**
 * @brief Initializes the cache environment and sets up a randomized passkey.
 * @return uint64_t The validation address (passkey) required for read/write.
 */
uint64_t init_t (void);

/**
 * @brief Checks the cache for an address and reads its contents if present.
 * @param address       The 64-bit target address to query inside the cache (RDI).
 * @param return_buffer Pointer to the C destination buffer where data will be copied (RSI).
 * @param bytes_to_read The number of bytes to read out of the cache cell (RDX).
 * @param passkey       The key returned by init() to authorize cache access (RCX).
 * * @return uint64_t Exit status code (RAX):
 * 0 - Success
 * 1 - Address not present in cache
 * 2 - Invalid passkey
 * 3 - Cell index miscalculation error
 */
uint64_t read_cache_t (const uint64_t address, 
                       const void *return_buffer, 
                       const size_t bytes_to_read, 
                       const uint64_t passkey);

/**
 * @brief Writes a block of memory into the cache allocation workspace.
 * @param address_to_write The 64-bit address context for the data entry (RDI).
 * @param passkey          The key returned by init() to authorize cache access (RSI).
 * * @return uint64_t Exit status code (RAX):
 * 0 - Success
 * 2 - Invalid passkey
 */
uint64_t write_cache_t (const uint64_t address_to_write, 
                        const uint64_t passkey);

# endif // DIRECTCACHELIB_H
