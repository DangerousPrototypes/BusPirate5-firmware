<<<<<<< HEAD
FROM debian:latest AS bp5plus_builder_base
=======
FROM debian:latest AS bp5_builder_base
>>>>>>> eb10223 (updating to address pip changes)

RUN rm /bin/sh && ln -s /bin/bash /bin/sh

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        bash \
        wget \
        curl \
        python3 \
        python3-pip \
        cmake \
        build-essential \
        git \
        libnewlib-arm-none-eabi \
        libnewlib-dev \
        libstdc++-arm-none-eabi-newlib \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN git clone https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk \
    && git -C /opt/pico-sdk submodule update --init --recursive

RUN curl -Lo gcc-arm-none-eabi.tar.bz2 "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2" \
    && mkdir /opt/gcc-arm-none-eabi \
    && tar xf gcc-arm-none-eabi.tar.bz2 --strip-components=1 -C /opt/gcc-arm-none-eabi \
    && rm -rf gcc-arm-none-eabi.tar.bz2

ENV PATH=$PATH:/opt/gcc-arm-none-eabi/bin
ENV PICO_SDK_PATH=/opt/pico-sdk

COPY entrypoint.sh /.

ENTRYPOINT [ "/entrypoint.sh" ]
