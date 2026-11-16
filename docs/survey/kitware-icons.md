# Icons across Kitware's software, and what GridForge takes (survey, 2026-11-16)

Purpose: make GridForge (and TenSense, SurView) sit visually beside
ParaView and VTK. Everything below was read from the repositories on
2026-11-16 (shallow, blob-less clones; nothing built or run) unless
marked "not checked". Colours and sizes were measured from the files
with a script over the SVG sources and ImageMagick `identify` over the
PNGs. This is a summary of what the sources say, not legal advice.

---

## 1. Repositories read

| Project | Repository | Commit read | Licence file read |
|---|---|---|---|
| ParaView | https://gitlab.kitware.com/paraview/paraview | `afcca0d` (2026-11-16) | `Copyright.txt` |
| ParaView icon rework | https://gitlab.kitware.com/paraview/icons-rework (wiki only; the git repo needs a login) | wiki pages via the GitLab API | none in the wiki |
| VTK | https://gitlab.kitware.com/vtk/vtk | `6b16e36` (2026-11-16) | `Copyright.txt` |
| 3D Slicer | https://github.com/Slicer/Slicer | `250dc5f` (2026-11-16) | `License.txt`, `COPYRIGHT.txt` |
| SMTK | https://gitlab.kitware.com/cmb/smtk | `6ada5c6` (2026-11-09) | `LICENSE.txt` |
| CMB / ModelBuilder | https://gitlab.kitware.com/cmb/cmb | `c2cc2c3` (2026-11-05) | `LICENSE.txt` |
| aeva | https://gitlab.kitware.com/aeva/aeva | `c978fab` (2026-11-07) | `LICENSE.txt` |
| tomviz | https://github.com/OpenChemistry/tomviz | `6db2aee` (2026-11-13), full history fetched | `LICENSE` |
| VolView | https://github.com/Kitware/VolView | `a15b639` (2026-11-14) | `LICENSE` |
| trame | https://github.com/Kitware/trame | `fc4162f` (2026-11-13) | `LICENSE`, `pyproject.toml` |
| CMake | https://gitlab.kitware.com/cmake/cmake | `20268ff` (2026-11-16) | `LICENSE.rst` |
| ParaView Flatpak | `org.paraview.ParaView` installed here, version 6.1.0 | inspected with `flatpak run --command=sh` | `/app/share/licenses/ParaView/` |

## 2. Licences, as stated in the files

| Project | Licence as found | Icon-specific terms found? |
|---|---|---|
| ParaView | BSD-3-Clause text, "Copyright (c) 2000 Kitware Inc." (`Copyright.txt`, 26 lines, no third-party section) | `Qt/Components/Resources/Icons/README.md` has a heading "License information for icons" with **no entries** under it. History: from 2015-07-29 (`d555ddb1`) it listed one exception, `pqPalette.svg` "Based on https://commons.wikimedia.org/wiki/File:Palette.svg", CC BY-SA 3.0; that entry was removed on 2017-05-17 (`fe2b1ec4`, "Replace ParaView palette icon with tomviz one"). So today no ParaView icon is declared under other terms. |
| ParaView `Clients/ParaView/Documentation/license.txt` | Creative Commons Attribution 3.0 Unported | Sits beside the bundled help pages (`about.html.in`, `images/img0*.jpg`), not beside the icons; which files it covers is not stated in the file. Not an icon licence as far as checked. |
| ParaView Flatpak 6.1.0 | Ships `/app/share/licenses/ParaView/Copyright.txt` plus per-module VTK and third-party licences | No separate licence file for the Qt icon resources. |
| VTK | BSD-3-Clause text, "Copyright (c) 1993-2015 Ken Martin, Will Schroeder, Bill Lorensen" (`Copyright.txt`) | No icon set to speak of (section 4). |
| 3D Slicer | "3D Slicer Contribution and Software License Agreement", a BSD-style licence with extra terms (`License.txt`) | None for icons; no licence or README in any `Resources/Icons` folder. |
| SMTK | "SMTK License Version 1.0", BSD-3-Clause text, "Copyright (c) 2012 Kitware Inc." plus a third-party list (pugixml, PyYaml, rtvl, FindDocutils.cmake) | No icon entries in the third-party list. |
| CMB | BSD-3-Clause text, "Copyright (c) 2012 Kitware Inc." | None. |
| aeva | "aeva License Version 1.0", BSD-3-Clause text, "Copyright (c) 2019 Kitware Inc." | None; it ships `cleveland-clinic-logo.svg` and `kitware-logo.svg` (logos, not icons; their terms not checked). |
| tomviz | BSD-3-Clause, "Copyright (c) 2014-2021, Kitware, Inc." | None. |
| VolView | Apache-2.0 | Uses Material Design Icons through `@mdi/font` ^7.4.47 (`package.json`). |
| trame | Apache-2.0, "Copyright 2022 Kitware Inc." | Its Vuetify widgets use `mdi-*` icon names (example `pv_selection.py`); `trame-vuetify` 3.3.0 is MIT (PyPI metadata). |
| CMake | BSD-3-Clause text, "Copyright 2000-2026 Kitware, Inc. and Contributors" (`LICENSE.rst`) | None. |

