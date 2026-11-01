#!/bin/bash
# 端到端回归：构建编译器，把每个用例编译成可执行文件运行，逐字节比对 stdout 与退出码。
# 用例文件只有 .sl 源码和期望输出，所有产物（编译器、可执行文件、日志）都写在临时目录，
# 仓库里不留任何中间结果，也不依赖任何本机绝对路径。
set -u
# CI 的默认 shell 带 -e，而用例程序有意以非零码退出，套件的成败由下面的计数决定
set +e

cd "$(dirname "$0")" || exit 1
CASES=$PWD
REPO=$(cd ../.. && pwd)

# 工作目录：Windows 上每套 Unix 环境各有一份 /tmp 与 $HOME，msys2 与 Git 对同一个
# 路径的映射并不一致，外部命令建出来的目录这个 shell 反而看不见，写文件就成了
# No such file or directory。改问系统要临时目录，再换成 /c/... 形式：两边对盘符的映射
# 一致，msys2 也会把它转回 Windows 路径交给编译器。
work_base() {
    case "$(uname -s)" in
        *NT*) ;;
        *) printf '%s\n' "${TMPDIR:-/tmp}"; return 0 ;;
    esac
    local win
    # MSYS2 会把 /c 当成路径改写成 C:\，所以这条命令关掉参数转换
    win=$(MSYS2_ARG_CONV_EXCL='*' cmd.exe /c echo %TEMP% 2>/dev/null | tr -d '\r')
    case "$win" in
        [A-Za-z]:\\*)
            printf '/%s%s\n' \
                "$(printf '%s' "$win" | cut -c1 | tr 'A-Z' 'a-z')" \
                "$(printf '%s' "$win" | cut -c3- | tr '\\' '/')"
            return 0
            ;;
    esac
    # 拿不到系统临时目录时退回仓库同级，盘符路径同样两边一致
    case "$REPO" in
        /[a-zA-Z]/*) printf '%s/shaolang-e2e\n' "$(dirname "$REPO")" ;;
        *) printf '%s\n' "${TMPDIR:-/tmp}" ;;
    esac
}

WORK="$(work_base)/shaolang-e2e-$$"
mkdir -p "$WORK" 2>/dev/null
# 用 shell 自己的重定向验证：外部命令看到的目录未必是这个 shell 看到的那个
if ! ( : > "$WORK/.probe" ) 2>/dev/null; then
    echo "FAIL 无法创建工作目录：$WORK"
    exit 1
fi
rm -f "$WORK/.probe"
trap 'rm -rf "$WORK"' EXIT

CXX=${CXX:-c++}
CC=${CC:-cc}

# 卡住命令的等待上限，没有 timeout（比如 macOS 本机）就不加保护
TIMEOUT_BIN=$(command -v timeout || true)
GUARD=""
[ -n "$TIMEOUT_BIN" ] && GUARD="$TIMEOUT_BIN 60"

# 行尾宏只影响词法分析的行号统计，与 CMakeLists 的取值保持一致
case "$(uname -s)" in
    Darwin | Linux) NEWLINE=LF ;;
    *) NEWLINE=CRLF ;;
esac

# Windows 上编译器会给产物补 .exe 后缀，用例要按同样的名字找回可执行文件
EXE_SUFFIX=
case "$(uname -s)" in
    *NT*) EXE_SUFFIX=.exe ;;
esac

# CI 里已经用 CMake 并行构建过，通过 SHAOLANG 传进来，避免每次串行重编全部源码
S=${SHAOLANG:-}
if [ -z "$S" ]; then
    echo "构建编译器：$CXX -std=c++11 -D$NEWLINE"
    if ! "$CXX" -std=c++11 -D$NEWLINE -I "$REPO/src" \
        "$REPO"/src/*.cpp "$REPO"/src/*/*.cpp -o "$WORK/shaolang" > "$WORK/build.log" 2>&1; then
        echo "FAIL 编译器构建失败"
        tail -20 "$WORK/build.log"
        exit 1
    fi
    S=$WORK/shaolang
    echo "编译器构建耗时 ${SECONDS}s"
else
    echo "使用已有编译器：$S"
fi

# 深栈帧用例不入库：局部变量数量决定帧大小，用于验证超出 x29 立即数寻址范围的访存，
# 期望值 N*(N+1)/2 由 shell 算术独立算出，不来自编译器输出
gen_deep() {
    local n=$1
    local name="deepFrame$n"
    {
        echo '-[int] main(){'
        for ((i = 1; i <= n; i++)); do echo "  int v$i = $i;"; done
        printf '  output 0'
        for ((i = 1; i <= n; i++)); do printf ' + v%s' "$i"; done
        printf ';\n  output "\\n";\n  return 0;\n}\n'
    } > "$WORK/$name.sl"
    echo $((n * (n + 1) / 2)) > "$WORK/$name.expected"
}

names=()
srcs=()
expects=()
add_case() {
    names+=("$1")
    srcs+=("$2")
    expects+=("$3")
}

