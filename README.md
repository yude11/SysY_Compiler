# SysY Compiler

将 SysY 语言（C 语言子集）编译为 Koopa IR，再生成 RISC-V 汇编代码的编译器实现。

## 功能特性

- **词法/语法分析**：基于 Flex + Bison，完整支持 SysY 文法
- **语义分析**：常量折叠、类型检查、作用域管理
- **IR 生成**：自建内存 IR，生成符合 Koopa IR 规范的中间表示
- **RISC-V 汇编生成**：从 IR 生成 RISC-V 32 位汇编

## 项目结构

```
├── src/
│   ├── sysy.l                  # Flex 词法分析器定义
│   ├── sysy.y                  # Bison 语法分析器定义
│   ├── AST.h                   # 抽象语法树节点定义
│   ├── visitor.h               # AST / IR 访问者模式接口
│   ├── type.h                  # 类型系统（枚举与类型信息）
│   ├── SymbolTable.h           # 符号表与作用域管理
│   ├── IR.h                    # IR 数据结构定义（Value 子类体系）
│   ├── IR.cpp                  # IR 实现
│   ├── IRGenerator.h           # IR 生成器声明
│   ├── IRGenerator.cpp         # IR 生成器实现（AST → IR）
│   ├── IRNameManager.h         # IR 符号命名管理
│   ├── AssemblyGenerator.h     # 汇编生成器声明
│   ├── AssemblyGenerator.cpp   # 汇编生成器实现（IR → RISC-V ASM）
│   ├── log.h                   # 日志宏
│   └── main.cpp                # 入口：命令行解析与编译流水线
├── ref/                        # 语言/指令规范参考
│   ├── SysY-spec.md            # SysY 语言规范
│   ├── SysY-runtime.md         # SysY 运行时库规范
│   ├── koopa.md                # Koopa IR 规范
│   ├── libkoppa.md             # libkoopa C 接口参考
│   └── riscv.md                # RISC-V 指令速查
├── test/                       # 测试用例
│   └── hello.c                 # 示例 SysY 程序
├── result/                     # 编译器输出（.koopa / .asm）
├── build/                      # CMake 构建输出
├── CMakeLists.txt              # CMake 配置
├── docker_start.sh             # 启动 Docker 容器
├── build.sh                    # 编译项目
├── run.sh                      # 构建并运行/测试
├── debug.sh                    # 构建并启动 GDB 调试
└── riscv.sh                    # 完整 RISC-V 编译→链接→QEMU 运行
```

## 环境搭建

项目依赖 [compiler-dev](https://github.com/pku-minic/compiler-dev) Docker 环境，其中包含 Flex、Bison、Koopa IR 工具链、RISC-V 工具链及 QEMU。

```bash
# 启动并进入 Docker 容器，需要提前下载镜像并创建容器
sh docker_start.sh
```

后续所有命令均需在容器内执行。

## 构建与运行

### 编译项目

```bash
sh build.sh
```

CMake 将在 `build/` 目录下生成名为 `compiler` 的可执行文件。

### 运行编译器

```bash
# 编译为 Koopa IR
sh run.sh run ir

# 编译为 RISC-V 汇编
sh run.sh run asm
```

也可直接调用编译器：

```bash
# 输出 Koopa IR
build/compiler -koopa test/hello.c -o result/hello.koopa

# 输出 RISC-V 汇编
build/compiler -riscv test/hello.c -o result/hello.asm
```

### 自动评测

```bash
# Koopa IR 自动评测
sh run.sh test ir

# RISC-V 汇编自动评测
sh run.sh test asm
```

### GDB 调试

```bash
sh debug.sh ir    # 调试 IR 生成
sh debug.sh asm   # 调试汇编生成
```

### 完整 RISC-V 执行

一键完成编译→汇编→链接→QEMU 运行：

```bash
sh riscv.sh
```