# ShaoLang Cookbook

Companion docs: [language grammar](grammar_EN.md) · [compiler design](compiler-design_EN.md) · [中文版本](cookbook.md)

Every program in this file compiles with the current build, and the line under it is what it printed. Save a recipe as name.sl, build it with shaolang name.sl, run it with ./name. Where the language has no obvious way to do something, the recipe shows the form that works and says why in one line.

---

## 1. First contact

### 1.1 The smallest program

main is the entry point, and the -[int] marker in front of its name is the return type, not a typo:

```
-[int] main(){
  return 0;
}
```

Nothing is printed, and the exit code is 0. A -[void] main() is legal too, but then the exit code is whatever the last expression left behind, so give main an int return and an explicit return statement.

### 1.2 Say something

output prints one expression and adds nothing — no space, no newline. A line is assembled from pieces:

```
-[int] main(){
  int n = 7;
  output "n=";
  output n;
  output " ";
  output 2 * n;
  output "\n";
  return 0;
}
```

Prints:

```
n=7 14
```

The end of every program should be an output "\n"; line, or the shell prompt lands on the same line as the last result.

### 1.3 The exit code is a result

```
-[int] main(){
  int bad = 1;
  if(bad){
    return 42;
  }
  return 0;
}
```

Runs with exit code 42, which is how the tests in test/e2e assert a value that is easier to compare as a status than as text.

## 2. Printing

### 2.1 The printed form follows the operand's type

A char prints as a character, a string literal as a sequence, and everything else as a number. Arithmetic turns the character into a number, so ask for the form you want:

```
-[int] main(){
  char c = 'A';
  output c;
  output " ";
  output c + 1;
  output " ";
  output 'a';
  output "\n";
  return 0;
}
```

Prints:

```
A 66 a
```

c + 1 is an int expression, so it prints 66 rather than B. To print the letter, put the sum back into a char variable first.

### 2.2 A character array is not a string

There is no way to build a string at run time: no assignment from a literal to an array, no concatenation. Text is either a literal or characters printed one by one:

```
-[int] main(){
  char word[4];
  word[0] = 's';
  word[1] = 'h';
  word[2] = 'a';
  word[3] = 'o';
  int i = 0;
  while(i < 4){
    output word[i];
    i = i + 1;
  }
  output "\n";
  output "shao\n";
  return 0;
}
```

Prints:

```
shao
shao
```

Printing the array name instead of the elements takes its address as the value, so what comes out is one meaningless character rather than the letters.

### 2.3 Escapes, and the percent sign

A string literal is one line and its escapes are the seven in the grammar:

```
-[int] main(){
  output "100%% done\n";
  output "tab\tsep\n";
  output "quote\"q\"\n";
  output "back\\slash\n";
  return 0;
}
```

Prints:

```
100%% done
tab	sep
quote"q"
back\slash
```

output takes a value, not a format string, so %% and %s have no special meaning — they print as themselves. A backslash before a letter that is not one of those escapes simply yields that letter.

## 3. Reading

### 3.1 input writes one storage location

input takes something writable — a variable, a[i], or \*p — and reads exactly one value into it. It is a read, not an initialiser, so the shape is a bare declaration followed by the input:

```
-[int] main(){
  int n;
  input n;
  output "got ";
  output n;
  output "\n";
  return 0;
}
```

With 41 on standard input it prints got 41. Reading into a cell of an array works the same way, which is how a list is taken in:

```
-[int] main(){
  int n;
  input n;
  int a[5];
  int i = 0;
  int s = 0;
  while(i < n){
    input a[i];
    s = s + a[i];
    i = i + 1;
  }
  output s;
  output "\n";
  return 0;
}
```

With 3 10 20 30 it prints 60. input 3; and input a + b; are rejected: the target has to be writable.

### 3.2 A char read takes the next character, spaces included

Numbers skip leading whitespace; a char does not. With " 42" as the whole input, this program puts the space in c and 42 in n:

```
-[int] main(){
  char c;
  int n;
  input c;
  input n;
  output c;
  output " ";
  output n;
  output "\n";
  return 0;
}
```

Read the shape of the input before deciding whether a char read is safe to mix with number reads.

### 3.3 Read until a sentinel

There is no end-of-file test, so a loop that consumes an unknown number of values needs a value that means stop:

```
-[int] main(){
  int a[8];
  int n = 0;
  int v;
  input v;
  while(v != 0){
    a[n] = v;
    n = n + 1;
    input v;
  }
  int s = 0;
  int i = 0;
  while(i < n){
    s = s + a[i];
    i = i + 1;
  }
  output n;
  output " items, sum ";
  output s;
  output "\n";
  return 0;
}
```

