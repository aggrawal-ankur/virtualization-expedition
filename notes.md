# Intel SDM Volume 3

| # | Chapter | Page Count |
| - | ------- | ---------- |
| 1 | Chapter 26 : Introduction to virtual machine extensions | 3 pages |
| 2 | Chapter 27 : Virtual machine control structures | 34 pages |
| 3 | Chapter 28 : VMX non-root operation | 27 pages |
| 4 | Chapter 29 : VM Entries | 29 pages |
| 5 | Chapter 30 : VM Exits | 35 pages |
| 6 | Chapter 31 : VMX support for address translation | 25 pages |
| 7 | Chapter 32 : APIC virtualization and virtual interrupts | 18 pages |
| 8 | Chapter 33 : VMX instruction reference | 30 pages |
| | | := 201 Pages |

# Chapter 26

Total Pages: 3
Sessions: 1

## 26.2 VIRTUAL MACHINE ARCHITECTURE

Virtual-machine extensions define processor-level support for virtual machines on Intel processors.

Two principal classes of software are supported:

1. **Virtual-machine monitors (VMM)**
   - A VMM acts as a host and has full control of the processor(s) and other platform hardware.
   - A VMM presents guest software with an abstraction of a virtual processor and allows it to execute directly on a logical processor.
   - A VMM is able to retain selective control of processor resources, physical memory, interrupt management, and I/O.

2. **Guest software**
   - Each virtual machine (VM) is a guest software environment that supports a stack consisting of operating system (OS) and application software. Each operates independently of other virtual machines and uses on the same interface to processor(s), memory, storage, graphics, and I/O provided by a physical platform.
   - The software stack acts as if it were running on a platform with no VMM. Software executing in a virtual machine must operate with reduced privilege so that the VMM can retain control of platform resources.

## 26.3 INTRODUCTION TO VMX OPERATION

Processor support for virtualization is provided by a form of processor operation called **VMX operation**.

There are two kinds of VMX operation: **VMX root operation** and **VMX non-root operation**. In general, a VMM will run in VMX root operation and guest software will run in VMX non-root operation.

Transitions between VMX root operation and VMX non-root operation are called **VMX transitions**. There are two kinds of VMX transitions.
  - Transitions into VMX non-root operation are called **VM entries**.
  - Transitions from VMX non-root operation to VMX root operation are called **VM exits**.

There is no software-visible bit whose setting indicates whether a logical processor is in VMX non-root operation. This fact may allow a VMM to prevent guest software from determining that it is running in a virtual machine.

The VMX operation places restrictions even on software running with current privilege level (CPL) 0. That means, the guest software can run at the privilege level for which it was originally designed. This capability may simplify the development of a VMM.

### Processor Behavior in VMX Operations

Processor behavior in VMX root operation is very much as it is outside VMX operation. The principal differences are that a set of new instructions (the VMX instructions) is available and that the values that can be loaded into certain control registers are limited.

Processor behavior in VMX non-root operation is restricted and modified to facilitate virtualization.
  - Certain instructions and events cause VM exits to the VMM, instead of their ordinary operation.
  - Because these VM exits replace ordinary behavior, the functionality of software in VMX non-root operation is limited. It is this limitation that allows the VMM to retain control of processor resources.

## 26.4 LIFE CYCLE OF VMM SOFTWARE

1. Software enters VMX operation by executing a `VMXON` instruction.

2. Using VM entries, a VMM can then enter guests into virtual machines (one at a time). The VMM effects a VM entry using instructions `VMLAUNCH` and `VMRESUME`; it regains control using VM exits.

3. VM exits transfer control to an entry point specified by the VMM. The VMM can take action appropriate to the cause of the VM exit and can then return to the virtual machine using a VM entry.

4. Eventually, the VMM may decide to shut itself down and leave VMX operation. It does so by executing the `VMXOFF` instruction.

## 26.5 VIRTUAL-MACHINE CONTROL STRUCTURE

VMX non-root operation and VMX transitions are controlled by a data structure called a virtual-machine control structure (VMCS).

Access to the VMCS is managed through a component of the processor state called, **the VMCS pointer**. It is one per logical processor.
  - The value of the VMCS pointer is the 64-bit address of the VMCS.
  - The VMCS pointer is read and written using the instructions `VMPTRST` and `VMPTRLD`. The VMM configures a VMCS using the `VMREAD`, `VMWRITE`, and `VMCLEAR` instructions.

