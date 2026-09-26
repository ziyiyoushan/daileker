#!/bin/bash
# ============================================================
# dedit 自动化测试（Level 2 + Level 3）
#
# 用法：bash tests/run_tests.sh        （在任意目录下运行都可以）
#
# 原理：dedit 是全屏 ncurses 程序，必须连接一个"终端"才能运行。
#       每个用例的流程：
#         1. 在隔离的临时目录里准备输入文件
#         2. 用 script 命令给 dedit 分配一个假终端（pty）
#         3. 按时间注入按键序列，模拟真实用户操作
#         4. 断言：文件内容 / 退出码 / 屏幕上出现过的关键文字
#
# 按键记法（与题目一致）：<Esc> <Ctrl-S> <Del> <Backspace> <Enter> ...
#
# 每个用例都用独立文件、独立 pty，互不影响；
# 扩展新用例 = 准备文件 + run_editor* + check* 三行，照葫芦画瓢即可。
# ============================================================

# ---------- 按键 -> 字节序列 ----------
# 方向键固定按 xterm 系终端（下面 export TERM）的序列发，
# 这样在任何机器上跑，按键字节都一样、结果可复现。
KEY_UP=$'\033OA';   KEY_DOWN=$'\033OB'
KEY_LEFT=$'\033OD'; KEY_RIGHT=$'\033OC'
KEY_DEL=$'\033[3~'             # <Del>
KEY_BS=$'\177'                 # <Backspace>
KEY_ENTER=$'\r'                # <Enter>
KEY_CTRL_S=$'\023'             # <Ctrl-S>
KEY_CTRL_Q=$'\021'             # <Ctrl-Q>

# ---------- Level 3 新增按键 ----------
KEY_ESC=$'\033'                # <Esc>
KEY_SLEFT=$'\033[1;2D'         # Shift+←
KEY_SRIGHT=$'\033[1;2C'        # Shift+→
KEY_SUP=$'\033[1;2A'           # Shift+↑（xterm 发的是"滚动"序列 KEY_SR）
KEY_SDOWN=$'\033[1;2B'         # Shift+↓（KEY_SF）
KEY_CTRL_F=$'\006'             # <Ctrl-F> 搜索
KEY_CTRL_R=$'\022'             # <Ctrl-R> 替换
KEY_CTRL_P=$'\020'             # <Ctrl-P> 上一个候选
KEY_CTRL_C=$'\003'             # <Ctrl-C> 复制
KEY_CTRL_X=$'\030'             # <Ctrl-X> 剪切
KEY_CTRL_V=$'\026'             # <Ctrl-V> 粘贴
KEY_CTRL_Z=$'\032'             # <Ctrl-Z> 撤回
KEY_CTRL_Y=$'\031'             # <Ctrl-Y> 重做

# ---------- 环境准备 ----------
for tool in gcc script timeout seq; do
    command -v "$tool" > /dev/null || { echo "缺少工具：$tool，请先安装"; exit 1; }
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"    # tests/ 目录
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"           # 项目根目录
SRC="$ROOT/src/dedit.c"

TMP="$(mktemp -d /tmp/dedit_tests.XXXXXX)"     # 每次运行独立的临时目录
trap 'rm -rf "$TMP"' EXIT                      # 无论成败都自动清理

export TERM=xterm-256color                     # 固定终端类型，按键序列才可复现

echo "开始运行 dedit 自动化测试（Level2 共 13 个用例 + Level3 共 16 个用例）"
echo "编译：$SRC"
if ! gcc -Wall -Wextra -g "$SRC" -o "$TMP/dedit" -lncursesw 2> "$TMP/gcc_err.txt"; then
    echo "！！编译失败，先修复代码再跑测试："
    cat "$TMP/gcc_err.txt"
    exit 1
fi
BIN="$TMP/dedit"

PASS=0; FAIL=0

