"""Registry of architecture / VFX kit assets. Asset modules decorate builder functions with @asset(...)."""

REG = {}
ORDER = []

BUDGETS = {
    "building": (1000, 6000),
    "landmark": (5000, 40000),
    "wall": (300, 8000),
    "prop": (20, 4000),
    "bridge": (200, 6000),
    "vfx": (8, 3000),
}


class AssetDef:
    def __init__(self, name, category, style, fn, kind="building", pivot="base", foundation=0.56, recentre=True,
                 budget=None, notes="", view=None, ground=True):
        self.name = name
        self.category = category
        self.style = style
        self.fn = fn
        self.kind = kind
        self.pivot = pivot
        self.foundation = foundation
        self.recentre = recentre
        self.budget = budget or BUDGETS.get(kind, (0, 40000))
        self.notes = notes
        self.view = view
        self.ground = ground


def asset(name, category, style, **kw):
    def deco(fn):
        if name in REG:
            raise ValueError(f"duplicate asset {name}")
        REG[name] = AssetDef(name, category, style, fn, **kw)
        ORDER.append(name)
        return fn
    return deco
