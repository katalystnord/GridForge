# FluidStress: licences, trademarks and good citizenship (survey, 2026-11-14)

Scope: a LGPL-2.1-or-later C++/Qt 6/VTK desktop GUI that reads FreeCAD
exports, runs CalculiX (later OpenFOAM) as separate processes, and writes
files for ParaView. Everything below was read at the source on 2026-11-14
unless marked "not checked". Quotes are kept short; this is a summary of
what the sources say, not legal advice.

---

## 1. Licences, read from the licence files

| Project | Licence as found | Where read |
|---|---|---|
| CalculiX ccx 2.23 | GPL **version 2** (headers say "Free Software Foundation(version 2)"; no "or later") | `ccx_2.23.src.tar.bz2`: `src/ccx_2.23.c` header plus `src/gpl-2.0.txt`, from https://www.dhondt.de/ |
| CalculiX cgx 2.23 | GPL "version 2 of the License" in the `src/cgx.c` header (no "or later") | `cgx_2.23.all.tar.bz2`, `src/cgx.c`, from https://www.dhondt.de/ |
| | Note: the cgx web page http://www.calculix.de/ says "version 2 ... or (at your option) any later version". Source header and web page disagree; treat cgx as GPL-2.0 (safe reading) | |
| OpenFOAM (Foundation, openfoam.org) | GPL-3.0-or-later ("either version 3 ... or (at your option) any later") | `README.org`, `COPYING` and file headers in https://github.com/OpenFOAM/OpenFOAM-dev |
| OpenFOAM (openfoam.com) | GPL-3.0-or-later; README copyright now "2016-2025 OpenCFD Ltd" and "2026 Keysight Technologies" | `README.md` and `src/OpenFOAM/global/argList/argList.C` in https://gitlab.com/openfoam/core/openfoam (develop.openfoam.com now requires sign-in) |
| Gmsh | GPL-2.0-or-later **with an exception** allowing combination with Netgen, METIS, OpenCASCADE and ParaView code under their own licences | https://gitlab.onelab.info/gmsh/gmsh/raw/master/LICENSE.txt (same text at https://gmsh.info/LICENSE.txt) |
| Netgen | LGPL-2.1 | https://github.com/NGSolve/netgen/blob/master/LICENSE |
| OpenCascade (OCCT) | LGPL-2.1 plus "Open CASCADE exception (version 1.0)": object code using OCCT headers may use terms of your choice, if you give "prominent notice in supporting documentation" that it uses OCCT | https://github.com/Open-Cascade-SAS/OCCT (`LICENSE_LGPL_21.txt`, `OCCT_LGPL_EXCEPTION.txt`) |
| VTK | BSD-3-Clause (Ken Martin, Will Schroeder, Bill Lorensen) | https://gitlab.kitware.com/vtk/vtk/-/raw/master/Copyright.txt |
| ParaView | BSD-3-Clause (Kitware); clause 3 forbids using Kitware's name to "endorse or promote products derived" without permission | https://github.com/Kitware/ParaView/blob/master/Copyright.txt |
| FreeCAD | LGPL-2.1-or-later (`LICENSE` is LGPL 2.1; sources carry `SPDX-License-Identifier: LGPL-2.1-or-later`) | https://github.com/FreeCAD/FreeCAD (`LICENSE`, `src/App/Application.cpp`) |
| Qt Core, Gui, Widgets, Concurrent | LGPL-3.0 or GPL-2.0 (or commercial) | module pages, e.g. https://doc.qt.io/qt-6/qtwidgets-index.html, https://doc.qt.io/qt-6/qtconcurrent-index.html |
| Qt GPL-3.0-only modules (avoid) | Qt 6.12 licensing page lists: Canvas Painter, CoAP, **Graphs**, GRPC, HTTP Server, Lottie, MQTT, **Network Authorization**, Qml Compiler, **Quick 3D**, Quick 3D Physics, Quick Timeline, **Virtual Keyboard**, Wayland Compositor. Module pages also show **Qt Charts** and **Qt Data Visualization** (both deprecated) as GPL-3.0 only | https://doc.qt.io/qt-6/licensing.html, https://doc.qt.io/qt-6/qtcharts-index.html, https://doc.qt.io/qt-6/qtdatavisualization-index.html |

What follows for us (reading only what the texts say):

- Qt essentials are LGPL-3.0, not 2.1. Our "LGPL-2.1-or-later" choice lets
  the combined binary be distributed under LGPL-3.0 terms; plots should use
  VTK's charts, not Qt Charts or Qt Graphs (both GPL-3.0 only).
