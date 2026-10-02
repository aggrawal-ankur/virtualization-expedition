#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>

/* ---+---+---+--- Data Structures ---+---+---+--- */

/* Host-side representation of the VM created here. */
struct vm {
	int   sys_fd;    /* File descriptor for /dev/kvm. */
	int   fd;        /* File descriptor for the KVM virtual machine. */
	char *mem;       /* Pointer to the memory region that the program maps for the guest's memory. */
};

/* Host-side representation of the vCPU created for this VM. */
struct vcpu {
	int fd;      /* File descriptor representing the created VCPU. */
	struct kvm_run *kvm_run;    /* [?] */
};

extern const unsigned char guest_code[], guest_code_end[];


/* Performs initial setup of a VM. */
void vm_init(struct vm *vm, size_t mem_size)
{
	/* KVM API Version */
	int api_ver;

	/*
	 * This struct describes the mapping between a region 
	 * of the host process's virtual memory and a region 
	 * of the guest's physical address space.
	 */
	struct kvm_userspace_memory_region mem_region;

	/* [STEP 1]: Open a handle to the kernel's kvm interface. */
	vm->sys_fd = open("/dev/kvm", O_RDWR);
	if (vm->sys_fd < 0) {
		perror("open /dev/kvm");
		exit(1);
	}

	/* [STEP 2]: Query the KVM API version. */
	api_ver = ioctl(vm->sys_fd, KVM_GET_API_VERSION, 0);
	if (api_ver < 0) {
		perror("KVM_GET_API_VERSION");
		exit(1);
	}

	/* [STEP 3]: Verify the API version. */
	if (api_ver != KVM_API_VERSION) {
		fprintf(
			stderr, "Got KVM api version %d, expected %d\n",
			api_ver, KVM_API_VERSION
		);
		exit(1);
	}

	/* [STEP 4]: Create a virtual machine.
   *
	 * KVM_CREATE_VM creates the resources needed to 
	 * represent a VM inside the kernel and gives 
	 * userspace a handle to them.
	*/
	vm->fd = ioctl(vm->sys_fd, KVM_CREATE_VM, 0);
	if (vm->fd < 0) {
		perror("KVM_CREATE_VM");
		exit(1);
	}

	/*
	 * [STEP 6]: Reserve a piece of host's userspace memory 
	 * that will be used as the guest's physical memory.
	 */
	vm->mem = mmap(
		NULL, 
		mem_size, 
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, 
		-1, 0
	);
	if (vm->mem == MAP_FAILED) {
		perror("mmap mem");
		exit(1);
	}

	/* A hint to the kernel to enable KSM. */
	madvise(vm->mem, mem_size, MADV_MERGEABLE);

	/*
	 * [STEP 7]: Set metadata about the host userspace 
	 * memory region and the guest physical region it 
	 * powers.
	 */

	/* Where the memory exists in the host process? */
	mem_region.userspace_addr = (unsigned long)(vm->mem);

	/*
	 * Where the host process's memory appears in the 
	 * guest's physical address space?
	 */
	mem_region.guest_phys_addr = 0xffff0000;

	/* The size of the memory. */
	mem_region.memory_size = mem_size;

	mem_region.slot  = 0;    /* # guest memory region. */
	mem_region.flags = 0;    /* [?] */

	/* [STEP 8]: Pass the updated mem_region description to KVM. */
	if (
		ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &mem_region) < 0
	){
		perror("KVM_SET_USER_MEMORY_REGION");
		exit(1);
	}
}

