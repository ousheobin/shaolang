# ShaoLang Compiler Design

Companion docs: [language grammar](grammar_EN.md) · [中文版本](compiler-design.md)

---

## 1. What this compiler is

ShaoLang is a **single-pass, syntax-directed translation compiler**:

- There is no separate AST stage. Parser procedures call IRGenerator while reducing, translating statements and expressions straight into three-address code.
- The default output is a **runnable executable**: the optimised three-address code is turned into **target assembly** — three architectures (ARM64 / X86 / X64), each wrapped as Mach-O, ELF or COFF depending on the platform — and the compiler then drives the host tool chain to assemble and link it. -S stops at the assembly, and only -ir prints the **intermediate code text** (once before and once after optimisation).
- There is exactly one optimisation: function-level constant propagation (plus algebraic simplification and constant-condition jump simplification), built on a hand-rolled control flow graph and iterative data flow framework.
- Multiple source files can be given on one command line, but each is compiled independently; symbol tables are not shared.

### 1.1 Pipeline

```
source file (each file is compiled independently)
  └─> FileScanner      commons/scanner.cpp     128B buffer, line/column, EOF sentinel
      └─> Lexer        lexer.cpp               token stream (printed via Parser::move under -t)
          └─> Parser   parser.cpp              LL(1) recursive descent, inline semantic actions
              ├─> SymbolTable    symbols/*     scope-path variable / function / string tables
              └─> IRGenerator    ir_generator.cpp
                     │            expressions and statements → three-address code
                     ▼
              Function.interCodeCollection   (one linear instruction sequence per function)
                     │
                     ▼  optimize(), once per function
              DataFlowGraph   dataflow/*      block formation, edges, reachability
                     └─> ConstPropagation     optimization/*  lattice iteration + two rewrite passes
                              └─> Function.optCode
                                    │
                    ┌───────────────┴───────────────┐
                    ▼ -ir                             ▼ -S / default
              show_all_code() prints it        five emitters, one per triple  codegen/*
                                               (GNU syntax plus MASM syntax,
                                                Mach-O / ELF / COFF assembly)
                                                   │   └ without -S: Linker runs ml/link or cc + codesign
```

SymbolTable and IRGenerator hold pointers to each other: registering a variable is what emits OP_DECLARE (see §3.4).

Compiler::compile() (compiler.cpp:17-31) branches on the output mode: with -ir it parses, prints the raw IR, runs optimize() and prints the optimised IR; otherwise it returns right after parsing (23-26) and nothing is printed. The assembly comes from a separate entry point, Compiler::emit_assembly() (compiler.cpp:33-55), which runs optimize() (Function::do_optimize() is guarded by is_opt, so a repeated call is a no-op) and then chooses between five emitters by architecture and platform: the two Windows x86 / x64 triples get the MASM-syntax X86MasmCodeGenerator and X64MasmCodeGenerator, the other x86 / x64 triples get the GNU-syntax X86CodeGenerator and X64CodeGenerator, and ARM64 gets AArch64CodeGenerator.

---

## 2. Module map

| Path | Responsibility | Key types |
|---|---|---|
| src/shaolang.cpp | CLI: argument parsing, per-file pipeline construction, diagnostic totals | main |
| src/compiler.{h,cpp} | Wires Lexer/Parser/SymbolTable/IRGenerator and drives them | Compiler |
| src/commons/common.h | Global enums and their message tables: tokens, syntax errors, semantic errors, IR opcodes; forward declarations | LexicalType syntaxError semanticError InterCodeOperator |
| src/commons/scanner.{h,cpp} | Buffered character stream + line/column tracking | FileScanner |
| src/commons/token.{h,cpp} | Token objects carrying a value (ID / int / char / string) | Token IDToken IntegerToken CharToken StringToken |
| src/lexer.{h,cpp} | Token recognition, comment skipping, literal values, lexical error recovery | Lexer |
| src/parser.{h,cpp} | Recursive-descent parsing + error recovery + semantic actions | Parser |
| src/symbols/variable.{h,cpp} | Variable/constant/temporary model: type, initial value, scope path, lvalue-ness, analysis index | Variable |
| src/symbols/function.{h,cpp} | Function symbol: parameters, instruction collection, return point, optimised code | Function |
| src/symbols/symbol_table.{h,cpp} | Scope-path variable table, function table, string constant table, instruction dispatch | SymbolTable |
| src/ir/inter_code.{h,cpp} | Three-address instruction record (5 construction shapes) + printing | IntermediateInstruct |
| src/ir/inter_code_collection.{h,cpp} | Linear instruction sequence + leader marking | InterCodeCollection |
| src/ir_generator.{h,cpp} | Expression/statement lowering, jump patch stacks | IRGenerator |
| src/dataflow/block.{h,cpp} | Basic block: instructions, prev/next edges, in/out value vectors | Block |
| src/dataflow/data_flow_graph.{h,cpp} | Block creation, edge linking, reachability, dead-block unlinking, opt code export | DataFlowGraph |
| src/optimization/const_opt.{h,cpp} | Constant lattice iteration + algebraic + jump simplification | ConstPropagation |
| src/codegen/target.{h,cpp} | Compile target (architecture × platform), --target= parsing, support check | CompileTarget Architecture SystemPlatform |
| src/codegen/aarch64_codegen.{h,cpp} | Optimised IR → ARM64 assembly: frame layout, calling convention, static data, Mach-O and ELF | AArch64CodeGenerator |
| src/codegen/x86_codegen.{h,cpp} | Optimised IR → GNU-syntax i386 assembly: 64-bit values as register pairs, cdecl all-stack arguments, built-in long-division helper; one emitter for ELF and Mach-O | X86CodeGenerator |
| src/codegen/x64_codegen.{h,cpp} | Optimised IR → GNU-syntax x86_64 assembly: System V x64 ABI (six argument registers, no shadow space), ELF | X64CodeGenerator |
| src/codegen/x86_masm_codegen.{h,cpp} | Optimised IR → MASM-syntax i386 assembly for windows-x86 (PE/COFF): the same word-pair lowering as the GNU twin, decorated with .model flat, C plus PROC/ENDP | X86MasmCodeGenerator |
| src/codegen/x64_masm_codegen.{h,cpp} | Optimised IR → MASM-syntax x86_64 assembly for windows-x64 (PE/COFF): Microsoft x64 ABI, four argument registers and a 32-byte shadow space | X64MasmCodeGenerator |
| src/codegen/linker.{h,cpp} | Drives the host tool chain to assemble and link the output: ml/ml64 plus link on Windows, cc elsewhere, ad-hoc signature on macOS | Linker |
| src/commons/type_util.{h,cpp} | Type canonicalisation, compatibility, offset step, type width | TypeUtil |
| src/commons/error.{h,cpp} | Static diagnostic output and counters | Error |
| src/commons/argument.{h,cpp} | Static switches (-t / -b / -S / build an executable) | CompilerArguments |

Dependencies point one way only: commons ← symbols/ir ← ir_generator ← parser ← compiler ← shaolang.cpp; dataflow/optimization are used exclusively by Function::do_optimize(), and codegen consumes only the finished sequence (Function::get_emitted_code(), function.cpp:111-116) — apart from writing frame offsets back onto the variables, which is covered in §5.10. Circular includes are avoided by concentrating the forward declarations in common.h.

