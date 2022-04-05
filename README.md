# HyperTouch 4.0 Kernel driver

(also compatible for HyperPixel 4.0 rectangle, but without backlight control)

HyperTouch 4.0 kernel driver

guide for raspberry pi 

## 1. prerequisites
```bash
sudo apt-get install dkms git raspberrypi-kernel-headers
```

## 2. clone repository

```bash
git clone https://github.com/shpi/hypertouch40
```

## 3. install
```bash
cd hypertouch40
   sudo ln -s /home/pi/hypertouch40 /usr/src/hypertouch40-1.0
   sudo dkms install hypertouch40/1.0 
   sudo dtc -I dts -O dtb -o /boot/overlays/hypertouch40.dtbo hypertouch40.dts
   ```
   

## 4. update /boot/config.txt

add following lines to config.txt

```bash
sudo nano /boot/config.txt
```

```bash
display_rotate=3
enable_dpi_lcd=1
dpi_group=2
dpi_mode=87
dpi_output_format=0x7f216
dpi_timings=480 0 10 16 59 800 0 15 113 15 0 0 0 60 0 32000000 6
dtoverlay=hypertouch40
dtparam=touchscreen-swapped-x-y
dtparam=touchscreen-inverted-x
```
## 5. reboot

Its necessary to reboot the pi, after changing config.txt

```bash
sudo reboot
```


## Control backlight (min:0 max:31)

Our kernel module provides a fully compliant backlight interface that conforms to the Linux subsystem. All known
backlight controls and features will work. However if u want to manually control the backlight,
u can do that via commandline.

```bash
echo 31 | sudo tee /sys/class/backlight/soc\:backlight/brightness
```


