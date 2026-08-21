import json
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]


class LotPilotBrandingTests(unittest.TestCase):
    def test_extension_uses_lotpilot_branding(self):
        manifest = json.loads((ROOT / "extension/manifest.json").read_text())
        self.assertEqual(manifest["name"], "LotPilot")
        self.assertEqual(manifest["version"], "0.2.0")
        self.assertEqual(manifest["browser_action"]["default_title"], "LotPilot")
        self.assertIn("auction and giveaway", manifest["description"])
        self.assertIn("LotPilot:", (ROOT / "extension/background.js").read_text())

    def test_package_uses_lotpilot_branding(self):
        package = json.loads((ROOT / "package.json").read_text())
        self.assertEqual(package["name"], "lotpilot")
        self.assertEqual(package["version"], "0.2.0")
        self.assertEqual(
            package["description"],
            "Hands-free auction and giveaway controls for Whatnot live sellers",
        )

    def test_linux_ui_and_installers_use_lotpilot_branding(self):
        config = (ROOT / "native/quick-swap-config.cpp").read_text()
        host = (ROOT / "native/quick-swap-host.cpp").read_text()
        source_installer = (ROOT / "scripts/install.sh").read_text()
        release_installer = (ROOT / "packaging/linux-kde/install.sh").read_text()
        for text in (config, host, source_installer, release_installer):
            self.assertIn("LotPilot", text)
        for installer in (source_installer, release_installer):
            self.assertIn("Name=LotPilot", installer)
            self.assertIn("LotPilot KDE hotkey bridge", installer)

    def test_windows_ui_and_installer_use_lotpilot_branding(self):
        native = (ROOT / "native/windows/quick-swap-tools.cpp").read_text()
        installer = (ROOT / "packaging/windows/install.ps1").read_text()
        uninstaller = (ROOT / "packaging/windows/uninstall.ps1").read_text()
        self.assertIn('L"LotPilot — Configure controls"', native)
        self.assertIn('"LotPilot.lnk"', installer)
        self.assertIn('product = "LotPilot"', installer)
        self.assertIn('"LotPilot Windows hotkey bridge"', installer)
        self.assertIn('"LotPilot"', uninstaller)
        self.assertIn('"Quick Swap Tools"', uninstaller)

    def test_current_docs_use_lotpilot_branding(self):
        current_docs = (
            "README.md",
            "CONTRIBUTING.md",
            "SECURITY.md",
            "THIRD_PARTY_NOTICES.md",
            "docs/FIREFOX_SIGNING.md",
            "packaging/linux-kde/README.md",
            "packaging/windows/README.md",
        )
        for relative in current_docs:
            with self.subTest(path=relative):
                text = (ROOT / relative).read_text()
                self.assertIn("LotPilot", text)

    def test_builders_use_lotpilot_artifact_names(self):
        release = (ROOT / "scripts/build-release.sh").read_text()
        windows = (ROOT / "scripts/build-windows-bundle.sh").read_text()
        self.assertIn("LotPilot-$VERSION-firefox.xpi", release)
        self.assertIn("lotpilot-$VERSION-linux-kde-x86_64", release)
        self.assertIn("lotpilot-${VERSION}-windows-x86_64-development.zip", windows)
        self.assertIn("Quick-Swap-Tools-*-firefox.xpi", windows)
        self.assertIn("LotPilot-*-firefox.xpi", windows)


class StableCompatibilityIdentifierTests(unittest.TestCase):
    def test_extension_native_host_and_kde_ids_are_unchanged(self):
        manifest = json.loads((ROOT / "extension/manifest.json").read_text())
        background = (ROOT / "extension/background.js").read_text()
        linux_host = (ROOT / "native/quick-swap-host.cpp").read_text()
        linux_config = (ROOT / "native/quick-swap-config.cpp").read_text()

        self.assertEqual(
            manifest["browser_specific_settings"]["gecko"]["id"],
            "quick-swap-tools@onibyts.com",
        )
        self.assertIn("com.onibyts.quickswap", background)
        for source in (linux_host, linux_config):
            self.assertIn('"quick-swap-tools"', source)
            self.assertIn('"next-auction"', source)
            self.assertIn('"next-giveaway"', source)


if __name__ == "__main__":
    unittest.main()
