# Low-Level Customizable Multi-Purpose Cache Util

## Author: João Carrilho Louro

### Design Choices

#### Usage / Validation buffer

The cache usage buffer is devided into 3 section, per each block of the cache:

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

##### LRU (Least Recently Used) Choise Tree

The tree bits used for the choice tree follow the following pattern for decision-making:

```txt
               [  B2  ]             <-- Top node: Chooses Left or Right pair
              /        \
       [  B1  ]        [  B0  ]       <-- Bottom nodes: Choose specific Cell
       /      \        /      \
    Cell 0   Cell 1  Cell 2  Cell 3
```

This tree follows a traditional binary tree pattern holding the least recently used cell number of the block.
This allows for easier management of what cell to rewrite when needed. An empty CT (Choice tree) will direct always to the first cell on the block.

(To Be Continued)


##### Presence Validation

This function could either be done by the choice tree or the presence validation section. The decision is totaly up to the developer. In the current state of the cache it is done by the presence validation section, where 4 bits (one for each cell) hold the state of each cell, using a '1' for filled and '0' for empty.



#### Data storage and validation

Data storage is done using the cache_buffer declared in bss. It should be declared with the exacte amount of bytes to be used. The identification of the address to read from the cache is done using the tags buffer as it is in a standard cache.
