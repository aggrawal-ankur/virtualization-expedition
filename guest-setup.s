BITS 16

; Memory Layout
;   CS: 0x10000
;   DS: 0x20000
;   SS: 0x30000
;
;   SP: 0xfffe    (0xffff - 1) gives us an even boundary, 
;                 as stack operations operate on a 16-bit
;                 (or 2-byte) boundary.

start:
  mov ax, 0x2000
  mov ds, ax

  mov ax, 0x3000
  mov ss, ax

  mov ax, 0xfffe
  mov sp, ax

  mov ax, 42
  mov ds:0, ax

  hlt
