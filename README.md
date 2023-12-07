# HyperTouch 4.0 Installation

(Also compatible for HyperPixel 4.0 rectangle, but without backlight control)

HyperTouch 4.0 kernel driver

Guide for raspberry pi 

## Warning

Please mate the extension header with the display pcb only one time. When you dissassemble the extension header from the display,
it can happen that you accidentially remove some goldpins and maybe loose some. When you remove the Raspberry Pi from the display please hold the extension header plastic during that process.

## FULL AUTOMATIC INSTALL *BETA*

```
wget https://raw.githubusercontent.com/shpi/hypertouch40/main/hypertouch40-install
sudo bash hypertouch40-install
```

## Manual Install

### 1. Prerequisites

Update system and reboot:
```bash
sudo apt-get update
sudo apt-get upgrade
sudo reboot
```

The reboot is necessary for dkms finding the current kernel-headers.


```bash
sudo apt-get install dkms git raspberrypi-kernel-headers i2c-tools
```

### 2. Clone repository into home directory

```bash
cd
git clone https://github.com/shpi/hypertouch40
```

### 3. Install

```bash
cd hypertouch40
sudo ln -s $HOME/hypertouch40 /usr/src/hypertouch40-1.0
sudo dkms install hypertouch40/1.0
```

### 4. Compile dtb


Compile `hypertouch40.dts` into `/boot/overlays/hypertouch40.dtbo`:
```bash
sudo dtc -I dts -O dtb -o /boot/overlays/hypertouch40.dtbo hypertouch40.dts
```

### 5. Update /boot/config.txt

Modify `config.txt` to enable and configure the drivers. Please make sure spi, i2c, display autodetect is removed before. You can also try our config.txt in this repository, if it doesn't work.

```bash
sudo nano /boot/config.txt
```

```bash
# Comment out or delete the internal GPIO interfaces because DPI requires most of the pins itself.
#dtparam=i2c_arm=on
#dtparam=i2s=on
#dtparam=spi=on


# Include and configure hypertouch driver
dtoverlay=hypertouch40
enable_dpi_lcd=1
dpi_group=2
dpi_mode=87
dpi_output_format=0x7f216
dpi_timings=480 0 10 16 59 800 0 15 113 15 0 0 0 60 0 32000000 6

# Disable (comment out) HDMI output driver.
# Dual output is probably not supported on all models.
#dtoverlay=vc4-kms-v3d

# Reduce framebuffers to 1 for the hypertouch display. Saves RAM.
max_framebuffers=1


# Select one orientation below!

# Standard: Landscape, top is at pin header:
display_rotate=3
dtparam=touchscreen-swapped-x-y
dtparam=touchscreen-inverted-x


# 180° rotated: Landscape, top is on the opposite of pin header
# display_rotate=1
# dtparam=touchscreen-swapped-x-y
# dtparam=touchscreen-inverted-y


# 270° rotated: Portrait, top is on opposite of flex connector cable
# display_rotate=0


# 90° rotated: Portrait, top is at flex connector cable
# display_rotate=2
# dtparam=touchscreen-inverted-y
# dtparam=touchscreen-inverted-x

```

### 6. reboot

Reboot the pi after a change to config.txt to apply the new settings.

```bash
sudo reboot
```


## Control backlight (min:0 max:31)

Our kernel module provides a fully compliant backlight interface that conforms to the Linux subsystem. All known backlight controls and features will work. However if you want to manually control the backlight, you can do that via command line also:

```bash
echo 31 | sudo tee /sys/class/backlight/soc\:backlight/brightness
```
## Issues with touch? Please run detection

i²c address of the touchscreen varies between `0x14` and `0x5d` depending on raspberry version.



**Manual method**: 
Find the correct i²c address:
```bash
i2cdetect -y 11
```

If I²C is not available, activate by adding i2c-dev and i2c-bcm2708 in /etc/modules

```
echo "i2c-dev" | sudo tee -a /etc/modules
```

If I²C-GPIO is not active yet, you can do it on the fly:

```
dtoverlay i2c-gpio i2c_gpio_sda=10 i2c_gpio_scl=11 bus=11
```

If **14** is highlighted, the address is set correctly already. Proceed with compile step.

If **5d** is highlighted, edit `hypertouch40.dts` with a text editor and replace the addresses to **5d** on line 57 and 59:
```
            ft6236: ft6236@5d {
...
                reg = <0x5d>;
```

Compile `hypertouch40.dts` into `/boot/overlays/hypertouch40.dtbo` again:
```bash
sudo dtc -I dts -O dtb -o /boot/overlays/hypertouch40.dtbo hypertouch40.dts
```
