SUMMARY = "Sensor Monitoring App"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

SRC_URI = "file://app.c"
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} app.c -o sensor-app
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 sensor-app ${D}${bindir}
}

FILES:${PN} = "${bindir}/sensor-app"
