#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>

#define SOCK_PATH "/tmp/log_socket" // socket文件路径
#define LOG_TYPE_MAX_LEN 10 // 日志类型最大长度
#define LOG_CONTENT_MAX_LEN 100 // 日志内容最大长度

// 结构体，存储日志信息
struct log_info {
    char log_type[LOG_TYPE_MAX_LEN];
    int log_priority;
    char log_content[LOG_CONTENT_MAX_LEN];
};

int main(int argc, char *argv[]) {
    int log_sock, ret, i;
    struct sockaddr_un remote;
    struct log_info log_data;
    int log_total_num, log_sent_num = 0, log_send_rate;
    char *log_type;
    int log_priority;
    time_t start_time, end_time;
    int elapsed_time;
    
    // 解析命令行参数
    if (argc != 9) {
        printf("Usage: %s -t [log_type] -p [log_priority] -r [log_send_rate] -n [log_total_num]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0) {
            log_type = argv[i + 1];
        } else if (strcmp(argv[i], "-p") == 0) {
            log_priority = atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "-r") == 0) {
            log_send_rate = atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "-n") == 0) {
            log_total_num = atoi(argv[i + 1]);
        }
    }
    
    // 建立socket连接
    log_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (log_sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    strncpy(remote.sun_path, SOCK_PATH, sizeof(remote.sun_path) - 1);
    
    ret = connect(log_sock, (struct sockaddr *)&remote, sizeof(remote));
    if (ret == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    
    // 发送日志消息
    start_time = time(NULL);
    while (log_sent_num < log_total_num) {
        // 构造日志消息
        memset(&log_data, 0, sizeof(struct log_info));
        strncpy(log_data.log_type, log_type, sizeof(log_data.log_type) - 1);
        log_data.log_priority = log_priority;
        sprintf(log_data.log_content, "This is a log message, log num is %d.", log_sent_num);
        // 发送日志消息
        ret = send(log_sock, &log_data, sizeof(struct log_info), 0);
        if (ret == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
        
		printf("send ok, log sent num %d, log total num %d\n", log_sent_num, log_total_num);
        log_sent_num++;
		/*
        if (log_sent_num % log_send_rate == 0) {
            usleep(1000000 / log_send_rate);
        }
		*/
        usleep(1000000 / log_send_rate);
    }
    end_time = time(NULL);
    elapsed_time = (int)(end_time - start_time);
    
    printf("Sent %d logs in %d seconds.\n", log_total_num, elapsed_time);
    
    // 关
	close(log_sock);
	return 0;
}
