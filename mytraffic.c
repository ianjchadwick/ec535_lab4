#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h>
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/uaccess.h>/* copy_from/to_user */
#include <linux/sched.h> /* timers and jiffy macros */
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Ian Chadwick & Sophie Evans");
MODULE_DESCRIPTION("Traffic light controller.");

/***************************************************************/
/*                    Function Declarations                    */
/***************************************************************/
static ssize_t mytraffic_write(struct file *flip, const char *buffer, 
                              size_t count, loff_t *f_pos);
static int __init traffic_init(void);
static void __exit traffic_exit(void);
static int mytraffic_open(struct inode* inode, struct file* file);
static int mytraffic_print(struct seq_file *m, void*v);
static void green_callback(struct timer_list *timer);
static void yellow_callback(struct timer_list *timer);
static void red_callback(struct timer_list *timer);
static void fred_callback(struct timer_list *timer);
static void fellow_callback(struct timer_list *timer);
static void off_callback(struct timer_list *timer);

/***************************************************************/
/*                    Defines, Variables and Structs           */
/***************************************************************/
#define ON  1
#define OFF 0

/*Update file ops struct with pointers to correct handlers*/
struct file_operations mytraffic_fops = 
{
    write: mytraffic_write,
    .open = mytraffic_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int pedestrian = 0;
static int cycle_rate = 1;
static int op_mode = 0;
static char op_mode_out[16] = {0}, 
            green_status[4] = {0},
            yellow_status[4] = {0},
            red_status[4] = {0},
            pedestrian_status[12] = {0};

static unsigned int redLED      = 67;
static unsigned int yellowLED   = 68;
static unsigned int greenLED    = 44;


const unsigned max_output = 1024;
static int major_num = 61;

static struct timer_list timer;

//Assign init and exit functions for module
module_init(traffic_init);
module_exit(traffic_exit);


/***************************************************************/
/*                    Function Declarations                    */
/***************************************************************/
static int __init traffic_init(void)
{
    int result;
    /*Register device*/
    result = register_chrdev(major_num, "mytraffic", &mytraffic_fops);
    if (result < 0)
    {
        printk(KERN_ALERT "Failed to register device. Cannot obtain major number %d.\n", major_num);
        goto fail;
    }

    timer_setup(&timer, green_callback, 0);
    mod_timer(&timer, jiffies + 3*(HZ/cycle_rate));

    gpio_request(greenLED, "sysfs");
    gpio_direction_output(greenLED, ON);
    gpio_export(greenLED, false);

    gpio_request(redLED, "sysfs");
    gpio_direction_output(redLED, OFF);
    gpio_export(redLED, false);

    gpio_request(yellowLED, "sysfs");
    gpio_direction_output(yellowLED, OFF);
    gpio_export(yellowLED, false);
    
    strcpy(op_mode_out, "normal");
    strcpy(red_status, "OFF");
    strcpy(yellow_status, "OFF");
    strcpy(green_status, "ON");
    strcpy(pedestrian_status, "Not Waiting");

    printk(KERN_ALERT "Initializing mytraffic module.\n");
    return 0;

fail:
    traffic_exit();
    return result;
}

static void __exit traffic_exit(void)
{
    gpio_set_value(greenLED, OFF);
    gpio_unexport(greenLED);
    gpio_free(greenLED);
    gpio_set_value(yellowLED, OFF);
    gpio_unexport(yellowLED);
    gpio_free(yellowLED);
    gpio_set_value(redLED, OFF);
    gpio_unexport(redLED);
    gpio_free(redLED);
    unregister_chrdev(major_num, "mytraffic");
    printk(KERN_ALERT "Exiting mytraffic module.\n");
    del_timer(&timer);
}

static ssize_t mytraffic_write(struct file *flip, const char *buffer, 
                              size_t count, loff_t *f_pos)
{
    return 0;
}

static int mytraffic_open(struct inode *inode, struct file *file)
{
    return single_open(file, mytraffic_print, NULL);
}

static int mytraffic_print(struct seq_file *m, void*v)
{    
    seq_printf(m,"Operational Mode: %s\nCycle Rate: %dHz\nRed Light Status:%s\nYellow Light Status:%s\nGreen Light Status:%s\nPedestrian Status:%s\n", 
               op_mode_out, cycle_rate, red_status, yellow_status, green_status, pedestrian_status);
    return 0;
}

static void green_callback(struct timer_list *timer)
{
    timer_setup(timer, yellow_callback, 0);
    mod_timer(timer, jiffies + 1*(HZ/cycle_rate));
    strcpy(green_status, "OFF");
    gpio_set_value(greenLED, OFF);
    strcpy(yellow_status, "ON");
    gpio_set_value(yellowLED, ON);
}

static void yellow_callback(struct timer_list *timer)
{
    if (op_mode == 0)
    {
        if (!pedestrian)
        {
            timer_setup(timer, red_callback, 0);
            mod_timer(timer, jiffies + 2*(HZ/cycle_rate));
            strcpy(yellow_status, "OFF");
            gpio_set_value(yellowLED, OFF);
            strcpy(red_status, "ON");
            gpio_set_value(redLED, ON);
        }
        else
        {
            //FIX ME!!
            timer_setup(timer, green_callback, 0);
            mod_timer(timer, jiffies + 3*(HZ/cycle_rate));
            strcpy(yellow_status, "OFF");
            strcpy(green_status, "ON");
        }
    } 
    else
    {
        timer_setup(timer, off_callback, 0);
        mod_timer(timer, jiffies + 1*(HZ/cycle_rate));
        strcpy(red_status, "OFF");
        strcpy(green_status, "OFF");
        strcpy(yellow_status, "OFF");
    }
}

static void red_callback(struct timer_list *timer)
{
    timer_setup(timer, green_callback, 0);
    mod_timer(timer, jiffies + 3*(HZ/cycle_rate));
    gpio_set_value(redLED, OFF);
    strcpy(red_status, "OFF");
    strcpy(green_status, "ON");
    gpio_set_value(greenLED, ON);
}

static void fred_callback(struct timer_list *timer)
{
    timer_setup(timer, off_callback, 0);
    mod_timer(timer, jiffies + 1*(HZ/cycle_rate));
    strcpy(yellow_status, "OFF");
    strcpy(red_status, "ON");
}

static void fellow_callback(struct timer_list *timer)
{
    timer_setup(timer, off_callback, 0);
    mod_timer(timer, jiffies + 1*(HZ/cycle_rate));
    strcpy(green_status, "OFF");
    strcpy(yellow_status, "ON");
}

static void off_callback(struct timer_list *timer)
{
    if (op_mode == 0)
    {
        timer_setup(timer, green_callback, 0);
        mod_timer(timer, jiffies + 3*(HZ/cycle_rate));
        strcpy(red_status, "OFF");
        strcpy(yellow_status, "OFF");
        strcpy(green_status, "ON");
    } 
    else if (op_mode == 1)
    {
        timer_setup(timer, fred_callback, 0);
        mod_timer(timer, jiffies + 1*(HZ/cycle_rate));
        strcpy(red_status, "ON");
        strcpy(green_status, "OFF");
        strcpy(yellow_status, "OFF");
    }
    else
    {
        timer_setup(timer, fellow_callback, 0);
        mod_timer(timer, jiffies + 1*(HZ/cycle_rate));
        strcpy(red_status, "OFF");
        strcpy(green_status, "OFF");
        strcpy(yellow_status, "ON");
    }
}