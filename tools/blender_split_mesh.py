bl_info = {
    "name": "Split Meshes for Madnight (lighting and chunking)",
    "author": "",
    "version": (1, 0),
    "blender": (4, 0, 0),
    "location": "Object > Split Meshes for Madnight",
    "description": "Split level meshes into square chunks, and each chunk by which way its faces point",
    "category": "Object",
}

# The engine lights each object once, from the middle of its bounding sphere, so one big level
# mesh gets one light direction and strength for the whole thing. This splits meshes into square
# chunks on the ground plane (Blender X and Y), and each chunk into its floor, its ceiling and its
# walls, so a piece's middle sits on its own surface. Pieces also stay under the mesh manager's
# face limit, which makes this a cheap way to chunk a level.
#
# In Blender: select the meshes to split (or nothing, for every mesh in the scene) and run
# Object > Split Meshes for Madnight. Each object's pieces go into a collection named
# <object>_split, and the object itself is hidden. Running it again replaces that collection.
#
# Without the UI:
#   blender -b level.blend --python tools/blender_split_mesh.py -- [--cell 4] [--no-facing]
#       [--objects Hall,Room2] [--save level_split.blend]

import bpy
import bmesh
import math
import os
import re
import sys
from mathutils import Vector
from bpy.props import BoolProperty, FloatProperty

# fallbacks for when src/mesh/mesh_manager.hh is not next to this script
DEFAULT_MAX_FACES_PER_MESH = 1000
DEFAULT_MAX_LOADED_MESHES = 250
SPARE_MESH_SLOTS = 10  # warn when a split leaves fewer than this many mesh slots for everything else

# obj-to-meshbin.py sends a quad as two triangles when it is folded more than this
MAX_QUAD_FOLD_DEGREES = 30.0

# a face counts as floor or ceiling when its normal is within ~45 degrees of straight up or down
FLAT_NORMAL_Z = 0.7
# wall faces pointing the same way are kept together when they are this close along their normal,
# as a fraction of the cell size, so a pillar's front stays with the wall it stands against
WALL_GAP = 0.25
# a wall group with less than this share of the biggest wall group's area in its chunk (pillar
# sides, trim) joins whichever bigger group in the chunk is closest
SMALL_GROUP_AREA = 0.25


def engine_limits():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "mesh", "mesh_manager.hh")
    limits = {"MAX_FACES_PER_MESH": DEFAULT_MAX_FACES_PER_MESH, "MAX_LOADED_MESHES": DEFAULT_MAX_LOADED_MESHES}
    try:
        with open(path) as f:
            text = f.read()
    except OSError:
        return limits["MAX_FACES_PER_MESH"], limits["MAX_LOADED_MESHES"], False
    for name in limits:
        m = re.search(r"\b%s\s*=\s*(\d+)" % name, text)
        if m:
            limits[name] = int(m.group(1))
    return limits["MAX_FACES_PER_MESH"], limits["MAX_LOADED_MESHES"], True


def meshbin_face_count(face, matrix):
    # the number of entries obj-to-meshbin.py writes for this face
    n = len(face.verts)
    if n < 4:
        return 1
    if n > 4:
        return n - 2
    p = [matrix @ v.co for v in face.verts]
    best = None
    for a in range(2):
        b, c, d = (a + 1) % 4, (a + 2) % 4, (a + 3) % 4
        first = (p[b] - p[a]).cross(p[d] - p[a])
        second = (p[c] - p[b]).cross(p[d] - p[b])
        if first.length == 0 or second.length == 0:
            continue
        fold = math.degrees(first.angle(second))
        best = fold if best is None else min(best, fold)
    return 1 if best is not None and best <= MAX_QUAD_FOLD_DEGREES else 2


class Face:
    __slots__ = ("index", "centre", "normal", "area", "count")

    def __init__(self, face, matrix, normal_matrix, front):
        self.index = face.index
        self.centre = matrix @ face.calc_center_median()
        # obj-to-meshbin.py takes a face's front from its normals, not its winding
        n = face.normal if front is None or front.length == 0 else front
        n = normal_matrix @ n
        self.normal = n.normalized() if n.length else Vector((0, 0, 1))
        self.area = face.calc_area()
        self.count = meshbin_face_count(face, matrix)


