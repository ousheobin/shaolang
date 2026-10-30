# ShaoLang Language Grammar

Companion docs: [compiler design](compiler-design_EN.md) · [中文版本](grammar.md)

---

## 1. Lexical structure

### 1.1 Source files and whitespace

- Source files are plain text read as a byte stream; non-ASCII bytes (e.g. UTF-8 comments) are treated as ordinary characters. Only `(char)-1` acts as the end-of-file sentinel.
- Whitespace: space, `\t`, `\n`, `\r`, skipped before each token.
- Several source files may be passed on one command line, but **each file is compiled independently** — symbol tables are not shared across files.

### 1.2 Comments

```
// line comment, runs to end of line
/* block comment, may span lines, nesting is NOT supported; a missing */ reports "注释没有结束" */
```

Comments are recognised inside `read_operator()` when `/` is seen and produce no token.

### 1.3 Identifiers and keywords

```
identifier ::= (letter | '_') (letter | digit | '_')*
letter     ::= A-Z | a-z
```

- Case sensitive; identifiers **may not start with a digit** (`2fast` reports `ShaoLang不允许使用数字作为标识符的开头`).
- Reserved words (cannot be used as identifiers):

| Group | Keywords |
|---|---|
| Types | `int` `char` `void` |
| Selection / loop | `if` `else` `while` |
| Flow control | `break` `continue` `return` |
| I/O | `input` `output` (**not reachable from the grammar, see §6**) |

### 1.4 Literals

| Form | Token | Rules |
|---|---|---|
| `123` | `C_INTEGER` | Decimal, non-negative only; must be ≤ `INT_MAX`; no floating point (`1.5` reports `ShaoLang不支持浮点数`); negatives require the unary operator |
| `'a'` `'\n'` | `C_CHAR` | Exactly one character or one escape between single quotes; empty or unterminated quotes are errors |
| `"abc"` | `C_STRING` | Double-quoted, single line only |

Supported escapes (shared by chars and strings): `\\` `\n` `\r` `\t` `\"` `\'` `\0`; an unrecognised escape such as `\x` yields `x`.

### 1.5 Operators and delimiters

| Chars | Token | Role in the grammar |
|---|---|---|
| `+` `-` `*` `/` `%` | `ADD` `SUB` `MUL` `DIV` `MOD` | Binary arithmetic; `-` also unary negation |
| `++` `--` | `INC` `DEC` | Prefix / postfix increment and decrement |
| `!` | `NOT` | Logical negation |
| `&&` `\|\|` | `AND` `OR` | Logical and / or |
| `>` `>=` `<` `<=` `==` `!=` | `GT` `GE` `LT` `LE` `EQ` `NEQ` | Relational / equality |
| `=` | `ASSIGN` | Assignment |
| `&` | `LEA` | Address-of — **lexed but never used by the grammar** |
| `\|` | — | **Invalid character**: `符号'\|'不是ShaoLang可用的运算符` (only `\|\|` is legal) |
| `<<` `>>` | `LEFT_STREAM` `RIGHT_STREAM` | Lexed, **not used by the grammar** |
| `,` `;` `:` | `COMMA` `SEMICOLON` `COLON` | `,` in declarator/argument lists, `;` terminates declarations and statements, `:` **not used** |
| `(` `)` `[` `]` `{` `}` | `L/R_PARENTHESE` `L/R_BRACKET` `L/R_BRACE` | Grouping, the `-[` `]` function-definition marker, compound-statement braces |

### 1.6 Lexical error handling

The lexer never returns an "error token": a bad token is reported as `[Error]` and **dropped**, and scanning continues from the current character (`lexer.cpp:78-81`), so a single illegal character cannot block parsing.

---

## 2. Type system

### 2.1 Declarable types

`int`, `char`, `void`. `void` is only meaningful as a function return type (`void v;` currently compiles, but no operation can consume such a variable — see §7).

### 2.2 Literal types

`C_INTEGER`, `C_CHAR`, `C_STRING`. Declaration types and literal types are two separate enums; `TypeUtil::canonical_type()` maps `C_INTEGER→int`, `C_CHAR→char`, `C_STRING→char`.

### 2.3 Compatibility rules (`type_util.cpp:17-28`)

