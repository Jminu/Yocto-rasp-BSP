SUMMARY = "Device Tree for Sensor System"

SRC_URI = "\
    file://jmw-hd44780.dts \
    file://jmw-sht20.dts \
"

DEPENDS += "dtc-native"

inherit deploy

do_compile() {
    dtc -@ -I dts -O dtb -o jmw-hd44780.dtbo jmw-hd44780.dts
    dtc -@ -I dts -O dtb -o jmw-sht20.dtbo jmw-sht20.dts
}

do_install() {
    install -d ${D}/boot/overlays
    install -m 0644 jmw-hd44780.dtbo ${D}/boot/overlays/
    install -m 0644 jmw-sht20.dtbo ${D}/boot/overlays/
}

FILES:${PN} += "/boot/overlays/jmw-hd44780.dtbo"
FILES:${PN} += "/boot/overlays/jmw-sht20.dtbo"

