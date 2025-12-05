SUMMARY = "Sensor Monitoring App"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} app.c -o sensor-app
}

do_install() {
    install -d ${D}${bindir}

    install -m 0755 sensor-app ${D}${bindir}
}

