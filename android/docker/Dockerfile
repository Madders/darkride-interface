# Darkride for Android (phones and tablets): the build environment.
#
# Ubuntu 22.04 with JDK 17 (for current Gradle and the Android Gradle Plugin), the Android SDK (platform 35,
# build-tools 35), NDK r22b and Qt 5.15.2 for Android. NDK r22b is the newest Qt 5.15.2's androiddeployqt
# works with: LLVM 13 (NDK r23) dropped the one-dash options it passes to llvm-readobj.
#
#   docker build -t darkride-android android/docker
#   docker run --rm -it -v darkride-android-work:/work -v <repo>:/src darkride-android

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y install --no-install-recommends \
        ca-certificates curl unzip zip git \
        openjdk-17-jdk-headless \
        python3 python3-pip python3-venv \
        build-essential pkg-config ninja-build \
        autoconf automake libtool nasm yasm \
        file patchelf \
    && rm -rf /var/lib/apt/lists/*

# A CMake that vcpkg's 2021 ports accept (3.x) and Qt 5.15 knows
ARG CMAKE_VERSION=3.31.6
RUN curl -sSL -o /tmp/cmake.sh https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.sh \
    && sh /tmp/cmake.sh --skip-license --prefix=/usr/local && rm /tmp/cmake.sh

# --- Android SDK and NDK
ENV ANDROID_HOME=/opt/android-sdk \
    ANDROID_SDK_ROOT=/opt/android-sdk
ARG CMDLINE_TOOLS=commandlinetools-linux-11076708_latest.zip
ARG ANDROID_PLATFORM=35
ARG ANDROID_BUILD_TOOLS=35.0.0
ARG ANDROID_NDK=22.1.7171670
RUN mkdir -p $ANDROID_HOME/cmdline-tools \
    && curl -sSL -o /tmp/tools.zip https://dl.google.com/android/repository/${CMDLINE_TOOLS} \
    && unzip -q /tmp/tools.zip -d $ANDROID_HOME/cmdline-tools && mv $ANDROID_HOME/cmdline-tools/cmdline-tools $ANDROID_HOME/cmdline-tools/latest \
    && rm /tmp/tools.zip \
    && yes | $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager --licenses > /dev/null \
    && $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager \
        "platform-tools" "platforms;android-${ANDROID_PLATFORM}" "build-tools;${ANDROID_BUILD_TOOLS}" "ndk;${ANDROID_NDK}"
ENV ANDROID_NDK_HOME=$ANDROID_HOME/ndk/22.1.7171670 \
    ANDROID_NDK_ROOT=$ANDROID_HOME/ndk/22.1.7171670 \
    PATH=$PATH:/opt/android-sdk/platform-tools:/opt/android-sdk/cmdline-tools/latest/bin

# --- Qt 5.15.2 for Android (all ABIs in one), with Qt Script, from Qt's archive through aqtinstall
ARG QT_VERSION=5.15.2
RUN python3 -m pip install --no-cache-dir aqtinstall==3.3.0 \
    && python3 -m aqt install-qt linux android ${QT_VERSION} android -m qtscript -O /opt/qt \
    && python3 -m aqt install-qt linux desktop ${QT_VERSION} gcc_64 -O /opt/qt
ENV QT_ANDROID=/opt/qt/5.15.2/android \
    QT_HOST=/opt/qt/5.15.2/gcc_64

# --- Gradle, for packaging the APK (android/docker/package-apk.sh)
ARG GRADLE_VERSION=8.9
RUN curl -sSL -o /tmp/gradle.zip https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip     && unzip -q /tmp/gradle.zip -d /opt && mv /opt/gradle-${GRADLE_VERSION} /opt/gradle && rm /tmp/gradle.zip
ENV PATH=$PATH:/opt/gradle/bin     JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64

WORKDIR /work
