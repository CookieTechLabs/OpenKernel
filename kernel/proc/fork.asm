; OpenKernel Fork Subsystem

BITS 32

section .text

global process_fork_context

; int32_t process_fork_context(
;     uint32_t *parent_context,
;     uint32_t *child_context
; )
;
; Arguments (cdecl):
;   [esp + 4] = parent_context
;   [esp + 8] = child_context
;
; Context layout:
;   +0  EBX
;   +4  ESI
;   +8  EDI
;   +12 EBP
;   +16 ESP
;   +20 EIP
;
; Kept as a low-level register-context helper for the process layer.

process_fork_context:
    push esi
    push edi

    mov esi, [esp + 12]
    mov edi, [esp + 16]

    mov ecx, 6
    cld
    rep movsd

    pop edi
    pop esi

    xor eax, eax
    ret

section .note.GNU-stack noalloc noexec nowrite progbits