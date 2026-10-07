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

/* Host-side representation of the VM. */
struct vm {
	/* File descriptor for /dev/kvm. */
	int dev_kvm_fd;

	/* File descriptor for the KVM virtual machine. */
	int vm_fd;

	/* File descriptor representing the created vCPU. */
	int vcpu_fd;

	/* The object for KVM_RUN operation associated to the vCPU above. */
	struct kvm_run *kvm_run;

	/* The memory slot representing the 1 MiB of guest memory. */
	struct kvm_userspace_memory_region guest_mem;
};

/* Perform initial VM setup. */
void vm_init(struct vm *vm)
{
	/* KVM API Version */
	int api_ver;

	/*
	 * An Intel 8086 can address a physical memory of 1 MiB 
	 * at max. This region describer the userspace to guest 
	 * metadata related to that memory.
	 */

	struct kvm_userspace_memory_region guest_memory;

	/* [STEP 1]: Open a handle to the kernel's kvm interface. */
	vm->dev_kvm_fd = open("/dev/kvm", O_RDWR);
	if (vm->dev_kvm_fd < 0) {
		perror("open /dev/kvm");
		exit(1);
	}

	/* [STEP 2]: Query and verify the KVM API version. */

	api_ver = ioctl(vm->dev_kvm_fd, KVM_GET_API_VERSION, 0);
	if (api_ver < 0) {
		perror("KVM_GET_API_VERSION");
		exit(1);
	}

	if (api_ver != KVM_API_VERSION) {
		fprintf(
			stderr, "Got KVM api version %d, expected %d\n",
			api_ver, KVM_API_VERSION
		);
		exit(1);
	}


	/* [STEP 3]: Create a virtual machine.
   *
	 * KVM_CREATE_VM creates the resources reuquired to 
	 * represent a VM inside the kernel and gives 
	 * userspace a handle to them.
	*/
	vm->vm_fd = ioctl(vm->dev_kvm_fd, KVM_CREATE_VM, 0);
	if (vm->vm_fd < 0) {
		perror("KVM_CREATE_VM");
		exit(1);
	}

	/*
	 * [STEP 4]: Reserve a region in the host's userspace 
	 * memory that will be used as the guest's physical 
	 * memory.
	 */
	void* mem = mmap(
		NULL, 
		0x100000, 
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, 
		-1, 0
	);
	if (mem == MAP_FAILED) {
		perror("mmap vm->mem");
		exit(1);
	}

	/* [STEP 5]: Set the metadata about this memory region. */

	/* Where the memory exists in the host process? */
	guest_memory.userspace_addr = (unsigned long)(mem);

	/*
	 * Where the host process's memory appears in the 
	 * guest's physical address space?
	 */
	guest_memory.guest_phys_addr = 0x0;

	/* The size of the memory. */
	guest_memory.memory_size = 0x100000;

	guest_memory.slot  = 0;    /* Guest memory region #. */
	guest_memory.flags = 0;    /* [?] */


	/* [STEP 6]: Notify KVM about this memory slot. */
	if (
		ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, &guest_memory) < 0
	){
		perror("KVM_SET_USER_MEMORY_REGION");
		exit(1);
	}

	/*
	 * [STEP 7]: Keep a record of this memory slot config in 
	 * the VM struct.
	 */
	vm->guest_mem = guest_memory;
}

