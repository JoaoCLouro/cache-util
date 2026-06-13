; Standard used
default rel

; -- | Functions Provided | --
global init
global read_cache
global write_cache
; -- || ---------------------

; ---------------------------
;   Useful cache sizes:
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
;       64KB cache
;----------------------------


; --------------------------------------------
; Unchangeable Cache details:
;
  SYSTEM_ADDRESS_SIZE EQU 64 
  CACHE_BLOCK_SIZE    EQU 64                           ; Bytes
  CACHE_WAYS EQU 4        ; Changing the number of cache cells per block requires major rework on read and write algorithms
  ; Mask to use to clean a tag from its buffer (Needs update the tags bits const is changed)
  TAG_ENTRY_CLEANING_MASK EQU 0xfffc000000000000
  
; Changeable Value:
  CACHE_SIZE EQU 65536
  
  CACHE_LINES       EQU (CACHE_SIZE / CACHE_BLOCK_SIZE)   ; 1024 lines
  CACHE_CELL_SIZE   EQU (CACHE_BLOCK_SIZE / CACHE_WAYS)
  CACHE_INDEX_BITS  EQU 8                           ; log 2 (CACHE_LINES / CACHE_WAYS)
  CACHE_OFFSET_BITS EQU 6                           ; log 2 (CACHE_BLOCK_SIZE)
  CACHE_TAG_BITS    EQU (SYSTEM_ADDRESS_SIZE - (CACHE_INDEX_BITS + CACHE_OFFSET_BITS)) ; 50
; --------------------------------------------

section .rodata
; Error messages
cell_miscalc_msg:    db "cell index was miscalculated", 10
CM_MSG_LEN:          EQU $ - cell_miscalc_msg

section .data
validation_address: dq 0    ; Passkey to the cache

section .bss
align CACHE_BLOCK_SIZE
cache_validity:   resb CACHE_LINES                          ; 2 bits for each cache cell (1 byte total by block)
align CACHE_BLOCK_SIZE
cache_tags:       resq (CACHE_WAYS * CACHE_LINES)           ; reserves 8 bytes for each tag (could be optimized) 
align CACHE_BLOCK_SIZE
cache_buffer:     resb CACHE_SIZE                                           

section .text

