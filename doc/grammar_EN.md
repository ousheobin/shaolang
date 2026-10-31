# ShaoLang Language Grammar

Companion docs: [compiler design](compiler-design_EN.md) · [中文版本](grammar.md)

---

## 1. Lexical structure

### 1.1 Source files and whitespace

- Source files are plain text read as a byte stream; non-ASCII bytes (e.g. UTF-8 comments) are treated as ordinary characters. Only (char)-1 acts as the end-of-file sentinel.
- Whitespace is exactly four characters: space, tab, carriage return, line feed; it is skipped before each token.
- Several source files may be passed on one command line, but **each file is compiled independently** — symbol tables are not shared across files.

### 1.2 Comments

```
// line comment, runs to end of line
/* block comment, may span lines, nesting is NOT supported; a missing */ reports "注释没有结束" */
```

Comments are recognised inside read_operator() when / is seen and produce no token.

### 1.3 Identifiers and keywords

```
identifier ::= (letter | '_') (letter | digit | '_')*
letter     ::= A-Z | a-z
```

- Case sensitive; identifiers **may not start with a digit** (2fast reports ShaoLang不允许使用数字作为标识符的开头 [identifiers may not start with a digit]).
- All user-facing diagnostics are Chinese-only in this build; they are quoted verbatim below, with English glosses collected in the §6 table.
- Reserved words (cannot be used as identifiers):

| Group | Keywords |
|---|---|
| Types | int char void |
| Selection / loop | if else while |
| Flow control | break continue return |
| I/O | input output |

### 1.4 Literals

| Form | Token | Rules |
|---|---|---|
| 123 | C_INTEGER | Decimal, non-negative only; must be ≤ INT_MAX; no floating point (1.5 reports ShaoLang不支持浮点数); negatives require the unary operator |
| 'a', the newline escape | C_CHAR | Exactly one character or one escape between single quotes; empty or unterminated quotes are errors |
| "abc" | C_STRING | Double-quoted, single line only |

Supported escapes are listed in the fence below (shared by chars and strings); a backslash followed by an unrecognised letter simply yields that letter:

```
\\   \n   \r   \t   \"   \'   \0
```

### 1.5 Operators and delimiters

| Chars | Token | Role in the grammar |
|---|---|---|
| + - * / % | ADD SUB MUL DIV MOD | Binary arithmetic; - also unary negation, * also unary dereference and the pointer declarator |
| ++ -- | INC DEC | Prefix / postfix increment and decrement |
| ! | NOT | Logical negation |
| && \|\| | AND OR | Logical and / or |
| > >= < <= == != | GT GE LT LE EQ NEQ | Relational / equality |
| = | ASSIGN | Assignment |
| & | LEA | Unary address-of (§2.4) |
| \| | — | **Invalid character**: 符号'\|'不是ShaoLang可用的运算符 (only \|\| is legal) |
| << >> | LEFT_STREAM RIGHT_STREAM | Lexed, **not used by the grammar** |
| , ; : | COMMA SEMICOLON COLON | , in declarator/argument lists, ; terminates declarations and statements, : **not used** |
| ( ) [ ] { } | L/R_PARENTHESE L/R_BRACKET L/R_BRACE | Grouping, the -[ and ] function-definition marker, array declaration and indexing (§2.4), compound-statement braces |

### 1.6 Lexical error handling

The lexer never returns an "error token": a bad token is reported as [Error] and **dropped**, and scanning continues from the current character (lexer.cpp:78-81), so a single illegal character cannot block parsing.

---

## 2. Type system

### 2.1 Declarable types

int, char, void. void is only meaningful as a function return type (void v; currently compiles, but no operation can consume such a variable — see §7).

### 2.2 Literal types

C_INTEGER, C_CHAR, C_STRING. Declaration types and literal types are two separate enums; TypeUtil::canonical_type() maps C_INTEGER→int, C_CHAR→char, C_STRING→char.

### 2.3 Compatibility rules (type_util.cpp)

Operands come in two flavours: **address values** (pointer variables, array names, temporaries that hold an address) and **plain values** (basic-type variables, literals, whatever an index or dereference reads out). The check only looks at the flavour — the concrete base type is ignored:

- Two **plain values**: always compatible. So int ← char, char ← int, even 1 == "abc" produce no diagnostic.
- Two **address values**: canonical base types must be equal, so int *p cannot take a char *q.
- One address value + one plain value: incompatible, p = 42; reports 赋值的类型不正确.

Relational operators require equal base types for two address values and accept any pair of plain values; the result type is always int, which makes p == &x legal (see §5).

