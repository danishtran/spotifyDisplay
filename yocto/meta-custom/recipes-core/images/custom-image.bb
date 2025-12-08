SUMMARY = "My custom Linux image."

LICENSE = "MIT"

IMAGE_FEATURES += "splash"

# SSH Server
IMAGE_INSTALL:append = " \
  dropbear \
"

# Auto Start
SYSTEMD_AUTO_ENABLE:append:dropbear = " dropbear.service"
SYSTEMD_AUTO_ENABLE:append:dropbear = " enable"

# WiFi Support
IMAGE_INSTALL:append = " \
  wpa-supplicant \
  wpa-supplicant-passphrase \  
  iw \
  linux-firmware-bcm43455 \
"

# SPI ILI9481 display FBTFT
IMAGE_INSTALL:append = " \
  kernel-modules \
  spidev-test \
  libgpiod \
  libgpiod-tools \
"

# HTTP requests and JSON parsing
IMAGE_INSTALL:append = " curl jansson"

# Custom Application Display
IMAGE_INSTALL:append = " spotifydisplay"

SYSTEMD_AUTO_ENABLE:append = " spotifydisplay.service"

inherit core-image
inherit extrausers

EXTRA_USERS_PARAMS ="\
  usermod -p '\$6\$AsXUWH.qySkunx0G\$x1agr5clZ.EXo1Af7kGdeM7dhQl1JldWlGlp0iog3uoKP468jScbXFIV9x6GGvN4rkU8KapCQoWJmvxE8uklv/' root; \
  "
