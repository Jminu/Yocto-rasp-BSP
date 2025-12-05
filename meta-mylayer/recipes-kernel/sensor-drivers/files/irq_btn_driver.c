#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define BTN 538
#define IRQ_NAME "button irq"
#define DEVICE_NAME "button_device"
#define DRIVER_NAME "button_driver"
#define CLASS_NAME "button_class"

static int irq_num;
static dev_t dev_num;
static struct cdev btn_cdev;
static struct device *btn_dev;
static struct class *class;
static char msg = '0';

static DECLARE_WAIT_QUEUE_HEAD(wq);
static int flag = 0;

static irqreturn_t irq_btn_handler(int irq, void *data) {
	flag = 1;
	printk(KERN_INFO "Button pushed!\n");
	wake_up_interruptible(&wq); // wait queue에 들어가있는 태스크 깨움
	return IRQ_HANDLED;
}

static ssize_t read_btn(struct file *file, char __user *buf, size_t len, loff_t *pos) {
	wait_event_interruptible(wq, flag != 0); // wait queue로 들어감
	printk(KERN_INFO "read_btn occur\n");
	
	flag = 0;
	if (msg == '0')
		msg = '1';
	else
		msg = '0';

	int ret;
	ret = copy_to_user(buf, &msg, 1); // 문자 1을 유저 단으로 보냄
	if (ret < 0) {
		printk(KERN_ERR "copy to user fail\n");
		return -1;
	}

	return 1;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = read_btn,
};

static int make_chrdev(void) {
	int ret;
	
	ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (ret != 0) {
		printk(KERN_ERR "alloc chrdev region err\n");
		return -1;
	}

	cdev_init(&btn_cdev, &fops);
	ret = cdev_add(&btn_cdev, dev_num, 1);
	if (ret != 0) {
		printk(KERN_ERR "cdev add fail\n");
		return -1;
	}

	class = class_create(CLASS_NAME);
	btn_dev = device_create(class, NULL, dev_num, NULL, DEVICE_NAME);

	printk(KERN_INFO "create device success\n");
	return 0;
}

static int __init btn_init(void) {
	int ret;
	
	irq_num = gpio_to_irq(BTN);
	if (irq_num < 0) {
		printk(KERN_ERR "gpio to irq fail\n");
		return -1;
	}

	ret = request_irq(irq_num, irq_btn_handler, IRQF_TRIGGER_RISING, IRQ_NAME, NULL);
	if (ret < 0) {
		printk(KERN_ERR "request irq fail\n");
		return -1;
	}

	ret = make_chrdev();
	if (ret == -1) {
		printk(KERN_ERR "create cdev error\n");
		return -1;
	}

	printk(KERN_INFO "btn module init\n");
	return 0;
}

static void __exit btn_exit(void) {
	free_irq(irq_num, NULL);
	device_destroy(class, dev_num);
	class_destroy(class);
	cdev_del(&btn_cdev);
	unregister_chrdev_region(dev_num, 1);
	return;
}

module_init(btn_init);
module_exit(btn_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");
MODULE_DESCRIPTION("BUTTON IRQ DRIVER");
