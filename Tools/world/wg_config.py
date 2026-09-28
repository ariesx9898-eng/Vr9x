"""LA PLACE world generator - shared constants (Docs/LaPlace/Spec.md sections 2, 3, 5).

Every distance in the generator is in metres unless a name says otherwise; UE outputs are converted to centimetres
only when JSON / heightmaps are written.
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_WORLD = os.path.join(ROOT, "SourceArt", "World")
OUT_LAYERS = os.path.join(OUT_WORLD, "Layers")
OUT_DENSITY = os.path.join(OUT_WORLD, "Density")
OUT_DATA = os.path.join(ROOT, "Content", "Data")
OUT_DOC_IMG = os.path.join(ROOT, "Docs", "Images")

SEED = 20260928

# ---------------------------------------------------------------- landscape (Spec section 2)
FULL_W, FULL_H = 6097, 4573          # landscape vertices (X columns, Y rows)
QUAD_M = 3.0                          # 300 cm quads
WORLD_W_M = (FULL_W - 1) * QUAD_M     # 18 288 m
WORLD_H_M = (FULL_H - 1) * QUAD_M     # 13 716 m
LANDSCAPE_MIN_X_CM = -914400.0
LANDSCAPE_MIN_Y_CM = -685800.0
LANDSCAPE_Z_CM = 72400.0
LANDSCAPE_SCALE_Z = 400.0
H_SEA = 9600                          # uint16 value at Z = 0
Z_MIN_M = -300.0                      # h = 0
Z_MAX_M = (65535 - 32768) * 400.0 / 128.0 / 100.0 + 724.0   # 1747.97 m at h = 65535 (spec rounds to ~1744)

# working grids (all cover exactly the same world rectangle, vertex-aligned with the full grid)
L1_STEP = 3                           # 9 m design / hydrology grid
L2_STEP = 6                           # 18 m stream-power erosion grid
REGION_W, REGION_H = 1524, 1143       # Regions.png / Density/*.png pixel grid (12 m pixels)
WORLDMAP_W, WORLDMAP_H = 4096, 3072

LAYERS = ["Grass", "Farmland", "ForestFloor", "Moss", "Snow", "Sand", "Desert", "DemonSoil", "Rock", "Road", "Mud"]
DENSITY_KINDS = ["Trees", "Bushes", "Grass", "Rocks", "Flowers"]

# ---------------------------------------------------------------- regions (Spec section 3)
# id: (key, display name, continent, biome)
REGIONS = {
    0: ("Ocean", "Ringus Sea", "", "Ocean"),
    1: ("Asura", "Asura Kingdom", "Central", "TemperatePlains"),
    2: ("Fittoa", "Fittoa Region", "Central", "RuralFields"),
    3: ("RedWyrm", "Red Wyrm Mountains", "Central", "Alpine"),
    4: ("North", "Northern Territories", "Central", "Snow"),
    5: ("Strife", "Strife Zone", "Central", "DryHills"),
    6: ("SouthCentral", "Southern Central", "Central", "Jungle"),
    7: ("Heaven", "Heaven Continent", "Heaven", "HighPlateau"),
    8: ("Demon", "Demon Continent", "Demon", "Badlands"),
    9: ("RikarisuCrater", "Rikarisu Crater", "Demon", "Crater"),
    10: ("GreatForest", "Great Forest", "Millis", "GiantForest"),
    11: ("MillisLowlands", "Millis Lowlands", "Millis", "GreenHills"),
    12: ("BlueWyrm", "Blue Wyrm Mountains", "Millis", "Alpine"),
    13: ("BegarittDesert", "Begaritt Desert", "Begaritt", "Desert"),
    14: ("BegarittBadlands", "Begaritt Badlands", "Begaritt", "Badlands"),
    15: ("Islands", "Islands", "", "Islands"),
}
N_REGIONS = 16


def uv_to_cm(u, v):
    """Spec map-to-world transform (u, v in 0..1 across the Japanese reference map)."""
    return (u - 0.5) * 1828800.0, (v - 0.5) * 1371600.0


def uv_to_m(u, v):
    """uv -> metres from the landscape min corner (x east, y south)."""
    return u * WORLD_W_M, v * WORLD_H_M


def m_to_cm(x, y):
    return x * 100.0 + LANDSCAPE_MIN_X_CM, y * 100.0 + LANDSCAPE_MIN_Y_CM


def z_to_h(z_m):
    """metres above sea -> uint16 heightmap value (float, caller rounds/clips)."""
    return (z_m * 100.0 - LANDSCAPE_Z_CM) * 128.0 / LANDSCAPE_SCALE_Z + 32768.0


def h_to_z(h):
    return (LANDSCAPE_Z_CM + (h - 32768.0) * LANDSCAPE_SCALE_Z / 128.0) / 100.0
