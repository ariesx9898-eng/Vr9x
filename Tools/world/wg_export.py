"""World.json and Locations.json writers (Spec sections 4-5). All positions in UE centimetres."""
import json
import math
import os

import numpy as np

import wg_config as C
import wg_geo as GEO
import wg_grid as G


def cm(x, y):
    return x * 100.0 + C.LANDSCAPE_MIN_X_CM, y * 100.0 + C.LANDSCAPE_MIN_Y_CM


def vec(x, y, z):
    X, Y = cm(x, y)
    return {"X": round(X, 1), "Y": round(Y, 1), "Z": round(z * 100.0, 1)}


def map_uv(x, y):
    return {"X": round(x / C.WORLD_W_M, 5), "Y": round(y / C.WORLD_H_M, 5)}


def ground_z(h0, x, y):
    return float(G.sample_bilinear(h0, np.array([x / C.QUAD_M]), np.array([y / C.QUAD_M]))[0])


def trace_contour(mask):
    """Outer boundary of the largest component of a boolean mask as a list of (col, row) vertex coords."""
    H, W = mask.shape
    m = np.pad(mask, 1)
    ys, xs = np.nonzero(m)
    if len(ys) == 0:
        return []
    k = np.lexsort((xs, ys))[0]
    start = (int(ys[k]), int(xs[k]))
    dirs = [(-1, 0), (-1, 1), (0, 1), (1, 1), (1, 0), (1, -1), (0, -1), (-1, -1)]
    b = start
    cdir = 6
    out = [b]
    for _ in range(200000):
        found = False
        for i in range(8):
            d = (cdir + 1 + i) % 8
            ny, nx = b[0] + dirs[d][0], b[1] + dirs[d][1]
            if m[ny, nx]:
                cdir = (d + 4) % 8
                b = (ny, nx)
                found = True
                break
        if not found or (b == start and len(out) > 2):
            break
        out.append(b)
    return [(x - 1, y - 1) for y, x in out]


def simplify(pts, tol):
    pts = np.asarray(pts, np.float64)
    if len(pts) < 4:
        return pts
    keep = np.zeros(len(pts), bool)
    keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        i, j = stack.pop()
        if j <= i + 1:
            continue
        p, q = pts[i], pts[j]
        d = q - p
        Ln = math.hypot(*d)
        seg = pts[i + 1:j]
        if Ln < 1e-9:
            dist = np.hypot(*(seg - p).T)
        else:
            dist = np.abs(d[0] * (seg[:, 1] - p[1]) - d[1] * (seg[:, 0] - p[0])) / Ln
        k = int(np.argmax(dist))
        if dist[k] > tol:
            keep[i + 1 + k] = True
            stack.append((i, i + 1 + k))
            stack.append((i + 1 + k, j))
    return pts[keep]


def lake_polygons(lakes, lake_level0):
    out = []
    for lk in lakes:
        sl1 = lk["mask_sl"]
        r0, r1 = sl1[0].start * 3, (sl1[0].stop - 1) * 3 + 1
        c0, c1 = sl1[1].start * 3, (sl1[1].stop - 1) * 3 + 1
        sub = lake_level0[r0:r1, c0:c1]
        m = np.isfinite(sub) & (np.abs(sub - lk["level"]) < 1e-3)
        if m.sum() < 3:
            continue
        lab = G.label_components(m, 8)
        ids, cnt = G.component_sizes(lab)
        mm = lab == ids[np.argmax(cnt)]
        ring = trace_contour(mm)
        pts = simplify(np.array([(x + c0, y + r0) for x, y in ring], np.float64) * C.QUAD_M, 2.0)
        lk["polygon_m"] = pts
        out.append(lk)
    return out