---

## 3. Stage design

### 3.1 FileScanner (commons/scanner.cpp)

- A fread buffer of BUFFER_SIZE = 128; move() returns one character, and EOF_CHAR = (char)-1 at end of file (scanner.h:12-15). (char)-1 is chosen over EOF's int form so it cannot collide with a UTF-8 byte.
- Line/column advance happens in the #ifdef LF / #elif CRLF branch on newline (scanner.cpp:64-74); CMakeLists.txt defines LF/CRLF per platform. If neither is defined, line numbers stay at 1.
- A failed fopen prints a message but **does not abort**; move() then returns EOF forever (scanner.cpp:47-51), so a bad path behaves like an empty file.

### 3.2 Lexer (lexer.cpp)

- The main loop next_token(): skip blanks → dispatch on the first character (identifier / " / ' / digit / operator); at EOF it produces and caches an END token.
- Keywords live in a static table inside an anonymous namespace and are matched by linear search (lexer.cpp:20-43); a miss yields ID.
- Two-character operators are handled with one token of lookahead in read_operator()'s switch (lexer.cpp:242-296); when / is followed by / or * the comment branch runs and **returns NULL**.
- Recovery policy: **a lexical error produces no token**. read_number/read_char/read_string/invalid_operator report and return NULL; the main loop continues and rescans (lexer.cpp:78-81). So 1abc, 1.5 or an unterminated string pollute exactly one token position.
- Ownership: current_token is deleted right before replacement, so a Token* is valid only until the next call — the Parser must consume it before its next move().

### 3.3 Parser (parser.cpp)

LL(1) recursive descent: one member function per non-terminal, and the comment above each function records the production it implements (e.g. parser.cpp:77-79).

- **FIRST sets are written as macros** (parser.cpp:14-22): ALL_TYPES, ALL_EXPR_KW, ALL_STATEMENT_KW, … — hand-maintained approximations of FIRST. They are the only dispatch mechanism, which is also where grammar gaps come from: a statement keyword that is absent from ALL_STATEMENT_KW can never reach statement() (K_INPUT/K_OUTPUT were added to it later, at parser.cpp:22).
- **Matching**: check_and_move(type) (parser.cpp:69-75) consumes on match.
- **Recovery**: recovery(in_follow_set, lost, error[, delete_all]) (parser.cpp:45-67). If the current token is in the follow set, treat it as *missing*: report xxx_LOST and consume nothing (equivalent to taking an empty production). Otherwise report xxx_ERROR and, by default, drop one token; the delete_all variant skips ahead to the next ), }, ; or END. This is simplified panic mode — no production-level re-entry.
- **Inline semantic actions**: value_definition returns a Variable* directly; every function in the expression chain returns the "result store" (Variable*); jump targets travel out through IntermediateInstruct*& parameters (while_statement, if_statement).
- Because there is no AST, **use-before-declare is structurally impossible**: the callee must already be in the function table at the call site.

### 3.4 SymbolTable (symbols/symbol_table.cpp)

| Structure | Purpose |
|---|---|
| unordered_map&lt;string, vector&lt;Variable*>*> variableTable | Same-named variables coexist per scope level; literal constants are also stored here and skip the duplicate check (symbol_table.cpp:78-80) |
| vector&lt;int> scope_vector | Current scope path. enter_new_scope() increments current_scope_id and pushes; exit_current_scope() pops, never popping the file-level scope (symbol_table.cpp:34-42) |
| unordered_map&lt;string, Function*> functionTable | **One slot per name**, so same-name-different-parameters is not overloading; the has_declare flag distinguishes prototype / definition / duplicate |
| vector&lt;Variable*> globalVariables | Depth-1 variables, used by constant propagation for initial values and the kill set |
| unordered_map&lt;string, Variable*> constStringTable | String constant pool — written, never read today |
| variable_define_order_list / function_define_order_list | The latter is the single source of "in definition order" for three consumers: IR printing, per-function optimisation, and back-end code emission (get_defined_functions, symbol_table.cpp:197-208, which filters out prototype-only entries); the former is still write-only |

Lookup get_variable() (symbol_table.cpp:106-127) picks the candidate whose scope path is the **longest prefix** of the current path — nearest-visible wins. A miss reports 使用未定义变量 and returns NULL; downstream operations tolerate NULL (they return silently).

**Key coupling**: the tail of add_variable() calls irGenerator->generate_variable_init(variable) (symbol_table.cpp:100-102). Registering a symbol *is* what emits OP_DECLARE, and IRGenerator holds the SymbolTable in return. The two are injected into each other in compiler.cpp:12-14.

Instruction dispatch add_inter_instruct() (symbol_table.cpp:209-215) appends only when current_function is set, otherwise it deletes the instruction. That is the mechanism behind "expression instructions at global scope are silently dropped".

### 3.5 Variable (symbols/variable.cpp)

One class plays five roles, distinguished by flags:

| Constructor | Use | Key flags |
|---|---|---|
| Variable(name, scope, type, init) | user declaration | is_left_value = true |
| Variable(Token*) | literal, name fixed to &lt;&lt;int&gt;&gt; / &lt;&lt;char&gt;&gt; / &lt;&lt;string&gt;&gt; | is_constraint = true, is_left_value = false |
| Variable(int) | integer constant used by the optimiser | same as above |
| Variable(scope, type, is_ptr) / Variable(scope, var) | temporary for an operation result, named .L&lt;tempId++> | is_temp_store = true |
| Variable() | the shared void value | TK_VOID + is_pointer |

Fields fall in three groups: type/value (type, integer_initial_value, char_initial_value, str_initial_value); reference-ness (is_pointer, is_array, array_length, ptr, written by the grammar's int *p / int a[n] / a[i] / *p forms — see grammar doc §2.4); and storage placement (frame_offsize, now both written and read by the back end's allocate_frame, while Function::frame_max_depth, current_esp_ptr and scope_esp remain placeholders).

Two reference concepts deserve names: a **cell** is a Variable whose ptr is set and not itself (is_cell()), meaning "the value stored at *ptr" — it may stand on the left of an assignment; an **address value** is a Variable that is neither basic nor a cell (TypeUtil::is_address_value), i.e. a pointer variable, an array name, or a temporary holding an address. Which of the two an operand is decides value fetching, type compatibility and the increment step.

opt_index is the lattice slot the data flow analysis assigns; check_init() / is_init() decide whether a literal initialiser is folded into the declaration or needs a separate assignment.

The static counter Variable::tempId (variable.cpp:17) drives both temporary names (variable.cpp:57) and instruction labels (inter_code.cpp:113), so the .L&lt;n> namespace is **shared and globally monotonic** — names never collide across functions, but the number tells you nothing about the kind of entity.

### 3.6 Intermediate representation (ir/inter_code.{h,cpp})

Each instruction is a fixed six-slot three-address record plus a label:

```
label | op | resultVar | leftArg | rightArg | jumpTarget | function | block
```

The five constructors map to the five usage shapes: (op, result, left, right) arithmetic/comparison; (op, fun, result) calls (result == NULL means OP_PROC); (op, var) declaration/argument push; (op, target, left, right) jumps; () an empty instruction, i.e. a **label pseudo-op** (inter_code.cpp:41-44).

