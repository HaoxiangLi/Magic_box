#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#define MAX_PAYLOAD 1024 /* maximum payload size*/
#define MY_GROUP    0

int main(void)
{
    int sock_fd;
	int ret;
    struct sockaddr_nl user_sockaddr, dest_addr;
    struct nlmsghdr *nl_msghdr;
    struct msghdr msghdr;
    struct iovec iov;

    char* kernel_msg;
	char* msg = "Hello from user";

    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
    if(sock_fd<0)
    {
        printf("Error creating socket because: %s\n", strerror(errno));
        return -1;
    }
		
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;   /* 对内核发送消息，pid为0 */
	dest_addr.nl_groups = MY_GROUP;

    bind(sock_fd, (struct sockaddr*)&dest_addr, sizeof(dest_addr));


	while (1) {
		nl_msghdr = (struct nlmsghdr*) malloc(NLMSG_SPACE(1024));	
		memset(nl_msghdr, 0, NLMSG_SPACE(1024));
		nl_msghdr->nlmsg_len = strlen(msg);
		nl_msghdr->nlmsg_pid = getpid();
		nl_msghdr->nlmsg_flags = 0;
		strcpy(NLMSG_DATA(nl_msghdr), msg);
        
		iov.iov_base = (void*) nl_msghdr;
        iov.iov_len = nl_msghdr->nlmsg_len;

		msghdr.msg_name = (void*)&dest_addr;
		msghdr.msg_namelen = sizeof(dest_addr);
		msghdr.msg_iov = &iov;
		msghdr.msg_iovlen = 1;

		ret = sendmsg(sock_fd, &msghdr, sizeof(msghdr));
		if (ret < 0) {
				printf("Error sending message\n");
				return -1;
		}
/*
        memset(nl_msghdr, 0, sizeof(nl_msghdr));
        msghdr.msg_name = (void*) &user_sockaddr;
        msghdr.msg_namelen = sizeof(user_sockaddr);
        msghdr.msg_iov = &iov;
        msghdr.msg_iovlen = 1;

		iov.iov_base = (void*) nl_msghdr;
        iov.iov_len = nl_msghdr->nlmsg_len;
*/
        printf("Waiting to receive message\n");
        ret = recvmsg(sock_fd, &msghdr, 0);
		if (ret < 0) {
   			printf("Error receiving message\n");
    		return -1;
		}

        kernel_msg = (char*)NLMSG_DATA(nl_msghdr);
        printf("Kernel message: %s\n", kernel_msg); // print to android logs
    }

    close(sock_fd);
}
