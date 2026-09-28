"""Writes the 'Architecture kit' and 'VFX meshes' sections of Docs/LaPlace/Kit.md from
SourceArt/Kit/manifest_architecture.json and QA_architecture.json. Other `##` sections of the file (e.g. the Nature
kit) are left untouched; the file is edited under the same advisory lock the other kit generators use."""
import contextlib
import fcntl
import json
import os
import re
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
KIT = os.path.join(ROOT, "SourceArt", "Kit")
DOC = os.path.join(ROOT, "Docs", "LaPlace", "Kit.md")

ARCH_CATS = ["Asura", "AsuraNoble", "Rural", "North", "Millis", "Demon", "Desert", "Walls", "Props", "Landmarks"]
CAT_TITLES = {
    "Asura": "Asura towns (Roa, Ars commoner districts)", "AsuraNoble": "Asura noble district (Ars)",
    "Rural": "Rural (Buena)", "North": "North (Sharia, northern villages)", "Millis": "Millis (Millishion)",
    "Demon": "Demon Continent (Rikarisu, Wenport)", "Desert": "Desert (Rapan)", "Walls": "City walls",
    "Props": "Props, bridges, pier, boat", "Landmarks": "Landmarks",
}


@contextlib.contextmanager
def locked(name):
    lock_path = os.path.join(tempfile.gettempdir(), "laplace_kit_%s.lock" % name)
    with open(lock_path, "w") as lf:
        fcntl.flock(lf, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(lf, fcntl.LOCK_UN)


def _slots(a):
    return ", ".join(m.replace("MT_", "") for m in a["materials"])


def _row(a, notes=True):
    sz = a.get("size", [a["footprint"][0], a["footprint"][1], a["height"]])
    n = a.get("notes", "")
    if len(n) > 150:
        n = n[:147].rsplit(" ", 1)[0] + " ..."
    return (f"| `{a['name']}` | {a['footprint'][0]:.1f} x {a['footprint'][1]:.1f} | {a['height']:.1f} | "
            f"{a['tris']} | {_slots(a)} |" + (f" {n} |" if notes else ""))


def section_arch(man, qa):
    assets = [a for a in man["assets"] if a["category"] in ARCH_CATS]
    by = {c: [a for a in assets if a["category"] == c] for c in ARCH_CATS}
    total = sum(a["tris"] for a in assets)
    qa_by = {r.get("name"): r for r in (qa or {}).get("assets", [])}
    arch_qa = [qa_by.get(a["name"]) for a in assets]
    npass = sum(1 for r in arch_qa if r and not r["errors"])
    L = []
    L.append("## Architecture kit\n")
    L.append("Procedural architecture for LA PLACE generated with Blender 5.0 (`bpy` as a Python module) by "
             "`Tools/kit/build_architecture_kit.py`: **%d assets** in %d categories (%s), %d triangles in total. "
             "Deterministic (every asset is seeded from its name; no network, no clock). This section is regenerated "
             "by `Tools/kit/arch_docs.py` on every full build; edit that script, not the text.\n"
             % (len(assets), len([c for c in ARCH_CATS if by[c]]),
                ", ".join("%s %d" % (c, len(by[c])) for c in ARCH_CATS if by[c]), total))
    L.append("### Files\n")
    L.append("| Path | Content |\n|---|---|")
    L.append("| `SourceArt/Kit/<Category>/<Name>.glb` | one GLB per asset (git-ignored, rebuilt): Asura, AsuraNoble, "
             "Rural, North, Millis, Demon, Desert, Walls, Props, Landmarks, VFX |")
    L.append("| `SourceArt/Kit/<Category>/_Preview.png` | contact sheet per category (EEVEE render of the exported "
             "GLBs, re-imported) |")
    L.append("| `SourceArt/Kit/manifest_architecture.json` | architecture + VFX asset list: name, category, style, "
             "kind, file, footprint, height, size, min_z, tris, materials, pivot, foundation, front, budget, bounds, "
             "notes, anchors / attach points |")
    L.append("| `SourceArt/Kit/manifest.json` | shared manifest; architecture entries are merged in by name under a "
             "file lock (same lock as the nature kit) |")
    L.append("| `SourceArt/Kit/QA_architecture.json` | per-asset QA results |")
    L.append("")
    L.append("Tools (all in `Tools/kit/`, prefixed `arch_` so they never collide with the nature kit): "
             "`build_architecture_kit.py` (entry point: build, export, manifests, sheets, QA, docs), `arch_geo.py` "
             "(mesh builder: parts, transforms, triangulation with holes, welding, UVs, Blender mesh), "
             "`arch_parts.py` (walls with recessed openings, trims, timber framing, stepped-course roofs, snow, "
             "dormers, chimneys, stairs, porticoes, balustrades, domes, arcades), `arch_house.py` (parametric "
             "storeyed house), set modules `arch_asura.py`, `arch_noble.py`, `arch_rural.py`, `arch_north.py`, "
             "`arch_millis.py`, `arch_demon.py`, `arch_desert.py`, `arch_walls.py`, `arch_props.py`, "
             "`arch_landmarks.py`, `arch_vfx.py`, plus `arch_palette.py` (slot names + preview colours), "
             "`arch_registry.py`, `arch_render.py` (headless previews), `arch_debug.py` (overlap diagnostics), "
             "`qa_architecture_kit.py` and `arch_docs.py`.\n")
    L.append("### Rebuild\n")
    L.append("```sh\nPY=~/.venvs/mushoku-bpy311/bin/python          # Python 3.11 with bpy 5.0, numpy, Pillow\n"
             "$PY Tools/kit/build_architecture_kit.py              # everything: GLBs, sheets, manifests, QA, this doc\n"
             "$PY Tools/kit/build_architecture_kit.py --category Walls,Props\n"
             "$PY Tools/kit/build_architecture_kit.py --only 'SM_Asura_*' --views /tmp/v --view-angles=-30:20,150:25\n"
             "$PY Tools/kit/build_architecture_kit.py --no-sheets    # geometry, manifests and QA only (~30 s)\n"
             "$PY Tools/kit/qa_architecture_kit.py [--only NAME] [--verbose] [--all]   # QA only; exit 1 on failure\n"
             "```\n")
    L.append("### Conventions (Spec sections 6-7)\n")
    L.append("- Metres, Blender Z-up (the glTF exporter writes Y-up), +X right, **fronts / doors face -Y**. One mesh "
             "node per file named after the asset, transforms applied (QA checks identity node transforms).")
    L.append("- Pivot (`pivot` in the manifest): `base` = centre of the XY bounding box at ground level z = 0; "
             "`base_point` = a documented attachment point at z = 0 (wall axis, tower axis, bridge centre, flame "
             "root, ...); `center` = bounding-box centre (VFX); `hub` = windmill blades rotation centre.")
    L.append("- Buildings sit on a buried foundation skirt (plinth / body continue down to `min_z`, typically "
             "-0.5 m; walls -1.0 m; landmarks up to -1.5 m; bridges reach the river bed) so they never float on "
             "gently sloping ground. Ground floors are raised 0.25-0.9 m with steps to the door.")
    L.append("- UVs: box-projected at **1 UV unit = 1 m** on all architecture; roof slopes use a planar projection "
             "along the slope (u horizontal, v up-slope, still 1 UV = 1 m); cylindrical surfaces unwrap by arc "
             "length. VFX meshes use the explicit layouts in the VFX table.")
    L.append("- Material slots use the Spec palette names verbatim (MT_Plaster, MT_Timber, MT_RoofRed, ...); the GLB "
             "materials only carry preview base colours.")
    L.append("- No coincident faces, by construction: every part is a closed (or buried-back) shell that is sunk a "
             "few centimetres into whatever it touches, and overlapping attachments use distinct proud depths "
             "(e.g. corner posts 0.07 m, posts 0.06, rails 0.05, braces 0.04/0.035, window frames 0.03, sills 0.08). "
             "Openings are real recessed pockets in the wall solids (glass / door leaf at the back, reveals on the "
             "sides), not decals. Roof tiles are stepped courses in the roof section; snow blankets are separate "
             "solids whose underside is buried under the tile valleys and whose top clears the lips and ridge caps.")
    L.append("- Budgets: houses 1-6k tris; large buildings (manor, keep, guild, inn courtyard, caravan yard, chapel, "
             "hall) up to 15k; wall pieces up to 8k (gates 12k); props up to 4k; bridges up to 6k; landmarks up to "
             "40k; VFX up to 3k.\n")
    L.append("### QA\n")
    L.append("`qa_architecture_kit.py` parses every GLB with its own numpy glTF reader (no Blender) and checks: "
             "identity node transforms, triangle count against the asset budget, bounds, pivot (base centre on the "
             "ground within the declared foundation depth, or the declared centre / base point), material slots in "
             "the Spec palette or `VFX`, UVs present and finite, zero-area triangles (< 1e-8 m2), duplicate "
             "triangles (same three corners within 1 mm, any winding), loose vertices, **coplanar overlapping "
             "triangles** (same plane within 1 mm and > 1 cm2 overlap: z-fighting risk, split into same-facing and "
             "back-to-back), inconsistent winding (a directed edge used twice) and closed shells with inward "
             "normals. Latest run: **%d / %d architecture assets pass**.\n" % (npass, len(assets)))
    L.append("### Attachment and placement notes\n")
    notes = [
        "**Windmill**: `SM_Rural_Windmill_Body` has its origin on the tower axis; attach `SM_Rural_Windmill_Blades` "
        "at the body's `hub` anchor **(0, -3.15, 11.25)** (hub height 11.25 m). The blades' origin is the hub centre; "
        "rotate them about their local **Y** axis (the forward axis; the sails face -Y).",
        "**City walls** (`Walls/`, six styles): `SM_<Style>_Wall_12m` is exactly **12.0 m** long along X "
        "(x = -6..+6, origin on the wall centreline), outer face -Y, walkway at 7-10 m by style. Place segments end to "
        "end every 12 m (never overlap them); for a run of length L use n = round(L/12) segments scaled along X by "
        "L/(12n). Put `SM_<Style>_WallTower` (origin = tower axis, radius 4.0-4.6 m) on joints and corners: walls "
        "meeting at any angle end inside it (their overlap is hidden in the tower). `SM_<Style>_Gate` replaces one "
        "segment: its flanking towers are centred at x = +/-6 m (on the joints), the open arched passage is "
        "**5.0 m wide, 6.4 m to the crown**, with a raised portcullis leaving 3.8 m clearance.",
        "**Fences**: `SM_Rural_Fence_2m` / `_4m` span x = -L/2..+L/2 with a post at each end; the +X post is smaller "
        "and nests inside the next segment's -X post, so segments chain every 2 / 4 m without z-fighting.",
        "**Bridges**: `SM_Bridge_Stone_12m` / `_24m` / `SM_Bridge_Wood_10m` run along X with the deck ends at "
        "z = 0 at x = +/-L/2 (resting on the banks); stone decks rise 0.6 / 1.0 m at mid-span, deck widths 4.3 m "
        "(stone, between parapets) and 2.3 m (wood); abutments / piles reach 3-3.6 m below the banks.",
        "**Pier**: `SM_Prop_Pier_10m` deck top at z = 0 (bank level), shore end at y = +5, water end at y = -5, "
        "piles to z = -3. **Boat**: `SM_Prop_Boat_A` origin at the waterline centre, draft 0.42 m, bow toward -Y.",
        "**Fountain**: water surface (MT_Glass placeholder) at z = 0.66 m, 12 cm below the 0.78 m rim; the upper "
        "bowl holds its own water.",
        "**Landmarks**: Silver Palace terrace top at 4.0 m with a 36 m grand stair on -Y; Ranoa University gate "
        "passage 7.0 x 8.5 m through the front range; Roa Keep gate passage 5.0 m; Rapan Guild portal 11 m wide "
        "(door 3.6 m); Labyrinth Gate opening 13 x 22 m, a 9 m deep dark recess, and the block runs 14 m back (+Y) "
        "so it can be sunk into a cliff; Rikarisu Hall arena floor at 1.1 m.",
        "**Stalls / shops**: market stall counters face -Y; shop openings are recessed with counters and cloth "
        "awnings; lanterns on the North shop and street lamps glow through MT_Crystal.",
    ]
    for n in notes:
        L.append("- " + n)
    L.append("")
    for c in ARCH_CATS:
        if not by[c]:
            continue
        L.append("### %s (`SourceArt/Kit/%s/`)\n" % (CAT_TITLES[c], c))
        L.append("Preview: `SourceArt/Kit/%s/_Preview.png`. Footprint = XY bounding box; height = top above "
                 "ground.\n" % c)
        L.append("| Asset | Footprint (m) | Height (m) | Tris | Slots | Notes |\n|---|---|---|---|---|---|")
        for a in by[c]:
            L.append(_row(a))
        L.append("")
    return "\n".join(L).rstrip() + "\n"


def section_vfx(man, qa):
    assets = [a for a in man["assets"] if a["category"] == "VFX"]
    qa_by = {r.get("name"): r for r in (qa or {}).get("assets", [])}
    npass = sum(1 for a in assets if qa_by.get(a["name"]) and not qa_by[a["name"]]["errors"])
    L = ["## VFX meshes\n"]
    L.append("Spell-effect meshes in `SourceArt/Kit/VFX/` (%d assets, built by the same "
             "`Tools/kit/build_architecture_kit.py --category VFX`, section regenerated by `Tools/kit/arch_docs.py`). "
             "Unit sizes so the game scales them; single material slot `VFX` except the rock pieces (`MT_Rock`). "
             "Every mesh is a closed shell with outward normals; the 'open' funnel and beam are thin double-walled "
             "shells so both sides render with back-face culling. Preview: `SourceArt/Kit/VFX/_Preview.png`. "
             "QA: **%d / %d pass**.\n" % (len(assets), npass, len(assets)))
    L.append("| Asset | Size x/y/z (m) | Tris | Slot | Pivot | Geometry and UVs |\n|---|---|---|---|---|---|")
    for a in assets:
        sz = a.get("size", [a["footprint"][0], a["footprint"][1], a["height"]])
        L.append("| `%s` | %.2f / %.2f / %.2f | %d | %s | %s | %s |" % (
            a["name"], sz[0], sz[1], sz[2], a["tris"], ", ".join(a["materials"]), a["pivot"], a.get("notes", "")))
    L.append("")
    L.append("Pivots: `center` = bounding-box centre on the origin; `base_point` = the root at z = 0 (flame base tip, "
             "funnel bottom-ring centre, spike base centre); `base` = base centre at z = 0. The dragon head faces +Y "
             "with its neck ring (radius 0.5 m, matching `SM_VFX_DragonSegment`) at the `neck` anchor in the "
             "manifest; chain segments 1 m apart along the body curve behind it.")
    return "\n".join(L).rstrip() + "\n"


def _replace_section(doc, title, text):
    m = re.search(r"^## %s[^\n]*\n" % re.escape(title), doc, flags=re.M)
    if m:
        nxt = re.search(r"^## (?!#)", doc[m.end():], flags=re.M)
        end = m.end() + nxt.start() if nxt else len(doc)
        return doc[:m.start()] + text + ("\n" if nxt else "") + doc[end:]
    return doc.rstrip("\n") + "\n\n" + text


def write_docs(man=None, qa=None):
    man = man or json.load(open(os.path.join(KIT, "manifest_architecture.json")))
    qp = os.path.join(KIT, "QA_architecture.json")
    if qa is None and os.path.exists(qp):
        qa = json.load(open(qp))
    arch = section_arch(man, qa)
    vfx = section_vfx(man, qa)
    os.makedirs(os.path.dirname(DOC), exist_ok=True)
    with locked("Kit.md"):
        doc = open(DOC).read() if os.path.exists(DOC) else ""
        if not doc.strip():
            doc = ("# LA PLACE asset kits\n\nGenerated Blender kits (GLB) for LA PLACE; see `Docs/LaPlace/Spec.md` "
                   "sections 6-7 for the contract. Each kit owns one `##` section, maintained by its generator.\n\n")
        doc = _replace_section(doc, "Architecture kit", arch)
        doc = _replace_section(doc, "VFX meshes", vfx)
        tmp = DOC + ".tmp%d" % os.getpid()
        with open(tmp, "w") as f:
            f.write(doc if doc.endswith("\n") else doc + "\n")
        os.replace(tmp, DOC)
    print("wrote the Architecture kit and VFX meshes sections of", os.path.relpath(DOC, ROOT))


if __name__ == "__main__":
    write_docs()
