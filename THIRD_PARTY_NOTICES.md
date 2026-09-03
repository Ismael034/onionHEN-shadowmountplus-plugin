# Third-party notices

## ShadowMountPlus

- Project: [drakmor/ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus)
- Upstream base: tag `1.6beta16`, commit
  `8566c0294cbf37b55375602e950a0e6b6bb928d7`
- Integrated snapshot source: OnionHEN commit
  `a534adcd600485edd8569c1773580c1c49ff8a6a`
- License: GNU General Public License v3.0
- Vendored paths: `third_party/ShadowMountPlus/include`,
  `third_party/ShadowMountPlus/src`, configuration template, and notification
  icon

The OnionHEN snapshot contains project-specific fixes beyond the named
upstream tag. The standalone `main.c` is retained for source provenance but is
not compiled. `source/shadowmount_core.c` adapts its runtime contract to the
OnionHEN-managed process, and `source/shadowmount_service.c` owns its worker
thread and graceful shutdown.

The upstream copyright and GPL text are preserved in
`third_party/ShadowMountPlus/LICENSE`.

## SQLite

- Project: [SQLite](https://www.sqlite.org/)
- Version: 3.53.4, amalgamation `sqlite-amalgamation-3530400`
- License: public domain
- Vendored paths: `third_party/sqlite/sqlite3.c` and `sqlite3.h`

SQLite is compiled into the plugin because the PS5 payload SDK does not ship
the required `libsqlite3` archive. Its source contains the upstream dedication
and license notice.
