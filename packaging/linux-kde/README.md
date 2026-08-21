# Quick Swap Tools 0.1.0 — Linux KDE bundle

This bundle installs the native KDE shortcut host and includes the Mozilla-signed Firefox extension.

## Requirements

- x86-64 Linux
- KDE Plasma 6 on Wayland or X11
- glibc 2.34 or newer
- Qt 6.11 or newer
- KDE Frameworks 6.12 or newer, including GlobalAccel, GuiAddons, and XmlGui
- Firefox 140 or newer
- Python 3 (used only during installation to write the native-messaging manifest)

The prebuilt bundle targets current rolling-release distributions rather than
every Plasma 6 distribution. The installer checks dynamic-library compatibility
before making changes. On Arch/CachyOS, the runtime dependencies are provided
by `qt6-base`, `kglobalaccel`, and `kxmlgui`.

## Install

Download both the `.tar.gz` bundle and its `.sha256` sidecar from the GitHub
release, then verify the download before extracting it:

```bash
sha256sum -c quick-swap-tools-0.1.0-linux-kde-x86_64.tar.gz.sha256
tar -xzf quick-swap-tools-0.1.0-linux-kde-x86_64.tar.gz
cd quick-swap-tools-0.1.0-linux-kde-x86_64
```

The installer also verifies every bundled executable and payload against the
archive's internal `SHA256SUMS` file before making changes.

```bash
./install.sh
```

The installer never requires root. It checks for unexpected shortcut conflicts before changing them, preserves the normal Plasma `Super+A` and `Super+G` assignments, and prints the path to the signed Firefox XPI.

Open that XPI in Firefox and click **Add**. Quick Swap Tools then remains installed across Firefox restarts.

## Controls

- `Super+A`: switch to Auctions and start the next auction.
- `Super+G`: switch to Giveaways and start the next giveaway.
- Logitech R400 Previous: switch to Auctions and start the next auction.
- Logitech R400 Next: switch to Giveaways and start the next giveaway.
- iKKEGOL left (`A`): start the next auction.
- iKKEGOL middle (`B`): run safe Inspect (no click).
- iKKEGOL right (`C`): start the next giveaway.

The R400 profile matches only the `046d:c538` Logitech USB Receiver. It does
not grab or suppress the device, so Page Up/Page Down continue to reach the
foreground application. Page Up and Page Down cannot also be selected as KDE
global shortcuts. The host rediscovers the receiver after a hot unplug.

Check whether your desktop user can read the receiver:

```bash
test -r /dev/input/by-id/usb-Logitech_USB_Receiver-event-if00
```

If that fails, configure a device-specific `uaccess` udev rule through your
distribution's administrator workflow. Do not grant broad access to all input
devices:

```text
SUBSYSTEM=="input", KERNEL=="event*", ATTRS{idVendor}=="046d", ATTRS{idProduct}=="c538", TAG+="uaccess"
```

### iKKEGOL B0CD3ZS7G2 triple pedal

The iKKEGOL model `10149C` (ASIN `B0CD3ZS7G2`) is supported only at the known
USB IDs `3553:b001` and `1a86:e026` and only through a matching keyboard event
interface. Its shipped HID keyboard defaults require no programming:
**A = Auction**, **B = Inspect** (safe/no click), **C = Giveaway**. The host
reads press events without grabbing the device, so normal A/B/C delivery is not
suppressed.

Optional one-time programming is available with the open-source Linux
**pedalctl/footswitch** tools (or the official Windows **FootSwitch** software).
The mapping is stored persistently in pedal firmware. Quick Swap Tools also
accepts **F20 = Auction**, **F22 = Inspect**, **F21 = Giveaway**, but F13+ output
is firmware-dependent and not guaranteed; keep the A/B/C defaults unless the
optional mapping is verified. Opening Quick Swap Tools never mutates hardware,
and this bundle does not include or download third-party programming binaries.

If the pedal event node is not readable, install only these narrow `uaccess`
rules through your distribution's administrator workflow and reconnect it.
Never use broad input-group or world-readable permissions:

```text
SUBSYSTEM=="input", KERNEL=="event*", ATTRS{idVendor}=="3553", ATTRS{idProduct}=="b001", TAG+="uaccess"
SUBSYSTEM=="input", KERNEL=="event*", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="e026", TAG+="uaccess"
```

### Configure a keyboard, macro pad, or controller

Open **Quick Swap Tools** from the application launcher, click a binding, and
press the desired key or button. Click **Apply** only when both mappings are
correct. Changes become active immediately and persist across restarts.

Keyboard-style devices such as the Razer Tartarus work directly. KDE receives
the key produced by the device, not its physical button number. If a keypad
button emits a normal letter, that letter on the main keyboard would also
trigger the action. Map controller buttons to unused keys such as `F13`–`F24`
for device-friendly bindings. Gamepads that do not emit keyboard keys require
an external mapper such as Input Remapper or Steam Input first.

## Uninstall

```bash
~/.local/lib/quick-swap-tools/uninstall.sh
```

Then remove Quick Swap Tools from `about:addons` in Firefox.

## Source and license

Corresponding source for this release:
https://github.com/johndrcombs-cmd/quick-swap-tools/tree/v0.1.0

Quick Swap Tools is licensed under GPL-3.0-or-later. See `LICENSE`.
