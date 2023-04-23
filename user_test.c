#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/log_socket"
#define MAX_LOG_LEN 200
#define MAX_LOG_NUM 10000

struct log_entry {
    char msg[MAX_LOG_LEN + 1];
};

int main()
{
    int sock_fd;
    struct sockaddr_un addr;
    struct log_entry *log_buf;
    int len, count, total_len;
    int i;

    sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    log_buf = malloc(sizeof(struct log_entry) * MAX_LOG_NUM);
    if (!log_buf) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    count = 0;
    total_len = 0;

    while (1) {
        len = recv(sock_fd, log_buf + count, sizeof(struct log_entry) * (MAX_LOG_NUM - count), MSG_DONTWAIT);
        if (len == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recv");
                exit(EXIT_FAILURE);
            }
        } else if (len == 0) {
            printf("Connection closed\n");
            exit(EXIT_SUCCESS);
        } else {
            count += len / sizeof(struct log_entry);
        }

        for (i = 0; i < count; i++) {
            total_len += strlen(log_buf[i].msg);
        }

        printf("Received %d logs, total length %d\n", count, total_len);

        count = 0;
        total_len = 0;

        usleep(100000); /* sleep for 100ms */
    }

    free(log_buf);

    return 0;
}

