<p align="center">
  <img src="assets/logo.png" alt="OnionHEN" height="128" width="128"/>
</p>

<p align="center">
  <b>OnionHEN ShadowMount+ Plugin</b><br/>
  Automatic PS5 game image scanning and mounting as a managed OnionHEN plugin
</p>

<p align="center">
  <b>English</b> · <a href="README_ZH.md">简体中文</a>
</p>

This repository packages ShadowMountPlus as a standalone OnionHEN plugin.
OnionHEN discovers the ELF, validates its embedded `.onion_plugin` descriptor,
owns its process lifecycle, and removes its dynamic UI contribution when the
plugin stops.

> [!WARNING]
> Mounting images can cause shutdown problems or data corruption on internal
> drives, especially on older firmware. Test only on hardware and data you can
> recover.

## Features

- Automatic scanning, mounting, and registration of supported game dumps
- `.ffpkg`, `.exfat`, `.ffpfs`, and experimental `.ffpfsc` image support
- Dynamic OnionHEN page with an immediate scan action and an editable list of
  custom scan paths
- Graceful stop, reload, replacement, deletion, and rest-mode recovery through
  the OnionHEN plugin manager
- Existing ShadowMountPlus configuration, logging, fakelib, and kstuff
  integration retained
- No package container or extraction step; metadata is embedded in the ELF

The plugin is marked `AUTO_START` because the scanner is a long-running
background service. Kstuff-lite v1.07 or newer must already be active.

## Requirements

- An OnionHEN build with external plugin discovery and dynamic UI support
- A supported jailbroken PS5 firmware and Kstuff-lite v1.07+
- [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk)
- CMake 3.20 or newer and Ninja
- Git and Python 3.9 or newer

## Build

```sh
export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
cmake --preset ps5
cmake --build --preset ps5
```

The output is `build-ps5/bin/shadowmountplus.elf`. The build validates plugin
ID `SMPL00001`, version `1.00`, and the embedded SDK descriptor.

To build against a local SDK checkout:

```sh
cmake --preset ps5 \
  -DONIONHEN_PLUGIN_SDK_SOURCE=/path/to/onionHEN-plugin-sdk
cmake --build --preset ps5
```

Delete `build-ps5/` before switching between a downloaded SDK and a local SDK
checkout.

## Install

Upload the completed ELF atomically:

```text
/data/OnionHEN/plugins/SMPL00001.installing
    rename after upload
/data/OnionHEN/plugins/SMPL00001.elf
```

OnionHEN detects and validates the final `.elf`, then starts the plugin. Open
**★ OnionHEN Plugins**, select **ShadowMount+**, and open its contributed page
to request an immediate scan. Replacing the ELF restarts the managed process;
deleting it stops the scanner and removes its UI.

Runtime files:

| Path | Purpose |
| --- | --- |
| `/data/OnionHEN/plugins/SMPL00001.elf` | Installed plugin |
| `/data/OnionHEN/SMPL00001.log` | Plugin lifecycle and daemon-session log |
| `/data/shadowmount/config.ini` | ShadowMountPlus configuration |
| `/data/shadowmount/debug.log` | Scanner and mount log |
| `/data/shadowmount/autotune.ini` | Automatically learned overrides |

The first run creates `config.ini` from the bundled upstream template. See
[`third_party/ShadowMountPlus/config.ini.example`](third_party/ShadowMountPlus/config.ini.example)
for all options and scan-path defaults.

### Scan paths

By default ShadowMount+ scans a built-in list of folders on every attached
volume, mirroring both etaHEN's and OnionHEN's conventions:
`homebrew`, `etaHEN/games`, and `OnionHEN/games`, under `/data`, `/mnt/ext0`
(internal M.2), `/mnt/ext1`, and each `/mnt/usb0`–`/mnt/usb7`, plus the raw
`/mnt/usbN`/`/mnt/extN` roots themselves.

Open **★ OnionHEN Plugins → ShadowMount+ → Scanner → Scan Paths** to add up
to 6 extra folders of your own — enter an absolute path in any "Custom path"
slot, or clear one to remove it. Paths added this way are additive: they
scan alongside the built-in list, not instead of it (this plugin sets
`scan_include_defaults=1` for you when you use this UI). Hand-editing
`scanpath=` lines directly in `config.ini` keeps the older upstream behavior
instead, where any `scanpath=` entry replaces the built-in list unless you
add `scan_include_defaults=1` yourself.

## Architecture

```text
OnionHEN plugin manager
  -> trusted SDK session
  -> source/main.c                    process lifecycle and UI event loop
     -> source/plugin_ui.c            UI document and action validation
     -> source/shadowmount_service.c  synchronized worker lifecycle
        -> source/shadowmount_core.c  standalone payload adapter
           -> third_party/ShadowMountPlus + SQLite
```

The descriptor declares notify, IPC, UI, process, and kernel capabilities with
`AUTO_START`, `LONG_RUNNING`, and `STOP_SUPPORTED`. The main thread owns the SDK
session and UI event pump while the scanner runs on one service-owned worker.
Shutdown requests wake the scanner, complete mount/database cleanup, and join
the worker before the process disconnects from OnionHEN.

## Contributing and security

Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.
Participation is governed by [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Report
security-sensitive issues privately according to [SECURITY.md](SECURITY.md).

## Credits and license

This plugin is based on
[`drakmor/ShadowMountPlus`](https://github.com/drakmor/ShadowMountPlus) and
retains its GPL notices. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
for source provenance and local integration notes.

The repository is licensed under the [GNU General Public License v3.0](LICENSE).
OnionHEN is an unofficial homebrew project and is not affiliated with Sony
Interactive Entertainment. Use it only on hardware you own and at your own
risk.
