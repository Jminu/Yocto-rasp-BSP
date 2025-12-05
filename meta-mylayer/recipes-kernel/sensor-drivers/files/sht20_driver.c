#include <linux/kernel.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#define DRIVER_NAME "sht20_driver"
#define DEVICE_COUNT 1
#define CLASS_NAME "sht20_class"
#define DEVICE_NAME "sht20_device"

#define TEMP_MEASUREMENT 0xE3 // temp measurement command
#define HUMID_MEASUREMENT 0xE5 // humid measurement command
#define WRITE_USER_REGISTER 0xE6
#define READ_USER_REGISTER 0xE7
#define SOFT_RESET 0xFE // soft reset command

static struct sht20_device {
	struct i2c_client *client; // i2c에 연결된 칩 인식
	dev_t dev_num;
	struct cdev sht20_cdev;
	struct class *class;
	int temp;
	int humid;
};

// 연관된 dtbo file을 찾기위함
static const struct of_device_id sht20_ids[] = {
	{.compatible = "jmw,sht20"}, // .dts와 일치
	{},
};
MODULE_DEVICE_TABLE(of, sht20_ids);

/*
 * Do soft reset
 * @client: target device(SHT20)
 */
static int sht20_soft_reset(struct i2c_client *client) {
	int ret = i2c_smbus_write_byte(client, SOFT_RESET); // write SOFT_RESET command to SHT20
	if (ret < 0) {
		printk(KERN_ERR "i2c smbus write fail\n");
		return -1;
	}
	msleep(100);

	return ret;
}

/*
 * Read data from SHT20
 * @client: target device(SHT20): client->addr (chip address: 0x40)
 * @command: TEMP_MEASUREMENT = 0xE3
 * 			 HUMID_MEASUREMENT = 0xE5
 * @val: variable to store the read value
 */
static int sht20_read_data(struct i2c_client *client, int command, int *val) {
	int ret;
	u8 buf[3]; // 데이터 받을 unsigned char 3byte

	ret = i2c_smbus_write_byte(client, command); // write command to sht20
	if (ret < 0) {
		printk(KERN_ERR "i2c_smbus_write_byte Fail\n");
		return -1;
	}

	msleep(100);

	ret = i2c_master_recv(client, buf, 3); // SHT20으로부터 word만큼 데이터 읽음(3byte)
	if (ret < 0) {
		printk(KERN_ERR "i2c_master_recv Fail\n");
		return -1;
	}
	
	*val = (buf[0] << 8) | (buf[1] & 0xFC); // buf[1]에서 하위 2비트는 stat비트이기 때문에 무시

	return 0;
}

/*
 * 유저가 read했을때 이 함수가 실행
 */
static ssize_t sht20_read(struct file *file, char __user *buf, size_t len, loff_t *pos) {
	struct sht20_device *sht20 = file->private_data;
	int temp_raw;
	int humid_raw;
	char kbuf[64];

	printk(KERN_INFO "sht20 read\n");

	if (*pos > 0) {
		printk(KERN_ERR "pos err\n");
		return -1;
	}

	int ret;

	ret = sht20_read_data(sht20->client, TEMP_MEASUREMENT, &temp_raw); // 0x40 chip address를 대상으로 온도 측정 명령
	if (ret < 0) {
		printk(KERN_ERR "Temp measurement fail\n");
		return -1;
	}

	ret = sht20_read_data(sht20->client, HUMID_MEASUREMENT, &humid_raw); // 0x40 chip address를 대상으로 습고 측정 명령
	if (ret < 0) {
		printk(KERN_ERR "Humid measurement fail\n");
		return -1;
	}

	len = snprintf(kbuf, sizeof(kbuf), "%d|%d", temp_raw, humid_raw);
	printk(KERN_INFO "%s\n", kbuf);

	copy_to_user(buf, kbuf, len);

	return len;
}

static int sht20_open(struct inode *inode, struct file *file) {
	struct sht20_device *sht20;
	sht20 = container_of(inode->i_cdev, struct sht20_device, sht20_cdev);
	file->private_data = sht20; // 센서 데이터에 접근가능 ex)temp
	
	return 0;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = sht20_read,
	.open = sht20_open,
};


static int sht20_probe(struct i2c_client *client) {
	struct sht20_device *sht20;
	int ret;

	sht20 = devm_kzalloc(&client->dev, sizeof(struct sht20_device), GFP_KERNEL); // sht20을 위한 kernel공간 할당
	if (sht20 == NULL) {
		printk(KERN_ERR "devm_kzalloc fail\n");
		return -1;
	}

	sht20->client = client; // 실제 칩을 연결(client)
	
	/*
	 * @client: i2c_client구조체안에 dev가 존재, 그 dev안에 driver_data
	 * @sht20: driver_data안에 넣을  데이터
	 */
	i2c_set_clientdata(client, sht20); // 종료되어도 sht20의 상태를 알 수 있음

	ret = sht20_soft_reset(client); // soft reset 명령 write
	if (ret < 0)
		return ret;

	// create char dev, device, class
	ret = alloc_chrdev_region(&(sht20->dev_num), 0, 1, DEVICE_NAME);
	if (ret != 0) {
		printk(KERN_ERR "alloc chrdev region fail\n");
		return -1;
	}

	cdev_init(&(sht20->sht20_cdev), &fops);
	ret = cdev_add(&(sht20->sht20_cdev), sht20->dev_num, DEVICE_COUNT);
	if (ret < 0) {
		printk(KERN_ERR "cdev add fail\n");
		return -1;
	}

	sht20->class = class_create(CLASS_NAME);
	device_create(sht20->class, NULL, sht20->dev_num, NULL, DEVICE_NAME);

	return 0;
}

static void sht20_remove(struct i2c_client *client) {
	struct sht20_device *sht20 = i2c_get_clientdata(client);

	device_destroy(sht20->class, sht20->dev_num);
	class_destroy(sht20->class);
	cdev_del(&(sht20->sht20_cdev));
	unregister_chrdev_region(sht20->dev_num, 1);

	return;
}

static struct i2c_driver sht20_driver = {
	.driver = {
		.name = "jmw_sht20",
		.of_match_table = sht20_ids,
	},
	.probe = sht20_probe,
	.remove = sht20_remove,
};

module_i2c_driver(sht20_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");
MODULE_DESCRIPTION("SHT20 sensor driver");