; -----------------------------------------------------
; init:
;       Returns a validation address to the cache
; Outputs:
;       RAX: address to use to access the cache
; Destroys: 
;       RAX
; -----------------------------------------------------
    init:
        ; gets a random value in the to randomize the address
        call _get_rand_val
        shr rax, 1
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
;       RSI: Return buffer
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
; Destroys: 
;       RAX, RBX, RCX, R8
; -----------------------------------------------------
    read_cache:        
        ; Passkey validation
        cmp rcx, validation_address
        jne _invalid_passkey
        
        ; Valid passkey detected!      
        ; Gets the index bits of the address
        call _get_index_bits
        mov rbx, rax                    ; RBX holds the index bits
        
        ; Validates the existence of the data in cache
        mov cl, [cache_validity + rbx]
        and cl, 0x0f
        test cl, 0
        ; Not present
        je _not_present
        
        _read:
            ; Determine presence cell
            
            ; Gets the tag bits of the address
            call _get_tag_bits          ; r8 holds the tag bits
            mov r8, rax
            
            push rdi
            push rsi
            
            mov rdi, rbx        ; RDI holds the index bits
            mov rsi, r8         ; RSI holds the tag bits
            call _tag_exists_in_cache

            cmp rax, 0
            je _not_present
           
        _return_data:
            ; Preserve the cache cell number matched
            push rax
            ; Gets the offset bits of the address
            call _get_offset_bits
            mov r8, rax
            pop rax
            ; Moves the address of the value in cache to the return buffer address
            
            ; Address simplification 
            push rax
            imul rax, CACHE_CELL_SIZE
            add r8, rax
            
            mov rax, rbx
            imul rax, CACHE_BLOCK_SIZE
            add r8, rax
            pop rax
            ; Final address displacement is all in r8
            mov rcx, [cache_buffer + r8]
            
            pop rdi           ; RDI holds the return address
            mov rsi, rcx      ; RSI holds the read address
            push rax
            call _write_to_address
            pop rax
            
        _loop_end:
            ; Updates cell decision tree      
            push rdi    
            mov rdi, [cache_validity + rbx * 8]     ; RDI holds the block's validity buffer address
            mov rsi, rax                            ; RSI holds the latest accessed cell 
            call _update_cells
            test rax, 0
            jne _cell_index_error
            pop rsi
            pop rdi
                
            ; Successful exit routine: moves the success exit code to rax
            xor RAX, RAX
            ret
            
        _not_present: 
            ; Error routine
            ; Address is not in the cache
            mov rax, 1
            ret
        
        _invalid_passkey:
            mov rax, 2
            ret
            
        _cell_index_error:
            ; writes the error msg to the std err
            mov rax, 1
            mov rdi, 2
            lea rsi, [cell_miscalc_msg]
            mov rdx, CM_MSG_LEN
            syscall
            
            ; Cell miscalculation exit code
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
;               2 - not valid passkey
;
;
; Destroys: 
;       RAX, RBX, RCX, RDX. RSI
; -----------------------------------------------------
    write_cache:     
        ; Passkey validation
        cmp rsi, validation_address
        jne _invalid_passkey
        
        ; Valid passkey detected!
        
        _determine_address_existence_in_cache:
            ; Determines if the address is already in cache
            
            call _get_index_bits
            mov rbx, rax                        ; RBX holds the index position
            
            mov rcx, [cache_validity + rbx * 8] ; RCX holds the blocks validity address
            
            call _get_tag_bits
            mov rsi, rax                        ; RSI holds the tag bits
            
            call _get_offset_bits
            mov r8, rax                     ; R8 holds the offset bits
            
            ; Verifies if the tag is already written in the cache at the correct line
            ; If so, the address was already written into the cache
            
            push rdi
            mov rdi, rbx                        ; RDI holds the index bits
            call _tag_exists_in_cache           ; RAX holds the cell number or 0
            cmp rax, 0
            ; 0 - not in cache
            je _decide_and_write
            
        _write_and_update:
            ; If enters, the address was already in the cache
            
            ; Rewrites data in cache (might be updated data)
            
            ; Address simplification
            push rax
            imul rax, CACHE_CELL_SIZE
            mov rdx, rax
            
            mov rax, rbx
            imul rax, CACHE_BLOCK_SIZE
            add rdx, rax            
            pop rax
            ; Final address displacement is all in rdx
            mov rsi, [cache_buffer + rdx]  
            pop rdi 
            xchg rsi, rdi               ; RDI holds the write address && RSI holds the address to read from 
            mov rdx, CACHE_BLOCK_SIZE   ; RDX holds the number of bytes to write
            call _write_to_address
            ; Updates the decision tree
            mov rdi, rcx                                                                    ; RDI holds the block validity address
            mov rsi, rax                                                                    ; RSI holds the cache cell number 
            call _update_cells  
                
        _exit:
            ; Exit routine
            xor RAX, RAX
            ret
        
        _decide_and_write:
            ; Determine if there is any empty cell
            mov rdi, rcx    ; RDI holds the block validity address
            call _any_cell_empty
            
            cmp rax, 0  ; if equal all are full
            je _decide_cell_overwrite
            
            _write_cell:
                ; Cleaning tag entry
                
                ; Address simplification
                push rax
                imul rax, 8
                mov rdi, rax
                
                mov rax, rbx
                imul rax, 8
                imul rax, CACHE_WAYS
                
                add rax, rdi
                ; Final address displacement is all in rax                
                mov rdi, [cache_tags + rax]
                mov rax, TAG_ENTRY_CLEANING_MASK
                and rdi, rax
                ; Writing new tag entry
                or rdi, rsi
                ; writing the data into the buffer and updating the validity buffer
                pop rax
                jmp _write_and_update
            
            _decide_cell_overwrite:
            ; If not decide what cell to rewrite and rewrite it
            call _decide_cell   ; RAX holds the cell to update
            jmp _write_cell
            

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
        xor rdx, rdx
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
        
        xor rax, rax
        ; binary cell index to zero out conversion to index format
        push rdi
        mov rdi, rsi
        call _bin_to_index
        pop rdi
        
        ; activates the given cell usage
        or rdi, rax
        
        ; advances the address to the decision tree
        add rdi, 4
        call _decision_logic
        xor RAX, RAX
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
        xor rax, rax
        cmp rdi, 0
        je _ret
        mov rax, 1b
        _loop:
            dec rdi
            cmp rdi, 0
            je _ret
            shl rax, 1
            jmp _loop
        _ret:
            ret

