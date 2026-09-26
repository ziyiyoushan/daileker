#!/bin/bash
KEY_UP=$'\033OA';   KEY_DOWN=$'\033OB'
KEY_LEFT=$'\033OD'; KEY_RIGHT=$'\033OC'
KEY_DEL=$'\033[3~'
KEY_BS=$'\177'
KEY_ENTER=$'\r'
KEY_CTRL_S=$'\023'
KEY_CTRL_Q=$'\021'

for tool in gcc script timeout seq; do
    command -v "$tool" > /dev/null || { echo "缺少工具：$tool，请先安装"; exit 1; }
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC="$ROOT/src/dedit.c"

TMP="$(mktemp -d /tmp/dedit_tests.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

export TERM=xterm-256color

echo "开始运行 dedit 自动化测试（13 个用例）"
echo "编译：$SRC"
if ! gcc -Wall -Wextra -g "$SRC" -o "$TMP/dedit" -lncursesw 2> "$TMP/gcc_err.txt"; then
    echo "！！编译失败，先修复代码再跑测试："
    cat "$TMP/gcc_err.txt"
    exit 1
fi
BIN="$TMP/dedit"

PASS=0; FAIL=0

check() {
    if [ "$2" = "$3" ]; then
        echo "PASS: $1"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $1"
        echo "    期望: $(printf '%q' "$2")"
        echo "    实际: $(printf '%q' "$3")"
        FAIL=$((FAIL + 1))
    fi
}

check_rc() {
    if [ "$2" -eq "$3" ]; then
        echo "PASS: $1（退出码 $3）"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $1（退出码实际 $2，期望 $3）"
        FAIL=$((FAIL + 1))
    fi
}

check_screen() {
    if grep -qF "$2" "$TMP/screen.txt"; then
        echo "PASS: $1（屏幕出现 \"$2\"）"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $1（屏幕输出里没找到 \"$2\"）"
        FAIL=$((FAIL + 1))
    fi
}
run_editor() {
    local f="$1" keys="$2"
    ( sleep 0.5; printf '%s' "$keys"; sleep 0.5 ) \
        | timeout 15 script -qec "$BIN $f" /dev/null > "$TMP/screen.txt" 2>&1
}

printf 'hello\nworld\n' > "$TMP/tc01.txt"
run_editor "$TMP/tc01.txt" "$KEY_CTRL_Q"
check_rc "TC01 打开文件直接退出" $? 0
check "TC01 文件内容没有被改动" $'hello\nworld' "$(cat "$TMP/tc01.txt")"

printf 'abc\ndef\n' > "$TMP/tc02.txt"
run_editor "$TMP/tc02.txt" "${KEY_RIGHT}${KEY_RIGHT}X${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC02 行中插入字符并保存" $'abXc\ndef' "$(cat "$TMP/tc02.txt")"
check_screen "TC02 状态栏显示 Modified" "Modified"
check_screen "TC02 保存后提示 Saved." "Saved."
printf 'abc\n' > "$TMP/tc03.txt"
run_editor "$TMP/tc03.txt" "${KEY_RIGHT}${KEY_RIGHT}${KEY_BS}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC03 行内 Backspace 删除" 'ac' "$(cat "$TMP/tc03.txt")"
printf 'abc\n' > "$TMP/tc04.txt"
run_editor "$TMP/tc04.txt" "${KEY_RIGHT}${KEY_DEL}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC04 行内 Del 删除" 'ac' "$(cat "$TMP/tc04.txt")"

printf 'hello\n' > "$TMP/tc05.txt"
run_editor "$TMP/tc05.txt" "${KEY_RIGHT}${KEY_RIGHT}${KEY_ENTER}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC05 Enter 在行中拆行" $'he\nllo' "$(cat "$TMP/tc05.txt")"

printf 'aa\nbb\n' > "$TMP/tc06.txt"
run_editor "$TMP/tc06.txt" "${KEY_DOWN}${KEY_BS}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC06 行首 Backspace 合并两行" 'aabb' "$(cat "$TMP/tc06.txt")"

printf 'aa\nbb\n' > "$TMP/tc07.txt"
run_editor "$TMP/tc07.txt" "${KEY_RIGHT}${KEY_RIGHT}${KEY_DEL}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC07 行尾 Del 合并两行" 'aabb' "$(cat "$TMP/tc07.txt")"

seq -f 'L%g' 1 120 > "$TMP/tc08.txt"
keys=""
for _ in $(seq 80); do keys="${keys}${KEY_DOWN}"; done
keys="${keys}Z${KEY_CTRL_S}${KEY_CTRL_Q}"
run_editor "$TMP/tc08.txt" "$keys"
check "TC08 滚动后编辑的是第 81 行" 'ZL81' "$(sed -n '81p' "$TMP/tc08.txt")"
check_screen "TC08 屏幕上能看到 L81" "L81"
if grep -qF 'L82' "$TMP/screen.txt"; then
    echo "FAIL: TC08 屏幕不应越界显示 L82"
    FAIL=$((FAIL + 1))
else
    echo "PASS: TC08 屏幕未越界显示 L82"
    PASS=$((PASS + 1))
fi

printf 'keep\n' > "$TMP/tc09.txt"
run_editor "$TMP/tc09.txt" "A${KEY_CTRL_Q}${KEY_CTRL_Q}"
check_rc "TC09 第二次 Ctrl-Q 强制退出" $? 0
check "TC09 强制退出不保存（文件原样）" 'keep' "$(cat "$TMP/tc09.txt")"
check_screen "TC09 显示了未保存警告" "Unsaved"

: > "$TMP/tc10.txt"
run_editor "$TMP/tc10.txt" "hello${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC10 空文件中输入并保存" 'hello' "$(cat "$TMP/tc10.txt")"

run_editor "$TMP/nosuch.txt" ""
check_rc "TC11 打开不存在的文件应报错退出" $? 1

( sleep 0.3 ) | timeout 15 script -qec "$BIN" /dev/null > "$TMP/screen.txt" 2>&1
check_rc "TC12 不带参数启动应提示用法并退出" $? 1
check_screen "TC12 打印了用法提示" "usage"

printf 'orig\n' > "$TMP/tc13.txt"
chmod 444 "$TMP/tc13.txt"
run_editor "$TMP/tc13.txt" "X${KEY_CTRL_S}${KEY_CTRL_Q}${KEY_CTRL_Q}"
check_rc "TC13 保存失败后仍可正常退出" $? 0
check_screen "TC13 显示了保存失败提示" "Save failed"
check "TC13 文件没有被改动" 'orig' "$(cat "$TMP/tc13.txt")"
chmod 644 "$TMP/tc13.txt"

echo "=============================="
echo "共 $((PASS + FAIL)) 项检查：PASS=$PASS FAIL=$FAIL"
if [ "$FAIL" -eq 0 ]; then
    echo "全部通过"
    exit 0
else
    echo "存在失败项，请查看上面的 FAIL 明细"
    exit 1
fi
