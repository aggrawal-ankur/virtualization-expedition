cc -c kvm-hello.c -o kvm-hello.o
cc -c guest.s -o guest.o        
ld -T payload.ld -r -o payload.o guest.o
cc kvm-hello.o payload.o -o kvm-hello
./kvm-hello