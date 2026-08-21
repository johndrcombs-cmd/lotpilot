# Changelog

All notable changes to LotPilot are documented here.

## Unreleased

## 0.2.0 — 2026-08-21

- Rebrand Quick Swap Tools as LotPilot, with the tagline “Hands-free controls
  for Whatnot live sellers,” while preserving the Firefox extension ID, native
  messaging host name, KDE action IDs, legacy install roots, and saved hotkeys.
- Rename new Linux, Windows, and Firefox artifacts for LotPilot. The signed
  Quick Swap Tools 0.1.0 XPI remains a clearly labeled compatibility artifact
  until Mozilla signs the branded 0.2.0 source.

- Add device-filtered iKKEGOL B0CD3ZS7G2 triple-pedal support on Linux and
  Windows for USB IDs `3553:b001` and `1a86:e026`: factory A/B/C maps to
  Auction/safe Inspect/Giveaway, with optional firmware-dependent F20/F22/F21.
- Add device-filtered Logitech R400 support on Linux: Previous starts the next
  auction and Next starts the next giveaway without suppressing normal Page
  Up/Page Down delivery or accepting those keys from other keyboards.
- Refresh the Windows and KDE desktop configurators with a shared modern,
  dark-only layout, clearer action cards and status feedback, and visible
  keyboard focus states.
- Add device-filtered Logitech R400 support on Windows: Previous starts the next
  auction and Next starts the next giveaway without treating Page Up/Page Down
  from other keyboards as Quick Swap controls.
- Add a game-style KDE control configurator for keyboards, macro pads, and
  controller buttons mapped to keyboard keys.
- Preserve user-selected global shortcuts through KDE `KGlobalAccel` and warn
  when an unmodified global key could also trigger from the main keyboard.
- Make the source-development installer refuse existing installations and
  unrelated desktop entries, and roll back files, manifests, state, and KDE
  shortcuts if installation fails.
- Exclude local Mozilla upload-tracking files from development packages.
- Add a dependency-light Windows 10/11 x64 native-messaging host using
  `RegisterHotKey`, a game-style Win32 shortcut configurator, and safe
  transactional live rebinding.
- Add a no-admin Windows development ZIP with checksum and Authenticode policy,
  closed per-user Firefox registration, ownership-checking uninstall, and
  native Windows CI. Public Windows release remains pending code signing and
  real Windows/Firefox acceptance testing.

## 0.1.0 — 2026-08-02

- Add persistent KDE Plasma global shortcuts for auctions and giveaways.
- Add safe semantic Whatnot control discovery and cross-mode transitions.
- Add Firefox native messaging and Mozilla-signed extension distribution.
- Add a rootless Linux KDE release installer and uninstaller.
- Add shortcut-conflict protection and restoration of Plasma defaults.