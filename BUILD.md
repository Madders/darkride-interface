# Building Darkride

| What | Guide | Status |
|---|---|---|
| Windows client and servers | [BUILD_WINDOWS.md](BUILD_WINDOWS.md) | **Tested** with Visual Studio 2022 and 2026. GitHub builds the releases the same way ([.github/workflows/release.yml](.github/workflows/release.yml)) |
| Linux world server | [docker/README.md](docker/README.md) | **Tested**: Ubuntu 24.04 in Docker. Releases publish it as `ghcr.io/madders/darkride-server` |
| macOS client | [BUILD_MACOS.md](BUILD_MACOS.md) | Tivoli's guide, not yet updated for Darkride |
| Linux client | [BUILD_LINUX.md](BUILD_LINUX.md) | Tivoli's guide, not tried with Darkride |
| Android | [BUILD_ANDROID.md](BUILD_ANDROID.md) | Probably doesn't work any more |

The rest of this page is general background for every platform.

# System dependencies

-   [git](https://git-scm.com/downloads)
-   [cmake](https://cmake.org/download)
-   [Python](https://www.python.org/downloads)
-   [Node.js](https://nodejs.org/en) v10.13 or higher
-   [Rust](https://rustup.rs) through rustup: chat's library (`libraries/chat/native`) is written in Rust. The build installs the version that library's `rust-toolchain.toml` names the first time

# CMake

Darkride uses CMake to generate build files and project files for your platform. If you don't know how to use CMake, you may struggle to understand what's going on.

## Qt

Darkride needs Qt 5.15.2 with Qt WebEngine and Qt Script. Point CMake at it with `QT_CMAKE_PREFIX_PATH`, its **lib/cmake** folder:

```bash
    # windows
    set QT_CMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64/lib/cmake
    # unix
    export QT_CMAKE_PREFIX_PATH=/path/to/qt/lib/cmake
```

[BUILD_WINDOWS.md](BUILD_WINDOWS.md) installs the official Qt with aqtinstall. The Linux server image uses Ubuntu's own Qt packages.

Without `QT_CMAKE_PREFIX_PATH`, CMake runs `hifi_qt.py` to download a prebuilt Qt into `TIVOLI_QT_BASE` (default `~/tivoli/qt`). That works on Windows, from build-deps.overte.org. The macOS and Linux downloads were on Tivoli's CDN, which is gone, so set `QT_CMAKE_PREFIX_PATH` there.

## Vcpkg

When CMake is preparing build files, it will download and bootstrap Vcpkg and use it to install packages.

Usually this is stored in your home directory as `tivoli/vcpkg`.

Building all of our dependencies can be lengthy and take up a lot of disk space. You can change the directory using the environment variable `TIVOLI_VCPKG_BASE`

```bash
    # windows
    set TIVOLI_VCPKG_BASE=C:\TIVOLI_VCPKG
    # unix
    export TIVOLI_VCPKG_BASE=/home/user/TIVOLI_VCPKG
```

## Generating build files

Create a build directory in the root of your checkout and then run the CMake build from there. This will keep the rest of the directory clean.

```bash
    mkdir build
    cd build
    cmake ..
```

**On Windows, use the whole command from [BUILD_WINDOWS.md](BUILD_WINDOWS.md) step 4** (`-G "Visual Studio …" -A x64 -T v143 …`) from the first run. A folder configured once with a plain `cmake ..` remembers its empty platform, and every later run with `-A x64` fails with `generator platform: x64 Does not match the platform used previously`. Delete the folder and start again.

If cmake gives you the same error message repeatedly after the build fails, try removing `CMakeCache.txt` or the entire build folder.

## Variables

Any variables that need to be set for CMake can be set as environment variables or passed directly with a `-D` flag appended to the `cmake ..` command.

For example, to pass the `QT_CMAKE_PREFIX_PATH` variable (if not using our version of Qt) during build file generation without using an environment variable:

```bash
    cmake .. -DQT_CMAKE_PREFIX_PATH=/path/to/qt/lib/cmake
```

## Load QML from resources folder, not `resources.rcc`

When working on QML, you'll need a way to easily see the changes you're making.

-   Make sure you're making a **dev build** (`cmake ..` without any production variables). You can verify this in `build/includes/BuildInfo.h` with `VERSION = "dev"`.

-   When launching interface, add `HIFI_USE_SOURCE_TREE_RESOURCES=1` environment variable. You can add this in the launcher under **Developer > Additional environment variables**.

-   Once in-world, you can test your QML and press **Edit > Clear Cache & Reload (very slow)** to reload.

## Other runtime environment variables

-   `HIFI_SHOW_DEVELOPER_CRASH_MENU` - enables menu to test crashing
-   `TIVOLI_SCRIPT_DEBUG` - adds "Script Debug" menu for all running scripts

## Optional components

The following build options can be used when running CMake

### Build options

-   `BUILD_CLIENT`
-   `BUILD_SERVER`
-   `BUILD_TESTS`
-   `BUILD_TOOLS`

### Developer build options

-   `USE_GLES`
-   `DISABLE_UI`

## Running a domain server locally for testing

We recommend using the `pm2` process manager

-   Install Node.js
-   `npm i -g pm2`

Enter the root directory of this repo, after building the servers into `build` (in RelWithDebInfo, which `ecosystem.config.js` expects), and run:

```bash
pm2 start ecosystem.config.js
# here are extra commands
pm2 ls
pm2 logs 0 # id of the process found in ls
pm2 logs 0 --lines 999
pm2 stop 0
pm2 start 0
pm2 restart 0
pm2 kill
```

**Don't forget to run `pm2 kill`** when you're finished or it'll keep running in the background!
