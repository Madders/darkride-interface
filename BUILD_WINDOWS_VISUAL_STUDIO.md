# Setting up Visual Studio for a Darkride build

This page covers step 1 of [BUILD_WINDOWS.md](BUILD_WINDOWS.md): which Visual Studio to install, and which parts of it the build needs. Everything here is free.

## Which Visual Studio?

| | Status | Use it? |
|---|---|---|
| **Visual Studio 2026** (Community) | Current, offered on Microsoft's download page | **Yes, recommended** |
| **Visual Studio 2022** (Community) | Microsoft's download page no longer offers it, but the installer and winget still do; supported until 13 January 2027, and 17.14 gets security fixes until January 2032 | Yes, if you already have it |
| **Build Tools for Visual Studio** 2026 or 2022 | The same compilers without the editor | Yes, for a command-line-only build |

Whichever you choose, the build needs the **Desktop development with C++** workload, plus three extra components.

## The components, and why

| Component (as named in the installer) | ID | Why |
|---|---|---|
| **Desktop development with C++** (workload) | `Microsoft.VisualStudio.Workload.NativeDesktop` | The C++ compiler, the Windows SDK, and CMake |
| **MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.39-17.9)** | `Microsoft.VisualStudio.Component.VC.14.39.17.9.x86.x64` | The dependencies are built by vcpkg, and the vcpkg version this project uses only recognises compiler versions up to 14.3x. Visual Studio's own compiler (14.4x in VS 2022, 14.5x in VS 2026) is newer, so vcpkg needs this one alongside. Under Visual Studio 2026, Darkride builds with it too (`-T v143`), because Qt 5.15's headers don't compile with 14.5x. Under VS 2022, Darkride uses VS 2022's own v143 compiler. |
| **C++ v14.39 (17.9) ATL for v143 build tools (x86 & x64)** | `Microsoft.VisualStudio.Component.VC.14.39.17.9.ATL` | ATL for that compiler. The speech recognition code includes the Windows SDK's `sphelper.h`, which needs ATL |
| **C++ ATL for x64/x86 (Latest MSVC)**; in VS 2022: **C++ ATL for latest v143 build tools (x86 & x64)** | `Microsoft.VisualStudio.Component.VC.ATL` | ATL for Visual Studio's own compiler, which Visual Studio 2022 builds Darkride with |

> In Visual Studio 2026 the 14.39 build tools and their ATL are listed as **"(Out of Support)"**. That only means Microsoft no longer updates that compiler version. They install and work.

## Installing with the Visual Studio Installer (point and click)

1.  Download **Visual Studio 2026 Community** from https://visualstudio.microsoft.com/downloads and run it.

    For Visual Studio 2022 instead, use https://aka.ms/vs/17/release/vs_community.exe, a direct Microsoft link.

2.  On the **Workloads** tab, tick **Desktop development with C++**.
3.  On the **Individual components** tab, use the search box to find and tick:
    -   `v14.39`: **MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.39-17.9)** and **C++ v14.39 (17.9) ATL for v143 build tools (x86 & x64)**
    -   `ATL`: **C++ ATL for x64/x86 (Latest MSVC)**. In Visual Studio 2022 it's called **C++ ATL for latest v143 build tools (x86 & x64)**.
4.  Click **Install**. Expect about 10 GB.

If Visual Studio is already installed, open the **Visual Studio Installer** from the Start menu, click **Modify** on your installation, and do steps 2–4.

## Installing from the command line

Run these in a command prompt; Windows asks for administrator rights. `--passive` shows progress without asking questions.

**Visual Studio 2026 Community.** Download the installer with `curl -LO https://aka.ms/vs/18/stable/vs_community.exe` (or from the link above), then:

```cmd
vs_community.exe --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --add Microsoft.VisualStudio.Component.VC.14.39.17.9.x86.x64 --add Microsoft.VisualStudio.Component.VC.14.39.17.9.ATL --add Microsoft.VisualStudio.Component.VC.ATL --passive --norestart
```

**Visual Studio 2022 Community:** the same command with the 2022 installer, from `curl -LO https://aka.ms/vs/17/release/vs_community.exe`. Or with winget:

```cmd
winget install --id Microsoft.VisualStudio.2022.Community -e --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --add Microsoft.VisualStudio.Component.VC.14.39.17.9.x86.x64 --add Microsoft.VisualStudio.Component.VC.14.39.17.9.ATL --add Microsoft.VisualStudio.Component.VC.ATL --passive --norestart"
```

**Adding the components to an existing installation:**

```cmd
"C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "C:\Program Files\Microsoft Visual Studio\18\Community" --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --add Microsoft.VisualStudio.Component.VC.14.39.17.9.x86.x64 --add Microsoft.VisualStudio.Component.VC.14.39.17.9.ATL --add Microsoft.VisualStudio.Component.VC.ATL --passive --norestart
```

For Visual Studio 2022 the path is `C:\Program Files\Microsoft Visual Studio\2022\Community`.

**Build Tools only (no editor):** use `vs_buildtools.exe` (2026: https://aka.ms/vs/18/stable/vs_buildtools.exe; 2022: https://aka.ms/vs/17/release/vs_buildtools.exe) with the workload `Microsoft.VisualStudio.Workload.VCTools` in place of `NativeDesktop`, and the same three `--add` components.

## Checking the installation

Open the **x64 Native Tools Command Prompt** for your Visual Studio from the Start menu, and run:

```cmd
dir "%VCINSTALLDIR%Tools\MSVC"
dir "%VCINSTALLDIR%Tools\MSVC\14.39*\atlmfc\include\atlbase.h"
```

The first should list a `14.39.xxxxx` folder next to Visual Studio's own version. The second should find `atlbase.h`; if it doesn't, the 14.39 ATL component is missing.

## More than one Visual Studio installed

CMake uses the newest Visual Studio for the generator you name (`Visual Studio 18 2026` or `Visual Studio 17 2022`), and that can be a **Build Tools** installation rather than Community. Add the components to the installation CMake actually uses, or name one on the first `cmake` run:

```cmd
cmake .. -G "Visual Studio 18 2026" -A x64 -T v143 "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" -DCMAKE_GENERATOR_INSTANCE="C:/Program Files/Microsoft Visual Studio/18/Community"
```

vcpkg separately picks the newest Visual Studio that has a 14.3x compiler.

## Extra components for the optional Qt with H.264 and AAC

Only needed for [Optional: Qt with H.264 and AAC](BUILD_WINDOWS.md#optional-qt-with-h264-and-aac). Qt WebEngine 5.15.2 (Chromium 83) is built with Visual Studio 2019's compiler and an older Windows SDK. Both Visual Studio 2026 and 2022 still offer the compiler:

| Component | ID |
|---|---|
| **MSVC v142 - VS 2019 C++ x64/x86 build tools (v14.29-16.11)** | `Microsoft.VisualStudio.Component.VC.14.29.16.11.x86.x64` |
| **C++ v14.29 (16.11) ATL for v142 build tools (x86 & x64)** | `Microsoft.VisualStudio.Component.VC.14.29.16.11.ATL` |
| **Windows 10 SDK (10.0.19041.0)**: Visual Studio 2022 offers it as a component (`Microsoft.VisualStudio.Component.Windows10SDK.19041`). Visual Studio 2026 doesn't, so install it from Microsoft's [Windows SDK archive](https://developer.microsoft.com/windows/downloads/sdk-archive/). | |
