#!/usr/bin/env python3
#
# Builds every asset the tech demo loads, from nothing: a long stone hall and a
# small rigged character, plus their textures, the walk cycle and the scene
# manifest. The meshes go through tools/obj-to-meshbin.py, the textures through
# tools/tim_creator.py and the manifest through tools/scenebin_converter.py, so
# the demo exercises the same pipeline a game does. The ANIMBIN and the hall's
# COLBIN are written directly, because the engine's animation and collision
# exporters run inside Blender.
#
#   python3 tech-demo/tools/make_assets.py
#
# Output goes to tech-demo/assets/. Everything is deterministic, so running it
# twice gives byte-identical files.
#
# Units are OBJ/Blender units: 1.0 = 1.0_ws = 2 m. Y points down, like the GTE,
# so the floor is at y = 0 and the ceiling at a negative y.

import math
import os
import re
import struct
import subprocess
import sys
import tempfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
DEMO = os.path.dirname(HERE)
ENGINE_TOOLS = os.path.join(os.path.dirname(DEMO), "tools")
OUT = os.path.join(DEMO, "assets")

HALL_WIDTH = 3.0  # 6 m
HALL_HEIGHT = 1.5  # 3 m
HALL_LENGTH = 24  # 48 m, in whole units so the floor tiles line up
PILLAR_EVERY = 4
PILLAR_SIZE = 0.3
EMBER_GLOW_POS = (-1.0, -0.1, 6.0)  # keep in step with the "embers" emitter
EMBER_GLOW_RADIUS = 1.6
EMBER_GLOW_STEP = 0.75  # faces the glow reaches are split this fine, in units, so it still shows
HALL_SEGMENT = 2  # floor, ceiling and side wall faces are this many units long
HALL_FLOOR_ACROSS = 3  # floor and ceiling faces across the width of the hall
HALL_PIECE = 4  # the hall mesh is split into stretches this many units long, see hall_pieces()

WALK_FRAMES = 40
LIMB_REST_DEGREES = 0


class Obj:
    """Collects one .obj worth of geometry in the extended format obj-to-meshbin.py reads."""

    def __init__(self, texture_size):
        self.size = texture_size
        self.lines = []
        self.verts = 0
        self.uvs = 0
        self.norms = 0
        self.bones_for_verts = []
        self.faces = []

    def vertex(self, p, colour, bone=None):
        self.lines.append("v %.5f %.5f %.5f %d %d %d" % (p[0], p[1], p[2], *colour))
        self.verts += 1
        if bone is not None:
            self.bones_for_verts.append(bone)
        return self.verts

    def uv(self, px, py):
        # px, py are texels from the top left. OBJ V runs bottom up, which is what the renderer
        # flips back, and obj-to-meshbin.py turns a coordinate into int(c * size) - 1, so aim
        # half a texel past it
        v = self.size - 1 - py
        self.lines.append("vt %.6f %.6f" % ((px + 1.5) / self.size, (v + 1.5) / self.size))
        self.uvs += 1
        return self.uvs

    def normal(self, n):
        self.lines.append("vn %.5f %.5f %.5f" % n)
        self.norms += 1
        return self.norms

    def quad(self, corners, normal, cell, colours, bone=None):
        # corners go round the face, cell is (x, y, w, h) in texels
        self.faces.append(corners)
        n = self.normal(normal)
        x, y, w, h = cell
        texels = [(x, y), (x + w - 1, y), (x + w - 1, y + h - 1), (x, y + h - 1)]
        refs = []
        for corner, texel, colour in zip(corners, texels, colours):
            v = self.vertex(corner, colour, bone)
            t = self.uv(*texel)
            refs.append("%d/%d/%d" % (v, t, n))
        self.lines.append("f " + " ".join(refs))

    def write(self, path, header=()):
        with open(path, "w") as f:
            for line in header:
                f.write(line + "\n")
            for line in self.lines:
                f.write(line + "\n")
            for i, bone in enumerate(self.bones_for_verts):
                f.write("vw %d %d\n" % (i + 1, bone))


def grey(level):
    return (level, level, level)


