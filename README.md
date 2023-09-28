Thermostat
==========

Build
=====
Prerequisites
-------------
Install pico sdk and toolchain according to getting started with raspberry pi pico document:
https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DPICO_BOARD=pico_w ..
make -j4

Docker
======
Prerequisites
-------------
docker

docker build -t development .
docker run -it --rm -v $(pwd -P):$(pwd -P) -w $(pwd -P) development sh -c 'mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug -DPICO_BOARD=pico_w .. && make -j4'

Debug
=====

using raspberry pi gpio as debug probe

pin 24 swclk
pin 25 swdio


install [Raspberry Pi OS Lite](https://www.raspberrypi.com/software/operating-systems/) with ssh and:

`sudo apt install openocd minicom`

openocd debug:

`sudo openocd -c "adapter driver bcm2835gpio" -c "bcm2835gpio speed_coeffs 146203 36" -c "adapter gpio swclk 25" -c "adapter gpio swdio 24" -c "adapter speed 1000" -c "bindto 0.0.0.0" -f target/rp2040.cfg`

gpio serial:

`minicom -b 115200 -o -D /dev/serial0`

usb serial:

`minicom -b 115200 -o -D /dev/ttyACM0`

wifi
====

Display
=======


Fonts
=====

Use the following string:

 !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ["]^_`abcdefghijklmnopqrstuvwxyz{|}~

xbm
---
only 8 pixels high, invert colours, make mode 1 bit and rotate 90˚ clockwise 

bmp
---
multiple of 32 pixels high, make mode 1 bit and rotate 90˚ anti-clockwise

Graphics
========
Support bmp as above




Acknowledgements
================

* AM2320 Driver
https://github.com/SimpleMethod/STM32-AM2320

* UC8151 Driver
https://github.com/BasicCode/UC8151c

* QR code
https://www.nayuki.io/page/qr-code-generator-library

* Fonts
https://jared.geek.nz/2014/jan/custom-fonts-for-microcontrollers

* Icons
https://iconify.design/