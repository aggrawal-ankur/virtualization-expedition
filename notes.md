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
