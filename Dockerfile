FROM ubuntu:24.04 AS build
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y \
        binutils-mips-linux-gnu \
        bsdmainutils \
        build-essential \
        ca-certificates \
        libaudiofile-dev \
        python3 \
        wget && \
    rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/ps2dev/ps2dev/releases/download/latest/ps2dev-ubuntu-latest.tar.gz && \
    tar xzf ps2dev-ubuntu-latest.tar.gz && \
    rm ps2dev-ubuntu-latest.tar.gz

RUN mkdir /xyon
WORKDIR /xyon
ENV PATH="/ps2dev/ee/bin:/ps2dev/iop/bin:/xyon/tools:${PATH}"
ENV PS2DEV=/ps2dev
ENV PS2SDK=/ps2dev/ps2sdk
ENV GSKIT=/ps2dev/gsKit

CMD echo 'usage: docker run --rm -v "$(pwd):/xyon" -w /xyon xyon-ps2 make -C examples/render_images all'
