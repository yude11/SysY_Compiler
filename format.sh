#!/bin/bash
find src/ -type f -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs clang-format -i