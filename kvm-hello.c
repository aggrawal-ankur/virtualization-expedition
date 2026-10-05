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

	/* All the memory slots backing the guest memory. */
	struct {
		struct kvm_userspace_memory_region reset;
		struct kvm_userspace_memory_region cs;
		struct kvm_userspace_memory_region ds;
		struct kvm_userspace_memory_region es;
		struct kvm_userspace_memory_region ss;
	} guest_mem_slots;
};

/* Perform initial VM setup. */
void vm_init(struct vm *vm)
{
	/* KVM API Version */
	int api_ver;

	/*
	 * This struct describes the mapping between a region 
	 * of the host process's virtual memory and a region 
	 * of the guest's physical address space.
	 * 
	 * reset_mem_region describes the memory region defined 
	 * in the x86 architecture that is required when the 
	 * process boots after a reset.
	 */
	struct kvm_userspace_memory_region reset_mem_region;

	/* [STEP 1]: Open a handle to the kernel's kvm interface. */
	vm->dev_kvm_fd = open("/dev/kvm", O_RDWR);
	if (vm->dev_kvm_fd < 0) {
		perror("open /dev/kvm");
		exit(1);
	}

	/* [STEP 2]: Query the KVM API version. */
	api_ver = ioctl(vm->dev_kvm_fd, KVM_GET_API_VERSION, 0);
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
	vm->vm_fd = ioctl(vm->dev_kvm_fd, KVM_CREATE_VM, 0);
	if (vm->vm_fd < 0) {
		perror("KVM_CREATE_VM");
		exit(1);
	}

	/*
	 * [STEP 5]: Reserve a region in the host's userspace 
	 * memory that will be used as the region where the 
	 * execution will start when the processor boots after 
	 * a reset.
	 */
	void* reset_mem = mmap(
		NULL, 
		0x10000, 
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, 
		-1, 0
	);
	if (reset_mem == MAP_FAILED) {
		perror("mmap mem");
		exit(1);
	}

	/* A hint to the kernel to enable KSM. */
	madvise(reset_mem, 0x10000, MADV_MERGEABLE);

	/* [STEP 6]: Set the metadata about the reset_mem_region */

	/* Where the memory exists in the host process? */
	reset_mem_region.userspace_addr = (unsigned long)(reset_mem);

	/*
	 * Where the host process's memory appears in the 
	 * guest's physical address space?
	 */
	reset_mem_region.guest_phys_addr = 0xffff0000;

	/* The size of the memory. */
	reset_mem_region.memory_size = 0x10000;

	reset_mem_region.slot  = 0;    /* # guest memory region. */
	reset_mem_region.flags = 0;    /* [?] */


	/* [STEP 7]: Notify KVM about this memory slot. */
	if (
		ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, &reset_mem_region) < 0
	){
		perror("KVM_SET_USER_MEMORY_REGION");
		exit(1);
	}

	/*
	 * [STEP 8]: Keep a record of this memory slot config in 
	 * the VM struct.
	 */
	vm->guest_mem_slots.reset = reset_mem_region;
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
		memcpy(&memval, (void*)(vm->guest_mem_slots.ds.userspace_addr), 2);
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