With 3 4 5 0 it prints 3 items, sum 12. The array has to be big enough: no bounds check is performed, and the ninth value would simply land past the end.

## 4. Repeating

### 4.1 The counting loop

There is no for. The three parts of a counted loop are three separate statements:

```
-[int] main(){
  int n = 5;
  int s = 0;
  int i = 0;
  while(i < n){
    s = s + i;
    i = i + 1;
  }
  output s;
  output "\n";
  return 0;
}
```

Prints 10. i = i + 1; can be written i ++; or ++ i;, on a variable, a[i] or \*p alike.

### 4.2 A while body is a block

The body has to be braces; a single statement after while gets reported as a missing brace. if is more forgiving and takes either a block or one statement.

```
-[int] main(){
  int i = 0;
  while(i < 3)
    i = i + 1;
  return 0;
}
```

reports 缺少'{'符号 (a missing '{' symbol) at the body and 缺少'}'符号 (a missing '}') at the end of the file, and builds nothing.

### 4.3 break, continue, and the loop that never ends

Both keywords belong to the nearest enclosing while, and outside one they are errors. while(1) plus a break is the form for loops whose exit test is somewhere in the middle:

```
-[int] main(){
  int n = 1;
  int s = 0;
  while(1){
    if(n > 10){
      break;
    }
    if(n % 3 == 0){
      n = n + 1;
      continue;
    }
    s = s + n;
    n = n + 1;
  }
  output s;
  output "\n";
  return 0;
}
```

Prints 37. continue skips the rest of the body, so it has to bump the counter itself — the usual way to hang a program in this language is a continue that forgets that.

## 5. Arrays and pointers

### 5.1 Length is a literal, and there is no initialiser list

An array is one base type, one dimension, and a positive int literal for the length. Values arrive element by element:

```
-[int] main(){
  int score[4];
  int i = 0;
  int n = 4;
  while(i < n){
    score[i] = i * i;
    i = i + 1;
  }
  output score[3];
  output "\n";
  return 0;
}
```

Prints 9. Writing int n = 4; int a[n]; reports 数组长度不符合规范 (the array length does not conform), and int a[3] = {1, 2, 3}; reports 变量或函数定义不正确 (a wrong variable or function definition) — an int a[4]; and four assignments is the whole of it.

### 5.2 Keep the length next to the array

An array name decays to the address of its first element and carries no length with it, so functions take the pair:

```
-[int] total(int *a, int n){
  int s = 0;
  int i = 0;
  while(i < n){
    s = s + a[i];
    i = i + 1;
  }
  return s;
}
-[int] main(){
  int score[4];
  int i = 0;
  while(i < 4){
    score[i] = i * i;
    i = i + 1;
  }
  output total(score, 4);
  output "\n";
  return 0;
}
```

Prints 14. total(score, 4) and a pointer variable holding the same address are interchangeable, because the parameter is a pointer either way.

### 5.3 Walking a pointer

Pointer arithmetic moves by elements, one 8-byte slot at a time, and subtracting two addresses in the same array gives the distance in elements rather than bytes. A run that ends in a zero slot counts itself:

```
-[int] main(){
  int a[4];
  a[0] = 10;
  a[1] = 20;
  a[2] = 30;
  a[3] = 0;
  int *p;
  p = a;
  int n = 0;
  while(*p){
    n = n + 1;
    p = p + 1;
  }
  output n;
  output " ";
  output * (p - 1);
  output " ";
  output p - a;
  output "\n";
  return 0;
}
```

Prints 3 30 3. &x, &a[i] and &\*p all give the address of that slot, so a pointer and an index describe the same cell: p = a; then \*(p + 2) is a[2].

### 5.4 Give a caller its value back

There are no references and no second return value, so an out-parameter is a pointer the caller fills in:

```
-[int] set_two(int *a, int *b, int v){
  *a = v;
  *b = v + 1;
  return 0;
}
-[int] main(){
  int x = 0;
  int y = 0;
  set_two(&x, &y, 7);
  output x;
  output " ";
  output y;
  output "\n";
  return 0;
}
```

Prints 7 8. Arrays arrive through the same pointer parameter; the caller writes the array name and the callee writes a[i].

## 6. Functions

### 6.1 Recursion

A function is in the symbol table before its body is parsed, so it can call itself:

```
-[int] fact(int n){
  if(n <= 1){
    return 1;
  }
  return n * fact(n - 1);
}
-[int] main(){
  output fact(10);
  output "\n";
  return 0;
}
```

Prints 3628800. A void function is called as a statement, and its result cannot take part in an expression:

