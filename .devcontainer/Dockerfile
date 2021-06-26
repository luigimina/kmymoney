# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.166.0/containers/cpp/.devcontainer/base.Dockerfile

# [Choice] Debian / Ubuntu version: debian-10, debian-9, ubuntu-20.04, ubuntu-18.04
ARG VARIANT="buster"
FROM mcr.microsoft.com/vscode/devcontainers/cpp:0-${VARIANT}

COPY library-scripts/desktop-lite-debian.sh /tmp/library-scripts/
RUN apt-get update && bash /tmp/library-scripts/desktop-lite-debian.sh root
RUN sed -i '/it_IT.UTF-8/s/^# //g' /etc/locale.gen && locale-gen
ENV DBUS_SESSION_BUS_ADDRESS="autolaunch:" DISPLAY=":1" LANG="it_IT.UTF-8" LANGUAGE="it_IT.UTF-8" LC_ALL="it_IT.UTF-8"
ENTRYPOINT ["/usr/local/share/desktop-init.sh"]
CMD ["sleep", "infinity"]

# [Optional] Uncomment this section to install additional packages.
# RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
#     && apt-get -y install --no-install-recommends <your-package-list-here>
RUN sed -i -- 's/#[ ]*deb-src/deb-src/g' /etc/apt/sources.list
RUN apt-get update
RUN apt-get install -y eatmydata
RUN eatmydata apt-get upgrade -y
RUN eatmydata apt-get install -y openssh-server gdb gdbserver rsync
RUN eatmydata apt-get build-dep -y kmymoney
RUN eatmydata apt-get install -y qt5-default qtwebengine5-dev
RUN eatmydata apt-get install -y libsqlcipher-dev
RUN eatmydata apt-get install -y libqt5sql5-*
RUN eatmydata apt-get install -y oxygen-icon-theme
RUN eatmydata apt-get install -y subversion