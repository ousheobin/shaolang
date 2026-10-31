#!/bin/bash
# 端到端回归：构建编译器，把每个用例编译成可执行文件运行，逐字节比对 stdout 与退出码。
# 用例文件只有 .sl 源码和期望输出，所有产物（编译器、可执行文件、日志）都写在临时目录，
# 仓库里不留任何中间结果，也不依赖任何本机绝对路径。
set -u

cd "$(dirname "$0")" || exit 1
CASES=$PWD
REPO=$(cd ../.. && pwd)
WORK=$(mktemp -d) || exit 1
trap 'rm -rf "$WORK"' EXIT

CXX=${CXX:-c++}
CC=${CC:-cc}

# 行尾宏只影响词法分析的行号统计，与 CMakeLists 的取值保持一致
case "$(uname -s)" in
    Darwin | Linux) NEWLINE=LF ;;
    *) NEWLINE=CRLF ;;
esac

echo "构建编译器：$CXX -std=c++11 -D$NEWLINE"
if ! "$CXX" -std=c++11 -D$NEWLINE -I "$REPO/src" \
    "$REPO"/src/*.cpp "$REPO"/src/*/*.cpp -o "$WORK/shaolang" > "$WORK/build.log" 2>&1; then
    echo "FAIL 编译器构建失败"
    tail -20 "$WORK/build.log"
    exit 1
fi
S=$WORK/shaolang

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
    local exe=$WORK/$name
    rm -f "$exe"
    if ! "$S" "$src" -o "$exe" > "$WORK/$name.log" 2>&1; then
        echo "FAIL $name : 编译命令返回失败"
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
    "$exe" < "$input" > "$WORK/$name.out" 2>&1
    local code=$?
    if ! diff -u "$expected" "$WORK/$name.out" > "$WORK/$name.diff"; then
        echo "FAIL $name : 输出不符"
        cat "$WORK/$name.diff"
        fail=$((fail + 1))
        return
    fi
    if [ -f "$CASES/$name.exit" ] && [ "$(cat "$CASES/$name.exit")" != "$code" ]; then
        echo "FAIL $name : 退出码 $code，期望 $(cat "$CASES/$name.exit")"
        fail=$((fail + 1))
        return
    fi
    echo "ok   $name"
    pass=$((pass + 1))
}

echo
echo "== 本机编译并运行 =="
for i in "${!names[@]}"; do
    check "${names[$i]}" "${srcs[$i]}" "${expects[$i]}"
done

# 交叉产物只验证到装配层：能生成 ELF aarch64 汇编并被工具链接受即可，
# 本机没有对应链接器或模拟器时跳过，不作为失败
echo
echo "== linux-arm64 交叉装配 =="
printf '\t.text\n\tadd x0, x0, #1\n\tret\n' > "$WORK/probe.s"
if ! "$CC" --target=aarch64-linux-gnu -c "$WORK/probe.s" -o "$WORK/probe.o" > "$WORK/probe.log" 2>&1; then
    echo "跳过：本机工具链不支持 aarch64-linux-gnu"
else
    cross_pass=0
    cross_fail=0
    for i in "${!names[@]}"; do
        name=${names[$i]}
        if ! "$S" "${srcs[$i]}" -S --target=linux-arm64 -o "$WORK/$name.linux.s" > "$WORK/$name.linux.log" 2>&1
        then
            echo "FAIL $name : 生成 linux 汇编失败"; tail -3 "$WORK/$name.linux.log"
            cross_fail=$((cross_fail + 1)); continue
        fi
        if ! "$CC" --target=aarch64-linux-gnu -c "$WORK/$name.linux.s" -o "$WORK/$name.linux.o" > "$WORK/$name.linux.asm.log" 2>&1
        then
            echo "FAIL $name : 装配 linux 汇编失败"; tail -5 "$WORK/$name.linux.asm.log"
            cross_fail=$((cross_fail + 1)); continue
        fi
        cross_pass=$((cross_pass + 1))
    done
    echo "装配通过 $cross_pass，失败 $cross_fail"
    fail=$((fail + cross_fail))
fi

echo
echo "通过 $pass，失败 $fail"
[ $fail -eq 0 ] || exit 1
