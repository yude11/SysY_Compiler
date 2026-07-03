#!/bin/bash
# 运行 clang-tidy 检查（并行）
# 用法:
#   sh tidy.sh              — 检查所有源文件
#   sh tidy.sh fix          — 自动修复
#   sh tidy.sh src/IR.cpp   — 只检查指定文件
#   sh tidy.sh -j4          — 指定并行数

cd "$(dirname "$0")"

echo "=== 运行 clang-tidy (并行) ==="
SRC_PATH="$(pwd)/src/"
# -header-filter=src/.* 限制只对用户代码产生诊断
# 2>&1 合并 stderr，最后统计实际 warning 行数
OUTPUT=$(run-clang-tidy -p build -quiet -header-filter="src/.*" "^${SRC_PATH}" 2>&1)
WARN_COUNT=$(echo "$OUTPUT" | grep -c ': warning:')
echo "$OUTPUT"
echo ""
echo "=== 用户代码 warning 总数: $WARN_COUNT ==="