The check only distinguishes "reference-like (pointer/array)" from "basic" — the concrete base type is ignored:

- Two **non-reference** operands: always compatible. So `int ← char`, `char ← int`, even `1 == "abc"` produce no diagnostic.
- Two **reference** operands: canonical base types must be equal.
- One reference + one non-reference: incompatible.

Relational operators accept any non-reference operands and always yield type `int`.

### 2.4 Reserved but unreachable: pointers and arrays

`Variable` carries `is_pointer` / `is_array` / `array_length` / `ptr`; the IR defines `OP_LEA` / `OP_PTR_SET` / `OP_PTR_GET`; the semantic error table has `ARRAY_LENGTH_INVAILD` and friends. But **no production can build a pointer or array type**, `set_pointer()` has no call site, so `get_pointer()` is always `NULL`. Writing array syntax falls into error recovery:

```
int b[3];        -> [Error] ... 变量或函数定义不正确 / 类型定义处于不正确位置
b[0] = 1;        -> [Error] ... 缺少'('符号 / 标识符定义处于不正确位置
```

---

## 3. Grammar

Notation: `::=` defines; `|` alternatives; `[ X ]` optional; `X*` zero or more; `<...>` non-terminals; **bold** keywords; `⟪token⟫` a lexical token class. What follows is the equivalent **abstract grammar**; the implementation is the left-recursion-removed recursive descent form (each level written as `operand extend`, see `parser.cpp`).

### 3.1 Program and top-level segments

```
program        ::= segment*
segment        ::= type var_definition
                 | '-' '[' type ']' ID '(' parameter ')' function_content
                 | '-' '[' type ']' ID '(' parameter ')' ';'      # prototype only
type           ::= **int** | **char** | **void**
```

The top level holds either global variable definitions or functions, in any order. Function definitions are introduced by the literal `-[` marker with the return type in brackets: `-[int] main()`. That is what the implementation accepts, not a typo in this document.

### 3.2 Variable definitions

```
var_definition ::= ID [ '=' expr ] definition_tail
definition_tail::= ',' var_definition | ';'
```

- One type may introduce several declarators: `int a = 1, b, c = 3;`
- Globals may only be initialised with a **literal**; `int a = 1 + 2;` or `int h = g;` reports `全局变量无法初始化`, because an expression initialiser has to be emitted into a function's instruction stream and the global scope has no current function.

### 3.3 Functions

```
parameter        ::= parameter_item parameter_tail | ε
parameter_item   ::= type ID              # no pointer/array parameters
parameter_tail   ::= ',' parameter_item parameter_tail | ε
function_content ::= block | ';'
```

- `-[int] f(int a);` is a **prototype**; it can later be completed by `-[int] f(int a) { ... }`.
- Declaring then defining requires the same parameter count and compatible types, otherwise `重复定义函数`; defining a body twice also reports `重复定义函数`.
- There is no check that every path of a non-void function returns; a missing `return` falls through as a valueless `OP_RETURN`.

### 3.4 Statements and compound statements

```
block       ::= '{' sub_program '}'
sub_program ::= ( var_definition_with_type | statement )*
statement   ::= while_stmt
              | if_stmt
              | 'break' ';'
              | 'continue' ';'
              | 'return' [ expr ] ';'
              | expr ';'                    # expression statement
              | ';'                         # empty statement
while_stmt  ::= 'while' '(' [ expr ] ')' block
if_stmt     ::= 'if' '(' expr ')' (block | statement) [ 'else' (block | statement) ]
```

- Declarations may be interleaved with statements (no C89 "declarations first" rule).
- A `while` body **must be a block**; each `if` branch may be a block or a single statement, and `else` binds to the nearest unmatched `if`.
- A bare block `{ ... }` is **not a statement** — `statement` has no `block` branch:
  ```
  { int a = 1; }   -> [Error] Line 2:6 '}'符号处于不正确位置，或无法识别 ...
  ```
- `break`/`continue` are legal only inside the nearest `while`; otherwise `break关键字没有处于while语句下` / `continue关键字没有处于while语句下`.
- Non-comparison loop conditions are legal (`while(1)`, `while(x)`); a `void` condition is forced to true.

