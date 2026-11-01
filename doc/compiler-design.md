# ShaoLang 编译器设计文档

配套文档：[语言语法](grammar.md) · [English version](compiler-design_EN.md)

---

## 1. 项目定位

ShaoLang 编译器是一个**单遍（one-pass）语法导向翻译（syntax-directed translation）编译器**：

- 没有独立的 AST 阶段。语法分析函数在归约的同时直接调用 IRGenerator，把语句/表达式翻译成三地址码。
- 默认产物是**可执行文件**：优化后的三地址码先翻成**目标机汇编**（架构有 ARM64 / X86 / X64 三种，封装按平台取 Mach-O、ELF 或 COFF），再交给本机工具链装配、链接。只要 -S 就停在汇编，要 -ir 才打印**中间代码文本**（优化前 + 优化后各一次）。
- 优化只有一个 pass：函数级常量传播（含代数化简与常量条件跳转化简），建立在自研的控制流图 + 迭代数据流框架上。
- 支持多文件命令行，但文件之间互相独立，符号表不共享。

### 1.1 流水线

```
源文件（每个文件独立编译一次）
  └─> FileScanner      commons/scanner.cpp     128B 缓冲、行列定位、EOF 哨兵
      └─> Lexer        lexer.cpp               记号流（-t 时由 Parser::move 打印）
          └─> Parser   parser.cpp             LL(1) 递归下降，语义动作内联
              ├─> SymbolTable    symbols/*     作用域路径式变量表 / 函数表 / 字符串表
              └─> IRGenerator    ir_generator.cpp
                     │            表达式与语句 → 三地址码
                     ▼
              Function.interCodeCollection   （每个函数一份线性指令序列）
                     │
                     ▼  optimize()：逐函数执行一次
              DataFlowGraph   dataflow/*      建块、连边、可达性
                     └─> ConstPropagation     optimization/*  格迭代 + 两轮化简
                              └─> Function.optCode
                                    │
                    ┌───────────────┴───────────────┐
                    ▼ -ir                            ▼ -S / 缺省
              show_all_code() 打印 IR        五个后端按三元组挑一个    codegen/*
                                            （GNU 语法 + MASM 语法，
                                              Mach-O / ELF / COFF 汇编）
                                                │     └─ -o 无 -S：Linker 调 ml/link 或 cc + codesign 出可执行文件
```

SymbolTable 与 IRGenerator 互相持有指针：登记变量这一步就会生成 OP_DECLARE（见 §3.4）。

两条出口由 Compiler::compile() 分岔（compiler.cpp:17-31）：带 -ir 时是解析 → 打印原始 IR → optimize() → 打印优化后 IR，否则解析完就直接返回（23-26），IR 不打印。汇编由另一个入口 Compiler::emit_assembly()（compiler.cpp:33-55）产出，它调 optimize()（Function::do_optimize 有 is_opt 幂等保护，重复调用是空操作），再按架构与平台五选一：Windows 的两个 x86 / x64 目标走 MASM 语法的 X86MasmCodeGenerator / X64MasmCodeGenerator，其余 x86 / x64 目标走 GNU 语法的 X86CodeGenerator / X64CodeGenerator，ARM64 走 AArch64CodeGenerator。五个后端读的是同一份优化后 IR，区别只在落地方式。

---

## 2. 模块地图

| 路径 | 职责 | 关键类型 |
|---|---|---|
| src/shaolang.cpp | CLI：参数解析、按文件构造流水线、汇总诊断数 | main |
| src/compiler.{h,cpp} | 组装 Lexer/Parser/SymbolTable/IRGenerator 并驱动 | Compiler |
| src/commons/common.h | 全局枚举与文案表：记号、语法错误、语义错误、IR 操作码；前向声明 | LexicalType syntaxError semanticError InterCodeOperator |
| src/commons/scanner.{h,cpp} | 带缓冲的字符流 + 行列定位 | FileScanner |
| src/commons/token.{h,cpp} | 记号对象（含 ID/int/char/string 四类携带值） | Token IDToken IntegerToken CharToken StringToken |
| src/lexer.{h,cpp} | 记号识别、注释跳过、字面量取值、词法错误恢复 | Lexer |
| src/parser.{h,cpp} | 递归下降语法分析 + 错误恢复 + 语义动作 | Parser |
| src/symbols/variable.{h,cpp} | 变量/常量/临时量模型，含类型、初值、作用域路径、左值属性、优化编号 | Variable |
| src/symbols/function.{h,cpp} | 函数符号：形参、指令集合、返回点、优化后代码 | Function |
| src/symbols/symbol_table.{h,cpp} | 作用域路径式变量表、函数表、字符串常量表、指令派发 | SymbolTable |
| src/ir/inter_code.{h,cpp} | 三地址指令记录（5 种构造形态）+ 打印 | IntermediateInstruct |
| src/ir/inter_code_collection.{h,cpp} | 线性指令序列 + leader 标记 | InterCodeCollection |
| src/ir_generator.{h,cpp} | 表达式/语句 → 三地址码的下落规则，跳转回填栈 | IRGenerator |
| src/dataflow/block.{h,cpp} | 基本块：内部指令 + prev/next 边 + in/out 值集 | Block |
| src/dataflow/data_flow_graph.{h,cpp} | 建块、连边、可达性、死块摘边、导出优化后线性码 | DataFlowGraph |
| src/optimization/const_opt.{h,cpp} | 常量格迭代分析 + 代数化简 + 常量跳转化简 | ConstPropagation |
| src/codegen/target.{h,cpp} | 编译目标三元组（架构 × 平台）、--target= 解析、支持性判定 | CompileTarget Architecture SystemPlatform |
| src/codegen/aarch64_codegen.{h,cpp} | 优化后 IR → ARM64 汇编：帧分配、调用约定、静态数据、两种目标格式 | AArch64CodeGenerator |
| src/codegen/x86_codegen.{h,cpp} | 优化后 IR → GNU 语法 i386 汇编：64 位值拆成字对、cdecl 全栈实参、内置长除子程序，一份产码出 ELF / Mach-O | X86CodeGenerator |
| src/codegen/x64_codegen.{h,cpp} | 优化后 IR → GNU 语法 x86_64 汇编：System V x64 ABI（六个实参寄存器、无影子空间），出 ELF | X64CodeGenerator |
| src/codegen/x86_masm_codegen.{h,cpp} | 优化后 IR → MASM 语法 i386 汇编（windows-x86 的 PE/COFF）：与 GNU 版同一套字对 lowering，装饰换成 .model flat, C + PROC/ENDP | X86MasmCodeGenerator |
| src/codegen/x64_masm_codegen.{h,cpp} | 优化后 IR → MASM 语法 x86_64 汇编（windows-x64 的 PE/COFF）：Microsoft x64 ABI，四个实参寄存器加 32 字节影子空间 | X64MasmCodeGenerator |
| src/codegen/linker.{h,cpp} | 用本机工具链把汇编装配、链接成可执行文件：Windows 走 ml/ml64 + link，其余走 cc，macOS 补 ad-hoc 签名 | Linker |
| src/commons/type_util.{h,cpp} | 类型归一、相容判定、偏移步长、类型宽度 | TypeUtil |
| src/commons/error.{h,cpp} | 静态诊断输出与计数 | Error |
| src/commons/argument.{h,cpp} | 静态开关（-t / -b / -S / 生成可执行文件） | CompilerArguments |

依赖方向基本是**单向**的：commons ← symbols/ir ← ir_generator ← parser ← compiler ← shaolang.cpp；dataflow/optimization 只被 Function::do_optimize() 使用，codegen 读的是优化后的序列（Function::get_emitted_code()，function.cpp:111-116），但它会把帧偏移写回 Variable 符号本身（见 §5.10）。common.h 里集中前向声明以避免循环 include。

---

## 3. 阶段设计