### Third-party icon sets

| Set | Who uses it (found) | Licence (read) |
|---|---|---|
| Material Design Icons (Pictogrammers, `@mdi/font`) | VolView (npm dependency), trame apps via Vuetify | `@mdi/font` npm metadata: Apache-2.0. Repository `LICENSE` ("Pictogrammers Free License"): icons Apache 2.0, fonts Apache 2.0, code MIT; "Some of the icons are redistributed under the Apache 2.0 license. All other icons are either redistributed under their respective licenses or are distributed under the Apache 2.0 license." (https://github.com/Templarian/MaterialDesign-Webfont/blob/master/LICENSE) |
| Google Material Symbols (apparent) | Slicer: `Base/QTGUI/Resources/Icons/Medium/SlicerDockToRight.svg` and `SlicerVisualDICOMBrowser.svg` (added 2025-11-21, `2d238227`) | Their `viewBox="0 -960 960 960"`, 48 px, `fill="#000000"` header matches Google's Material Symbols export; the commit and repository do not state a source or licence. Material Symbols' own licence: not checked. |
| Wikimedia `Palette.svg` (CC BY-SA 3.0) | ParaView, 2015 to 2017 only | Read from the 2015 README entry; removed in 2017. |
| Tango, Oxygen, Breeze, Font Awesome, Feather, Lucide, Nuvola, Crystal | none | File-name search across all ten trees found no such set. |

## 3. ParaView in detail

### Where the icons live

| Path (ParaView repository) | Content | Qt resource prefix |
|---|---|---|
| `Qt/Components/Resources/Icons/` | 198 SVG, 92 PNG, `README.md`; the main set | `/pqWidgets` (`Qt/Components/Resources/pqComponents.qrc`), so code says `:/pqWidgets/Icons/pqResetCamera.svg` |
| `Qt/Widgets/Resources/Icons/` | 7 SVG and PNG (`pqPlus`, `pqMinus`, `pqDelete`, `pqExpand`, `pqCollapse`, `pqShowMenu`, `pqMultiply`) | `QtWidgets.qrc` |
| `Qt/Core/Resources/Icons/` | `pqDomain16.png`, `pqNetwork16.png` | |
| `Qt/ApplicationComponents/Resources/Thumbnails/` | 9 images (not icons) | |
| `Plugins/*/Icons/` | per plugin (SLACTools 22, FlipBook 8, ...) | |
| `Clients/ParaView/` | application icon: `pvIcon-{16,22,32,64,96,512}x*.png`, `pvIcon.ico` (256 px largest), `pvIcon.icns`, `pvIcon.iconset/` (macOS, 16 to 512@2x) | |
| `Qt/Components/Resources/Icons/pvIcon.svg` | the application icon as SVG (Inkscape, `viewBox 0 0 512 512`) | |
| Flatpak 6.1.0 | `/app/share/icons/hicolor/{22x22,32x32,96x96,scalable}/apps/org.paraview.ParaView.{png,svg}` | |

### The "newer icon set": the 2018 to 2020 rework

| Fact | Source |
|---|---|
| A separate project `paraview/icons-rework` was created 2018-03-12 to track the redesign | GitLab API project record |
| Its wiki lists batches 1 to 7 (file menu, edit/view menus, properties panel, custom viewpoints, render-view toolbar, misc, VCR and camera toolbars), each a table "old icon / new icon" | wiki `home`, `batch-1` to `batch-7` |
| Batch 1 "Complete, Jan 2020, all icons imported into ParaView" (merge request 3792) | wiki `batch-1` |
| Before the rework "most are 24 or 32 pixel pngs, some have x2 versions at 64x64" | wiki `batch-5` |
| Release note: "Icons in the render view toolbar were updated to SVG versions, so they can scale to any resolution. Other uses of .png icons are being replaced if possible." | `Documentation/release/dev/renderview_toolbar_icons.md` in commit `7d781fe7` (2020-01-13) |
| Developer guide: "To add icons, Kitware's graphic designer may be able to help create an SVG icon." | `Documentation/dev/git/develop.md`, same commit |
| A published style guide (grid, palette, stroke rules) | **none found** in the repository or the wiki; the style below is measured, not quoted |

### Measured style of the SVG set (198 files in `Qt/Components/Resources/Icons`)

| Property | Measured | Note |
|---|---|---|
| Canvas | `viewBox="0 0 64 64"` in 168 files, `0 0 60.52 60.52` in 8, others odd | no width/height attributes; Qt scales them |
| Authoring tool | Adobe Illustrator export (`id="Layer_1"`, `<title>SVG_Artboards</title>`, inline `style="fill:#..."`) on the reworked set; 11 Inkscape files | read in `pqResetCamera.svg`, `pqClip.svg`, `pqSave.svg`, `pqXPlus.svg` |
| Fill style | flat; 11 of 198 contain a gradient | |
| Outline | drawn as a **filled path** about 2.25 units wide (path offsets of 1.13 each side), not as an SVG stroke; real `stroke-width` where used is mostly 2.2536 (54 occurrences) | 2.25/64 is 3.5 %: 0.56 px at 16, 0.84 px at 24, 1.1 px at 32 |
| Corners | small rounded joins (arc radius about 1.1 on outline ends) | |
| Composition | a dark outline around flat light fills, with one or two saturated accents for the "active" part (arrow, axis, cut plane) | rendered contact sheet checked by eye |
| Raster leftovers | 92 PNG: 42 at 16x16, 25 at 24x24, 19 at 32x32; size in the name (`pqSave16.png`), `@2x` for HiDPI | `identify` |

| Colour | Hex | SVG files using it (of 198) | Typical role |
|---|---|---|---|
| Olive dark grey | `#6f6f59` | 132 | every outline |
| Light green | `#a7d28c` | 93 | main fill (arrows, objects, data) |
| Mid grey | `#787878` | 86 | glyphs and text (axis letters, "+90") |
| Light grey | `#cdcece` | 38 | secondary fill, inactive parts |
| Blue | `#2896d3` | 25 | accent (planes, selection, info) |
| Red | `#de4130` | 23 | accent (X axis, stop, delete) |
| Pale blue | `#d0e5f6` | 20 | windows, panels |
| Yellow | `#f3c017` | 19 | accent (Y axis, highlights) |
| Yellow (alt) | `#ffc418` | 19 | accent |
| Dark green | `#719943` | 7 | shadow side of a green fill |
| Folder yellows | `#f0bc5e`, `#f3d86a` | 6 and fewer | folders, files (`pqSave.svg`) |

### Application icon

| Property | Measured |
|---|---|
| Motif | three slanted parallel bars, red, green, blue, black outline (rendered from `pvIcon.svg`) |
| Fills | `#be2128` red, `#249847` / `#269847` green, `#064f8c` / `#054f8c` blue, `#010101` outline |
| Outline | `stroke-width:7` on a 512 canvas (1.4 %) |
| Shipped sizes | 16, 22, 32, 64, 96, 512 PNG; ICO up to 256; ICNS; scalable SVG in the Flatpak |

## 4. The other projects

| Project | Where the icons live | Format and sizes (measured) | Visual style | Style guide |
|---|---|---|---|---|
| VTK | No application icon set. `Examples/Infovis/Cxx/EasyView/Icons/` (4 PNG, 22x22: fileopen, filesave, help, print); iOS example app icons; documentation figures | PNG | generic example art | none found |
| 3D Slicer | `Base/QTGUI/Resources/Icons/{XSmall,Small,Medium,Large,XLarge,Scalable}/`, `Libs/MRML/Widgets/Resources/Icons/` (114 plus 52 LUT swatches), per-module `Modules/*/Resources/Icons/`; app icon `Applications/SlicerApp/Resources/Icons/{Small..XXLarge}/Slicer-DesktopIcon.png` | Mostly PNG in size folders: XSmall 16, Small 21 to 24, Medium 32, Large 64, XLarge 128; few SVG (12 in Base/QTGUI, 48 px or 16 px) | mixed generations: older glossy PNGs, a few flat Material-style SVGs; `#666666` and `#f7bf21` most common in its SVGs | none found for icons; extension docs recommend a 128x128 PNG extension icon (`Docs/developer_guide/extensions.md`) |
| SMTK | `smtk/extension/qt/icons/` (50), `smtk/view/icons/` (44 SVG: model, mesh, vertex, edge, face, volume, align, distribute, zoom), `smtk/extension/paraview/appcomponents/resources/` (26) | SVG on 13 mm, 32 px or 64 px canvases; PNG at 32 and 64 (`@2x`) | thin dark line art (`stroke-width` 0.5 most common), `#000000`, `#d1d1d1`, `#666666`; some coloured entity icons | `appcomponents/resources/readme.md`: regenerate PNG with `inkscape -w 64 -h 64`, keep the page "exactly square ... with enough border padding" |
| CMB / ModelBuilder | `cmb/resource/` (8 PNG, 1 SVG), `modelbuilder/resource/` (app icon 22, 32, 96 PNG, ICO, ICNS, splash) | PNG 16/24/32, one SVG | copies of ParaView's (section 5) | none found |
| aeva | `aeva/resource/` (app icon 22, 32, 96, 137x158 PNG, SVG logo, splash), `aeva/aeva.ico`, `aeva/aeva.icns` | PNG and SVG | logo art only; relies on ParaView and SMTK icons for tools (not counted) | none found |
| tomviz | `tomviz/icons/` (21 SVG, 34 PNG, app icon `tomviz.png` 32 px, `tomvizfull.png`, ICO, ICNS) | SVG: 10 on 64x64, 10 on 24x24; PNG 32 plus `@2x` 64 | same family as ParaView: `#6f6f59` outline in 10 SVGs, `#a7d28c` fill in 5, `stroke-width 2.25` in 13 | none found |
| VolView | No raster icon set; `src/assets/logo.svg`, `docs/assets/icon/favicon-{16,32,96,196}.png`, `public/favicon.ico`; UI icons come from `@mdi/font` | web font plus SVG logo | Material Design Icons, monochrome | Material Design's own (not checked) |
| trame | `docs/vitepress/assets/logos/` (trame-icon.svg, vtk.svg, kitware.svg ...), favicons 16 to 196; apps use `mdi-*` names | SVG, PNG | Vuetify plus MDI; the ParaView picking example ships ParaView SVGs (section 5) | none found |
| CMake | `Source/QtDialog/`: `CMakeSetup{32,64,128}.png`, `CMakeSetup.ico` (16, 24, 32, 48, 64, 96, 128, 256), `CMakeSetup.icns`, `Plus16.png`, `Delete16.png` | PNG, ICO, ICNS | the triangle logo; two 16 px button glyphs | none found |

## 5. Who reuses ParaView's icons, and how (checked by content hash)

Method: every PNG and SVG blob in tomviz's whole history, and in the
current SMTK, CMB, aeva, Slicer and trame trees, was matched by git blob
hash against ParaView's `Qt/Components/Resources/Icons` at tags v5.4.0,
v5.5.0, v5.7.0, v5.8.0, v5.10.0 and master (1957 blob entries, via the
GitLab API).

### 5.1 tomviz: the worked example of an app outside ParaView's tree

| Finding | Evidence |
|---|---|
| tomviz links ParaView's Qt libraries and **uses ParaView's icons straight from ParaView's compiled-in Qt resources**, by path, with no copy in its own tree | 38 distinct `:/pqWidgets/Icons/...` paths referenced in `tomviz/*.cxx` and `*.ui` |
| Camera toolbar (what the screenshot shows): `pqResetCamera.svg`, `pqZoomToSelection.svg` ("Zoom to Box"), `pqXPlus/XMinus/YPlus/YMinus/ZPlus/ZMinus.svg`, `pqRotateCameraCW/CCW.svg` | `tomviz/CameraReaction.cxx` lines 145 to 190, added to `utilitiesToolbar` in `MainWindow.cxx:1033` |
| VCR playback buttons: ParaView's own `pqVCRToolbar` widget, which draws `pqVcrFirst/Back/Play/Forward/Last/Loop/Reverse.svg` | `MainWindow.ui` custom widget `pqVCRToolbar`; icon list from ParaView `pqVCRToolbar.ui` |
| Palette button: `QIcon(":pqWidgets/Icons/pqPalette.svg")` | `MainWindow.cxx:1024` |
| Render-view frame (split, maximise, close) and camera undo/redo/link: ParaView's `pqTabbedMultiViewWidget`, `pqViewFrame`, `pqStandardViewFrameActionsImplementation`, `pqCameraLinkReaction` | tomviz `CentralWidget.ui`, `ViewFrameActions.cxx`, `AddRenderViewContextMenuBehavior.cxx`. In ParaView, split uses `pqSplitHorizontal/Vertical.svg`; maximise and close use **Qt's standard style icons** (`SP_TitleBarMaxButton`, `SP_TitleBarCloseButton`), not ParaView art; camera undo/redo use `pqUndoCamera.svg`, `pqRedoCamera.svg` (`pqViewFrame.cxx`, `pqStandardViewFrameActionsImplementation.cxx`) |
| Other ParaView icons used: pqFavorites, pqThreshold, pqRuler, pqProbeLocation, pqLineChart16.png, pqGroup, pqExtractGrid, pqClip, pqSpreadsheet, pqScalarBar, pqSave, pqResetRange, pqProgrammableFilter, pqLock, pqIsosurface, pqInvert, pqHome, pqEyeball, pqEyeballClosed, pqCalculator, pqAdvanced, pqVcrPause | same grep |
| tomviz's own files with `pq` names (`pqInspect`, `pqLock.png`, `pqMeasurementCube`, `pqResetRangeCustom.png`, `pqVolumeData`, `pqVcrStop.svg`) are **tomviz's own drawings**, none hash-equal to any ParaView file | first commits 2017-03 by Marcus D. Hanwell ("Updated a number of other icons", "All about scale") and 2026-04-14 (pqVcrStop) |
| Until 2021 tomviz carried its own copies; commit `507349d6` (2021-02-02) deleted 22 of them: "Remove unused/duplicated icons. Use the upstream paraview icons instead" | tomviz history |
| **The flow also ran the other way.** tomviz's `pqPalette.png` and `pqPalette@2x.png` (added 2017-03-06, `41a1158a`) are byte-identical to ParaView's at v5.4.0 to v5.7.0; ParaView commit `fe2b1ec4` (2017-05-17) is titled "Replace ParaView palette icon with tomviz one". tomviz's `pqEyeball*.png` (2017-02-24, "Some updated icons from Steve Jordan ... More coming, but they need to be integrated in ParaView") equal ParaView's from v5.5.0; `pqRuler*.png` (2017-03-06) equal ParaView's at v5.7.0 and v5.8.0 | blob hashes; dates order tomviz first. Today's ParaView `pqPalette.svg` was redrawn on 2020-01-13 (`3762f9ac`) |
| How tomviz attributes ParaView's icons: **it does not, separately.** No NOTICE, no icon credit in `README.md` or `LICENSE`; the About dialog lists only "ParaView Version" (`AboutDialog.cxx:117`). Both projects are Kitware's under BSD-3, and the icons arrive inside ParaView's libraries, whose licence travels with them | grep of `LICENSE`, `README.md`, `CMakeLists.txt`, `cmake/`, `AboutDialog.cxx`. How tomviz's binary packages ship ParaView's `Copyright.txt`: not checked |

### 5.2 Other reusers

| Project | ParaView files copied byte for byte (renamed where shown) | Attribution found |
|---|---|---|
| SMTK | `pqEyeball.svg`, `pqEyeballClosed.svg` (identical to ParaView master); `eyeball{,@2x,Closed,Closed@2x}.png` (= ParaView v5.5.0); `Save16/24/32.png` (= pqSave16/24/32 v5.4.0); `Close22.png` (= pqQuit22); `cancel32.png` (= pqCancel32); `growaccept24.png` (= pqExtractSelection24); `edgesplit.png`, `session/polygon/qt/edgepoint.png` (= pqSurfaceSelectionPoint24) | none beyond its own Kitware BSD licence |
| CMB / ModelBuilder | `cmbSaveAnimation.svg` (= pqSaveAnimation.svg master); `cmbOpen16/24/32.png`, `cmbSave16/24/32.png`, `cmbCaptureScreenshot24.png`, `cmbClose22.png` (= pqOpen*, pqSave*, pqCaptureScreenshot24, pqQuit22 at v5.4.0) | none |
| 3D Slicer (Sequences module) | `pqVcrBack24, First24, Forward24, Last24, Loop24, Pause24, Play24.png` (= ParaView v5.4.0), name kept | none found in the module, `COPYRIGHT.txt` or `License.txt` |
| trame (example `07_paraview/Picking`) | 8 selection SVGs identical to ParaView master (`pqSurfaceSelectionPoint/Cell`, `pqFrustumSelection*`, `pqPolygonSelectSurface*`, `pqSelectBlock`, `pqSurfaceSelectionCellInteractive`); `pqSurfaceHovering{Cell,Point}.svg` not in ParaView | none |

So the house practice inside Kitware is to reuse ParaView's icons freely,
keeping the `pq` name or renaming, without per-icon credit. That works for
them because the copyright holder is the same. GridForge is not Kitware,
so it must carry ParaView's notice (section 6).

## 6. What GridForge takes

### 6.1 Icons to reuse directly

ParaView's icons are covered by ParaView's BSD-3-Clause `Copyright.txt`
and, per the icons README, by nothing else today. BSD-3-Clause is
compatible with GridForge's LGPL-2.1-or-later. Reuse the **current SVGs
from `Qt/Components/Resources/Icons` at a pinned commit**, copied into
GridForge's tree (GridForge links VTK, not ParaView's Qt libraries, so the
`:/pqWidgets` resources tomviz relies on are not available to us).

