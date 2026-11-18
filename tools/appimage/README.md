# The AppImage

GridForge as one file that runs on Linux distributions at least as new as
the one it was built on, ccx included (David, 2026-11-18).

    tools/appimage/make-appimage.sh            # into dist/
    tools/appimage/smoke-test.sh dist/GridForge-<version>-x86_64.AppImage

`make-appimage.sh` builds Netgen (`tools/build-netgen.sh`) and ccx 2.23
(`tools/build-ccx.sh`, from its released source at a pinned SHA-256),
then GridForge in Release, and packs them with linuxdeploy and its Qt
plugin (pinned as TenSense's). Inside:

```
usr/bin/GridForge, gf-study
usr/lib/gridforge/toolkits/calculix/  toolkit.json, gf-calculix, ccx
usr/share/doc/gridforge/                LICENSE, NOTICE, README.md
usr/share/doc/ccx/                      gpl-2.0.txt, LOGBOOK, README.INSTALL
usr/lib/                                Qt, VTK, OpenCASCADE, Netgen, ARPACK, ...
```

The adapter finds the ccx beside it; GridForge finds the toolkits
installed beside it before any source tree.

`smoke-test.sh` runs the AppImage as a user would: it says its version,
solves the box study with the ccx it carries (no `GF_CCX`, no ccx on
PATH), and carries ccx's licence and GridForge's NOTICE.

## A release

Beside the AppImage on the release page, also:

- `ccx_2.23.src.tar.bz2` and `spooles.2.2.tgz`, exactly as
  `tools/build-ccx.sh` fetched them (`build-ccx/downloads/`; their SHA-256
  are in the script): the source of the ccx it carries, which ccx's
  licence (GPL-2.0, section 3(a)) asks to accompany it;
- `tools/build-ccx.sh` itself, which says how that source was built.

First built 2026-11-18: 110 MB, GridForge 0.0.1, on Ubuntu 26.04; its
window opened on a private X server, and the box solved by its ccx.