### 3.1 FileScanner（commons/scanner.cpp）

- BUFFER_SIZE = 128 的 fread 缓冲，move() 一次返回一个字符，到文件尾返回 EOF_CHAR = (char)-1（scanner.h:12-15）。选 (char)-1 而不是 EOF 的 int 形式，是为了不与 UTF-8 字节冲突。
- 行列号靠 #ifdef LF / #elif CRLF 分支在换行时递增（scanner.cpp:64-74）；LF/CRLF 由 CMakeLists.txt 按平台定义，两个都没定义时行号恒为 1。
- 打开失败时打印一条提示但**不中止**，之后 move() 一直返回 EOF（scanner.cpp:47-51），因此坏路径的文件表现为「空文件」。

### 3.2 Lexer（lexer.cpp）

- 主循环 next_token()：跳空白 → 按首字符分类派发（标识符 / " / ' / 数字 / 运算符）；文件尾产出并缓存一个 END 记号。
- 关键字用匿名 namespace 里的静态表 + 线性查表（lexer.cpp:20-43），未命中即 ID。
- 双字符运算符在 read_operator() 的 switch 里前瞻一格处理（lexer.cpp:242-296）；/ 之后是 / 或 * 时走注释分支并**返回 NULL**。
- 错误恢复策略：**词法错误不产生记号**。read_number/read_char/read_string/invalid_operator 报错后返回 NULL，主循环 continue 重新扫描（lexer.cpp:78-81）。这样 1abc、1.5、未闭合字符串只会污染一个记号位置。
- 所有权：current_token 在被替换前 delete，因此 Token* 只在「下一次调用之前」有效——Parser 必须在下一个 move() 前用完它。

### 3.3 Parser（parser.cpp）

LL(1) 递归下降，每个非终结符一个 void/Variable* 成员函数，函数头上的注释写的就是该文法产生式（如 parser.cpp:77-79）。

- **选集用宏表达**（parser.cpp:14-22）：ALL_TYPES、ALL_EXPR_KW、ALL_STATEMENT_KW 等，本质是 FIRST 集合的手写近似。这些宏是语法分发唯一的依据，也因此成为语法缺口的来源——一条语句的关键字只要没进 ALL_STATEMENT_KW，就永远到不了 statement()（输入输出的 K_INPUT/K_OUTPUT 是后来补进去的，parser.cpp:22）。
- **匹配**：check_and_move(type)（parser.cpp:69-75）匹配则消费。
- **恢复**：recovery(in_follow_set, lost, error[, delete_all])（parser.cpp:45-67）。in_follow_set 为真 → 判定「缺失」，只报 xxx_LOST，不消费（相当于插入空产生式继续）；为假 → 报 xxx_ERROR，默认丢弃 1 个记号，delete_all 版本则一直跳到 )、}、;、END。这是简化的 panic mode，不做产生式级重入。
- **语义动作内联**：value_definition 直接返回 Variable*，expr 链每个函数都返回「结果存储点」（Variable*），跳转回填通过 IntermediateInstruct*& 出参传出（while_statement、if_statement）。
- 因为没有 AST，**「先使用后声明」在结构上不可行**：调用点解析时就要在函数表里查到目标。

### 3.4 SymbolTable（symbols/symbol_table.cpp）

| 数据结构 | 作用 |
|---|---|
| unordered_map&lt;string, vector&lt;Variable*>*> variableTable | 同名变量按作用域路径分层共存；字面量常量也塞进这张表（不参与重复检查，symbol_table.cpp:78-80） |
| vector&lt;int> scope_vector | 当前作用域路径；enter_new_scope() 递增 current_scope_id 并压入，exit_current_scope() 弹出（文件级作用域不弹，symbol_table.cpp:34-42） |
| unordered_map&lt;string, Function*> functionTable | **每个名字一个槽位**，所以同名不同参不是重载；配合 has_declare 标志区分「原型 / 定义 / 重复定义」 |
| vector&lt;Variable*> globalVariables | 深度为 1 的变量清单，供常量传播取全局初值与 kill 集合 |
| unordered_map&lt;string, Variable*> constStringTable | 字符串常量表，目前只写不读 |
| variable_define_order_list / function_define_order_list | 后者是三处「按定义顺序」的唯一依据：打印 IR、逐函数优化、以及后端产码（get_defined_functions，symbol_table.cpp:197-208 会滤掉只有原型的声明）；前者仍只写不读 |

查找算法 get_variable()（symbol_table.cpp:106-127）：在候选列表里挑「作用域路径是当前路径前缀且最长」的那个，即就近可见。找不到报 使用未定义变量 并返回 NULL，上层运算对 NULL 容忍（静默返回）。

**关键耦合点**：add_variable() 末尾会调用 irGenerator->generate_variable_init(variable)（symbol_table.cpp:100-102）。也就是说「符号表登记」这个动作本身就产生 OP_DECLARE 指令；IRGenerator 也反向持有 SymbolTable。两者互相依赖，构造函数注入（compiler.cpp:12-14）。

指令派发 add_inter_instruct()（symbol_table.cpp:209-215）：只有 current_function 非空时才追加，否则 delete。这就是「全局作用域的表达式指令被静默丢弃」的实现来源。

### 3.5 Variable（symbols/variable.cpp）

一个类同时承担 5 种角色，靠标志位区分：

| 构造 | 用途 | 关键标志 |
|---|---|---|
| Variable(name, scope, type, init) | 用户变量声明 | is_left_value = true |
| Variable(Token*) | 字面量，名字固定 &lt;&lt;int&gt;&gt; / &lt;&lt;char&gt;&gt; / &lt;&lt;string&gt;&gt; | is_constraint = true, is_left_value = false |
| Variable(int) | 优化用的整型常量对象 | 同上 |
| Variable(scope, type, is_ptr) / Variable(scope, var) | 运算临时量，名字 .L&lt;tempId++> | is_temp_store = true |
| Variable() | 全局共享的 void 值 | TK_VOID + is_pointer |

字段分三类：类型/值（type、integer_initial_value、char_initial_value、str_initial_value）、引用属性（is_pointer、is_array、array_length、ptr，由文法的 int *p / int a[n] / a[i] / *p 写入，见语法文档 §2.4）、存储位置（frame_offsize 由后端的 allocate_frame 写入并读取，Function 侧的 frame_max_depth、current_esp_ptr、scope_esp 仍只是占位）。

引用属性里有两个概念值得单独说：**单元（cell）**是 ptr 非空且 ptr != this 的 Variable（is_cell()），它代表「存放在 *ptr 那一格里的值」，可以出现在赋值号左边；**地址值**是既非基本类型又非单元的 Variable（TypeUtil::is_address_value），即指针变量、数组名、存放地址的临时量。两者的区别决定了取值、类型检查与自增步长。

opt_index 是数据流分析给变量分配的格下标，check_init()/is_init() 决定「字面量初值可折进声明」还是「需要额外赋值指令」。

静态计数器 Variable::tempId（variable.cpp:17）同时被临时变量命名（variable.cpp:57）和指令标号生成（inter_code.cpp:113）使用，因此 .L&lt;n> 名字空间是**两者共用、全局单调**的——跨函数不会撞名，但也不能靠数字推断类别。

### 3.6 中间代码（ir/inter_code.{h,cpp}）

指令记录是固定 6 槽位的三地址形态 + 标号：

```
label | op | resultVar | leftArg | rightArg | jumpTarget | function | block
```

5 个构造器对应 5 种使用形态：(op, result, left, right) 算术/比较；(op, fun, result) 调用（result 为空即 OP_PROC）；(op, var) 声明/压参；(op, target, left, right) 跳转；() 空指令即**标号伪指令**（inter_code.cpp:41-44）。

跳转目标是**指针而不是索引**，因此回填零成本：语句 lowering 时先 new IntermediateInstruct() 占位，语义完成时把占位对象本身 add_inter_instruct() 进序列（ir_generator.cpp:356-401）。

