sh build.sh

./build/compiler -riscv test/hello.c -o test/hello.S

cat test/hello.S

clang test/hello.S -c -o test/hello.o -target riscv32-unknown-linux-elf -march=rv32im -mabi=ilp32
ld.lld test/hello.o -L$CDE_LIBRARY_PATH/riscv32 -lsysy -o test/hello
qemu-riscv32-static test/hello

echo $?
