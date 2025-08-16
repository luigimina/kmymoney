FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

ARG REINSTALL_CMAKE_VERSION_FROM_SOURCE="3.22.2"

# Optionally install the cmake for vcpkg
COPY ./reinstall-cmake.sh /tmp/

RUN if [ "${REINSTALL_CMAKE_VERSION_FROM_SOURCE}" != "none" ]; then \
        chmod +x /tmp/reinstall-cmake.sh && /tmp/reinstall-cmake.sh ${REINSTALL_CMAKE_VERSION_FROM_SOURCE}; \
    fi \
    && rm -f /tmp/reinstall-cmake.sh

# [Optional] Uncomment this section to install additional vcpkg ports.
# RUN su vscode -c "${VCPKG_ROOT}/vcpkg install <your-port-name-here>"

# [Optional] Uncomment this section to install additional packages.
# RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
#     && apt-get -y install --no-install-recommends <your-package-list-here>

RUN echo "tzdata tzdata/Areas select Europe" | debconf-set-selections
RUN echo "tzdata tzdata/Zones/Europe select Rome" | debconf-set-selections
RUN apt update 
RUN apt install -y apt-utils gnupg wget 
RUN wget -qO - http://archive.neon.kde.org/public.key | apt-key add - 
RUN echo "deb http://archive.neon.kde.org/user noble main" >> /etc/apt/sources.list 
RUN echo "deb-src http://archive.neon.kde.org/user noble main" >> /etc/apt/sources.list 
RUN sed -i -- 's/#[ ]*deb-src/deb-src/g' /etc/apt/sources.list 
RUN apt update 
RUN apt install -y eatmydata 
RUN eatmydata apt upgrade -y 
RUN eatmydata apt install -y openssh-server gdb rsync iputils-ping telnet 
RUN eatmydata apt build-dep -y kmymoney 
RUN eatmydata apt install -y qt5-default 
RUN eatmydata apt install -y libsqlcipher-dev 
RUN eatmydata apt install -y libqt5sql5-* 
RUN eatmydata apt install -y subversion python3-dev ninja-build 
RUN eatmydata apt install -y breeze-icon-theme fonts-noto-color-emoji
RUN eatmydata apt install -y libkf5crash-dev
RUN eatmydata apt install -y oxygen-icon-theme
# Set the locale
RUN sed -i -e 's/# it_IT.UTF-8 UTF-8/it_IT.UTF-8 UTF-8/' /etc/locale.gen && \
    locale-gen
ENV LANG it_IT.UTF-8  
ENV LANGUAGE it_IT:it  
ENV LC_ALL it_IT.UTF-8