FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
  file://wpa_supplicant-wlan0.conf \
  file://wifi.service \
"

do_install:append() {
  # Install wpa_supplicant-wlan0.conf
  install -d ${D}${sysconfdir}/wpa_supplicant
  install -m 0644 ${WORKDIR}/wpa_supplicant-wlan0.conf \
    ${D}${sysconfdir}/wpa_supplicant/wpa_supplicant-wlan0.conf

  # Install wifi.service into the correct systemd directory
  install -d ${D}${systemd_system_unitdir}
  install -m 0644 ${WORKDIR}/wifi.service \
    ${D}${systemd_system_unitdir}/wifi.service
}

SYSTEMD_SERVICE:${PN} += "wifi.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