```
-[void] noop(){
}
-[void] walk(int n){
  if(n > 0){
    noop();
    walk(n - 1);
  }
}
-[int] main(){
  walk(200);
  output "ok\n";
  return 0;
}
```

Prints ok. Each call takes a frame, so depth is bounded by the stack rather than by the compiler.

### 6.2 Define before you call

Calls are resolved while parsing, so a function used before it exists is reported as 使用未定义的函数. The reliable order puts every definition above its uses:

```
-[int] later(int a){
  return a + 1;
}
-[int] use(){
  return later(41);
}
-[int] main(){
  output use();
  output "\n";
  return 0;
}
```

Prints 42. A prototype on its own does not buy the same program: this version, with -[int] later(int a); ahead of use and the body still after it, compiles and runs but prints 1 — the definition reuses the prototype's parameter slots while its body resolves to different ones. Prototypes are documented in full in section 7.10 of the grammar; treat them as not working and write the definition first.

## 7. Numbers and logic

### 7.1 Division truncates toward zero

Only int and char arithmetic exists — no floats. / throws the fraction away and % keeps the sign of the left operand:

```
-[int] main(){
  output 7 / 2;
  output " ";
  output 0 - 7 / 2;
  output " ";
  output 7 % 3;
  output " ";
  output 0 - 7 % 3;
  output " ";
  output 100 / 7;
  output "\n";
  return 0;
}
```

Prints 3 -3 1 -1 14. Averages need the sum first: sum / count; truncates, and there is no rounding operator. Division by zero does not trap: the quotient comes back 0 and the remainder is the dividend.

### 7.2 && and || evaluate both sides

Both operands of a logical operator are always computed, because they lower to one three-address instruction rather than a conditional jump. That makes the C habit of guarding an expression with && wrong:

```
-[int] loud(int v){
  output "[loud]";
  return v;
}
-[int] main(){
  output 0 && loud(1);
  output " ";
  output 1 || loud(2);
  output "\n";
  return 0;
}
```

Prints [loud]0 [loud]1 — loud ran both times, even though the result was already decided. Put the guard in an if instead:

```
-[int] ratio(int a, int b){
  if(b == 0){
    return 0;
  }
  return a / b;
}
-[int] main(){
  output ratio(10, 3);
  output " ";
  output ratio(10, 0);
  output "\n";
  return 0;
}
```

Prints 3 0. Comparisons chain, always to the left: 1 > 2 == 0 is (1 > 2) == 0, which is 1.

### 7.3 Ladders of conditions

There is no else if keyword and no switch, so a decision ladder is an else that holds another if:

```
-[int] main(){
  int i = 1;
  while(i <= 15){
    if(i % 15 == 0){
      output "FizzBuzz";
    } else {
      if(i % 3 == 0){
        output "Fizz";
      } else {
        if(i % 5 == 0){
          output "Buzz";
        } else {
          output i;
        }
      }
    }
    output " ";
    i = i + 1;
  }
  output "\n";
  return 0;
}
```

Prints 1 2 Fizz 4 Buzz Fizz 7 8 Fizz Buzz 11 Fizz 13 14 FizzBuzz. Braces are not optional in the nested branches above, because else binds to the nearest unmatched if.

## 8. Names

### 8.1 Globals take literals only

A global lives outside every function, and an expression initialiser needs a function's instruction stream to run in, so only a literal is allowed:

```
int base = 10;
-[int] inner(){
  int base = 3;
  return base;
}
-[int] main(){
  output base;
  output " ";
  output inner();
  output "\n";
  if(1){
    int base = 5;
    output base;
  }
  output " ";
  output base;
  output "\n";
  return 0;
}
```

Prints 10 3 then 5 10. int h = g; at the top level reports 全局变量无法初始化 (a global variable cannot be initialised).

### 8.2 Shadowing is nearest-wins

Scopes nest: the file is one, each function body and each pair of braces adds one. A name declared in an inner level hides the outer one and the outer one comes back as soon as the block ends, as the program above shows — the global base was still 10 after the block that declared its own. Sibling blocks may reuse a name freely. Redefining a name at the same level is the case that reports 重复定义变量 (a duplicated variable definition).

## 9. Where to look next

- [grammar_EN.md](grammar_EN.md) — the full grammar, plus the table of constructs that are rejected and the list of known gaps.
- [compiler-design_EN.md](compiler-design_EN.md) — how a program reaches assembly, and what each back end guarantees.
- test/e2e — 25 committed programs in the same shapes as these recipes, each with its expected output and, where the status matters, its expected exit code; the two runners add a case with hundreds of locals of their own.
