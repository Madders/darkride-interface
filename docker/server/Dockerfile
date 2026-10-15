# syntax=docker/dockerfile:1.7
#
# Builds the Darkride server stack (domain-server, assignment-client, ice-server) on Ubuntu 24.04
# and packs it into a slim runtime image. Run from the repository root:
#
#   docker build -f docker/server/Dockerfile -t darkride-server .
#
# vcpkg (its tree and its binary cache) and the CMake build directory live in BuildKit cache
# mounts, so a rebuild after a source change only recompiles what changed.
#
# The checkout must have LF line endings (on Windows: git clone -c core.autocrlf=input).

ARG UBUNTU_VERSION=24.04

FROM ubuntu:${UBUNTU_VERSION} AS toolchain

ENV DEBIAN_FRONTEND=noninteractive

# Ubuntu's own Qt 5.15 is used: the prebuilt Qt that hifi_qt.py downloads came from cdn.tivolicloud.com,
# which is gone. Node.js is only needed at configure time (tivoli-scripts).
RUN apt-get update -y && apt-get install -y --no-install-recommends \
        build-essential ninja-build cmake git curl ca-certificates zip unzip tar pkg-config \
        autoconf automake libtool python3 nodejs patchelf \
        qtbase5-dev qtbase5-private-dev qtdeclarative5-dev qtscript5-dev qtmultimedia5-dev \
        libqt5websockets5-dev libqt5webchannel5-dev libqt5xmlpatterns5-dev libqt5svg5-dev \
        libssl-dev zlib1g-dev libminizip-dev libgl1-mesa-dev libxxf86vm-dev libxi-dev libxrandr-dev \
        libasound2-dev libpulse-dev libudev-dev \
    && rm -rf /var/lib/apt/lists/*

FROM toolchain AS build

ENV TIVOLI_VCPKG_BASE=/vcpkg \
    RELEASE_TYPE=PRODUCTION

COPY . /src

RUN --mount=type=cache,id=darkride-vcpkg,target=/vcpkg \
    --mount=type=cache,id=darkride-vcpkg-archives,target=/root/.cache/vcpkg \
    --mount=type=cache,id=darkride-server-build,target=/build \
    set -e; \
    QT_CMAKE_PREFIX_PATH="/usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/cmake"; \
    cmake -S /src -B /build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DQT_CMAKE_PREFIX_PATH="$QT_CMAKE_PREFIX_PATH" \
        -DSERVER_ONLY=ON -DBUILD_SERVER=ON -DBUILD_CLIENT=OFF \
        -DBUILD_TESTS=OFF -DBUILD_TOOLS=OFF -DBUILD_INSTALLER=OFF; \
    cmake --build /build --target domain-server assignment-client ice-server; \
    sh /src/docker/server/install-server.sh /build /opt/darkride

# The runtime image: Ubuntu's Qt libraries and the installed server, nothing else.
FROM ubuntu:${UBUNTU_VERSION} AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y && apt-get install -y --no-install-recommends \
        ca-certificates libqt5core5t64 libqt5gui5t64 libqt5network5t64 libqt5widgets5t64 libqt5qml5 \
        libqt5script5 libqt5scripttools5 libqt5websockets5 libminizip1t64 libssl3t64 libgl1 \
        libopengl0 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /opt/darkride /opt/darkride
COPY docker/server/run-server.sh /opt/darkride/run-server.sh

# Settings, the world, assets and backups are kept here; mount a volume on it.
ENV TIVOLI_APP_DATA_DIR=/data \
    QT_QPA_PLATFORM=offscreen
VOLUME /data

# domain server web interface (TCP), domain server (UDP), assignment clients (UDP)
EXPOSE 40100/tcp 40101/tcp 40102/udp 48000-48006/udp

CMD ["bash", "/opt/darkride/run-server.sh"]
