# rs —— 自动更新宿主机 IP 并转发 Rails 命令

在 WSL 环境中进行 Rails 开发时，宿主机运行的 MySQL 数据库 IP 地址经常变更。为了解决这个问题，本工具可以：

- 自动获取 WSL 宿主机当前的 IP；
- 更新 `config/database.yml` 中的数据库连接地址；
- 将所有命令转发给 `rails` 脚本（rs server = rails server）。