def facing_of(f):
    if f.normal.z >= FLAT_NORMAL_Z:
        return "floor", None
    if f.normal.z <= -FLAT_NORMAL_Z:
        return "ceiling", None
    axis = 0 if abs(f.normal.x) >= abs(f.normal.y) else 1
    return "wall", (axis, 1 if f.normal[axis] >= 0 else -1)


def centre_of(faces):
    total = sum(f.area for f in faces) or 1.0
    return sum((f.centre * f.area for f in faces), Vector()) / total


def wall_groups(faces, gap):
    # faces pointing the same compass way, clustered along that axis
    by_direction = {}
    for f in faces:
        by_direction.setdefault(facing_of(f)[1], []).append(f)
    groups = []
    for (axis, sign), members in sorted(by_direction.items()):
        members.sort(key=lambda f: f.centre[axis])
        current = [members[0]]
        for f in members[1:]:
            if f.centre[axis] - current[-1].centre[axis] > gap:
                groups.append(((axis, sign), current))
                current = []
            current.append(f)
        groups.append(((axis, sign), current))
    if len(groups) < 2:
        return groups
    biggest = max(sum(f.area for f in g) for _, g in groups)
    keep = [g for g in groups if sum(f.area for f in g[1]) >= biggest * SMALL_GROUP_AREA]
    centres = [centre_of(g) for _, g in keep]
    for direction, g in groups:
        if any(g is k[1] for k in keep):
            continue
        # one face at a time: a small group can hold pillar sides from both ends of the chunk
        for f in g:
            i = min(range(len(keep)), key=lambda i: (centres[i] - f.centre).length)
            keep[i][1].append(f)
    return keep


def split_to_fit(faces, max_faces):
    # halve along the longer ground axis until every part is under the face limit
    if sum(f.count for f in faces) < max_faces or len(faces) < 2:
        return [faces]
    xs = [f.centre.x for f in faces]
    ys = [f.centre.y for f in faces]
    axis = 0 if max(xs) - min(xs) >= max(ys) - min(ys) else 1
    faces = sorted(faces, key=lambda f: f.centre[axis])
    half = len(faces) // 2
    return split_to_fit(faces[:half], max_faces) + split_to_fit(faces[half:], max_faces)


def auto_cell_size(objects, depsgraph):
    lo, hi = ground_bounds(objects, depsgraph)
    width, depth = hi[0] - lo[0], hi[1] - lo[1]
    # about six chunks along the longest side, and no more than two across the shortest
    return max(max(width, depth) / 6.0, min(width, depth) / 2.0, 0.001)


def ground_bounds(objects, depsgraph):
    lo, hi = [math.inf, math.inf], [-math.inf, -math.inf]
    for obj in objects:
        ev = obj.evaluated_get(depsgraph)
        for corner in ev.bound_box:
            p = ev.matrix_world @ Vector(corner)
            for i in range(2):
                lo[i] = min(lo[i], p[i])
                hi[i] = max(hi[i], p[i])
    return lo, hi


def plan_pieces(obj, depsgraph, cell, origin, split_facing, max_faces):
    ev = obj.evaluated_get(depsgraph)
    bm = bmesh.new()
    bm.from_object(obj, depsgraph)
    bm.faces.ensure_lookup_table()
    matrix = ev.matrix_world
    normal_matrix = matrix.to_3x3().inverted_safe().transposed()
    mesh = ev.data
    corner = mesh.corner_normals if len(mesh.corner_normals) == len(mesh.loops) else None
    fronts = [sum((corner[i].vector for i in p.loop_indices), Vector()) if corner else None
              for p in mesh.polygons] if len(mesh.polygons) == len(bm.faces) else [None] * len(bm.faces)
    faces = [Face(f, matrix, normal_matrix, fronts[f.index]) for f in bm.faces]
    bm.free()

    lo, hi = origin
    counts = [max(1, math.ceil((hi[i] - lo[i]) / cell[i] - 1e-6)) if cell[i] > 0 else 1 for i in range(2)]
    cells = {}
    for f in faces:
        # a face on the far edge of the bounds goes in the last chunk, not one of its own
        key = tuple(min(int(math.floor((f.centre[i] - lo[i]) / cell[i])), counts[i] - 1) if cell[i] > 0 else 0
                    for i in range(2))
        cells.setdefault(key, []).append(f)

    pieces = []
    for key in sorted(cells):
        groups = []
        if split_facing:
            flat = {"floor": [], "ceiling": [], "wall": []}
            for f in cells[key]:
                flat[facing_of(f)[0]].append(f)
            groups += [(name, flat[name]) for name in ("floor", "ceiling") if flat[name]]
            if flat["wall"]:
                gap = WALL_GAP * max(c for c in cell if c > 0) if any(c > 0 for c in cell) else math.inf
                for i, (direction, g) in enumerate(wall_groups(flat["wall"], gap)):
                    axis, sign = direction
                    groups.append(("wall%s%s%d" % ("+" if sign > 0 else "-", "XY"[axis], i), g))
        else:
            groups.append(("all", cells[key]))
        for name, g in groups:
            parts = split_to_fit(g, max_faces)
            for j, part in enumerate(parts):
                suffix = "" if len(parts) == 1 else "_%d" % j
                pieces.append(("%s_%02d_%02d_%s%s" % (obj.name, key[0], key[1], name, suffix), part))
    return pieces


