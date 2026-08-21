# LotPilot

**Hands-free controls for Whatnot live sellers.** LotPilot provides a separate,
low-latency path for starting the next auction or giveaway in Firefox on KDE
Plasma/Wayland and Windows.

## Controls

| Platform | Shortcut | Action |
|---|---|---|
| Linux/KDE | `Super+A` | Start the next auction |
| Linux/KDE | `Super+G` | Start the next giveaway |
| Linux + Logitech R400 | Previous | Start the next auction |
| Linux + Logitech R400 | Next | Start the next giveaway |
| Linux + iKKEGOL triple pedal | Left (`A`) | Start the next auction |
| Linux + iKKEGOL triple pedal | Middle (`B`) | Inspect controls safely (no click) |
| Linux + iKKEGOL triple pedal | Right (`C`) | Start the next giveaway |
| Windows | `Ctrl+Shift+F9` | Start the next auction |
| Windows | `Ctrl+Shift+F10` | Start the next giveaway |
| Windows + Logitech R400 | Previous | Start the next auction |
| Windows + Logitech R400 | Next | Start the next giveaway |
| Windows + iKKEGOL triple pedal | Left (`A`) | Start the next auction |
| Windows + iKKEGOL triple pedal | Middle (`B`) | Inspect controls safely (no click) |
| Windows + iKKEGOL triple pedal | Right (`C`) | Start the next giveaway |

The host applies only a 150 ms hardware key-bounce filter. Distinct presses are queued, while duplicate delivery of the same command ID is suppressed. The page script refuses to click when the matching control is hidden, disabled, covered, missing, or ambiguous.

## Configure controls

Open **LotPilot** from KDE's application launcher or the Windows Start Menu. Click the auction or
giveaway binding, press the desired keyboard, macro-pad, or controller button,
then click **Apply**. The configurator checks for duplicate and system-wide
conflicts, warns before accepting potentially disruptive unmodified keys, and
can reset the defaults.

The Windows and KDE configurators use the same streamlined, dark-only visual
style. The theme is fixed rather than following the operating-system light
setting, so controls, status messages, and focus indicators remain consistent
while streaming.

The Razer Tartarus Pro appears to Linux and Windows as a keyboard, so its keys can be
recorded directly. The operating system sees the key emitted by the device rather than the
physical label such as “Key 20.” To keep a binding specific in practice, map
the Tartarus or controller button to an unused key such as `F13`–`F24`, then
record that key in LotPilot. A gamepad that emits only joystick-button
events needs Input Remapper, Steam Input, or a similar keyboard mapper first.

On Windows, a built-in Raw Input profile recognizes the Logitech R400 receiver
(`VID_046D`, `PID_C538`, keyboard interface `MI_00`) separately from other
keyboards. Previous triggers Auction and Next triggers Giveaway automatically;
the Start Slideshow, Black Screen, and Laser buttons are not assigned. The R400's
normal Page Up/Page Down keystrokes still reach the foreground application.
Page Up and Page Down are reserved and cannot also be configured as global
hotkeys, preventing one R400 press from producing conflicting actions.

On Linux, the native host recognizes only the matching USB input device
(`046d:c538`, `Logitech USB Receiver`). It reads press events without grabbing
the device, so the R400's normal Page Up/Page Down events still reach the
foreground application. An owner-only runtime lock ensures that only one
Firefox native-host process handles a physical press, while another profile can
take over after the owner exits. Hot-unplugged receivers are rediscovered after
they reconnect. Page Up and Page Down are reserved from KDE shortcut
configuration for the same conflict-safety reason as on Windows.

The Linux host needs read access to the R400 event device. Check it with:

```bash
test -r /dev/input/by-id/usb-Logitech_USB_Receiver-event-if00 \
  && echo "R400 access ready" \
  || echo "R400 access needs a device-specific udev rule"
```

Do not grant broad access to every keyboard event device. If access is denied,
install a device-specific `uaccess` rule through your distribution's normal
administrator workflow, reconnect the receiver, and sign out/in if your login
manager does not refresh the ACL immediately:

```text
SUBSYSTEM=="input", KERNEL=="event*", ATTRS{idVendor}=="046d", ATTRS{idProduct}=="c538", TAG+="uaccess"
```

### iKKEGOL B0CD3ZS7G2 triple foot pedal

