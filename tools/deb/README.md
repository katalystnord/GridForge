# The .deb packages

    tools/deb/make-deb.sh          # into dist/deb/

Builds, from the commit checked out, a Debian source package and two
binary packages for Ubuntu 26.04 and newer (packaging/debian):

| package | holds |
|---|---|
| `gridforge` | the window and `gf-study`; GridForge's Netgen, privately in `/usr/lib/gridforge` |
| `gridforge-calculix` | the CalculiX toolkit: its manifest, adapter and ccx 2.23 in `/usr/lib/gridforge/toolkits/calculix` |

`gridforge` recommends `gridforge-calculix` (toolkits are optional,
tenet 11). Everything else (Qt, VTK, OpenCASCADE, yaml-cpp, ARPACK,
LAPACK) comes from the archive. The source package carries Netgen, ccx
and SPOOLES as component tarballs, so it holds the source of the ccx it
ships.

    sudo apt install ./dist/deb/gridforge_0.0.1-1_amd64.deb ./dist/deb/gridforge-calculix_0.0.1-1_amd64.deb

Not yet "the Debian way" (David, 2026-11-18): Netgen and ccx are carried,
not taken from the archive, because the archive's versions are older
than GridForge's. `packaging/debian/README.source` says what has to be
true to change that, and that it is a change to `debian/control` and
`debian/rules` only.

First built 2026-11-18: lintian with no errors and five warnings (no
manual pages yet; an initial upload closes no bug, which matters only for
an archive upload; ccx built without debug symbols). Unpacked, its
`gf-study` solved the box with its ccx 2.23, and its window ran.