DESCRIPTIONS = {
    "Ars": "The royal capital of Asura, a sprawl of walled wards and busy markets spread beneath the Silver Palace on "
           "its rise at the city's edge.",
    "Roa": "Walled seat of the Fittoa Region, where stone streets gather around a market square and the Boreas "
           "manor keeps watch over the farmland.",
    "Buena": "A quiet farming village of timber houses, wheat fields and a turning windmill at the foot of the Red "
             "Wyrm foothills; the Greyrat home sits on a gentle hill nearby.",
    "Sharia": "A cold northern city of stone and timber, full of magic shops and workshops, crowned by the spires "
              "of the Ranoa University of Magic.",
    "Rikarisu": "A dense town of rock-cut buildings packed onto the floor of a vast crater, the first stop for "
                "adventurers crossing the Demon Continent.",
    "Wenport": "A rough harbour at the southern tip of the Demon Continent where ships from Millis unload under "
               "dusty red cliffs.",
    "ZantPort": "The northern harbour of Millis, a gateway between the green Great Forest and the ships bound for "
                "the Demon Continent.",
    "Millishion": "The white holy capital of Millis, ordered streets and bright plazas rising to a great cathedral "
                  "beside a calm lake.",
    "WestPort": "A busy trading port on the western point of Millis, facing East Port across a narrow strait dotted "
                "with islets.",
    "EastPort": "The ferry town at the southern tip of the Central Continent, where travellers cross the strait "
                "to Millis.",
    "KingDragon": "Capital of the King Dragon Kingdom, a fortified town on the narrow southern cape below the King "
                  "Dragon range.",
    "Rapan": "A sandstone city of bazaars, inns and caravan yards that lives off the labyrinths hidden in the "
             "Begaritt desert.",
    "SwordSanctuary": "A secluded snowbound village in the far north-west where swordsmen come to train under "
                      "strict masters.",
    "Labyrinth_1": "A colossal stone gate carved into the inner wall of the rocky ring, leading down into a "
                   "labyrinth.",
    "Labyrinth_2": "An ancient gate set into a mesa cliff on the northern badlands of Begaritt.",
    "Labyrinth_3": "A half-buried gate in a canyon wall, guarded by wind-carved rock.",
    "Labyrinth_4": "A monumental gate cut into the sandstone butte north-west of Rapan.",
    "Labyrinth_5": "A lonely gate in the flank of a southern butte, reached by a faint caravan track.",
}

LEGACY_QUEST_LOCATIONS = [
    # id, display name, placement rule, discovery radius m, rank
    ("Buena_Village", "Buena Village", "site:Buena", 300.0, "F"),
    ("Buena_Square", "Buena Village Square", "centre:Buena", 25.0, "F"),
    ("Buena_Greyrat_House", "Greyrat House", "landmark:Buena:Greyrat House", 15.0, "F"),
    ("Buena_Farms", "Buena Wheat Fields", "farms:Buena", 90.0, "F"),
    ("Buena_Hill_Tree", "Hill of the Lone Tree", "hill:Buena", 30.0, "F"),
    ("Buena_Forest_Edge", "Buena Forest Edge", "forest:Buena", 60.0, "F"),
    ("Fittoa_River_Ford", "Fittoa River Crossing", "bridge:FittoaRiver", 30.0, "F"),
    ("Fittoa_Deep_Forest", "Deep Fittoa Forest", "deepforest:Buena", 120.0, "E"),
    ("Fittoa_Watchtower_Ruins", "Old Watchtower Ruins", "hill2:Buena", 40.0, "D"),
    ("Fittoa_Road_To_Roa", "Road to Roa", "roadmid:FittoaLane", 50.0, "F"),
    ("Roa_Gate", "Roa Road Waystone", "gate:Roa", 30.0, "F"),
    ("Fittoa_Wyrm_Foothills", "Red Wyrm Foothills", "foothills:Buena", 150.0, "C"),
]

