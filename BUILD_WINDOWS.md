# Building Darkride for Windows

This guide builds the Darkride client (`interface.exe`) and, if you want them, the servers (`domain-server.exe`, `assignment-client.exe`) on Windows 10 or 11 (64-bit).

**Plan for:**

-   **About 1 hour.** The first build compiles every third-party library, which takes 15–20 minutes, then compiles Darkride itself in 30–40 minutes. Later builds take a few minutes.
-   **About 30 GB of disk space.**
-   **16 GB of RAM or more.** Step 5 says how to build with less.

Commands are for the **x64 Native Tools Command Prompt** for your Visual Studio, which step 1 installs. Find it in the Start menu.

## 1. Install the tools

| Tool | Notes |
|---|---|
| [Visual Studio 2026](https://visualstudio.microsoft.com/downloads) (Community is free), or Visual Studio 2022 | The **Desktop development with C++** workload plus two components. **[BUILD_WINDOWS_VISUAL_STUDIO.md](BUILD_WINDOWS_VISUAL_STUDIO.md)** has step-by-step instructions |
| [PowerShell 7](https://github.com/PowerShell/PowerShell/releases) | Use the **`.msi`** installer. The Microsoft Store version won't do: the build looks for `pwsh` in `C:\Program Files\PowerShell\7`. |
| [Python 3](https://www.python.org/downloads) | Tick "Add python.exe to PATH" |
| [Node.js](https://nodejs.org/en) (LTS) | Builds the in-world web UI |
| [Git](https://git-scm.com/downloads) | |

**Visual Studio components, in short:** the **Desktop development with C++** workload, plus the individual components **MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.39-17.9)**, **C++ v14.39 (17.9) ATL for v143 build tools** and **C++ ATL** for the latest MSVC. vcpkg and, under Visual Studio 2026, Darkride itself build with the 14.39 compiler, and the speech recognition code needs ATL. Visual Studio 2026 labels the 14.39 tools "(Out of Support)"; install them anyway. [BUILD_WINDOWS_VISUAL_STUDIO.md](BUILD_WINDOWS_VISUAL_STUDIO.md) covers point-and-click and command-line installs, Build Tools, checking the result, and the extras for the optional codec build.

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

-   `QT_CMAKE_PREFIX_PATH` tells the build which Qt to use. If it isn't set, the build downloads a prebuilt Qt instead (1.65 GB). This guide was tested with the official Qt from step 2.
-   `TIVOLI_VCPKG_BASE` is where vcpkg builds and keeps the dependencies. It grows to several gigabytes. **Keep the path short**: Windows limits paths to 260 characters unless long paths are enabled, and vcpkg's folders go deep. The default is `C:\Users\<you>\tivoli\vcpkg`.

These `set` commands only last for the current window. To keep them, set them once under **System Properties → Environment Variables**.

## 4. Configure

```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 18 2026" -A x64 -T v143 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
```

With Visual Studio 2022, use `-G "Visual Studio 17 2022"`. The other options stay the same.

-   `-T v143` builds with the Visual Studio 2022 compiler (the 14.39 one from step 1). Qt 5.15's headers use `stdext::make_checked_array_iterator`, which Visual Studio 2026's own compiler (14.5x) no longer has.
-   `CMAKE_POLICY_VERSION_MINIMUM` is needed with CMake 4, which comes with Visual Studio 2026: it lets CMake accept the older `cmake_minimum_required` lines in the tree. It's harmless with older CMake. In PowerShell, quote the whole argument (`'-DCMAKE_POLICY_VERSION_MINIMUM=3.5'`), or PowerShell splits it at the dot.

The first run downloads and compiles all the dependencies, which takes 15–20 minutes. It's finished when CMake prints `-- Generating done`. Any change to the files in `cmake/ports` makes vcpkg rebuild the dependencies, but it reuses unchanged packages from its cache in `%LOCALAPPDATA%\vcpkg\archives`.

## 5. Build

```cmd
cmake --build . --target interface --config RelWithDebInfo
```

To build the servers too, add their targets: `--target interface domain-server assignment-client`.

**If the PC runs short of memory** (the build slows to a crawl, or tools quit with out-of-memory errors), build fewer projects at once by adding `-- /m:4` to the end of the command. By default Visual Studio builds as many projects at once as the PC has CPU threads, each compiling several files at a time, which can need more than 32 GB on a 24-thread CPU.

Or open `build\hifi.sln` in Visual Studio, set the configuration to **RelWithDebInfo**, right-click **interface** in the Solution Explorer and choose **Build**.

## 6. Run

```cmd
build\interface\RelWithDebInfo\interface.exe
```

**Darkride's metaverse.** The worlds listed in **Explore**, and accounts, come from `https://vr.darkride.ai`, the client's default. To try another metaverse server, set `HIFI_METAVERSE_URL` for your Windows user; programs started afterwards pick it up, and `reg delete HKCU\Environment /v HIFI_METAVERSE_URL /f` goes back to the default.

```cmd
setx HIFI_METAVERSE_URL https://metaverse.example.com
```

**A world on your own PC.** Start the servers in two command prompts, then go to `hifi://localhost` in interface:

```cmd
build\domain-server\RelWithDebInfo\domain-server.exe
build\assignment-client\RelWithDebInfo\assignment-client.exe --min 6 --max 10
```

The world's admin page is http://localhost:40100.

## 7. Package (optional)

To make the installer and zip that releases ship, build the interface and both servers in **Release**, install [Inno Setup 6](https://jrsoftware.org/isinfo.php) (`winget install JRSoftware.InnoSetup`), then from the repository folder:

```cmd
pwsh tools\installer\package-windows.ps1 -BuildDir build -Version 2026.10.1 -OutDir dist
```

It puts the interface and servers in one folder with the Visual C++ runtime beside them, and writes `dist\Darkride-2026.10.1-windows-x64.zip` and `dist\Darkride-2026.10.1-windows-x64-setup.exe`. The installer script is `tools\installer\darkride.iss`.

GitHub builds these the same way: pushing a tag such as `v2026.10.1` runs `.github/workflows/windows-release.yml`, which publishes them as a release.

## What's left out on Windows

These SDKs can no longer be downloaded, so the build leaves out the plugins that need them:

-   **Oculus PC SDK.** Rift and Quest headsets work through SteamVR with the OpenVR plugin. If you have `LibOVR.lib` in the vcpkg tree, `oculus-plugin` is built again.
-   **Leap Motion 2.3 SDK.**
-   **Tea protocol plugin**, a prebuilt binary on every platform. It is used if its extracted archive is in `plugins/tea-protocol-plugin-prebuilt/tea-protocol-plugin-2021-12-12`.

## Troubleshooting

| Error | Fix |
|---|---|
| `Could not find PowerShell Core` | Install PowerShell 7 from the `.msi` (step 1), then open a **new** command prompt |
| `Cannot open include file: 'atlbase.h'` | Install the ATL component (step 1). With more than one Visual Studio installed (for example **Build Tools** as well as Community), CMake may pick the other one; see [More than one Visual Studio installed](BUILD_WINDOWS_VISUAL_STUDIO.md#more-than-one-visual-studio-installed) |
| vcpkg can't find a Visual Studio instance or toolset | Install the **v14.39** compiler (step 1) |
| Errors about paths that are too long | Use a short `TIVOLI_VCPKG_BASE` such as `C:\vcpkg`, and keep the repository near the root of the drive |
| `'stdext': is not a class or namespace name` in Qt's `qvector.h` | Visual Studio 2026's own compiler can't build Qt 5.15's headers. Configure with `-T v143` (step 4), in a new build folder |
| `Compatibility with CMake < 3.5 has been removed`, when configuring Darkride | Add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` to the `cmake` command (step 4) |
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

**Additional tools.** Qt WebEngine 5.15.2 (Chromium 83) predates current Visual Studio:

-   **Visual Studio 2019's compiler (v14.29) with its ATL, and the Windows 10 SDK 10.0.19041.** Visual Studio 2026 and 2022 both still offer the compiler as a component. Visual Studio 2026 doesn't offer that SDK, but Microsoft's SDK archive does. See [Extra components for the optional Qt with H.264 and AAC](BUILD_WINDOWS_VISUAL_STUDIO.md#extra-components-for-the-optional-qt-with-h264-and-aac).
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
    cmake .. -G "Visual Studio 18 2026" -A x64 -T v143 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    cmake --build . --target interface --config RelWithDebInfo
    ```