### 2.4 Pointers and arrays

Declarations accept two reference forms, both single-level:

```
int a[5];      // array whose length is a positive int literal, one slot per element
int *p;        // single-level pointer to int
```

- The length must be a **positive int literal**: int n = 5; int a[n]; reports 数组长度不符合规范, and int a[2][3] is rejected the same way.
- An element is always one slot (8 bytes), regardless of the base type. Arrays are laid out contiguously, in the stack frame for locals and in .zero static data for globals, and the address of a[i] is the base plus i slots.
- There is no array initializer and no whole-array assignment: both int a[2] = {1,2}; and a = p; are errors — elements must be written one by one.
- In an expression the array name **decays to the address of its first element** (the same as &a[0]), so p = a; and sum(a,2) work, and afterwards a and p are interchangeable.
- Pointer addition and subtraction move per element (one slot): p + 1 advances 8 bytes; subtracting two address values, p - a, yields the **element distance**, not a byte distance.
- Both *p and a[i] denote a **cell** (a storage unit), and a cell is readable and writable: *p = 42;, a[i] = v;, ++a[i];, input a[i]; are all legal.
- &x, &a[i] and &*p produce exactly that slot's address; taking the address of a literal or a temporary reports 左值表达式不正确.
- There is **no runtime bounds checking**; an out-of-range index simply reads or writes the neighbouring slot.

---

## 3. Grammar

Notation: ::= defines; | alternatives; [ X ] optional; X* zero or more; <...> non-terminals; **bold** keywords; ⟪token⟫ a lexical token class. What follows is the equivalent **abstract grammar**; the implementation is the left-recursion-removed recursive descent form (each level written as operand extend, see parser.cpp).

### 3.1 Program and top-level segments

```
program        ::= segment*
segment        ::= type var_definition
                 | '-' '[' type ']' ID '(' parameter ')' function_content
                 | '-' '[' type ']' ID '(' parameter ')' ';'      # prototype only
type           ::= **int** | **char** | **void**
```

