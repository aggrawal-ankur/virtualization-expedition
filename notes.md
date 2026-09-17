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

4. The following fields for each of the registers CS, SS, DS, ES, FS, GS, LDTR, and TR:
   - Selector (16 bits).
   - Base address (64 bits; 32 bits on processors that do not support Intel 64 architecture). The base-address fields for CS, SS, DS, and ES have only 32 architecturally-defined bits; nevertheless, the corresponding VMCS fields have 64 bits on processors that support Intel 64 architecture.
   - Segment limit (32 bits). The limit field is always a measure in bytes.
   - Access rights (32 bits).
     - The low 16 bits correspond to bits `23:8` of the upper 32 bits of a 64-bit segment descriptor. While bits 19:16 of code-segment and data-segment descriptors correspond to the upper 4 bits of the segment limit, the corresponding bits (bits 11:8) are reserved in this VMCS field.
     - Bit 16 indicates an unusable segment. Attempts to use such a segment fault except in 64-bit mode. In general, a segment register is unusable if it has been loaded with a null selector. There are a few exceptions to this statement.
     - Bits 31:17 are reserved.

     | Bit Position(s) | Field |
     | --------------- | ----- |
     | 3:0 | Segment type |
     | 4 | (S) - Descriptor type (0:system, 1:code/data) |
     | 6:5 | (DPL) - Descriptor privilege level |
     | 7 | (P) - Segment present |
     | 11:8 | Reserved |
     | 12 | (AVL) - Available for use by system software |
     | 13 | Reserved (except CS) |
     | | (L) - 64-bit mode active (CS only) |
     | 14 | (D/B) - Default operation size (0: 16-bit segment, 1: 32-bit segment) |
     | 15 | (G) - Granularity |
     | 16 | Segment usability (0:usable, 1:unusable) |
     | 31:17 | Reserved |

   - The base address, segment limit, and access rights compose the "hidden" part (or "descriptor cache") of each segment register. These data are included in the VMCS because it is possible for a segment register's descriptor cache to be inconsistent with the segment descriptor in memory (in the GDT or the LDT) referenced by the segment register's selector.

   - The value of the DPL field for SS is always equal to the logical processor's current privilege level (CPL).
   - On some processors, executions of VMWRITE ignore attempts to write non-zero values to any of bits 11:8 or bits 31:17. On such processors, VMREAD always returns 0 for those bits, and VM entry treats those bits as if they were all 0.

