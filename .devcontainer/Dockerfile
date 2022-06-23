FROM debian:11.3

# Run Updates
RUN apt-get update && apt-get upgrade -y

# Install prerequisites for PicoSDK
RUN apt-get install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib build-essential
RUN apt-get install -y gdb-multiarch
RUN apt-get install -y wget git python3
# Link gdb to arm-none-eabi-gdb for debugging
RUN ln -s /usr/bin/gdb-multiarch /usr/bin/arm-none-eabi-gdb

# Download and extract Segger JLink drivers and link to bin for later use
RUN wget --post-data 'accept_license_agreement=accepted&non_emb_ctr=confirmed&submit=Download+software' -O jlink.tgz https://www.segger.com/downloads/jlink/JLink_Linux_V764c_x86_64.tgz
RUN tar xfz jlink.tgz
RUN mv JLink_Linux_V764c_x86_64 /opt/jlink
RUN rm jlink.tgz
RUN ln -s /opt/jlink/JLinkExe /usr/local/bin/jlinkexe

# Install prerequisites for building Picoprobe version of OpenOCD
RUN apt-get install -y  automake autoconf texinfo libtool libftdi-dev libusb-1.0-0-dev pkg-config
RUN mkdir pico

# clone OpenOCD make and install
WORKDIR /pico
RUN git clone https://github.com/raspberrypi/openocd.git --branch rp2040 --depth=1 --no-single-branch
WORKDIR /pico/openocd
RUN ./bootstrap
RUN ./configure --enable-picoprobe
RUN make -j4 && make install

# clone PicoSDK
WORKDIR /pico
RUN git clone -b master https://github.com/raspberrypi/pico-examples.git && git clone -b master https://github.com/raspberrypi/pico-sdk.git
WORKDIR /pico/pico-sdk
RUN git submodule update --init

# add user and make directories to persistently store extension (prevent installation when connecting to container)
RUN useradd -ms /bin/bash user

ARG USERNAME=user

RUN mkdir -p /home/$USERNAME/.vscode-server/extensions \
        /home/$USERNAME/.vscode-server-insiders/extensions \
    && chown -R $USERNAME \
        /home/$USERNAME/.vscode-server \
        /home/$USERNAME/.vscode-server-insiders

#ENV PICO_SDK_PATH=/pico/pico-sdk