- Linking OCCT, Netgen or VTK into our binary is fine under their terms
  (OCCT asks for the notice in README/NOTICE, which matches
  `katalystnord/policy-on-attribution.md`).
- Gmsh and CalculiX are GPL: run them as programs (`gmsh`, `ccx`), do not
  link `libgmsh` into our binary unless we accept GPL for the combination.

### Separate process design under the GPL (FSF GPL FAQ)

Source: https://www.gnu.org/licenses/gpl-faq.en.html (the plain `.html`
URL returned 403 to curl; the `.en.html` copy was read).

- `#MereAggregation`: an aggregate is "separate programs, distributed
  together on the same CD-ROM or other media"; the GPL permits it. The FAQ
  says the line between two programs and one is "a legal question" and
  depends on both the mechanism and the semantics of the communication.
  "pipes, sockets and command-line arguments" are normally used between
  separate programs; but exchanging "complex internal data structures"
  could make them one program.
- `#GPLInProprietarySystem`: GPL software can ship alongside other software
  if they "communicate at arms length".
- GPLv2 section 2 (ccx's own `gpl-2.0.txt`): "mere aggregation of another
  work ... on a volume of a storage or distribution medium" does not bring
  it under the GPL. GPLv3 section 5 (OpenFOAM `COPYING`): inclusion in an
  "aggregate" does not apply the licence to the other parts.

Our design (write `.inp`, run `ccx job`, read `.frd`/`.dat`; write an
OpenFOAM case directory, run solvers, read results) is file plus
command-line communication through documented, public file formats. That is
the pattern the FAQ describes as normally separate programs. Whether an
AppImage (one SquashFS file holding separate executables) counts as a
"distribution medium" is not addressed in the FAQ; not checked further.

Precedents found:

- The FreeCAD (LGPL) Linux AppImage build script installs `calculix` and
  `gmsh` from conda-forge and copies `ccx` and `gmsh` binaries into the
  bundle: https://github.com/FreeCAD/FreeCAD-Bundle/blob/master/conda/linux/create_bundle.sh
  (lines 23, 67, 71). How FreeCAD meets the source duty for those binaries:
  not checked.
- PrePoMax (GPL-3.0) ships "a sub-directory with the Calculix solver" taken
  from the upstream Windows package: https://prepomax.fs.um.si/downloads/

### Source-offer duties if we ship ccx or OpenFOAM binaries

- GPLv2 (ccx, cgx, Gmsh): section 3 needs the binary to be accompanied by
  source, or by a written offer valid three years "to any third party";
  and section 3's last paragraph: if binaries are offered "from a designated
  place", offering the source "from the same place" counts. FAQ
  `#WhatDoesWrittenOfferValid` explains the offer must work for anyone.
- GPLv3 (OpenFOAM): section 6(d) allows the source on "a different server
  (operated by you or a third party)" with clear directions next to the
  binary; FAQ `#SourceAndBinaryOnDifferentSites` and
  `#AnonFTPAndSendSources` add that the source must match the exact version
  shipped and stay available as long as the binary is offered.
- FAQ `#DistributeExtendedBinary`: patched binaries need the patched source.
- Practical reading: put the exact upstream source tarballs (plus any
  patches and build script) on the same release page as each AppImage. That
  satisfies the "same place" wording in both licences without relying on
  upstream servers staying up.
- Not checked: whether a ccx build linked against Intel MKL/Pardiso can be
  redistributed under GPLv2. Build ccx with SPOOLES/ARPACK (or PaStiX, the
  upstream fork https://github.com/Dhondtguido/PaStiX4CalculiX) to avoid the
  question.

---

## 2. Trademarks and naming rules

### OpenFOAM (OpenCFD Limited)

Policy: https://www.openfoam.com/opencfd-limited-trade-mark-policy
(authored 2009, still served in 2026; page footer says "OpenCFD is an
affiliate of KEYSIGHT"). Guidelines:
https://www.openfoam.com/openfoam-trade-mark-guidelines

Rules from the policy:

- Use only "to reference OpenCFD Limited's software, products and services";
  no implied "sponsorship, affiliation or endorsement".
- Required **Disclaimer** on each page describing a non-OpenCFD product,
  prominent, font at least half the headline size and no smaller than body
  text. Exact text (quoted in full because it must be reproduced verbatim;
  it is the only quote here over 15 words):

  > This offering is not approved or endorsed by OpenCFD Limited, producer
  > and distributor of the OpenFOAM software via www.openfoam.com, and owner
  > of the OPENFOAM® and OpenCFD® trade marks.

- Use ® on each initial reference, plus the **Acknowledgement**, usually in
  footnotes or legal information:

  > OPENFOAM® is a registered trade mark of OpenCFD Limited, producer and
  > distributor of the OpenFOAM software via www.openfoam.com.

- Not in product or service names without "a specific written agreement or
  licence"; never in corporate or trading names; never in domain or
  subdomain names.
- No shortening, acronyms or different capitalisation.
- Developers may call themselves "a developer using OpenFOAM technology",
  not "OpenFOAM developers".

From the guidelines:

- Names like "hydrostaticFoam" or "OpenFOAM Hydrostatics" are "strongly"
  discouraged; an own name in a descriptive title is the recommended form,
  e.g. "Eureka - Hydrostatics Solver in OpenFOAM". Derivative names like
  "cloudFoam" "should be avoided".
- The limits apply "under any other capitalisation" too.

Foundation phrasing (https://openfoam.org/website-terms-of-use): "OPENFOAM®
is a registered trademark of OpenCFD Ltd licensed in perpetuity to the
OpenFOAM Foundation." Background on that licence:
https://openfoam.org/ip-history/ (2011 IP Agreement grants the Foundation an
"irrevocable, non-exclusive, royalty-free license" to the mark).

Practice by others: CfdOF (https://github.com/jaheyns/CfdOF README) and
HELYX-OS (https://github.com/ENGYS/HELYX-OS README) both print the exact
Disclaimer and write "OpenFOAM®".

For us: no "Foam" in our name, file extensions, binaries or domains;
README, website, AppStream metainfo and a legal page (not the About dialog) carry the
Disclaimer and Acknowledgement; first mention per page is "OpenFOAM®".

### CalculiX

No trademark policy found on https://www.dhondt.de/ or http://www.calculix.de/.
The only naming notice is that CalculiX's input conventions are based on
ABAQUS "used with kind permission from HKS", and that results are "in no
way connected to ABAQUS". Trademark registration of "CalculiX": not checked.

### ParaView, VTK (Kitware)

No published trademark usage guideline found on paraview.org or
kitware.com/legal. Third-party aggregators report PARAVIEW (US reg.
4011747, live) and VTK (US reg. 4834510) owned by Kitware Inc.:
https://markinton.com/trademark/paraview-85215601,
https://trademarks.justia.com/862/74/vtk-86274389.html. USPTO TSDR itself:
not checked. Independently, the BSD-3 clause 3 in both VTK and ParaView
bars using the copyright holders' names to endorse or promote derived
products without written permission.

### FreeCAD

The FreeCAD Project Association registered a trademark on the FreeCAD
logo in Benelux; the logo stays LGPL and the FPA says "keep using the
FreeCAD logo everywhere": https://blog.freecad.org/2022/10/12/the-freecad-project-association-now-owns-a-trademark/
A separate word-mark policy for "FreeCAD": not found. wiki.freecad.org/Logo
was blocked by bot protection: not checked directly.

---

## 3. Good citizenship in practice

### How established third-party tools relate

- **CfdOF** (FreeCAD workbench, LGPL-3.0): supports "Foundation versions
  9-13" and "OpenCFD versions 2006-2512", installs OpenFOAM from upstream
  links or distro packages, prints the OpenCFD Disclaimer, and routes
  support to the FreeCAD forum subforum and bugs to GitHub:
  https://github.com/jaheyns/CfdOF
- **PrePoMax** (GPL-3.0): pre/post for CalculiX; vendors Netgen, Mmg and
  Gmsh binaries "together with the license texts"; moved from GitLab to
  https://github.com/PrePoMax/PrePoMax (old repo archived 2026-11); its own
  forum and Discord; ships upstream ccx rather than a fork:
  https://prepomax.fs.um.si/downloads/
- **HELYX-OS** (ENGYS, GPL, Java + VTK): the README now says the project
  "has been deprecated"; it targeted OpenFOAM 4.1 and v1606+ only, which
  shows the cost of pinning to old solver versions:
  https://github.com/ENGYS/HELYX-OS
- **ccx2paraview** (GPL-3.0, Python): converts `.frd` to `.vtu`/`.pvd`;
  last push 2026-09: https://github.com/calculix/ccx2paraview
- **FreeCAD FEM / FreeCAD AppImage**: bundles upstream ccx from conda-forge
  (see section 1).

### Where the communities talk

| Project | Discussion | Bugs and contributions |
|---|---|---|
| CalculiX | https://calculix.discourse.group (the old group was on Yahoo, not Google; no Google group found); a #calculix channel on Elmer's Discord | https://github.com/Dhondtguido/CalculiX/issues (Guido Dhondt's repo, active 2026-11-10, 203 open issues). Forum regulars note Guido is "rarely active" on Discourse, so file issues on GitHub |
| OpenFOAM (Foundation) | CFD Online forums https://www.cfd-online.com/Forums/ (403 to curl, not opened) | https://bugs.openfoam.org; contributions target OpenFOAM-dev, significant ones need the Contributor Agreement: https://openfoam.org/dev/how-to-contribute/ |
| OpenFOAM (openfoam.com) | CFD Online | https://gitlab.com/openfoam/core/openfoam/-/issues (listed in README; page itself returned 404 to curl, possibly login-gated) |
| FreeCAD | https://forum.freecad.org (CfdOF subforum f=37) | https://github.com/FreeCAD/FreeCAD/issues |
| ParaView | https://discourse.paraview.org | GitLab issues and merge requests at gitlab.kitware.com, per https://github.com/Kitware/ParaView/blob/master/CONTRIBUTING.md ("every code change should have an associated issue") |

### AI-assisted contributions (relevant to how we upstream)

FreeCAD's `AI_POLICY.md` (2026) expects "disclosure of AI assistance" and
says they "will not accept pull requests with clearly AI-generated code,
commit messages, PR descriptions": https://github.com/FreeCAD/FreeCAD/blob/main/AI_POLICY.md
Any FreeCAD exporter fix must be one David understands, reviews, discloses
and presents himself. AI policies of CalculiX, OpenFOAM, ParaView, VTK,
Gmsh: not checked.

### Contribution paths worth taking instead of workarounds

- **ParaView `.frd` reader.** ParaView has no native CalculiX reader:
  open request https://discourse.paraview.org/t/calculix-format-native-support-frd/7807,
  and Kitware's own blog uses `.frd` as the example of an unsupported format:
  https://www.kitware.com/easy-data-conversion-to-vtk-with-python/
  We will write an FRD reader anyway; offering it as a ParaView reader
  plugin (or upstream to VTK, BSD-3 compatible if we license that file
  BSD-3 or the copyright holders agree) helps everyone and keeps our
  output path "plain ParaView". Coordinate with ccx2paraview rather than
  starting a third converter.
- **FreeCAD export bugs**: report on the forum/GitHub and send fixes
  upstream (under the AI policy above) rather than post-processing broken
  exports silently.
- **CalculiX bugs**: GitHub issue with a minimal `.inp`.
- **OpenFOAM**: bugs.openfoam.org with a minimal case; report against the
  distribution that has the bug, do not cross-post.

### Formats and distributions

- Write and read the solvers' own formats (`.inp`, `.frd`, OpenFOAM case
  dirs) and standard VTK (`.vtu`, `.pvd`, `.vtkhdf`) for ParaView. Avoid a
  private results format that only our app reads.
- Support both OpenFOAM lines, as CfdOF does; detect which one is installed
  rather than assuming.
- Packaging: Ubuntu ships `calculix-ccx` 2.21, `calculix-cgx` 2.21,
  `gmsh` 4.14, `paraview` 6.0.1, `netgen` 6.2 (`apt-cache show` on beast,
  2026-11-14). Debian's `openfoam` package is the openfoam.com 1912 release
  from 2020 (https://sources.debian.org/api/src/openfoam/), so for OpenFOAM
  point users to the upstream repositories (CfdOF warns distro OpenFOAM
  "may be out of date or incomplete"). Use distro or unmodified upstream
  solver binaries; if we must patch, upstream the patch and publish it.
- Flathub already carries `org.paraview.ParaView` and `org.freecad.FreeCAD`
  (HTTP 200 from the Flathub API). No CalculiX or OpenFOAM Flatpak found
  in a Flathub search.

---

## 4. Name check (web and GitHub only; not a trademark search)

| Name | Findings |
|---|---|
| **FluidStress** | No software, company or GitHub repo or user found. GitHub repo search `fluidstress`: 0 results; user/org `fluidstress`: free (404). `fluidstress.com` does not resolve. Web: only generic uses ("fluid stress" tensor in Goma docs, Wolfram's `FluidViscousStress` function) |
| **TauFlow** | **Conflict.** Tau Flow (https://tauflow.com, live 2026-11-14) is a Brazilian CFD engineering company in Campinas that sells CFD projects and consulting and teaches OpenFOAM courses ("Ensinamos OpenFOAM"); founded 2015 at the Unicamp incubator: https://agencia.fapesp.br/virtual-simulation-and-3d-modeling-assure-high-performance-of-projects/28690 . Same field and same solver. Also an unrelated GitHub repo `FeiYee/TauFlow-Seg` (image segmentation) |
| **FlowTau** | No product found; GitHub user/org free; `flowtau.com` does not resolve. But it is a word-swap of the Tau Flow company name in the same field |
| DLR **TAU** | DLR's unstructured compressible CFD solver, licensed only under agreement, not to individuals or outside the EEA: https://www.dlr.de/en/as/research-and-transfer/software-solutions/aerodynamics/software-tau . "TAU" is a known name in aerospace CFD, which adds to the confusion risk for any Tau-based CFD name |
| Close CFD/CAE names | FluidFlow (Flite Software, pipe flow), FLUID (Fraunhofer ITWM), Stress Engineering Services (stress.com). None identical to the candidates |

Reading: FluidStress is the only one of the three with no conflict found
in this check. Formal search (EUIPO, WIPO, USPTO, PRV) still to do.

---

## Rules of citizenship for the project

1. **Talk to solvers through files and command lines only; never link
   GPL code (ccx, Gmsh library, OpenFOAM libs) into our binary.**
   Source: GPL FAQ `#MereAggregation`, `#GPLInProprietarySystem`.
2. **Use only LGPL/GPL-2.0 Qt modules (Core, Gui, Widgets, Concurrent,
   OpenGL); no Qt Charts, Graphs, Data Visualization, Quick 3D.**
   Source: https://doc.qt.io/qt-6/licensing.html and module pages.
3. **If an AppImage ships ccx, gmsh or OpenFOAM binaries, ship them
   unmodified (or with published patches), include their licence texts,
   and host the exact matching source on the same release page.**
   Source: GPLv2 section 3, GPLv3 section 6(d), FAQ
   `#SourceAndBinaryOnDifferentSites`, `#DistributeExtendedBinary`.
   Prefer the user's distro or upstream install when present.
4. **Treat ccx and cgx as GPL-2.0 only** (source headers), even though the
   cgx web page says "or later". Source: ccx/cgx 2.23 tarballs.
5. **No "Foam" or "OpenFOAM" in our name, binaries, file extensions or
   domain; write "OpenFOAM®" at first mention; print the exact OpenCFD
   Disclaimer and Acknowledgement on every page that describes the product.**
   Source: https://www.openfoam.com/opencfd-limited-trade-mark-policy and
   the guidelines page.
6. **Describe, do not borrow:** "FluidStress, a GUI for CalculiX and
   OpenFOAM®", never wording that implies endorsement by OpenCFD, the
   Foundation, Kitware, the FPA or Guido Dhondt. Source: OpenCFD guidelines
   ("Eureka - Hydrostatics Solver in OpenFOAM" pattern); VTK/ParaView BSD-3
   clause 3.
7. **Support both OpenFOAM lines (openfoam.org and openfoam.com) and say
   which versions are tested.** Source: CfdOF README; HELYX-OS deprecation
   after pinning to 4.1/v1606+.
8. **Credit third parties in README, NOTICE and LICENSES/, including the
   OCCT notice if we link OCCT; not in the About dialog.** Source: OCCT
   LGPL exception; `katalystnord/policy-on-attribution.md`.
9. **Keep formats open and shared:** solver-native formats in, standard VTK
   out; contribute the FRD reader to ParaView/VTK or alongside
   ccx2paraview rather than inventing a private format. Source: ParaView
   Discourse request 7807; Kitware blog.
10. **Fix upstream, report upstream, in the right place:** CalculiX GitHub
    issues, bugs.openfoam.org, OpenFOAM.com GitLab, FreeCAD GitHub/forum,
    ParaView GitLab; minimal reproducible cases. Source: each project's
    README/CONTRIBUTING (section 3 table).
11. **Follow each upstream's AI policy when contributing**; for FreeCAD,
    disclose AI assistance and present only human-understood, human-written
    PRs. Source: FreeCAD `AI_POLICY.md`.
12. **Give back where it counts:** the OpenFOAM Foundation says development
    stalls "without funding"; consider a maintenance contribution if the
    product earns money from OpenFOAM. Source: https://openfoam.org/dev/how-to-contribute/
13. **Pick a name with no CFD conflict:** avoid TauFlow (Tau Flow, a
    Brazilian OpenFOAM CFD firm) and, to a lesser degree, FlowTau.
    Source: https://tauflow.com, FAPESP article.

## Not checked

- USPTO/EUIPO records for PARAVIEW, VTK, CalculiX, FreeCAD word marks.
- How FreeCAD's AppImage meets the GPL source duty for its bundled ccx.
- MKL/Pardiso redistribution in a GPLv2 ccx build.
- AI contribution policies of CalculiX, OpenFOAM, ParaView, VTK, Gmsh.
- CFD Online forum pages (blocked to curl) and wiki.freecad.org/Logo
  (bot protection).
