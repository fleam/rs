#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <fcntl.h>

// 获取宿主机 IP 地址
int get_host_ip(char *ip_buf, size_t len) {
    FILE *fp = popen("ip route show | grep -i default | awk '{ print $3 }'", "r");
    if (!fp) return -1;

    if (fgets(ip_buf, len, fp)) {
        ip_buf[strcspn(ip_buf, "\n")] = '\0';
        pclose(fp);
        return 0;
    }

    pclose(fp);
    return -1;
}

// 替换 .env 文件中的 DATABASE_URL 的 IP 地址
void update_env_file(const char *filename, const char *new_ip) {
    FILE *read_fp = fopen(filename, "r");
    FILE *write_fp = fopen(".env.tmp", "w");

    if (!read_fp || !write_fp) {
        perror("无法打开 .env 文件");
        if (read_fp) fclose(read_fp);
        if (write_fp) fclose(write_fp);
        return;
    }

    regex_t regex;
    regcomp(&regex, "^DATABASE_URL=\"mysql://[^@]+@[^:]+:[0-9]+/[^\"\\?]*\"", REG_EXTENDED);

    char line[1024];
    while (fgets(line, sizeof(line), read_fp)) {
        if (regexec(&regex, line, 0, NULL, 0) == 0) {
            // 匹配成功，进行替换
            char *start = strstr(line, "@");
            if (start) {
                char *colon = strchr(start + 1, ':');
                if (colon) {
                    char new_line[1024];
                    snprintf(new_line, sizeof(new_line),
                             "DATABASE_URL=\"mysql://%s%s",
                             new_ip,
                             colon);  // 保留端口和数据库名
                    fputs(new_line, write_fp);
                } else {
                    fputs(line, write_fp);
                }
            } else {
                fputs(line, write_fp);
            }
        } else {
            fputs(line, write_fp);
        }
    }

    fclose(read_fp);
    fclose(write_fp);
    regfree(&regex);

    remove(filename);
    rename(".env.tmp", filename);
}

// 替换 config/database.yml 文件中的 DB_HOST
void update_yaml_file(const char *filename, const char *new_ip) {
    FILE *read_fp = fopen(filename, "r");
    FILE *write_fp = fopen("database.yml.tmp", "w");

    if (!read_fp || !write_fp) {
        perror("无法打开 database.yml 文件");
        if (read_fp) fclose(read_fp);
        if (write_fp) fclose(write_fp);
        return;
    }

    regex_t regex;
    regcomp(&regex, "ENV\\.fetch\\(.*?\\) \\{ \".*?\" \\}", REG_EXTENDED);

    char line[1024];
    while (fgets(line, sizeof(line), read_fp)) {
        regmatch_t matches[1];
        if (regexec(&regex, line, 1, matches, 0) == 0) {
            int prefix_len = matches[0].rm_so;
            int suffix_start = matches[0].rm_eo;

            char prefix[prefix_len + 1];
            memcpy(prefix, line, prefix_len);
            prefix[prefix_len] = '\0';

            const char *middle_start = strstr(line + prefix_len, "{ \"");
            const char *middle_end = strstr(middle_start + 2, "\" }");
            if (!middle_start || !middle_end) {
                fputs(line, write_fp);
                continue;
            }

            char suffix[1024];
            strcpy(suffix, line + suffix_start);

            char new_line[1024];
            int n = snprintf(new_line, sizeof(new_line), "%s", prefix);
            if (n >= 0 && n < sizeof(new_line)) {
                snprintf(new_line + n, sizeof(new_line) - n,
                         "ENV.fetch(\"DB_HOST\") { \"%s\" }%s", new_ip, suffix);
            } else {
                fputs(line, write_fp);
                continue;
            }

            fputs(new_line, write_fp);
        } else {
            fputs(line, write_fp);
        }
    }

    fclose(read_fp);
    fclose(write_fp);
    regfree(&regex);

    remove(filename);
    rename("database.yml.tmp", filename);
}

int main(int argc, char *argv[]) {
    char new_ip[128];

    // 获取宿主机 IP
    if (get_host_ip(new_ip, sizeof(new_ip)) != 0) {
        fprintf(stderr, "获取宿主机 IP 失败\n");
        return 1;
    }

    printf("检测到宿主机 IP: %s\n", new_ip);

    int is_node = (access(".env", F_OK) != -1);
    int is_rails = (access("config/database.yml", F_OK) != -1);

    if (!is_node && !is_rails) {
        fprintf(stderr, "未找到 .env 或 config/database.yml，无法判断项目类型\n");
        return 1;
    }

    if (is_node) {
        printf("检测到 Node.js 项目，更新 .env...\n");
        update_env_file(".env", new_ip);

        // 执行 npm start
        printf("启动 npm start ...\n");
        char *new_argv[] = {"npm", "start", NULL};
        execvp("npm", new_argv);
    } else if (is_rails) {
        printf("检测到 Rails 项目，更新 database.yml...\n");
        update_yaml_file("config/database.yml", new_ip);

        // 执行 rails server
        printf("启动 rails server ...\n");
        char *new_argv[] = {"rails", "server", NULL};
        execvp("rails", new_argv);
    }

    // 如果走到这里说明 exec 失败
    perror("Failed to execute command");
    return 1;
}