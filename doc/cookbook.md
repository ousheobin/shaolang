# ShaoLang 编程手册

配套文档：[语言语法](grammar.md) · [编译器设计文档](compiler-design.md) · [English version](cookbook_EN.md)

本文每个程序都能在当前版本上编译通过，程序下面那行就是它实际打印的内容。把食谱存成 name.sl，用 shaolang name.sl 编译，再用 ./name 运行。语言里没有现成写法的地方，本文给出可用的那种写法，并用一句话说明原因。

---

## 1. 第一个程序

### 1.1 最小的程序

main 是入口，名字前面的 -[int] 是返回类型，不是笔误：

```
-[int] main(){
  return 0;
}
```

什么都不打印，退出码 0。-[void] main() 也合法，但退出码就是最后留在返回寄存器里的东西，所以 main 一律写成 int 返回并给出明确的 return。

### 1.2 打印一句话

output 一次打印一个表达式，不加空格也不换行，一行内容要自己拼：

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

打印：

```
n=7 14
```

每个程序都应以 output "\n"; 收尾，否则 shell 的提示符会挤在最后一行结果后面。

### 1.3 退出码也是一种结果

```
-[int] main(){
  int bad = 1;
  if(bad){
    return 42;
  }
  return 0;
}
```

以退出码 42 结束。test/e2e 里那些关心返回值的用例就是这么断言的：比退出码比比起文本更省事。

## 2. 打印

### 2.1 打印的形式由操作数的类型决定

char 打印成字符，字符串字面量打印成一串，其余一律打印成数。算术会把字符变回数值，所以要先想清楚要哪一种：

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

打印：

```
A 66 a
```

c + 1 是 int 表达式，于是打印 66 而不是 B。要打印字母，先把和存回一个 char 变量再打印那个变量。

### 2.2 字符数组不是字符串

运行期造不出字符串：不能把字面量赋给数组，也没有拼接。文本要么是字面量，要么一个字符一个字符地打印：

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

打印：

```
shao
shao
```

直接打印数组名，取到的是它的地址，打印出来的是一个无意义的字符，而不是那几个字母。

### 2.3 转义，以及百分号

字符串字面量不能跨行，可用的转义只有文法里那七个：

```
-[int] main(){
  output "100%% done\n";
  output "tab\tsep\n";
  output "quote\"q\"\n";
  output "back\\slash\n";
  return 0;
}
```

打印：

```
100%% done
tab	sep
quote"q"
back\slash
```

output 拿到的是一个值，不是格式串，所以 %% 和 %s 都没有特殊含义，照原样打印。\\ 之外的反斜杠也不算转义：反斜杠后面跟一个认不出来的字母，结果就是那个字母。

## 3. 读入

### 3.1 input 一次写一个存储位置

input 的参数必须可写——变量、a[i] 或 \*p 都行——一次只读一个值。它是读入，不是初始化，所以写法是一行裸声明紧跟一条 input：

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

标准输入给 41，打印 got 41。往数组的元素里读是同一个写法，一串数就是这么进来的：

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

输入 3 10 20 30，打印 60。input 3; 和 input a + b; 会被拒绝：目标必须可写。

### 3.2 读字符时，空格也算一个字符

读数值会跳过前导空白，读字符不会。整段输入是 " 42" 时，下面这个程序把空格放进 c，把 42 放进 n：

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

字符读和数值读混用之前，先看清输入长什么样。

### 3.3 读到哨兵值为止

没有判断文件结束的办法，所以个数不定的循环要约定一个表示结束的值：

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

输入 3 4 5 0，打印 3 items, sum 12。数组必须够大：运行期不做越界检查，第九个数就直接写到末尾之外。

## 4. 循环

### 4.1 计数循环

没有 for。计数循环的三段就是三条语句：

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

打印 10。i = i + 1; 也可以写成 i ++; 或 ++ i;，变量、a[i]、\*p 都能用。

### 4.2 while 的循环体必须是块

循环体只有花括号这一种写法，紧跟一条语句会报缺少花括号。if 宽松一些，块和单条语句都接受。

```
-[int] main(){
  int i = 0;
  while(i < 3)
    i = i + 1;
  return 0;
}
```

在循环体处报 缺少'{'符号，在文件末尾报 缺少'}'符号，且不生成目标代码。

### 4.3 break、continue 与永不结束的循环

两个关键字都只属于最近的一层 while，在外面写就是错误。退出判断落在循环中间时，用 while(1) 配 break：

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

