SUMMARY = "Displays Spotify Playback"
DESCRIPTION = "Custom recipe to build spotifydisplay.c application"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
  file://src/spotifydisplay.c \
  file://src/spotifydata.c \
  file://src/spotifydata.h \
  file://src/font8x8_basic.inl \
  file://spotifydisplay.service \
  file://src/stb_image.h \
"

DEPENDS += " curl jansson jpeg"

S = "${WORKDIR}/src"

TARGET_CC_ARCH += "${LDFLAGS}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "spotifydisplay.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
  ${CC} spotifydisplay.c spotifydata.c -o spotifydisplay \
    -lcurl -ljansson -ljpeg -lm
}

do_install() {
  # Binary application file
  install -d ${D}${bindir}
  install -m 0755 spotifydisplay ${D}${bindir}

  # systemd service file
  install -d ${D}${systemd_system_unitdir}
  install -m 0644 ${WORKDIR}/spotifydisplay.service \
    ${D}${systemd_system_unitdir}
}