check() {      # check 用例名 期望 实际 —— 字符串相等
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

check_rc() {   # check_rc 用例名 实际退出码 期望退出码
    if [ "$2" -eq "$3" ]; then
        echo "PASS: $1（退出码 $3）"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $1（退出码实际 $2，期望 $3）"
        FAIL=$((FAIL + 1))
    fi
}

check_screen() {   # check_screen 用例名 关键字 —— 在伪终端录制输出里找文字
    if grep -qF "$2" "$TMP/screen.txt"; then
        echo "PASS: $1（屏幕出现 \"$2\"）"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $1（屏幕输出里没找到 \"$2\"）"
        FAIL=$((FAIL + 1))
    fi
}

# 在假终端里运行 dedit：run_editor 输入文件 "按键序列"
run_editor() {
    local f="$1" keys="$2"
    ( sleep 0.5; printf '%s' "$keys"; sleep 0.5 ) \
        | timeout 15 script -qec "$BIN $f" /dev/null > "$TMP/screen.txt" 2>&1
}

# Level 3 用：按键分块发送，块与块之间留 0.35s 间隔。
# 为什么分块：按下 <Esc> 后终端要等一小段"静默"才能确认它就是 Esc 键
#（而不是某个组合键序列的开头），真人打字天然有这个间隔，这里如实模拟。
run_editor_chunks() {
    local f="$1"; shift
    ( sleep 0.5
      for chunk in "$@"; do printf '%s' "$chunk"; sleep 0.35; done
      sleep 0.4 ) \
        | timeout 20 script -qec "$BIN $f" /dev/null > "$TMP/screen.txt" 2>&1
}

# ==================== Level 2 用例 ====================

# ---- TC01 打开文件直接退出（无修改） ----
# 步骤：<Ctrl-Q>
printf 'hello\nworld\n' > "$TMP/tc01.txt"
run_editor "$TMP/tc01.txt" "$KEY_CTRL_Q"
check_rc "TC01 打开文件直接退出" $? 0
check "TC01 文件内容没有被改动" $'hello\nworld' "$(cat "$TMP/tc01.txt")"

# ---- TC02 行中插入字符并保存 ----
# 步骤：<Right><Right>X<Ctrl-S><Ctrl-Q>
printf 'abc\ndef\n' > "$TMP/tc02.txt"
run_editor "$TMP/tc02.txt" "${KEY_RIGHT}${KEY_RIGHT}X${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC02 行中插入字符并保存" $'abXc\ndef' "$(cat "$TMP/tc02.txt")"
check_screen "TC02 状态栏显示 Modified" "Modified"
check_screen "TC02 保存后提示 Saved." "Saved."

# ---- TC03 行内 Backspace 删除 ----
# 步骤：<Right><Right><Backspace><Ctrl-S><Ctrl-Q>
printf 'abc\n' > "$TMP/tc03.txt"
run_editor "$TMP/tc03.txt" "${KEY_RIGHT}${KEY_RIGHT}${KEY_BS}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC03 行内 Backspace 删除" 'ac' "$(cat "$TMP/tc03.txt")"

# ---- TC04 行内 Del 删除 ----
# 步骤：<Right><Del><Ctrl-S><Ctrl-Q>
printf 'abc\n' > "$TMP/tc04.txt"
run_editor "$TMP/tc04.txt" "${KEY_RIGHT}${KEY_DEL}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC04 行内 Del 删除" 'ac' "$(cat "$TMP/tc04.txt")"

# ---- TC05 Enter 在行中拆行 ----
# 步骤：<Right><Right><Enter><Ctrl-S><Ctrl-Q>
printf 'hello\n' > "$TMP/tc05.txt"
run_editor "$TMP/tc05.txt" "${KEY_RIGHT}${KEY_RIGHT}${KEY_ENTER}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC05 Enter 在行中拆行" $'he\nllo' "$(cat "$TMP/tc05.txt")"

# ---- TC06 行首 Backspace 合并两行 ----
# 步骤：<Down><Backspace><Ctrl-S><Ctrl-Q>
printf 'aa\nbb\n' > "$TMP/tc06.txt"
run_editor "$TMP/tc06.txt" "${KEY_DOWN}${KEY_BS}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC06 行首 Backspace 合并两行" 'aabb' "$(cat "$TMP/tc06.txt")"

# ---- TC07 行尾 Del 合并两行 ----
# 步骤：<Right><Right><Del><Ctrl-S><Ctrl-Q>
printf 'aa\nbb\n' > "$TMP/tc07.txt"
run_editor "$TMP/tc07.txt" "${KEY_RIGHT}${KEY_RIGHT}${KEY_DEL}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC07 行尾 Del 合并两行" 'aabb' "$(cat "$TMP/tc07.txt")"

# ---- TC08 长文件滚动后编辑 ----
# 步骤：<Down> 按 80 次，输入 Z，<Ctrl-S><Ctrl-Q>
# 120 行文本，向下 80 次后光标在第 81 行，输入后该行变为 "ZL81"
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

# ---- TC09 未保存修改的退出保护 ----
# 步骤：输入 A，然后 <Ctrl-Q><Ctrl-Q>（第一次警告，第二次强制退出）
printf 'keep\n' > "$TMP/tc09.txt"
run_editor "$TMP/tc09.txt" "A${KEY_CTRL_Q}${KEY_CTRL_Q}"
check_rc "TC09 第二次 Ctrl-Q 强制退出" $? 0
check "TC09 强制退出不保存（文件原样）" 'keep' "$(cat "$TMP/tc09.txt")"
check_screen "TC09 显示了未保存警告" "Unsaved"

# ---- TC10 空文件中输入并保存 ----
# 步骤：输入 hello，<Ctrl-S><Ctrl-Q>
: > "$TMP/tc10.txt"
run_editor "$TMP/tc10.txt" "hello${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC10 空文件中输入并保存" 'hello' "$(cat "$TMP/tc10.txt")"

# ---- TC11 打开不存在的文件 ----
# 步骤：不发任何按键
run_editor "$TMP/nosuch.txt" ""
check_rc "TC11 打开不存在的文件应报错退出" $? 1

# ---- TC12 不带参数启动 ----
# 步骤：不发任何按键
( sleep 0.3 ) | timeout 15 script -qec "$BIN" /dev/null > "$TMP/screen.txt" 2>&1
check_rc "TC12 不带参数启动应提示用法并退出" $? 1
check_screen "TC12 打印了用法提示" "usage"

# ---- TC13 只读文件保存失败提示 ----
# 步骤：输入 X，<Ctrl-S>（应失败并提示），<Ctrl-Q><Ctrl-Q>
printf 'orig\n' > "$TMP/tc13.txt"
chmod 444 "$TMP/tc13.txt"
run_editor "$TMP/tc13.txt" "X${KEY_CTRL_S}${KEY_CTRL_Q}${KEY_CTRL_Q}"
check_rc "TC13 保存失败后仍可正常退出" $? 0
check_screen "TC13 显示了保存失败提示" "Save failed"
check "TC13 文件没有被改动" 'orig' "$(cat "$TMP/tc13.txt")"
chmod 644 "$TMP/tc13.txt"

# ==================== Level 3 用例：搜索 ====================

# ---- TC14 搜索：找到第一个匹配并跳过去 ----
# 步骤：<Ctrl-F>world<Enter> → 光标跳到 "world"；
#       <Esc> 退出搜索后输入 "!" → 应插在匹配词前面
printf 'hello world\nsay hello\n' > "$TMP/tc14.txt"
run_editor_chunks "$TMP/tc14.txt" \
    "${KEY_CTRL_F}world${KEY_ENTER}" "${KEY_ESC}" "!${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC14 搜索后光标跳到匹配处" $'hello !world\nsay hello' "$(cat "$TMP/tc14.txt")"
# 注：不要断言 "Search: world" 这样的整串——ncurses 是增量绘制，
# 只在屏幕上重发变化的部分，残留字节流里未必有连续整串。
check_screen "TC14 输入提示行出现搜索提示" "Search:"

# ---- TC15 搜索：Enter 下一个 / Ctrl-P 上一个 / 不回绕 ----
# 文件 "ab ab ab"：三个匹配在列 1、4、7
# 步骤：<Ctrl-F>ab<Enter>×4 → 三个候选走完，再按提示 No more matches；
#       <Ctrl-P>×3 → 一路回到第一个，再按提示 No previous match；
#       <Esc> 后输入 X：验证光标停在第一个匹配前
printf 'ab ab ab\n' > "$TMP/tc15.txt"
run_editor_chunks "$TMP/tc15.txt" \
    "${KEY_CTRL_F}ab${KEY_ENTER}${KEY_ENTER}${KEY_ENTER}${KEY_ENTER}${KEY_CTRL_P}${KEY_CTRL_P}${KEY_CTRL_P}" \
    "${KEY_ESC}" "X${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC15 多次切换候选后停在第一个匹配" 'Xab ab ab' "$(cat "$TMP/tc15.txt")"
check_screen "TC15 到尾后提示 No more matches" "No more matches"
check_screen "TC15 到首后提示 No previous match" "No previous match"
check_screen "TC15 状态栏显示搜索状态" "[SEARCH]"

# ---- TC16 搜索：查无结果 ----
# 步骤：<Ctrl-F>zzz<Enter>（提示 No match）<Esc> Y <Ctrl-S><Ctrl-Q>
printf 'abc\n' > "$TMP/tc16.txt"
run_editor_chunks "$TMP/tc16.txt" \
    "${KEY_CTRL_F}zzz${KEY_ENTER}" "${KEY_ESC}" "Y${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC16 无匹配退出搜索后编辑正常" 'Yabc' "$(cat "$TMP/tc16.txt")"
check_screen "TC16 提示 No match" "No match"

# ---- TC17 搜索：长文件（120 行）跨屏跳转 ----
# 搜索 "L100"（第 100 行，初始不在屏幕内）：Enter 后应跳到第 100 行；
# Esc 后输入 Z → 第 100 行变成 "ZL100"
seq -f 'L%g' 1 120 > "$TMP/tc28.txt"
run_editor_chunks "$TMP/tc28.txt" \
    "${KEY_CTRL_F}L100${KEY_ENTER}" "${KEY_ESC}" "Z${KEY_CTRL_S}${KEY_CTRL_Q}"
rc=$?
check "TC17 搜索跳到第 100 行并编辑" 'ZL100' "$(sed -n '100p' "$TMP/tc28.txt")"
check_screen "TC17 输入提示行出现搜索提示" "Search:"
check_rc "TC17 全程操作后正常退出" "$rc" 0

# ==================== Level 3 用例：替换 ====================

# ---- TC18 替换：Y 逐个替换直到结束 ----
# 步骤：<Ctrl-R>foo<Enter>ww<Enter> → 进入逐个替换；
#       <Y><Y><Y> → 三个 foo 全换成 ww
printf 'foo bar foo baz foo\n' > "$TMP/tc17.txt"
run_editor_chunks "$TMP/tc17.txt" \
    "${KEY_CTRL_R}foo${KEY_ENTER}ww${KEY_ENTER}" "YYY" "${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC18 Y 逐个全部替换" 'ww bar ww baz ww' "$(cat "$TMP/tc17.txt")"
check_screen "TC18 替换结束提示" "Replaced. No more matches."

# ---- TC19 替换：N 跳过、Ctrl-P 回退、Y 替换 ----
# 步骤：<Ctrl-R>ab<Enter>XY<Enter> → 第 1 个候选；
#       N → 跳到第 2 个；<Ctrl-P> → 回到第 1 个；Y → 替换第 1 个；
#       <Esc> 退出。结果：第一个变成 XY，中间被跳过，最后没处理
printf 'ab ab ab\n' > "$TMP/tc18.txt"
run_editor_chunks "$TMP/tc18.txt" \
    "${KEY_CTRL_R}ab${KEY_ENTER}XY${KEY_ENTER}" "n${KEY_CTRL_P}Y" "${KEY_ESC}" "${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC19 跳过+回退+替换" 'XY ab ab' "$(cat "$TMP/tc18.txt")"
check_screen "TC19 逐个替换提示行" "Y=replace"

# ---- TC20 替换：A 全部替换 ----
# 步骤：<Ctrl-R>x<Enter>y<Enter>A → 四个 x 全换成 y
printf 'x x x x\n' > "$TMP/tc19.txt"
run_editor_chunks "$TMP/tc19.txt" \
    "${KEY_CTRL_R}x${KEY_ENTER}y${KEY_ENTER}A" "${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC20 A 全部替换" 'y y y y' "$(cat "$TMP/tc19.txt")"
check_screen "TC20 提示替换个数" "Replaced 4 occurrence(s)."

# ---- TC21 替换：替换词为空 = 删除 ----
# 步骤：<Ctrl-R>XX<Enter><Enter>（替换词留空）→ 逐个替换；Y Y → XX 全被删掉
printf 'aXXbXXc\n' > "$TMP/tc20.txt"
run_editor_chunks "$TMP/tc20.txt" \
    "${KEY_CTRL_R}XX${KEY_ENTER}${KEY_ENTER}" "YY" "${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC21 空替换词删除匹配" 'abc' "$(cat "$TMP/tc20.txt")"
check_screen "TC21 替换完成提示" "Replaced. No more matches."

# ---- TC22 替换：无匹配时提示 No match ----
printf 'abc\n' > "$TMP/tc29.txt"
run_editor_chunks "$TMP/tc29.txt" \
    "${KEY_CTRL_R}zz${KEY_ENTER}q${KEY_ENTER}" "${KEY_ESC}" "${KEY_CTRL_Q}"
check "TC22 无匹配时文件不变" 'abc' "$(cat "$TMP/tc29.txt")"
check_screen "TC22 提示 No match" "No match"

# ==================== Level 3 用例：剪贴板 ====================

# ---- TC23 剪贴板：同行选择、复制、粘贴 ----
# 步骤：Shift+→×3 选中 "hel"，<Ctrl-C> 复制；
#       <→>×2 到行尾，<Ctrl-V> 粘贴 → "hellohel"
printf 'hello\n' > "$TMP/tc21.txt"
run_editor_chunks "$TMP/tc21.txt" \
    "${KEY_SRIGHT}${KEY_SRIGHT}${KEY_SRIGHT}${KEY_CTRL_C}${KEY_RIGHT}${KEY_RIGHT}${KEY_CTRL_V}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC23 复制后粘贴" 'hellohel' "$(cat "$TMP/tc21.txt")"
check_screen "TC23 显示复制字符数" "Copied 3 char(s)"

# ---- TC24 剪贴板：跨行选择、剪切、再粘贴回原样 ----
# Shift+→×4：从行首选到第二行行首（选区含换行，共 4 个字符 "abc\n"）；
# <Ctrl-X> 剪切 → 文件变 "def\nghi"；<Ctrl-V> 原地粘贴 → 恢复原样
printf 'abc\ndef\nghi\n' > "$TMP/tc22.txt"
run_editor_chunks "$TMP/tc22.txt" \
    "${KEY_SRIGHT}${KEY_SRIGHT}${KEY_SRIGHT}${KEY_SRIGHT}${KEY_CTRL_X}${KEY_CTRL_V}${KEY_CTRL_S}${KEY_CTRL_Q}"
rc=$?
check "TC24 剪切后粘贴内容一致" $'abc\ndef\nghi' "$(cat "$TMP/tc22.txt")"
check_screen "TC24 显示剪切字符数" "Cut 4 char(s)"
check_screen "TC24 显示粘贴提示" "Pasted"
check_rc "TC24 全程操作后正常退出" "$rc" 0

# ---- TC25 选择：Shift+↑/↓ 跨行选择并复制 ----
# 光标移到 (0,1)，Shift+↓×2 → 选中 "aa\nbbb\nc"（8 个字符）；
# <Ctrl-C> 复制；<Ctrl-V> 粘贴到光标处 → 3 行文件变 5 行
printf 'aaa\nbbb\nccc\n' > "$TMP/tc26.txt"
run_editor_chunks "$TMP/tc26.txt" \
    "${KEY_RIGHT}${KEY_SDOWN}${KEY_SDOWN}${KEY_CTRL_C}${KEY_CTRL_V}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC25 跨行选择复制粘贴" $'aaa\nbbb\ncaa\nbbb\nccc' "$(cat "$TMP/tc26.txt")"
check_screen "TC25 显示复制字符数" "Copied 8 char(s)"

# ---- TC26 选择：Esc 取消选择 / 空选区复制 ----
# Shift+→×3 选中后 <Esc> 取消；<Ctrl-C> 提示 No selection；
# 再 Shift+→ <Shift-←> 构成空选区，<Ctrl-C> 提示 Nothing selected；文件不变
printf 'hello\n' > "$TMP/tc27.txt"
run_editor_chunks "$TMP/tc27.txt" \
    "${KEY_SRIGHT}${KEY_SRIGHT}${KEY_SRIGHT}" "${KEY_ESC}" \
    "${KEY_CTRL_C}${KEY_SRIGHT}${KEY_SLEFT}${KEY_CTRL_C}${KEY_CTRL_Q}"
check "TC26 无修改时文件保持原样" 'hello' "$(cat "$TMP/tc27.txt")"
check_screen "TC26 没有选区时提示" "No selection"
check_screen "TC26 空选区提示" "Nothing selected"

# ==================== Level 3 用例：撤回 / 重做 ====================

# ---- TC27 撤回/重做：剪切可撤回、重做 ----
# 剪切 "hel" → 文件 "lo"；<Ctrl-Z> 撤回 → "hello"；再按一次 → 插入光标，
# 让状态栏先回到普通显示；<Ctrl-Y> 重做 → "lo"
# （中间加个方向键是因为：若状态栏上一帧是 "Undo"、"Redo" 只差一个字母，
#   ncurses 增量绘制只重发变化的字符，测试就抓不到完整 "Redo" 字样）
printf 'hello\n' > "$TMP/tc23.txt"
run_editor_chunks "$TMP/tc23.txt" \
    "${KEY_SRIGHT}${KEY_SRIGHT}${KEY_SRIGHT}${KEY_CTRL_X}${KEY_CTRL_Z}${KEY_RIGHT}${KEY_CTRL_Y}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC27 撤回剪切再重做" 'lo' "$(cat "$TMP/tc23.txt")"
check_screen "TC27 显示撤回提示" "Undo"
check_screen "TC27 显示重做提示" "Redo"
check_screen "TC27 显示剪切提示" "Cut 3 char(s)"

# ---- TC28 撤回：字符级事件，以及 Nothing to undo ----
# 在 "X" 前输入 "abc"（3 个事件）；<Ctrl-Z>×4：
#   前 3 次撤销 c、b、a，第 4 次提示 Nothing to undo；
# <Ctrl-Y>×2 重做 a、b → 文件应为 "abX"
printf 'X\n' > "$TMP/tc24.txt"
run_editor_chunks "$TMP/tc24.txt" \
    "abc" "${KEY_CTRL_Z}${KEY_CTRL_Z}${KEY_CTRL_Z}${KEY_CTRL_Z}" \
    "${KEY_CTRL_Y}${KEY_CTRL_Y}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC28 字符级撤回与重做" 'abX' "$(cat "$TMP/tc24.txt")"
check_screen "TC28 撤回到底提示" "Nothing to undo"
check_screen "TC28 显示重做提示" "Redo"

# ---- TC29 撤回：Y 替换逐步撤回 ----
# "aa aa"：Y×2 全换成 "b" → "b b"；
# 撤回两次回到 "aa aa"，重做一次 → "b aa"
# （撤回后按一下方向键，让 "Redo" 提示能完整重画，理由同 TC27）
printf 'aa aa\n' > "$TMP/tc25.txt"
run_editor_chunks "$TMP/tc25.txt" \
    "${KEY_CTRL_R}aa${KEY_ENTER}b${KEY_ENTER}" "YY" \
    "${KEY_CTRL_Z}${KEY_CTRL_Z}${KEY_RIGHT}${KEY_CTRL_Y}${KEY_CTRL_S}${KEY_CTRL_Q}"
check "TC29 替换可逐步撤回/重做" 'b aa' "$(cat "$TMP/tc25.txt")"
check_screen "TC29 替换结束提示" "Replaced. No more matches."
check_screen "TC29 显示撤回提示" "Undo"
check_screen "TC29 显示重做提示" "Redo"

# ==================== 汇总 ====================
echo "=============================="
echo "共 $((PASS + FAIL)) 项检查：PASS=$PASS FAIL=$FAIL"
if [ "$FAIL" -eq 0 ]; then
    echo "全部通过"
    exit 0
else
    echo "存在失败项，请查看上面的 FAIL 明细"
    exit 1
fi