The iKKEGOL Upgraded 2023 Digital Optical USB Triple Foot Pedal (Amazon ASIN
`B0CD3ZS7G2`, model `10149C`) works out of the box as a standard HID keyboard.
LotPilot accepts only the known pedal USB IDs `3553:b001` and
`1a86:e026`, on their keyboard event interface. The factory keys need no
programming: **A = Auction**, **B = Inspect** (the existing safe no-click
inspection command), and **C = Giveaway**. Input is observed without grabbing
or suppressing ordinary keyboard delivery.

If desired, program the pedal once with the official Windows **FootSwitch**
software or the open-source Linux **pedalctl/footswitch** tools. Mappings are
stored in pedal firmware and persist when moved between computers. LotPilot also
accepts the optional convention **F20 = Auction**, **F22 = Inspect**,
**F21 = Giveaway**, but some firmware revisions emit media actions for keys
above F12; the A/B/C defaults are therefore recommended. LotPilot never
writes pedal firmware or downloads/vendor-bundles a programming utility.

Linux needs read access to the specific pedal event node. Use your
distribution's administrator workflow to install only narrow `uaccess` rules,
then reconnect the pedal (do not grant an input group or world-readable access):

```text
SUBSYSTEM=="input", KERNEL=="event*", ATTRS{idVendor}=="3553", ATTRS{idProduct}=="b001", TAG+="uaccess"
SUBSYSTEM=="input", KERNEL=="event*", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="e026", TAG+="uaccess"
```

## Why this architecture

Firefox starts the platform-native C++ host once and keeps it connected through native messaging. It registers directly with KDE's `KGlobalAccel` on Linux or `RegisterHotKey` on Windows, so a hotkey does not launch a shell, Python process, browser, or desktop automation tool:

```text
Linux:   KGlobalAccel   → native host → Firefox native messaging → Whatnot content script
Windows: RegisterHotKey / device-filtered Raw Input → native host → Firefox native messaging → Whatnot content script
```

This is faster and more reliable on Wayland than coordinate clicking, `ydotool`, Firefox remote debugging, or launching a command for every keypress. Chrome would not make this path faster, so Firefox remains the target.

## Windows development distribution

Windows 10/11 x64 support is currently available as an **unsigned development bundle**, not a trusted public release. Build it on Linux with MinGW-w64 and the existing Mozilla-signed XPI:

```bash
pnpm run build:windows:bundle
```

The resulting `dist/windows/lotpilot-*-windows-x86_64-development.zip` contains two framework-free Win32 executables, a checksum-verifying per-user PowerShell installer, an ownership-checking uninstaller, and Windows-specific instructions. The `%LOCALAPPDATA%\Programs\Quick Swap Tools` installation directory is retained as a compatibility internal so existing installs and saved settings remain usable; the Start Menu displays **LotPilot**. Registration is limited to the exact current-user Firefox native-messaging key and does not request administrator access.

Until AMO credentials are available, this development bundle may contain the
Mozilla-signed 0.1.0 extension renamed
`LotPilot-0.1.0-legacy-signed-compat-firefox.xpi`. Firefox will still show its
old Quick Swap Tools manifest name. It keeps the stable extension ID but is
clearly a compatibility artifact and does not match the branded 0.2.0 source.

Unsigned executables may trigger SmartScreen. A public Windows release remains blocked on Authenticode signing and a real Windows 10/11 Firefox end-to-end test. See [`packaging/windows/README.md`](packaging/windows/README.md) for development installation details and controller limitations.

## Install a signed release

After a branded 0.2.0 XPI has been signed, download the
`lotpilot-*-linux-kde-x86_64.tar.gz` bundle and matching
`.sha256` sidecar from the [latest GitHub release](../../releases/latest), then
verify and extract it:

```bash
sha256sum -c lotpilot-0.2.0-linux-kde-x86_64.tar.gz.sha256
tar -xzf lotpilot-0.2.0-linux-kde-x86_64.tar.gz
cd lotpilot-0.2.0-linux-kde-x86_64
./install.sh
```

The release installer does not need root. It installs the native host, safely
claims the two KDE shortcuts, and prints the local path to the included
Mozilla-signed XPI. Open that XPI in Firefox and click **Add**.

Requirements:

