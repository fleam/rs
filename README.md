# rs —— 自动更新宿主机 IP 并智能启动项目

在 WSL 环境中开发时，宿主机运行的 MySQL 数据库 IP 地址经常变更。本工具可以：

- 自动获取 WSL 宿主机当前的 IP；
- 如果是 **Node.js 项目**（存在 `.env`）：
  - 更新 `.env` 中 `DATABASE_URL` 的数据库 IP；
  - 启动项目使用 `npm start`。
- 如果是 **Rails 项目**（存在 `config/database.yml`）：
  - 更新 `config/database.yml` 中 `<%= ENV.fetch("DB_HOST") { "127.0.0.1" } %>` 的数据库 IP；
  - 转发命令给 `rails` 脚本（如：`rs server` 就等价于 `rails server`）。

## 使用说明

编译：

```bash
gcc main.c -o rs