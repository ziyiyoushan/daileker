# 学习记录

## 2026/09/23

### 今日完成内容

1. 配置 WSL 开发环境
- 完成 Ubuntu 环境导入和初始化。
- 安装并检查开发所需工具：
  - gcc
  - git
  - make
  - ncurses

2. 配置 Git 环境
- 设置 Git 用户信息。
- 学习 Git 基本流程：
  - git status 查看状态
  - git add 添加文件
  - git commit 提交修改
  - git log 查看提交记录

3. 拉取项目代码
- 使用 git clone 将项目仓库拉取到 WSL 环境。
- 熟悉项目目录结构：
  - src：源代码目录
  - include：头文件目录
  - build：构建相关目录
  - docs：学习记录目录

4. 项目初始化
- 创建学习记录文档。
- 完成第一次提交，建立规范的 Git 提交历史。

### 遇到的问题

1. WSL 中无法直接访问 GitHub。
- 原因：Windows 代理配置没有同步到 WSL。
- 解决：开启 WSL mirrored 网络模式，使 WSL 可以正常访问网络。

2. Git 提交时提示无法识别用户信息。
- 原因：Git 未配置用户名和邮箱。
- 解决：配置 git user.name 和 git user.email。

### 今日收获

了解了 Linux 开发环境的基本使用流程，熟悉了从环境配置、代码拉取到版本管理的完整流程，为后续项目开发做好准备。
## 2026/09/25

### 今日完成内容

1. 完成 dedit（Level 1）终端文本查看器
- 掌握 ncurses 程序基本流程：initscr → raw / noecho / keypad → getch 主循环 → endwin。
- 用方向键移动光标（KEY_UP / KEY_DOWN / KEY_LEFT / KEY_RIGHT），光标不越出屏幕。
- 输入可打印字符时显示在光标处。
- Ctrl-Q 退出：raw 模式下 Ctrl 键原样传给 getch()，Ctrl-Q 的键码是 17。

2. 实现文件读取
- ./dedit <filename> 打开文件：用 fgets 逐行读入，去掉行尾换行符；空文件可正常打开。
- 文件不存在或参数不对时给出明确提示，并返回退出码 1。
- 设计取舍：先读文件、失败直接退出，再初始化 ncurses（此时终端还没被接管，出错处理更简单）。

3. 编译、测试与提交
- 编译命令：gcc -Wall -Wextra -g src/dedit.c -o build/dedit -lncursesw（-lncursesw 必须放在源文件后面）。
- 四个场景全部验证通过：正常文件、空文件、文件不存在、不带参数。
- 添加 .gitignore 忽略 build/ 编译产物；提交并推送：feat: level1 - dedit skeleton, cursor move, file loading。

### 遇到的问题

1. 编译报错：keypad(stdscr, TRUE) 行尾漏写分号。
- 后面还"连坐"出 rows/cols 未定义的错误。
- 解决：从第一条错误开始修，改完重新编译，后续报错自动消失。

2. 分号误打成中文全角"；"，编译器无法识别。
- 原因：中文输入法状态下输入标点，C 代码中所有符号必须是英文半角。
- 解决：改为英文分号；写代码时保持输入法在英文状态。

3. 为什么 load_file 要写在 main 前面？
- 理解：C 编译器从上往下读，函数在被调用前必须先声明。
- 若想让 main 放在前面，需要在文件顶部先写函数声明原型。

4. GitHub 推送失败（TLS 连接中断、提示不支持密码认证）。
- 解决：WSL 通过 Windows 代理访问 GitHub（127.0.0.1:21882）；改用 Personal Access Token 认证并保存凭据。

### 今日收获

完成第一个可交互的终端程序：理解了 ncurses 的屏幕刷新机制与按键处理流程，掌握了 C 语言"先声明后使用"的规则，也学会了用编译器报错定位问题——从第一条错误开始修是最高效的方法。