# ---------------------------------------------------------------- textures


def noise(x, y, seed):
    # cheap deterministic hash noise, 0..1
    h = (x * 374761393 + y * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0


def hall_texture():
    # 128x128 atlas, four 64x64 cells: floor, wall, ceiling, pillar
    img = Image.new("RGB", (128, 128))
    px = img.load()
    for y in range(64):
        for x in range(64):
            # floor: four flagstones with dark grout
            grout = (x % 32) < 2 or (y % 32) < 2
            stone = 0.75 + 0.25 * noise(x // 4, y // 4, 1) + 0.1 * noise(x, y, 2)
            c = 40 if grout else int(110 * stone)
            px[x, y] = (c, c - 4, c - 10)

            # wall: running bond bricks
            row = y // 8
            offset = 8 if row % 2 else 0
            mortar = (y % 8) < 1 or ((x + offset) % 16) < 1
            brick = 0.8 + 0.2 * noise((x + offset) // 16, row, 3) + 0.08 * noise(x, y, 4)
            if mortar:
                px[64 + x, y] = (52, 50, 46)
            else:
                px[64 + x, y] = (int(120 * brick), int(70 * brick), int(52 * brick))

            # ceiling: planks
            seam = (x % 16) < 1
            grain = 0.75 + 0.25 * noise(x // 16, y // 2, 5)
            c = 30 if seam else int(90 * grain)
            px[x, 64 + y] = (c, int(c * 0.8), int(c * 0.6))

            # pillar: dressed stone blocks
            joint = (y % 16) < 1
            c = 46 if joint else int(128 * (0.8 + 0.2 * noise(x // 8, y // 16, 6)))
            px[64 + x, 64 + y] = (c, c, c + 6)
    return img


# character atlas cells, 64x64, 16 colours
HERO_CELLS = {
    "skin": (0, 0, 16, 16),
    "face": (16, 0, 16, 16),
    "hair": (32, 0, 16, 16),
    "shirt": (0, 16, 16, 16),
    "shirt_back": (16, 16, 16, 16),
    "trousers": (0, 32, 16, 16),
    "boots": (16, 32, 16, 16),
}


def hero_texture():
    img = Image.new("RGB", (64, 64), (60, 60, 60))
    px = img.load()

    def fill(cell, colour, detail=None):
        x0, y0, w, h = HERO_CELLS[cell]
        for y in range(h):
            for x in range(w):
                c = detail(x, y) if detail else None
                px[x0 + x, y0 + y] = c or colour

    skin = (200, 150, 120)
    fill("skin", skin)
    fill("hair", (70, 44, 30))

    def face(x, y):
        if y < 4:
            return (70, 44, 30)  # fringe
        if y in (7, 8) and x in (4, 5, 10, 11):
            return (30, 30, 40)  # eyes
        if y == 12 and 5 <= x <= 10:
            return (140, 80, 70)  # mouth
        return None

    fill("face", skin, face)

    def shirt(x, y):
        return (230, 200, 60) if 6 <= x <= 9 and y < 3 else None  # collar

    fill("shirt", (40, 90, 150), shirt)
    fill("shirt_back", (36, 80, 136))
    fill("trousers", (60, 56, 50))
    fill("boots", (36, 28, 24))
    return img


# ---------------------------------------------------------------- hall


def hall_mesh():
    # returns the whole hall as one Obj (what the collision is built from) and every quad
    # in it, each tagged with the z it is sorted into a piece by, see hall_pieces()
    o = Obj(128)
    quads = []
    half = HALL_WIDTH / 2
    top = -HALL_HEIGHT
    FLOOR, WALL, CEIL, PILLAR = (0, 0, 64, 64), (64, 0, 64, 64), (0, 64, 64, 64), (64, 64, 64, 64)

    def put(corners, normal, cell, colours, z=None):
        o.quad(corners, normal, cell, colours)
        if z is None:
            z = sum(c[2] for c in corners) / len(corners)
        quads.append((z, corners, normal, cell, colours))

    def shade(c):
        # darker towards the far end and towards the ceiling, so the hall reads as dim
        # even before the fog takes over
        level = max(56, 112 - int(c[2] * 1.5) + int(c[1] * 16))
        # warm glow baked around the ember emitter in hall_scene.cpp
        f = max(0.0, 1.0 - math.dist(c, EMBER_GLOW_POS) / EMBER_GLOW_RADIUS) ** 2
        return (min(255, level + int(120 * f)), min(255, level + int(50 * f)), max(0, level - int(10 * f)))

    def face(corners, normal, cell):
        # one quad, unless the ember glow reaches it: per-vertex colour needs vertices under
        # the glow, so split it into a grid there, each piece taking its share of the cell so
        # the texture still runs unbroken across the whole face
        lo = [min(c[i] for c in corners) for i in range(3)]
        hi = [max(c[i] for c in corners) for i in range(3)]
        nearest = [min(max(EMBER_GLOW_POS[i], lo[i]), hi[i]) for i in range(3)]
        n_u = n_v = 1
        # (past three quarters of the radius the glow is a couple of colour steps, not worth it)
        if math.dist(nearest, EMBER_GLOW_POS) < EMBER_GLOW_RADIUS * 0.75:
            n_u = max(1, round(math.dist(corners[0], corners[1]) / EMBER_GLOW_STEP))
            n_v = max(1, round(math.dist(corners[0], corners[3]) / EMBER_GLOW_STEP))
        c0, c1, c3 = corners[0], corners[1], corners[3]
        x, y, w, h = cell
        whole_z = sum(c[2] for c in corners) / len(corners)  # the pieces stay together

        def at(u, v):
            return tuple(c0[i] + u * (c1[i] - c0[i]) + v * (c3[i] - c0[i]) for i in range(3))

        for j in range(n_v):
            for i in range(n_u):
                u0, u1, v0, v1 = i / n_u, (i + 1) / n_u, j / n_v, (j + 1) / n_v
                piece = [at(u0, v0), at(u1, v0), at(u1, v1), at(u0, v1)]
                tx0, tx1 = x + round(w * u0), x + round(w * u1)
                ty0, ty1 = y + round(h * v0), y + round(h * v1)
                put(piece, normal, (tx0, ty0, tx1 - tx0, ty1 - ty0), [shade(c) for c in piece], whole_z)

    # floor, ceiling and side walls in faces HALL_SEGMENT units long: the engine subdivides
    # big quads near the camera, so the hall does not need small faces. A cell can't repeat
    # inside a quad (it is a corner of the atlas), so the texture stretches over the face
    for zi in range(0, HALL_LENGTH, HALL_SEGMENT):
        z0, z1 = float(zi), float(min(zi + HALL_SEGMENT, HALL_LENGTH))
        for xi in range(HALL_FLOOR_ACROSS):
            x0 = -half + HALL_WIDTH * xi / HALL_FLOOR_ACROSS
            x1 = -half + HALL_WIDTH * (xi + 1) / HALL_FLOOR_ACROSS
            face([(x0, 0, z0), (x1, 0, z0), (x1, 0, z1), (x0, 0, z1)], (0, -1, 0), FLOOR)
            face([(x0, top, z1), (x1, top, z1), (x1, top, z0), (x0, top, z0)], (0, 1, 0), CEIL)

        # side walls, one row of brick per face
        face([(-half, top, z0), (-half, top, z1), (-half, 0, z1), (-half, 0, z0)], (1, 0, 0), WALL)
        face([(half, top, z1), (half, top, z0), (half, 0, z0), (half, 0, z1)], (-1, 0, 0), WALL)

    # end walls
    for z, normal in ((0.0, (0, 0, 1)), (float(HALL_LENGTH), (0, 0, -1))):
        for xi in range(int(HALL_WIDTH)):
            x0, x1 = -half + xi, -half + xi + 1
            for row in range(2):
                y0, y1 = top * row / 2, top * (row + 1) / 2
                if normal[2] > 0:
                    corners = [(x1, y1, z), (x0, y1, z), (x0, y0, z), (x1, y0, z)]
                else:
                    corners = [(x0, y1, z), (x1, y1, z), (x1, y0, z), (x0, y0, z)]
                put(corners, normal, WALL, [shade(c) for c in corners])

    # square pillars against both walls
    p = PILLAR_SIZE
    for zi in range(PILLAR_EVERY, HALL_LENGTH, PILLAR_EVERY):
        z = float(zi)
        for side in (-1, 1):
            xc = side * (half - p / 2)
            x0, x1 = xc - p / 2, xc + p / 2
            z0, z1 = z - p / 2, z + p / 2
            faces = [
                ([(x0, top, z0), (x1, top, z0), (x1, 0, z0), (x0, 0, z0)], (0, 0, -1)),
                ([(x1, top, z1), (x0, top, z1), (x0, 0, z1), (x1, 0, z1)], (0, 0, 1)),
            ]
            # only the face pointing into the hall
            if side < 0:
                faces.append(([(x1, top, z0), (x1, top, z1), (x1, 0, z1), (x1, 0, z0)], (1, 0, 0)))
            else:
                faces.append(([(x0, top, z1), (x0, top, z0), (x0, 0, z0), (x0, 0, z1)], (-1, 0, 0)))
            for corners, normal in faces:
                # a pillar goes into one piece whole, sorted by its middle
                put(corners, normal, PILLAR, [shade(c) for c in corners], z)
    return o, quads


def hall_pieces(quads):
    # The engine lights each object once, from the middle of its mesh's bounding sphere: a
    # light gets one direction and one strength for the whole object. One hall-long mesh
    # would be lit evenly end to end, so the hall is split into HALL_PIECE-unit stretches,
    # and each stretch into its floor, its ceiling and each side (wall plus pillars), so a
    # piece's middle sits on its own surface and a light beside it lights it from the
    # right side. The end walls are a piece each. Every piece costs the renderer a little
    # on its own, so the pillars go in with their wall rather than in pieces of their own
    groups = {}
    for z, corners, normal, cell, colours in quads:
        stretch = min(int(z // HALL_PIECE), HALL_LENGTH // HALL_PIECE - 1)
        if normal[1]:
            side = normal
        elif normal[2] and z in (0, HALL_LENGTH):
            side = "end"
        else:
            side = "left" if sum(c[0] for c in corners) < 0 else "right"
        groups.setdefault((stretch, side), []).append((corners, normal, cell, colours))
    pieces = []
    for key in sorted(groups, key=lambda k: (k[0], str(k[1]))):
        o = Obj(128)
        for corners, normal, cell, colours in groups[key]:
            o.quad(corners, normal, cell, colours)
        pieces.append(o)
    return pieces


# ---------------------------------------------------------------- hall collision

# what tools/blender_colbin.py uses for the hall: 128 engine units per unit, its
# default two unit grid cell and its minimum wall half thickness
COLBIN_SCALE = 128
COLBIN_CELL = 256
COLBIN_MIN_HALF = 32
COLBIN_UP = (0.0, -1.0, 0.0)


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def unit(a):
    length = math.sqrt(dot(a, a))
    return tuple(x / length for x in a)


def hall_colbin(hall):
    # The hall's COLBIN as tools/blender_colbin.py exports it from the scene with walls
    # merged: floor faces become floor triangles, ceilings are dropped, and the wall faces
    # that share an edge on one plane become one box. The hall is all right angles, so
    # each merged wall is the bounding box of its faces.
    up = COLBIN_UP
    floors = []
    planes = {}  # (normal, distance) -> [face corners]
    for corners in hall.faces:
        pts = [tuple(c * COLBIN_SCALE for c in p) for p in corners]
        # the winding normal, which is what the exporter reads, not the OBJ normal
        n = unit(cross(sub(pts[1], pts[0]), sub(pts[2], pts[0])))
        facing = dot(n, up)
        if facing > 0.5:
            fn = n if n[1] >= 0 else tuple(-c for c in n)
            fn = tuple(int(round(c * 4096)) for c in fn)
            for i in range(1, len(pts) - 1):
                floors.append([tuple(int(round(c)) for c in p) for p in (pts[0], pts[i], pts[i + 1])] + [fn])
        elif facing > -0.5:
            key = (tuple(round(c, 6) for c in n), round(dot(n, pts[0]), 3))
            planes.setdefault(key, []).append(pts)

    walls = []
    for (n, _), faces in planes.items():
        u = unit(cross(up, n))
        axes = (u, cross(n, u), n)
        spans = [[(min(dot(a, p) for p in f), max(dot(a, p) for p in f)) for a in axes[:2]] for f in faces]

        # faces on one plane that share an edge, by flood fill
        def touch(a, b):
            over = [min(a[i][1], b[i][1]) - max(a[i][0], b[i][0]) for i in range(2)]
            return min(over) > -1e-6 and max(over) > 1e-6

        left = list(range(len(faces)))
        while left:
            group = [left.pop(0)]
            for i in group:
                for j in [j for j in left if touch(spans[i], spans[j])]:
                    left.remove(j)
                    group.append(j)
            pts = [p for i in group for p in faces[i]]
            centre = [0.0, 0.0, 0.0]
            half = []
            for a in axes:
                lo, hi = min(dot(a, p) for p in pts), max(dot(a, p) for p in pts)
                centre = [c + x * (lo + hi) / 2 for c, x in zip(centre, a)]
                half.append((hi - lo) / 2)
            thin = half.index(min(half))
            half[thin] = max(half[thin], COLBIN_MIN_HALF)
            walls.append((centre, axes, half))

    # the grid over the walls' X/Z footprints, laid out like the exporter's
    bounds = []
    for centre, axes, half in walls:
        rx = sum(abs(a[0]) * h for a, h in zip(axes, half))
        rz = sum(abs(a[2]) * h for a, h in zip(axes, half))
        bounds.append((centre[0] - rx, centre[0] + rx, centre[2] - rz, centre[2] + rz))
    min_x, max_x = min(b[0] for b in bounds), max(b[1] for b in bounds)
    min_z, max_z = min(b[2] for b in bounds), max(b[3] for b in bounds)
    origin_x, origin_z = int(min_x) - COLBIN_CELL, int(min_z) - COLBIN_CELL
    width = math.ceil((max_x - min_x + COLBIN_CELL * 2) / COLBIN_CELL)
    height = math.ceil((max_z - min_z + COLBIN_CELL * 2) / COLBIN_CELL)
    cells = [[] for _ in range(width * height)]

    def span(lo, hi, origin, count):
        first = max(0, int((lo - origin) // COLBIN_CELL))
        return range(first, min(count - 1, int((hi - origin) // COLBIN_CELL)) + 1)

    for i, (x0, x1, z0, z1) in enumerate(bounds):
        for gx in span(x0, x1, origin_x, width):
            for gz in span(z0, z1, origin_z, height):
                cells[gx * height + gz].append(i)

    out = bytearray(b"COLBIN")
    out += struct.pack("<BII", 2, len(floors), len(walls))
    out += struct.pack("<iiIHH", origin_x, origin_z, COLBIN_CELL, width, height)
    for cell in cells:
        out += struct.pack("<H%dH" % len(cell), len(cell), *cell)
    for v0, v1, v2, n in floors:
        out += struct.pack("<9i3hxx", *v0, *v1, *v2, *n)
    for centre, axes, half in walls:
        out += struct.pack("<3i", *(int(round(c)) for c in centre))
        for a in axes:
            out += struct.pack("<3i", *(int(round(c * 4096)) for c in a))
        out += struct.pack("<3iI", *(int(round(h)) for h in half), 0)
    return bytes(out)


# ---------------------------------------------------------------- character

# bone ids
ROOT, LEG_L, LEG_R, ARM_L, ARM_R = range(5)
HIP_Y = -0.45
SHOULDER_Y = -0.74
BONES = [
    # name, parent, world position, rest rotation in degrees about X
    ("root", -1, (0.0, HIP_Y, 0.0), 0),
    ("leg_l", ROOT, (0.075, HIP_Y, 0.0), LIMB_REST_DEGREES),
    ("leg_r", ROOT, (-0.075, HIP_Y, 0.0), LIMB_REST_DEGREES),
    ("arm_l", ROOT, (0.2, SHOULDER_Y, 0.0), LIMB_REST_DEGREES),
    ("arm_r", ROOT, (-0.2, SHOULDER_Y, 0.0), LIMB_REST_DEGREES),
]


def quat_x(degrees):
    half = math.radians(degrees) / 2
    return (math.cos(half), math.sin(half), 0.0, 0.0)


def box(o, mins, maxs, bone, cells):
    (x0, y0, z0), (x1, y1, z1) = mins, maxs
    colour = grey(128)
    sides = {
        "front": ([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0, 0, -1)),
        "back": ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
        "left": ([(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (1, 0, 0)),
        "right": ([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1, 0, 0)),
        "top": ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0)),
        "bottom": ([(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], (0, 1, 0)),
    }
    for name, (corners, normal) in sides.items():
        cell = HERO_CELLS[cells.get(name, cells["default"])]
        o.quad(corners, normal, cell, [colour] * 4, bone)


def hero_mesh():
    o = Obj(64)
    # The character faces +Z, so its front is the z1 side of every box ("back" in box()).
    box(o, (-0.15, -0.78, -0.08), (0.15, -0.43, 0.08), ROOT, {"default": "shirt", "front": "shirt_back"})
    box(o, (-0.09, -0.94, -0.09), (0.09, -0.78, 0.09), ROOT,
        {"default": "skin", "back": "face", "top": "hair", "front": "hair"})
    for bone, xc in ((LEG_L, 0.075), (LEG_R, -0.075)):
        box(o, (xc - 0.055, -0.45, -0.055), (xc + 0.055, -0.06, 0.055), bone, {"default": "trousers"})
        box(o, (xc - 0.06, -0.06, -0.06), (xc + 0.06, 0.0, 0.09), bone, {"default": "boots"})
    for bone, xc in ((ARM_L, 0.2), (ARM_R, -0.2)):
        box(o, (xc - 0.04, -0.76, -0.045), (xc + 0.04, -0.5, 0.045), bone, {"default": "shirt"})
        box(o, (xc - 0.035, -0.5, -0.04), (xc + 0.035, -0.42, 0.04), bone, {"default": "skin"})

    header = ["skel %d" % len(BONES)]
    for i, (name, parent, pos, rest) in enumerate(BONES):
        if parent < 0:
            local = pos
        else:
            # every parent here is the root, which is unrotated, so the local
            # position is just the offset from it
            ppos = BONES[parent][2]
            local = tuple(a - b for a, b in zip(pos, ppos))
        w, x, y, z = quat_x(rest)
        header.append("bone %d %s %d %.5f %.5f %.5f %.6f %.6f %.6f %.6f" % (i, name, parent, *local, w, x, y, z))
    return o, header


def walk_animbin():
    # the limbs swing about X around their rest pose, legs 30 degrees each way, arms 24
    def angles(frame):
        s = math.sin(2 * math.pi * frame / WALK_FRAMES)
        return {
            ROOT: 0,
            LEG_L: LIMB_REST_DEGREES + 30 * s,
            LEG_R: LIMB_REST_DEGREES - 30 * s,
            ARM_L: LIMB_REST_DEGREES - 24 * s,
            ARM_R: LIMB_REST_DEGREES + 24 * s,
        }

    frames = list(range(0, WALK_FRAMES + 1, 5))
    out = bytearray(b"ANIMBIN")
    out += struct.pack("<BB", 1, 1)  # version, one animation
    name = b"walk".ljust(32, b"\0")
    bones = (LEG_L, LEG_R, ARM_L, ARM_R)
    out += name + struct.pack("<IHHH", 1, WALK_FRAMES, len(bones), 0)  # looping
    for bone in bones:
        out += struct.pack("<BBH", 0, bone, len(frames))
        for frame in frames:
            q = quat_x(angles(frame)[bone])
            fixed = [int(round(c * 4096)) for c in q]
            out += struct.pack("<HB4h", frame, 0, *fixed)
    return bytes(out)


MANIFEST = """\
# tech demo scene, converted with tools/scenebin_converter.py
texture TEXTURES/HALL.TIM 320 0 0 240
texture_extra TEXTURES/HERO.TIM 384 0 0 241 1
@HALL_OBJECTS@
object MODELS/HERO.MB
colbin COLBIN/HALL.CB
animation ANIMS/HERO.AB
vag SFX/FCNTNA.VAG
vag SFX/AMBIENT_CREAK.VAG
"""


def check_piece_count(count):
    # hall_scene.cpp creates HALL_PIECES objects and toc.json lists the files for a disc
    # build; both are written by hand, so say so if they no longer match the pieces
    with open(os.path.join(DEMO, "src", "hall_scene.cpp")) as f:
        found = re.search(r"HALL_PIECES = (\d+);", f.read())
    with open(os.path.join(DEMO, "toc.json")) as f:
        toc = f.read()
    missing = [n for n in ("MODELS/HALL%02d.MB" % i for i in range(count)) if '"name": "%s"' % n not in toc]
    if not found or int(found.group(1)) != count or missing:
        sys.exit("the hall is %d pieces: set HALL_PIECES in src/hall_scene.cpp to match and list "
                 "MODELS/HALL00.MB to MODELS/HALL%02d.MB in toc.json" % (count, count - 1))


def run(*args):
    subprocess.run([sys.executable, "-I", *args], check=True, stdout=subprocess.DEVNULL)


def main():
    for d in ("MODELS", "TEXTURES", "ANIMS", "COLBIN"):
        os.makedirs(os.path.join(OUT, d), exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        hall, quads = hall_mesh()
        with open(os.path.join(OUT, "COLBIN", "HALL.CB"), "wb") as f:
            f.write(hall_colbin(hall))
        pieces = hall_pieces(quads)
        check_piece_count(len(pieces))
        # pieces from an earlier split would still land in the PCDRV image
        for old in os.listdir(os.path.join(OUT, "MODELS")):
            if re.fullmatch(r"HALL\d+\.MB", old):
                os.remove(os.path.join(OUT, "MODELS", old))
        hall_names = []
        for i, piece in enumerate(pieces):
            name = "MODELS/HALL%02d.MB" % i
            hall_names.append(name)
            piece.write(os.path.join(tmp, "hall.obj"))
            run(os.path.join(ENGINE_TOOLS, "obj-to-meshbin.py"), os.path.join(tmp, "hall.obj"),
                os.path.join(OUT, name), "128")
        hero, header = hero_mesh()
        hero.write(os.path.join(tmp, "hero.obj"), header)
        run(os.path.join(ENGINE_TOOLS, "obj-to-meshbin.py"), os.path.join(tmp, "hero.obj"),
            os.path.join(OUT, "MODELS", "HERO.MB"), "64")

        hall_texture().save(os.path.join(tmp, "hall.png"))
        hero_texture().save(os.path.join(tmp, "hero.png"))
        run(os.path.join(ENGINE_TOOLS, "tim_creator.py"), os.path.join(tmp, "hall.png"), "-o",
            os.path.join(OUT, "TEXTURES", "HALL.TIM"), "-x", "320", "-y", "0", "-c", "0", "-d", "240",
            "--quantize", "256")
        run(os.path.join(ENGINE_TOOLS, "tim_creator.py"), os.path.join(tmp, "hero.png"), "-o",
            os.path.join(OUT, "TEXTURES", "HERO.TIM"), "-x", "384", "-y", "0", "-c", "0", "-d", "241",
            "--quantize", "16")

        with open(os.path.join(OUT, "ANIMS", "HERO.AB"), "wb") as f:
            f.write(walk_animbin())

        manifest = os.path.join(tmp, "scene.txt")
        with open(manifest, "w") as f:
            f.write(MANIFEST.replace("@HALL_OBJECTS@", "\n".join("object " + n for n in hall_names)))
        run(os.path.join(ENGINE_TOOLS, "scenebin_converter.py"), manifest, os.path.join(OUT, "DEMO.SCN"),
            "--asset-root", OUT)

    print("assets written to %s" % OUT)


if __name__ == "__main__":
    main()
