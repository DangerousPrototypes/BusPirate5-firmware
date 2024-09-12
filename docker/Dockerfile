FROM debian:latest AS bp5plus_builder_base

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
        sudo \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Note: this does some builds of the picotool SDK to make the CMake files work
# If the version of the tool updates, this needs to as well
# https://github.com/raspberrypi/pico-sdk/pull/1820#issuecomment-2291371915
# https://github.com/raspberrypi/pico-sdk/pull/1820#issuecomment-2291611448
RUN git clone https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk \
    && git -C /opt/pico-sdk submodule update --init --recursive \
    && cd /opt/pico-sdk \
    && git checkout 2.0.0 \
    && mkdir -p build \
    && cd build \
    && cmake ../ \
    && make picotoolBuild \
    && cmake --install .

RUN curl -Lo gcc-arm-none-eabi.tar.bz2 "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2" \
    && mkdir /opt/gcc-arm-none-eabi \
    && tar xf gcc-arm-none-eabi.tar.bz2 --strip-components=1 -C /opt/gcc-arm-none-eabi \
    && rm -rf gcc-arm-none-eabi.tar.bz2

ENV PATH=$PATH:/opt/gcc-arm-none-eabi/bin/
ENV picotool_DIR=/opt/pico-sdk/build/_deps/picotool/
ENV PICO_SDK_PATH=/opt/pico-sdk/

# modified from here
# https://code.visualstudio.com/remote/advancedcontainers/add-nonroot-user#_creating-a-nonroot-user
ARG USERNAME=build
ARG UID=1000
ARG GID=1000

# Create the user
RUN groupadd --gid "${UID}" "${USERNAME}" \
    && useradd --uid "${UID}" --gid "${GID}" -m "${USERNAME}" -s /usr/bin/bash \
    && echo "${USERNAME} ALL=\(root\) NOPASSWD:ALL" > /etc/sudoers.d/"${USERNAME}" \
    && chmod 0440 /etc/sudoers.d/"${USERNAME}"

USER $USERNAME

COPY entrypoint.sh /.

ENTRYPOINT [ "/entrypoint.sh" ]
