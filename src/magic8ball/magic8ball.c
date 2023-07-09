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
MODULE_DESCRIPTION("Takes magic8ball count has input; returns magic8ball roll result as output");

// ----------------------------------------------------------------------------
// Constants

#define MAGIG8BALL_PROC_NAME "magic8ball"
#define MAGIG8BALL_BUF_LEN 256
#define MAGIG8BALL_INPUT_LEN 1
#define MAGIG8BALL_DEFAULT_MAGIG8BALL_COUNT 2

// Messages

#define MAGIG8BALL_MSG0 "It is certain."
#define MAGIG8BALL_MSG1 "It is decidedly so."
#define MAGIG8BALL_MSG2 "Without a doubt."
#define MAGIG8BALL_MSG3 "Yes. definitely."
#define MAGIC8BALL_MSG4 "You may rely on it."
#define MAGIC8BALL_MSG5 "As I see it, yes."
#define MAGIC8BALL_MSG6 "Most likely."
#define MAGIC8BALL_MSG7 "Outlook good."
#define MAGIC8BALL_MSG8 "Yes."
#define MAGIC8BALL_MSG9 "Signs point to yes."
#define MAGIC8BALL_MSG10 "Reply hazy, try again."
#define MAGIC8BALL_MSG11 "Ask again later."
#define MAGIC8BALL_MSG12 "Better not tell you now."
#define MAGIC8BALL_MSG13 "Cannot predict now."
#define MAGIC8BALL_MSG14 "Concentrate and ask again."
#define MAGIC8BALL_MSG15 "Don't count on it."
#define MAGIC8BALL_MSG16 "My reply is no."
#define MAGIC8BALL_MSG17 "My sources say no."
#define MAGIC8BALL_MSG18 "Outlook not so good."
#define MAGIC8BALL_MSG19 "Very doubtful."

#define MAGIC8BALL_NUM_MSG 20

// ----------------------------------------------------------------------------
// Module state

static struct proc_dir_entry *proc_magic8ball;


typedef struct _proc_data 
{
    char* buffer;
    char** messages;
    int magic8ball_count;
    size_t size;
    struct mutex crit_sec_mutex;

} proc_data;


proc_data data;

// ----------------------------------------------------------------------------
// IO operations

