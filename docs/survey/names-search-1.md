# Name survey for the FreeCAD to ParaView simulation GUI

Date: 2026-11-14. Raw check output: `checks1.txt`, `checks2.txt`, `checks3.txt` in this folder.

## What was checked, and how

- GitHub: `gh api search/repositories q="NAME in:name"`; total count and top 3 by stars. Counts are substring matches on repo names, so a big count often means a common word, not a project.
- PyPI: `https://pypi.org/pypi/NAME/json`; 404 = name free, 200 = taken.
- DNS: `dig +short NAME.com A` and `NAME.io A`. "none" = no A record (likely unregistered or parked without a web host; not proof the domain is free). A few .se domains and page titles were also fetched with curl.
- Web: one WebSearch query per name (or pair of names), looking for software or companies, especially CAE/CFD/simulation.
- apt: `apt-cache policy` on this machine for the clean names: no Debian/Ubuntu package of any of those names.
- NOT done: any formal trademark search (EUIPO, PRV, USPTO). Out of scope; do one before committing to a name.

## Brainstorm (44 candidates)

Swedish/Norse: Smedja (smithy), Ässja (forge hearth), Völund (the smith of the sagas), Sindri and Brokkr (dwarf smiths), Ägir (sea god), Sluss (canal lock), Brygga (jetty, also "to brew"), Färja (ferry), Spann (span), Varp/Varpa (warp thread; kedge anchor), Inslag (weft), Virvel (vortex), Fors (rapids).
Swedish compounds with -verk (works, mill), -sund (strait), -bro (bridge), -smed (smith): Sundverk, Spannverk, Flodverk, Lastverk, Smidverk, Fältverk/Faltverk, Solvverk, Bryggverk, Kraftsund, Fältsund/Faltsund, Solvsund, Strömbro, Sundsmed, Flodsmed, Strömsmed.
English craft compounds: Strainloom, Stressloom, Tensiloom, Fluxloom, FieldLoom, Strainwright, Meshwright, Fieldwright, Fieldsmith, Fluxsmith, Flowsmith, Tensmith.
Physics and in-between: Kontinua, Piola (Piola-Kirchhoff stress), Tensorium, Weir, Heddle, Isthmus.

## Checks on the shortlisted ~30

| Name | Web search | GitHub (count, top) | PyPI | .com / .io | Verdict |
|---|---|---|---|---|---|
| Sundverk | nothing named Sundverk | 0 | free | none / none (sundverk.se resolves to an IONOS host, page returned no title) | clean |
| Strainloom | nothing | 0 | free | none / none (strainloom.org none) | clean |
| Stressloom | nothing (same query as Strainloom) | 0 | free | none / none | clean |
| Spannverk | nothing as a name; German "Spannwerk" is a clamping/tensioning mechanism in machine tooling | 0 | free | none / none (spannverk.se none) | clean (minor: German tooling word) |
| Kraftsund | nothing; nearest is Raftsundet, a Norwegian strait | 0 | free | none / none | clean |
| Flodverk | nothing; nearest is Norwegian "flomverk" (flood barrier) | 0 | free | none / none | clean |
| Lastverk | nothing (same query as Flodverk) | 0 | free | none / none | clean |
| Smidverk | nothing; results only about smithing firms | 0 | free | none / none | clean |
| Fältverk / Faltverk | nothing (same query as Smidverk) | 0 | free | none / none | clean, but the ä makes the ASCII form awkward |
| Strömbro / Strombro | nothing as software; Strömbron is a Stockholm viaduct, Strömsbro a place in Gävleborg | 0 | free | none / none | clean (minor: place names) |
| Strainwright | nothing exact; Strainstall UK (strain monitoring company) is a near neighbour in name and field | 0 | free | none / none | clean, watch Strainstall |
| Tensmith | nothing | 0 | free | .com resolves (no page title) / none | clean (domain held by someone) |
| Tensiloom | not web searched | 0 | free | none / none | probably clean, web not checked |
| Sundsmed, Flodsmed, Solvsund, Strömsmed | not web searched | 0 each | free | none / none | probably clean, web not checked |
| Sluss | nothing in software | 41 (portfolio repos, "slussen" API) | free | .com resolves (genealogy site) / none | minor conflict (unrelated) |
| Brygga | Brygga is a Norwegian rehab co-operative with an app; Bryggans takeaway app | 28 (brygga.nvim, a Claude Code bridge for Neovim, 20 stars; Brygga IRC client) | free | both resolve | minor conflict |
| Bryggverk | not web searched | 4 (Swedish brewery projects "Bryggverket") | free | none / none | minor conflict |
| Smedja | nothing in simulation | 14 (Smedja game engine, smedja compiler backend, smedjan LLM engine) | free | .com resolves; smedja.se is an Estonian site | minor conflict |
| Völund / Volund | not web searched | 125 (volundr ELF library 32 stars; Volund game engine; Volund build system) | free | both resolve | minor conflict |
| Heddle | not web searched | 100 (heddlehq workflow platform 43 stars; heddle coding-agent CLI) | taken | .com resolves | minor conflict, crowded |
| Kontinua | not web searched | 19 (kontinua.io site) | taken | .com resolves | minor conflict |
| Isthmus | not web searched | 148 (CARTO geospatial UI) | free | .com resolves | minor conflict |
| Virvel | not web searched | 17 | free | .com resolves | minor conflict |
| Fluxsmith | not web searched | 2 (AI schematic design for KiCad, a wiki) | free | .com resolves | minor conflict (engineering tooling nearby) |
| FieldLoom | not web searched | 7 (a "Fieldloom" GitHub org) | free | both resolve | minor conflict |
| Fluxloom | not web searched | 4, incl. younglin90/FluxLoom described as a "1D analysis builder" (Korean) | free | .com resolves | conflict-ish: simulation field, tiny |
| Sindri | no FEA hit on the web | 275, top is MakerViking/sindricad (184 stars; README brands it SindriCAD) | taken | both resolve | conflict (CAD) |
| Fieldsmith | not web searched | 4: FraunhoferMEVIS/FieldSmith (differentiable design and optimization in PyTorch), GlassOnTin/fieldsmith ("grow its interior to fit the load"), a field-theory tool | free | both resolve | conflict (same field) |
| Meshwright | not web searched | 5, top GeekatplayStudio/Meshwright (62 stars, mesh analysis and repair for printing) | free | both resolve | conflict (mesh tooling) |
| Tensorium | not web searched | 23, TensoriumCore HPC tensor/grid library and DSL | free | none | conflict (numerics, and too close to TensorFlow) |
| Spann, Weir, Varp, Piola | not web searched | 4366, 8112, 643, 227 (common words; spann3r 3D reconstruction, varpro fitting) | Spann, Weir, Varp taken | all resolve | crowded, drop |
| Flowsmith | not web searched | 30 (workflow engines) | taken | both resolve | minor conflict, crowded |

