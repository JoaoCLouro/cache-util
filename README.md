# Low-Level Customizable Multi-Purpose Cache Util

## Author: João Carrilho Louro

---

### **Modules**

#### -> **Cache module**

This is the main engine of this project. It is totally built in **assembly x86 64 bit, Intel syntax**. It follows **System V amd 64 ABI** function call convection to be as compatible as possible with other programs. This module defines all data buffers for the tag, validation and actual data, as well as all read, write and initialization functions that interact with them.

##### API

```txt
+-------------------------------------------------------------------+
  Function Name |          Inputs           |         Outputs       
---------------------------------------------------------------------
     init       |           None            | Caches costume passkey
---------------------------------------------------------------------
                | RDI: Target address       | RAX: Exit code        
                | RSI: Return buffer         | 0- successful exit   
   read_cache   | RDX: Byte count to read   | 1- cache miss         
                | RCX: Passkey              | 2- invalid passkey    
                |                           | 3- cell miscalculation
---------------------------------------------------------------------
                | RDI: Address of value     | RAX: Exit code:       
   write_cache  |       the data to write   |   0 - successful exit 
                | RSI: Passkey              |   2 - invalid passkey 
+-------------------------------------------------------------------+
```

### Design Choices

#### -> **Usage/Validation buffer**

The cache usage buffer is divided into 3 section, per each block of the cache:

```txt
+-------------------------------------------------------------------+
| Bit 7 (Unused) | B2 | B1 | B0 | Valid3 | Valid2 | Valid1 | Valid0 | < Address start 
+-------------------------------------------------------------------+
  \____________/   \__________/   \_________________________________/
     Padding        3 Tree Bits        4 Individual Validity Bits
                    for Age Ranks         (One for each Cell)
```

Each byte on the buffer refers to a block of the cache. Each byte is then devisable into this 3 sections:

- A **padding section** with an unused bit
- A **LRU choice tree** section
- A **presence validation** section for each cell
  
#### -> **LRU (Least Recently Used) Choice Tree**

The tree bits used for the choice tree follow the following pattern for decision-making:

```txt
               [  B2  ]             <-- Top node: Chooses Left or Right pair
              /        \
       [  B1  ]        [  B0  ]       <-- Bottom nodes: Choose specific Cell
       /      \        /      \
    Cell 1   Cell 2  Cell 3  Cell 4
```

This tree follows a traditional binary tree pattern that compares two bits at the time for the least recently used. At any given node a 0 or a 1 is used to refer to the cell least recently used between them. Bit 0 of the tree compares between cell 4 and 3, bit 1 between 2 and 1, and bit 0 between the least recently used between the previous two.
You specify the decision to take on any node attributing to that bit a '0' for left and a '1' for right.
This allows for easier management of what cell to rewrite when needed. An empty CT (Choice tree) will direct always to the first cell on the block.

#### -> **Presence Validation**

This function could either be done by the choice tree or the presence validation section. The decision is totally up to the developer. In the current state of the cache it is done by the presence validation section, where 4 bits (one for each cell) hold the state of each cell, using a '1' for filled and '0' for empty.

#### -> **Data storage and validation**

Data storage is done using the cache_buffer declared in bss. It should be declared with the exact amount of bytes to be used. The identification of the address to read from the cache is done using the tags buffer as it is in a standard cache.

#### -> **Module Flexibility**

This particular module enables some changes to constant values (in the changeable section), mainly (and the most useful) the cache size. The current default value for the cache size gives a large enough cache for most use cases but if you need a larger cache this also enables for multi-cache usage through different objects (using the provided interfaces). If you require a single cache and therefore you want to increase its size, you just have to insert the number of bytes you want in the **CACHE_SIZE** constant definition. **You should always choose a value of a natural power of 2!!!**

Besides cache size you can also change the number of **cache ways**, **despite being highly not recommended** because it would require major rework in some of the core logic and data structures.
**Caches block size** byte number was selected to match most CPUs internal to maximize performance, so it is also not recommended to be changed.

#### -> **Potential improvements**

- Tags:  
    As of right now, the tags buffer is using 8 full bytes for each tag. Each tag only uses 50 bits per each address so there is a 16 bit waste per entry on the cache. It gets quite substantial when you consider the size of the cache. This means there is a lot of space wasted.
    For now, for simplicity reasons, it will stay this way. The tag detections and writing algorithms are considerably easier this way, but **suggestions and improvements are welcome**.

---

### **Interfaces**

There are currently two interfaces in construction:

- A **C** procedural interface oriented to low level performance code with integration with other technologies and languages;
- A **C++** object oriented interface for low level systems / high performance c++ code;

#### -> **C Interface**

The C interface is split across two headers: a **direct interface** for single accesses, and a **higher-level interface** for batched, multi-threaded accesses built on top of it.

##### `direct_cache_interface.h`

Thin wrapper around the raw assembly functions, same inputs/outputs, just called with a normal C function signature instead of fixed registers.