5. Base address (64 bits; 32 bits that don't support I-64 arc) and Limit (32 bits) fields for GDTR and IDTR registers.
6. Lots of MSRs.
7. The shadow-stack pointer register `SSP` (64 bits; 32 bits on processors that do not support Intel 64 architecture). This field is supported only on processors that support the 1-setting of the "load CET state" VM-entry control.
8. The register `SMBASE` (32 bits). This register contains the base address of the logical processor's `SMRAM` image.

### 27.4.2 Guest Non-Register State

The guest-state area also includes the following fields that characterize guest state but they don't correspond to processor registers.

1. **Activity state**: A 32-bit field that identifies the logical processor's activity state. When a logical processor is executing instructions normally, it is in the active state. Execution of certain instructions and the occurrence of certain events may cause a logical processor to transition to an **inactive state** in which it ceases to execute instructions. The following activity states are defined:

   | State | Description | Value |
   | ----- | ----------- | ----- |
   | Active | 0 | The logical processor is executing instructions normally. |
   | HLT | 1 | The logical processor is inactive because it executed the HLT instruction. |
   | Shutdown | 2 | The logical processor is inactive because it incurred a *triple fault* or some other serious
   error. |
   | Wait-for-SIPI | 3 | The logical processor is inactive because it is waiting for a startup-IPI (SIPI). |

   Note that the execution of the `MWAIT` instruction may put a logical processor into an inactive state.However, this VMCS field never reflects this state.

   Future processors may include support for other activity states. Software should read the VMX capability MSR `IA32_VMX_MISC` to determine what activity states are supported.

2. **Interruptibility state**: The IA-32 architecture includes features that permit certain events to be blocked for a period of time. This 32-bits field contains information about such blocking.

3. **Pending debug exceptions**: IA-32 processors may recognize one or more debug exceptions without immediately delivering them. This 64-bit field (or 32-bit, if I-64 arch isn't supported) contains information about such exceptions.

4. **VMCS link pointer**: If the "VMCS shadowing" VM-execution control is 1, the `VMREAD` and `VMWRITE` instructions access the VMCS referenced by this pointer (a 64-bit field). Otherwise, software should set this field to `FFFFFFFF_FFFFFFFFH` to avoid VM-entry failures.

5. **VMX-preemption timer value**: This 32-bit field contains the value that the VMX-preemption timer will use following the next VM entry with that setting. It is supported only on processors that support the 1-setting of the "activate VMX-preemption timer" VM-execution control.

6. **Page-directory-pointer-table entries** (PDPTEs; 64 bits each). These four fields (`PDPTE0`, `PDPTE1`, `PDPTE2`, and `PDPTE3`) are supported only on processors that support the 1-setting of the "enable EPT" VM-execution control. They correspond to the PDPTEs referenced by CR3 when PAE paging is in use. They are used only if the "enable EPT" VM-execution control is 1.

7. **Guest interrupt status**: This 16-bit field characterizes part of the guest's virtual-APIC state and does not correspond to any processor or APIC registers. It comprises two 8-bit subfields:

   - **Requesting virtual interrupt (RVI)**: This is the low byte of the guest interrupt status. The processor treats this value as the vector of the highest priority virtual interrupt that is requesting service. The value 0 implies that there is no such interrupt.

   - **Servicing virtual interrupt (SVI)**: This is the high byte of the guest interrupt status. The processor treats this value as the vector of the highest priority virtual interrupt that is in service. The value 0 implies that there is no such interrupt.

   This field is supported only on processors that support the 1-setting of the "virtual-interrupt delivery" VM-execution control.

8. **PML index**: This 16-bit field contains the logical index of the next entry in the page-modification log. Because the page-modification log comprises 512 entries, the PML index is typically a value in the range 0–511. It is supported only on processors that support the 1-setting of the "enable PML" VM-execution control.

9. **Guest deadline** This 64-bit field contains the value with which the guest timer will be configured. It is supported only on processors that support the 1-setting of the "APIC-timer virtualization" VM-execution control.

## 27.5 HOST-STATE AREA

This section describes fields contained in the host-state area of the VMCS.

All fields in the host-state area correspond to processor registers.

1. **Control registers** CR0, CR3, and CR4 (64 bits each; 32 bits on processors that do not support Intel 64 architecture).

2. RSP and RIP (64 bits each; 32 bits on processors that do not support Intel 64 architecture).

3. **Selector fields** (16 bits each) for the segment registers CS, SS, DS, ES, FS, GS, and TR. There is no field in the host-state area for the LDTR selector.

4. **Base-address fields** for FS, GS, TR, GDTR, and IDTR (64 bits each; 32 bits on processors that do not support I-64 architecture).

5. Lots of MSRs.

6. The shadow-stack pointer register SSP (64 bits; 32 bits on processors that do not support Intel 64 architecture). This field is supported only on processors that support the 1-setting of the "load CET state" VM-exit control.

---

Note that some processor state components are loaded with fixed values on every VM exit in addition to the state identified here. There are no fields corresponding to those components in the host-state area.

## 27.6 VM-EXECUTION CONTROL FIELDS

The VM-execution control fields govern VMX non-root operation. They are divided into:
  1. Pin-Based VM-Execution Controls.
  2. Processor-Based VM-Execution Controls.
  3. Exception Bitmap.
  4. I/O-Bitmap Addresses.
  5. Time-Stamp Counter Offset and Multiplier.
  6. Guest/Host Masks and Read Shadows for CR0 and CR4.
  7. CR3-Target Controls.
  8. Controls for APIC Virtualization.

### 27.6.1 Pin-Based VM-Execution Controls

The pin-based VM-execution controls constitute a 32-bit vector that governs the handling of asynchronous events.

| Bit Position | Name | Description |
| ------------ | ---- | ----------- |
| 0 | External-interrupt exiting | 0: External interrupts are normally delivered. |
| | | 1: External interrupts cause VM exits and the value of RFLAGS.IF doesn't affect interrupt blocking. |
| 3 | NMI exiting | Determines interactions between IRET and blocking by NMI. |
| | | 0: Non-maskable interrupts are normally delivered using vector 2. |
| | | 1: NMIs cause VM exits. |
| 5 | Virtual NMIs | If 1, NMIs are never blocked and the "blocking by NMI" bit (bit 3) in the interruptibility-state field indicates "virtual-NMI blocking". This control also interacts with the "NMI-window exiting" VM-execution control. |
| 6 | Activate VMX-preemption timer | If 1, the VMX-preemption timer counts down in VMX non-root operation. A VM exit occurs when the timer counts down to zero. |
| 7 | Process-posted interrupts | If 1, the processor treats interrupts with the posted-interrupt notification vector specially, updating the virtual-APIC page with posted-interrupt requests. |

All other bits in this field are reserved, some to 0 and some to 1. Software should consult the VMX capability MSRs `IA32_VMX_PINBASED_CTLS` and `IA32_VMX_TRUE_PINBASED_CTLS` to determine how to set reserved bits. Failure to set reserved bits properly causes subsequent VM entries to fail.

The first processors to support the virtual-machine extensions supported only the 1-settings of bits 1, 2, and 4. The VMX capability MSR `IA32_VMX_PINBASED_CTLS` will always report that these bits must be 1.

  - Logical processors that support the 0-settings of any of these bits will support the VMX capability MSR `IA32_VMX_TRUE_PINBASED_CTLS`, and software should consult this MSR to discover support for the 0-settings of these bits.

  - Software that is not aware of the functionality of any one of these bits should set that bit to 1.

---

Note that some asynchronous events cause VM exits regardless of the settings of the pin-based VM-execution controls

### 27.6.2 Processor-Based VM-Execution Controls

The processor-based VM-execution controls constitute three vectors that govern the handling of synchronous events, mainly those caused by the execution of specific instructions. These vectors are:
  1. Primary processor-based VM-execution controls (32 bits)
  2. Secondary processor-based VM-execution controls (32 bits)
  3. Tertiary VM-execution controls (64 bits)

[HUGE TABLES]

### 27.6.3 Exception Bitmap

The exception bitmap is a 32-bit field that contains one bit for each exception. When an exception occurs, its vector is used to select a bit in this field. If the bit is 1, the exception causes a VM exit. If the bit is 0, the exception is delivered normally using the exception's vector.

Whether a page fault (exception with vector 14) causes a VM exit is determined by bit 14 in the exception bitmap as well as the error code produced by the page fault and two 32-bit fields in the VMCS (the page-fault error-code mask and page-fault error-code match).

### 27.6.4 I/O-Bitmap Addresses

The VM-execution control fields include the 64-bit physical addresses of I/O bitmaps A and B (each of which are 4 KBytes in size). I/O bitmap A contains one bit for each I/O port in the range 0000H through 7FFFH; I/O bitmap B contains bits for ports in the range 8000H through FFFFH.

A logical processor uses these bitmaps if and only if the "use I/O bitmaps" control is 1. If the bitmaps are used, execution of an I/O instruction causes a VM exit if any bit in the I/O bitmaps corresponding to a port it accesses is 1. If the bitmaps are used, their addresses must be 4-KByte aligned.

### 27.6.5 Time-Stamp Counter Offset and Multiplier

The VM-execution control fields include a 64-bit TSC-offset field. If the "`RDTSC` exiting" control is 0 and the "use TSC offsetting" control is 1, this field controls executions of the `RDTSC` and `RDTSCP` instructions. It also controls executions of the `RDMSR` instruction that read from the `IA32_TIME_STAMP_COUNTER` MSR. For all of these, the value of the TSC offset is added to the value of the time-stamp counter, and the sum is returned to guest software in `EDX:EAX`.

Processors that support the 1-setting of the "use TSC scaling" control also support a 64-bit TSC-multiplier field. If this control is 1 (and the "`RDTSC` exiting" control is 0 and the "use TSC offsetting" control is 1), this field also affects the executions of the `RDTSC`, `RDTSCP`, and `RDMSR` instructions identified above. Specifically, the contents of the time-stamp counter is first multiplied by the TSC multiplier before adding the TSC offset.

### 27.6.6 Guest/Host Masks and Read Shadows for CR0 and CR4

VM-execution control fields include guest/host masks and read shadows for the CR0 and CR4 registers. These fields control executions of instructions that access those registers (including CLTS, LMSW, MOV CR, and SMSW).

They are 64 bits on processors that support Intel 64 architecture and 32 bits on processors that do not.

In general, bits set to 1 in a guest/host mask correspond to bits "owned" by the host:
  - Guest attempts to set them (using CLTS, LMSW, or MOV to CR) to values differing from the corresponding bits in the corresponding read shadow cause VM exits.
  - Guest reads (using MOV from CR or SMSW) return values for these bits from the corresponding read shadow.

Bits cleared to 0 correspond to bits "owned" by the guest; guest attempts to modify them succeed and guest reads return values for these bits from the control register itself.

### 27.6.7 CR3-Target Controls

The VM-execution control fields include a set of 4 CR3-target values and a CR3-target count.
  - The CR3-target values each have 64 bits on processors that support Intel 64 architecture and 32 bits on processors that do not.
  - The CR3-target count has 32 bits on all processors.

An execution of MOV to CR3 in VMX non-root operation does not cause a VM exit if its source operand matches one of these values. If the CR3-target count is n, only the first n CR3-target values are considered; if the CR3-target count is 0, MOV to CR3 always causes a VM exit.

There are no limitations on the values that can be written for the CR3-target values. VM entry fails if the CR3-target count is greater than 4.

Future processors may support a different number of CR3-target values. Software should read the VMX capability MSR `IA32_VMX_MISC` to determine the number of values supported.

### 27.6.8 Controls for APIC Virtualization

There are three mechanisms by which software accesses registers of the logical processor's local APIC.

1. If the local APIC is in **xAPIC mode**, it can perform memory-mapped accesses to addresses in the 4-KByte page referenced by the physical address in the `IA32_APIC_BASE` MSR.

2. If the local APIC is in x2APIC mode, it can access the local APIC's registers using the `RDMSR` and `WRMSR` instructions. If the local APIC does not support x2APIC mode, it is always in xAPIC mode.

3. In 64-bit mode, it can access the local APIC's task-priority register (TPR) using the MOV CR8 instruction.

Several processor-based VM-execution controls control such accesses. These are "use TPR shadow", "virtualize APIC accesses", "virtualize x2APIC mode", "virtual-interrupt delivery", "APIC-register virtualization", and "IPI virtualization". These controls interact with the following fields:

1. **APIC-access address**: This 64-bit field contains the physical address of the 4-KByte APIC-access page.
   - If the "virtualize APIC accesses" VM-execution control is 1, access to this page may cause VM exits or be virtualized by the processor.
   - It exists only on processors that support the 1-setting of the "virtualize APIC accesses" VM-execution control.

2. **Virtual-APIC address**: This 64-bit field contains the physical address of the 4-KByte virtual-APIC page. The processor uses the virtual-APIC page to virtualize certain accesses to APIC registers and to manage virtual interrupts. Depending on the setting of the controls indicated earlier, the virtual-APIC page may be accessed by the following operations:
   - The MOV CR8 instructions.
   - Accesses to the APIC-access page if, in addition, the "virtualize APIC accesses" VM-execution control is 1.
   - The `RDMSR` and `WRMSR` instructions if, in addition, the value of ECX is in the range 800H–8FFH (indicating an APIC MSR) and the "virtualize x2APIC mode" VM-execution control is 1.

   If the "use TPR shadow" VM-execution control is 1, VM entry ensures that the virtual-APIC address is 4-KByte aligned. The virtual-APIC address exists only on processors that support the 1-setting of the "use TPR shadow" VM-execution control.

3. **TPR threshold**: A 32-bit field, whose bits 3:0 determine the threshold below which bits 7:4 of VTPR cannot fall. If the "virtual-interrupt delivery" VM-execution control is 0, a VM exit occurs after an operation (e.g., an execution of MOV to CR8) that reduces the value of those bits below the TPR threshold. The TPR threshold exists only on processors that support the 1-setting of the "use TPR shadow" VM-execution control.

4. **EOI-exit bitmap**: It includes four 64-bit fields. They are used to determine which virtualized writes to the APIC's EOI register cause VM exits. These fields are supported only on processors that support the 1-setting of the "virtual-interrupt delivery" VM-execution control.

   - `EOI_EXIT0` contains bits for vectors from 0 (bit 0) to 63 (bit 63).
   - `EOI_EXIT1` contains bits for vectors from 64 (bit 0) to 127 (bit 63).
   - `EOI_EXIT2` contains bits for vectors from 128 (bit 0) to 191 (bit 63).
   - `EOI_EXIT3` contains bits for vectors from 192 (bit 0) to 255 (bit 63).

5. **Posted-interrupt notification vector**: This 16-bit field is supported only on processors that support the 1-setting of the "process posted interrupts" VM-execution control. Its low 8 bits contain the interrupt vector that is used to notify a logical processor that virtual interrupts have been posted.

6. **Posted-interrupt descriptor address**: This 64-bit field is supported only on processors that support the 1-setting of the "process posted interrupts" VM-execution control. It is the physical address of a 64-byte aligned posted interrupt descriptor.

7. **PID-pointer table address**: This 64-bit field contains the physical address of the PID-pointer table. If the "IPI virtualization" VM-execution control is 1, the logical processor uses entries in this table to virtualize IPIs.

8. **Last PID-pointer index**: This 16-bit field contains the index of the last entry in the PID-pointer table.

---

There are a lot more ### headings in section 27.6.

## 27.7 VM-EXIT CONTROL FIELDS

The VM-exit control fields govern the behavior of VM exits.

### 27.7.1 VM-Exit Controls

The VM-exit controls constitute two vectors that govern the basic operation of VM exits.
  1. **Primary VM-exit controls** (32 bits).
  2. **Secondary VM-exits controls** (64 bits).

More details.

### 27.7.2 VM-Exit Controls for MSRs

A VMM may specify lists of MSRs to be stored and loaded on VM exits. The following VM-exit control fields determine how MSRs are stored on VM exits:

  1. **VM-exit MSR-store count**: This 32-bit field specifies the number of MSRs to be stored on VM exit. It is recommended that this count not exceed 512. Otherwise, unpredictable processor behavior (including a machine check) may result during VM exit. Future implementations may allow more MSRs to be stored reliably. Software should consult the VMX capability MSR `IA32_VMX_MISC` to determine the number supported.

  2. **VM-exit MSR-store address**: This 64-bit field contains the physical address of the VM-exit MSR-store area. The area is a table of entries, 16 bytes per entry, where the number of entries is given by the VM-exit MSR-store count. If the VM-exit MSR-store count is not zero, the address must be 16-byte aligned.

---

The following VM-exit control fields determine how MSRs are loaded on VM exits:

  1. **VM-exit MSR-load count**: This 32-bit field contains the number of MSRs to be loaded on VM exit. It is recommended that this count not exceed 512. Otherwise, unpredictable processor behavior (including a machine check) may result during VM exit.

  2. **VM-exit MSR-load address**: This 64-bit field contains the physical address of the VM-exit MSR-load area. The area is a table of entries, 16 bytes per entry, where the number of entries is given by the VM-exit MSR-load count. If the VM-exit MSR-load count is not zero, the address must be 16-byte aligned.

## 27.8 VM-ENTRY CONTROL FIELDS

The VM-entry control fields govern the behavior of VM entries.

### 27.8.1 VM-Entry Controls

The VM-entry controls constitute a 32-bit vector that governs the basic operation of VM entries.

[DETAILS]

### 27.8.2 VM-Entry Controls for MSRs

A VMM may specify a list of MSRs to be loaded on VM entries. The following VM-entry control fields manage this functionality:

  1. **VM-entry MSR-load count**: This 32-bit field contains the number of MSRs to be loaded on VM entry. It is recommended that this count not exceed 512. Otherwise, unpredictable processor behavior (including a machine check) may result during VM entry.

  2. **VM-entry MSR-load address**: This 64-bit field contains the physical address of the VM-entry MSR-load area. The area is a table of entries, 16 bytes per entry, where the number of entries is given by the VM-entry MSR-load count. If the VM-entry MSR-load count is not zero, the address must be 16-byte aligned.

### 27.8.3 VM-Entry Controls for Event Injection

VM entry can be configured to conclude by delivering an event (after all guest state and MSRs have been loaded). This process is called **event injection** and is controlled by the following three VM-entry control fields:

  1. **Injected-event identification field**: This 32-bit field provides details about the event to be injected. [DETAILS]

  2. **Injected-event exception error code**: This 32-bit field is used if and only if the valid bit (bit 31) and the deliver-error-code bit (bit 11) are both set in the injected-event identification field.

  3. **Injected-event data**: This 64-bit field is used if and only if the valid bit is set in the injected-event identification field and FRED transitions will be enabled following VM entry.

  4. **VM-entry instruction length** For injection of events whose type is software interrupt, software exception, or privileged software exception, this 32-bit field is used to determine the value of RIP that is pushed on the stack. It is used also for injection of `SYSCALL` and `SYSEXIT` when FRED transitions would be enabled.

Note that VM exits clear the valid bit (bit 31) in the injected-event identification field.

## 27.9 VM-EXIT INFORMATION FIELDS

The VMCS contains a section of fields that contain information about the most recent VM exit.

On some processors, attempts to write to these fields with `VMWRITE` fail.

[more details]

## 27.10 VMCS TYPES: ORDINARY AND SHADOW

Every VMCS is either an ordinary VMCS or a shadow VMCS. A VMCS's type is determined by the shadow-VMCS indicator in the VMCS region (this is the value of bit 31 of the first 4 bytes of the VMCS region.

0 indicates an ordinary VMCS, while 1 indicates a shadow VMCS. Shadow VMCSs are supported only on processors that support the 1-setting of the "VMCS shadowing" VM-execution control.

A shadow VMCS differs from an ordinary VMCS in two ways:
  1. An ordinary VMCS can be used for VM entry but a shadow VMCS cannot. Attempts to perform VM entry when the current VMCS is a shadow VMCS fail.
  2. The VMREAD and VMWRITE instructions can be used in VMX non-root operation to access a shadow VMCS but not an ordinary VMCS. This fact results from the following:
     - If the "VMCS shadowing" VM-execution control is 0, execution of the VMREAD and VMWRITE instructions in VMX non-root operation always cause VM exits.
     - If the "VMCS shadowing" VM-execution control is 1, execution of the VMREAD and VMWRITE instructions in VMX non-root operation can access the VMCS referenced by the VMCS link pointer
     - If the "VMCS shadowing" VM-execution control is 1, VM entry ensures that any VMCS referenced by the VMCS link pointer is a shadow VMCS.

In VMX root operation, both types of VMCSs can be accessed with the VMREAD and VMWRITE instructions.

Software should not modify the shadow-VMCS indicator in the VMCS region of a VMCS that is active. Doing so may cause the VMCS to become corrupted. Before modifying the shadow-VMCS indicator, software should execute VMCLEAR for the VMCS to ensure that it is not active.

# Chapter 28

In a virtualized environment using VMX, the guest software stack typically runs on a logical processor in VMX non-root operation. This mode of operation is similar to that of ordinary processor operation outside of the virtualized environment.

This chapter describes the differences between VMX non-root operation and ordinary processor operation with special attention to causes of VM exits (which bring a logical processor from VMX non-root operation to root operation).

### 28.1 INSTRUCTIONS THAT CAUSE VM EXITS

Certain instructions may cause VM exits if executed in VMX non-root operation. Unless otherwise specified, such VM exits are "fault-like", meaning that the instruction causing VM exit does not execute and no processor state is updated by the instruction.

### 28.1.1 Relative Priority of Faults and VM Exits

This section defines the prioritization between faults and VM exits for instructions subject to both.

The following principles describe the ordering between existing faults and VM exits:

1. **Certain exceptions have priority over VM exits.** These include invalid-opcode exceptions, faults based on privilege level, and general-protection exceptions that are based on checking I/O permission bits in the task-state segment (TSS). For example, execution of `RDMSR` with CPL=3 generates a general-protection exception, not a VM exit.

2. Faults incurred while fetching instruction operands have priority over VM exits that are conditioned based on the contents of those operands.

3. VM exits caused by execution of the `INS` and `OUTS` instructions, resulting either because the "unconditional I/O exiting” VM-execution control is 1 or because the "use I/O bitmaps control" is 1, have priority over the following faults:
   - A general-protection fault due to the relevant segment (ES for INS; DS for OUTS unless overridden by an instruction prefix) being unusable.
   - A general-protection fault due to an offset beyond the limit of the relevant segment.
   - An alignment-check exception.

4. Fault-like VM exits have priority over exceptions other than those mentioned above. For example, RDMSR of a non-existent MSR with CPL = 0 generates a VM exit and not a general-protection exception.

### 28.1.2 Instructions That Cause VM Exits Unconditionally

This section identifies instructions that cause VM exits whenever they are executed in VMX non-root operation, and thus can never be executed in VMX non-root operation.

When an instruction execution that may lead to a VM exit is identified, it is assumed that the instruction does not incur a fault that takes priority over a VM exit.

---

The following instructions cause VM exits when they are executed in VMX non-root operation: CPUID, GETSEC, INVD, and XSETBV. This is also true of instructions introduced with VMX: INVEPT, INVVPID, SEAMCALL, TDCALL, VMCALL, VMCLEAR, VMLAUNCH, VMPTRLD, VMPTRST, VMRESUME, VMXOFF, and VMXON.

### 28.1.3 Instructions That Cause VM Exits Conditionally

This section identifies instructions that cause VM exits depending on the settings of certain VM-execution control fields.

When an instruction execution that may lead to a VM exit is identified, it is assumed that the instruction does not incur a fault that takes priority over a VM exit.

---

Certain instructions cause VM exits in VMX non-root operation depending on the setting of the VM-execution
controls. The following instructions can cause "fault-like" VM exits based on the conditions described.

[LOTS OF INSTRUCTIONS/CONDITIONS]

## 28.2 OTHER CAUSES OF VM EXITS

[LOTS OF INSTRUCTIONS/CONDITIONS]

## 28.3 CHANGES TO INSTRUCTION BEHAVIOR IN VMX NON-ROOT OPERATION

[LOTS OF INSTRUCTIONS/CONDITIONS]
