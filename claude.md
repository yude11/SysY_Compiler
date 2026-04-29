# SysY 编译器项目 - AI 上下文

## 项目概述

这是一个 **SysY 编译器** 实现项目，目标是将 SysY 语言（C 语言子集）编译为 Koopa IR（中间表示），再进一步生成 RISC-V 汇编代码。

- **构建系统**: CMake
- **语言**: C++17（`CPP_MODE=ON`）
- **词法/语法分析**: Flex / Bison
- **IR**: 自建内存 IR（指令格式与数据结构**参考** `libkoopa` 的设计，但并非直接调用 `libkoopa` 库）
- **目标架构**: RISC-V

## 每次对话必须参考的规范文件

项目根目录下的 `ref/` 文件夹包含本编译器涉及的全部语言/指令规范。**在回答任何与编译器实现相关的问题前，必须先理解并遵循这些规范**：

| 文件 | 内容 | 何时参考 |
|------|------|----------|
| `ref/SysY-spec.md` | SysY 语言文法与语义约束 | 任何涉及 SysY 语法、语义、类型、作用域的问题 |
| `ref/SysY-runtime.md` | SysY 运行时库函数定义 | 涉及库函数（`getint`, `putint`, `getch`, `putch`, `getarray`, `putarray`, `starttime`, `stoptime`）的问题 |
| `ref/koopa.md` | Koopa IR 规范 | 涉及中间表示生成、IR 指令、类型系统的问题 |
| `ref/libkoopa.md` | Koopa IR C 接口 (`libkoopa`) | 涉及 IR 内存数据结构/遍历方式设计的问题（作为自建 IR 的**设计参考**） |
| `ref/riscv.md` | RISC-V 指令速查 | 涉及汇编代码生成、寄存器分配、指令选择的问题 |

> **重要**: 如果用户问题涉及上述任何领域，**不要凭记忆回答**，必须先读取对应的规范文件，确保答案与规范一致。

---

## 关键规范摘要

### 1. SysY 语言（`ref/SysY-spec.md`）

- **类型**: 仅 `int`，所有表达式均为 `int` 类型。条件判断中 0 为假，非 0 为真。
- **数组**: 支持多维数组，维度长度必须在编译时确定（`ConstExp`）。
- **常量**: 所有常量必须在编译时求值（类似 C++ `consteval`）。
- **作用域**: 块级作用域，局部变量可覆盖全局变量，同名局部变量作用域不可重叠。
- **求值顺序**: 与 C 语言一致，函数参数、数组下标、赋值左右的求值顺序未定义（UB）。
- **逻辑运算**: `&&` 和 `||` 支持短路求值，结果只能是 0 或 1。
- **必须存在 `main` 函数**: 返回类型为 `int`，无参数，作为程序入口。
- **标识符规则**: `[_A-Za-z][_A-Za-z0-9]*`，可与函数名同名。
- **数值常量**: 支持十进制、八进制（`0` 开头）、十六进制（`0x`/`0X` 开头），范围 `[0, 2^31-1]`，不含负号。
- **注释**: 与 C 语言一致，`//` 单行注释，`/* */` 多行注释。

### 2. SysY 运行时库（`ref/SysY-runtime.md`）

库函数**无需在 SysY 程序中声明**即可使用：

| 函数 | 签名 | 说明 |
|------|------|------|
| `getint` | `int getint()` | 从 stdin 读取整数 |
| `getch` | `int getch()` | 读取字符，EOF 返回 `-1` |
| `getarray` | `int getarray(int[])` | 读取一串整数到数组，返回个数 |
| `putint` | `void putint(int)` | 输出整数 |
| `putch` | `void putch(int)` | 输出 ASCII 字符（参数范围 0-255）|
| `putarray` | `void putarray(int, int[])` | 输出 N 个整数，格式 `N: x y z` |
| `starttime` | `void starttime()` | 开启计时器（仅性能测试用例）|
| `stoptime` | `void stoptime()` | 停止计时器 |

### 3. Koopa IR（`ref/koopa.md`）

- **符号**: 具名符号 `@name`，临时符号 `%name`。
- **类型**: `i32`、数组 `[Type, Len]`、指针 `*Type`、函数 `(Type,...): Type`。
- **强类型**: 只需为 `alloc`/函数参数/返回值/基本块参数标注类型，其余自动推断。
- **关键指令**:
  - `alloc`: 内存分配（局部/全局），返回指针类型。
  - `load` / `store`: 内存读写。
  - `getptr`: 指针算术（C 风格指针偏移）。
  - `getelemptr`: 数组元素指针运算（对数组指针索引）。
  - `add`, `sub`, `mul`, `div`, `mod`, `and`, `or`, `xor`, `shl`, `shr`, `sar`, `eq`, `ne`, `lt`, `gt`, `le`, `ge`: 二元运算（仅 `i32`）。
  - `br`, `jump`: 分支与跳转。
  - `call`, `ret`: 函数调用与返回。
  - `global`: 全局变量定义，必须带初始化器（`zeroinit` 或聚合常量）。