def build_piece(obj, depsgraph, name, face_indices, collection):
    bm = bmesh.new()
    bm.from_object(obj, depsgraph)
    bm.faces.ensure_lookup_table()
    keep = set(face_indices)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in keep], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for mat in obj.data.materials:
        mesh.materials.append(mat)
    piece = bpy.data.objects.new(name, mesh)
    piece.matrix_world = obj.matrix_world.copy()
    collection.objects.link(piece)
    return piece


def split_objects(context, objects, cell_size=0.0, cell_xy=None, split_facing=True, log=print):
    max_faces, max_meshes, found = engine_limits()
    if not found:
        log("mesh_manager.hh not found next to this script, using MAX_FACES_PER_MESH %d and "
            "MAX_LOADED_MESHES %d" % (max_faces, max_meshes))
    depsgraph = context.evaluated_depsgraph_get()
    skipped = [o for o in objects if any(m.type == "ARMATURE" for m in o.modifiers)]
    for o in skipped:
        log("skipping %s: it is skinned, and splitting would cut its bones apart" % o.name)
    objects = [o for o in objects if o not in skipped]
    if not objects:
        return []

    if cell_xy:
        cell = list(cell_xy)
    else:
        size = cell_size if cell_size > 0 else auto_cell_size(objects, depsgraph)
        cell = [size, size]
    origin = ground_bounds(objects, depsgraph)
    log("chunk size %s" % " x ".join("%.3g" % c if c > 0 else "unsplit" for c in cell))

    # a face may be counted as two in the meshbin, so a piece must stay under the limit,
    # and the mesh manager refuses a mesh with MAX_FACES_PER_MESH faces or more
    plans = [(o, plan_pieces(o, depsgraph, cell, origin, split_facing, max_faces)) for o in objects]

    made = []
    for obj, pieces in plans:
        name = obj.name + "_split"
        old = bpy.data.collections.get(name)
        if old:
            for child in list(old.objects):
                mesh = child.data
                bpy.data.objects.remove(child)
                if mesh and mesh.users == 0:
                    bpy.data.meshes.remove(mesh)
            bpy.data.collections.remove(old)
        collection = bpy.data.collections.new(name)
        context.scene.collection.children.link(collection)
        for piece_name, faces in pieces:
            made.append((build_piece(obj, depsgraph, piece_name, [f.index for f in faces], collection),
                         sum(f.count for f in faces)))
        obj.hide_set(True)
        obj.hide_render = True
        log("%s: %d pieces" % (obj.name, len(pieces)))

    biggest = max(made, key=lambda m: m[1])
    log("largest piece %s, %d meshbin faces (limit %d)" % (biggest[0].name, biggest[1], max_faces - 1))
    for piece, count in made:
        if count >= max_faces:
            log("ERROR: %s has %d faces and the mesh manager refuses anything over %d, so it will not "
                "load at all. Its faces are packed too tight to split further: thin out that part of the "
                "mesh" % (piece.name, count, max_faces - 1))
    left = max_meshes - len(made)
    if left >= 0:
        log("%d pieces, %d mesh slots left of %d" % (len(made), left, max_meshes))
    else:
        log("%d pieces, %d more than the %d mesh slots" % (len(made), -left, max_meshes))
    if left < 0:
        log("ERROR: the mesh manager holds %d meshes and this split makes %d. They cannot all be loaded, "
            "even with nothing else in the game: use a bigger chunk size or split fewer meshes"
            % (max_meshes, len(made)))
    elif left < SPARE_MESH_SLOTS:
        log("WARNING: this leaves %d mesh slots for the rest of the game's objects; use a bigger chunk "
            "size or split fewer meshes" % left)
    return made


