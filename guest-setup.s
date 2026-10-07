BITS 16

; Memory Layout
; 0x00000 - 0x0ffff >> System DS
;   0x000 - 0x3ff -> IVT
;
; 0x10000 - 0x1ffff >> System CS
; 0x20000 - 0x2ffff >> System SS
;   SP: 0x0fffe    (0x0fffe - 1) gives us an even boundary, 
;                  as stack operations operate on a 16-bit
;                  (or 2-byte) boundary.
;
; 0x30000 - 0x3ffff >> Scratch space where the vmm will copy guest app code for the program loader.
; 0x40000 - 0xfffff >> Available to applications, made accessible by the allocator.

setup_segment_registers:
  mov ax, 0x0000
  mov ds, ax        ; Data Segment

  mov ax, 0x2000
  mov ss, ax        ; Stack Segment

  mov ax, 0xfffe
  mov sp, ax        ; Stack Pointer

  mov ax, 42
  mov ds:0, ax