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
; Unchangeable Cache details:
;
  SYSTEM_ADDRESS_SIZE EQU 64 
  CACHE_BLOCK_SIZE    EQU 64                           ; Bytes
  CACHE_WAYS EQU 4        ; Changing the number of cache cells per block requires major rework on read and write algorithms
    
; Changeable Values:
  CACHE_SIZE EQU 16777216
  CACHE_LINES       EQU (CACHE_SIZE / CACHE_BLOCK_SIZE)   ; 262144 lines
  CACHE_CELL_SIZE   EQU (CACHE_BLOCK_SIZE / CACHE_WAYS)
  
  CACHE_INDEX_BITS  EQU 16                           ; log 2 (CACHE_LINES / CACHE_WAYS)
  CACHE_OFFSET_BITS EQU 6                           ; log 2 (CACHE_BLOCK_SIZE)
  CACHE_TAG_BITS    EQU (SYSTEM_ADDRESS_SIZE - (CACHE_INDEX_BITS + CACHE_OFFSET_BITS)) ;42
; --------------------------------------------

section.rodata
; Error messages
cell_miscalc_msg:    db "cell index was miscalculated", 10
CM_MSG_LEN:          EQU $ - cell_miscalc_msg

section .data
validation_address: dq 0    ; Passkey to the cache

section .bss
align CACHE_BLOCK_SIZE
cache_validity:   resb CACHE_LINES      ; 2 bits for each cache cell (1 byte total by block)
align CACHE_BLOCK_SIZE
cache_tags:       resb (CACHE_TAG_BITS * CACHE_WAYS * CACHE_LINES)
align CACHE_BLOCK_SIZE
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
;               3 - cell index miscalculation
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
        test cl
        ; not present
        jz _not_present
        ; determine presence cell
        
        _read:
            ; gets the tag bits of the address
            call _get_tag_bits
            mov r8, rax
            
            ; tries to match the tag bits to the ones in cache
            xor rax
            _loop:
            cmp [cache_tags + rbx * (CACHE_TAG_BITS + CACHE_WAYS) + rax * CACHE_TAG_BITS], r8
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
                mov rdi, [cache_validity + rbx * 8]
                mov rsi, rax
                call _update_cells
                test rax
                jnz _cell_index_error
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
            
        _cell_index_error:
            ; writes the error msg to the std err
            mov rax, 1
            mov rdi, 2
            lea rsi, [cell_miscalc_msg]
            mov rdx, CM_MSG_LEN
            syscall
            
            ; cell miscalculation exit code
            mov rax, 3
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
        
        call _get_index_bits
        mov rbx, rax                        ; RBX holds the index position
        mov rcx, [cache_validity + rbx * 8] ; RCX holds the blocks validity address
        
        push rcx
        and rcx, 0x0f                   ; Ignores padding and decision tree bits for now
        
        cmp rcx, 0x0f
        je _take_decision
        
        call _get_tag_bits              ; RAX holds the tag bits
        
        
            
        ; update cells time usage manager
        
        ; write to the cache
        
        
        
        
        
        _take_decision:
            pop rcx     ; restores the full block info
            
            ; call the take decision function and overwrites the data
        
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
;       Updates all validity / usage parameters
;
; Inputs:
;       RDI: Base Address of the 
;            cell block validity buffer
;       RSI: Latest accessed cell
;
; Outputs:
;       RAX: Exit code: 
;               0 - success
;               1 - invalid cell number given
;
; Destroys:
;       RAX
; -------------------------------------------------
    _update_cells:
        ; cell validation
        cmp rsi, 0
        jl _invalid_cell
        cmp rsi, CACHE_WAYS
        jg _invalid_cell
        
        ; cell is valid!
        
        xor rax
        ; binary cell index to zero out convertion to index format
        push rdi
        mov rdi, rsi
        call _bin_to_index
        pop rdi
        
        ; activates the given cell usage
        or rdi, rax
        
        ; advances the address to the decision tree
        add rdi, 4
        call _decision_logic
        xor RAX
        ret
        
        
    _invalid_cell:
        ; invalid cell exit code
        mov RAX, 1
        ret
; -----------------------------------------------------------
; _bin_to_index:
;       Converts a binary number to an index representation.
;       Ex: 0011 converts to 0100 and 0100 converts to 1000
;
; Inputs:
;       RDI: binary number to convert
;
; Destroys:
;       RAX, RDI
; -------------------------------------------------
    _bin_to_index:
        xor rax
        test rdi
        jz _ret
        mov rax, 1b
        _loop:
            dec rdi
            test rdi
            jz _ret
            shl rax, 1
            jmp _loop
        _ret:
            ret

; -------------------------------------------------
; _decision_logic:
;       Updates the decision tree 
;       based on the newlly accessed cell
;
; Inputs:
;       RDI: Base Address of the decision tree 
;       RSI: Latest accessed cell
;
; Destroys:
;       RAX (does not clean it)
; -------------------------------------------------
    _decision_logic:
        cmp rsi, 2
        ja _3or4cell
      
        _1or2cell:
            ; standard cell 1 or 2 logic
            mov al, 11111001b
            and [rdi], al
            ; Verifies the need for cell 2 logic            
            cmp rsi,2
            je _2cell
            ; If was cell 1 just return
            ret
        
        _2cell:
            mov al, 00000010b
            or [rdi], al
            ret
       
        _3or4cell:
            ; standard cell 3 or 4 logic
            mov al, 00000101b
            or [rdi], al
            ; Verifies the need for cell 3 logic            
            cmp rsi,3
            je _3cell
            ; If was cell 4 just return
            ret
            
        _3cell:
            mov al, 11111110b
            and [rdi], al
            ret