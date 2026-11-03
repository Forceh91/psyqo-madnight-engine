#!/usr/bin/env python3
#
# Builds every asset the tech demo loads, from nothing: a long stone hall and a
# small rigged character, plus their textures, the walk cycle and the scene
# manifest. The meshes go through tools/obj-to-meshbin.py, the textures through
# tools/tim_creator.py and the manifest through tools/scenebin_converter.py, so
# the demo exercises the same pipeline a game does. Only the ANIMBIN is written
# directly, because the engine's animation exporter runs inside Blender.
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

WALK_FRAMES = 40
LIMB_REST_DEGREES = 90


class Obj:
    """Collects one .obj worth of geometry in the extended format obj-to-meshbin.py reads."""

    def __init__(self, texture_size):
        self.size = texture_size
        self.lines = []
        self.verts = 0
        self.uvs = 0
        self.norms = 0
        self.bones_for_verts = []

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
    o = Obj(128)
    half = HALL_WIDTH / 2
    top = -HALL_HEIGHT
    FLOOR, WALL, CEIL, PILLAR = (0, 0, 64, 64), (64, 0, 64, 64), (0, 64, 64, 64), (64, 64, 64, 64)

    def shade(z, y):
        # darker towards the far end and towards the ceiling, so the hall reads as dim
        # even before the fog takes over
        level = 112 - int(z * 1.5) + int(y * 16)
        return grey(max(56, level))

    # floor and ceiling, one quad per 1x1 unit so every tile gets its own texture
    for zi in range(HALL_LENGTH):
        z0, z1 = float(zi), float(zi + 1)
        for xi in range(int(HALL_WIDTH)):
            x0, x1 = -half + xi, -half + xi + 1
            floor = [(x0, 0, z0), (x1, 0, z0), (x1, 0, z1), (x0, 0, z1)]
            o.quad(floor, (0, -1, 0), FLOOR, [shade(c[2], c[1]) for c in floor])
            ceil = [(x0, top, z1), (x1, top, z1), (x1, top, z0), (x0, top, z0)]
            o.quad(ceil, (0, 1, 0), CEIL, [shade(c[2], c[1]) for c in ceil])

        # side walls, two rows of brick per unit of length
        for row in range(2):
            y0, y1 = top * row / 2, top * (row + 1) / 2
            left = [(-half, y1, z0), (-half, y1, z1), (-half, y0, z1), (-half, y0, z0)]
            o.quad(left, (1, 0, 0), WALL, [shade(c[2], c[1]) for c in left])
            right = [(half, y1, z1), (half, y1, z0), (half, y0, z0), (half, y0, z1)]
            o.quad(right, (-1, 0, 0), WALL, [shade(c[2], c[1]) for c in right])

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
                o.quad(corners, normal, WALL, [shade(c[2], c[1]) for c in corners])

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
                o.quad(corners, normal, PILLAR, [shade(c[2], c[1]) for c in corners])
    return o


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
    # the character faces -Z, so its front is the z0 side of every box
    box(o, (-0.15, -0.78, -0.08), (0.15, -0.43, 0.08), ROOT, {"default": "shirt", "back": "shirt_back"})
    box(o, (-0.09, -0.94, -0.09), (0.09, -0.78, 0.09), ROOT,
        {"default": "skin", "front": "face", "top": "hair", "back": "hair"})
    for bone, xc in ((LEG_L, 0.075), (LEG_R, -0.075)):
        box(o, (xc - 0.055, -0.45, -0.055), (xc + 0.055, -0.06, 0.055), bone, {"default": "trousers"})
        box(o, (xc - 0.06, -0.06, -0.09), (xc + 0.06, 0.0, 0.06), bone, {"default": "boots"})
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
    # Limb rest rotation is 90 degrees about X and the swing stays within 90 +- 30,
    # so every quaternion component stays positive. A negative component loses its
    # sign on load today (issue #101), which is why the rig is built this way.
    def angles(frame):
        s = math.sin(2 * math.pi * frame / WALK_FRAMES)
        return {
            LEG_L: LIMB_REST_DEGREES + 30 * s,
            LEG_R: LIMB_REST_DEGREES - 30 * s,
            ARM_L: LIMB_REST_DEGREES - 24 * s,
            ARM_R: LIMB_REST_DEGREES + 24 * s,
        }

    frames = list(range(0, WALK_FRAMES + 1, 5))
    out = bytearray(b"ANIMBIN")
    out += struct.pack("<BB", 1, 1)  # version, one animation
    name = b"walk".ljust(32, b"\0")
    out += name + struct.pack("<IHHH", 1, WALK_FRAMES, 4, 0)  # looping
    for bone in (LEG_L, LEG_R, ARM_L, ARM_R):
        out += struct.pack("<BBH", 0, bone, len(frames))
        for frame in frames:
            q = quat_x(angles(frame)[bone])
            fixed = [int(round(c * 4096)) for c in q]
            assert min(fixed) >= 0, "a negative component would hit issue #101"
            out += struct.pack("<HB4h", frame, 0, *fixed)
    return bytes(out)


MANIFEST = """\
# tech demo scene, converted with tools/scenebin_converter.py
texture TEXTURES/HALL.TIM 320 0 0 240
texture_extra TEXTURES/HERO.TIM 384 0 0 241 1
object MODELS/HALL.MB
object MODELS/HERO.MB
animation ANIMS/HERO.AB
"""


# MeshManager::LoadMesh does not step over the bounding sphere radius, so it reads the bones
# of a skinned version 3 mesh 4 bytes early (issue #108). Until that is fixed the radius is
# left out of HERO.MB, which lines the bones up again and leaves the reader taking the
# root bone's first 4 bytes as a (generous) radius. Set this to False once #108 is fixed.
WORK_AROUND_108 = True


def drop_bsphere_radius(path):
    with open(path, "rb") as f:
        data = bytearray(f.read())
    assert data[:7] == b"MESHBIN" and data[7] == 3 and data[0x1D] == 1, "expected a skinned version 3 mesh"
    vertices, indices, _, normals, uvs = struct.unpack_from("<5I", data, 9)
    radius = 0x1F + 15 * vertices + 24 * indices + 6 * normals + 2 * uvs + 12 + 6
    del data[radius:radius + 4]
    with open(path, "wb") as f:
        f.write(data)


def run(*args):
    subprocess.run([sys.executable, "-I", *args], check=True, stdout=subprocess.DEVNULL)


def main():
    for d in ("MODELS", "TEXTURES", "ANIMS"):
        os.makedirs(os.path.join(OUT, d), exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        hall_mesh().write(os.path.join(tmp, "hall.obj"))
        hero, header = hero_mesh()
        hero.write(os.path.join(tmp, "hero.obj"), header)
        run(os.path.join(ENGINE_TOOLS, "obj-to-meshbin.py"), os.path.join(tmp, "hall.obj"),
            os.path.join(OUT, "MODELS", "HALL.MB"), "128")
        run(os.path.join(ENGINE_TOOLS, "obj-to-meshbin.py"), os.path.join(tmp, "hero.obj"),
            os.path.join(OUT, "MODELS", "HERO.MB"), "64")
        if WORK_AROUND_108:
            drop_bsphere_radius(os.path.join(OUT, "MODELS", "HERO.MB"))

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
            f.write(MANIFEST)
        run(os.path.join(ENGINE_TOOLS, "scenebin_converter.py"), manifest, os.path.join(OUT, "DEMO.SCN"),
            "--asset-root", OUT)

    print("assets written to %s" % OUT)


if __name__ == "__main__":
    main()