### 3.5 Expressions

```
expr          ::= assign_expr
assign_expr   ::= or_expr [ '=' assign_expr ]                 # re-extends on the result, so a = b = 7 works
or_expr       ::= and_expr ( '||' and_expr )*
and_expr      ::= compare_expr ( '&&' compare_expr )*
compare_expr  ::= arith_expr ( ( '>' | '>=' | '<' | '<=' | '==' | '!=' ) arith_expr )*
arith_expr    ::= item ( ( '+' | '-' ) item )*
item          ::= factor ( ( '*' | '/' | '%' ) factor )*
factor        ::= ( '!' | '-' | '++' | '--' ) factor
                | auto_inc_dec
auto_inc_dec  ::= element [ '++' | '--' ]
element       ::= ID id_extend
                | '(' expr ')'
                | ⟪C_INTEGER⟫ | ⟪C_CHAR⟫ | ⟪C_STRING⟫
id_extend     ::= '(' [ argument_list ] ')'                   # call
                | '=' expr                                    # direct assignment
                | ε                                           # rvalue use
argument_list ::= expr ( ',' expr )*
```

Notes:

- **There is no unary `+`**: `+` only appears in binary position. In `y = -x + +a;` the `+a` cannot reduce, and the whole statement is **silently dropped** (no diagnostic, `y` never assigned) — see §7.
- The left side of `=` must be an lvalue; literals, call results and temporaries give `左值表达式不正确` (`1 = 2;`).
- Comparisons are left-associative and **chainable**: `1 > 2 == 0` parses as `(1 > 2) == 0`.
- Prefix `++ --` binds over the whole `factor` (right-associative); postfix `++ --` attaches to a single `element`, so `-a++` is `-(a++)`.
- `!` and `-` on a reference operand first emit `PTR_GET`; since no reference variable is writable in the source, only basic operands actually reach that path.
- A call used as an expression may not return `void` (`表达式使用了返回值为void的函数`); a call used as a statement (`f(1);`) also goes through `element`, so **void functions cannot appear in any expression position at all** (see §6).

### 3.6 Precedence and associativity

Lowest to highest:

| Level | Operators | Associativity |
|---|---|---|
| 1 | `=` | right (implemented as "recurse for the right side, then re-extend on the result") |
| 2 | `\|\|` | left |
| 3 | `&&` | left |
| 4 | `>` `>=` `<` `<=` `==` `!=` | left (chaining allowed) |
| 5 | `+` `-` (binary) | left |
| 6 | `*` `/` `%` | left |
| 7 | `!` `-` (unary), `++` `--` (prefix) | right |
| 8 | `++` `--` (postfix) | non-associative |
| 9 | `( ... )`, literals, identifiers, calls | — |

Short-circuiting: `&&` and `||` emit `OP_AND` / `OP_OR` three-address instructions — **not** conditional jumps — so both operands are always evaluated.

---

## 4. Semantic rules

### 4.1 Scoping

- Scopes are lexical and nested: file level is depth 1; each function body and each `{}` block adds one; parameters live in the function's own scope.
- Variables are stored and looked up by **scope path** (`vector<int>`): redefining the same name **in the same level** reports `重复定义变量`; defining it in an inner level shadows the outer one and is legal.
  ```
  int a = 1;
  -[int] main(){
      int a = 2;          // shadows the global a, legal
      if (1) { int a = 3; a = a + 1; }   // shadows again
      a = a + 1;          // refers to main's a
      return a;
  }
  ```
- Sibling blocks may each declare the same name (block ids increase monotonically, so they never collide).
- Lookup selects "the longest scope prefix that still matches in the current context", i.e. nearest-visible wins.

### 4.2 Initialisation

- A literal initialiser (`int c = 'A';`) is folded into the variable record; no assignment instruction is emitted.
- A local initialised with an expression generates an extra `ASSIGN` (declare, then assign).
- Reading an undeclared name reports `使用未定义变量`.

### 4.3 Functions

- One table slot per name: same-name functions are distinguished by their parameter list, which is **not real overloading** — a second definition with a different signature reports `重复定义函数`.
- Calls are resolved at parse time, so the callee must be declared or defined first:
  ```
  -[int] use(){ return later(1); }     // [Error] 使用未定义的函数
  -[int] later(int a){ return a; }
  ```
  Adding the prototype `-[int] later(int a);` before `use` fixes it.
