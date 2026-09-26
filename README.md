# Dian Editor (dedit)

一个用 C 语言 + ncurses 实现的终端文本编辑器：支持文本编辑、滚动、搜索/替换、剪贴板与撤销/重做。

## 功能

- **编辑**：插入、Backspace、Del、回车拆行；行首退格、行尾删除自动合并相邻行
- **浏览**：方向键移动光标，光标移出屏幕时显示区域自动滚动；调整终端窗口大小后正常显示
- **状态栏**：屏幕最后一行显示 `[EDIT] 文件名 | Modified/Saved | 行:列`；保存失败等提示也显示在这里
- **保存**：Ctrl-S 存盘；有未保存修改时按 Ctrl-Q 会提示警告，再按一次强制退出
- **搜索**（Ctrl-F）：输入关键词回车定位并高亮显示；回车找下一个候选、Ctrl-P 找上一个候选；候选不回绕；Esc 退出
- **替换**（Ctrl-R）：输入搜索词 → 输入替换词 → 逐个候选确认：Y 替换并前进、N 跳过、A 全部替换、Ctrl-P 上一个、Esc 或 Ctrl-C 退出；替换词留空即删除
- **剪贴板**：Shift + 方向键选择文本，Ctrl-C 复制 / Ctrl-X 剪切 / Ctrl-V 粘贴（支持跨行）
- **撤销 / 重做**：Ctrl-Z / Ctrl-Y，最多 100 步；每个字符、退格、回车、剪切、粘贴、替换各记为一步事件

## 编译

开发环境为 WSL2 下的 Ubuntu。依赖 `gcc` 与 ncurses 开发库（Ubuntu / Debian：`sudo apt install libncurses-dev`）。

```bash
mkdir -p build
gcc -Wall -Wextra -g src/dedit.c -o build/dedit -lncursesw
```

## 运行

```bash
./build/dedit <filename>
```

- 空文件可以正常打开；
- 文件不存在或参数个数不对时给出提示，并以退出码 1 结束（此时尚未接管终端）。

## 快捷键


| 按键                       | 功能                                                         |
| ------------------------ | ---------------------------------------------------------- |
| ↑ / ↓ / ← / →            | 移动光标（不会超出文本范围；移出可视区时自动滚动）                                  |
| Shift + 方向键              | 扩展 / 缩小选区（Esc 取消选区）                                        |
| 可打印字符                    | 在光标处插入字符                                                   |
| Backspace / Del          | 删除光标前 / 光标后的字符（在行首 / 行尾则与相邻行合并）                            |
| Enter                    | 换行（在光标处拆行）                                                 |
| Ctrl-S                   | 保存                                                         |
| Ctrl-Q                   | 退出（有未保存修改时需按两次）                                            |
| Ctrl-F                   | 搜索（Enter 下一个候选，Ctrl-P 上一个候选，Esc 退出）                        |
| Ctrl-R                   | 替换（输入搜索词、替换词后：Y 替换、N 跳过、A 全部替换、Ctrl-P 上一个、Esc / Ctrl-C 退出） |
| Ctrl-C / Ctrl-X / Ctrl-V | 复制 / 剪切 / 粘贴（基于 Shift + 方向键 的选区）                           |
| Ctrl-Z / Ctrl-Y          | 撤销 / 重做                                                    |


## 测试

项目自带一套自动化测试（**29 个用例、66 项检查**），覆盖编辑、保存、错误路径、搜索、替换、剪贴板与撤销/重做的完整交互链：

```bash
bash tests/run_tests.sh
```

在 Linux / WSL 下运行（依赖 `script` 命令分配伪终端，一般系统自带）；全程无需人工干预，逐条输出 PASS/FAIL 明细与汇总。测试设计与数据集说明见 [tests/README.md](tests/README.md)。

## 实现摘要

- 文本以 `char lines[1000][256]` 的行数组存储（有意做的简化：换取直观可靠，代价是固定上限）；
- 光标记录「文本坐标」，显示层单独维护窗口与滚动偏移，两者分离；
- 撤销 / 重做用「整篇快照环」实现（最多 100 步），优先保证正确性。

更多设计取舍与开发过程记录在 [docs/learning-log.md](docs/learning-log.md)。

## 目录结构

```
src/dedit.c            编辑器全部源码（单文件）
tests/run_tests.sh     自动化测试
tests/README.md        测试设计说明与用例表
docs/learning-log.md   学习记录
exercises/hello.c      Level 0：Hello World
```

