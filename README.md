<p align="center">
  <img src="assets/logo.png" alt="OnionHEN" height="128" width="128"/>
</p>

<p align="center">
  <b>OnionHEN Plugin Boilerplate</b><br/>
  A ready-to-build starting point for standalone OnionHEN plugins
</p>

<p align="center">
  <a href="README_ZH.md">简体中文</a>
  ·
  <b>English</b>
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="license"/></a>
  <img src="https://img.shields.io/badge/Platform-PlayStation%205-003791?style=flat&logo=playstation" alt="PlayStation 5"/>
  <img src="https://img.shields.io/badge/C-00599C?style=flat&logo=c&logoColor=white" alt="C"/>
  <img src="https://img.shields.io/badge/Build-CMake-064F8C?style=flat&logo=cmake" alt="CMake"/>
</p>

This repository is a minimal, production-oriented template for an OnionHEN
plugin. It builds a normal PS5 ELF with an embedded `.onion_plugin` descriptor;
there is no package, archive, or separate manifest to maintain.

The included example connects to the OnionHEN daemon, opens a plugin session,
registers a dynamic settings UI, handles toggle/list/input/action events, and
unregisters its UI during shutdown.

## Requirements

- [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk)
- CMake 3.20 or newer
- Ninja
- Git and Python 3.9 or newer

## Create your plugin

Use this repository as a GitHub template or clone it, then change the plugin
metadata at the top of [`CMakeLists.txt`](CMakeLists.txt):

```cmake
set(ONION_PLUGIN_TARGET example_plugin)
set(ONION_PLUGIN_ID ONIO10001)
set(ONION_PLUGIN_VERSION 1.00)
set(ONION_PLUGIN_NAME "Example Plugin")
```

`ONION_PLUGIN_ID` must be four ASCII letters followed by five digits. Treat it
as a permanent application identity after publishing. Versions use `N.NN`.
The configured values generate `plugin_config.h` and are shared by the ELF
descriptor, UI contribution, logs, and post-build validation.

Replace the example behavior in `source/plugin_ui.c` and `source/main.c`. Keep
`source/plugin_descriptor.c` unless the plugin needs different capabilities or
lifecycle flags.

## Build

```sh
export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
cmake --preset ps5
cmake --build --preset ps5
```

The result is `build-ps5/bin/example_plugin.elf`. The build automatically
checks that the ELF contains a valid descriptor with the configured ID and
version.

The SDK dependency is pinned to a tested commit. During SDK development, use a
local checkout without changing the project:

```sh
cmake --preset ps5 \
  -DONIONHEN_PLUGIN_SDK_SOURCE=/path/to/onionHEN-plugin-sdk
cmake --build --preset ps5
```

Delete `build-ps5/` before switching between downloaded and local SDK sources.

## Install and run

Upload the ELF to the PS5 plugin directory using its descriptor ID as the file
name:

```text
/data/OnionHEN/plugins/ONIO10001.elf
```

For an atomic update, upload it as `ONIO10001.installing`, then rename it to
`ONIO10001.elf`. OnionHEN discovers the plugin and starts it automatically
because the example descriptor includes `AUTO_START`. Open **★ OnionHEN
Plugins**, select the plugin, then open its contributed settings page.

The example writes lifecycle and error messages to
`/data/OnionHEN/ONIO10001.log`. OnionHEN removes the contribution when the
plugin exits or disconnects; the plugin also unregisters explicitly on normal
`SIGINT`/`SIGTERM` shutdown.

## Project structure

```text
.
├── cmake/ps5-toolchain.cmake     PS5 compiler selection
├── include/plugin_config.h.in    generated metadata contract
├── include/plugin_ui.h           example UI module interface
├── source/main.c                 process/session lifecycle and event loop
├── source/plugin_descriptor.c    embedded ELF descriptor
├── source/plugin_ui.c            UI document and action handling
├── CMakeLists.txt                metadata, SDK dependency, plugin target
└── CMakePresets.json             standard PS5 configure/build commands
```

The split is intentional: `main.c` owns resources and shutdown ordering,
`plugin_ui.c` owns presentation state and actions, and the SDK owns protocol,
transport, validation, and ELF inspection. Plugin code depends only on the
public C ABI.

## Customization notes

- Request only capabilities the plugin actually uses.
- Remove `AUTO_START` for a manually started plugin.
- Keep `LONG_RUNNING` for a resident service and `STOP_SUPPORTED` when graceful
  termination is implemented.
- Node IDs and binding keys are stable protocol identifiers, not display text.
- Do not send pointers, C++ objects, or compiler-specific layouts across IPC.
- Validate action values in the plugin even though the UI validates input.
- Do not commit PS5 SDK files, proprietary libraries, keys, decrypted system
  files, console identifiers, logs, or built ELF files.

The current SDK provides dynamic UI and IPC. Daemon-backed logging,
notifications, and persistent configuration services may return
`ONION_E_NOT_SUPPORTED`; the example therefore keeps state in memory and uses
local file logging.

## Contributing and security

Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.
Participation is governed by [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Report
security-sensitive issues privately according to [SECURITY.md](SECURITY.md).

## Related projects

- [OnionHEN](https://github.com/aydencharles/onionHEN)
- [OnionHEN Plugin SDK](https://github.com/OnionBuddies/onionHEN-plugin-sdk)
- [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk)

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
Third-party components retain their respective licenses.

OnionHEN is an unofficial homebrew project and is not affiliated with Sony
Interactive Entertainment. Use it only on hardware you own and at your own
risk. No warranty is provided.
