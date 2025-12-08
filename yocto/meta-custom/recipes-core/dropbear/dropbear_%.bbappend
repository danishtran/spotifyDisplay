FILESEXTRAPATHS:prepend := "${THISDIR}:"

SRC_URI += " file://dropbear.service "

do_install:append() {
  install -Dm0644 ${WORKDIR}/dropbear.service ${D}${systemd_system_unitdir}/dropbear.service

  rm -f ${D}${systemd_system_unitdir}/dropbear.socket
  rm -f ${D}${systemd_system_unitdir}/dropbearkey.service
}

SYSTEMD_SERVICE:${PN} = "dropbear.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