Jump targets are **pointers, not indices**, so patching is free: lowering allocates a placeholder IntermediateInstruct(), and when the construct is complete the placeholder itself is appended to the sequence (ir_generator.cpp:356-401).

Opcodes (enum common.h:147-161, the strictly order-matched hint table common.h:163-176, printing in inter_code.cpp:162-205):

| Opcode | Printed form | Emitted by |
|---|---|---|
| OP_EMPTY_INST | .L&lt;n> (label line only) | while/if placeholders, function return point |
| OP_DECLARE | dec v | generate_variable_init |
| OP_ENTER / OP_EXIT | entry / exit | generate_function_head/tail |
| OP_ASSIGN | r = l | generate_assign |
| OP_ADD/SUB/MUL/DIV/MOD | r = l op rr | binary ops; inc/dec reuse it (r = r + step) |
| OP_MINUS / OP_NOT | r = -l / r = !l | unary ops |
| OP_GT/GE/LT/LE/EQ/NEQ | r = l cmp rr | comparisons (result is always an int temporary) |
| OP_AND / OP_OR | r = l && rr | logical ops (**no short circuit**) |
| OP_LEA / OP_PTR_GET / OP_PTR_SET | r = &l / r = *l / *l = r | indexing, address-of and dereference (§3.7); OP_PTR_SET always has an empty resultVar |
| OP_JMP | goto L | while back edge, if/else joins, break/continue |
| OP_JMP_FALSE | if c no true jmp to L | while / if conditions |
| OP_JMP_TRUE / OP_JMP_NEQ | — | never emitted by the front end; only handled by is_jmcond() and jump simplification |
| OP_ARG | arg v | argument push |
| OP_PROC | f( | void call |
| OP_CALL | r = f() | call with a result |
| OP_READ | read v | input statement (ir_generator.cpp:440-453) |
| OP_PRINT | print v | output statement (ir_generator.cpp:455-461) |
| OP_RETURN / OP_RETURN_WITH_VAL | goto L / return v then goto L | return statement, targeting the function return point |

Two cosmetic defects worth fixing: OP_MOD prints %% (inter_code.cpp:177), and the default branch dereferences resultVar unconditionally (inter_code.cpp:202) — any unexpected opcode landing there is a null dereference.

### 3.7 Lowering rules (ir_generator.cpp)

**Expressions**: every operation allocates a fresh temporary and one three-address instruction. No reuse, no register-allocation hints.

```
a + b            →  dec .Lt ; .Lt = a + b        (generate_two_value_op → generate_add)
a = b            →  a = b                        (returns the left variable)
a = b = c        →  b = c ; a = b                (assign_extend re-extends on the result)
++a              →  a = a + step                 (generate_inc_first, yields a)
a++              →  dec .Lt ; .Lt = a ; a = a + step   (generate_inc_last, yields .Lt)
!x / -x          →  dec .Lt ; .Lt = !x / -x
f(1, 2)          →  arg 2 ; arg 1 ; dec .Lt ; .Lt = f()   (arguments pushed right to left, ir_generator.cpp:70-72)
g()  (void)      →  arg ... ; g(                 (OP_PROC, yields the shared voidVariable)
input a          →  read a                       (generate_read; the target is the declared variable itself)
&p               →  dec .Lt ; .Lt = &p            (generate_lea → address_of; a literal or a non-lvalue is rejected first as an invalid lvalue)
*p               →  dec .Lt ; .Lt = *p            (generate_deref builds a cell, value_of then emits the OP_PTR_GET)
a[i]             →  dec .Lt ; .Lt = &a ; .Lt2 = i * 8 ; .Lt3 = .Lt + .Lt2 ; dec (*.Lt3)   (generate_index; the cell only describes the location)
a[i] = v         →  *.Lt3 = v                     (generate_assign emits OP_PTR_SET for a cell, resultVar stays empty)
input a[i]       →  read (*p)                     (OP_READ targets the cell; the write address is the pointer value held in it)
output e         →  <evaluation chain of e> ; print .Lt    (generate_print receives the result of all_expr)
```

The increment step comes from TypeUtil::get_offset_step(): 8 for an address value (pointer, array name) and 1 for a cell or a plain value (type_util.cpp). So p++ advances one slot while a[i]++ and x++ change by 1.

**Control flow**: the two parallel stacks head_link / tail_link are pushed/popped only by while; break jumps to tail_link.back() and continue to head_link.back() (ir_generator.cpp:403-419). A break inside an if therefore resolves to the enclosing while for free, and an empty stack is exactly the "not inside a while" diagnostic.

```
while (c) B      →  L_head: dec .t; .t = c; if .t no true jmp L_exit
                        B
                    goto L_head
                    L_exit:
if (c) A else B  →  dec .t; .t = c; if .t no true jmp L_else
                        A ; goto L_exit ; L_else: B ; L_exit:
```

Note that if's L_else placeholder is appended *after* the conditional jump, which is why the label line appears after the jump that targets it (.L6 below y = 5 in the sample output).

**Functions**: OP_ENTER → body → return point (empty label instruction) → OP_EXIT. return only jumps to the return point, so a single unified exit always exists and generate_function_tail materialises it (ir_generator.cpp:477-487).

### 3.8 Control flow graph (dataflow/*)

Block construction is two steps:

1. InterCodeCollection::mark_all_first_inst() finds leaders (inter_code_collection.cpp:15-39): the first, second and last instructions of the sequence, the successor of every (conditional) jump, and every jump **target**. The "first two instructions are always leaders" rule is an implementation extra, which is why entry is always alone in block 0.
2. create_new_blocks() slices the sequence in order and link_blocks() adds edges (data_flow_graph.cpp:41-68): if a block's last instruction is not OP_JMP (note is_jmp() only tests the unconditional form, so conditional jumps still get a fallthrough edge) add the sequential edge; if it is a jump or conditional jump, additionally add the edge to the target's block.

OP_RETURN* targets are the function return point rather than control-flow jumps, so a sequential edge into the exit block always survives and the exit block is always reachable.

Edges are maintained explicitly in both directions: link() de-duplicates then adds prev/next; un_link() removes both directions and recursively prunes blocks that just died via delete_all_next_link() (guarded by is_reachable(), data_flow_graph.cpp:97-114). write_opt_code() walks blocks in index order and exports the instructions of reachable ones, producing the optimised linear code (data_flow_graph.cpp:169-180).

The -b dump (Block::to_string):

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

### 3.9 Constant propagation (optimization/const_opt.cpp)

A textbook **forward data flow analysis over a finite-height lattice**, run once per function right after the graph is built (function.cpp:95-109).

- Lattice: NAC = LONG_MIN (not a constant) < a constant c / UNDEF = LONG_MAX (unknown). The meet is ux() (const_opt.cpp:139-151): any NAC → NAC; UNDEF is the identity; two different constants → NAC.
- Variable numbering (opt_index) covers three groups: globals (initial value taken from the literal folded by check_init(), UNDEF when uninitialised, NAC for non-basic types), parameters (always NAC), and locals/temporaries declared by OP_DECLARE in this function. Before starting, all three groups are reset to opt_index = -1 so a previous function's numbering cannot leak (const_opt.cpp:22-33).
- Boundary conditions: block 0's out vector is the initial value vector; all other blocks start with in = out = UNDEF… (const_opt.cpp:90-96).
- Iteration: round-robin over blocks 1 … n-1; each pass does union_blocks() (bitwise meet over predecessors) then transform_block() (apply fx to each instruction in the block), repeating while any out changes (const_opt.cpp:97-106). There is no worklist, so the cost is O(passes × blocks).
- The transfer function fx (const_opt.cpp:176-304): expression instructions are evaluated from operand lattice values (division/modulo by zero → NAC); OP_READ flushes its target to NAC (external input is unknown to the analysis); OP_PRINT changes no value; OP_PTR_SET and OP_ARG of a non-basic operand flush the **entire** vector to NAC; OP_LEA/OP_PTR_GET set only the result to NAC; OP_PROC/OP_CALL kill all globals plus the call result.
- Simplification 1 — algebra_simplify() (const_opt.cpp:306-456):
  - result known constant → the whole instruction becomes storePoint = constant;
  - identity/zero rules: x+0→x, 0-x→-x, x-0→x, x*0→0, x*1→x, 0/x→0, x/1→x, x%0→0, x%1→0, 0&&x→0, nonzero&&x→x!=0, nonzero||x→1, 0||x→x!=0;
  - otherwise one known operand is replaced by a constant object (partial folding);
  - OP_ARG / OP_RETURN_WITH_VAL operands are replaced by constants when known.
  New constants are registered through symbolTable->add_variable() — being literals, generate_variable_init returns immediately and emits no instruction (symbol_table.cpp:56-61, ir_generator.cpp:465-467).
- Simplification 2 — condition_jump_simplify() (const_opt.cpp:472-531):
  - constant condition → decide whether the jump is taken;
  - taken and target is the sequential successor → delete the instruction (natural fallthrough); target elsewhere → degrade to OP_JMP;
  - not taken → delete the instruction, add the sequential edge, and un_link the jump edge if the target is not the successor (the code comments call out this add-then-remove ordering dependency explicitly).

Measured (-[int] main(){ int x = 1 + 2 * 3; int y = x * 1 + 0; if (y > 10) y = 5; else y = 6; return y; }):

```
before                                 after
dec .L1                                dec .L1
.L1 = 2 * 3                            .L1 = 6
dec .L2                                dec .L2
.L2 = 1 + .L1                          .L2 = 7
dec x                                  dec x
x = .L2                                x = 7
.L3 = x * 1                     →      .L3 = 7
.L4 = .L3 + 0                          .L4 = 7
...                                    ...
.L5 = y > 10                           .L5 = 0
if .L5 no true jmp to .L6              goto .L6
```

### 3.10 Back end: ARM64, X86 and X64 assembly plus host linking (codegen/*)

-S turns the optimised three-address code into target assembly, and --target= picks the target (codegen/target.cpp: host_target first reads the MAC_OSX / LINUX / WINDOWS macros, then falls back on the compiler's own __APPLE__ / __linux__ / _WIN32 plus the architecture macros, so a plain command-line build still recognises the host; parse_target accepts macos-arm64 / darwin-arm64 / apple-arm64 / linux-arm64 / aarch64-linux / macos-x86 / darwin-x86 / osx-x86 / linux-x86 / i386-linux / x86-linux / windows-x86 / win-x86 / i386-windows / linux-x64 / x86_64-linux / amd64-linux / windows-x64 / win-x64 / amd64-windows; target_supported admits those seven triples, spells out its own reason for windows-arm64, and macos-x64 falls into the catch-all refusal). Compiler::emit_assembly() (compiler.cpp:33-55) first branches on architecture and then on platform: the two Windows x86 / x64 triples get the MASM-syntax X86MasmCodeGenerator and X64MasmCodeGenerator - ml and ml64 only read Intel syntax - the remaining x86 / x64 triples get the GNU-syntax X86CodeGenerator and X64CodeGenerator, and ARM64 gets AArch64CodeGenerator. All five read the same optimised IR and differ only in how it lands. Without -S, and without -ir, the compiler goes one step further and hands that assembly to the host tool chain, so the default job already produces a directly runnable binary (end of this section).

ARM64 comes first below — it is the oldest and the most literal of the three; the X86 and X64 deltas follow as two groups of paragraphs right after the ARM64 ones.

Register layout is **hard-coded** (aarch64_codegen.cpp:11-24): x0 holds the left operand, x1 the right, x2 the result, x3 the quotient of a modulo, and x0..x7 double as the argument registers (VALUE_REGISTERS); x10 is used only for address computation and x11 only for shuffling stack arguments, deliberately out of x0..x7. There is no register allocation pass: every variable with storage stays in its frame slot or static data, and the registers are nothing but a per-instruction conveyor belt.

Top level (aarch64_codegen.cpp:51-62): emit .text (plus .subsections_via_symbols on Mach-O, telling the assembler each symbol may start its own data section) → one function at a time **in definition order** → static data at the end. The function list comes from SymbolTable::get_defined_functions() (symbol_table.cpp:197-207), which drops prototype-only declarations, so a declaration never produces code — the optimiser applies no such filter, see §5.8.

Per function (generate_function, aarch64_codegen.cpp:64-88): get_emitted_code() (optimised sequence when available, function.cpp:111-116) → allocate_frame → symbol header (Mach-O prefixes an underscore; ELF additionally needs .type/@function and .size) → prologue → parameters spilled from registers → instruction-by-instruction translation → if the sequence never reached OP_EXIT, an epilogue is appended so a function cannot fall through into the next one after its exit block was optimised away.

The storage model treats **every value as a 64-bit signed integer**, and every variable that has storage gets one 8-byte slot. storage_of() (371-388) classifies each operand:

| StorageKind | Condition | Reading it |
|---|---|---|
| STORAGE_IMMEDIATE | literal | mov dest, #n; outside one mov encoding it is built with movz plus movk instructions (load_immediate, 398-413) |
| STORAGE_STRING | C_STRING literal | the value *is* the address of its static symbol (adrp+add) |
| STORAGE_FRAME | frame_offset ≠ 0, i.e. locals and temporaries | ldur dest, [x29, #negative offset] |
| STORAGE_STATIC | scope path of length 1, i.e. file-level global | take the symbol address, then ldr |
| STORAGE_NOWHERE | void / null / anything the above excluded | mov dest, xzr |

That last row is a **silent fallback**: a variable that is neither slotted in the frame nor a global is read as 0 with no diagnostic — the measured defect in §5.9 lands exactly here.

allocate_frame hands out negative offsets to parameters first, in declaration order (their DECLARE instructions precede the body, so they must be counted separately), then scans OP_DECLARE for locals and temporaries; void, literals, strings, file-level globals and **variables that already carry an offset** are skipped. An **array takes one slot per element**: a length-L array gets the deepest still-free slot as its base (-8 × (used + L)), so a[i] is base plus i slots and the first element sits at the lowest address. Scalars take a single slot. frame_size = align_up(8 × slots, 16), and because every offset is a negative multiple of 8, 0 stays a safe "not allocated yet" sentinel. When frame_size exceeds 4095 the prologue switches to mov x10 / sub sp, sp, x10, since the add/sub immediate is a 12-bit unsigned field. Slot addressing does the same: offsets inside [-256,255] use [x29, #off] directly with ldur/str, anything deeper first materialises the slot address in x10, and that large number itself goes through load_immediate.

Calling convention (emit_arguments / emit_call, 128-169): arguments in x0..x7; from the 9th onward the caller places them on the stack — sub sp by a 16-byte-aligned block, move each through x11, add sp back after the bl. The callee stores x0..x7 into its own frame slots, and spills the stack-passed parameters out of the fixed positions above the saved fp into its own slots. The result is always in x0: OP_CALL stores it into its result variable (277-280), a void call goes through OP_PROC (281-283), and a valued return is compiled as "load the expression into x0, then b to the exit label" (284-287), where OP_EXIT performs the single unwinding sequence.

The argument sequence and the instruction stream are **interlocked**: the front end lowers a valued call to arg a; arg b; dec .Lt; .Lt = f(), so the return temporary's OP_DECLARE sits exactly between the arguments and the call. More importantly an argument may itself need evaluation instructions — in sum(d,4) the array name d must first go through OP_LEA to decay into an address — and those naturally land between two arg instructions, so the back end **must not** read "some other instruction appeared" as evidence that the argument list was interrupted: an earlier version flushed pending_args that way and sum(d,4) passed only one argument, letting the callee treat an address as n and walk off the stack. The only rule now is that pending_args is consumed and cleared by OP_CALL / OP_PROC, and cleared once before each function. OP_DECLARE still has its second job: for int a = 3; the front end only registers the symbol and folds the literal into the Variable, emitting no OP_ASSIGN at all, so the back end must produce the mov + str that initialises the slot.

Labels (local_label, 541-547): on Mach-O a label starting with a dot is treated as an external symbol, and conditional branches refuse it as a target, so the IR's .L prefix is stripped to L on output (ELF keeps it as is).

I/O lowering (306-319, 360-369, 580-595): the format string follows the operand's literal type — %c for char, %s for strings, %lld otherwise. OP_READ takes the variable's address and calls scanf (for %c it first stores xzr into the slot, because scanf writes only the low byte and the upper bits would keep stack garbage). OP_PRINT goes through emit_vararg_call: Apple ARM64 requires the caller to open a 64-byte window at sp for variadic arguments, so the value is left in x1 *and* stored at [sp] — Linux reads only x1, and writing both positions is correct on either target. Format strings themselves go through the same deduplicating string_symbol table.

Static data (emit_static_data, 489-539): deduplicated strings go to __TEXT,__cstring / .rodata, globals to __DATA,__data / .data (uninitialised → .zero 8, an array → .zero 8 × its element count, initialised → .quad value). On Mach-O both get .private_extern (ELF uses .hidden plus .type/@object), because adrp needs a real symbol rather than a file-local temporary label. String bytes pass through escape() (597-615): quotes and backslashes are escaped, non-printable bytes become fixed three-digit octal so they cannot merge with a following digit.

The X86 back end (codegen/x86_codegen.cpp) is one emitter with two wrappers: ELF for linux-x86 and Mach-O for macos-x86. Both use cdecl, so every platform difference sits in symbol and section decoration, decided by the two booleans set in the constructor; windows-x86 belongs to the MASM emitter (see the MASM paragraphs below).

A value is still a 64-bit signed integer, but on a 32-bit machine it becomes a pair of registers, low word first: left operand eax:edx, right operand ecx:ebx (11-14). %ebx is callee-saved, so the prologue pushes one extra pushl %ebx and the epilogue pops it back (149-159, 195-200); that push happens after %ebp is established, so the saved word sits at [ebp-4] and the epilogue has to lea -4(%ebp), %esp before popl %ebx / popl %ebp — a plain movl %ebp, %esp would pop %ebp into %ebx and the return address into %ebp (the MASM emitter has the same shape, x86_masm_codegen.cpp:195-200). Literals are int32 by the time the lexer is done with them, so splitting a constant is movl $low plus movl $high where the high word is always 0 or -1 (68-75) — no literal in this language ever needs the second word. Frame layout mirrors ARM64 (allocate_frame, 122-147) except that RESERVED_SLOTS = 1 makes slot numbering start at 2 (accounting for pushl %ebp + pushl %ebx) and frame_size = align_up(8 × (slots + 1), 16); the prologue closes with andl $-16, %esp, because cdecl only promises 16-byte alignment at the call site and library calls such as printf touch SSE state that assumes it. There is no addressing-window problem like ARM64 has: displacements are 32-bit signed, so offset(%ebp) is always usable (frame_ref, 489-491).

cdecl has no argument registers: every parameter arrives on the stack, and emit_arguments (161-171) reads the two words of argument i out of 8 + 8i(%ebp) and stores them into its frame slot. The call site (emit_call, 173-193) reserves align_up(8 × number of arguments, 16), copies pending_args in reverse, word by word, into (%esp), and restores %esp right after the call. The result still comes back in the register pair and is stored into the result variable (320-326).

Arithmetic and comparison are assembled word by word: add is addl + adcl, subtract is subl + sbbl, negation is notl + addl $1 + adcl $0 (379-387, 255-262); multiplication keeps only the low 64 bits — one mull gives the full a0×b0 product and two imull instructions contribute the low 32 bits of each cross term into the high word (389-404). There is no instruction for 64/64 division, so the back end ships its own sl_divmod subroutine (565-647): take absolute values while remembering both signs, run 64 rounds of "shift left one bit, subtract the divisor if it fits and or 1 into the current quotient bit", keep the remainder in esi:edi, then negate quotient and remainder according to the xor of those signs; a zero divisor yields quotient 0 and remainder equal to the dividend, matching what the ARM64 back end gets from sdiv + msub. emit_div / emit_mod only set needs_divmod and call sl_divmod (406-417), and the helper is emitted at the end only if something asked for it (77-94); it uses esi/edi as accumulators, but those are callee-saved under cdecl, so the subroutine saves and restores them itself (GNU 565-655, MASM 578-668). Comparison first cmpl's the high words and, if they differ, jumps to a signed setcc; otherwise it cmpl's the low words and uses the unsigned setcc (419-437). Both paths movzbl the result into a full 0/1 and clear the high word. Those per-site jumps need unique labels, so next_local_label (707-711) issues counted labels such as L_cmp_hi1 / L_cmp_end1, separate from the .L labels the IR carries. Conditional jumps take the cheaper route: OP_JMP_NEQ compares each word and jumps if either differs (309-316).

I/O: the integer format is %lld (731-734 — %I64d is the MASM emitter's business), %c for char and %s for strings. emit_vararg_call (453-467) opens a 16-byte window at %esp with the format string address at [esp] and the value's two words at [esp+4] and [esp+8]; scanf gets only the address word, and %c zeroes both words of the slot first so a partially written byte cannot leave stale garbage in the upper half (351-369).

Platform decoration lives in three places. C symbols get an underscore prefix on Mach-O but not on ELF, and function names, slg_ globals and sls_ strings all go through the same c_symbol() (706-730). The string section is __TEXT,__cstring,cstring_literals or .rodata, the globals section __DATA,__data or .data, with .hidden plus .type/@object added per global on ELF (648-691). ELF also gets .type/@function and .size around every function and a .note.GNU-stack at the end of the file (76-93, 95-119). Mach-O still needs the dot stripped from IR labels (692-698). Static addresses are taken as movl $symbol, %dest (559-562), i.e. absolute relocations — which is exactly why linking linux-x86 needs -no-pie (see the linking paragraph below).

The X64 back end (codegen/x64_codegen.cpp) covers linux-x64 only (ELF, System V ABI). One value is one 64-bit register or one 8-byte slot, so nothing is split; macos-x64 sits in the catch-all refusal list of target_supported and windows-x64 belongs to the MASM emitter.

Expression temporaries are fixed to r10 and r11 (LEFT_REG / RIGHT_REG, 11-12), volatile under System V, so they never need saving and never collide with arguments in flight. The result comes back in rax; division goes through rdx:rax — emit_div / emit_mod load the dividend into rax, extend its sign with cqto, put the divisor in r11 and idivq, then read the quotient from rax and the remainder from rdx (348-363). Add, subtract, multiply and compare are single instructions (318-332), and the logical operators normalise both operands to 0/1 first (334-346).

The calling convention is now a single fixed set: rdi/rsi/rdx/rcx/r8/r9 take the first six arguments (register table 14-16) and the seventh onward sit on the stack, with no shadow space. The call site reserves align_up(8 × stack arguments, 16) and writes them at 8k(%rsp) (emit_call, 128-153), while the callee finds the parameters that did not fit in registers at 16 + 8k(%rbp) (emit_arguments, 111-126). The frame is pushq %rbp / movq %rsp,%rbp / subq $frame,%rsp (103-109), and because frame_size is already a multiple of 16 the call sites are aligned by construction.

A variadic call puts the format string address in the first argument register, passes the values in registers, and prefixing the call is xorl %eax,%eax — System V defines AL as the number of XMM registers passed (364-374). Static addresses use leaq symbol(%rip),%dest (451-454), i.e. PC-relative relocations only, so linux-x64 does not need the PIE switch the X86 back end requires. The .type/@function, .size, .hidden + .type/@object, .rodata and .note.GNU-stack decorations match the X86 back end, and the integer format is %lld (523-527).

The two MASM emitters (codegen/x86_masm_codegen.cpp and codegen/x64_masm_codegen.cpp) serve windows-x86 and windows-x64 alone: ml and ml64 read Intel syntax, not the AT&T syntax the other four emitters produce, so each Windows triple gets its own file. The lowering is line-for-line the same as its GNU twin — the same register pair, the same cdecl all-stack arguments and 16-byte-aligned call sites, the same built-in sl_divmod long division, the same one-value-per-8-byte-slot frame — and everything that differs is decoration. The header becomes .486 plus .model flat, C (x86 generate, 81-97); because the C language type makes MASM add the underscore itself, the emitter writes bare names (c_symbol, 705-708). Runtime symbols are declared EXTRN printf:PROC / EXTRN scanf:PROC (EXTERN in x64, 52-62), sections are .code / .const / .data, each function is wrapped in PUBLIC name plus name PROC … name ENDP (generate_function, x86 99-119, x64 64-83), and the file has to close with END. Memory operands take the bracketed MASM form, DWORD PTR [ebp-8] and QWORD PTR [rbp-8], with the displacement normalised to ±N by disp_text and the four shapes built by slot_ref / slot_addr / stack_ref / word_at (x86 489-503, x64 402-416). Static data no longer uses .asciz or .quad: % is an operator inside MASM and escaping is easy to get wrong, so a format string becomes an explicit decimal byte list terminated by zero, name BYTE 37,73,54,52,100,0 (emit_static_data and byte_list, x86 661-692 and 754-762); globals are ALIGN 4 with DWORD low, high — or count DUP (0) where the count is twice the element count — on x86, and ALIGN 8 with DQ on x64 (481-513). Addresses come from mov reg, OFFSET FLAT:sym and mov reg, OFFSET sym instead of leaq symbol(%rip) (x86 572-575, x64 476-479), and the integer format is the one msvcrt understands, %I64d (x86 732-735, x64 543-546).

Two MASM-specific traps are handled by naming. IR labels look like .L1 and MASM refuses a leading dot, so the dot is stripped and the function's ordinal is prefixed (local_label, x86 694-698, x64 515-519); the counted labels that comparisons and conditional jumps need are issued as f-ordinal_L_name-ordinal by next_local_label (x86 700-703). The other trap is that a source-level function name becomes a MASM label directly, and mnemonics such as add, sub and cmp are reserved words that cannot be labels — so every defined function except the CRT entry main gets an slf_ prefix (function_symbol, x86 710-714, x64 521-525), while runtime library calls must not follow that rule and reference printf and scanf by their real names (emit_vararg_call, x86 453-468, x64 373-381). Globals and strings keep their slg_ / sls_ prefixes as file-local labels that take no part in C naming (global_symbol, x86 716-718, x64 527-530).

Assembling and linking are a separate step (codegen/linker.cpp). linkable_on_host() (54-64) compares both platform and architecture with the host and refuses to link when either differs. Non-Windows targets go through assemble_and_link() (66-98): cc -o does both jobs, macOS adds -arch i386 or -arch x86_64 for the target word size, linux-x86 adds -no-pie because the X86 back end's static data uses absolute relocations that a distribution's default PIE link refuses, and the result gets an ad-hoc codesign --force --sign - (the Apple Silicon kernel refuses to run unsigned code). Windows targets go through link_coff() (100-142): ml.exe or ml64.exe assembles the MASM text into a .obj, then link.exe /SUBSYSTEM:CONSOLE links it against msvcrt.lib ucrt.lib vcruntime.lib — since VS2015 the CRT is split in three, msvcrt.lib holds the native CRT startup code (mainCRTStartup lives there), ucrt.lib is the C library, and vcruntime.lib is the C++ runtime; the link line also carries legacy_stdio_definitions.lib, because since VS2015 the printf and scanf families are inline definitions in stdio.h and the UCRT import libraries no longer name them, so an object that declares them locally without the header - which is what the MASM output does - needs that compatibility library. The 32-bit ml.exe uppercases non-local names, so its assembly step passes /Cx to preserve their case, otherwise an EXTRN printf would miss the CRT's _printf; ml64 has no /Cu option and keeps the case as written; ml.exe picks the assembler dialect from the source extension, so the MASM file has to be named .asm. Both tools are probed with where first (tool_available, 11-13); if either is missing the driver reports that the Windows triples need the MASM assembler and linker that ship with Visual Studio, suggests a Developer Command Prompt - or -S for assembly only - rather than guessing where a tool chain might be. Neither intermediate survives: the .obj is removed whether the link worked or not (object_path_of just swaps the extension, 15-25) and the temporary .asm is removed by the caller (next paragraph). Paths go through shell_quote() (27-52): single quotes on POSIX, double quotes on Windows because cmd.exe does not treat a single quote as quoting. The output name gets a .exe suffix on Windows through with_exe_suffix() (shaolang.cpp:102-109), otherwise CreateProcess refuses to load it.

Where the intermediate assembly lives is the driver's decision (temp_assembly_path, shaolang.cpp:44-86): GetTempPathA on Windows, otherwise TMPDIR, TMP, TEMP, /var/tmp and /tmp in order, with every candidate first checked by usable_directory() (32-42) - directory attributes on Windows, stat + S_ISDIR + access(R_OK|W_OK|X_OK) on POSIX - because a path an environment variable supplies may not be one the native executable can use (an MSYS-style environment converts argv but not the environment, so it hands over TMP as /tmp unchanged); when nothing is usable the file goes to the current directory. The name is shaolang-process-id-file-index.s (.asm on Windows, because ml.exe picks its assembler from the extension) and never mentions the source file, so compiling a.sl cannot overwrite someone else's a in /tmp. As soon as the link attempt is over the file is removed, whether it succeeded or not (218-231): the tool chain's diagnostics are already captured in the failure string, and -S is the option that produces assembly the user actually wants to keep.

Measured coverage is maintained by test/e2e in the repository, with run.sh on macOS and Linux and run.bat on Windows: 25 committed cases (a .sl plus its .expected, both scripts locate themselves and keep every artefact in the system temporary directory, so nothing is left inside the repo) plus two deep-frame pairs generated at run time with 300 and 600 locals. Together they cover globals and char globals, local literal initialisers, calls with up to 8 arguments and beyond them (a 10-argument sum returns 55), recursion (fib(10) = 55), void calls, while / if / break / continue, input for int, char, an array element and a dereferenced cell, string output, add/sub/mul/div/mod and negatives, constants such as 123456789 that need a movz + movk sequence, and array indexing, address-of and dereference, arrays passed as pointers, pointer subtraction and incrementing through a cell. Every case must pass compile → assemble → link → run → byte-exact stdout, with .in files feeding standard input and .exit files pinning the expected exit code. The two runners cannot be merged into one. A Windows CRT writes its newlines as carriage return plus line feed, so .gitattributes checks the .expected files out that way everywhere, run.bat compares them byte for byte with fc /b, and run.sh deletes the carriage return before it diffs; and on the AppVeyor image the name timeout resolves to the timeout.exe in the Windows system directory, which has nothing in common with the GNU tool of the same name and would have killed every command before the compiler ever started.

GNU formats the host cannot run are covered by cross-assembly rounds, cross_round() (145-172, called at 178-181): the same case is emitted with --target= for one triple and then assembled by clang --target= into an object file, which only asks "does a native assembler accept this assembly" — linking and running stay with each platform's own job. The rounds cover linux-arm64, linux-x86, linux-x64 and macos-x86, and each starts by probing the tool chain with a couple of baseline instructions for that architecture; if the probe fails the round prints a skip and counts no failure, so running the suite on a machine without cross assemblers just leaves rounds out. The two Windows triples are not among them: they carry MASM text, which no local clang can assemble, so cross-assembly would prove nothing, and their real verdict comes from AppVeyor running them natively with the Visual Studio tool chain (next section). The same ARM64 code under --target=linux-arm64 differs in six places only: no .subsections_via_symbols, no underscore prefix, extra .type/@function and .size, section names .rodata / .data (plus .hidden and .type/@object for globals), page references written as adrp + add :lo12: instead of adrp + add @PAGE/@PAGEOFF, and local labels keeping the IR's original .L prefix.

### 3.11 Diagnostics (commons/error.{h,cpp})

- Error is entirely static: error_amount / warning_amount accumulate across files (the total is printed in shaolang.cpp:192-197), and Error::scanner is injected by the Lexer constructor so messages can carry line/column.
- Message text never lives at the throw site. Four tables — typeName / syntaxErrorHints / semanticErrorHints / interOpHints — are kept strictly index-aligned with their enums, each guarded by a static_assert so adding an enum entry cannot silently shift the strings (common.h:49-50, 88-89, 143-144, 178-179). Throw sites just write SE_ERROR(semanticErrorHints[XXX]).
- All messages are Chinese-only; the grammar doc's §6 table carries English glosses for the ones a reader is most likely to hit.
- There are only Error and Warning, no fatal: the pipeline continues after diagnostics, a file with errors still finishes parsing and optimisation, and the exit code stays 0 (only an invalid command line returns 2 — see §4). ErrorNames misspells [Waring] (error.cpp:15).
- Position semantics need care: what is printed is where the scanner **currently is**, not where the offending construct started, which is why diagnostics often point past the real error (root cause of grammar doc §7.8).

---

## 4. CLI and build

```
shaolang <source file> [options]
  -h                usage              (→ CompilerArguments::showHelp)
  -t                print token stream (printed inside Parser::move())
  -b                print basic blocks (printed inside Function::do_optimize())
  -ir               print intermediate code, build nothing (→ CompilerArguments::showIntermediateCode)
  -v                print where the executable landed plus warning/error totals
  -S                emit target assembly only (→ CompilerArguments::emitAssembly, Compiler::emit_assembly)
  -o <file>         with -S: the assembly file; otherwise the executable's name
  --target=<name>   macos-arm64 / linux-arm64 / macos-x86 / linux-x86 / windows-x86 / linux-x64 / windows-x64, host by default
```

What the switches combine into (shaolang.cpp:127-128 defines generateExecutable as "neither -S nor -ir"): -S with -o writes the assembly to that file, -S alone sends it to stdout, and with neither switch the compiler assembles and links by default — without -o the binary is named after the source file minus its extension and written to the current directory (default_output_path, 40-51), with .exe appended on Windows by with_exe_suffix(). The line 已生成可执行文件 [executable written to] plus the output path appears only with -v, and the temporary .s is deleted once the link attempt ends, whether it succeeded or failed. Asking the compiler to link for a target that differs from the host is refused with a suggestion to use -S instead (Linker::linkable_on_host, linker.cpp:54-64). -ir prints intermediate code and writes no target file at all.

Every command-line rejection returns 2: an unrecognised option, -o without a filename, an unknown --target= value (parse_target knows only the aliases listed above, and macos-x64 has no spelling at all), a parsed triple outside the supported set (that reason surfaces when the host itself is windows-arm64 or an Intel Mac), -o combined with several source files, an output file that cannot be opened, or a temporary assembly file that cannot be written (shaolang.cpp:79-201). **Errors in the source itself do not change the exit code** — it stays 0; such a file simply gets no target code and prints 源文件存在错误，没有生成目标代码 [the source file has errors, no target code was generated].

The error gate is **per file**: the driver notes Error::get_error_amount() before each file and compares afterwards (shaolang.cpp:143-148), so one file's errors do not suppress another file's output.

- build.sh runs mkdir build && cmake ../ && make; CMakeLists.txt declares cmake_minimum_required(VERSION 2.8) while the README asks for 3.6+, defines -DLF / -DCRLF per platform, and already lists all three back-end sources (x86_codegen / x64_codegen / aarch64_codegen).
- Without cmake, a direct compile works — this is what was used to verify everything in these docs:
  
  c++ -std=c++11 -Isrc -DMAC_OSX -DLF $(find src -name '*.cpp') -o shaolang
  
  The emitted assembly can be linked by hand (cc -arch arm64 x.s -o x) or by the compiler itself (shaolang x.sl -o x).
- CI: .github/workflows/ci.yml runs build (Linux and macOS) and e2e (macos-latest, ubuntu-24.04-arm and ubuntu-latest running test/e2e/run.sh, including the four GNU cross-assembly rounds). The two Windows triples live in appveyor.yml: on the Visual Studio 2022 image MSVC builds the compiler, then vswhere locates the installation and Enter-VsDevShell loads the developer environment for the job's architecture - ml.exe / ml64.exe and link.exe are only on PATH after that, so the script asserts each with Get-Command straight away - and loading that environment and running the suite is the same step, which hands the CMake-built shaolang.exe to test/e2e/run.bat through SHAOLANG so the whole suite runs natively. Not one line of the compiler is built with MinGW. The matrix is platform x86 / x64 crossed with configuration Debug / Release, and the suite runs once, in Release: the compiler's word size picks the host triple, so the x86 job covers windows-x86 and the x64 job windows-x64, while the Debug jobs only prove that MSVC can build the compiler.
- Grammar, semantics and design claims were checked by feeding .sl samples to that binary and reading the IR, the diagnostics, and the actual output of the assembled and linked program; the reproducible part of that is now the two runners in test/e2e, described in §3.10.

---

## 5. Trade-offs a maintainer should know

1. **Cost of having no AST**: short code path (parse == emit), but anything requiring structural look-back is limited — forward references, cross-statement type propagation, and any new pass all mean editing parser.cpp itself.
2. **Symbol table and IR generator trigger each other**: add_variable → generate_variable_init → add_inter_instruct. Adding a "register only, emit nothing" symbol (as the optimiser does with constants) relies on the is_literal_value() short circuit — an implicit contract that is easy to break.
3. **Raw pointers and manual ownership**: tokens are freed on replacement; Variable/Function/IntermediateInstruct are owned by the symbol table and instruction lists and essentially never freed (acceptable for a short-lived process). The five static constant objects (SymbolTable::zeroVariable, …) are shared by all functions, which is exactly why the opt_index reset logic exists.
4. **Optimisation is per-function and single-run**: no interprocedural propagation, no copy propagation/CSE/DCE, and no re-analysis after simplification (simplification runs once, after analysis).
5. **Jump simplification does not delete newly dead code**: when the condition is provably *taken*, only the opcode is rewritten to OP_JMP and the sequential edge stays (const_opt.cpp:512-520), so the bypassed block is still exported by write_opt_code() — the measured output prints y = 5 after goto .L6. Only the "never taken" branch calls un_link and drops the dead block.
6. **Labels and temporaries share one counter**, and display() short-circuits label-only instructions (inter_code.cpp:164-167) — a printed .L3 may be either; only whether it occupies its own line tells you which.
7. **Little reentrancy**: Error, CompilerArguments, Variable::tempId and Block::global_block_id are static, so positions and counters bleed across files in a multi-file run.
8. **Prototypes and definitions are filtered the same way now**: SymbolTable::optimize() (symbol_table.cpp:232-240) and get_defined_functions() (197-207) both skip is_declare() functions, so a declaration without a body neither enters the data flow graph nor gets code generated. Calling such a function only surfaces at link time — the linker reports the undefined symbol while the compiler still exits 0.
9. **Adding the definition after a prototype silently loses the parameters**: when define_function() (symbol_table.cpp:144-162) finds an existing prototype under the same name, it keeps **the prototype's Function object** (and with it the prototype's parameter Variable list) — but parameter() (parser.cpp:217-224) has already registered the definition's parameters as **new** symbols under a different scope id before that. So allocate_frame lays out the prototype's parameters while the body reads the definition's, whose frame_offset stays 0, which storage_of classifies as STORAGE_NOWHERE → the parameter always reads as 0. Measured: -[int] helper(int x); followed by -[int] helper(int x) { return x; } makes output helper(3); print 0; delete the prototype line and it prints 3. Grammar doc §4.3 recommends "add a prototype" as the forward-reference workaround — that is correct through the IR stage and only breaks once -S / -o is involved.
10. **The back end is not a pure consumer**: allocate_frame writes the layout back onto the symbols via set_frame_offset(), so generating twice from the same symbol table reuses the old offsets — which is also why the "already has an offset, skip it" rule exists.

---

## 6. Improvement roadmap (best value first)

**Cheap fixes**
- Expression statements throw their result away yet still leave a temporary behind (3 + 4; survives simplification as .L1 = 7): the rewrite pass could drop assignments whose result temporary is never read. Calling a void function as a statement already works (arg … ; f(, i.e. OP_PROC).
- Fix %% for OP_MOD, the null dereference in display()'s default branch, and the [Waring] typo.
- Report the start position of the failing construct (let the Lexer record token start offsets, or pass positions explicitly).
- The remaining half of the prototype defect: when define_function() reuses a prototype's Function object, replace its parameter list with the definition's freshly registered symbols — or simply reject the double registration (§5.9). This is the only known fault left in the -S / -o path; the optimiser side of the is_declare filter is already in.

**Structural upgrades**
- Introduce an AST (or at least split declaration from use into two passes) to unlock forward references and real int/char type checking. Array and pointer syntax has landed (single-level pointers, one-dimensional arrays, one slot per element); what remains is multi-dimensional arrays, array initialiser lists and bounds checking.
- Move semantic checks out of IRGenerator into a dedicated pass so the symbol table stops emitting code.
- Replace round-robin iteration with a worklist, add DCE/copy propagation, and re-run the analysis after simplification.

**Back end**
- Already there: three architectures across seven triples — macos-arm64 / linux-arm64 / macos-x86 / linux-x86 / windows-x86 / linux-x64 / windows-x64. ARM64 passes arguments in x0..x7 with stack arguments from the 9th; X86 splits each 64-bit value into a register pair, uses cdecl's all-stack arguments and its own sl_divmod long-division helper; X64 keeps one value per 64-bit register or 8-byte slot. One lowering, two syntaxes: Linux and macOS get GNU text, the two Windows triples get MASM text, and the Microsoft x64 four-register-plus-32-byte-shadow convention lives in the MASM emitter. The three share the 8-byte-slot frame layout (arrays take one slot per element, with the x10 fallback once an offset leaves the ARM64 x29 immediate window) and the scanf/printf I/O lowering; on the wrapper side Mach-O, ELF and COFF are all covered, linking goes through the host cc (with -arch plus an ad-hoc signature on macOS and -no-pie for linux-x86) and through ml / ml64 plus link on Windows. Measured working: recursion, multi-argument calls, char input, string output, arrays and pointers, 600 local variables; the Windows targets and the i386 formats get their runtime verdict from CI's native tool chains. The remaining inert fields are Function::frame_max_depth and current_esp_ptr (scope_esp only ever push/pops a constant 0).
- Missing wrappers, only two: macos-x64 would need the underscore-prefixed symbols and Mach-O section names wired through the X64 back end, and windows-arm64 is deliberately out of scope (target_supported states the reason).
- Already pinned down: test/e2e/run.sh and test/e2e/run.bat each perform compile → assemble → link → run → compare stdout and exit code, and only the shell one adds the four GNU cross-assembly rounds (linux-arm64 / linux-x86 / linux-x64 / macos-x86), each skipped as a whole when its probe fails. GitHub Actions runs build plus e2e on three host architectures, and AppVeyor covers both Windows triples natively with MSVC plus MASM - MASM text cannot be assembled locally, so it stays out of the cross rounds. Every emitted triple therefore ends up in front of a tool chain that can actually link it.
- Worth doing structurally: register allocation (every three-address instruction currently round-trips through memory), real widths for char and strings (everything is handled as a 64-bit integer, and the 1-byte char semantics exist only in the format string), jump tables and shorter prologues/epilogues.
