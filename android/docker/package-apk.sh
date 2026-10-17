#!/bin/bash
# Packages Darkride's Android build (from build-native.sh) into an APK, inside the darkride-android image.
#
#   docker run --rm -v darkride-android-work:/work -v <repo>:/src darkride-android:dev /src/android/docker/package-apk.sh
#
# 1. androiddeployqt (Qt 5.15) gathers Qt's libraries, jars, QML and plugins for the app's library and fills in
#    the manifest's Qt loader settings, into /work/deploy.
# 2. Gradle (android/apps/interface) builds the APK from that, the app's Java and resources, and Darkride's
#    resources and scripts. The debug APK lands in /work/apk.
set -euo pipefail

SRC=${SRC:-/src}
BUILD=${BUILD:-/work/build-android}
DEPLOY=${DEPLOY:-/work/deploy}
ABI=arm64-v8a
API_LEVEL=${API_LEVEL:-26}
TARGET_SDK=${TARGET_SDK:-35}
# androiddeployqt adds the ABI's triple to this when it looks for libc++_shared.so
NDK_LIBS="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib"

rm -rf "$DEPLOY"
mkdir -p "$DEPLOY/libs/$ABI"

# Qt 5.15's CMake support writes every library as android-build/libs/<abi>/lib<name>_<abi>.so, the names its
# loader looks for. The app's library goes in the deploy folder; Darkride's other libraries (BUILD_SHARED_LIBS is
# on for Android) and the codec plugin (libplugins_lib*.so, which PluginManager loads) go in as extra libraries.
LIBS_DIR="$BUILD/android-build/libs/$ABI"
APP_LIB_NAME=tivoli-native-lib
cp "$BUILD/android/apps/interface/lib${APP_LIB_NAME}_$ABI.so" "$DEPLOY/libs/$ABI/"
EXTRA_LIBS=$(find "$LIBS_DIR" -maxdepth 1 -name "*.so" | sort | paste -sd, -)

# Qt 5.15's networking (HTTPS, as to the metaverse) loads OpenSSL 1.1.1 at runtime as libcrypto_1_1.so and
# libssl_1_1.so: link them from vcpkg's static OpenSSL, the one Darkride itself links
VCPKG_ROOT=$(sed -n 's/^set(VCPKG_INSTALL_ROOT "\(.*\)")$/\1/p' "$BUILD/vcpkg.cmake")
NDK_BIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin"
SSL_DIR=/work/openssl-shared
mkdir -p "$SSL_DIR"
"$NDK_BIN/aarch64-linux-android$API_LEVEL-clang" -shared -o "$SSL_DIR/libcrypto_1_1.so" -Wl,-soname,libcrypto_1_1.so \
    -Wl,--whole-archive "$VCPKG_ROOT/lib/libcrypto.a" -Wl,--no-whole-archive -ldl
"$NDK_BIN/aarch64-linux-android$API_LEVEL-clang" -shared -o "$SSL_DIR/libssl_1_1.so" -Wl,-soname,libssl_1_1.so \
    -Wl,--whole-archive "$VCPKG_ROOT/lib/libssl.a" -Wl,--no-whole-archive -L"$SSL_DIR" -lcrypto_1_1
EXTRA_LIBS="$EXTRA_LIBS,$SSL_DIR/libcrypto_1_1.so,$SSL_DIR/libssl_1_1.so"

# The package sources androiddeployqt starts from: the app's manifest
PKGSRC=/work/pkgsrc
rm -rf "$PKGSRC" && mkdir -p "$PKGSRC"
cp "$SRC/android/apps/interface/src/main/AndroidManifest.xml" "$PKGSRC/"

cat > /work/deployment-settings.json <<EOF
{
    "description": "Darkride for Android",
    "qt": "$QT_ANDROID",
    "sdk": "$ANDROID_HOME",
    "sdkBuildToolsRevision": "35.0.0",
    "ndk": "$ANDROID_NDK_HOME",
    "toolchain-prefix": "llvm",
    "tool-prefix": "llvm",
    "ndk-host": "linux-x86_64",
    "architectures": {"$ABI": "aarch64-linux-android"},
    "android-min-sdk-version": "$API_LEVEL",
    "android-target-sdk-version": "$TARGET_SDK",
    "android-package-source-directory": "$PKGSRC",
    "android-extra-libs": "$EXTRA_LIBS",
    "qml-importscanner-binary": "$QT_ANDROID/bin/qmlimportscanner",
    "rcc-binary": "$QT_ANDROID/bin/rcc",
    "qml-root-path": "$SRC/interface/resources/qml",
    "stdcpp-path": "$NDK_LIBS/",
    "useLLVM": true,
    "application-binary": "$APP_LIB_NAME"
}
EOF

# androiddeployqt's auxiliary mode copies the dependencies and fills in the templates already in the output
# folder (the manifest's Qt loader settings and res/values/libs.xml), without building
cp "$SRC/android/apps/interface/src/main/AndroidManifest.xml" "$DEPLOY/"
mkdir -p "$DEPLOY/res/values"
cp "$QT_ANDROID/src/android/templates/res/values/libs.xml" "$DEPLOY/res/values/"
"$QT_ANDROID/bin/androiddeployqt" --input /work/deployment-settings.json --output "$DEPLOY" \
    --android-platform "android-$TARGET_SDK" --jdk "${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk-amd64}" --aux-mode --verbose

# the NDK's C++ runtime, which everything links and auxiliary mode doesn't copy
cp "$NDK_LIBS/aarch64-linux-android/libc++_shared.so" "$DEPLOY/libs/$ABI/"

# Qt jars auxiliary mode leaves out: Qt Network's (its startup looks up its class), and Qt Gamepad's for controllers
cp "$QT_ANDROID/jar/QtAndroidNetwork.jar" "$QT_ANDROID/jar/QtAndroidGamepad.jar" "$DEPLOY/libs/"

# Of its resources only libs.xml is needed: the app has its own strings (app_name)
find "$DEPLOY/res" -type f ! -name libs.xml -delete

# AGP 8 takes the namespace from build.gradle and refuses a package attribute in the manifest
sed -i 's/ package="[^"]*"//' "$DEPLOY/AndroidManifest.xml"

# keep the debug signing key between runs, so a new APK installs over the last
export ANDROID_USER_HOME=/work/android-user-home
mkdir -p "$ANDROID_USER_HOME"

# the version shown in Android's app info (VERSION_NAME) and the number updates must increase (VERSION_CODE)
VERSION_ARGS=()
if [ -n "${VERSION_NAME:-}" ]; then VERSION_ARGS+=(-PRELEASE_NUMBER="$VERSION_NAME"); fi
if [ -n "${VERSION_CODE:-}" ]; then VERSION_ARGS+=(-PVERSION_CODE="$VERSION_CODE"); fi

cd "$SRC/android"
gradle --no-daemon :interface:assembleDebug -PDARKRIDE_DEPLOY_DIR="$DEPLOY" -PQT_ANDROID="$QT_ANDROID" "${VERSION_ARGS[@]}" \
    --project-cache-dir /work/gradle-project-cache -g /work/gradle-home

mkdir -p /work/apk
cp "$SRC/android/apps/interface/build/outputs/apk/debug/"*.apk /work/apk/
ls -la /work/apk
