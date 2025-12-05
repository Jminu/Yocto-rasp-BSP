SUMMARY = "Custom Sensor Drivers By Jin Minu"
LICENSE = "GPL-2.0-only"

inherit module

SRC_URI = "file://Makefile \
    file://irq_btn_driver.c \
    file://sht20_driver.c \
    file://hd44780_driver.c \
"

S = "${WORKDIR}"

KERNEL_MODULE_AUTOLOAD += "irq_btn_driver \
    sht20_driver \
    hd44780_driver \
"

