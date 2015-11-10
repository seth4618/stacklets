# export CFLAGS=-m32
# we are building the tool and application under the 32bit mode (IA32)
make clean
rm -Rf obj-ia32
make obj-ia32/cache_sim.so TARGET=ia32
../../../pin.sh -t obj-ia32/cache_sim.so -- ../../../a.out
