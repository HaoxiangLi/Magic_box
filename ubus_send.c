#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/types.h>

#define NETLINK_USER 31
#define MY_GROUP    0
struct sock *nl_sk = NULL;

static void send_event_to_user_space(struct sk_buff* skb)
{
    struct nlmsghdr *nlh;
    char* msg = "Hello from kernel";
	int len = strlen(msg) + 1;

	printk("[debug]:send event to user space\n");
    skb = nlmsg_new(len, GFP_KERNEL);
    if (!skb) {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    }
    nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, len, 0);
    NETLINK_CB(skb).dst_group = MY_GROUP;
	NETLINK_CB(skb).portid = 0;
    memcpy(NLMSG_DATA(nlh), msg, len);

    if(nlmsg_multicast(nl_sk, skb, 0, 0, GFP_KERNEL) < 0) {
		printk("Not sent\n");
	}
	else {
		printk("Sent ok\n");
	}
}

static int __init netlink_init(void)
{
    struct netlink_kernel_cfg cfg = {
    	.input = send_event_to_user_space,
    };
    nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    if (!nl_sk) {
        printk(KERN_ERR "Failed to create netlink socket\n");
        return -ENOMEM;
    }
	printk("[debug]:Create netlink ok\n");


    return 0;
}

static void __exit netlink_exit(void)
{
    if (nl_sk) {
        netlink_kernel_release(nl_sk);
    }
}

module_init(netlink_init);
module_exit(netlink_exit);
MODULE_LICENSE("GPL");

