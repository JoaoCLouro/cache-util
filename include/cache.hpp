// Imported libs 
#include <cstdint>
#include <cstddef>
// ----------------------------------------------

// Class Interface 
class Cache
{
    public:
        
        void read (uint64_t read_address, size_t byte_count, void* return_buffer);
        void write (void* write_buffer);
        void multi_read (uint64_t* read_addresses, uint8_t address_count, size_t* byte_counts, void** return_buffers); 
        void multi_write (void** write_buffers);
        bool multi_operation_compatible (uint64_t address_1, uint64_t address_2);
};


