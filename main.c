#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // execvp, popen 等
#include <regex.h>

int main(int argc, char *argv[]) {
    printf("Hello, World!\n");

    FILE *fp;
    char line[1024];
    char *config_file = "config/database.yml";
    char *temp_file = "config/database.yml.tmp";
    char new_ip[128];

    // 获取 WSL 宿主机 IP 地址
    fp = popen("ip route show | grep -i default | awk '{ print $3 }'", "r");
    if (fp == NULL) {
        perror("Failed to run command");
        return 1;
    }
    if (fgets(new_ip, sizeof(new_ip), fp) != NULL) {
        new_ip[strcspn(new_ip, "\n")] = '\0';  // 去除换行符
    }
    pclose(fp);

    // 打开原文件和临时文件
    FILE *read_fp = fopen(config_file, "r");
    FILE *write_fp = fopen(temp_file, "w");

    if (read_fp == NULL || write_fp == NULL) {
        perror("Failed to open file");
        return 1;
    }

    // 正则表达式匹配 host: <%= ENV.fetch("DB_HOST") { "127.0.0.1" } %> 模式
    regex_t regex;
    regcomp(&regex, "ENV\\.fetch\\(.*?\\) \\{ \".*?\" \\}", REG_EXTENDED);

    while (fgets(line, sizeof(line), read_fp)) {
        regmatch_t matches[1];
        if (regexec(&regex, line, 1, matches, 0) == 0) {
            // 匹配成功，进行替换
            int prefix_len = matches[0].rm_so;
            int suffix_start = matches[0].rm_eo;

            // 提取前缀部分
            char prefix[prefix_len + 1];
            memcpy(prefix, line, prefix_len);
            prefix[prefix_len] = '\0';

            const char *middle_start = strstr(line + prefix_len, "{ \"");
            const char *middle_end = strstr(middle_start + 2, "\" }");
            if (!middle_start || !middle_end) {
                fputs(line, write_fp);  // 格式不匹配，原样写入
                continue;
            }

            // 提取后缀部分
            char suffix[1024];
            strcpy(suffix, line + suffix_start);

            // 构造新字符串（使用分步 snprintf 避免溢出警告）
            char new_line[1024];
            int n = snprintf(new_line, sizeof(new_line), "%s", prefix);
            if (n >= 0 && n < sizeof(new_line)) {
                snprintf(new_line + n, sizeof(new_line) - n,
                         "ENV.fetch(\"DB_HOST\") { \"%s\" }%s", new_ip, suffix);
            } else {
                // 如果缓冲区太小或出错，降级处理：原样写入
                fputs(line, write_fp);
                continue;
            }

            fputs(new_line, write_fp);
        } else {
            // 不匹配，直接写入原行
            fputs(line, write_fp);
        }
    }

    fclose(read_fp);
    fclose(write_fp);
    regfree(&regex);

    // 替换原文件
    remove(config_file);
    rename(temp_file, config_file);

    // 记录操作日志
    char log_file_path[256];
    snprintf(log_file_path, sizeof(log_file_path), "/home/%s/.railsc", getenv("USER"));
    FILE *log_fp = fopen(log_file_path, "a");
    if (log_fp != NULL) {
        fprintf(log_fp, "Updated DB_HOST to %s in %s\n", new_ip, config_file);
        fclose(log_fp);
    }

    // 转发命令给 rails
    if (argc <= 1) {
        fprintf(stderr, "Usage: %s <rails command...>\n", argv[0]);
        return 1;
    }

    char *new_argv[argc + 1];
    new_argv[0] = "rails";
    memcpy(new_argv + 1, argv + 1, sizeof(char *) * (argc - 1));
    new_argv[argc] = NULL;

    execvp("rails", new_argv);

    // 如果走到这里说明 exec 失败
    perror("Failed to execute rails command");
    return 1;
}