def source_of(obj):
    # a piece stands for the object it was split from, which is hidden after a split
    for c in obj.users_collection:
        if c.name.endswith("_split"):
            return bpy.data.objects.get(c.name[:-len("_split")])
    return obj


def meshes_in_scope(context):
    selected = []
    for o in context.selected_objects:
        o = source_of(o) if o.type == "MESH" else None
        if o and o.type == "MESH" and o not in selected:
            selected.append(o)
    if selected:
        return selected
    already = {c.name[:-len("_split")] for c in bpy.data.collections if c.name.endswith("_split")}
    return [o for o in context.scene.objects if o.type == "MESH" and source_of(o) is o
            and (o.visible_get() or o.name in already)]


class OBJECT_OT_madnight_split_mesh(bpy.types.Operator):
    """Split the selected meshes (or every mesh in the scene) into square chunks for the engine"""
    bl_idname = "object.madnight_split_mesh"
    bl_label = "Split Meshes for Madnight"
    bl_options = {"REGISTER", "UNDO"}

    cell_size: FloatProperty(
        name="Chunk Size",
        description="Width of each square chunk in Blender units. 0 picks one from the size of the meshes",
        default=0.0, min=0.0)
    split_facing: BoolProperty(
        name="Split by Facing",
        description="Split each chunk into its floor, its ceiling and its walls, so each piece is lit from its own side",
        default=True)
    advanced: BoolProperty(name="Advanced", description="Set the chunk size per axis", default=False)
    cell_x: FloatProperty(name="Chunk X", description="Chunk size along X, 0 for no split along X", default=4.0, min=0.0)
    cell_y: FloatProperty(name="Chunk Y", description="Chunk size along Y, 0 for no split along Y", default=4.0, min=0.0)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "split_facing")
        layout.prop(self, "advanced")
        if self.advanced:
            layout.prop(self, "cell_x")
            layout.prop(self, "cell_y")
        else:
            layout.prop(self, "cell_size")

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        objects = meshes_in_scope(context)
        if not objects:
            self.report({"WARNING"}, "No meshes to split")
            return {"CANCELLED"}
        lines = []
        made = split_objects(context, objects, self.cell_size,
                             (self.cell_x, self.cell_y) if self.advanced else None, self.split_facing,
                             log=lambda s: (print(s), lines.append(s)))
        for line in lines:
            if line.startswith("ERROR: "):
                self.report({"ERROR"}, line[len("ERROR: "):])
            elif line.startswith("WARNING: "):
                self.report({"WARNING"}, line[len("WARNING: "):])
            else:
                self.report({"INFO"}, line)
        return {"FINISHED"} if made else {"CANCELLED"}


def menu_func(self, context):
    self.layout.operator(OBJECT_OT_madnight_split_mesh.bl_idname)


def register():
    bpy.utils.register_class(OBJECT_OT_madnight_split_mesh)
    bpy.types.VIEW3D_MT_object.append(menu_func)


def unregister():
    bpy.types.VIEW3D_MT_object.remove(menu_func)
    bpy.utils.unregister_class(OBJECT_OT_madnight_split_mesh)


def main_background(argv):
    import argparse
    parser = argparse.ArgumentParser(prog="blender -b file.blend --python blender_split_mesh.py --")
    parser.add_argument("--cell", type=float, default=0.0, help="chunk size, 0 picks one")
    parser.add_argument("--no-facing", action="store_true", help="do not split chunks by facing")
    parser.add_argument("--objects", help="comma separated object names, default every mesh in the scene")
    parser.add_argument("--save", help="write the result to this .blend")
    args = parser.parse_args(argv)
    context = bpy.context
    if args.objects:
        objects = [bpy.data.objects[n] for n in args.objects.split(",")]
    else:
        objects = meshes_in_scope(context)
    errors = []
    split_objects(context, objects, args.cell, None, not args.no_facing,
                  log=lambda s: (print(s), s.startswith("ERROR: ") and errors.append(s)))
    if args.save:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(args.save))
    if errors:
        sys.exit(1)


if __name__ == "__main__":
    if bpy.app.background:
        main_background(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    else:
        register()
