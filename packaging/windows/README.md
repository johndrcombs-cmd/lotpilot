# LotPilot for Windows — development bundle

This bundle targets **64-bit Windows 10/11** and **Firefox**. It installs only for the current Windows user and does not request administrator access.

> This is an unsigned development build. Windows SmartScreen may warn about both executables. Public releases should be Authenticode-signed before distribution to nontechnical users.

## Install

1. Extract the entire ZIP to a normal folder on a local drive.
2. Open Windows PowerShell in the extracted folder.
3. For this explicitly unsigned development build, run:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1 -AllowUnsignedDevelopment
   ```

4. In Firefox, press **Ctrl+O**, select the included `LotPilot-*-firefox.xpi`, and approve **Add**.
5. Open **LotPilot** from the Windows Start Menu to configure controls.

The installer verifies every payload checksum, the extension ID, and expected Mozilla XPI signature metadata before writing anything. Firefox performs the authoritative cryptographic extension-signature check when you add the XPI. The installer retains `%LOCALAPPDATA%\Programs\Quick Swap Tools` as a legacy compatibility internal, registers the Firefox native host under the current user's Mozilla registry key, and creates the user-visible **LotPilot** Start Menu shortcut.

Until Mozilla signs the branded 0.2.0 source, the included file may be named
`LotPilot-0.1.0-legacy-signed-compat-firefox.xpi`. It is the genuine old signed
0.1.0 package, so Firefox displays its old Quick Swap Tools name. It preserves
update compatibility but must not be mistaken for a signed branded build.

## Default controls

- **Ctrl+Shift+F9** — start the next Auction
- **Ctrl+Shift+F10** — start the next Giveaway
- **Logitech R400 Previous** — start the next Auction
- **Logitech R400 Next** — start the next Giveaway
- **iKKEGOL left (`A`)** — start the next Auction
- **iKKEGOL middle (`B`)** — safe Inspect (no click)
- **iKKEGOL right (`C`)** — start the next Giveaway

Windows reserves many `Win+…` shortcuts, including common combinations such as `Win+A` and `Win+G`, so the Windows defaults intentionally differ from Linux.

To rebind a control, click its binding, press the desired key or key combination, then click **Apply**. The configurator updates a running Firefox host and restores the prior pair if Windows rejects either replacement.

### Logitech R400

The built-in Logitech R400 profile is automatic and device-specific. It accepts
Previous and Next only from the R400 receiver's `VID_046D`, `PID_C538`, `MI_00`
keyboard interface, so Page Up/Page Down from another keyboard do not trigger
LotPilot. Start Slideshow, Black Screen, and Laser are intentionally
unassigned. Windows still delivers the R400's ordinary Page Up/Page Down keys to
the foreground application. Page Up and Page Down are reserved and cannot also
be configured as global hotkeys, preventing one R400 press from producing
conflicting actions.

### iKKEGOL B0CD3ZS7G2 triple pedal

The iKKEGOL model `10149C` (ASIN `B0CD3ZS7G2`) works without programming. The
device-filtered Raw Input profile accepts only known USB IDs `3553:b001` and
`1a86:e026`: **A = Auction**, **B = Inspect** (the existing safe no-click
command), and **C = Giveaway**. Windows still delivers those normal keyboard
events to the foreground application; LotPilot does not use
`RIDEV_NOLEGACY`.

For optional one-time changes, use iKKEGOL's official Windows **FootSwitch**
software. It writes the mapping to pedal firmware, where it persists across
reboots and computers. LotPilot also recognizes the convention
**F20 = Auction**, **F22 = Inspect**, **F21 = Giveaway**, but output above F12
is firmware-dependent and is not guaranteed on every revision. The shipped
A/B/C defaults are recommended. Opening the configurator never writes hardware,
and the bundle neither vendors nor downloads programming software. Linux users
can program with open-source **pedalctl/footswitch**; Linux event-read access
must use narrow per-ID `uaccess` rules, never broad input-group/world access:

```text
SUBSYSTEM=="input", KERNEL=="event*", ATTRS{idVendor}=="3553", ATTRS{idProduct}=="b001", TAG+="uaccess"
SUBSYSTEM=="input", KERNEL=="event*", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="e026", TAG+="uaccess"
```

### Tartarus, macro pads, and controllers

LotPilot records the keyboard key emitted by a device. `F13`–`F24` are recommended for a Razer Tartarus or other macro pad. If two physical keyboards emit the same key, the normal Windows global-hotkey API cannot distinguish which device produced it. Map gamepad, HID-only, or vendor-only buttons to keyboard keys before recording them.

## Uninstall

Close Firefox, then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$env:LOCALAPPDATA\Programs\Quick Swap Tools\uninstall.ps1"
```

The uninstaller verifies that the manifest, registry entry, and Start Menu shortcut belong to this installation before deleting them. It accepts strict ownership markers and shortcut properties from both pre-rebrand Quick Swap Tools installs and new LotPilot installs. It removes only the four owned shortcut-setting values and preserves unrelated values or subkeys. Remove the Firefox extension separately from `about:addons` if it is no longer wanted.

## Signing status

The included 0.1.0 compatibility XPI is Mozilla-signed; no signed branded 0.2.0
XPI exists yet. The Windows executables and development ZIP are not yet
Authenticode-signed. Do not relabel this development artifact as a production
release.

Third-party license notices for the vendored JSON for Modern C++ parser are included in `THIRD_PARTY_NOTICES.md`.
