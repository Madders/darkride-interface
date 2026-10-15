# Building Darkride for Windows

This guide builds the Darkride client (`interface.exe`) and, if you want them, the servers (`domain-server.exe`, `assignment-client.exe`) on Windows 10 or 11 (64-bit).

**Plan for:**

-   **About 1 hour.** The first build compiles every third-party library, which takes 15–20 minutes, then compiles Darkride itself in 30–40 minutes. Later builds take a few minutes.
-   **About 30 GB of disk space.**

Commands are for the **x64 Native Tools Command Prompt for VS 2022**, which step 1 installs. Find it in the Start menu.

## 1. Install the tools

| Tool | Notes |
|---|---|
| [Visual Studio 2022](https://visualstudio.microsoft.com/vs) (Community is free) | Install the components listed below |
| [PowerShell 7](https://github.com/PowerShell/PowerShell/releases) | Use the **`.msi`** installer. The Microsoft Store version won't do: the build looks for `pwsh` in `C:\Program Files\PowerShell\7`. |
| [Python 3](https://www.python.org/downloads) | Tick "Add python.exe to PATH" |
| [Node.js](https://nodejs.org/en) (LTS) | Builds the in-world web UI |
| [Git](https://git-scm.com/downloads) | |

**Visual Studio components.** In the Visual Studio Installer, select the **Desktop development with C++** workload. Under **Individual components**, also select:

-   **MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.39-17.9)**
-   **C++ ATL for latest v143 build tools (x86 & x64)**

Or install everything from the command line, which needs administrator rights:

```cmd
"C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "C:\Program Files\Microsoft Visual Studio\2022\Community" --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --add Microsoft.VisualStudio.Component.VC.14.39.17.9.x86.x64 --add Microsoft.VisualStudio.Component.VC.ATL
```

> **Why 14.39?** The dependencies are built by vcpkg, and the vcpkg version this project uses only recognises compilers up to 14.3x. Current Visual Studio 2022 releases default to 14.4x. With 14.39 installed alongside, vcpkg uses it for the dependencies, and Darkride itself builds with the default compiler. The two are compatible.
>
> **Why ATL?** The speech recognition code includes the Windows SDK's `sphelper.h`, which needs it.

## 2. Install Qt 5.15.2

Darkride needs Qt 5.15.2 for MSVC, with **Qt WebEngine** and **Qt Script**. The official build from the Qt archive works. [aqtinstall](https://github.com/miurahr/aqtinstall) downloads it without a Qt account (about 2 GB):

```cmd
python -m pip install aqtinstall
python -m aqt install-qt windows desktop 5.15.2 win64_msvc2019_64 -m qtwebengine qtscript qtwebglplugin -O C:\Qt
```

This installs Qt into `C:\Qt\5.15.2\msvc2019_64`.

The official Qt's web browser component can't play H.264 or AAC video (MP4 files and most live streams) inside web entities; WebM, VP9 and Opus work. To add those codecs, see [Optional: Qt with H.264 and AAC](#optional-qt-with-h264-and-aac) at the end.

## 3. Get the code and set two variables

```cmd
git clone https://github.com/caitlynmeeks/darkride-interface.git
cd darkride-interface

set QT_CMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64/lib/cmake
set TIVOLI_VCPKG_BASE=C:\vcpkg
```

-   `QT_CMAKE_PREFIX_PATH` tells the build which Qt to use. If it isn't set, the build downloads a prebuilt Qt instead (1.65 GB, from Overte's server). This guide was tested with the official Qt from step 2.
-   `TIVOLI_VCPKG_BASE` is where vcpkg builds and keeps the dependencies. It grows to several gigabytes. **Keep the path short**: Windows limits paths to 260 characters unless long paths are enabled, and vcpkg's folders go deep. The default is `C:\Users\<you>\tivoli\vcpkg`.

These `set` commands only last for the current window. To keep them, set them once under **System Properties → Environment Variables**.

## 4. Configure

```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
```

The first run downloads and compiles all the dependencies, which takes 15–20 minutes. It's finished when CMake prints `-- Generating done`. Any change to the files in `cmake/ports` makes vcpkg rebuild the dependencies, but it reuses unchanged packages from its cache in `%LOCALAPPDATA%\vcpkg\archives`.

## 5. Build

```cmd
cmake --build . --target interface --config RelWithDebInfo
```

To build the servers too, add their targets: `--target interface domain-server assignment-client`.

Or open `build\hifi.sln` in Visual Studio, set the configuration to **RelWithDebInfo**, right-click **interface** in the Solution Explorer and choose **Build**.

## 6. Run

```cmd
build\interface\RelWithDebInfo\interface.exe
```

**Darkride's metaverse.** The worlds listed in **Explore**, and accounts, come from `https://vr.darkride.ai`. The client's built-in default is still Tivoli's old server, which is gone, so set this once for your Windows user. Programs started afterwards pick it up.

```cmd
setx HIFI_METAVERSE_URL https://vr.darkride.ai
```

**A world on your own PC.** Start the servers in two command prompts, then go to `hifi://localhost` in interface:

```cmd
build\domain-server\RelWithDebInfo\domain-server.exe
build\assignment-client\RelWithDebInfo\assignment-client.exe --min 6 --max 10
```

The world's admin page is http://localhost:40100.

## What's left out on Windows

These SDKs can no longer be downloaded, so the build leaves out the plugins that need them:

-   **Oculus PC SDK.** Rift and Quest headsets work through SteamVR with the OpenVR plugin. If you have `LibOVR.lib` in the vcpkg tree, `oculus-plugin` is built again.
-   **Leap Motion 2.3 SDK.**
-   **Tea protocol plugin**, a prebuilt binary on every platform. It is used if its extracted archive is in `plugins/tea-protocol-plugin-prebuilt/tea-protocol-plugin-2021-12-12`.

## Troubleshooting

| Error | Fix |
|---|---|
| `Could not find PowerShell Core` | Install PowerShell 7 from the `.msi` (step 1), then open a **new** command prompt |
| `Cannot open include file: 'atlbase.h'` | Install the ATL component (step 1). If you also have **Build Tools for Visual Studio 2022**, CMake may use that instead of Community, so add the components to it as well, or choose one with `-DCMAKE_GENERATOR_INSTANCE="C:/Program Files/Microsoft Visual Studio/2022/Community"` on the first `cmake` run. |
| vcpkg can't find a Visual Studio instance or toolset | Install the **v14.39** compiler (step 1) |
| Errors about paths that are too long | Use a short `TIVOLI_VCPKG_BASE` such as `C:\vcpkg`, and keep the repository near the root of the drive |
| `Compatibility with CMake < 3.5 has been removed` | Your PATH has CMake 4. Use the CMake that comes with Visual Studio (it's on the Native Tools prompt's PATH), or add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` |
| The same CMake error after you fixed its cause | Delete `build\CMakeCache.txt`, or the whole `build` folder, and run step 4 again |
| A dependency failed to build | vcpkg's log for it is in `%TIVOLI_VCPKG_BASE%\<id>\buildtrees\<package>\` |
| Explore shows no worlds | Set `HIFI_METAVERSE_URL` (step 6) |

## Optional: Qt with H.264 and AAC

To play H.264/AAC video in web entities, build Qt WebEngine with Chromium's proprietary codecs, then build Darkride against that Qt. Only the WebEngine module is rebuilt; everything else comes from the official Qt you installed in step 2.

**H.264 and AAC are patented.** A build with these codecs is fine for your own use, but distributing it may require patent licences.

**What it costs:**

-   **1½–3 hours** on a fast PC, with the machine busy throughout.
-   **About 60 GB** of disk space.
-   **At least 16 GB of RAM.** Lower `NINJAJOBS` if memory is short.

**Additional tools.** Qt WebEngine 5.15.2 (Chromium 83) predates Visual Studio 2022:

-   In the Visual Studio Installer, under **Individual components**, add:
    -   **MSVC v142 - VS 2019 C++ x64/x86 build tools (v14.29-16.11)**
    -   **C++ v14.29 (16.11) ATL for v142 build tools (x86 & x64)**
    -   **Windows 10 SDK (10.0.19041.0)**

    The equivalent IDs are `Microsoft.VisualStudio.Component.VC.14.29.16.11.x86.x64`, `Microsoft.VisualStudio.Component.VC.14.29.16.11.ATL` and `Microsoft.VisualStudio.Component.Windows10SDK.19041`.
-   **Python 2.7**, which Chromium 83's build scripts need. It doesn't have to go on your PATH, or even be installed: this unpacks it into a folder without administrator rights:

    ```cmd
    curl -LO https://www.python.org/ftp/python/2.7.18/python-2.7.18.amd64.msi
    msiexec /a python-2.7.18.amd64.msi /qn TARGETDIR=C:\python27
    ```

**Build it:**

1.  **Copy the Qt from step 2**, so you keep the original. Add a `qt.conf` so the copy's `qmake` installs into the copy:

    ```cmd
    robocopy C:\Qt\5.15.2\msvc2019_64 C:\Qt-codecs\5.15.2\msvc2019_64 /E /NFL /NDL
    (echo [Paths]& echo Prefix=..) > C:\Qt-codecs\5.15.2\msvc2019_64\bin\qt.conf
    ```

2.  **Run the build script** from a normal command prompt. It sets up the Visual Studio environment itself. The work folder must be outside any git repository and have a short path.

    ```cmd
    tools\qt-builder\build-qtwebengine-codecs-windows.bat C:\Qt-codecs\5.15.2\msvc2019_64 C:\qwe C:\python27
    ```

    The script:
    -   downloads the Qt WebEngine 5.15.2 source and checks its SHA-256;
    -   applies the patches in `tools/qt-builder/patches`;
    -   downloads jom and Qt's gperf/bison/flex;
    -   builds only what Darkride needs: release only (it marks the Qt copy's `mkspecs\qconfig.pri` release-only and keeps the original as `qconfig.pri.orig`), with `-webengine-proprietary-codecs`, and without QtPdf;
    -   installs into the Qt copy.

    If it stops, run it again: it carries on where it left off.

3.  **Build Darkride against the copy**, in a new build folder, because CMake won't switch Qt in an existing one:

    ```cmd
    set QT_CMAKE_PREFIX_PATH=C:/Qt-codecs/5.15.2/msvc2019_64/lib/cmake
    mkdir build-codecs
    cd build-codecs
    cmake .. -G "Visual Studio 17 2022" -A x64
    cmake --build . --target interface --config RelWithDebInfo
    ```
