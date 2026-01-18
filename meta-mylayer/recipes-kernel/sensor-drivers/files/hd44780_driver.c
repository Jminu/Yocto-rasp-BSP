#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/string.h>

#define DRIVER_NAME "hd44780_driver"
#define DEVICE_COUNT 1
#define DEVICE_NAME "hd44780_device"
#define CLASS_NAME "hd44780_class"

/*
 * ----- Expander for I2C bus -----
 * I2C slave address: 0100 A2 A1 A0 -> 0x27
 * Write mode: 0
 * Read mode: 1
 *
 * 총 3바이트에서 1바이트는 slave + address 이고
 * 나머지 2바이트는 데이터.
 *
 * 확장모듈과 HD44780은 D7 D6 D5 D4가 연결되어있다.
 */

/*
 * ----- LCD Moudle -----
 * E: Data Enable
 * 8 bit mode: DB0 ~ DB7 use
 * 4 bit mode: DB4 ~ DB7 use
 *
 * 기본 세팅
 * N=1: 2-line display
 * F=0: 5x8 dots font
 * D=1: display on
 * 위의 세팅은 LCD모듈이 켜질때, 명령을 내려야한다.
 *
 * LCD모듈에 전원이 인가되면, reset 루틴을 자동적으로 실행한다.
 * 이것은 약 50ms 정도가 걸린다. 이 reset 루틴 후,
 * LCD 모듈의 상태는 다음과 같이 된다.
 *
 * Display clear
 * DL=1: 8bit interface
 * N=0: 1-line display
 * F=0: 5x8 dot char display
 * D=0: Display off
 * C=0: Cursor off
 * B=0: Blinking off
 * I/D=1: Increment by 1
 * S=0: No Shift
 *
 * 이것이 reset routine이다.
 */

#define RS (1 << 0) // RS:0 명령, RS:1 데이터
#define RW (0 << 1) // RW:0 write, RW:1 read, 근데 어차피 쓰기만 할거임
#define E (1 << 2) // 펄스
#define BL (1 << 3) // 백라이트

#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_FUNCTIONSET 0x28 //4bit mode, 2line set
#define LCD_DISPLAYON 0xF // display on, cursor on, blink on
#define LCD_DISPLAYOFF 0x08
#define LCD_ENTRYMODESET 0x06

#define MODE_8BIT 0x30
#define MODE_4BIT 0x20

#define WRITE_MODE 0
#define READ_MODE 1

#define INST_MODE 0
#define DATA_MODE 1

static struct hd44780_device {
	struct i2c_client *client;
	dev_t dev_num;
	struct cdev hd44780_cdev;
	struct class *class;
};

static const struct of_device_id hd44780_of_match[] = {
	{.compatible = "jmw,hd44780"},
	{},
};
MODULE_DEVICE_TABLE(of, hd44780_of_match);

static int i2c_lcd_write_byte(struct i2c_client *client, u8 byte) {
	int ret;

	ret = i2c_smbus_write_byte(client, byte);
	if (ret < 0) {
		printk(KERN_ERR "i2c write fail\n");
		return -1;
	}

	return 0;
}

/*
 * 4비트(데이터)에 나머지 4비트(제어비트) 결합
 * 4bit를 보낸다.
 *
 * @mode: register set (RS)
 * 	- RS:0 명령 전송
 * 	- RS:1 데이터 전송
 * nibble: 4bit
 */
static void lcd_send_nibble(struct i2c_client *client, u8 data, u8 mode) {
	u8 byte_no_e = data | BL | mode; // data|1|0|0|0
	u8 byte_with_e = data | BL | E | mode; // data|1|1|0|0

	i2c_lcd_write_byte(client, byte_no_e); // 펄스 없는 바이트 보냄
	udelay(10);

	i2c_lcd_write_byte(client, byte_with_e); // 펄스 있는 바이트 보냄
	udelay(10);

	i2c_lcd_write_byte(client, byte_no_e); // 펄스 없는 바이트 보냄 -> 하강엣지에서 LCD에 데이터가 들어가게됨
	udelay(50);
}

/*
 * 4bit 모드에서, 1바이트를 LCD에 보내는데, 4비트를 2번 보낸다.
 */
static void lcd_send_byte(struct i2c_client *client, u8 data, u8 mode) {
	lcd_send_nibble(client, data & 0xF0, mode); // D4~D7 send
	lcd_send_nibble(client, (data << 4) & 0xF0, mode); // D0~D3 send
}

/*
 * @client: i2c slave 주소 (0x27)
 * @data: write할 데이터
 */
static void lcd_show_data(struct i2c_client *client, char data) {
	lcd_send_byte(client, data, DATA_MODE); // 0x01: RS=1
}