for src in "$CASES"/*.sl; do
    name=$(basename "$src" .sl)
    add_case "$name" "$src" "$CASES/$name.expected"
done
gen_deep 300
add_case deepFrame300 "$WORK/deepFrame300.sl" "$WORK/deepFrame300.expected"
gen_deep 600
add_case deepFrame600 "$WORK/deepFrame600.sl" "$WORK/deepFrame600.expected"

pass=0
fail=0

check() {
    local name=$1 src=$2 expected=$3
    local exe=$WORK/$name$EXE_SUFFIX
    rm -f "$exe"
    # GUARD 为空时要展开成零个词，所以不能加引号
    $GUARD "$S" "$src" -o "$exe" > "$WORK/$name.log" 2>&1
    local code=$?
    if [ $code -eq 124 ]; then
        echo "FAIL $name : 编译超过 60s"
        tail -3 "$WORK/$name.log"
        fail=$((fail + 1))
        return
    fi
    if [ $code -ne 0 ]; then
        echo "FAIL $name : 编译命令返回 $code"
        tail -3 "$WORK/$name.log"
        fail=$((fail + 1))
        return
    fi
    if [ ! -x "$exe" ]; then
        echo "FAIL $name : 没有产出可执行文件"
        tail -3 "$WORK/$name.log"
        fail=$((fail + 1))
        return
    fi
    local input=/dev/null
    [ -f "$CASES/$name.in" ] && input=$CASES/$name.in
    $GUARD "$exe" < "$input" > "$WORK/$name.out" 2>&1
    code=$?
    if [ $code -eq 124 ]; then
        echo "FAIL $name : 运行超过 60s"
        fail=$((fail + 1))
        return
    fi
    if ! diff -u "$expected" "$WORK/$name.out" > "$WORK/$name.diff"; then
        echo "FAIL $name : 输出不符"
        cat "$WORK/$name.diff"
        fail=$((fail + 1))
        return
    fi
    if [ -f "$CASES/$name.exit" ] && [ "$(cat "$CASES/$name.exit")" != "$code" ]; then
        echo "FAIL $name : 退出码 ${code}，期望 $(cat "$CASES/$name.exit")"
        fail=$((fail + 1))
        return
    fi
    echo "ok   $name"
    pass=$((pass + 1))
}

echo
echo "== 本机编译并运行 == (第 ${SECONDS}s)"
for i in "${!names[@]}"; do
    check "${names[$i]}" "${srcs[$i]}" "${expects[$i]}"
done

# 交叉产物只验证到装配层：能生成目标平台汇编并被本机工具链接受即可。
# 本机没有对应架构的汇编器时整轮跳过，不作为失败；probe 只是探测工具链，
# 用目标架构最基础的指令，避免把工具链缺失误报成后端缺陷。
cross_round() {
    local label=$1 triple=$2 probe=$3
    echo
    echo "== $label 交叉装配 == (第 ${SECONDS}s)"
    printf '%s\n' "$probe" > "$WORK/probe.s"
    if ! "$CC" --target="$triple" -c "$WORK/probe.s" -o "$WORK/probe.o" > "$WORK/probe.log" 2>&1; then
        echo "跳过：本机工具链不支持 $triple"
        return
    fi
    local round_pass=0
    local round_fail=0
    for i in "${!names[@]}"; do
        local name=${names[$i]}
        if ! $GUARD "$S" "${srcs[$i]}" -S --target="$label" -o "$WORK/$name.s" > "$WORK/$name.log" 2>&1
        then
            echo "FAIL $name : 生成 $label 汇编失败"; tail -3 "$WORK/$name.log"
            round_fail=$((round_fail + 1)); continue
        fi
        if ! $GUARD "$CC" --target="$triple" -c "$WORK/$name.s" -o "$WORK/$name.o" > "$WORK/$name.asm.log" 2>&1
        then
            echo "FAIL $name : 装配 $label 汇编失败"; tail -5 "$WORK/$name.asm.log"
            round_fail=$((round_fail + 1)); continue
        fi
        round_pass=$((round_pass + 1))
    done
    echo "装配通过 ${round_pass}，失败 ${round_fail}"
    fail=$((fail + round_fail))
}

ARM64_PROBE=$'\t.text\n\tadd x0, x0, #1\n\tret'
X86_PROBE=$'\t.text\n\taddl %ecx, %eax\n\tret'
X64_PROBE=$'\t.text\n\taddq %rcx, %rax\n\tret'

cross_round linux-arm64 aarch64-linux-gnu "$ARM64_PROBE"
cross_round linux-x86 i686-unknown-linux-gnu "$X86_PROBE"
cross_round linux-x64 x86_64-unknown-linux-gnu "$X64_PROBE"
cross_round macos-x86 i386-apple-macosx10.15 "$X86_PROBE"

echo
echo "通过 ${pass}，失败 ${fail}，共耗时 ${SECONDS}s"
[ $fail -eq 0 ] || exit 1