/* Create and initialize a vCPU. */
void vcpu_init(struct vm *vm)
{
	int vcpu_mmap_size;

	/* [STEP 1]: Create a vCPU. */
	vm->vcpu_fd = ioctl(vm->vm_fd, KVM_CREATE_VCPU, 0);
	if (vm->vcpu_fd < 0) {
		perror("KVM_CREATE_VCPU");
		exit(1);
	}

	/* [STEP 2]: Ask the kernel the total memory required by a vCPU. */
	vcpu_mmap_size = ioctl(vm->dev_kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (vcpu_mmap_size <= 0) {
		perror("KVM_GET_VCPU_MMAP_SIZE");
		exit(1);
	}

	/* [STEP 3]: Reserve memory for the vCPU's KVM_RUN object. */
	vm->kvm_run = mmap(
		NULL, 
		vcpu_mmap_size, 
		PROT_READ | PROT_WRITE,
		MAP_SHARED, vm->vcpu_fd, 0
	);
	if (vm->kvm_run == MAP_FAILED) {
		perror("mmap kvm_run");
		exit(1);
	}
}

/* Run a VM. */
int run_vm(struct vm *vm)
{
	struct kvm_regs regs = {0};
	uint64_t memval = 0;

	for (;;) {
		/* Start guest code execution. */
		if (ioctl(vm->vcpu_fd, KVM_RUN, 0) < 0) {
			perror("KVM_RUN");
			exit(1);
		}

		/*
		 * A VM exit has returned control back to userspace. 
		 * The userspace accesses the shared vCPU state to 
		 * analyze the cause of exit and act appropriately.
		 */
		switch (vm->kvm_run->exit_reason) {
			/*
			 * The guest ends with a HLT instruction. 
			 * If that's the reason, exit the loop.
			 */
		  case KVM_EXIT_HLT:
		  	goto check;

			/*
			 * If the VM EXIT is caused by an I/O operation, 
			 * handle it below and resume the VM.
			 */
  		case KVM_EXIT_IO:
  			if (
					vm->kvm_run->io.direction == KVM_EXIT_IO_OUT &&
  			  vm->kvm_run->io.port == 0xE9
				){
  				char *p = (char*)(vm->kvm_run);
  				fwrite(
						p + vm->kvm_run->io.data_offset,
						vm->kvm_run->io.size, 1, stdout
					);
  				fflush(stdout);
  				continue;
  			}

			/* Fall through. */
  		default:
  			fprintf(
					stderr,	
					"Got exit_reason %d, expected KVM_EXIT_HLT (%d)\n",
  				vm->kvm_run->exit_reason, KVM_EXIT_HLT
				);
  			exit(1);
		}
	}

	check:
		/* Get the general-purpose registers. */
		if (ioctl(vm->vcpu_fd, KVM_GET_REGS, &regs) < 0) {
			perror("KVM_GET_REGS");
			exit(1);
		}

		/*
		 * Check if rax contains the intended value. It is 
		 * 42, as per the guest machine-code.
		 */
		if (regs.rax != 42) {
			printf("Wrong result: {R, E,}AX is %lld\n", regs.rax);
			return 1;
		}

		printf("ax contains 42 as expected.\n");

		/* The guest stores 42 at ds:0x0. We check the 
			 memory at 0x400 into sz. */
		memcpy(
			&memval, 
			(void*)((char*)(vm->guest_mem.userspace_addr) + 0x00000), 
			2
		);
		if (memval != 42) {
			printf(
				"Wrong result: memory at ds:0x0 is %lld\n",
				(unsigned long long)(memval)
			);
			return 1;
		}

		printf("The memory ds:0x0 contains 42 as expected.\n");

		return 0;
}

int copy_code_into_guest(
	const char* filename, 
	void* dest_host_mem
){
	FILE* fp = fopen(filename, "rb");

	fseek(fp, 0, SEEK_END);
	size_t file_size = ftell(fp);
	rewind(fp);

	unsigned char* guest_code = malloc(file_size);
	if (!guest_code){
		fprintf(
			stderr, 
			"copy_code_into_guest: guest_code malloc failed.\n"
		);
	}

	fread(guest_code, 1, file_size, fp);
	fclose(fp);

	memcpy(dest_host_mem, guest_code, file_size);
}

/* Bring the processor in real mode. */
int init_real_mode(
	struct vm *vm, 
	const char* filename
){
	struct kvm_regs  regs  = {0};    /* General CPU registers. */
	struct kvm_sregs sregs = {0};    /* Special CPU registers. */

	/* [STEP 1]: Retreive the current register state. */

	if (ioctl(vm->vcpu_fd, KVM_GET_REGS, &regs) < 0) {
		perror("KVM_GET_REGS");
		exit(1);
	}

	if (ioctl(vm->vcpu_fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}


	/*
	 * [STEP 2]: Set the registers as per the Intel 8086 
	 * reset configuration.
	 */

	regs.rflags = 0x0000;
	regs.rip    = 0x0000;

  sregs.cs.selector = 0xffff;
	sregs.cs.base = 0xffff0;
	sregs.cs.limit = 0xffff;

	sregs.ds.selector = 0x0000;
	sregs.ds.base = 0x00000;
	sregs.ds.limit = 0xffff;

  sregs.ss.selector = 0x0000;
	sregs.ss.base = 0x00000;
	sregs.ds.limit = 0xffff;

  sregs.es.selector = 0x0000;
	sregs.es.base = 0x00000;
	sregs.es.limit = 0xffff;


	/* [STEP 3]: Sync the updated register state. */

	if (ioctl(vm->vcpu_fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	if (ioctl(vm->vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}


	/*
	 * [STEP 4]: Copy the bootstrap instruction in the reset 
	 * memory region. When it is executed, it will initiate a 
	 * far jump to the CS segment where the guest instructions 
	 * for software initialization are placed.
	 */
	copy_code_into_guest(
		"./bootstrap.bin", 
		(void*)((char*)(vm->guest_mem.userspace_addr) + 0xffff0)
	);

	/*
	 * [STEP 5]: Copy the software initialization guest code 
	 * into the CS memory region.
	 */
	copy_code_into_guest(
		"./guest-setup.bin", 
		(void*)((char*)(vm->guest_mem.userspace_addr) + 0x10000)
	);

	/* Run the VM. */
	return run_vm(vm);
}

int main(int argc, char **argv)
{
	struct vm vm;

	printf("Creating and Initializing a VM....\n");
	vm_init(&vm);
	printf("VM Initialized.\n");

	printf("Creating and Initializing a vCPU....\n");
	vcpu_init(&vm);
	printf("vCPU Initialized.\n");

	printf("Making the vCPU enter the guest in real-address\n");
	printf("mode as per the x86 reset configuration and\n");
	printf("perform essential software initializtion.\n");

	if (init_real_mode(&vm, "./guest-setup.s") != 0){
		fprintf(stderr, "Real-Address mode initialization failed.\n");
		exit(1);
	}
}