操作码一览（枚举 common.h:147-161，与之逐项对应的文案表 common.h:163-176，display 文本见 inter_code.cpp:162-205）：

| 操作码 | 打印形式 | 生成位置 |
|---|---|---|
| OP_EMPTY_INST | .L&lt;n>（只有标号行） | while/if/函数返回点的占位标号 |
| OP_DECLARE | dec v | generate_variable_init |
| OP_ENTER / OP_EXIT | entry / exit | generate_function_head/tail |
| OP_ASSIGN | r = l | generate_assign |
| OP_ADD/SUB/MUL/DIV/MOD | r = l op rr | 二元运算；inc/dec 也复用它（r = r + step） |
| OP_MINUS / OP_NOT | r = -l / r = !l | 一元运算 |
| OP_GT/GE/LT/LE/EQ/NEQ | r = l cmp rr | 比较（结果恒 int 临时量） |
| OP_AND / OP_OR | r = l && rr | 逻辑运算（**非短路**） |
| OP_LEA / OP_PTR_GET / OP_PTR_SET | r = &l / r = *l / *l = r | 数组下标、取地址、解引用（§3.7）；OP_PTR_SET 的 resultVar 恒为空 |
| OP_JMP | goto L | while 回边、if/else 汇合、break/continue |
| OP_JMP_FALSE | if c no true jmp to L | while / if 条件 |
| OP_JMP_TRUE / OP_JMP_NEQ | — | 前端不生成，只在 is_jmcond() 与跳转化简里被处理 |
| OP_ARG | arg v | 实参压栈 |
| OP_PROC | f( | void 调用 |
| OP_CALL | r = f() | 有返回值调用 |
| OP_READ | read v | input 语句（ir_generator.cpp:440-453） |
| OP_PRINT | print v | output 语句（ir_generator.cpp:455-461） |
| OP_RETURN / OP_RETURN_WITH_VAL | goto L / return v then goto L | return 语句，目标为函数返回点 |

两个小瑕疵值得修：OP_MOD 打印成 %%（inter_code.cpp:177），default 分支无条件解引用 resultVar（inter_code.cpp:202）——OP_LEA/OP_PTR_SET/OP_JMP 之外的异常指令一旦落到这里就是空指针。

### 3.7 IR 下落规则（ir_generator.cpp）

**表达式**：每个运算都「新建临时量 + 一条三地址指令」，不做复用、不产生寄存器分配信息。

```
a + b            →  dec .Lt ; .Lt = a + b        (generate_two_value_op → generate_add)
a = b            →  a = b                        (结果返回左变量 a)
a = b = c        →  b = c ; a = b                (assign_extend 对结果再扩展)
++a              →  a = a + step                 (generate_inc_first，返回 a)
a++              →  dec .Lt ; .Lt = a ; a = a + step   (generate_inc_last，返回 .Lt)
!x / -x          →  dec .Lt ; .Lt = !x / -x
f(1, 2)          →  arg 2 ; arg 1 ; dec .Lt ; .Lt = f()   (实参逆序压栈，ir_generator.cpp:70-72)
g()  (void)      →  arg ... ; g(                 (OP_PROC，返回共享 voidVariable)
input a          →  read a                       (generate_read，目标是已声明变量本身)
&p               →  dec .Lt ; .Lt = &p            (generate_lea → address_of，字面量与非左值先报 左值表达式不正确)
*p               →  dec .Lt ; .Lt = *p            (generate_deref 造出单元，value_of 再补 OP_PTR_GET)
a[i]             →  dec .Lt ; .Lt = &a ; .Lt2 = i * 8 ; .Lt3 = .Lt + .Lt2 ; dec (*.Lt3)   (generate_index，单元只描述位置)
a[i] = v         →  *.Lt3 = v                     (generate_assign 对单元出 OP_PTR_SET，resultVar 为空)
input a[i]       →  read (*p)                     (OP_READ 的目标是单元，写入地址取单元里的指针值)
output e         →  <e 的求值链> ; print .Lt      (generate_print 收 all_expr 的结果)
```

自增步长来自 TypeUtil::get_offset_step()：地址值（指针、数组名）为 8，单元与普通值为 1（type_util.cpp）。因此 p++ 前进一个槽位，a[i]++ 与 x++ 一样按 1 变化。

**控制流**：head_link / tail_link 两个平行栈只由 while push/pop，break 跳 tail_link.back()、continue 跳 head_link.back()（ir_generator.cpp:403-419），因此 if 内的 break 天然指向外层 while；栈空即报「不在 while 语句下」。

```
while (c) B      →  L_head: dec .t; .t = c; if .t no true jmp L_exit
                        B
                    goto L_head
                    L_exit:
if (c) A else B  →  dec .t; .t = c; if .t no true jmp L_else
                        A ; goto L_exit ; L_else: B ; L_exit:
```

注意 if 的 L_else 占位标号在条件指令之后才被插入序列，因此标号行出现在它自己的跳转指令之后（示例输出里 .L6 位于 y = 5 之后）。

**函数**：OP_ENTER → 体 → 返回点（空指令标号）→ OP_EXIT。return 只跳到返回点，所以「函数末尾统一出口」天然存在，generate_function_tail 负责落地（ir_generator.cpp:477-487）。

### 3.8 控制流图（dataflow/*）

建块分两步：

1. InterCodeCollection::mark_all_first_inst() 找 leader（inter_code_collection.cpp:15-39）：序列第 1、2 条与最后一条，每条跳转/条件跳转的**后继**，以及每个跳转**目标**。注意「前两条强制为 leader」是实现的额外规则，所以 entry 总是单独成块。
2. DataFlowGraph::create_new_blocks() 顺序切块，link_blocks() 连边（data_flow_graph.cpp:41-68）：块尾不是 OP_JMP（is_jmp() 只判无向跳转，条件跳转仍算）→ 加顺序边；是跳转/条件跳转 → 加指向目标块的边。

OP_RETURN* 的 target 是函数返回点而非跳转语义，因此返回后仍有顺序边指向 exit 块，出口块恒可达。

边维护是显式双向的：link() 去重后同时加 prev/next，un_link() 删双向边并递归 delete_all_next_link() 摘除随之变死的块（先判 is_reachable()，不可达才剪，data_flow_graph.cpp:97-114）。write_opt_code() 按块序导出可达块的指令，得到优化后的线性码（data_flow_graph.cpp:169-180）。

-b 的块转储（Block::to_string）：

```
---------------------------------
Block:1
- Prev:
0
- Next:
2 , 3
- Code:
dec s
dec .L1
.L1 = n > 1
if .L1 no true jmp to .L2
```

### 3.9 常量传播（optimization/const_opt.cpp）

标准**有限高度格上的前向流分析**，Function::do_optimize() 在建图后调用一次（function.cpp:95-109）。

- 格：NAC = LONG_MIN（非常量）< 常数 c / UNDEF = LONG_MAX（未定）。meet 是 ux()（const_opt.cpp:139-151）：任一 NAC → NAC；UNDEF 单位元；两常数不等 → NAC。
- 变量编号（opt_index）覆盖三类：全局变量（用 check_init() 折进声明的字面量作初值，未初始化 UNDEF，非基本类型 NAC）、形参（一律 NAC）、本函数 OP_DECLARE 的局部/临时量。开工前先把这三类的 opt_index 复位为 -1，避免相邻两次分析残留（const_opt.cpp:22-33）。
- 边界条件：入口块（block 0）的 out 直接是初值向量，其余块 in = out = UNDEF…（const_opt.cpp:90-96）。
- 迭代：按块号从 1 到 n-1 **轮转**，每轮 union_blocks()（多前驱按位 meet）+ transform_block()（块内逐指令 fx），任何一块 out 变化就继续（const_opt.cpp:97-106）。没有工作队列，复杂度是 O(轮数 × 块数)。
- fx 转移函数（const_opt.cpp:176-304）：表达式指令按操作数格值求值（除零、模零 → NAC）；OP_READ 把目标变量刷成 NAC（外部输入在本次分析内不可知）；OP_PRINT 不改变任何值；OP_PTR_SET 与非基本类型的 OP_ARG 把**整个**值集刷成 NAC；OP_LEA/OP_PTR_GET 只把结果置 NAC；OP_PROC/OP_CALL kill 所有全局变量与调用结果。
- 化简一：algebra_simplify()（const_opt.cpp:306-456）
  - 结果已知为常数 → 整条指令改写成 storePoint = 常数；
  - 已知的恒等/零元规则：x+0→x、0-x→-x、x-0→x、x*0→0、x*1→x、0/x→0、x/1→x、x%0→0、x%1→0、0&&x→0、非0&&x→x!=0、非0||x→1、0||x→x!=0；
  - 否则把已求得的一侧操作数替换成常数对象（部分折叠）；
  - OP_ARG / OP_RETURN_WITH_VAL 的操作数已知常数时直接换成常数。
  新常数通过 symbolTable->add_variable() 登记——因为是字面量，generate_variable_init 立即返回、不产生指令（symbol_table.cpp:56-61 与 ir_generator.cpp:465-467）。
- 化简二：condition_jump_simplify()（const_opt.cpp:472-531）
  - 条件为常数 → 判定「是否跳转」；
  - 跳且目标就是顺序后继 → 删掉指令（自然落入）；目标不是后继 → 退化成 OP_JMP；
  - 不跳 → 删指令、补顺序边，若目标不是后继再 un_link 摘掉跳转边（注释里明确了「先补顺序边再摘跳转边」的顺序依赖）。

实测（-[int] main(){ int x = 1 + 2 * 3; int y = x * 1 + 0; if (y > 10) y = 5; else y = 6; return y; }）：

```
优化前                                优化后
dec .L1                               dec .L1
.L1 = 2 * 3                           .L1 = 6
dec .L2                               dec .L2
.L2 = 1 + .L1                         .L2 = 7
dec x                                 dec x
x = .L2                               x = 7
.L3 = x * 1                    →      .L3 = 7
.L4 = .L3 + 0                         .L4 = 7
...                                   ...
.L5 = y > 10                          .L5 = 0
if .L5 no true jmp to .L6             goto .L6
```

### 3.10 后端：ARM64 / X86 / X64 汇编生成与本机链接（codegen/*）

-S 把优化后的三地址码翻成目标机汇编，目标由 --target= 决定（codegen/target.cpp：host_target 先看平台宏 MAC_OSX / LINUX / WINDOWS，再靠编译器自身的 __APPLE__ / __linux__ / _WIN32 与架构宏兜底，所以不经 CMake 直接编译也能识别本机；parse_target 认 macos-arm64 / darwin-arm64 / apple-arm64 / linux-arm64 / aarch64-linux / macos-x86 / darwin-x86 / osx-x86 / linux-x86 / i386-linux / x86-linux / windows-x86 / win-x86 / i386-windows / linux-x64 / x86_64-linux / amd64-linux / windows-x64 / win-x64 / amd64-windows；target_supported 放行其中七种三元组，windows-arm64 单独给出不支持的原因，macos-x64 也落在兜底的拒绝列表里）。Compiler::emit_assembly（compiler.cpp:33-55）先按架构再按平台挑后端：Windows 的两个 x86 / x64 目标用 MASM 语法的 X86MasmCodeGenerator / X64MasmCodeGenerator（微软的 ml / ml64 只认 Intel 语法），其余 x86 / x64 用 GNU 语法的 X86CodeGenerator / X64CodeGenerator，ARM64 用 AArch64CodeGenerator。五个后端读的是同一份优化后 IR，区别只在落地方式。不给 -S 也不给 -ir 时，编译器的缺省任务就是把这份汇编交给本机工具链装配、链接成可直接运行的可执行文件（见本节末）。

下面先讲 ARM64，它是最早也是结构最直白的一版；X86 与 X64 各自的差异集中在 ARM64 段落之后的两组小段。

寄存器布局是**静态写死的**（aarch64_codegen.cpp:11-24）：x0 装左值、x1 装右值、x2 装结果、x3 装取模的商，x0..x7 同时就是传参寄存器（VALUE_REGISTERS），x10 只用于地址计算、x11 只用于栈实参搬运，故意避开 x0..x7。没有寄存器分配阶段：每个有存储的变量始终待在栈槽或静态数据里，寄存器只是每条指令内部的临时搬运工。

顶层流程（aarch64_codegen.cpp:51-62）：输出 .text（Mach-O 额外加 .subsections_via_symbols，让汇编器知道每个符号可自成数据段）→ 按**定义顺序**逐函数生成 → 末尾统一出静态数据。函数清单来自 SymbolTable::get_defined_functions()（symbol_table.cpp:197-207），只有原型没有函数体的声明被排除，因此不会为它们产码；优化器走同一条件（SymbolTable::optimize 跳过 is_declare() 的函数），空指令序列不会再进控制流图。

单函数（generate_function，aarch64_codegen.cpp:64-88）：取 get_emitted_code()（有优化结果就用优化后的，function.cpp:111-116）→ allocate_frame → 符号头（Mach-O 符号带下划线前缀，ELF 还要 .type/@function 与 .size）→ 序言 → 形参落栈 → 逐条翻译 → 若序列里没遇到 OP_EXIT 就补一段退栈序列，防止出口块被优化掉后函数直接落进下一个函数体。

存储模型是**全部 64 位有符号整数**，每个有存储的变量占一个 8 字节槽；storage_of()（371-388）把操作数分成五类：

| StorageKind | 判定 | 取值的写法 |
|---|---|---|
| STORAGE_IMMEDIATE | 字面量 | mov dest, #n；超出单条 mov 编码范围时用 movz 加若干 movk 拼（load_immediate，398-413） |
| STORAGE_STRING | C_STRING 字面量 | 值就是它的静态符号地址（adrp+add） |
| STORAGE_FRAME | frame_offset ≠ 0，即局部变量与临时量 | ldur dest, [x29, #负偏移] |
| STORAGE_STATIC | 作用域路径长度为 1，即文件级全局 | 先取符号地址再 ldr |
| STORAGE_NOWHERE | void / 空 / 已被上面排除的情况 | mov dest, xzr |

最后一类是**静默兜底**：一个既没有帧槽又不是全局的变量会被当成 0，不会报错（§5.9 的实测缺陷正落在这里）。

帧分配（allocate_frame，90-115）先给形参按声明顺序发负偏移（形参的 DECLARE 在指令序列之前，得单独算），再扫一遍 OP_DECLARE 给局部量与临时量发偏移；void、字面量、字符串、文件级全局、以及**已有偏移的变量**都被跳过。**数组按元素个数连续占用若干槽**：长度 L 的数组基址取当前可用槽里最深的一格（-8 × (已用 + L)），a[i] 就是基址加 i 个槽，因此首元素位于最低地址、后续元素依次向 x29 靠近。非数组变量占一格。frame_size = align_up(8 × 槽数, 16)。由于偏移恒为 -8 的倍数，0 可以放心地当作「尚未分配」的哨兵值。序言（117-129）在 frame_size 超过 4095 时改用 mov x10 / sub sp, sp, x10，因为 add/sub 的立即数只有 12 位无符号范围。槽位寻址同理（slot_ref / slot_address）：偏移落在 [-256,255] 就直接用 [x29, #off] 的 ldur/str，超出先用 x10 算出槽地址再访存，而 x10 里的这个大数本身就走 load_immediate。

调用约定（emit_arguments / emit_call，128-169）：实参寄存器 x0..x7，第 9 个起由调用者放栈——sub sp 预留 16 字节对齐的空间、用 x11 逐个搬运、bl 之后 add sp 回收；被调方把 x0..x7 存进各自帧槽，第 9 个起的形参则从「保存后的 fp 之上」ldur 出来再存进自己的槽。返回值固定在 x0：OP_CALL 把 x0 存进结果变量（277-280），void 调用走 OP_PROC（281-283），带值的 return 则是「把表达式加载进 x0，再 b 到出口标号」（284-287），出口标号上挂 OP_EXIT 统一退栈。

参数序列与产码顺序是**相互咬合**的：前端把一个有返回值的调用翻译成 arg a; arg b; dec .Lt; .Lt = f()，返回临时量的 OP_DECLARE 正好插在实参序列和调用之间。更关键的是，实参本身可能需要求值指令（sum(d,4) 里数组名 d 要先 OP_LEA 退化成地址），这些指令天然夹在两条 arg 之间，所以后端**不能**把「遇到非 arg 指令」当作参数序列被打断的信号——早期版本正是这样清空 pending_args，结果 sum(d,4) 只送出一个实参、被调方把地址当成了 n 而越界。现在的规则只有一条：pending_args 由 OP_CALL / OP_PROC 消费并清空，函数开始前也清一次。OP_DECLARE 的另一职责是把局部变量的字面初值写进栈槽：前端遇到 int a = 3; 时只登记符号、不生成 OP_ASSIGN，初值折在 Variable 自己身上，后端必须补出 mov + str。

引用类指令的翻译（emit_body 末尾三条）：OP_LEA 用 load_address 取变量/数组的槽地址或全局符号地址再存进结果变量；OP_PTR_GET 先 load_value 读出「指针值本身」，再 ldr 解引用；OP_PTR_SET 同理但方向相反（str 到 x0 指向的地址，resultVar 恒空，因此不能走 store_value）。数组下标在前端已被拆成 基址 + 索引 × 8 的普通加法，所以后端看到的只是地址临时量，不需要知道元素大小。input 的目标若是单元（a[i] / *p），写入地址取的是该单元里存的指针值，而不是单元自身的槽位。

标号处理（local_label，541-547）：IR 里的 .L 前缀在 Mach-O 下会被汇编器当成外部符号，条件跳转因此拒绝以它为目标，所以输出时剥成 L 开头的局部标号（ELF 保留原样）。

输入输出的降级（306-319、360-369、580-595）：格式串按操作数字面类型选——char 用 %c、字符串用 %s、其余用 %lld；OP_READ 取变量地址后调 scanf（%c 先 str xzr, [x1] 清槽，避免 scanf 只写低字节时高位残留栈垃圾），OP_PRINT 走 emit_vararg_call：Apple ARM64 要求调用者在 sp 处预留 64 字节窗口放可变参数，所以同一个值既留在 x1 又写入 [sp]，Linux 侧只依赖 x1，两个位置都写即可两边正确。格式串本身也走 string_symbol 的去重表。

静态数据（emit_static_data，489-539）：字符串去重后进 __TEXT,__cstring / .rodata，全局变量进 __DATA,__data / .data（未初始化 .zero 8，数组是 .zero 8 × 元素个数，已初始化 .quad 值）；Mach-O 下两者都加 .private_extern（ELF 用 .hidden + .type/@object），因为 adrp 需要真实符号而不是仅本文件可见的临时标号。字符串内容经 escape()（597-615）处理：双引号与反斜杠加转义，不可打印字节写成固定三位八进制，避免和后续数字粘连。

X86 后端（codegen/x86_codegen.cpp）是一套产码逻辑出两种封装：linux-x86 的 ELF 与 macos-x86 的 Mach-O，两者的调用约定都是 cdecl，所以平台差异全落在符号与段的装饰上（构造函数用 is_macho / is_linux 两个布尔决定）；windows-x86 交给 MASM 版后端（见本节末）。

值仍然是 64 位有符号整数，但在 32 位机器上它是一对寄存器，低位在前：左操作数 eax:edx、右操作数 ecx:ebx（11-14）。%ebx 是 callee-saved，序言因此多压一格 pushl %ebx、退栈时弹回（148-159、194-199）；这格是压在 %ebp 建立之后的，所以落在 [ebp-4]，退栈得先 lea -4(%ebp), %esp 再 popl %ebx、popl %ebp，直接 movl %ebp, %esp 会把 %ebp 弹进 %ebx、返回地址弹进 %ebp（MASM 版同理，x86_masm_codegen.cpp:194-199）。字面量在词法层就是 int32，所以常量拆出来是 movl $低字 加 movl $高字，而高字恒为 0 或 -1（67-76）——语言层面不存在需要第二个字的字面量。帧布局与 ARM64 同构（allocate_frame，121-146），只是 RESERVED_SLOTS = 1 让槽位从第 2 格起分配（对应序言里的 pushl %ebp + pushl %ebx），frame_size = align_up(8 × (槽数 + 1), 16)，序言收尾再补一条 andl $-16, %esp：cdecl 只保证调用点 16 字节对齐，printf 一类库函数内部的 SSE 访存才不会踩空。寻址没有 ARM64 那种窗口限制，位移是 32 位有符号数，offset(%ebp) 直接可用（frame_ref，488-490）。

cdecl 没有实参寄存器：形参全部来自栈，emit_arguments（160-170）从 8 + 8i(%ebp) 逐字读出第 i 个实参的两个字再存进帧槽；调用点（emit_call，172-192）按 align_up(8 × 实参个数, 16) 预留空间，把 pending_args 的逆序逐字搬进 (%esp)，call 之后原样回收。返回值仍留在寄存器对里，取回后落进结果变量（319-325）。

四则与比较要按字拼：加法 addl + adcl、减法 subl + sbbl、取负 notl + addl $1 + adcl $0（378-386、254-261）；乘法只保留低 64 位，a0×b0 用一条 mull 拿到完整积，两个交叉项各用一条 imull 取低 32 位累进高位字（388-403）。64/64 除法没有对应指令，后端因此自带一个 sl_divmod 子程序（564-646）：先取绝对值并记下两侧符号，做 64 轮「左移一位、够减则减去除数并把商当前位 or 上 1」的逐位长除，余数走 esi:edi，最后按符号异或决定商与余数是否再取负；除数为 0 时商取 0、余数取被除数，与 ARM64 的 sdiv + msub 行为一致。emit_div / emit_mod 只负责置 needs_divmod 再 call sl_divmod（405-416），子程序按需在末尾发射一次（76-93），它拿 esi:edi 当累加器，但这两个寄存器在 cdecl 里是 callee-saved，所以子程序自己先存后取（GNU 版 565-655，MASM 版 578-668）。比较先 cmpl 高位字，不等就跳到带符号的 setcc，相等再 cmpl 低位字、走不带符号的 setcc（418-436），两条路径都要 movzbl 把结果展成 0/1 再清掉高位字；这种「每处跳转要唯一标号」的地方由 next_local_label（700-704）发 L_cmp_hi1、L_cmp_end1 一类的计数标号，与 IR 自带的 .L 标号分开。条件跳转 OP_JMP_NEQ 则两个字各比一次、任一不等即跳（308-315）。

输入输出：格式串固定 %lld（731-734，Windows 的 %I64d 归 MASM 版后端），char 用 %c、字符串用 %s。emit_vararg_call（453-467）在 %esp 处留 16 字节窗口，[esp] 放格式串地址，[esp+4] 与 [esp+8] 放值的两个字；scanf 只写地址那一格，%c 先把槽里两个字清零，免得 scanf 只写低字节时高位残留栈垃圾（351-369）。

平台装饰集中在三处：C 符号在 Mach-O 带下划线前缀、ELF 不带，函数名、全局量 slg_ 与字符串 sls_ 都走同一个 c_symbol()（706-730）；字符串段写成 __TEXT,__cstring,cstring_literals 或 .rodata，全局量段写成 __DATA,__data 或 .data，ELF 下每个全局额外加 .hidden 与 .type/@object（648-691）；ELF 还给函数补 .type/@function 与 .size，并在文件末尾追加 .note.GNU-stack（76-93、95-119）。Mach-O 依旧要剥掉 IR 标号的 . 前缀（692-698）。静态数据的地址取法是 movl $符号, %dest（559-562），也就是绝对重定位——这正是 linux-x86 链接时必须补 -no-pie 的原因（见下段）。

X64 后端（codegen/x64_codegen.cpp）只覆盖 linux-x64（ELF，System V ABI），一个值就是一个 64 位寄存器或一格 8 字节栈槽，不需要拆字；macos-x64 落在 target_supported 的兜底拒绝列表里，windows-x64 交给 MASM 版后端（见本节末）。

表达式暂存固定用 r10 与 r11（LEFT_REG / RIGHT_REG，11-12），System V 把它们划成 volatile，因此既不必保存恢复，也不会撞坏正在传的实参。返回值走 rax；除法走 rdx:rax——emit_div / emit_mod 把被除数装进 rax、cqto 扩符号、除数放 r11，再 idivq，商取 rax、余数取 rdx（366-381）。加减乘与比较都是单条指令（336-350），逻辑运算同样先把两侧归一化成 0/1（352-364）。

调用约定固定一套（寄存器表 14-16，六个：rdi/rsi/rdx/rcx/r8/r9，第 7 个起放栈上，没有影子空间）。调用点预留 align_up(8 × 栈实参数, 16)，栈实参落在 8k(%rsp)（emit_call，128-153）；被调方从寄存器落栈之后剩下的形参在 16 + 8k(%rbp)（emit_arguments，111-126）。帧是 pushq %rbp / movq %rsp,%rbp / subq $frame,%rsp（103-109），frame_size 已按 16 对齐，所以调用点天然满足对齐要求。

可变参数调用把格式串地址放进第一个实参寄存器，值全走寄存器，并且调用前补一条 xorl %eax,%eax——System V 规定 AL 是传出的 XMM 个数（364-374）。静态数据用 leaq 符号(%rip),%dest 取地址（451-454），全是 PC 相对重定位，所以 linux-x64 不需要像 X86 后端那样关 PIE；.type/@function、.size、.hidden + .type/@object、.rodata 与 .note.GNU-stack 这些 ELF 装饰与 X86 后端一致，整数格式串固定 %lld（523-527）。

两个 MASM 后端（codegen/x86_masm_codegen.cpp 与 codegen/x64_masm_codegen.cpp）只服务 windows-x86 与 windows-x64：微软的 ml / ml64 认 Intel 语法，不认前面四份 GNU 文本用的 AT&T 语法，所以每个 Windows 三元组各配一份产码器。lowering 与对应的 GNU 版逐条同构——同一套字对寄存器、同样的 cdecl 全栈实参与 16 字节对齐的调用点、同样自带 sl_divmod 逐位长除、同样一个值一个 64 位槽——差别全在装饰上：文件头换成 .486 加 .model flat, C（x86 的 generate，81-97），C 命名约定由 MASM 负责补下划线，所以产码器写的是原名（c_symbol，705-708）；外部符号写成 EXTRN printf:PROC / EXTRN scanf:PROC（x64 写成 EXTERN，52-62），段用 .code / .const / .data，函数用 PUBLIC 符号 + 符号 PROC … 符号 ENDP 成对包住（generate_function，x86 99-119、x64 64-83），文件末尾必须补一条 END。访存写成 DWORD PTR [ebp-8] / QWORD PTR [rbp-8] 这种中缀形式，位移由 disp_text 统一成 ±N，槽位、栈实参与解引用分别由 slot_ref / slot_addr / stack_ref / word_at 拼出（x86 489-503、x64 402-416）。静态数据不再用 .asciz / .quad：格式串里的 % 在 MASM 中是运算符，转义也容易出错，所以写成符号 BYTE 37,73,54,52,100,0 的十进制字节列表再加一个 0（emit_static_data 与 byte_list，x86 661-692、754-762），全局量在 x86 是 ALIGN 4 + DWORD 低字, 高字（数组是 2 × 元素个数 DUP (0)），在 x64 是 ALIGN 8 + DQ（x64 481-513），取地址由 leaq 符号(%rip) 换成 mov reg, OFFSET FLAT:sym 与 mov reg, OFFSET sym（x86 572-575、x64 476-479）。整数格式串这边是 msvcrt 认的 %I64d（x86 732-735、x64 543-546）。

MASM 特有的两个坑由命名兜住。IR 标号形如 .L1，开头的点 MASM 不认，因此剥掉点、再冠上函数序号（local_label，x86 694-698、x64 515-519），比较与条件跳转要的唯一计数标号同理由 next_local_label 发成 f序号_L_名序号（x86 700-703）。另一处是源程序里的函数名会直接变成 MASM 标号，而 add、sub、cmp 这些助记符在 MASM 里是保留字、做不了标号，所以除入口 main 要留给 CRT 保持原名之外，函数标号统一加 slf_ 前缀（function_symbol，x86 710-714、x64 521-525）；调用运行库时不能照这个规则走，printf / scanf 按原名引用（emit_vararg_call，x86 453-468、x64 373-381）。全局量与字符串仍是 slg_ / sls_ 前缀的文件内标号，不参与 C 命名约定（global_symbol，x86 716-718、x64 527-530）。

装配与链接（codegen/linker.cpp）是独立一步：linkable_on_host()（54-64）同时比较本机与目标的平台和架构，不一致就只出汇编。非 Windows 目标走 assemble_and_link()（66-98）：cc -o 一把梭，macOS 按目标架构补 -arch i386 / -arch x86_64，linux-x86 补 -no-pie（X86 后端的静态数据是绝对地址引用，发行版默认的 PIE 链接会拒收这类重定位），链接完再补一次 codesign --force --sign - 的 ad-hoc 签名（Apple Silicon 内核拒绝执行未签名代码）。Windows 目标走 link_coff()（100-129）：先按架构挑 ml.exe 或 ml64.exe 把 MASM 文本装配成 .obj，再用 link.exe /SUBSYSTEM:CONSOLE 链上 msvcrt.lib ucrt.lib vcruntime.lib——VS2015 起 CRT 拆成三块，msvcrt.lib 是本机 CRT 启动代码（mainCRTStartup 在这里），ucrt.lib 才是含 printf / scanf 的 C 库，vcruntime.lib 是 C++ 运行库；链接行给 printf / scanf / main 各写两条 /alternatename，MASM 补不补下划线两种约定缺哪种都改用另一种去同一个符号。32 位的 ml.exe 还会把非本地名转成大写，汇编那一步要补 /Cx 保住原样，否则和 CRT 里的 _printf 对不上，ml64 没有 /Cu 这个选项、大小写本来就保留；ml.exe 按扩展名判断源语言，所以 MASM 源文件必须叫 .asm。动手前用 where 探一次这两个工具（tool_available，11-13），探不到就报「windows 目标的装配与链接要用 Visual Studio 自带的 MASM 和链接器，请在开发者命令提示符里运行」并提示改用 -S——装 Visual Studio 或 Build Tools、或者换到已加载 vcvarsall.bat 的 shell 里是唯一的出路，编译器不自己去猜工具链在哪。两份中间产物都不留在这台机器上：.obj 无论链接成败都删（object_path_of 只负责把 .asm 换成 .obj，15-25），临时 .asm 由调用方删（见下段）。路径参数一律走 shell_quote()（27-52），POSIX 侧单引号转义，Windows 侧因为 cmd.exe 不认单引号而改用双引号。产物名在 Windows 下由 with_exe_suffix()（shaolang.cpp:102-109）补上 .exe，否则 CreateProcess 拒载。

中间汇编的位置由驱动决定（temp_assembly_path，shaolang.cpp:44-86）：Windows 用 GetTempPathA，其余平台按 TMPDIR、TMP、TEMP、/var/tmp、/tmp 的顺序试，每个候选都要先过 usable_directory()（32-42）——Windows 查目录属性，POSIX 查 stat + S_ISDIR + access(R_OK|W_OK|X_OK)——因为环境变量给的路径未必是本机可执行文件认得出的那种（MSYS 一类的环境只转 argv，不转环境变量，会把 TMP 原样给成 /tmp），全部不可用就退回当前目录。文件名是 shaolang-进程号-文件序号.s（Windows 下是 .asm，因为 ml.exe 按扩展名挑汇编器），一律不带源文件名，因此编译 a.sl 不可能撞掉 /tmp 里别人的 a；链接一结束，成败都删（218-231），真要看汇编用 -S，那才是留给用户的产物。

实测覆盖面由仓库里的 test/e2e/run.sh 维护：25 组随仓库提交的用例（.sl 配 .expected，脚本自定位、工作目录自己按盘符路径建，不在仓库里留中间产物）加上运行时生成的 300 / 600 个局部变量两组深栈用例，覆盖全局变量与 char 全局、局部字面初值、8 个以内与超过 8 个实参的调用（10 个实参求和 55 正确）、递归（fib(10) = 55）、void 调用、while / if / break / continue、input（int、char、数组元素、解引用单元）、字符串 output、300 与 600 个局部变量两种深度（后者已走 mov x10 的超窗口路径）、加减乘除与取模、负数、123456789 这类需要 movz + movk 拼出来的大常量，以及数组下标读写、取地址与解引用、指针传参、指针相减、通过单元自增。每组都要过「编译→装配→链接→运行→逐字节比对 stdout」，用例目录里另有 .in 喂标准输入、.exit 记期望退出码，两者也参与比对；Windows 上找可执行文件要带 .exe 后缀，脚本按 uname 的 NT 分支决定。

本机跑不了的 GNU 封装由 cross_round()（145-172，调用在 178-181）做交叉装配轮次：同一个用例用 --target= 指定三元组出一份汇编，再交给 clang --target= 装配成目标格式的目标文件，只验证「这份汇编本机的汇编器认不认」，链接与运行仍归各平台的原生作业。轮次覆盖 linux-arm64、linux-x86、linux-x64、macos-x86，每种先拿一小段目标架构的基础指令做探针，工具链不支持就打印跳过、不计失败，所以在没有交叉汇编器的机器上跑整套也只是少跑几轮。两个 Windows 三元组不在这里：它们产的是 MASM 文本，本机的 clang 装不了，交叉装配没有意义，真正的验证由 AppVeyor 用 Visual Studio 的工具链原生完成（见下节）。同一份 ARM64 产码在 --target=linux-arm64 下只有六处形态差异：无 .subsections_via_symbols、符号不带下划线前缀、多了 .type/@function 与 .size、段名换成 .rodata / .data（全局还额外加 .hidden + .type/@object）、页引用由 adrp+add @PAGE/@PAGEOFF 换成 adrp + add :lo12:、局部标号保留 IR 原本的 .L 前缀。

### 3.11 诊断系统（commons/error.{h,cpp}）

- Error 全是静态成员：error_amount / warning_amount 跨文件累计（shaolang.cpp:192-197 打印总计），Error::scanner 由 Lexer 构造时注入以取行列位置。
- 错误文案不写在抛出点，而是四张与枚举**顺序严格对应**的表：typeName / syntaxErrorHints / semanticErrorHints / interOpHints，每张表后都跟一个 static_assert 保证枚举扩展时不会错位（common.h:49-50, 88-89, 143-144, 178-179）。抛出点只写 SE_ERROR(semanticErrorHints[XXX])。
- 只有 Error / Warning 两级，没有 fatal：诊断之后流程照常推进，源文件有错也照样走完解析与优化，进程退出码仍是 0（只有命令行本身不合法才返回 2，见 §4）。ErrorNames 里 [Waring] 拼写有误（error.cpp:15）。
- 位置语义要留意：打印的是扫描器**当前**读到的位置，而不是出错构造的起点，所以一条错误的行列经常落在问题代码之后（这是语法文档 §7.8 的根因）。

---

## 4. CLI 与构建

```
shaolang 源文件 [options]
  -h                帮助            （→ CompilerArguments::showHelp）
  -t                打印词法记号流   （Parser::move() 里逐条打印）
  -b                打印数据流基本块 （Function::do_optimize() 里打印）
  -ir               打印中间代码，不出目标文件（→ CompilerArguments::showIntermediateCode）
  -v                打印可执行文件的落点与警告/错误计数
  -S                只输出目标机汇编 （→ CompilerArguments::emitAssembly，走 Compiler::emit_assembly）
  -o <文件>         配合 -S 时是汇编文件名，否则是可执行文件名
  --target=<目标>   macos-arm64 / linux-arm64 / macos-x86 / linux-x86 / windows-x86 / linux-x64 / windows-x64，缺省本机
```

-S 与 -o 的组合决定产物（shaolang.cpp:127-128 把 generateExecutable 定义成「既没有 -S 也没有 -ir」）：-S 加 -o 写汇编文件，-S 不加 -o 汇编打到标准输出；不给 -S 也不给 -ir 时默认就装配并链接，未指定 -o 时产物名取源文件名去掉后缀、写在当前目录（default_output_path，40-51），Windows 下再由 with_exe_suffix() 补 .exe；链接成功且带 -v 才打印一行 已生成可执行文件 加目标路径，链接失败则保留临时 .s 并回显命令；目标与本机不同却只要求链接时会提示改用 -S（Linker::linkable_on_host，linker.cpp:33-43）。-ir 只打印中间代码，不出任何目标文件。

命令行层的拒绝路径都返回 2：无法识别的选项、-o 后没有文件名、--target= 取值不认识（parse_target 只认上面那些别名，macos-x64 根本没有写法）、解析出的三元组不在支持范围（在本机就是 windows-arm64 或 Intel Mac 上会给出原因）、-o 配多个源文件、-o 的目标文件打不开、临时汇编文件写不进去（shaolang.cpp:79-201）。**源文件本身的语法/语义错误不改变退出码**，仍然是 0，只是不出目标代码并提示 源文件存在错误，没有生成目标代码。

错误门是**按文件**判断的：编译每个文件前记下 Error::get_error_amount()，之后比较差值（shaolang.cpp:143-148），所以前一个文件的错误不会阻止后一个文件出码。

- build.sh 走 mkdir build && cmake ../ && make；CMakeLists.txt 声明 cmake_minimum_required(VERSION 2.8) 而 README 要求 3.6+，并按平台 -DLF / -DCRLF；三个后端的 x86_codegen / x64_codegen / aarch64_codegen 都已在源文件列表里。
- 没有 cmake 时可直接编译（本次文档核对用的就是这条）：
  
  c++ -std=c++11 -Isrc -DMAC_OSX -DLF $(find src -name '*.cpp') -o shaolang
  
  -S 的产物可以手工链接（cc -arch arm64 x.s -o x），也可以让编译器自己走完（shaolang x.sl -o x）。
- CI：.github/workflows/ci.yml 有 build（Linux / macOS 构建）与 e2e（macos-latest、ubuntu-24.04-arm、ubuntu-latest 三台机器上跑 test/e2e/run.sh，含四种 GNU 封装的交叉装配轮次）两个作业。两个 Windows 三元组归 appveyor.yml：Visual Studio 2022 镜像上先用 MSVC 构建编译器，再用 vswhere 找到安装路径、Enter-VsDevShell 按作业架构加载开发者环境（ml.exe / ml64.exe 与 link.exe 只有加载之后才在 PATH 上，加载完立刻 Get-Command 断言一次），最后把 CMake 产出的 shaolang.exe 通过 SHAOLANG 交给 test/e2e/run.sh 原生跑整套——MSYS2 在这里只当 bash 解释器用，一行编译器代码都不是 MinGW 建的。矩阵是 x86 / x64 两种架构 × Debug / Release 两种配置，端到端只在 Release 跑一轮：编译器的字长决定本机三元组，所以 x86 作业验 windows-x86、x64 作业验 windows-x64，Debug 那两轮只证明 MSVC 编得过。
- 语法/语义与设计结论的核对方式：写 .sl 样例喂给上面编出的二进制，比对 IR 文本、诊断信息，以及 -S 产物经汇编链接后的实际运行输出；其中可复跑的那部分已经固化成 test/e2e/run.sh，见 §3.10 末。

---

## 5. 设计中需要知道的取舍

1. **无 AST 换来的代价**：代码路径短（解析即产出 IR），但一切需要「回看结构」的能力都受限——前向引用、类型检查跨语句传播、后续引入新优化 pass 都要改 parser.cpp 本身。
2. **符号表与 IR 生成器相互触发**：add_variable → generate_variable_init → add_inter_instruct。想加一个「只登记不产码」的符号（如优化用的常数）必须依赖 is_literal_value() 短路，容易踩到隐式约定。
3. **裸指针 + 手动所有权**：Token 由 Lexer 逐个替换释放；Variable/Function/IntermediateInstruct 由符号表和指令序列持有，几乎不释放（短生命周期进程可接受）。SymbolTable::zeroVariable 等 5 个静态常量对象被所有函数共享，opt_index 复位逻辑正是为此而写。
4. **优化是 per-function、per-run 一次**：没有跨函数常量传播、没有复制传播/CSE/DCE、没有迭代到不动点后再化简（化简只在分析后跑一遍，因此 x = 7; if (x > 10) 折叠后残留的死块不会被删除——见下条）。
5. **跳转化简不会删除随之死掉的代码**：条件恒成立时只把条件跳转改成 OP_JMP，保留顺序边（const_opt.cpp:512-520），于是被跳过的那块仍被 write_opt_code() 导出（实测 goto .L6 之后还打印 y = 5）。只有「条件恒不成立 → 走顺序」那条分支会 un_link 并摘掉死块。
6. **标号与临时量共用一个计数器**，且指令 display() 对标号行短路（inter_code.cpp:164-167）——打印出的 .L3 既可能是标号也可能是临时变量，只能靠是否独占一行区分。
7. **可重入性差**：Error、CompilerArguments、Variable::tempId、Block::global_block_id 都是静态状态；多文件编译时行列号与错误计数会互相串。
8. **原型与定义的过滤现在两处一致**：SymbolTable::optimize()（symbol_table.cpp:232-240）和 get_defined_functions()（197-207）都跳过 is_declare() 的函数，因此只有原型、没有定义的声明既不进控制流图也不产码；调用它的错误只能在链接阶段暴露（链接器报未定义符号，编译器退出码仍是 0）。
9. **原型后面补定义会静默丢掉形参**：define_function()（symbol_table.cpp:144-162）发现表里已有同名原型时，沿用**原型那个 Function 对象**（连带它的形参 Variable 列表）；可函数体解析前 parameter()（parser.cpp:217-224）已经把定义里的形参**另外注册**成了一个新符号，作用域编号也不同。结果 allocate_frame 布局的是原型的形参、体内读到的是定义的形参，后者 frame_offset 一直是 0，storage_of 落到 STORAGE_NOWHERE → 形参读出来永远是 0。实测 -[int] helper(int x); 之后 -[int] helper(int x) { return x; }，output helper(3); 打印 0；把首行原型删掉就打印 3。语法文档 §4.3 用「先加原型」解决前向引用，这一步在 IR 阶段完全正确，只有走到 -S / -o 才出错。
10. **后端不是纯消费者**：allocate_frame 用 set_frame_offset() 把布局结果写回 Variable 符号本身，同一符号表再跑一次生成会复用旧偏移；这也是「已有偏移就跳过」那条规则的来源。

---

## 6. 改进路线（按投入产出排序）

**低成本修补**
- 表达式语句丢掉结果却照样留下临时量（3 + 4; 化简后仍有 .L1 = 7）：化简阶段可以顺手删掉「结果临时量无人使用」的赋值。void 调用作为语句已经能走通（arg … ; f( 即 OP_PROC）。
- 修 OP_MOD 的 %%、display() 的 default 空指针、[Waring] 拼写。
- 诊断带出「出错构造起点」的行列（Lexer 记 token 起始位置，或 Parser 传参）。
- 原型剩下的那半缺陷：define_function 沿用原型 Function 时，把它的形参列表也换成定义里新注册的那一组，或者干脆拒绝「原型 + 定义」两次注册（§5.9）。这是当前 -S / -o 唯一已知的产码/流程错误；优化器一侧的 is_declare 过滤已经补上。

**结构升级**
- 引入 AST（或先引入「声明/使用分离」的两遍符号解析），解锁前向引用与真正的类型检查（区分 int/char）。数组与指针语法已经落地（单层指针、一维数组、元素恒为一槽），下一步要的是多维数组、数组初始化列表与越界检查。
- 把语义检查从 IRGenerator 里剥出来，成为独立的语义分析阶段，使符号表不再产码。
- 优化改成工作列表 + 独立 DCE/copy propagation，并在化简后重跑一次分析。

**后端**
- 已有：三种架构在七种三元组上都能出码——macos-arm64 / linux-arm64 / macos-x86 / linux-x86 / windows-x86 / linux-x64 / windows-x64。ARM64 用 x0..x7 传参、第 9 个起走栈；X86 把一个 64 位值拆成一对 32 位字，cdecl 全栈实参，除法靠自带的 sl_divmod 逐位长除；X64 一个值就是一格 8 字节槽。同一套 lowering 出两种语法：Linux 与 macOS 是 GNU 文本，两个 Windows 目标是 MASM 文本，Microsoft x64 的四个实参加 32 字节影子空间写在 MASM 版后端里。三者共用 8 字节槽的帧布局（数组按元素连续占槽，ARM64 超出 x29 立即数窗口时走 x10 变址）和 scanf/printf 的输入输出降级；封装侧 Mach-O、ELF、COFF 三种都覆盖，链接走本机 cc（macOS 补 -arch 与 ad-hoc 签名，linux-x86 补 -no-pie），Windows 走 ml / ml64 加 link。实测递归、多实参、char 输入、字符串输出、数组与指针、600 个局部变量都能跑出正确结果，Windows 两种目标与 i386 的运行结论由 CI 的原生工具链给出。剩余的空转字段是 Function::frame_max_depth 与 current_esp_ptr（scope_esp 只 push/pop 恒为 0 的值）。
- 还差的封装只有两个：macos-x64 需要给 X64 后端补一套下划线符号与 Mach-O 段名的出路，windows-arm64 明确不在计划内（target_supported 单独给出理由）。
- 已经固化：test/e2e/run.sh 做「编译 → 汇编 → 链接 → 运行 → 比对 stdout 与退出码」，再补 linux-arm64 / linux-x86 / linux-x64 / macos-x86 四种 GNU 封装的交叉装配轮次（探针不通就整轮跳过）；GitHub Actions 的 build 与 e2e 覆盖构建和三种主机架构的原生运行，AppVeyor 用 MSVC 加 MASM 原生覆盖两个 Windows 目标，MASM 文本本机装不了，因此不进交叉装配轮次。
- 结构上值得做的：寄存器分配（现在每条三地址指令都落一次栈）、字符串/字符的实际宽度（现在一律按 8 字节 64 位整数处理，%c 与 char 的 1 字节语义只在格式串上区分）、跳转表与更短的序言/尾言。