; -------------------------------------------------
; _decision_logic:
;       Updates the decision tree 
;       based on the newly accessed cell
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
        ja _update_3or4cell
      
        _update_1or2_cell:
            ; standard cell 1 or 2 logic
            mov al, 11111001b
            and [rdi], al
            ; Verifies the need for cell 2 logic            
            cmp rsi,2
            je _update_2cell
            ; If was cell 1 just return
            ret
        
        _update_2cell:
            mov al, 00000010b
            or [rdi], al
            ret
       
        _update_3or4cell:
            ; standard cell 3 or 4 logic
            mov al, 00000101b
            or [rdi], al
            ; Verifies the need for cell 3 logic            
            cmp rsi,3
            je _update_3cell
            ; If was cell 4 just return
            ret
            
        _update_3cell:
            mov al, 11111110b
            and [rdi], al
            ret

; --------------------------------------------------
; _tag_exists_in_cache:
;       Verifies if the tag already exists
;       in cache at the specific index line
;
; Inputs:
;       RDI: Index bits to match
;       RSI: Tag bits to match
;
; Outputs:
;       RAX: Cell number match or 0 if not in cache
; --------------------------------------------------
    _tag_exists_in_cache:
        ; tries to match the tag bits to the ones in cache
            xor rax, rax
            push rdi
            ; Address simplification
            imul rdi, 8
            imul rdi, CACHE_WAYS
            add rdi, cache_tags
            _tag_detection_loop:
                
                cmp qword [rdi + rax * 8], rsi
                ; if equal, rax holds the cache cell position with the correct data
                je _tag_loop_end
            
                ; if not equal increment rax and validate it
                inc rax
                cmp rax, CACHE_WAYS
                je _tag_not_present
            
                ; if still in valid range continue with the loop
                jmp _tag_detection_loop

            _tag_loop_end:
                pop RDI
                inc RAX
                ret            
            
            _tag_not_present:
                pop RDI
                xor RAX, RAX
                ret
                
; --------------------------------------------------
; _write_to_address:
;       Writes data in memory
;
; Inputs:
;       RDI: Address where to write
;       RSI: Address where to read data from
;       RDX: Number of bytes to write
;
; Outputs:
;       RAX: Number of bytes written
; --------------------------------------------------    
    _write_to_address:
        push rcx
        xor rax, rax
        _writing_loop:
                ; writing on the return buffer the exact number of bytes 
                cmp rax, rdx
                je _writing_loop_end
                ; If the number of bytes passed has not been reached yet continue writting
                mov cl, [rsi + rax]
                mov byte [rdi + rax], cl
                inc rax
                jmp _writing_loop
        _writing_loop_end:
            ret

; ------------------------------------------------------
; _any_cell_empty:
;       Verifies and returns the value of
;       the first empty cell on a block or
;       0 if none are empty
;
; Inputs:
;       RDI: Address to the blocks validity buffer byte
;
; Outputs:
;       RAX: Number of the first empty cell or 0
; -------------------------------------------------------   
    _any_cell_empty:
        push RDI
        push RSI
        xor RSI, RSI ; Helper for bitwise comparison 
        xor RAX, RAX ; Cell number identifier (0-3)
        
        _comparison_loop:
            mov rsi, rdi
            and rsi, 00000001b
            
            cmp rsi, 0
            je _return_cell_number
            
            inc rax
            cmp rax, CACHE_WAYS
            je _ret_none
            
            shr rdi, 1
            jmp _comparison_loop
        
        _return_cell_number:
            inc RAX     ; (1-4) based result
            pop RSI
            pop RDI
            ret
        
        _ret_none:
            xor RAX, RAX
            pop RSI
            pop RDI
            ret

; ------------------------------------------------------
; _decide_cell:
;       Implement the decision logic and
;       returns the cell number to rewrite.
;       Specific to a 4 way cache
;       (if the cache ways are ever changed needs
;       new implementation)
;
; Inputs:
;       RDI: Address to the blocks validity buffer byte
;
; Outputs:
;       RAX: Number of the cell to rewrite
; -------------------------------------------------------   
    _decide_cell:
        push RDI
        push RSI
        shr rdi, 4  ; RDI holds the decision tree base
        
        _top_node:
            mov rsi, rdi
            and rsi, 100b
            cmp rsi, 0
            je _1or2_cell
            
        _3or4_cell:
            mov rsi, rdi
            and rsi, 1b
            cmp rsi, 0
            je _3_cell
            
            _4_cell:
                mov rax, 4
                jmp _ret_cell
            _3_cell:
                mov rax, 3
                jmp _ret_cell

        _1or2_cell:
            mov rsi, rdi
            and rsi, 10b
            cmp rsi, 0
            je _1_cell
            
            _2_cell:
                mov rax, 2
                jmp _ret_cell
            _1_cell:
                mov rax, 1
                jmp _ret_cell
        
        _ret_cell:
            pop RSI
            pop RDI
            ret