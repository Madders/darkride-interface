## macOS

**Darkride-@VERSION@-macos-arm64.dmg** is Darkride for Macs with Apple silicon.

### Will it run on my Mac?

It needs:
- a Mac with an **Apple chip**: M1, M2, M3, M4 or later;
- **macOS 14 Sonoma or later**.

To check, click the **Apple logo** in the top-left corner of the screen and choose **About This Mac**:
- **Chip** should say *Apple M…*. If you see **Processor** with *Intel* instead, this version won't run on your Mac.
- **macOS** should be **14** or higher. If it's lower, update in **System Settings → General → Software Update**.

### Install

1. Under **Assets** at the bottom of this page, click **Darkride-@VERSION@-macos-arm64.dmg** to download it.
2. Open your **Downloads** folder and double-click the file. A window opens with **Darkride** and an **Applications** folder.
3. Drag **Darkride** onto **Applications**.
4. Close the window, and eject the **Darkride** disk: click the ⏏ next to it in a Finder window's sidebar.

### Open it the first time

Darkride isn't registered with Apple yet, so the first time you open it, macOS stops it and says it can't check it for malware. This happens once:

1. Open your **Applications** folder and double-click **Darkride**. When macOS says it can't open it, click **Done** (or **OK**). Don't choose **Move to Trash**.
2. Open **System Settings** (Apple menu → System Settings), click **Privacy & Security**, and scroll down to **Security**. Next to the message about Darkride, click **Open Anyway**, and enter your Mac's password if asked.
3. In the window that appears, click **Open Anyway** (or **Open**).

On macOS 14 Sonoma there's a shortcut: right-click (or Control-click) **Darkride** in Applications, choose **Open**, then click **Open**.

From then on, Darkride opens like any other app.

### Allow the microphone

When Darkride asks to use the microphone, click **Allow** so people near you can hear you. If you clicked **Don't Allow**, turn it on later in **System Settings → Privacy & Security → Microphone**.

### If macOS says Darkride "is damaged and can't be opened"

Nothing is broken: this is the same protection, worded differently. Open **Terminal** (Applications → Utilities → Terminal), paste this line, press **Return**, then open Darkride again:

```
xattr -dr com.apple.quarantine /Applications/Darkride.app
```