```txt
+-------------------------------------------------------------------------------------------------------------+
  Function                                                                    |         Returns
------------------------------------------------------------------------------------------------------------
  uint64_t init_t (void)                                                     |  passkey (uint64_t)
------------------------------------------------------------------------------------------------------------
  uint64_t read_cache_t (uint64_t address, void *return_buffer,              |  0 - success
                         size_t bytes_to_read, uint64_t passkey)             |  1 - cache miss
                                                                              |  2 - invalid passkey
                                                                              |  3 - cell miscalculation
------------------------------------------------------------------------------------------------------------
  uint64_t write_cache_t (uint64_t address_to_write, uint64_t passkey)       |  0 - success
                                                                              |  2 - invalid passkey
+-------------------------------------------------------------------------------------------------------------+
```

**Things to be aware of:**

- `write_cache_t` doesn't take the data to write as a separate parameter - the value stored is whatever lives at `address_to_write` itself. The address you write **is** the data source.
- `bytes_to_read` is silently clamped to `CACHE_CELL_SIZE` (16 bytes). A cell only holds that many bytes, so asking for more doesn't get you more data - it gets you the first 16 bytes and nothing past that, rather than spilling into a neighboring cell's unrelated cached value.
- Every call needs the `passkey` returned by `init_t()`. There's currently one global passkey per process, not one per `Definition*` - see `cache.h` below for where multiple concurrent caches come in.

##### `cache.h`

Adds batching and multi-threading on top of the direct interface. You get a `Definition*` handle per logical cache configuration, queue up several reads/writes on it, and explicitly `flush()` to actually execute them (possibly across several threads at once).

```txt
+-------------------------------------------------------------------------------------------------------------+
  Function                                                                    |         Returns
------------------------------------------------------------------------------------------------------------
  Definition* m_init_t (uint64_t passkey)                                    |  Definition* handle
------------------------------------------------------------------------------------------------------------
  CacheResult multi_read_cache_t (Definition* def,                          |  CacheResult (see below)
                                   const uint64_t* read_addresses,
                                   uint8_t address_count,
                                   const size_t* byte_counts,
                                   const void** return_buffers)
------------------------------------------------------------------------------------------------------------
  CacheResult multi_write_cache_t (Definition* def,                         |  CacheResult (see below)
                                    const uint64_t* write_buffers,
                                    int write_count)
------------------------------------------------------------------------------------------------------------
  enum Error_Type flush (Definition* def)                                    |  SUCCESS / CACHE_MISS /
                                                                              |  INVALID_PASSKEY /
                                                                              |  IMPLEMENTATION_ERROR /
                                                                              |  NO_RETURN_BUFFER
------------------------------------------------------------------------------------------------------------
  void clean (Definition* def)                                               |  (none) - discards pending
                                                                              |  accesses without running them
------------------------------------------------------------------------------------------------------------
  uint8_t multi_operation_compatible_t (uint64_t address_1,                 |  1 - same cache set (conflict)
                                         uint64_t address_2)                 |  0 - different sets, safe
                                                                              |  to queue together
+-------------------------------------------------------------------------------------------------------------+
```

Plus a set of getters/setters - `set_thread_count` / `get_thread_count` and `set_max_wait_size` / `get_max_wait_size` - to configure how many threads a batch can use and how many pending accesses can queue up before an automatic flush.

**`CacheResult`**, returned by both `multi_read_cache_t` and `multi_write_cache_t`, is a tagged union (`Result<T, E>`-style):

```c
CacheResult result = multi_read_cache_t(def, addrs, count, byte_counts, buffers);
if (result.is_ok) {
    uint8_t completed  = result.value.ok.operations_completed;
    size_t  bytes_read = result.value.ok.total_bytes;      // 0 for writes
} else {
    enum Error_Type code   = result.value.err.code;
    uint64_t failed_address = result.value.err.failed_address;
}
```

**Things to be aware of:**

- **Queueing is not executing.** `multi_read_cache_t`/`multi_write_cache_t` only queue accesses into `def`'s internal buffer (or auto-flush if the queue fills up mid-call). If everything fits, nothing actually runs against the cache until you call `flush(def)` yourself - `return_buffers` won't have real data in them until after that call succeeds.
- **`byte_counts` is per-address**, matching `read_addresses` index-for-index - it's not a single shared count for the whole batch, and it's still subject to the same `CACHE_CELL_SIZE` clamp as the direct interface.
- **A queued address that collides with one already in the batch** (same cache set - see `multi_operation_compatible_t`) doesn't get queued; the function queues everything it safely can, and it's on you to check the returned count/`CacheResult` against what you asked for rather than assuming the whole batch was accepted.
- **On a fatal error during `flush()`**, every address that hadn't been dispatched yet (not just the one that failed) is recorded rather than silently dropped, so nothing in a batch disappears without a trace - but only the *first* failure is surfaced directly through `CacheResult`'s `err` fields.
- **There's currently no teardown function** for a `Definition*` - `clean()` only discards pending (not-yet-flushed) accesses, it doesn't free `def` itself. Keep that in mind for long-running processes that create many `Definition`s.

---

## License

MIT