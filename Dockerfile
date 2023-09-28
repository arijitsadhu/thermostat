FROM debian:11.7

ARG SDK_PATH=/usr/share/pico_sdk USERNAME=devcontainer

RUN apt-get update &&\
    DEBIAN_FRONTEND=noninteractive apt-get install -y locales python3-pip python3-venv git build-essential cmake automake autoconf texinfo libtool libftdi-dev libusb-1.0-0-dev gcc-arm-none-eabi libnewlib-arm-none-eabi gdb-multiarch binutils-multiarch &&\
    apt clean &&\
    rm -rf /var/lib/apt/lists/* /tmp/* /usr/share/doc/* /usr/share/info/* /var/tmp/* &&\
    localedef -i en_GB -c -f UTF-8 -A /usr/share/locale/locale.alias en_GB.UTF-8 &&\
    adduser --disabled-password --gecos '' --shell=/bin/bash $USERNAME &&\
    ln -s /usr/bin/objdump /usr/bin/objdump-multiarch &&\
    ln -s /usr/bin/nm /usr/bin/nm-multiarch &&\
    git clone --depth 1 --branch 1.5.0 https://github.com/raspberrypi/pico-sdk $SDK_PATH &&\
    cd $SDK_PATH &&\
    git submodule update --init
    
ENV LANG=en_GB.utf8 SHELL=/bin/bash PICO_SDK_PATH=$SDK_PATH

USER $USERNAME
