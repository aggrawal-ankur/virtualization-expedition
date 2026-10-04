# cc -c kvm-hello.c -o kvm-hello.o
# cc -c guest.s -o guest.o        
# ld -T payload.ld -r -o payload.o guest.o
# cc kvm-hello.o payload.o -o kvm-hello
# ./kvm-hello

cc kvm-hello.c -o kvm-hello
nasm guest-setup.s -f bin -o guest-setup.bin
nasm bootstrap.s -f bin -o bootstrap.bin
./kvm-hello

rm kvm-hello guest-setup.bin bootstrap.bin