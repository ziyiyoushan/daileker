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