/* Create and initialize a vCPU. */
void vcpu_init(struct vm *vm, struct vcpu *vcpu)
{
	int vcpu_mmap_size;

	/* [STEP 1]: Create a vCPU. */
	vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
	if (vcpu->fd < 0) {
		perror("KVM_CREATE_VCPU");
		exit(1);
	}

	/* [STEP 2]: Ask the kernel the total memory 
			required to create a vCPU. */
	vcpu_mmap_size = ioctl(vm->sys_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (vcpu_mmap_size <= 0) {
		perror("KVM_GET_VCPU_MMAP_SIZE");
		exit(1);
	}

	/* [STEP 3]: Reserve memory for the vCPU. */
	vcpu->kvm_run = mmap(
		NULL, 
		vcpu_mmap_size, 
		PROT_READ | PROT_WRITE,
		MAP_SHARED, vcpu->fd, 0
	);
	if (vcpu->kvm_run == MAP_FAILED) {
		perror("mmap kvm_run");
		exit(1);
	}
}

/* Run a VM. */
int run_vm(struct vm *vm, struct vcpu *vcpu, size_t sz)
{
	struct kvm_regs regs;
	uint64_t memval = 0;

	for (;;) {
		/* Start guest code execution. */
		if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
			perror("KVM_RUN");
			exit(1);
		}

		/* A VM exit has happened and the control has 
			 come back to userspace. The userspace accesses 
			 the shared vCPU state to analyze the cause 
			 of exit and act appropriately.
		 */
		switch (vcpu->kvm_run->exit_reason) {
			/* The guest ends with a HLT instruction. If 
				 that's the reason, exit the loop. */
		  case KVM_EXIT_HLT:
		  	goto check;

			/* If the VM EXIT is caused by an I/O 
				 operation, handle it below and resume. */
  		case KVM_EXIT_IO:
  			if (
					vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT &&
  			  vcpu->kvm_run->io.port == 0xE9
				){
  				char *p = (char*)(vcpu->kvm_run);
  				fwrite(
						p + vcpu->kvm_run->io.data_offset,
						vcpu->kvm_run->io.size, 1, stdout
					);
  				fflush(stdout);
  				continue;
  			}

			/* Fall through. */
  		default:
  			fprintf(
					stderr,	
					"Got exit_reason %d, expected KVM_EXIT_HLT (%d)\n",
  				vcpu->kvm_run->exit_reason, KVM_EXIT_HLT
				);
  			exit(1);
		}
	}

	check:
		/* Get the general-purpose registers. */
		if (ioctl(vcpu->fd, KVM_GET_REGS, &regs) < 0) {
			perror("KVM_GET_REGS");
			exit(1);
		}

		/* Check if rax contains the intended value. 
		   It is 42, as per the guest machine-code. */
		if (regs.rax != 42) {
			printf("Wrong result: {E,R,}AX is %lld\n", regs.rax);
			return 0;
		}

		printf("ax contains 42 as expected.\n");

		/* The guest stores 42 at 0x400. We check the 
			 memory at 0x400 into sz. */
		memcpy(&memval, &vm->mem[0x0], sz);
		if (memval != 42) {
			printf(
				"Wrong result: memory at cs:0x0 is %lld\n",
				(unsigned long long)(memval)
			);
			return 0;
		}

		printf("The memory cs:0x0 contains 42 as expected.\n");

		return 1;
}

/* Bring the processor in real mode. */
int init_real_mode(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_regs  regs;     /* General CPU registers. */
	struct kvm_sregs sregs;    /* Special CPU registers. */


	/* [STEP 1]: Retreive register state from KVM. */

	if (ioctl(vcpu->fd, KVM_GET_REGS, &regs) < 0) {
		perror("KVM_GET_REGS");
		exit(1);
	}

	if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}


	/* [STEP 2]: Set the general-purpose registers. */

	memset(&regs, 0, sizeof(regs));

	regs.rflags = 0x00000002;    /* Bit 1's mask is 2. */
	regs.rip    = 0x0000FFF0;


	/* [STEP 3]: Set the special-purpose registers. */

  sregs.cr0 = 0x60000010;
  sregs.cr2 = 0x00000000;
  sregs.cr3 = 0x00000000;
  sregs.cr4 = 0x00000000;

  sregs.cs.selector = 0xF000;
  sregs.cs.base     = 0xFFFF0000;
  sregs.cs.limit    = 0xFFFF;
  sregs.cs.present  = 0x1;
	sregs.cs.type     = 0xB;

	sregs.ds.selector = 0x0000;
	sregs.ds.base     = 0x00000000;
	sregs.ds.limit    = 0xFFFF;
	sregs.ds.present  = 0x1;
	sregs.ds.type     = 0x3;

  sregs.ss.selector = 0x0000;
  sregs.ss.base     = 0x00000000;
  sregs.ss.limit    = 0xFFFF;
  sregs.ss.present  = 0x1;
	sregs.ss.type     = 0x3;

  sregs.es.selector = 0x0000;
  sregs.es.base     = 0x00000000;
  sregs.es.limit    = 0xFFFF;
  sregs.es.present  = 0x1;
	sregs.es.type     = 0x3;

  sregs.fs.selector = 0x0000;
  sregs.fs.base     = 0x00000000;
  sregs.fs.limit    = 0xFFFF;
  sregs.fs.present  = 0x1;
	sregs.fs.type     = 0x3;

  sregs.gs.selector = 0x0000;
  sregs.gs.base     = 0x00000000;
  sregs.gs.limit    = 0xFFFF;
  sregs.gs.present  = 0x1;
	sregs.gs.type     = 0x3;

	sregs.gdt.base  = 0x00000000;
	sregs.gdt.limit = 0xFFFF;

	sregs.idt.base  = 0x00000000;
	sregs.idt.limit = 0xFFFF;

  sregs.efer = 0x0000000000000000;


	/* [STEP 3]: Inform KVM about the updated register state (sync). */

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	if (ioctl(vcpu->fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}


	/* 
	 * [STEP ]: Copy the guest machine-code bytes into the 
	 * host memory region that is registered with KVM as the 
	 * VM's guest physical memory.
	 */
	memcpy(vm->mem + 0xfff0, guest_code, guest_code_end - guest_code);

	/* Run the VM. */
	return run_vm(vm, vcpu, 2);
}

int main(int argc, char **argv)
{
	struct vm   vm;
	struct vcpu vcpu;

	vm_init(&vm, 0x10000);
	vcpu_init(&vm, &vcpu);

	return init_real_mode(&vm, &vcpu);
}
