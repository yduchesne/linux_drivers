#include <linux/init.h>         // Needed for the lifecycle macros
#include <linux/module.h>       // Needed by all modules
#include <linux/sched.h>        // Needed for the 'current' macro
#include <linux/kdev_t.h>       // Needed for dev_t 
#include <linux/fs.h>           // Needed for register/unregister_chrdev_region
#include <linux/cdev.h>         // Needed for cdev_xxxx functions
#include <linux/mutex.h>        // Needed for mutex
#include <linux/ioctl.h>        // Needed for the IOCTL-related macros

#include "echo_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yduchesne");
MODULE_DESCRIPTION("Takes input from user-space and echoes it back");

// ----------------------------------------------------------------------------
// Constants

#define ECHO_MAJOR       42
#define ECHO_MAX_DEVICES  1
#define ECHO_DEVICE_NAME "echo"
#define ECHO_CLASS_NAME "echo_class"
#define ECHO_BUF_LEN 256

#define ECHO_IOCTL_CLEAR _IO(ECHO_IOCTL_MAGIC, ECHO_IOCTL_CLEAR_CMD)
#define ECHO_IOCTL_REVERSE _IO(ECHO_IOCTL_MAGIC, ECHO_IOCTL_REVERSE_CMD)

// ----------------------------------------------------------------------------
// Device-related & concurrency

struct echo_device_data {
    struct cdev cdev;
    dev_t dev_no;
    char buffer[ECHO_BUF_LEN];
    size_t size;
    struct class*  cdrv_class;
    struct device* cdrv_dev;
};

struct echo_device_data echo_devices[ECHO_MAX_DEVICES];
struct mutex crit_sec_mutex;


// ----------------------------------------------------------------------------
// IO operations

static int echo_open(struct inode *inode, struct file *file){
   printk(KERN_INFO "echo::open\n");
   return 0;
}

static int echo_release(struct inode *inode, struct file *file){
   printk(KERN_INFO "echo::release\n");
   return 0;
}

static ssize_t echo_write(struct file *file, const char __user *user_buffer,
                    size_t size, loff_t * offset)
{
    printk(KERN_INFO "echo::write");
    mutex_lock(&crit_sec_mutex);

    struct echo_device_data *data = &echo_devices[0];
    ssize_t len = min(ECHO_BUF_LEN - *offset, size);
    printk(KERN_INFO "actual size: %d, to size: %d, from size: %d\n", len, sizeof(data->buffer), size);
    int status = copy_from_user(data->buffer, user_buffer, len);
    if (status)
    {
        printk(KERN_INFO, "Error copying data from user space (status code: %d)", status);
        mutex_unlock(&crit_sec_mutex);
        return -EFAULT;
    }
    *offset += len;
    data->size += len;
    mutex_unlock(&crit_sec_mutex);
    return len;
}

static ssize_t echo_read(struct file *file, char __user *user_buffer,
                   size_t size, loff_t *offset)
{
    printk(KERN_INFO "echo::read");
    mutex_lock(&crit_sec_mutex);

    struct echo_device_data *data = &echo_devices[0];
    ssize_t len = min(data->size - *offset, size);
    printk(KERN_INFO "actual size: %d, to size: %d, from size: %d\n", len, size, sizeof(data->buffer));
    int status = copy_to_user(user_buffer, data->buffer, len);
    if (status)
    {
        printk(KERN_INFO "Error copying data to user space (status code: %d)", status);
        mutex_unlock(&crit_sec_mutex);
        return -EFAULT;
    }
    *offset += len;
    mutex_unlock(&crit_sec_mutex);
    return len;
}

static long echo_ioctl(struct file *, unsigned int cmd, unsigned long arg)
{
    struct echo_device_data *data = &echo_devices[0];
    
    switch (cmd)
    {
        case ECHO_IOCTL_CLEAR:
            printk(KERN_INFO "echo::ioctl::clear");
            data->size = 0;
            data->buffer[0] = '\0';
            return 0;
        case ECHO_IOCTL_REVERSE:
            printk(KERN_INFO "echo::ioctl::reverse");   
            int i = 0;
            int j = data->size - 1;
            while (i < j)
            {
                char tmp = data->buffer[i];
                data->buffer[i] = data->buffer[j];
                data->buffer[j] = tmp;
                i++;
                j--;
            }
            return 0;
        default:
            printk(KERN_ERR "echo::ioctl::invalid_cmd");
            return -EINVAL;
    }

}

const struct  file_operations echo_ops = 
{
    .owner = THIS_MODULE,
    .open = echo_open,
    .release = echo_release,
    .read = echo_read,
    .write = echo_write,
    .unlocked_ioctl = echo_ioctl
};

// ----------------------------------------------------------------------------
// Lifecycle

static int __init echo_init(void)
{
    printk(KERN_INFO "-> echo::init\n");
    int status = register_chrdev_region(MKDEV(ECHO_MAJOR, 0), ECHO_MAX_DEVICES, "echo_device_driver");
    if (status != 0)
    {
        goto Error;
    }
    for(int minor = 0; minor < ECHO_MAX_DEVICES; minor++) {
        cdev_init(&echo_devices[minor].cdev, &echo_ops);
        echo_devices[minor].dev_no = MKDEV(ECHO_MAJOR, minor);
        cdev_add(&echo_devices[minor].cdev, echo_devices[minor].dev_no, 1);
        echo_devices[minor].cdrv_class = class_create(THIS_MODULE, ECHO_CLASS_NAME);
        if (IS_ERR(echo_devices[minor].cdrv_class)){
             printk(KERN_ALERT "Could not create %s device class\n", ECHO_CLASS_NAME);
             status = PTR_ERR(echo_devices[minor].cdrv_class);
             goto Error;
        }
        echo_devices[minor].size = 0;
        printk(KERN_INFO "Created %s device class\n", ECHO_CLASS_NAME);
        echo_devices[minor].cdrv_dev = device_create(echo_devices[minor].cdrv_class, NULL, MKDEV(ECHO_MAJOR, minor), NULL, ECHO_DEVICE_NAME);
    }
    mutex_init(&crit_sec_mutex);
    printk(KERN_INFO "<- echo::init\n");
    return 0;

    Error:
        printk(KERN_INFO, "Error occurred. Aborting module initialization (status code: %i)\n", status);
        return status;
}

static void __exit echo_exit(void)
{
    printk(KERN_INFO, "-> echo::exit\n");
    for(int minor = 0; minor < ECHO_MAX_DEVICES; minor++) {
        device_destroy(echo_devices[minor].cdrv_class, echo_devices[minor].dev_no);
        class_destroy(echo_devices[minor].cdrv_class);
        cdev_del(&echo_devices[minor].cdev);
    }
    unregister_chrdev_region(MKDEV(ECHO_MAJOR, 0), ECHO_MAX_DEVICES);
    printk(KERN_INFO, "<- echo::exit\n");
}

module_init(echo_init);
module_exit(echo_exit);

// ----------------------------------------------------------------------------

void mod_info(void)
{
    printk(KERN_INFO, "Proc name: %s, PID: %i\n", current->comm, current->pid);
}