int copy_guest_code(const char* filename, void* dest_host_mem){
	FILE* fp = fopen(filename, "rb");

	fseek(fp, 0, SEEK_END);
	size_t file_size = ftell(fp);
	rewind(fp);

	unsigned char* guest_code = malloc(file_size);
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

	struct kvm_userspace_memory_region es_region, cs_region;
	struct kvm_userspace_memory_region ds_region, ss_region;


	/* [STEP 1]: Retreive register state from KVM. */

	if (ioctl(vm->vcpu_fd, KVM_GET_REGS, &regs) < 0) {
		perror("KVM_GET_REGS");
		exit(1);
	}

	if (ioctl(vm->vcpu_fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}


	/*
	 * [STEP 2]: Set the registers as per the Intel x86 
	 * reset configuration.
	 */

	regs.rflags = 0x00000002;    /* Bit 1's mask is 2. */
	regs.rip    = 0x0000FFF0;

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


	/* [STEP 3]: Inform KVM about the updated register state (sync).*/

	if (ioctl(vm->vcpu_fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	if (ioctl(vm->vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}


	/* 
	 * [STEP 4]: Reserve host memory regions for essential 
	 * software initialization.
	 */

	void* es_mem = mmap(
		NULL,
		0x10000,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
		-1, 0
	);
	if (es_mem == MAP_FAILED){
		perror("mmap mem");
		exit(1);
	}

	void* cs_mem = mmap(
		NULL,
		0x10000,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
		-1, 0
	);
	if (cs_mem == MAP_FAILED){
		perror("mmap mem");
		exit(1);
	}

	void* ds_mem = mmap(
		NULL,
		0x10000,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
		-1, 0
	);
	if (ds_mem == MAP_FAILED){
		perror("mmap mem");
		exit(1);
	}

	void* ss_mem = mmap(
		NULL,
		0x10000,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
		-1, 0
	);
	if (ss_mem == MAP_FAILED){
		perror("mmap mem");
		exit(1);
	}


	/* 
	 * [STEP 5]: Set the memory slots with metadata 
	 * corresponding to these memory reiongs.
	 */

	es_region.guest_phys_addr = 0x0;
	es_region.userspace_addr  = (unsigned long)(es_mem);
	es_region.memory_size = 0x10000;
	es_region.slot  = 1;
	es_region.flags = 0x0;

	cs_region.guest_phys_addr = 0x10000;
	cs_region.userspace_addr  = (unsigned long)(cs_mem);
	cs_region.memory_size = 0x10000;
	cs_region.slot  = 2;
	cs_region.flags = 0x0;

	ds_region.guest_phys_addr = 0x20000;
	ds_region.userspace_addr  = (unsigned long)(ds_mem);
	ds_region.memory_size = 0x10000;
	ds_region.slot  = 3;
	ds_region.flags = 0x0;

	ss_region.guest_phys_addr = 0x30000;
	ss_region.userspace_addr  = (unsigned long)(ss_mem);
	ss_region.memory_size = 0x10000;
	ss_region.slot  = 4;
	ss_region.flags = 0x0;


	/* [STEP 6]: Inform KVM about these memory regions. */

	if (ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, &es_region) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION es_region");
		exit(1);
	}

	if (ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, &cs_region) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION cs_region");
		exit(1);
	}

	if (ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, &ds_region) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION ds_region");
		exit(1);
	}

	if (ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, &ss_region) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION ss_region");
		exit(1);
	}


	/* [STEP 7]: Keep a record of these memory slots in the VM struct. */

	vm->guest_mem_slots.es = es_region;
	vm->guest_mem_slots.cs = cs_region;
	vm->guest_mem_slots.ds = ds_region;
	vm->guest_mem_slots.ss = ss_region;


	/*
	 * [STEP 8]: Copy the bootstrap instruction in the reset 
	 * memory region. When it is executed, it will initiate a 
	 * far jump to the CS segment where the guest instructions 
	 * for software initialization are placed.
	 */
	copy_guest_code(
		"./bootstrap.bin", 
		(void*)(vm->guest_mem_slots.reset.userspace_addr + 0xfff0)
	);

	/*
	 * [STEP 9]: Copy the software initialization guest code into 
	 * the CS memory region.
	 */
	copy_guest_code(
		"./guest-setup.bin", 
		(void*)(vm->guest_mem_slots.cs.userspace_addr)
	);

	/* Run the VM. */
	return run_vm(vm);
}

int main(int argc, char **argv)
{
	struct vm   vm;

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
	// printf("The vCPU has entered the guest in real-address mode\n");
	// printf("and the following data structures are initialized:\n");
}
