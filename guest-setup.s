BITS 16

; Memory Layout
;   ES: 0x00000  (For IVT)
;   CS: 0x10000
;   DS: 0x20000
;   SS: 0x30000
;
;   SP: 0xfffe    (0xffff - 1) gives us an even boundary, 
;                 as stack operations operate on a 16-bit
;                 (or 2-byte) boundary.

setup_segment_registers:
  mov ax, 0x0000
  mov es, ax

  mov ax, 0x2000
  mov ds, ax

  mov ax, 0x3000
  mov ss, ax

  mov ax, 0xfffe
  mov sp, ax

  mov ax, 42
  mov ds:0, ax

setup_ivt:
  ; IVT[0]: #DE
  mov ax, DE_handler
  mov es:[0x0000], ax

  mov ax, cs
  mov es:[0x0002], ax

  jmp guest_code

DE_handler:
  mov al, 'D'
  out 0xe9, al

  mov al, 'E'
  out 0xe9, al

  hlt

guest_code:
  mov ax, 25
  mov dx, 0
  mov bx, 10
  div bx

  hlt
