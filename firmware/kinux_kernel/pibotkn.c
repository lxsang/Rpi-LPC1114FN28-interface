
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#define DEVICE_NAME "pibot0"
#define CLASS_NAME "pibot"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xuan Sang LE");
MODULE_DESCRIPTION("Char device drive for reading LPC1114FN28 buffer from Rasberry Pi");
MODULE_VERSION("0.1.10a");

static int major_number;
static char message[256] = {0};
static short size_of_message;
static int open_cnt = 0;
static struct class *char_class = NULL;
static struct device *char_device = NULL;

static DEFINE_MUTEX(pibot_mutex);

/// The prototype functions for the character driver -- must come before the struct definition
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

/**
 * Devices are represented as file structure in the kernel. The file_operations structure from
 * /linux/fs.h lists the callback functions that you wish to associated with your file operations
 * using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
    {
        .open = dev_open,
        .read = dev_read,
        .write = dev_write,
        .release = dev_release,
};

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init pibot_init(void)
{
    printk(KERN_INFO "PIBOT: Initializing the device\n");

    // Try to dynamically allocate a major number for the device -- more difficult but worth it
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0)
    {
        printk(KERN_ALERT "PIBOT failed to register a major number\n");
        return major_number;
    }
    printk(KERN_INFO "PIBOT: registered correctly with major number %d\n", major_number);

    // Register the device class
    char_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(char_class))
    {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "PIBOT: Failed to register device class\n");
        return PTR_ERR(char_class);
    }
    printk(KERN_INFO "PIBOT: device class registered correctly\n");

    // Register the device driver
    char_device = device_create(char_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(char_device))
    {
        class_destroy(char_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(char_device);
    }
    printk(KERN_INFO "PIBOT: device class created correctly\n");
    mutex_init(&pibot_mutex);
    return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit pibot_exit(void)
{
    mutex_destroy(&pibot_mutex);
    device_destroy(char_class, MKDEV(major_number, 0));
    class_unregister(char_class);
    class_destroy(char_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "PIBOT: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the open_cnt counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep)
{

    if (!mutex_trylock(&pibot_mutex))
    {
        printk(KERN_ALERT "PIBOT: Device in use by another process");
        return -EBUSY;
    }
    open_cnt++;
    printk(KERN_INFO "PIBOT: Device has been opened %d time(s)\n", open_cnt);
    return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
    int error_count = 0;
    int ret = size_of_message;
    // copy_to_user has the format ( * to, *from, size) and returns 0 on success
    error_count = copy_to_user(buffer, message, size_of_message);
    if (error_count == 0)
    {
        printk(KERN_INFO "PIBOT: Sent %d characters to the user\n", size_of_message);
        size_of_message = 0;
        return ret;
    }
    else
    {
        printk(KERN_INFO "PIBOT: Failed to send %d characters to the user\n", error_count);
        return -EFAULT;
    }
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using message[x] = buffer[x]
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
    char tmp[len + 1];
    memcpy(tmp,buffer,len);
    tmp[len] = '\0';
    sprintf(message, "%s(%zu letters)", tmp, len);
    size_of_message = strlen(message);
    printk(KERN_INFO "PIBOT: received '%s' \n", tmp);
    printk(KERN_INFO "PIBOT: Received %zu characters from the user\n", len);
    return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep)
{
    mutex_unlock(&pibot_mutex);
    printk(KERN_INFO "PIBOT: Device successfully closed\n");
    return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(pibot_init);
module_exit(pibot_exit);