/*
 * 처음 LCD에 전원이 인가되면, 다음의 시퀀스를 따른다.
 *
 * @client: i2c slave의 주소 (0x27)
 */
static void lcd_init(struct i2c_client *client) {
	msleep(50); // 하드웨어 자동 리셋 대기

	// 처음에는 8bit 모드
	lcd_send_nibble(client, MODE_8BIT, INST_MODE); // 0x30: 0011 0000, 0x00: RS가 0(instruction mode)
	msleep(10);
	lcd_send_nibble(client, MODE_8BIT, INST_MODE);
	udelay(150);
	lcd_send_nibble(client, MODE_8BIT, INST_MODE);
	udelay(150);
	printk(KERN_INFO "8비트 모드로 변경\n");

	// 4bit 모드로 변경
	lcd_send_nibble(client, MODE_4BIT, INST_MODE); // 0x20: 0010 0000, 0x00: RS가 0(instruction mode)
	udelay(100);
	lcd_send_nibble(client, MODE_4BIT, INST_MODE);
	udelay(100);
	lcd_send_nibble(client, MODE_4BIT, INST_MODE);
	udelay(150);
	printk(KERN_INFO "4비트 모드로 변경\n");

	lcd_send_byte(client, LCD_DISPLAYON, INST_MODE);
	lcd_send_byte(client, LCD_CLEARDISPLAY, INST_MODE);
	lcd_send_byte(client, LCD_ENTRYMODESET, INST_MODE);

	printk(KERN_INFO "LCD init success\n");
}

static void lcd_print(struct i2c_client *client, const char *str, int len) {
	printk(KERN_INFO "hd 처음에는 8bit 모드44780_driver.c: recived string length: %d\n", len);

	int i = 0;
	for (i = 0; i < 16; i++) {
		if (*str == '\0') {
			lcd_show_data(client, ' ');
			continue;
		}
		lcd_show_data(client, *str++);
	}
}

static ssize_t hd44780_write(struct file *file, const char __user *buf, size_t len, loff_t *pos) {
	struct hd44780_device *hd44780 = file->private_data;
	char kbuf[32];

	if (len > 31)
		len = 31;

	int ret;
	ret = copy_from_user(kbuf, buf, len);
	if (ret < 0) {
		return -1;
	}

	kbuf[len] = '\0';

	lcd_write_cmd(hd44780->client, LCD_CLEARDISPLAY);
	lcd_print(hd44780->client, kbuf, strlen(kbuf));

	return len;
}

static int hd44780_open(struct inode *inode, struct file *file) {
	struct hd44780_device *hd44780;
	hd44780 = container_of(inode->i_cdev, struct hd44780_device, hd44780_cdev);
	file->private_data = hd44780;

	return 0;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = hd44780_open,
	.write = hd44780_write,
};


static int hd44780_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	struct hd44780_device *hd44780;
	int ret;

	hd44780 = devm_kzalloc(&client->dev, sizeof(struct hd44780_device), GFP_KERNEL);
	if (hd44780 < 0) {
		printk(KERN_ERR "devm kzalloc fail\n");
		return -1;
	}

	hd44780->client = client;

	i2c_set_clientdata(client, hd44780);

	lcd_init(client); // 초기화 작업

	ret = alloc_chrdev_region(&(hd44780->dev_num), 0, 1, DEVICE_NAME);
	if (ret < 0) {
		printk(KERN_ERR "alloc chrdev region fail\n");
		return -1;
	}

	cdev_init(&(hd44780->hd44780_cdev), &fops);
	ret = cdev_add(&(hd44780->hd44780_cdev), hd44780->dev_num, DEVICE_COUNT);
	if (ret < 0) {
		printk(KERN_ERR "cdev add fail\n");
		return -1;
	}

	hd44780->class = class_create(THIS_MODULE, CLASS_NAME);
	device_create(hd44780->class, NULL, hd44780->dev_num, NULL, DEVICE_NAME);

	printk(KERN_INFO "probe success\n");

	return 0;
}

static int  hd44780_remove(struct i2c_client *client) {
	struct hd44780_device *hd44780 = i2c_get_clientdata(client);

	device_destroy(hd44780->class, hd44780->dev_num);
	class_destroy(hd44780->class);
	cdev_del(&(hd44780->hd44780_cdev));
	unregister_chrdev_region(hd44780->dev_num, 1);

	printk(KERN_INFO "remove success\n");
	return 0;
}

static struct i2c_driver hd44780_driver = {
	.driver = {
		.name = "jmw_hd44780",
		.of_match_table = hd44780_of_match,
	},
	.probe = hd44780_probe,
	.remove = hd44780_remove,
};

module_i2c_driver(hd44780_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");
MODULE_DESCRIPTION("HD44780+extension module driver");