## Ranked shortlist (clean names)

1. **Sundverk** ("SUND-verk"). *Sund* is a strait, the narrow water between two shores; *verk* is a works or mill. The workshop in the strait between FreeCAD and ParaView, and the place two flows meet (solid and fluid toolkits). *Sund* also means "sound, healthy", a nice nod to validating against measurements. Checks: no web hit, 0 GitHub repos, PyPI free, no .com/.io A record; sundverk.se does resolve (IONOS host, empty page), so check who holds it. Command `sundverk` reads well.
2. **Strainloom**. Strain plus loom: a loom weaves warp (geometry) and weft (physics) into cloth, as the app weaves toolkits into one run. English, readable worldwide, sits beside TenForge (craft word plus physics word). Checks: no web hit, 0 GitHub, PyPI free, no DNS for .com/.io/.org.
3. **Smidverk** ("SMEED-verk"). *Smide* is forging; "forge works". The natural sibling of TenForge without repeating "Forge". Checks: no web hit, 0 GitHub, PyPI free, no DNS.
4. **Kraftsund**. *Kraft* (force) plus *sund* (strait): force passing through the strait between CAD and post. Checks: no web hit (only the Norwegian strait Raftsundet), 0 GitHub, PyPI free, no DNS.
5. **Flodverk**. *Flod* (river, flood) and *flöde* (flow) plus *verk*: the flow works, with OpenFOAM in mind without using its name. Checks: no web hit (nearest is Norwegian "flomverk", flood barrier), 0 GitHub, PyPI free, no DNS.
6. **Spannverk**. *Spänn/spann*: tension and a bridge span. Stress plus bridge in one word. Checks: 0 GitHub, PyPI free, no DNS; minor overlap with German "Spannwerk" (clamping mechanism in machine tooling), unrelated as a product.
7. **Strömbro** (ASCII `strombro`). *Ström* (current, stream) plus *bro* (bridge): a bridge over the current; also the name of a viaduct by Stockholm's Royal Palace. Checks: 0 GitHub, PyPI free, no DNS; only place-name hits.
8. **Lastverk**. *Last* (load) plus *verk*: "load works". Plain and engineering-literal. Checks: no web hit, 0 GitHub, PyPI free, no DNS. Weakness: English readers see "last".
9. **Stressloom**. Same idea as Strainloom with stress. Checks: no web hit, 0 GitHub, PyPI free, no DNS. Strainloom pairs better with SurView (which measures strain).

Honourable mentions: **Strainwright** (clean on all checks, but Strainstall UK is a strain-monitoring company with a similar sound), **Tensmith** (clean, but tensmith.com is registered), **Tensiloom**, **Sundsmed**, **Solvsund** (GitHub/PyPI/DNS clean, web not searched).

Caveats: no trademark search was done; web checks were one search each with a general engine; DNS "none" means no A record, not a confirmed unregistered domain. Swedish names with å/ä/ö were avoided or given an ASCII form for the command and repo name.
