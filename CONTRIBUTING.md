# Contributing

Thanks for helping improve LotPilot.

## Development setup

LotPilot currently targets x86-64 Linux with KDE Plasma 6 and 64-bit Windows
10/11, using Firefox 140 or newer. Linux native builds require Qt 6, KDE
Frameworks 6 GlobalAccel, a C++20 compiler, Python 3, Node.js 20+, and pnpm;
Windows cross-builds additionally require MinGW-w64.

```bash
pnpm install
pnpm run check
```

## Safety requirements

A shortcut can start a real Whatnot auction or giveaway. Changes to selectors, routing, command sequencing, or shortcut registration must:

- include regression tests;
- reject hidden, disabled, covered, missing, or ambiguous controls;
- avoid using a live show for testing unless the tester intends to start the item;
- preserve the no-click inspection path;
- never silently replace unexpected global shortcuts.

## Pull requests

Keep changes focused and describe how they were verified. Do not commit AMO credentials, account information, live-show URLs, runtime state, build outputs, or signed artifacts.

By contributing, you agree that your contribution is licensed under GPL-3.0-or-later.
