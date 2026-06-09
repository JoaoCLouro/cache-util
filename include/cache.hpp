// Imported libs 
#include <cstdint>
#include <cstddef>
// ----------------------------------------------

// Class Interface 
class Cache
{
    public:
    /**
     * @brief Constructor for the Cache Object wrapper.
     * Automatically executes the low-level assembly 'init' sequence 
     * and stores the generated random validation passkey internally.
     */
    Cache();

    /**
     * @brief Default Destructor.
     */
    ~Cache() = default;

    /**
     * @brief Queries the cache for an address and reads its contents if present.
     * * @param read_address   The target 64-bit address context being read.
     * @param byte_count     Number of bytes to extract from the cache data workspace.
     * @param return_buffer  Pointer to the destination memory region where data will be copied.
     * * @throws std::runtime_error if a cache miss occurs, the validation passkey fails,
     * or an internal index miscalculation takes place.
     */
    void read_cache_t(uint64_t read_address, std::size_t byte_count, void* return_buffer);

    /**
     * @brief Writes a block of data into the underlying cache environment.
     * * @param write_buffer   Pointer to the data source being mapped into cache. 
     * The pointer's absolute address value is tracked as the target lookup index.
     * * @throws std::runtime_error if the verification security passkey is rejected.
     */
    void write_cache_t(void* write_buffer);
    
    /**
     * @brief Executes batch-mode sequence reads against multiple cache targets.
     * * @param read_addresses Array of 64-bit hardware address lines to look up.
     * @param address_count  Total size of the input pointer arrays (bounded by a uint8_t capacity).
     * @param byte_counts    Array containing execution read lengths matching each sequential lookup index.
     * @param return_buffers Array of destination memory addresses receiving mapped data chunks.
     */
    void multi_read_cache_t(uint64_t* read_addresses, uint8_t address_count, std::size_t* byte_counts, void** return_buffers);

    /**
     * @brief Executes batch-mode sequence allocations into the cache space.
     * * Sequential iterations terminate gracefully upon encountering a null array pointer wrapper boundary.
     * * @param write_buffers  Null-terminated array of pointers containing the block entries to be written.
     */
    void multi_write_cache_t(void** write_buffers);
    
    /**
     * @brief Structural compatibility check to evaluate alignment invariants.
     * * Checks if two target memory blocks resolve to identical structural cache row lines (sets) 
     * using the formula: (address >> 6) & 0xFF.
     * * @param address_1      First 64-bit data address segment pointer context.
     * @param address_2      Second 64-bit data address segment pointer context.
     * @return true          If both addresses map to the exact same cache set index boundary.
     * @return false         Otherwise.
     */
    bool multi_operation_compatible_t(uint64_t address_1, uint64_t address_2);

private:
    uint64_t asm_passkey; ///< Protected authorization passkey required to interact with the raw assembly routines.
};