The top level holds either global variable definitions or functions, in any order. Function definitions are introduced by the literal -[ marker with the return type in brackets: -[int] main(). That is what the implementation accepts, not a typo in this document.

### 3.2 Variable definitions

```
var_definition ::= declarator [ '=' expr ] definition_tail
declarator     ::= [ '*' ] ID [ '[' ⟪C_INTEGER⟫ ']' ]
definition_tail::= ',' declarator [ '=' expr ] definition_tail | ';'
```

- One type may introduce several declarators, and each name carries its own markers: int a = 1, *p, b[3];
- '*' and '[' are parsed independently, so int *p[3]; is accepted — it is just an array of 3 slots holding plain values: neither p[0] = &x; nor *(p[0]) can produce an address value, because the language has no second-level pointer.
- Globals may only be initialised with a **literal**; int a = 1 + 2; or int h = g; reports 全局变量无法初始化, because an expression initialiser has to be emitted into a function's instruction stream and the global scope has no current function.

### 3.3 Functions

```
parameter        ::= parameter_item parameter_tail | ε
parameter_item   ::= type [ '*' ] ID       # pointer parameters are written int *p; arrays arrive through them
parameter_tail   ::= ',' parameter_item parameter_tail | ε
function_content ::= block | ';'
```

- -[int] f(int a); is a **prototype**; it can later be completed by -[int] f(int a) { ... }.
- A return type can only be int / char / void — -[int *] f() does not reduce, so arrays and pointers travel only through pointer parameters.
- Declaring then defining requires the same parameter count and compatible types, otherwise 重复定义函数; defining a body twice also reports 重复定义函数.
- There is no check that every path of a non-void function returns; a missing return falls through as a valueless OP_RETURN.

### 3.4 Statements and compound statements

```
block       ::= '{' sub_program '}'
sub_program ::= ( var_definition_with_type | statement )*
statement   ::= while_stmt
              | if_stmt
              | 'break' ';'
              | 'continue' ';'
              | 'return' [ expr ] ';'
              | 'input' lvalue ';'            # read into a writable cell
              | 'output' expr ';'             # print an expression
              | expr ';'                    # expression statement
              | ';'                         # empty statement
while_stmt  ::= 'while' '(' [ expr ] ')' block
if_stmt     ::= 'if' '(' expr ')' (block | statement) [ 'else' (block | statement) ]
lvalue      ::= expr                        # grammatically a full expression; writability is checked while generating
```

- Declarations may be interleaved with statements (no C89 "declarations first" rule).
- A while body **must be a block**; each if branch may be a block or a single statement, and else binds to the nearest unmatched if.
- A bare block { ... } is **not a statement** — statement has no block branch:
  
  { int a = 1; }   -> [Error] Line 2:6 '}'符号处于不正确位置，或无法识别 ...
  
- break/continue are legal only inside the nearest while; otherwise break关键字没有处于while语句下 / continue关键字没有处于while语句下.
- Non-comparison loop conditions are legal (while(1), while(x)); a void condition is forced to true.
- The right-hand side of input is a full expression that must land on a **writable storage location**: input x;, input a[i]; and input *p; are all legal and each lowers to one OP_READ (for a cell such as a[i] or *p the write address is exactly the address the cell denotes). input 3; and input a + b; report 左值表达式不正确: input 的目标必须是可写的存储位置 [invalid left-value expression: the target of input must be writable storage], input zz; (undeclared) reports 使用未定义变量 [use of undefined variable].
- The right-hand side of output is the full expression chain, assignment included: output a * b + 1; and output a = 3; are both legal (the latter assigns, then prints), and a string literal may be printed directly. One OP_PRINT is emitted. A void expression is rejected — output; reports 表达式使用了返回值为void的函数 [expression uses a void-returning function].
- Neither statement appends a newline and neither takes a format string; the printed form follows the operand's literal type (char → %c, string → %s, everything else → %lld) — see §3.10 of the design document.

### 3.5 Expressions

```
expr          ::= assign_expr
assign_expr   ::= or_expr [ '=' assign_expr ]                 # re-extends on the result, so a = b = 7 works
or_expr       ::= and_expr ( '||' and_expr )*
and_expr      ::= compare_expr ( '&&' compare_expr )*
compare_expr  ::= arith_expr ( ( '>' | '>=' | '<' | '<=' | '==' | '!=' ) arith_expr )*
arith_expr    ::= item ( ( '+' | '-' ) item )*
item          ::= factor ( ( '*' | '/' | '%' ) factor )*
factor        ::= ( '!' | '-' | '++' | '--' | '&' | '*' ) factor
                | auto_inc_dec
auto_inc_dec  ::= element [ '++' | '--' ]
element       ::= ID id_extend
                | '(' expr ')'
                | ⟪C_INTEGER⟫ | ⟪C_CHAR⟫ | ⟪C_STRING⟫
id_extend     ::= '(' [ argument_list ] ')'                   # call
                | '[' expr ']'                                # indexing, see §2.4
                | ε                                           # rvalue use
argument_list ::= expr ( ',' expr )*
```

Notes:

- **There is no unary +**: + only appears in binary position. In y = -x + +a; the +a cannot reduce, so 常量定义处于不正确位置，或无法识别:+ is reported and the statement is dropped by error recovery.
- The left side of = must be an lvalue: an ordinary variable, a[i] and *p all qualify; literals, call results and temporaries give 左值表达式不正确 (1 = 2;), and an array name gives 左值表达式不正确: 数组名不可赋值 [invalid left-value expression: an array name cannot be assigned].
- Comparisons are left-associative and **chainable**: 1 > 2 == 0 parses as (1 > 2) == 0.
- Prefix ++ -- binds over the whole factor (right-associative); postfix ++ -- attaches to a single element, so -a++ is -(a++). ++/-- work on a[i] and *p too: the cell is loaded, adjusted, then written back to the same slot.
- Every operand is **normalised to a value** before an instruction is emitted (value_of in ir_generator): a cell (a[i] / *p) emits OP_PTR_GET and becomes a plain value, an array name decays into OP_LEA, and a pointer variable is used as-is as an address value. That is why output *p; and a[i] = *p + 1; need no extra parentheses.
- A call used as a **statement** is legal (p(3); takes the expression-statement branch and emits arg 3 ; p(, i.e. OP_PROC), so void functions are callable. The result is rejected as soon as it participates in an operation: a = h(); and output h(); both report 表达式使用了返回值为void的函数 [expression uses a void-returning function], and declaring with it — int z = h(); — reports 变量无法赋初值 [variable cannot be initialised] while the call itself is still emitted.

### 3.6 Precedence and associativity

Lowest to highest:

| Level | Operators | Associativity |
|---|---|---|
| 1 | = | right (implemented as "recurse for the right side, then re-extend on the result") |
| 2 | \|\| | left |
| 3 | && | left |
| 4 | > >= < <= == != | left (chaining allowed) |
| 5 | + - (binary) | left |
| 6 | * / % | left |
| 7 | ! - (unary), ++ -- (prefix), & * (unary) | right |
| 8 | ++ -- (postfix) | non-associative |
| 9 | ( ... ), literals, identifiers, calls, [ ] indexing | — |

Short-circuiting: && and || emit OP_AND / OP_OR three-address instructions — **not** conditional jumps — so both operands are always evaluated.

---

## 4. Semantic rules

### 4.1 Scoping

- Scopes are lexical and nested: file level is depth 1; each function body and each {} block adds one; parameters live in the function's own scope.
- Variables are stored and looked up by **scope path** (vector&lt;int>): redefining the same name **in the same level** reports 重复定义变量; defining it in an inner level shadows the outer one and is legal.
  
  int a = 1;
  -[int] main(){
      int a = 2;          // shadows the global a, legal
      if (1) { int a = 3; a = a + 1; }   // shadows again
      a = a + 1;          // refers to main's a
      return a;
  }
  
- Sibling blocks may each declare the same name (block ids increase monotonically, so they never collide).
- Lookup selects "the longest scope prefix that still matches in the current context", i.e. nearest-visible wins.

### 4.2 Initialisation

- A literal initialiser (int c = 'A';) is folded into the variable record; no assignment instruction is emitted.
- A local initialised with an expression generates an extra ASSIGN (declare, then assign).
- Reading an undeclared name reports 使用未定义变量.

### 4.3 Functions

- One table slot per name: same-name functions are distinguished by their parameter list, which is **not real overloading** — a second definition with a different signature reports 重复定义函数.
- Calls are resolved at parse time, so the callee must be declared or defined first:
  
  -[int] use(){ return later(1); }     // [Error] 使用未定义的函数
  -[int] later(int a){ return a; }
  
  Adding the prototype -[int] later(int a); before use fixes it — except once -S / -o emits assembly (see §7.10).
- Actual arguments are checked positionally against formals using §2.3; a mismatch in count or compatibility reports 函数的形参列表和实参列表不匹配.
- Recursion works (the function name is in the table before its body is parsed).
- A prototype with no definition is harmless on its own (the optimiser skips functions without an instruction stream), but calling it only shows up at link time — see §7.11.

### 4.4 Return values

- return e; where e is void (e.g. a void call result) reports 返回值与声明不匹配.
- return; is only legal in a void function.
- A non-void function whose body cannot reach a return is not diagnosed; the result is an uninitialised store.

---

## 5. Complete example

This program compiles without diagnostics (shaolang demo.sl) and exercises most of the rules above:

```
int base = 10;

-[int] add(int x, int y) {
    return x + y;
}

-[int] calc(int n) {
    int sum = 0;
    int i = 1;
    while (i <= n) {
        sum = sum + add(i, base);
        i++;
    }
    output sum;
    if (sum > 100)
        return sum;
    else
        return 0;
    return i;
}

-[int] main() {
    int n;
    input n;
    return calc(n);
}
```

The array and pointer forms compile the same way and print 60 then 15:

```
-[int] total(int *items, int n) {
    int sum = 0;
    int i = 0;
    while (i < n) {
        sum = sum + items[i];
        i++;
    }
    return sum;
}

-[int] main() {
    int scores[3];
    scores[0] = 10; scores[1] = 20; scores[2] = 30;
    int *cursor;
    cursor = scores;                 // the array name decays to &scores[0]
    output total(cursor, 3);
    output "\n";
    *cursor = *cursor + 5;           // rewrites scores[0]
    output scores[0];
    return 0;
}
```

Minimal legal program:

```
-[void] main() { }
```

---

## 6. Quick reference: illegal or unsupported constructs

All rows below were tested against the current build. Diagnostics are emitted in Chinese only; the verbatim message is quoted, with an English gloss in brackets.

| Construct | Symptom | Cause |
|---|---|---|
| input 3; | 左值表达式不正确: input 的目标必须是可写的存储位置 [invalid left-value expression] | input takes an lvalue expression; literals and temporaries are not writable (§3.4) |
| input zz; (zz undeclared) | 使用未定义变量 [use of undefined variable] | get_variable finds nothing, so no OP_READ is emitted |
| output; | 表达式使用了返回值为void的函数 [expression uses a void-returning function] | all_expr fails and yields voidVariable, which generate_print rejects |
| int n = 3; int a[n]; | 常量定义处于不正确位置 [constant definition in a wrong position] + 数组长度不符合规范 [array length does not conform] | only a positive int literal is accepted as a length (§2.4) |
| char s[2][3]; | 变量或函数定义不正确 [variable or function definition in a wrong position] | one-dimensional arrays only |
| a = p; (a is an array name) | 左值表达式不正确: 数组名不可赋值 [an array name cannot be assigned] | no whole-array assignment, no whole-array comparison |
| -[int *] f() | ']'符号处于不正确位置 [';' is in a wrong position] | return types are limited to int / char / void |
| p[0] = &x; (p declared as int *p[3]) | 赋值的类型不正确 [assignment types do not match] | the cell holds a plain value; there is no second-level pointer |
| { int a = 1; } as a statement | '}符号处于不正确位置 | no block branch in statement |
| +a (unary plus) | 常量定义处于不正确位置，或无法识别:+ [constant definition in a wrong position], the statement is dropped | factor does not list ADD |
| 1.5 | ShaoLang不支持浮点数 [floats are not supported] | no float type |
| 2fast | ShaoLang不允许使用数字作为标识符的开头 [identifiers may not start with a digit] | identifier rule |
| a \| b | 符号'\|'不是ShaoLang可用的运算符 ['\|' is not a ShaoLang operator] | only \|\| exists |
| a void call's result used in an operation (a = h(); / output h();) | 表达式使用了返回值为void的函数 [expression uses a function returning void] | a void call yields voidVariable, and every binary/unary operation rejects void; writing it as a statement — h(); — is legal |
| int z = h(); (initialising with a void call) | 变量无法赋初值 [variable cannot be initialised] | declaration-time initialisers go through the type compatibility check; the call still emits its OP_PROC |
| -[int] helper(int x); with no body | compiles; calling it fails at **link** time with 链接失败 [link failed] | a prototype has no instruction stream, so the missing symbol only surfaces in the linker (§7.11) |
| 1 = 2; | 左值表达式不正确 [invalid lvalue expression] | literals are not lvalues |
| int h = g; at global scope | 全局变量无法初始化:h [global variable cannot be initialised] | no instruction stream at global scope to carry the assignment |
| a = main(); where main is void | 表达式使用了返回值为void的函数 | same void rule |
| bare break; / continue; outside a while | break关键字没有处于while语句下 [break is not inside a while] | the jump patch stack is empty |

---

## 7. Known grammar / semantic gaps

Ordered by impact; each item is a fact about the code, not a guess.

1. **I/O is narrow in shape**: input writes exactly one writable unit at a time (a variable, a[i], or *p) — there is no bulk read; output appends no newline and accepts no format string.
2. **Expression statements discard their result**: 3 + 4; is legal and leaves an unused temporary (it becomes .L1 = 7 after constant propagation). There is no "useless expression" warning and no check on the discarded value.
3. **Reference types are single-level and unchecked**: arrays are one-dimensional only, the length must be an int literal, and there is no initializer list or whole-array assignment; pointers have no second-level dereference and cannot be a return type; a[i] performs no bounds check at runtime, so an out-of-range index reads or writes the neighbouring slot.
4. **Unary + is missing**: + only appears in binary position, so y = -x + +a; reports 常量定义处于不正确位置 and the statement is dropped by error recovery. generate_two_value_op still only returns NULL on a null operand (ir_generator.cpp:26), so whether a malformed expression is reported depends on whether the layer above already did.
5. **void local declarations are not diagnosed**: VARIABLE_CAN_NOT_BE_VOID is only checked on the unary-operation path and inside generate_read, so void v; passes.
6. **No overloading**: the function table holds one entry per name, so only the first signature survives.
7. **No block statements, for, switch, do-while, struct, bitwise operators, or cast syntax.**
8. **Diagnostics point past the fault**: messages print the scanner's *current* read position, not the start of the offending construct, so errors often appear on a later line/column (error.cpp:28-31).
9. **Errors do not stop the pipeline**: parsing and optimisation continue after diagnostics and the exit code stays 0 — only a bad command line exits with 2. With -S or -o a file that produced any error gets no target code at all (源文件存在错误，没有生成目标代码 [the source file has errors, no target code was generated]).
10. **Prototype plus later definition zeroes the parameters in assembly**: the definition reuses the prototype's function symbol and therefore its parameter list, while the body resolves to the differently registered parameter of the definition — frame layout and use end up pointing at different variables. The IR stage is entirely correct; only -S / -o breaks (measured in design doc §5.9: -[int] helper(int x); followed by a definition then helper(41) really prints 1). Prototypes therefore still do not really solve forward references — defining before using remains the reliable form.
11. **A prototype with no definition is only caught by the linker**: such a line on its own compiles cleanly (the optimiser now skips functions without an instruction stream), and a call to it produces 链接失败 [link failed] from the linker while the compiler still exits 0.
