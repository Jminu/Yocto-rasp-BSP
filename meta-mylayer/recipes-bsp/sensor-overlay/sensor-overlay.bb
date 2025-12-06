SUMMARY = "Device Tree for Sensor System"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;801f80980d171dd6425610833a22dbe6"

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

addtask deploy before do_build after do_compile

do_deploy:append() {
        install -d ${DEPLOYDIR}/bcm2711-bootfiles
        install -m 0644 ${B}/jmw-hd44780.dtbo ${DEPLOYDIR}/bcm2711-bootfiles/
        install -m 0644 ${B}/jmw-sht20.dtbo ${DEPLOYDIR}/bcm2711-bootfiles/
    }