A VMM could use a different VMCS for each virtual machine that it supports. For a virtual machine with multiple logical processors (virtual processors), the VMM could use a different VMCS for each virtual processor.

## 26.6 DISCOVERING SUPPORT FOR VMX

Before system software enters into VMX operation, it must discover the presence of VMX support in the processor.

System software can determine whether a processor supports VMX operation using `CPUID`. If `CPUID.01H:ECX.VMX[5]` = 1, then VMX operation is supported.

## 26.7 ENABLING AND ENTERING VMX OPERATION

Before system software can enter VMX operation, it enables VMX by setting `CR4.VMXE[bit 13]` = 1.
  - VMX operation is then entered by executing the `VMXON` instruction.
  - `VMXON` causes an invalid-opcode exception (#UD) if executed with `CR4.VMXE` = 0.
  - Once in VMX operation, it is not possible to clear `CR4.VMXE`.
  - System software leaves VMX operation by executing the `VMXOFF` instruction. CR4.VMXE can be cleared outside of VMX operation after executing of `VMXOFF`.

Before executing `VMXON`, the software should allocate a naturally aligned 4-KByte region of memory that a logical processor may use to support VMX operation. This region is called the **VMXON region**. The address of the VMXON region (the VMXON pointer) is provided in an operand to VMXON.

# Chapter 27

Total Pages: 34
Sessions:

## 27.1 OVERVIEW

A logical processor uses virtual-machine control data structures (VMCSs) while it is in VMX operation.
  - These data structures manage transitions into and out of VMX non-root operation (VM entries and VM exits) as well as processor behavior in VMX non-root operation.
  - This structure is manipulated by the new instructions `VMCLEAR`, `VMPTRLD`, `VMREAD`, and `VMWRITE`.

A logical processor associates a region in memory with each VMCS. This region is called **the VMCS region**.
  - Software references a specific VMCS using the 64-bit physical address of the region (a VMCS pointer). 
  - VMCS pointers must be aligned on a 4-KByte boundary (bits 11:0 must be zero).

---

A VMM can use a different VMCS for each virtual machine that it supports. For a virtual machine with multiple logical processors (virtual processors), the VMM can use a different VMCS for each virtual processor.

A logical processor may maintain a number of VMCSs that are **active**. The processor may optimize VMX operation by maintaining the state of an **active VMCS** in memory, on the processor, or both. 
  - At any given time, at most one of the active VMCSs is the **current VMCS**. This document frequently uses the term "the VMCS" to refer to the current VMCS.
  - The `VMLAUNCH`, `VMREAD`, `VMRESUME`, and `VMWRITE` instructions operate only on the **current VMCS**.

The points below describe how a logical processor determines which VMCSs are active and which is current:

1. The memory operand of the `VMPTRLD` instruction is the address of a VMCS. After this instruction is executed, the VMCS in it is both active and current on the logical processor it was executed on. Any other VMCS that had been active remains so, but no other VMCS is current.

2. The memory operand of the `VMCLEAR` instruction is also the address of a VMCS. After execution of the instruction, that VMCS is neither active nor current on the logical processor. If the VMCS had been current on the logical processor, the logical processor no longer has a current VMCS.

3. The **VMCS link pointer** field in the current VMCS is itself the address of a VMCS. If VM entry is performed successfully with the 1-setting of the "VMCS shadowing" VM-execution control, the VMCS referenced by the VMCS link pointer field becomes active on the logical processor. The identity of the current VMCS does not change.

The `VMPTRST` instruction stores the address of the logical processor's current VMCS into a specified memory location. It stores the value `FFFFFFFF_FFFFFFFFH` if no current VMCS is found.

---

The **launch state** of a VMCS determines which VM-entry instruction should be used with that VMCS. 
  1. The `VMLAUNCH` instruction requires a VMCS whose launch state is "clear".
  2. The `VMRESUME` instruction requires a VMCS whose launch state is "launched".

A logical processor maintains a VMCS's launch state in the corresponding VMCS region. The following items describe how a logical processor manages the launch state of a VMCS:
  1. If the launch state of the current VMCS is "clear", successful execution of the VMLAUNCH instruction changes the launch state to "launched".
  2. The memory operand of the `VMCLEAR` instruction is the address of a VMCS. After execution of the instruction, the launch state of that VMCS is "clear".

**Note that there are no other ways to modify the launch state of a VMCS (it cannot be modified using VMWRITE) or discover it (it cannot be read using VMREAD).**

## 27.2 FORMAT OF THE VMCS REGION

A VMCS region comprises up to 4-KBytes. The exact size is implementation specific and can be determined by consulting the VMX capability MSR `IA32_VMX_BASIC`.

### VMCS revision identifier

The first 4 bytes of the VMCS region contain the VMCS revision identifier at bits `30:0`.

Processors that maintain VMCS data in different formats use different VMCS revision identifiers. These identifiers enable software to avoid using a VMCS region formatted for one processor on a processor that uses a different format.

Bit 31 of this 4-byte region indicates whether the VMCS is a shadow VMCS.

---

Software should write the VMCS revision identifier to the VMCS region before using that region for a VMCS. It is never written by the processor.

`VMPTRLD` fails if its operand references a VMCS region whose VMCS revision identifier differs from that used by the processor.

Software can discover the VMCS revision identifier that a processor uses by reading the VMX capability MSR `IA32_VMX_BASIC`.

---

Software should clear or set the shadow-VMCS indicator depending on whether the VMCS is to be an ordinary VMCS or a shadow VMCS.

`VMPTRLD` fails if the shadow-VMCS indicator is 1 and the processor does not support the 1-setting of the "VMCS shadowing" VM-execution control.

Software can discover support for this setting by reading the VMX capability MSR `IA32_VMX_PROCBASED_CTLS2`.

### VMX-abort indicator

The next 4 bytes of the VMCS region are used for the VMX-abort indicator.

The contents of these bits do not control processor operation in any way. A logical processor writes a non-zero value into these bits if a VMX abort occurs. Software may also write into this field.

### VMCS data

The remainder of the VMCS region is used for VMCS data, the parts that control VMX non-root operation and VMX transitions.

The format of this data is implementation-specific. 

To ensure proper behavior in VMX operation, software should maintain the VMCS region and related structures in writeback cacheable memory.

Software should consult the VMX capability MSR `IA32_VMX_BASIC`.

## 27.3 ORGANIZATION OF VMCS DATA

The VMCS data is organized into six logical groups.

1. **Guest-state area**. Processor state is saved into the guest-state area on VM exits and loaded from there on VM entries.

2. **Host-state area**. Processor state is loaded from the host-state area on VM exits.

3. **VM-execution control fields**. These fields control processor behavior in VMX non-root operation. They determine, in part, the causes of VM exits.

4. **VM-exit control fields**. These fields control VM exits.

5. **VM-entry control fields**. These fields control VM entries.

6. **VM-exit information fields**. These fields receive information on VM exits and describe the cause and the nature of VM exits. On some processors, these fields are read-only. Software can discover whether these fields can be written by reading the VMX capability MSR `IA32_VMX_MISC`.

The VM-execution control fields, the VM-exit control fields, and the VM-entry control fields are sometimes referred to collectively as VMX controls.

## 27.4 GUEST-STATE AREA

VM entries load processor state from these fields and VM exits store processor state into these fields.

### 27.4.1 Guest Register State

The following fields in the guest-state area correspond to processor registers:

1. Control registers CR0, CR3, and CR4 (64 bits each; 32 bits on processors that do not support Intel 64 architecture).

2. Debug register DR7 (64 bits; 32 bits on processors that do not support Intel 64 architecture).

3. RSP, RIP, and RFLAGS (64 bits each; 32 bits on processors that do not support Intel 64 architecture).

<!-- 4. The following fields for each of the registers CS, SS, DS, ES, FS, GS, LDTR, and TR:
   - Selector (16 bits).
   - Base address (64 bits; 32 bits on processors that do not support Intel 64 architecture). The base-address fields for CS, SS, DS, and ES have only 32 architecturally-defined bits; nevertheless, the corresponding VMCS fields have 64 bits on processors that support Intel 64 architecture.
   - Segment limit (32 bits). The limit field is always a measure in bytes.
     - Access rights (32 bits).
     - Bit 3:0 represent the segment type, bit 4 is the (S) descriptor type (0 for system and 1 for code/data), bit 6:5 represent the descriptor privilege level (DPL), and bit 7 represent the segment present (P).
     - The low 16 bits correspond to bits `23:8` of the upper 32 bits of a 64-bit segment descriptor. While bits 19:16 of code-segment and data-segment descriptors correspond to the upper 4 bits of the segment limit, the corresponding bits (bits 11:8) are reserved in this VMCS field.
     - Bit 16 indicates an unusable segment. Attempts to use such a segment fault except in 64-bit mode. In general, a segment register is unusable if it has been loaded with a null selector. There are a few exceptions to this statement.
     - Bits 31:17 are reserved.
-->