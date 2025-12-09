# Spotify Display (Yocto + Raspberry Pi)

A small embedded display that shows Now Playing information from Spotify:  
album art, track title, artist, play/pause status, repeat and shuffle state.

This project is designed to work together with my other project:

**RFID + Spotify controller (required):**
https://github.com/danishtran/rfidSpotifyPlayer

That repo runs on a seperate Raspberry Pi, talks to Spotify, uses
a small Flask API.

This repo builds a Yocto-based image for a second Raspberry Pi
that:

- boots straight into a full-screen C app
- reads JSON state from the controller Pi over HTTPS
- decodes album art, renders it on an SPI framebuffer LCD
- displays scrolling track & artist text plus playback status

## Hardware

- Raspberry Pi 4 Model B (1 GB)
- Inland 3.5" TFT LCD Touch Screen  
  - Resolution: 480 × 320  
  - Driver: ILI9486 (SPI)
- Wi-Fi network (both Pis on the same LAN)
- Optional (for debugging):
  - USB-TTL serial adapter for UART console
  - SSH client to manage the device over the network

## Prerequisites
- A working Yocto/Poky (kirkstone) setup to build an image for the Raspberry Pi 4.
- rfidSpotifyPlayer running on another machine / Pi on the same network: https://github.com/danishtran/rfidSpotifyPlayer
- That project should use the /state endpoint (Flask server) which returns current Spotify playback state.

**Required Yocto layers**
- poky (meta, meta-poky, meta-yocto-bsp)
- meta-openembedded
- meta-raspberrypi
- This repo’s meta-custom

## Repo Layout
```
spotifyDisplay/
  LICENSE
  README.md
  yocto
    build-rpi
      conf
      bblayers.conf
      local.conf
    meta-custom
      conf
        .conf
      recipes-apps
        myspotifydisplay
          spotifydisplay_0.1.bb
          files
            spotifydisplay.service
            src
              font8x8_basic.inl
              spotifydata.c
              spotifydata.h
              spotifydisplay.c
              stb_image.h
      recipes-bsp
        rpi-config
          rpi-config_%.bbappend
          spi.txt
      recipes-connectivity
        wpa-supplicant
          wpa-supplicant_%.bbappend
          files
            wifi.service
            wpa_supplicant-wlan0.conf
      recipes-core
        dropbear
          dropbear.service
          dropbear_%.bbappend
        images
          custom-image.bb
      recipes-kernel
        linux
          linux-raspberrypi_%.bbappend
          raspberrypi4-64
            0002-add-spi-ili9486-display.patch
            spi.cfg
            wifi.cfg
```

## Configuration
**Wi-Fi**

Edit the Wi-Fi config in the custom layer before building (Password must be hashed):

```yocto/meta-custom/recipes-connectivity/wpa-supplicant/files/wpa_supplicant-wlan0.conf```

Set SSID and Password so the Pi can join the network at boot.

**Spotify state endpoint**

In spotifydisplay.c, the Pi recieve the Flask API:

```const char *API_URL = "IP/state";```

Set the IP (or hostname) of the machine running rfidSpotifyPlayer Project.

**Set up Path**

in bblayers.conf change the ```{Path}``` to fit the right path to the repo

**Building the Image**

From your Yocto/Poky tree (adjust paths to match your setup):

## From the poky root
```source oe-init-build-env yocto/build-rpi```

## Build custom image including spotifydisplay and dependencies
```bitbake custom-image```

The resulting .wic.bz2 image will be in:

```yocto/build-rpi/tmp/deploy/images/raspberrypi4-64/```

Flash it to an SD card using bmaptool.

```sudo bmaptool copy (image)-(date).rootfs.wiz.bz2 /dev/sd*```

## Access over UART and SSH
### UART console
The image is set up with serial console on the Pi’s UART pins; plug in  a USB-TTL adapter (115200) to get debugging information.

### SSH
SSH using Dropbear. Once Wi-Fi is setup:

```
ssh root@raspberrypi4-64.local (or root@IPAddress)
Password default = 'toor'
```
