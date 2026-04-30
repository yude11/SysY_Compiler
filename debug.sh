sh build.sh

mode=${1:-ir}

if [ "$mode" = "ir" ]; then
  gdb --args build/compiler -koopa test/hello.c -o result/hello.koopa
elif [ "$mode" = "asm" ]; then
  gdb --args build/compiler -riscv test/hello.c -o result/hello.asm
else
  echo "Usage: $0 [ir|asm]"
  exit 1
fi