- x86-64 Linux;
- KDE Plasma 6 with glibc 2.34+, Qt 6.11+, and KDE Frameworks 6.12+
  GlobalAccel and XmlGui runtime libraries;
- Firefox 140 or newer;
- Python 3 for the one-time native-messaging manifest setup.

### Install from source for development

```bash
cd ~/quick-swap-tools
./scripts/install.sh
```

The source installer:

- refuses to overwrite existing installations, manifests, or desktop entries;
- rolls back files and shortcut changes if installation fails;
- builds and installs the native host at `~/.local/lib/quick-swap-tools/`;
- installs the Firefox native-host manifest;
- packages the extension;
- backs up the prior `Super+A` and `Super+G` KDE assignments;
- assigns the shortcuts to LotPilot.

Before installation, `Super+A` walked through Plasma Activities and `Super+G` opened Grid View. Those bindings are saved and the uninstall script restores them.

### Branded Firefox release signing

The 0.2.0 branded source expects this artifact after Mozilla signing:

```text
dist/release/LotPilot-0.2.0-firefox.xpi
```

That branded signed XPI does not exist yet. Do not substitute or relabel the old
signed 0.1.0 XPI as a source-matching 0.2.0 release. Once signed, the update will
install in place because the permanent extension ID remains
`quick-swap-tools@onibyts.com`.

### Temporary development builds only

When testing unpublished source changes, Firefox can load the local source temporarily:

1. Open `about:debugging#/runtime/this-firefox`.
2. Click **Load Temporary Add-on…**.
3. Select:
   `~/.local/lib/quick-swap-tools/extension/manifest.json`

When reloading a changed development build, refresh any already-open Whatnot tab once so Firefox replaces its old content script.

Release signing is configured through `pnpm run sign:unlisted`. See [`docs/FIREFOX_SIGNING.md`](docs/FIREFOX_SIGNING.md) for Mozilla account setup, secure credential handling, signed installation, updates, and friend distribution.

## Safe verification

Never test `Super+A` or `Super+G` against an active show unless you intend to start the queued item.

After loading the extension, verify the native connection:

```bash
pgrep -af quick-swap-host
qdbus6 org.kde.kglobalaccel /component/quick_swap_tools \
  org.kde.kglobalaccel.Component.isActive
```

Inspect current Whatnot control labels **without clicking**:

```bash
qdbus6 org.kde.kglobalaccel /component/quick_swap_tools \
  org.kde.kglobalaccel.Component.invokeShortcut inspect-controls
cat ~/.local/state/quick-swap-tools/last-result.json
```

## Whatnot selector safety

LotPilot re-queries the live React DOM for every action. It uses visible, enabled controls and exact normalized labels rather than generated CSS classes. The script first selects the requested `Auction`/`Auctions` or `Giveaway`/`Giveaways` tab. It waits for Whatnot's React UI to finish changing modes, then uses one unique visible action control. Current accepted action labels are:

- `Start next auction`
- `Run next auction`
- `Start auction`
- `Start next giveaway`
- `Run next giveaway`
- `Start giveaway`
- `Run next`

If Whatnot uses a different label, run the safe inspection command and add that exact label with a regression test before changing production matching.

## Bluetooth scroll ring later

Most TikTok-style Bluetooth rings identify as HID keyboards or media remotes. Map two physical ring inputs to `Super+A` and `Super+G`; no browser or Whatnot code changes are required. If the ring emits only wheel events, add a device-specific input adapter that forwards deliberate ring gestures to the same two native actions. The native host's 150 ms key-bounce filter protects against switch chatter without blocking normal rapid input.

## Uninstall

```bash
cd ~/quick-swap-tools
./scripts/uninstall.sh
```

This removes the host, extension files, native manifest, and launcher, then restores the previous KDE shortcut values.

## Development checks

```bash
npm run check
```

This builds the native host and runs the JavaScript selector/routing tests, Python protocol tests, native-process integration test, and syntax checks.

Build the distributable Linux KDE bundle from an already signed XPI. Release
builds use a clean standard Arch Linux Docker image so the native binary does
not inherit CachyOS x86-64-v3 compiler defaults:

```bash
pnpm run build:release
```

## License

Copyright © 2026 OniByts.

LotPilot is free software licensed under the GNU General Public License,
version 3 or any later version (`GPL-3.0-or-later`). See [`LICENSE`](LICENSE).
