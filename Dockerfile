FROM ubuntu:20.04

WORKDIR /tmp/

RUN set -ex; \
    dpkg --add-architecture i386; \
    DEBIAN_FRONTEND=noninteractive apt-get update -y; \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        apt-transport-https \
        binutils \
        cabextract \
        cpulimit \
        curl \
        fluxbox \
        gpg-agent \
        imagemagick \
        less \
        nginx \
        openssh-server \
        p7zip \
        silversearcher-ag \
        software-properties-common \
        sudo \
        unzip \
        vim \
        wget \
        x11vnc \
        xvfb \
        xz-utils \
	xterm

#-----------------------------------------------------------------------

RUN set -ex; \
    wget -nc https://dl.winehq.org/wine-builds/winehq.key; \
    apt-key add winehq.key; \
    apt-add-repository https://dl.winehq.org/wine-builds/ubuntu/; \
    DEBIAN_FRONTEND=noninteractive apt-get update -y; \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --install-recommends \
        winehq-stable; \
    rm winehq.key

RUN set -ex; \
    wget https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks; \
    chmod +x winetricks; \
    mv winetricks /usr/local/bin

COPY waitonprocess.sh /docker/
RUN chmod a+rx /docker/waitonprocess.sh

#-----------------------------------------------------------------------

ARG USER=rapier
ARG HOME=/home/$USER
ARG USER_ID=1000
# To access the values from children containers.
ENV USER=$USER \
    HOME=$HOME

RUN set -ex; \
    adduser --disabled-password --gecos "" --shell /bin/bash $USER;\
    adduser $USER sudo;\
    echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers; \
    echo "Set disable_coredump false" >> /etc/sudo.conf

USER $USER

# WINEDLLOVERRIDES="mscoree,mshtml="
# -> it means don't worry about missing gecko
# -> this is fine as as we won't be looking at doc
USER $USER
ENV WINEARCH=win32 \
    WINEPREFIX=$HOME/.wine \
    WINEDLLOVERRIDES="mscoree,mshtml=" \
    DISPLAY=:1 \
    SCREEN_NUM=0 \
    SCREEN_WHD=1366x768x24
RUN set -ex; \
    wine wineboot --init; \
    /docker/waitonprocess.sh wineserver

USER root

#-----------------------------------------------------------------------
# ssh

ADD sshd_config /etc/sshd/sshd_config
ADD authorized_keys $HOME/.ssh/authorized_keys

RUN chown -R $USER:$USER $HOME/.ssh

#-----------------------------------------------------------------------
# IB + IBC + zorro

ENV STORE=$HOME/store

RUN mkdir $HOME/ibc
ENV IBC=$HOME/ibc
ADD staging/ibc.tar.bz2 $IBC
ADD secrets/config-demo.ini $IBC/
ADD secrets/config-live.ini $IBC/
ADD staging/ib.tar.bz2 $HOME


ENV ZORRO=$WINEPREFIX/drive_c/zorro
ADD staging/zorro.tar.bz2 $ZORRO
ADD secrets/ZorroFix.ini $ZORRO/
ADD secrets/Accounts.csv $ZORRO/History
ADD secrets/password.txt $ZORRO/
ADD secrets/key8e.dta $ZORRO/
ADD strategy $ZORRO/Strategy

RUN set -e; \
    chown -R $USER:$USER $ZORRO; \
    chown -R $USER:$USER $IBC

#-----------------------------------------------------------------------
# nginx

USER $USER

RUN mkdir -p $HOME/public; \
    ln -s $ZORRO/Log $HOME/public/zorro; \
    ln -s $IBC/logs $HOME/public/ibc; \
    #ln -s $MT4/MQL4/Logs $HOME/public/mt4_mql4; \
    ln -s $SCREENSHOTS $HOME/public/screenshots

ADD nginx.conf /etc/nginx/nginx.conf

USER root
#------ install IBC -------------------------------------------
#RUN set -e; \
#    cd /home/rapier/ibc/ \
#    chown -r $USER:$USER . \
    #chmod +x *.sh */*.sh
#    chmod +x twsstart.sh

#-----------------------------------------------------------------------
# home

ADD bashrc $HOME/.bashrc
ADD inputrc $HOME/.inputrc

RUN set -e; \
    chown $USER:$USER $HOME/.bashrc; \
    chown $USER:$USER $HOME/.inputrc

#-----------------------------------------------------------------------
# boot

ADD boot.sh /docker/
RUN chmod +x /docker/boot.sh

USER $USER
WORKDIR $WINEPREFIX/drive_c

ENTRYPOINT ["/bin/bash"]

# /docker/boot.sh [account=demo|live] [strategy] [usd_exposure]
CMD ["/docker/boot.sh", "981", "live", "risk-premia", "5000"]
