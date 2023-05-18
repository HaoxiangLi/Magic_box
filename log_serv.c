#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define SOCK_PATH "/tmp/log_socket"
#define LOG_PRIO_HIGH 1
#define LOG_PRIO_MEDIUM 2
#define LOG_PRIO_LOW 3
#define LOG_TYPE_MAX_LEN 10
#define LOG_CONTENT_MAX_LEN 100

#define MAX_CACHE_SIZE 1000

struct log_message {
	char log_type[LOG_TYPE_MAX_LEN];
    int priority;
    char message[LOG_CONTENT_MAX_LEN];
};

struct log_cache {
    struct log_message logs[MAX_CACHE_SIZE];
    int head;
    int tail;
    int count;
};

struct log_stats {
    int high_received;
    int high_sent;
    int high_dropped;
    int medium_received;
    int medium_sent;
    int medium_dropped;
    int low_received;
    int low_sent;
    int low_dropped;
};

int sock_fd;
int cache_enabled = 1;
int rate_limit = 0;
struct sockaddr_in server_addr;
struct log_cache cache;
struct log_stats stats;

void fa(int signo) {
	printf("High priority logs: received %d, sent %d, dropped %d\n", stats.high_received, stats.high_sent, stats.high_dropped);
	printf("Medium priority logs: received %d, sent %d, dropped %d\n", stats.medium_received, stats.medium_sent, stats.medium_dropped);
	printf("Low priority logs: received %d, sent %d, dropped %d\n", stats.low_received, stats.low_sent, stats.low_dropped);

	close(sock_fd);
	printf("Service exit\n");
	exit(0);
}

void send_log(struct log_message log) {
    int send_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    if (sendto(send_fd, &log, sizeof(log), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }
    close(send_fd);
    switch (log.priority) {
    case LOG_PRIO_HIGH:
        stats.high_sent++;
        break;
    case LOG_PRIO_MEDIUM:
        stats.medium_sent++;
        break;
    case LOG_PRIO_LOW:
        stats.low_sent++;
        break;
    }
}

void drop_cache() {
    if (cache.count == 0) {
        return;
    }
    send_log(cache.logs[cache.tail]);
    cache.tail = (cache.tail + 1) % MAX_CACHE_SIZE;
    cache.count--;
    switch (cache.logs[cache.tail].priority) {
    case LOG_PRIO_HIGH:
        stats.high_dropped++;
        break;
    case LOG_PRIO_MEDIUM:
        stats.medium_dropped++;
        break;
    case LOG_PRIO_LOW:
        stats.low_dropped++;
        break;
    }
}

int main(int argc, char* argv[]) {
	if (argc < 5 || strcmp(argv[1], "-r") != 0 || strcmp(argv[3], "-s") != 0) {
			printf("Usage: %s -r <rate limit> -s <server IP>\n", argv[0]);
			return 1;
	}
	rate_limit = atoi(argv[2]);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(argv[4]);
	server_addr.sin_port = htons(514);

	// 创建UNIX域socket
	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd == -1) {
			perror("socket");
			exit(EXIT_FAILURE);
	}
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
	if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
			perror("bind");
			exit(EXIT_FAILURE);
	}
	if (listen(sock_fd, 5) == -1) {
			perror("listen");
			exit(EXIT_FAILURE);
	}
	struct timeval timeout;
	fd_set read_fds;

	signal(SIGINT, fa);
	printf("Ctrl+C exit service.\n");

	while (1) {
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		FD_ZERO(&read_fds);
		FD_SET(sock_fd, &read_fds);

		int max_fd = sock_fd;

		if (cache.count > 0) {
			FD_SET(sock_fd, &read_fds);
		}

		int ready_fds = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
		if (ready_fds == -1) {
			if (errno == EINTR) {
				continue;
			} else {
				perror("select");
				exit(EXIT_FAILURE);
			}
		}

		if (ready_fds == 0) {
			continue;
		}

		if (FD_ISSET(sock_fd, &read_fds)) {
			// 接收日志
			struct log_message log;
			int i = 0;
			int client_fd = accept(sock_fd, NULL, NULL);
			if (client_fd == -1) {
				perror("accept");
				exit(EXIT_FAILURE);
			}
			pid_t pid = fork();
			if(pid == 0) {
				while(1) {
					int n = recv(client_fd, &log, sizeof(log), 0);
					if (n == -1) {
						perror("read");
						exit(EXIT_FAILURE);
					} else if (n == 0) {
						switch (log.priority) {
							case LOG_PRIO_HIGH:
								stats.high_received++;
								break;
							case LOG_PRIO_MEDIUM:
								stats.medium_received++;
								break;
							case LOG_PRIO_LOW:
								stats.low_received++;
								break;
						}
			printf("recv log: log_type: %s, log_priority: %d, msg %s\n", log.log_type, log.priority, log.message);
						//sleep(1);
						//close(client_fd);
						continue;
					}
				}
				close(client_fd);
			}

			if (cache_enabled && cache.count == MAX_CACHE_SIZE) {
				drop_cache();
			}

			if (rate_limit == 0 || (rate_limit > 0 && stats.high_sent + stats.medium_sent + stats.low_sent < rate_limit)) {
				send_log(log);
			} else if (cache_enabled) {
				if (cache.count < MAX_CACHE_SIZE) {
					cache.logs[cache.head] = log;
					cache.head = (cache.head + 1) % MAX_CACHE_SIZE;
					cache.count++;
				} else {
					drop_cache();
					cache.logs[cache.head] = log;
					cache.head = (cache.head + 1) % MAX_CACHE_SIZE;
					cache.count++;
				}
			}
		}
	}


	return 0;
}