- **SSA 扩展**: 支持基本块参数（非 Phi 函数）。
- **函数声明**: `decl @name(Type,...): Type`（如 SysY 库函数）。

### 4. RISC-V（`ref/riscv.md`）

- **寄存器约定**:
  - `ra` (x1): 返回地址，调用者保存。
  - `sp` (x2): 栈指针，被调用者保存。
  - `a0-a7` (x10-x17): 函数参数/返回值，调用者保存。
  - `t0-t6` (x5-7, x28-31): 临时寄存器，调用者保存。
  - `s0-s11` (x8-9, x18-27): 保存寄存器，被调用者保存。
  - `s0/fp` (x8): 帧指针。
- **常用指令**:
  - `li rd, imm`: 加载立即数。
  - `la rd, label`: 加载标号地址。
  - `mv rd, rs`: 寄存器复制。
  - `lw/sw`: 加载/存储 32 位字。
  - `add/addi`, `sub`, `mul`, `div`, `rem`: 算术运算。
  - `slt/sgt`, `seqz/snez`: 比较操作。
  - `beqz/bnez`, `j`, `call`, `ret`: 控制转移。

---

## 项目结构

```
├── src/              # 编译器源代码
│   ├── AST.h         # 抽象语法树定义
│   ├── IR.h / IR.cpp # IR 相关数据结构
│   ├── IRGenerator.h / IRGenerator.cpp   # IR 生成器
│   ├── AssemblyGenerator.h / AssemblyGenerator.cpp  # 汇编生成器
│   ├── SymbolTable.h # 符号表
│   └── ...           # 其他源文件（含 Flex/Bison 文件）
├── ref/              # 规范参考文件（见上文）
├── build/            # 构建输出目录
├── test/             # 测试用例
├── CMakeLists.txt    # CMake 配置
└── build.sh          # 构建脚本
```

## 编码与修改原则

1. **优先读取规范**: 修改任何涉及语法、语义、IR、汇编生成的代码前，先读取 `ref/` 中对应的规范文件。
2. **最小修改**: 尽量只做必要的改动，避免大范围重构。
3. **保持一致性**: 新代码的风格应与现有代码保持一致。
4. **构建验证**: 修改后应能通过 `cd build && cmake .. && make` 编译成功。
5. **类型安全**: Koopa IR 是强类型的，生成 IR 时注意类型标注（尤其是 `alloc`、函数参数、返回值）。
6. **符号命名**: IR 中全局符号和局部符号均不可重名，需要时进行自动换名处理。
7. **自建 IR**: 本项目的 IR 是自行实现的内存数据结构，参考了 `libkoopa` 的设计理念（如 `raw_slice`、`raw_value`、`raw_function`、`raw_basic_block` 等结构），但所有 IR 生成、遍历、输出逻辑均为自行实现，不依赖 `libkoopa` 库。

## IR 生成问题排查流程

当遇到"某个语言特性/概念在生成 IR 时该如何表示"的问题时，按以下步骤思考：

1. **先查 `ref/koopa.md` 的语法定义和示例**：Koopa IR 规范是最终权威。找到对应特性的语法（如函数参数 `FunParams ::= SYMBOL ":" Type`）和示例代码，明确它在文本 IR 中的具体形式。
2. **映射到内存 IR 的 Value 类型**：Koopa IR 文本中的概念对应到内存 IR 的哪个 `Value` 子类？
   - 符号引用（如函数参数 `@a`、变量名 `@x`）→ `Value_REF`
   - 指令产生的值（如 `%0 = add ...`）→ 对应指令类（`Value_BINARY` 等）
   - 整数常量 → `Value_INTEGER`
   - **不要为"已经存在的值"发明新的指令类型**（比如函数参数不是 `alloc` 出来的）。
3. **区分"源语言语义"与"IR 表示"**：
   - SysY 参数可重新赋值 → 需要在 entry block 中 `alloc` 局部内存 + `store` 参数值进去。
   - 但**参数值本身**（作为 `store` 的 source）就是 `Value_REF`，不是 `alloc` 指令的产物。
4. **检查 `Value_Type` 枚举是否已有预留类型**：`type.h` 中的 `Value_Type` 枚举（如 `KOOPA_RVT_FUNC_ARG_REF`）反映了 IR 设计的语义分类，遇到问题先看是否有对应类型可循。
5. **修改 IR 结构后，必须同步检查输出路径**：
   - `IROutputer`（文本 IR 输出）：结构变动是否导致输出格式不符合 `koopa.md` 规范？
   - `AssemblyGenerator`（汇编生成）：新的 Value 类型是否在 codegen 中被正确处理？
