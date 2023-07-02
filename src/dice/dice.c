#include <linux/kernel.h>       // Needed for kstrtoint
#include <linux/version.h>      // Needed for LINUX_VERSION_CODE and KERNEL_VERSION macros
#if LINUX_VERSION_CODE > KERNEL_VERSION(4,0,1)
#include <linux/vmalloc.h>
#endif
#include <linux/init.h>         // Needed for the lifecycle macros
#include <linux/module.h>       // Needed by all modules
#include <linux/sched.h>        // Needed for the 'current' macro
#include <linux/mutex.h>        // Needed for mutex
#include <linux/proc_fs.h>      // Needed for proc-related functions

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yduchesne");
MODULE_DESCRIPTION("Takes dice count has input; returns dice roll result as output");

// ----------------------------------------------------------------------------
// Constants

#define DICE_PROC_NAME "dice"
#define DICE_BUF_LEN 256
#define DICE_DEFAULT_DICE_COUNT 2

// ----------------------------------------------------------------------------
// Module state

static struct proc_dir_entry *proc_dice;


typedef struct _proc_device_data 
{
    char* buffer;
    int dice_count;
    size_t size;
    struct mutex crit_sec_mutex;

} proc_device_data;


proc_device_data mod_data;

// ----------------------------------------------------------------------------
// IO operations

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static ssize_t dice_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
#else
static int dice_write(struct file *file,const char * buf, unsigned long count, void *data)
#endif
{
    printk(KERN_INFO "dice::write");
    mutex_lock(&mod_data.crit_sec_mutex);

    proc_device_data* p_mod_data = &mod_data;
    size_t len = min(DICE_BUF_LEN, count);
    printk(KERN_INFO "actual size: %lu, to size: %lu, from size: %lu\n", len, sizeof(p_mod_data->buffer), count);
    int status = copy_from_user(p_mod_data->buffer, buf, len);
    // Insuring that buffer is a valid string
    p_mod_data->buffer[len - 1] = "\0";
    if (status)
    {
        printk(KERN_ERR "Error copying data from user space (status code: %d)", status);
        goto Error;
    }
    printk(KERN_ERR "Got dice count value from user: %s\n", p_mod_data->buffer);
    int new_dice_count;
    status = kstrtoint(p_mod_data->buffer, 0, &new_dice_count);
    if (status)
    {
        printk(KERN_INFO "Converting (status code: %d)", status);
        goto Error;        
    }
    p_mod_data->dice_count = new_dice_count;

    mutex_unlock(&mod_data.crit_sec_mutex);
    return len;

    Error:
        mutex_unlock(&mod_data.crit_sec_mutex);
        return -EFAULT;

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static int eof[1];
static ssize_t dice_read(struct file *file, char *buf, size_t len, loff_t *ppos)
#else
static int dice_read(char *buf, char **start, off_t offset, int len, int *eof, void *unused)
#endif
{
    printk(KERN_INFO "dice::read");
    mutex_lock(&mod_data.crit_sec_mutex);

    // Convert to pointer for more intuitive use
    int fmt_total_len = 0;

    if (*eof) { 
        *eof = 0; 
    }
    else
    {
        *eof = 1;

        unsigned dice_roll_result;
        char fmt_buf[DICE_BUF_LEN];
        int fmt_len;
        for (int dice_num = 0; dice_num < mod_data.dice_count; dice_num++)
        {
            
            // () not needed with sizeof object
            get_random_bytes(&dice_roll_result, sizeof dice_roll_result); 

            // Best to use unsigned constants with unsigned objects.
            dice_roll_result = 1u + (dice_roll_result % 6u);

            if (dice_num == 0)
            {
                // scnprintf returns the number of characters written, excluding
                // the string termination char ('\0')
                fmt_len = scnprintf(fmt_buf, DICE_BUF_LEN, "%u", dice_roll_result);
            }
            else
            {
                fmt_len = scnprintf(fmt_buf, DICE_BUF_LEN, ",%u", dice_roll_result);
            }
            if (fmt_len < 0)
            {
                printk(KERN_ERR "Error converting dice roll result: %u\n", dice_roll_result);
                goto Error;
            }
            printk(KERN_INFO "Got result for dice %d: %lu (temp string: %s)\n", dice_num,  dice_roll_result, fmt_buf);

 
            // Usin fmt_lem - 1 as we want to exclude copying the '\0' character
            // from the source buffer to the target (we're going to add the '\0'
            // character to the target and the end of all dice roll results)
            for (int srcIndex = 0; srcIndex < fmt_len - 1; srcIndex++, fmt_total_len++)
            {
                // (DICE_BUF_LEN - 1) -> ensuring we have one slot left for the 
                // string termination character at the end.
                if ( fmt_total_len >= (DICE_BUF_LEN - 1) )
                {
                    printk(KERN_ERR "Cannot populate dice roll result: end of output buffer reached\n");
                    goto Error;
                }
                printk(KERN_INFO "Copying character %c at index %d to target buffer index %d\n", fmt_buf[srcIndex], srcIndex, fmt_total_len);
                mod_data.buffer[fmt_total_len] = fmt_buf[srcIndex];
            }
        }
        
        // Accounting for string termination character
        fmt_total_len += 1;
        // Now adding string termination character to result
        mod_data.buffer[fmt_total_len - 1] = '\0';
        //printk(KERN_INFO "Got dice roll result (dice_count: %d): %s\n", mod_data.dice_count, mod_data.buffer);
        const char* tmp = "Result!!!";
        snprintf(buf, len, "%s", tmp);
        //snprintf(buf, len, "%s", mod_data.buffer);
    }

    mutex_unlock(&mod_data.crit_sec_mutex);
    return fmt_total_len;

    Error:
        mutex_unlock(&mod_data.crit_sec_mutex);
        return -EFAULT;
    
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static const struct proc_ops proc_fops = {
    .proc_read = dice_read,
    .proc_write = dice_write
};
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static const struct file_operations  proc_fops = {
    .owner = THIS_MODULE,
    .read = dice_read,
    .write = dice_write
};
#endif

// ----------------------------------------------------------------------------
// Lifecycle

static int __init dice_init(void)
{
    printk(KERN_INFO "-> dice::init\n");

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    proc_dice = proc_create(DICE_PROC_NAME, 0777, NULL, &proc_fops);
    if(proc_dice == NULL)
    {
        goto Error;
    }
    #else
    proc_dice = create_proc_entry(DICE_PROC_NAME, 0, 0);
    if(proc_dice == NULL)
    {
        goto Error;
    }
    proc_dice->read_proc = dice_read;
    proc_dice->write_proc = dice_write;
    #endif


    #if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,29)
    proc_dice->owner = THIS_MODULE;
    #endif

    mod_data.buffer = vmalloc(DICE_BUF_LEN);
    mod_data.dice_count = DICE_DEFAULT_DICE_COUNT;

    mutex_init(&mod_data.crit_sec_mutex);
    printk(KERN_INFO "<- dice::init\n");
    return 0;

    Error:
        printk(KERN_ERR, "Error occurred. Aborting module initialization (could not create proc entry %s)\n", DICE_PROC_NAME);
        return -ENOMEM;
  
}

static void __exit dice_exit(void)
{
    printk(KERN_INFO, "-> dice::exit\n");
    if (mod_data.buffer)
    {
        vfree(mod_data.buffer);
    }
    if (proc_dice)
    {
        remove_proc_entry(DICE_PROC_NAME, 0);
    }
    printk(KERN_INFO, "<- dice::exit\n");
}

module_init(dice_init);
module_exit(dice_exit);

// ----------------------------------------------------------------------------

void mod_info(void)
{
    printk(KERN_INFO, "Proc name: %s, PID: %i\n", current->comm, current->pid);
}