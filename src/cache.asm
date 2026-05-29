; ---------------------------
;   Usefull cache sizes:
;   (Bytes)
;
; 265 MB cache:
;   268435456
;
; 64 MB cache:
;   67108864
;
; 16 MB cache:
;   16777216
;
; 256 KB cache:
;   262144
;
; 64 KB cache:
;   65536
;
; 4 KB cache: 
;   4096
; 
; ------------||-----------
;   In Use:
;
;       16MB cache
;----------------------------


; --------------------------------------------
; Cache details:
;
  SYSTEM_ADDRESS_SIZE EQU 64 
  CACHE_BLOCK_SIZE    EQU 64                           ; Bytes
  
; Changeable Values:

  CACHE_WAYS EQU 4        ; Changing the number of cache cells per block requires major rework on read and write algorithms
  CACHE_SIZE EQU 16777216

  CACHE_LINES       EQU (CACHE_SIZE / CACHE_BLOCK_SIZE)   ; 262144 lines
  CACHE_CELL_SIZE   EQU (CACHE_BLOCK_SIZE / CACHE_WAYS)
  
  CACHE_INDEX_BITS  EQU 16                           ; log 2 (CACHE_LINES / CACHE_WAYS)
  CACHE_OFFSET_BITS EQU 6                           ; log 2 (CACHE_BLOCK_SIZE)
  CACHE_TAG_BITS    EQU (SYSTEM_ADDRESS_SIZE - (CACHE_INDEX_BITS + CACHE_OFFSET_BITS)) ;42
; --------------------------------------------
  
section .data
validation_address: dq 0    ; Passkey to the cache

section .bss
align CACHE_BLOCK_SIZE
cache_validity:   resb CACHE_LINES      ; 2 bits for each cache cell (1 byte total by block)
cache_tags:       resb (CACHE_TAG_BITS * CACHE_WAYS * CACHE_LINES)
cache_buffer:     resb CACHE_SIZE

section .text
global init
global read_cache
global write_cache

; To Implement
;   WRITE CACHE



; -----------------------------------------------------
; init:
;       Returns a validation address to the cache
; Outputs:
;       RAX: address to use to access the cache
; Destoys: 
;       RAX
; -----------------------------------------------------
    init:
        ; gets a random value in the to randomize the address
        xor RAX
        call _get_rand_val
        add rax, validation_address
        mov [validation_address], rax
        ret

; -----------------------------------------------------
; read_cache:
;       Checks the cache for an address.
;       Returns the its buffer address in cache if present.
;
; Inputs:
;       RDI: Address to read for
;       RSI: Address where to give the datas address in cache
;       RDX: Number of bytes to read
;       RCX: Validation address (passkey)
; Outputs:
;       RAX: Exit code: 
;               0 - success
;               1 - address not in cache
;               2 - not valid passkey
;
;
; Destoys: 
;       RAX, RBX, RCX, R8
; -----------------------------------------------------
    read_cache:
        ; register cleaning
        xor RAX
        xor RBX
        
        ; passkey validation
        cmp rcx, validation_address
        jne _invalid_passkey
        
        ; valid passkey detected!
        xor RCX
        
        ; gets the index bits of the address
        call _get_index_bits
        mov rbx, rax
        
        ; validates the existance of the data in cache
        mov cl, [cache_validity + rbx]
        and cl, 0x0f
        cmp cl, 0
        ; not present
        je _not_present
        ; determine presence cell
        
        _read:
            ; gets the tag bits of the address
            call _get_tag_bits
            mov r8, rax
            
            ; tries to match the tag bits to the ones in cache
            xor rax
            _loop:
            cmp [cache_tags + rbx + rax], r8
            ; if equal, rax holds the cache cell position with the correct data
            je _return_data
            
            ; if not equal increment rax and validate it
            inc rax
            cmp rax, CACHE_WAYS
            je _not_present
            
            ; if still in valide range continue with the loop
            jmp _loop
            
        _return_data:
            ; gets the offset bits of the address
            call _get_offset_bits
            mov r8, rax
            ; Moves the address of the value in cache to the return buffer address
            mov rcx, [cache_buffer + rbx * CACHE_BLOCK_SIZE + rax * CACHE_CELL_SIZE + r8]
            
            xor r8
            _writting_loop:
                ; writting on the return buffer the exact number of bytes from the cache
                cmp r8, rdx
                je _loop_end
                ; If the number of bytes passed has not been reached yet continue writting
                mov byte [rsi + r8], [rcx + r8]
                inc r8
                jmp _writting_loop
            
            _loop_end:
                ; Cell usage update routine
                push rdi
                push rsi
                mov rdi, [cache_validity + rbx]
                mov rsi, rax
                call _update_cells
                pop rsi
                pop rdi
                
                ; Successful exit routine: moves the success exit code to rax
                xor RAX
                xor RBX
                xor RCX
                xor R8
                ret
            
        _not_present: 
            ; error routine
            ; address is not in the cache
            xor RCX
            xor RBX
            xor r8
            mov rax, 1
            ret
        
        _invalid_passkey:
            xor RCX
            xor RBX
            xor r8
            mov rax, 2
            ret