打印 37。continue 跳过循环体剩下的部分，所以计数得自己加——这门语言里最经典的死循环，就是 continue 忘了这一句。

## 5. 数组与指针

### 5.1 长度必须是字面量，也没有初始化列表

数组只有一种元素类型、一维，长度是一个正的 int 字面量。逐个元素赋值：

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

打印 9。写 int n = 4; int a[n]; 会报 数组长度不符合规范，写 int a[3] = {1, 2, 3}; 会报 变量或函数定义不正确——一行声明加四次赋值就是全部。

### 5.2 数组和它的长度一起传

数组名退化到首元素地址，长度不带过去，所以函数收一对参数：

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

打印 14。传 total(score, 4) 和传一个装着同一地址的指针变量可以互换，因为形参本来就是指针。

### 5.3 用指针走一遍数组

指针加减按元素移动（一个 8 字节槽），同一个数组里两个地址相减得到的是元素间距而不是字节间距。以 0 结尾的一段可以自己数自己：

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

打印 3 30 3。&x、&a[i]、&\*p 取到的都是那个槽的地址，所以指针和下标说的是同一个单元：p = a; 之后 \*(p + 2) 就是 a[2]。

### 5.4 把值还给调用者

没有引用，也不能返回第二个值，所以出参就是一个指针，由调用者把地址给它：

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

打印 7 8。数组走的也是同一个指针形参：调用者写数组名，被调用者写 a[i]。

## 6. 函数

### 6.1 递归

函数的名字在解析函数体之前就进了符号表，所以它能调用自己：

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

打印 3628800。void 函数作为一条语句来调用，它的结果不能参与表达式：

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

打印 ok。每次调用都要一个栈帧，递归深度受栈限制，而不是受编译器限制。

### 6.2 先定义，后调用

调用在解析期就定了，函数还没出现就被使用会报 使用未定义的函数。可靠的顺序是把每个定义写在调用之前：

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

打印 42。只加一行声明并不能得到同样的程序：把 -[int] later(int a); 写在 use 前面、函数体仍写在后面，程序能编译能运行，但打印的是 1——定义沿用了声明那一份形参槽位，函数体里解析到的却是另外注册的变量。声明的这个坑在语法文档 7.10 节有完整说明，实践上就当作它不可用，先写定义。

## 7. 数值与逻辑

### 7.1 除法向零取整

只有 int 和 char 的算术，没有浮点数。/ 把小数部分丢掉，% 的符号跟着左操作数：

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

打印 3 -3 1 -1 14。求平均值要先把和算出来：sum / count; 是截断的，也没有四舍五入的运算符。除以 0 不会中断程序，商取 0、余数取被除数。

### 7.2 && 与 || 两边都算

逻辑运算的两个操作数一定会被分别求值——它们降写成一条三地址指令，而不是条件跳转。所以 C 里那种用 && 挡住后一项的写法是错的：

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

打印 [loud]0 [loud]1——结果早就定了，loud 还是跑了两次。要挡就把判断写进 if：

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

打印 3 0。比较运算左结合而且可以连着写：1 > 2 == 0 就是 (1 > 2) == 0，值为 1。

### 7.3 一层层判断

没有 else if 关键字，也没有 switch，判断梯子就是 else 里再套一个 if：

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

打印 1 2 Fizz 4 Buzz Fizz 7 8 Fizz Buzz 11 Fizz 13 14 FizzBuzz。上面这些嵌套分支的花括号不能省，因为 else 匹配的是最近的、还没配对的 if。

## 8. 名字

### 8.1 全局变量只能用字面量初始化

全局变量在所有函数之外，而表达式的初始化要写进某个函数的指令流才能执行，所以顶层只接受字面量：

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

打印 10 3，再打印 5 10。在顶层写 int h = g; 会报 全局变量无法初始化。

### 8.2 遮蔽取最近的一层

作用域是嵌套的：文件是一层，每个函数体和每一对花括号各加一层。内层声明的同名变量盖住外层，出了块又恢复外层——上面那个程序里，声明过自己的 base 的块结束后，全局的 base 仍然是 10。同级的两个块可以各自用同一个名字。只有同一层重复定义才报 重复定义变量。

## 9. 接下来读什么

- [grammar.md](grammar.md) —— 完整文法，外加被拒绝的写法一览和已知缺口清单。
- [compiler-design.md](compiler-design.md) —— 一个程序怎样走到汇编，各后端各自保证了什么。
- test/e2e —— 25 个和本文写法同形的程序，每个都带着期望输出，关心退出码的还带着期望退出码；两个 runner 自己再生成一大块栈帧的用例。