- Actual arguments are checked positionally against formals using §2.3; a mismatch in count or compatibility reports `函数的形参列表和实参列表不匹配`.
- Recursion works (the function name is in the table before its body is parsed).

### 4.4 Return values

- `return e;` where `e` is void (e.g. a void call result) reports `返回值与声明不匹配`.
- `return;` is only legal in a `void` function.
- A non-void function whose body cannot reach a `return` is not diagnosed; the result is an uninitialised store.

---

## 5. Complete example

This program compiles without diagnostics (`shaolang demo.sl`) and exercises most of the rules above:

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
    if (sum > 100)
        return sum;
    else
        return 0;
    return i;
}
```

Minimal legal program:

```
-[void] main() { }
```

---

## 6. Quick reference: illegal or unsupported constructs

All rows below were tested against the current build.

| Construct | Symptom | Cause |
|---|---|---|
| `output output n;` / `input n;` | a cascade starting with `'}'符号处于不正确位置` | `K_INPUT`/`K_OUTPUT` are missing from the `sub_program` FIRST set (`parser.cpp:22`), and both branches in `statement()` additionally require the keyword to **appear twice** (`parser.cpp:360`, `parser.cpp:370` — the latter even expects `input output x;`). Conclusion: **I/O statements are currently unusable** |
| `int b[3];`, `b[0] = 1;` | several `类型定义处于不正确位置` / `缺少'('符号` | no array grammar (§2.4) |
| `p = &b;` | syntax error | `&` token exists but is unused |
| `{ int a = 1; }` as a statement | `'}'符号处于不正确位置` | no `block` branch in `statement` |
| `+a` (unary plus) | the enclosing assignment is **silently dropped**, no error | `factor` does not list `ADD` |
| `1.5` | `ShaoLang不支持浮点数` | no float type |
| `2fast` | `ShaoLang不允许使用数字作为标识符的开头` | identifier rule |
| `a \| b` | `符号'\|'不是ShaoLang可用的运算符` | only `\|\|` exists |
| using a `void` call's result, or `f(1);` as a statement | `表达式使用了返回值为void的函数` | a void call yields `voidVariable`, and every binary/unary operation rejects void |
| `1 = 2;` | `左值表达式不正确` | literals are not lvalues |
| `int h = g;` at global scope | `全局变量无法初始化:h` | no instruction stream at global scope to carry the assignment |
| `a = main();` where `main` is void | `表达式使用了返回值为void的函数` | same void rule |
| bare `break;` / `continue;` outside a `while` | `break关键字没有处于while语句下` | the jump patch stack is empty |

---

## 7. Known grammar / semantic gaps

Ordered by impact; each item is a fact about the code, not a guess.

1. **`input` / `output` are unusable**: the keywords are lexed and `statement()` even has branches for them, but the missing FIRST-set entry plus the duplicated-keyword check make every spelling an error.
2. **Void functions cannot be called as statements**: expressions reject void unconditionally and there is no call-statement production, so a defined `-[void] f(){}` can never be invoked.
3. **Pointers and arrays cannot be written at all**: data structures, IR instructions and error codes are all in place; only the grammar is missing.
4. **Unary `+` is missing and failures are silent**: `generate_two_value_op` returns `NULL` on a null operand without reporting (`ir_generator.cpp:26`), so a parse failure surfaces as "the statement vanished".
5. **`void` local declarations are not diagnosed**: `VARIABLE_CAN_NOT_BE_VOID` is only checked on the unary-operation path, so `void v;` passes.
6. **No overloading**: the function table holds one entry per name, so only the first signature survives.
7. **No block statements, `for`, `switch`, `do-while`, `struct`, address-of/dereference, bitwise operators, or cast syntax.**
8. **Diagnostics point past the fault**: messages print the scanner's *current* read position, not the start of the offending construct, so errors often appear on a later line/column (`error.cpp:28-31`).
9. **Errors do not stop the pipeline**: parsing and optimisation continue after diagnostics; the process always exits with `0` and only summarises `N Warning,M Errors` at the end.