; -----------------------------------------------------
; write_cache:
;       Writes a value in cache
;
; Inputs:
;       RDI: Address of the value to write in cache
;       RSI: Validation address (passkey)
; Outputs:
;       RAX: Exit code: 
;               0 - success
;               1 - 
;               2 - not valid passkey
;
;
; Destoys: 
;       RAX, RBX, RCX, RDX
; -----------------------------------------------------
    write_cache:
        ; register cleaning
        xor RAX
        xor RBX
        xor RCX
        
        ; passkey validation
        cmp rsi, validation_address
        jne _invalid_passkey
        
        ; valid passkey detected!
        
        
        ; determine the oldest cell in cache
        
        
        ; update cells time usage manager
        
        ; write to the cache
        
        
        _invalid_passkey:
            mov rax, 2
            ret

; --------------------------
;   Multi purpose helpers
; --------------------------


; --------------------------------------------
; _get_index_bits:
;       Returns the index bits of an address.
;       Costume for this cache system.
;
; Inputs:
;       RDI: Base Address
;
; Outputs:
;       RAX: index bits
; --------------------------------------------
    _get_index_bits:
        mov rax, rdi
        shl rax, CACHE_TAG_BITS
        shr rax, CACHE_TAG_BITS + CACHE_OFFSET_BITS
        ret
        
; --------------------------------------------
; _get_tag_bits:
;       Returns the tag bits of an address.
;       Costume for this cache system.
;
; Inputs:
;       RDI: Base Address
;
; Outputs:
;       RAX: tag bits
; --------------------------------------------
    _get_tag_bits:
        mov rax, rdi
        shr rax, CACHE_OFFSET_BITS + CACHE_INDEX_BITS
        ret

; --------------------------------------------
; _get_tag_bits:
;       Returns the offset bits of an address.
;       Costume for this cache system.
;
; Inputs:
;       RDI: Base Address
;
; Outputs:
;       RAX: offset bits
; --------------------------------------------
    _get_offset_bits:
        mov rax, rdi
        shl rax, CACHE_TAG_BITS + CACHE_INDEX_BITS
        shr rax, CACHE_TAG_BITS + CACHE_INDEX_BITS
        ret

; --------------------------------------------
; _get_rand_val:
;       Returns a random 8 bytes value.
;       Uses a syscall to get the random 8 bytes value.
;
; Outputs:
;       RAX: random 8 bytes value
; --------------------------------------------
    _get_rand_val:
        ; saves the stack state and reserves 8 bytes
        xor rdx
        push rbp
        mov rbp, rsp
        sub rsp, 8
        ; random value syscall
        mov rax, 318        
        mov rdi, rsp        
        mov rsi, 8          
        syscall                
        ; sends that value to the return register
        mov rax, [rsp]
        ; resets the stack
        mov rsp, rbp
        pop rbp
        ret

; -------------------------------------------------
; _update_cells:
;       Increments each cache validity cell value 
;       besides the one to zero out.
;
; Inputs:
;       RDI: Base Address of the 
;            cell block validity buffer
;       RSI: Cell to zero out
; -------------------------------------------------
    _update_cells:
        push rax
        xor rax
        
        cmp rax, CACHE_WAYS
            