| Group | Files (ParaView names) | GridForge use |
|---|---|---|
| Camera | `pqResetCamera`, `pqResetCameraClosest`, `pqZoomToData`, `pqZoomToSelection`, `pqXPlus`, `pqXMinus`, `pqYPlus`, `pqYMinus`, `pqZPlus`, `pqZMinus`, `pqRotateCameraCW`, `pqRotateCameraCCW`, `pqIsometricView`, `pqUndoCamera`, `pqRedoCamera`, `pqShowOrientationAxes`, `pqShowCenterAxes`, `pqResetCenter`, `pqPickCenter` | the quick-look view, so it behaves and looks like ParaView's |
| Playback | `pqVcrFirst`, `pqVcrBack`, `pqVcrPlay`, `pqVcrPause`, `pqVcrForward`, `pqVcrLast`, `pqVcrLoop`, `pqVcrReverse` | stepping through increments and steps of results |
| File and session | `pqOpen`, `pqSave`, `pqSaveAs`, `pqRecentFile`, `pqReloadFile`, `pqExportScene`, `pqCaptureScreenshot`, `pqUndo`, `pqRedo`, `pqSettings`, `pqHelp`, `pqExitApp`, `pqNewFolder` | main toolbar and menus |
| Data and colour | `pqPointData`, `pqCellData`, `pqNodeSetData`, `pqElemSetData`, `pqSideSetData`, `pqFaceSetData`, `pqEdgeSetData`, `pqEyeball`, `pqEyeballClosed`, `pqScalarBar`, `pqResetRange`, `pqResetRangeCustom`, `pqEditColor`, `pqPalette`, `pqSpreadsheet` | node sets, element sets, surfaces, result arrays, legends |
| View layout | `pqSplitHorizontal`, `pqSplitVertical`, `pqActiveViewFullscreen` | split views |
| Tools | `pqRuler`, `pqProbeLocation`, `pqPlotOverLine`, `pqClip`, `pqSlice`, `pqWarp`, `pqCalculator` | measured against predicted, quick checks |

