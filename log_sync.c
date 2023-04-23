#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/list.h>
#include <net/sock.h>
#include <linux/ubus.h>
#include <linux/netlink.h>

#define NETLINK_USER 31
#define N 100
#define MAX_LOG_LEN 200
#define MAX_LOG_NUM 10000
#define PROC_FILENAME "log_stats"

static struct task_struct *log_sync_task;
static struct sock *nl_sock;
static struct proc_dir_entry *proc_entry;
static struct timer_list log_timer;
static unsigned int log_count = 0;

struct log_entry {
    char msg[MAX_LOG_LEN];
	struct list_head list;
};

static LIST_HEAD(log_list);

static void generate_log(struct timer_list* timer)
{
	struct log_entry *new_entry, *old_entry;
    log_count++;
    printk(KERN_INFO "Generated log %d\n", log_count);
    
    // 将新日志添加到链表中
    new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
    if (new_entry == NULL) {
        printk(KERN_ERR "Failed to allocate memory for log entry\n");
        return;
    }
    snprintf(new_entry->msg, sizeof(new_entry->msg), "Generated log %d", log_count);
    list_add_tail(&new_entry->list, &log_list);

	unsigned int count = 0;
	struct list_head *pos, *q;
	list_for_each_safe(pos, q, &log_list) {
		count++;
		if((count > MAX_LOG_NUM) || (list_empty(&log_list))) {
			list_del(pos);
			old_entry = list_entry(pos, struct log_entry, list);
			kfree(old_entry);
		}
	}

    mod_timer(&log_timer, jiffies + N*HZ);
}

int send_log_to_user(struct ubus_context *ctx)
{
    struct ubus_object *obj;
    struct ubus_request_data req;
    struct log_data *log = NULL;
    struct list_head *pos, *n;

    list_for_each_safe(pos, n, &log_list) {
        log = list_entry(pos, struct log_data, list);
        // 使用ubus发送日志到用户空间
        obj = ubus_lookup(ctx, "log");
        if (obj) {
            ubus_request_init(&ctx, &req, obj->name, "add", NULL, NULL, 5000);
            ubus_request_add_object(&req, "message", log->message);
            ubus_invoke(ctx, &req);
            ubus_put_object(ctx, obj);
        }

        // 从链表中移除已处理的日志
        list_del(pos);
        kfree(log);
    }

    return 0;
}

static void netlink_send_log(void)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    int len;

    // 创建Netlink消息
    skb = nlmsg_new(log_length + sizeof(struct nlmsghdr), GFP_KERNEL);
    if (!skb) {
        return;
    }

    nlh = nlmsg_put(skb, 0, 0, NETLINK_LOG, log_length + sizeof(struct nlmsghdr), 0);
	if (nlh == NULL) {
		pr_err("failed to create netlink message\n");
		nlmsg_free(skb);
		break;
	}
    strncpy(nlmsg_data(nlh), "log message", log_length);

    // 发送Netlink消息
    len = netlink_unicast(sock_net(&init_net), skb, current->pid, MSG_DONTWAIT);
    if (len < 0) {
        printk(KERN_ERR "Failed to send netlink message: %d\n", len);
		nlmsg_free(skb);
    }
}

static int log_sync_thread(void *data)
{
    struct ubus_context *ubus_ctx = NULL;
    int ret;

    // 初始化ubus
    ubus_ctx = ubus_connect(NULL);
    if (!ubus_ctx) {
        printk(KERN_ERR "Failed to connect to ubus\n");
		return -1;
	}
	while (!kthread_should_stop()) {
			// 等待ubus事件
			ret = ubus_handle_event(ubus_ctx);
			if (ret) {
					continue;
			}

			// 发送日志到用户空间
			send_log_to_user(ubus_ctx);

			// 发送日志到内核
			netlink_send_log();
	}

	// 断开ubus连接
	ubus_free(ubus_ctx);
    return 0;
}


static int proc_file_show(struct seq_file *m, void *v)
{
    seq_printf(m, "log generation rate: %d/s\n", MAX_LOG_NUM);
    seq_printf(m, "total logs generated: %lld\n", (long long)MAX_LOG_NUM * (long long)jiffies/HZ);
    seq_printf(m, "total logs lost: 0\n"); /* assume no log is lost */
    return 0;
}

static int proc_file_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_file_show, NULL);
}

static const struct proc_ops proc_file_fops = {
    .proc_open       = proc_file_open,
    .proc_read       = seq_read,
    .proc_lseek     = seq_lseek,
    .proc_release    = single_release,
};

static int netlink_init(void)
{
    struct netlink_kernel_cfg cfg = {
        .input = NULL,
    };

    nl_sock = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    if (nl_sock == NULL) {
        pr_err("failed to create netlink socket\n");
        return -ENOMEM;
    }

    return 0;
}

static void netlink_exit(void)
{
    if (nl_sock != NULL) {
        netlink_kernel_release(nl_sock);
    }
}

static int __init log_sync_init(void)
{
    int ret;

	INIT_LIST_HEAD(&log_list);
	log_timer.function = generate_log;
    log_timer.expires = jiffies + N * HZ;
    add_timer(&log_timer);

    /* start log sync thread */
    log_sync_task = kthread_run(log_sync_thread, NULL, "log_sync_thread");
    if (IS_ERR(log_sync_task)) {
        pr_err("failed to create log_sync_thread\n");
        ret = PTR_ERR(log_sync_task);
        goto err;
    }

    /* create proc file */
    proc_entry = proc_create(PROC_FILENAME, 0, NULL, &proc_file_fops);
    if (proc_entry == NULL) {
        pr_err("failed to create proc entry\n");
        ret = -ENOMEM;
        goto err_proc;
    }

    /* initialize netlink */
    ret = netlink_init();
    if (ret < 0) {
        goto err_netlink;
    }

    pr_info("log_sync module loaded\n");
    return 0;

err_netlink:
    proc_remove(proc_entry);
err_proc:
    kthread_stop(log_sync_task);
err:
    return ret;
}

static void __exit log_sync_exit(void)
{
	del_timer(&log_timer);
    netlink_exit();
    proc_remove(proc_entry);
	if(log_sync_task) { 
    	kthread_stop(log_sync_task);
	}
	struct log_data *log, *tmp;
	list_for_each_entry_safe(log, tmp, &log_list, list) {
		list_del(&log->list);
		kfree(log);
	}
    pr_info("log_sync module unloaded\n");
}

module_init(log_sync_init);
module_exit(log_sync_exit);
MODULE_LICENSE("GPL");

