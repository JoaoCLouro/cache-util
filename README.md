# Low-Level Customizable Multi-Purpose Cache Util

## Author: João Carrilho Louro

### Modules

#### Cache module

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

#### Usage / Validation buffer

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

##### LRU (Least Recently Used) Choice Tree

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

##### Presence Validation

This function could either be done by the choice tree or the presence validation section. The decision is totally up to the developer. In the current state of the cache it is done by the presence validation section, where 4 bits (one for each cell) hold the state of each cell, using a '1' for filled and '0' for empty.

#### Data storage and validation

Data storage is done using the cache_buffer declared in bss. It should be declared with the exact amount of bytes to be used. The identification of the address to read from the cache is done using the tags buffer as it is in a standard cache.


#### Module Flexibility

This particular module enables some changes to constant values (in the changeable section), mainly (and the most usefull) the cache size. The current default value for the cache size gives a large enought cache for most usecases but if you need a larger cache this also enables for multi-cache usage throught different objects (using the provided interfaces). If you require a single cache and therfore you want to increase its size, you just have to insert the number of bytes you want in the **CACHE_SIZE** constant definition. **You should always choose a value of a natural power of 2!!!**

Besides cache size you can also change the number of **cache ways**, **despite being highly not recommended** because it would require major rework in some of the core logic and data structures.
**Caches block size** byte number was selected to match most cpus internal to maximize performance, so it is also not recommended to be changed.


---


