FILESEXTRAPATHS:prepend := "${THISDIR}/raspberrypi4-64:"
SRC_URI += " \
  file://0002-add-spi-ili9486-display.patch \
  file://spi.cfg \
  file://wifi.cfg \
"