static int safe_copy(const char* src, int len, char* dst, int dstIndex, int maxLen)
{
    int count = 0;
    for (int srcIndex = 0; srcIndex < len; srcIndex++, count++)
    {
        // (DICE_BUF_LEN - 1) -> ensuring we have one slot left for the 
        // string termination character at the end.
        if (dstIndex >= maxLen)
        {
            //printk(KERN_DEBUG "Cannot copy: end of destination buffer reached\n");
            goto Error;
        }
        //printk(KERN_DEBUG "Copying character %c in source buffer at index %d to destination buffer at index %d\n", src[srcIndex], srcIndex, (dstIndex + count));
        dst[dstIndex + count] = src[srcIndex];
    }
    
    // Appending space char at end of copied string.
    // Ensuring enough space is left.
    if ( (dstIndex + count) == maxLen )
    {
        printk(KERN_ERR "Cannot copy: end of destination buffer reached\n");
        goto Error;
    }
    dst[dstIndex + count] = ' ';
    count++;

    printk(KERN_INFO "Copied %d characters to destination buffer (initial len: %d)\n", count, len);

    return count;

    Error:
        return -1;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static ssize_t magic8ball_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
#else
static int magic8ball_write(struct file *file,const char * buf, unsigned long count, void *data)
#endif
{
    printk(KERN_INFO "magic8ball::write");
    mutex_lock(&data.crit_sec_mutex);

    size_t len = min(MAGIG8BALL_BUF_LEN - 1, count);
    printk(KERN_INFO "actual size: %lu, to size: %lu, from size: %lu\n", len, MAGIG8BALL_BUF_LEN, count);
    int status = copy_from_user(data.buffer, buf, len);
    // Insuring that buffer is a valid string
    data.buffer[len] = '\0';
    if (status)
    {
        printk(KERN_ERR "Error copying data from user space (status code: %d)", status);
        goto Error;
    }
    printk(KERN_DEBUG "Got magic8ball count from user: '%s' (copied %lu chars. %lu)\n", data.buffer, len, strlen(data.buffer));

    int new_magic8ball_count;
    status = kstrtoint(data.buffer, 10, &new_magic8ball_count);
    if (status)
    {
        printk(KERN_ERR "Could no convert magic8ball count '%s' (status code: %d)\n", data.buffer, status);
        goto Error;        
    }
    printk(KERN_INFO "Parsed magic8ball count: %d\n", new_magic8ball_count);
    data.magic8ball_count = new_magic8ball_count;

    mutex_unlock(&data.crit_sec_mutex);
    return len;

    Error:
        mutex_unlock(&data.crit_sec_mutex);
        return -EFAULT;

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static int eof[1];
static ssize_t magic8ball_read(struct file *file, char *buf, size_t len, loff_t *ppos)
#else
static int magic8ball_read(char *buf, char **start, off_t offset, int len, int *eof, void *unused)
#endif
{
    printk(KERN_INFO "magic8ball::read");
    mutex_lock(&data.crit_sec_mutex);

    int total_len = 0;
    int max_len = ( data.magic8ball_count * sizeof(char*) + data.magic8ball_count * MAGIG8BALL_BUF_LEN );
    printk(KERN_INFO "tmp buffer max len: %d", max_len);
    char* random_messages = (char*)vmalloc(max_len);
    if (!random_messages)
    {
        goto Error;
    }
    if (*eof) { 
        *eof = 0; 
    }
    else
    {
        *eof = 1;

        for (int magic8ball_num = 0; magic8ball_num < data.magic8ball_count; magic8ball_num++)
        {
            unsigned random_index;
            unsigned num_msg = MAGIC8BALL_NUM_MSG;
            char* random_msg;
            
            // () not needed with sizeof object
            get_random_bytes(&random_index, sizeof random_index); 

            // Best to use unsigned constants with unsigned objects.
            random_index = 0u + (random_index % num_msg);
            random_msg = data.messages[random_index];
            int random_msg_len = strnlen(random_msg, MAGIG8BALL_BUF_LEN);
            int actual_len = safe_copy(random_msg, random_msg_len, random_messages, total_len, max_len);
            printk(KERN_DEBUG "Got random message: %s (index: %u, len: %d, actual_len: %d)", random_msg, random_index, random_msg_len, actual_len);
            if (actual_len < 0)
            {
                printk(KERN_ERR "Error copying random message to result buffer");
                goto Error;
            }
            total_len += actual_len;
        }

        // Ensuring we have enough space for adding the 
        // string termination char and appending it to the result.
        if (total_len == max_len)
        {
            printk(KERN_ERR "Full buffer capacity reached: could not copy random messages");
            goto Error;
        }
        random_messages[total_len] = '\0';
        total_len += 1;

        printk(KERN_ERR "Copying data to user space");
        int status = copy_to_user(buf, random_messages, total_len);
        if (status)
        {
            printk(KERN_ERR "Error copying data to user space (status code: %d)", status);
            goto Error;
        }
    }


    mutex_unlock(&data.crit_sec_mutex);
    if (random_messages)
    {
        vfree(random_messages);
    }
    return total_len;

    Error:
        mutex_unlock(&data.crit_sec_mutex);
        if (random_messages)
        {
            vfree(random_messages);
        }
        return -EFAULT;
    
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static const struct proc_ops proc_fops = {
    .proc_read = magic8ball_read,
    .proc_write = magic8ball_write
};
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static const struct file_operations  proc_fops = {
    .owner = THIS_MODULE,
    .read = magic8ball_read,
    .write = magic8ball_write
};
#endif

// ----------------------------------------------------------------------------
// Lifecycle

static int __init magic8ball_init(void)
{
    printk(KERN_INFO "-> magic8ball::init\n");

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    proc_magic8ball = proc_create(MAGIG8BALL_PROC_NAME, 0777, NULL, &proc_fops);
    if(proc_magic8ball == NULL)
    {
        goto Error;
    }
    #else
    proc_magic8ball = create_proc_entry(MAGIG8BALL_PROC_NAME, 0, 0);
    if(proc_magic8ball == NULL)
    {
        goto Error;
    }
    proc_magic8ball->read_proc = magic8ball_read;
    proc_magic8ball->write_proc = magic8ball_write;
    #endif


    #if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,29)
    proc_magic8ball->owner = THIS_MODULE;
    #endif

    data.buffer = vmalloc(MAGIG8BALL_BUF_LEN);
    data.magic8ball_count = MAGIG8BALL_DEFAULT_MAGIG8BALL_COUNT;
    // Allocating message pointers in contiguous memory using kalloc.
    // Each individual message, for its part, is allocated in 
    // potentially non-contiguous (virtual) memory. 
    size_t messages_ptr_len = ( MAGIC8BALL_NUM_MSG * sizeof(char*) + MAGIC8BALL_NUM_MSG * sizeof(char) );
    printk(KERN_INFO "Allocating %d bytes to store message pointers", messages_ptr_len);
    data.messages = (char**) kmalloc(messages_ptr_len, GFP_KERNEL);
    if (!data.messages)
    {
        printk(KERN_ERR "Could not allocate memory for storing message pointers");
        goto Error;
    }
    for (int i = 0; i < MAGIC8BALL_NUM_MSG; i++)
    {
        data.messages[i] = (char*) vmalloc(MAGIG8BALL_BUF_LEN);
        if (!data.messages[i])
        {
            printk(KERN_ERR "Could not allocate memory for storing message string (index: %d)", i);
            goto Error;
        }
        switch(i)
        {
            case 0:
                strncpy(data.messages[i], MAGIG8BALL_MSG0, MAGIG8BALL_BUF_LEN);
                break;
            case 1:
                strncpy(data.messages[i], MAGIG8BALL_MSG1, MAGIG8BALL_BUF_LEN);
                break;
            case 2:
                strncpy(data.messages[i], MAGIG8BALL_MSG2, MAGIG8BALL_BUF_LEN);
                break;
            case 3:
                strncpy(data.messages[i], MAGIG8BALL_MSG3, MAGIG8BALL_BUF_LEN);
                break;
            case 4:
                strncpy(data.messages[i], MAGIC8BALL_MSG4, MAGIG8BALL_BUF_LEN);
                break;
            case 5:
                strncpy(data.messages[i], MAGIC8BALL_MSG5, MAGIG8BALL_BUF_LEN);
                break;
            case 6:
                strncpy(data.messages[i], MAGIC8BALL_MSG6, MAGIG8BALL_BUF_LEN);
                break;
            case 7:
                strncpy(data.messages[i], MAGIC8BALL_MSG7, MAGIG8BALL_BUF_LEN);
                break;
            case 8:
                strncpy(data.messages[i], MAGIC8BALL_MSG8, MAGIG8BALL_BUF_LEN);
                break;
            case 9:
                strncpy(data.messages[i], MAGIC8BALL_MSG9, MAGIG8BALL_BUF_LEN);
                break;
            case 10:
                strncpy(data.messages[i], MAGIC8BALL_MSG10, MAGIG8BALL_BUF_LEN);
                break;
            case 11:
                strncpy(data.messages[i], MAGIC8BALL_MSG11, MAGIG8BALL_BUF_LEN);
                break;
            case 12:
                strncpy(data.messages[i], MAGIC8BALL_MSG12, MAGIG8BALL_BUF_LEN);
                break;
            case 13:
                strncpy(data.messages[i], MAGIC8BALL_MSG13, MAGIG8BALL_BUF_LEN);
                break;
            case 14:
                strncpy(data.messages[i], MAGIC8BALL_MSG14, MAGIG8BALL_BUF_LEN);
                break;
            case 15:
                strncpy(data.messages[i], MAGIC8BALL_MSG15, MAGIG8BALL_BUF_LEN);
                break;
            case 16:
                strncpy(data.messages[i], MAGIC8BALL_MSG16, MAGIG8BALL_BUF_LEN);
                break;
            case 17:
                strncpy(data.messages[i], MAGIC8BALL_MSG17, MAGIG8BALL_BUF_LEN);
                break;
            case 18:
                strncpy(data.messages[i], MAGIC8BALL_MSG18, MAGIG8BALL_BUF_LEN);
                break;
            case 19:
                strncpy(data.messages[i], MAGIC8BALL_MSG19, MAGIG8BALL_BUF_LEN);
                break;
        }
        size_t actual_len = strnlen(data.messages[i], MAGIG8BALL_BUF_LEN);
        printk(KERN_INFO "Stored string #%d into dynamically allocted buffer (len=%d): %s", (i + 1), actual_len, data.messages[i]);
    }

    mutex_init(&data.crit_sec_mutex);
    printk(KERN_INFO "<- magic8ball::init\n");
    return 0;

    Error:
        printk(KERN_ERR "Error occurred. Aborting module initialization (could not create proc entry %s)\n", MAGIG8BALL_PROC_NAME);
        return -ENOMEM;
  
}

static void __exit magic8ball_exit(void)
{
    printk(KERN_INFO, "-> magic8ball::exit\n");
    if (data.buffer)
    {
        vfree(data.buffer);
    }
    if (data.messages)
    {
        for (int i = 0; i < MAGIC8BALL_NUM_MSG; i++)
        {
            if (data.messages[i])
            {
                vfree(data.messages[i]);
            }
        }
        kfree(data.messages);
    }
    if (proc_magic8ball)
    {
        remove_proc_entry(MAGIG8BALL_PROC_NAME, 0);
    }
    printk(KERN_INFO, "<- magic8ball::exit\n");
}

module_init(magic8ball_init);
module_exit(magic8ball_exit);

// ----------------------------------------------------------------------------

void mod_info(void)
{
    printk(KERN_INFO, "Proc name: %s, PID: %i\n", current->comm, current->pid);
}