SPAWN = {"Buena": 1, "Roa": 1, "Ars": 2, "Sharia": 3, "Rikarisu": 4, "Millishion": 2, "Rapan": 5}
FAST_ONLY = {"Wenport": 3, "ZantPort": 2}
DIFFICULTY = {"WestPort": 2, "EastPort": 2, "KingDragon": 3, "SwordSanctuary": 4, "Labyrinth_1": 5,
              "Labyrinth_2": 5, "Labyrinth_3": 5, "Labyrinth_4": 5, "Labyrinth_5": 5}
BIOME_NAME = {1: "TemperatePlains", 2: "RuralFields", 3: "Alpine", 4: "Snowfield", 5: "DryHills", 6: "Jungle",
              7: "HighPlateau", 8: "Badlands", 9: "Crater", 10: "GiantForest", 11: "GreenHills", 12: "Alpine",
              13: "Desert", 14: "Badlands", 15: "Islands"}


def preview_camera(h0, x, y, z, radius, prefer_yaw, lv0=G.L0):
    """Scenic camera looking at (x, y, z): distance ~2.4 R, raised so the view line clears the terrain."""
    dist = max(260.0, 2.4 * radius)
    best = None
    for k in range(12):
        a = math.radians(prefer_yaw + 180.0 + (k // 2 + 1) * 25.0 * (-1) ** k) if k else math.radians(prefer_yaw + 180.0)
        cx, cy = x + dist * math.cos(a), y + dist * math.sin(a)
        if not (10 < cx < C.WORLD_W_M - 10 and 10 < cy < C.WORLD_H_M - 10):
            continue
        cz = max(ground_z(h0, cx, cy), 0.0) + 0.38 * dist + 25.0
        # clear the line of sight
        for t in np.linspace(0.1, 0.9, 12):
            px, py = cx + (x - cx) * t, cy + (y - cy) * t
            need = ground_z(h0, px, py) + 12.0
            lz = cz + (z - cz) * t
            if lz < need:
                cz += (need - lz) / max(1e-3, 1 - t)
        cost = cz - z
        if best is None or cost < best[0]:
            best = (cost, cx, cy, cz)
    _, cx, cy, cz = best
    yaw = math.degrees(math.atan2(y - cy, x - cx))
    pitch = -math.degrees(math.atan2(cz - z, math.hypot(x - cx, y - cy)))
    return {"Location": vec(cx, cy, cz), "Rotation": {"Pitch": round(pitch, 2), "Yaw": round(yaw, 2), "Roll": 0.0}}


def _region_key(rid):
    return C.REGIONS[rid][0]


def write_all(D, h0, Hy, sites, roads, bridges, lakes_poly, stats, P, out_world, out_locations, log=print):
    region = D.region
    sbid = {s.id: s for s in sites}
    # ---------------------------------------------------------------- World.json
    W = {
        "Version": 1,
        "Title": "LA PLACE",
        "Generator": "Tools/world/generate_world.py",
        "Seed": C.SEED,
        "Units": "UE centimetres; Z is height above sea level (sea level Z = 0)",
        "Landscape": {
            "VerticesX": C.FULL_W, "VerticesY": C.FULL_H, "QuadSizeCm": C.QUAD_M * 100.0,
            "ComponentsX": 24, "ComponentsY": 18, "QuadsPerComponent": 254, "SectionsPerComponent": 2,
            "QuadsPerSection": 127,
            "LocationCm": {"X": C.LANDSCAPE_MIN_X_CM, "Y": C.LANDSCAPE_MIN_Y_CM, "Z": C.LANDSCAPE_Z_CM},
            "ScaleCm": {"X": 300.0, "Y": 300.0, "Z": C.LANDSCAPE_SCALE_Z},
            "HeightEncoding": "Z_cm = 72400 + (h - 32768) * 400 / 128; uint16 little endian, rows = Y (north to south)",
            "SeaLevelZCm": 0.0, "SeaLevelH": C.H_SEA,
            "Heightmap": "SourceArt/World/Height.r16",
            "Layers": ["SourceArt/World/Layers/%s.png" % n for n in C.LAYERS],
            "LayerNames": C.LAYERS,
            "RegionsMap": "SourceArt/World/Regions.png",
            "DensityMaps": {k: "SourceArt/World/Density/%s.png" % k for k in C.DENSITY_KINDS},
            "WorldMap": "SourceArt/World/WorldMap.png",
        },
        "MapToWorld": {"XCm": "(u - 0.5) * 1828800", "YCm": "(v - 0.5) * 1371600",
                       "WorldMapPixels": [C.WORLDMAP_W, C.WORLDMAP_H],
                       "Note": "WorldMap.png and MapUV cover exactly the landscape rectangle"},
        "Regions": [],
        "Sites": [], "Roads": [], "Rivers": [], "Lakes": [], "Bridges": [], "DryRiverbeds": [], "Passes": [],
        "MountainRanges": [],
    }
    for rid, (key, name, cont, biome) in C.REGIONS.items():
        st = stats["regions"].get(rid, {})
        W["Regions"].append({"Id": rid, "Key": key, "Name": name, "Continent": cont, "Biome": biome,
                             "AreaKm2": round(st.get("area_km2", 0.0), 3),
                             "HeightM": {"Min": st.get("min"), "Median": st.get("median"), "Max": st.get("max")}})
    for s in sites:
        zc = ground_z(h0, s.x, s.y)
        W["Sites"].append({
            "Id": s.id, "Name": s.name, "RegionId": s.region, "Region": _region_key(s.region),
            "Continent": C.REGIONS[s.region][2], "Style": s.style,
            "Center": vec(s.x, s.y, zc), "GroundZ": round(zc * 100.0, 1), "RadiusCm": round(s.r * 100.0),
            "Yaw": round(s.yaw, 2), "TiltPercent": {"X": round(s.tilt[0] * 100, 3), "Y": round(s.tilt[1] * 100, 3)},
            "MapUV": map_uv(s.x, s.y), "MovedFromSpecM": round(s.moved_m, 1),
            "Districts": s.districts,
            "Landmarks": [{"Name": lm["name"], "Location": vec(lm["x"], lm["y"], ground_z(h0, lm["x"], lm["y"])),
                           "Yaw": round(lm["yaw"], 2), "RadiusCm": round(lm["r"] * 100.0),
                           "RiseCm": round(lm.get("rise", 0.0) * 100.0)} for lm in s.landmarks],
            "RoadEntry": vec(s.road_entry[0], s.road_entry[1], ground_z(h0, *s.road_entry)) if hasattr(s, "road_entry") else None,
        })
    for rd in roads:
        p, z = rd["pts"], rd["z"]
        idx = list(range(len(p)))
        W["Roads"].append({"Id": rd["key"], "Name": rd["name"], "Class": rd["klass"], "WidthCm": rd["width"] * 100.0,
                           "Stops": rd["stops"], "LengthM": round(float(rd["s"][-1]), 1),
                           "MaxGradePercent": round(100 * rd["max_grade"], 2),
                           "Points": [vec(p[i, 0], p[i, 1], float(z[i])) for i in idx]})
    for rv in Hy.rivers:
        p = rv["pts"]
        W["Rivers"].append({"Id": rv["key"], "Name": rv["name"], "Continent": rv["continent"],
                            "Mouth": "Sea" if rv["ends_in"] == "sea" else "Lake:" + rv["ends_in"].split(":", 1)[1],
                            "FromLake": rv.get("lake_in"),
                            "LengthM": round(float(np.sum(np.hypot(np.diff(p[:, 0]), np.diff(p[:, 1])))), 1),
                            "Points": [dict(vec(p[i, 0], p[i, 1], float(rv["zw"][i])),
                                            WidthCm=round(float(rv["width"][i]) * 100.0, 1),
                                            DepthCm=round(float(rv["depth"][i]) * 100.0, 1))
                                       for i in range(len(p))]})
    for lk in lakes_poly:
        W["Lakes"].append({"Id": lk["key"], "Name": lk["name"], "Kind": lk["kind"], "Frozen": lk["frozen"],
                           "WaterZ": round(lk["level"] * 100.0, 1), "AreaM2": round(lk["area_m2"], 1),
                           "Polygon": [dict(zip(("X", "Y"), [round(v, 1) for v in cm(x, y)])) for x, y in lk["polygon_m"]]})
    for i, b in enumerate(bridges):
        W["Bridges"].append({"Id": "Bridge_%02d" % (i + 1), "Road": b["road"], "River": b["river"],
                             "Start": vec(*b["start"]), "End": vec(*b["end"]), "DeckZ": round(b["deck"] * 100.0, 1),
                             "WaterZ": round(b["water"] * 100.0, 1), "LengthM": round(b["length"], 1),
                             "WidthCm": round(b["road_width"] * 100.0 + 200.0)})
    for i, db in enumerate(Hy.drybeds):
        p = db["pts"][::2]
        W["DryRiverbeds"].append({"Id": "DryBed_%d" % (i + 1),
                                  "Points": [dict(vec(x, y, ground_z(h0, x, y)), WidthCm=round(float(w) * 100.0))
                                             for (x, y), w in zip(p, db["width"][::2])]})
    for key, Pp in GEO.PASSES.items():
        x, y = Pp["uv"][0] * C.WORLD_W_M, Pp["uv"][1] * C.WORLD_H_M
        W["Passes"].append({"Id": key, "Name": {"UpperJaw": "Red Wyrm's Upper Jaw", "LowerJaw": "Red Wyrm's Lower Jaw"}[key],
                            "Location": vec(x, y, ground_z(h0, x, y))})
    for R in GEO.RANGES:
        W["MountainRanges"].append({"Id": R["name"], "RegionId": R["region"],
                                    "Crests": [[map_uv(p[0] * C.WORLD_W_M, p[1] * C.WORLD_H_M) for p in line]
                                               for line in R["lines"]]})
    with open(out_world, "w", encoding="utf-8") as f:
        json.dump(W, f, indent=1)
    log("  wrote %s" % out_world)

    # ---------------------------------------------------------------- Locations.json
    rows = []
    order = ["Buena", "Roa", "Ars", "Sharia", "Rikarisu", "Millishion", "Rapan", "Wenport", "ZantPort", "WestPort",
             "EastPort", "KingDragon", "SwordSanctuary", "Labyrinth_1", "Labyrinth_2", "Labyrinth_3", "Labyrinth_4",
             "Labyrinth_5"]
    for sid in order:
        s = sbid[sid]
        spawn = sid in SPAWN
        fast = spawn or sid in FAST_ONLY
        rp = None
        for rd in roads:
            if sid in rd["stops"]:
                rp = rd["pts"]
                break
        loc_x, loc_y, yaw = spawn_point(s, rp)
        z = ground_z(h0, loc_x, loc_y)
        rows.append({
            "LocationID": sid, "DisplayName": s.name if sid != "KingDragon" else "King Dragon Kingdom",
            "Continent": C.REGIONS[s.region][2], "Region": _region_key(s.region),
            "Biome": BIOME_NAME.get(s.region, "Grassland"),
            "Difficulty": SPAWN.get(sid, FAST_ONLY.get(sid, DIFFICULTY.get(sid, 2))),
            "Description": DESCRIPTIONS[sid],
            "WorldLocation": vec(loc_x, loc_y, z), "SpawnYaw": round(yaw, 2),
            "bSpawnPoint": spawn, "bFastTravel": fast,
            "DiscoveryRadius": round(max(s.r * 1.6, 120.0) * 100.0),
            "RequiredRank": "F", "MapUV": map_uv(s.x, s.y),
            "PreviewCamera": preview_camera(h0, s.x, s.y, ground_z(h0, s.x, s.y), s.r, s.yaw),
        })
    rows += legacy_rows(D, h0, Hy, sites, roads, bridges, P)
    with open(out_locations, "w", encoding="utf-8") as f:
        json.dump(rows, f, indent="\t")
    log("  wrote %s (%d rows)" % (out_locations, len(rows)))
    return W, rows


def spawn_point(s, road_pts=None):
    """On the main road inside the flat core of the footprint (streets stay clear of buildings), facing the centre."""
    if road_pts is not None and len(road_pts) and s.mode != "gate":
        dd = np.hypot(road_pts[:, 0] - s.x, road_pts[:, 1] - s.y)
        k = int(np.argmin(np.abs(dd - 0.45 * s.r)))
        x, y = float(road_pts[k, 0]), float(road_pts[k, 1])
        yaw = math.degrees(math.atan2(s.y - y, s.x - x))
        return x, y, yaw
    if hasattr(s, "road_entry") and s.mode != "gate":
        ex, ey = s.road_entry
        a = math.atan2(ey - s.y, ex - s.x)
        rr = 0.45 * s.r
        x, y = s.x + rr * math.cos(a), s.y + rr * math.sin(a)
        yaw = math.degrees(math.atan2(s.y - y, s.x - x))
        return x, y, yaw
    if s.mode == "gate":
        a = math.radians(s.yaw)
        x, y = s.x + 0.6 * s.r * math.cos(a), s.y + 0.6 * s.r * math.sin(a)
        return x, y, (s.yaw + 180.0) % 360.0
    return s.x, s.y, s.yaw


def legacy_rows(D, h0, Hy, sites, roads, bridges, P):
    """Fittoa quest locations (ids referenced by Quests.json) placed around the new Buena / Roa."""
    sb = {s.id: s for s in sites}
    B = sb["Buena"]
    out = []
    lv1 = G.L1
    hb = G.blur(G.downsample(h0, 3), 2.0)

    def best_in_ring(r0, r1, score_fn):
        best, bx, by = -1e18, B.x, B.y
        for a in range(0, 360, 6):
            for rr in np.linspace(r0, r1, 8):
                x = B.x + rr * math.cos(math.radians(a))
                y = B.y + rr * math.sin(math.radians(a))
                sc = score_fn(x, y)
                if sc > best:
                    best, bx, by = sc, x, y
        return bx, by

    def hz(x, y):
        return float(G.sample_bilinear(hb, np.array([x / lv1.cell]), np.array([y / lv1.cell]))[0])

    def fo(x, y):
        return float(G.sample_bilinear(P.forest, np.array([x / lv1.cell]), np.array([y / lv1.cell]))[0])

    def fa(x, y):
        return float(G.sample_bilinear(P.farm, np.array([x / lv1.cell]), np.array([y / lv1.cell]))[0])

    def dw(x, y):
        return float(G.sample_bilinear(P.dwater, np.array([x / lv1.cell]), np.array([y / lv1.cell]))[0])

    for lid, name, rule, disc, rank in LEGACY_QUEST_LOCATIONS:
        kind, *args = rule.split(":")
        if kind == "site":
            x, y = B.x, B.y
        elif kind == "centre":
            x, y = B.x + 12.0, B.y + 6.0
        elif kind == "landmark":
            lm = [l for l in sb[args[0]].landmarks if l["name"] == args[1]]
            x, y = (lm[0]["x"], lm[0]["y"]) if lm else (B.x, B.y)
        elif kind == "farms":
            x, y = best_in_ring(B.r + 60, B.r + 700, lambda x, y: fa(x, y) - 0.0005 * math.hypot(x - B.x, y - B.y))
        elif kind == "hill":
            x, y = best_in_ring(B.r + 150, B.r + 650, lambda x, y: hz(x, y) - 0.02 * math.hypot(x - B.x, y - B.y)
                                - 50.0 * (dw(x, y) < 30))
        elif kind == "hill2":
            x, y = best_in_ring(700, 1600, lambda x, y: hz(x, y) - 0.01 * math.hypot(x - B.x, y - B.y) - 50.0 * (dw(x, y) < 30))
        elif kind == "forest":
            x, y = best_in_ring(B.r + 100, B.r + 1200,
                                lambda x, y: -abs(fo(x, y) - 0.5) - 0.0004 * math.hypot(x - B.x, y - B.y) - 5.0 * (dw(x, y) < 20))
        elif kind == "deepforest":
            x, y = best_in_ring(B.r + 300, 2600, lambda x, y: fo(x, y) - 0.0001 * math.hypot(x - B.x, y - B.y))
        elif kind == "bridge":
            bb = [b for b in bridges if b["river"] == args[0]]
            if bb:
                b = min(bb, key=lambda b: math.hypot(b["x"] - B.x, b["y"] - B.y))
                # beside the bridge on the Buena side
                x, y = b["start"][0], b["start"][1]
            else:
                rv = [r for r in Hy.rivers if r["key"] == args[0]][0]
                k = int(np.argmin(np.hypot(rv["pts"][:, 0] - B.x, rv["pts"][:, 1] - B.y)))
                x, y = rv["pts"][k] + np.array([rv["width"][k] + 6.0, 0.0])
        elif kind == "roadmid":
            rd = [r for r in roads if r["key"] == args[0]][0]
            k = len(rd["pts"]) // 2
            x, y = rd["pts"][k]
        elif kind == "gate":
            s = sb[args[0]]
            ex, ey = s.road_entry if hasattr(s, "road_entry") else (s.x + s.r, s.y)
            a = math.atan2(ey - s.y, ex - s.x)
            x, y = s.x + (s.r + 25.0) * math.cos(a), s.y + (s.r + 25.0) * math.sin(a)
        elif kind == "foothills":
            x, y = best_in_ring(900, 2200, lambda x, y: -abs(hz(x, y) - 230.0) - 0.02 * math.hypot(x - B.x, y - B.y))
        else:
            x, y = B.x, B.y
        z = ground_z(h0, x, y)
        out.append({
            "LocationID": lid, "DisplayName": name, "Continent": "Central", "Region": "Fittoa", "Biome": "RuralFields",
            "Difficulty": {"F": 1, "E": 2, "D": 2, "C": 3}[rank],
            "Description": LEGACY_DESC[lid], "WorldLocation": vec(float(x), float(y), z),
            "SpawnYaw": round(math.degrees(math.atan2(B.y - y, B.x - x)) if (x, y) != (B.x, B.y) else B.yaw, 2),
            "bSpawnPoint": False, "bFastTravel": False, "DiscoveryRadius": round(disc * 100.0),
            "RequiredRank": rank, "MapUV": map_uv(float(x), float(y)),
            "PreviewCamera": preview_camera(h0, float(x), float(y), z, max(disc, 60.0), 0.0),
        })
    return out


LEGACY_DESC = {
    "Buena_Village": "Quest marker for Buena Village as a whole.",
    "Buena_Square": "The small square at the heart of Buena where villagers trade news.",
    "Buena_Greyrat_House": "A two-storey family home on a gentle rise at the edge of Buena.",
    "Buena_Farms": "Wheat and barley fields that patchwork the land around Buena.",
    "Buena_Hill_Tree": "A grassy hilltop near Buena with a single broad tree and a view of the fields.",
    "Buena_Forest_Edge": "Where Buena's pastures meet the first trees of the woods.",
    "Fittoa_River_Ford": "The crossing where the Fittoa lane meets the Fittoa River.",
    "Fittoa_Deep_Forest": "The dense heart of the nearest Fittoa woodland, where wolves den.",
    "Fittoa_Watchtower_Ruins": "A crumbling watchtower on a high point overlooking the Buena countryside.",
    "Fittoa_Road_To_Roa": "The lane between Buena and Roa, halfway along.",
    "Roa_Gate": "A waystone outside the gate of Roa.",
    "Fittoa_Wyrm_Foothills": "The rising foothills of the Red Wyrm Mountains east of Buena.",
}