Do **not** take: `pvIcon*` and `ParaViewLogo.png` (ParaView's identity;
whether "ParaView" and the logo are registered trade marks was not
checked, and using another program's logo would mislead either way);
Slicer's two Material-style SVGs (source and licence not stated); any
`Plugins/*` icon without reading that plugin's licence first.

Record the source commit and file list in `NOTICE` (or a
`resources/icons/paraview/README.md` next to the copies), and keep the
`pq` file names so a later diff against ParaView is trivial.

### 6.2 Exact attribution, for README and NOTICE (never the About dialog)

BSD-3-Clause requires that "Redistributions in binary form must reproduce
the above copyright notice, this list of conditions and the following
disclaimer in the documentation and/or other materials provided with the
distribution." The notice to reproduce, verbatim from ParaView's
`Copyright.txt`:

```
Icons from ParaView (https://gitlab.kitware.com/paraview/paraview,
Qt/Components/Resources/Icons, commit <pinned hash>):

Copyright (c) 2000 Kitware Inc.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list
  of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

* Neither the name of Kitware nor the names of any contributors may be used to
  endorse or promote products derived from this software without specific prior
  written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

README line: "Toolbar icons for camera, playback, files and data are
ParaView's, by Kitware, under the BSD 3-Clause licence; see NOTICE." The
"Neither the name of Kitware" clause means no "endorsed by Kitware" or
"ParaView-approved" wording anywhere. Follow
`katalystnord/policy-on-attribution.md` for placement (not re-read for
this note).

If an MDI icon is ever needed: Apache-2.0, so ship the Apache licence text
and Pictogrammers' notice in NOTICE as well.

### 6.3 House style for GridForge's own icons (to sit beside ParaView's)

| Rule | Value | Why (measured above) |
|---|---|---|
| Format | SVG only, plain (no Inkscape or Illustrator metadata, no embedded raster, no text elements: convert glyphs to paths) | 168 of ParaView's 198 SVGs are on the 64-unit grid, with flat fills set by inline styles |
| Canvas | `viewBox="0 0 64 64"`, no width/height | ParaView's grid |
| Rendered sizes to check | 16, 22, 24, 32 px (and 2x of each) | ParaView PNG legacy sizes 16/24/32; app icons also 22 |
| Safe area | keep artwork inside 4 to 60 (4-unit margin) | ParaView art spans about 3 to 61; SMTK's readme asks for padding so Qt buttons do not clip |
| Outline | 2.25 units, drawn as a filled outline path or a stroke with round joins, colour `#6f6f59` | ParaView's outline width and colour in 132 files |
| Fill | flat, no gradients, no drop shadows | 187 of 198 ParaView SVGs have no gradient |
| Main fill | `#a7d28c` light green; shadow side `#719943` | ParaView |
| Neutral fills | `#cdcece` light grey, `#d0e5f6` pale blue (windows, panels) | ParaView |
| Glyphs and letters | `#787878` | ParaView axis letters |
| Accents, one or two per icon | red `#de4130`, yellow `#f3c017`, blue `#2896d3` | ParaView; X red, Y yellow, as in `pqXPlus.svg` |
| GridForge-specific meaning | keep the same accents with fixed meanings: red = load or constraint, blue = mesh or geometry, yellow = step or time, green = result or OK | our convention (DRAFT), built on ParaView's colours |
| Predictions (surrogates) | same icon plus a dashed outline (`stroke-dasharray` 4 3) | tenet: predictions labelled everywhere (DRAFT proposal) |
| Naming | `gf` + CamelCase verb or noun, e.g. `gfMeshGenerate.svg`, `gfStepAdd.svg`, `gfRunSolver.svg`, `gfBoundaryFixed.svg`; reused ParaView files keep their `pq` names | mirrors ParaView's `pq` prefix and the project's `gf` prefix |
| Qt resource prefix | `:/gf/icons/` for ours, `:/gf/icons/paraview/` for the copies | keeps provenance visible in code |
| Application icon | its own motif, not three bars; flat fills, dark outline about 1.4 % of the canvas (7 on 512), shipped as SVG plus 16, 22, 32, 64, 96, 512 PNG, ICO, ICNS | mirrors ParaView's `pvIcon` set and outline weight |
| Dark themes | not handled by ParaView's icons (fixed colours); test ours on a dark Qt palette before shipping | ParaView behaviour on dark themes: not checked |

---

Sources not opened, so not used: Kitware's corporate brand guidelines,
Material Symbols' licence page, trade mark registers for ParaView and VTK,
tomviz's and Slicer's binary packages.
