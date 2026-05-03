sh build.sh

action=${1:-run}
mode=${2:-ir}

if [ "$action" = "run" ]; then
  if [ "$mode" = "ir" ]; then
    build/compiler -koopa test/hello.c -o result/hello.koopa
    cat result/hello.koopa
  elif [ "$mode" = "asm" ]; then
    build/compiler -riscv test/hello.c -o result/hello.asm
    cat result/hello.asm
  else
    echo "Usage: $0 [run|test] [ir|asm]"
    exit 1
  fi
elif [ "$action" = "test" ]; then
  if [ "$mode" = "ir" ]; then
    autotest -koopa -s lv9 /root/compiler
  elif [ "$mode" = "asm" ]; then
    autotest -riscv -s lv9 /root/compiler
  else
    echo "Usage: $0 [run|test] [ir|asm]"
    exit 1
  fi
else
  echo "Usage: $0 [run|test] [ir|asm]"
  exit 1
fi
