SUMMARY = "Sensor Monitoring App"
LICENSE = "GPL-2.0-only"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} app.c -o sensor-app
}

do_install() {
    install -d ${D}${bindir}

    install -m 0755 sensor-app ${D}${bindir}
}

