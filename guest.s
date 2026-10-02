.code16

.global guest_code
.global guest_code_end

guest_code:
  movw $42, %ax
  movw %ax, %cs:0
  hlt

guest_code_end:
