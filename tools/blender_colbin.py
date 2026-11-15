bl_info = {
    "name": "Export Collision Binary (.colbin)",
    "author": "",
    "version": (2, 1),
    "blender": (3, 0, 0),
    "location": "File > Export > Collision Binary (.colbin)",
    "description": "Export collision (COL collection, selection or scene meshes) to PS1 collision binary format",
    "category": "Import-Export",
}

import bpy
import bmesh
import struct
import math
import time
import mathutils
from mathutils import Vector
from bpy_extras.io_utils import ExportHelper, axis_conversion
from bpy.props import StringProperty, FloatProperty, EnumProperty, BoolProperty

COLLECTION_NAME = "COL"
IGNORE_COLLECTION_NAME = "NOCOL"
IGNORE_PROP = "colbin_ignore"
MODE_PROP = "colbin_mode"
MIN_WALL_THICKNESS = 32

# tolerances for inferred walls, in engine units (128 = one blender unit at the default scale)
PLANE_COS_TOLERANCE = 0.9995  # faces within ~1.8 degrees of each other can share a plane
PLANE_DISTANCE_TOLERANCE = 2.0
EDGE_TOLERANCE = 1.0
MIN_FACE_AREA = 1.0  # anything smaller is a sliver left over from a cut, not a wall

AXIS_ITEMS = (
    ('X',  'X',  ''),
    ('Y',  'Y',  ''),
    ('Z',  'Z',  ''),
    ('-X', '-X', ''),
    ('-Y', '-Y', ''),
    ('-Z', '-Z', ''),
)

SOURCE_ITEMS = (
    ('COL', 'COL Collection', 'Hand built collision in a collection named COL'),
    ('SELECTED', 'Selected Objects', 'Infer collision from the selected mesh objects'),
    ('SCENE', 'Scene', 'Infer collision from every visible mesh, plus anything in COL'),
)


def find_collection(name):
    return next((c for c in bpy.data.collections if c.name.upper() == name.upper()), None)


def axis_to_fixed(v):
    return (int(round(v.x * 4096)), int(round(v.y * 4096)), int(round(v.z * 4096)))


def vec_to_int(v):
    return (int(round(v.x)), int(round(v.y)), int(round(v.z)))


def write_vec3_int32(f, x, y, z):
    f.write(struct.pack('<iii', x, y, z))


def write_vec3_int16(f, x, y, z):
    f.write(struct.pack('<hhh', x, y, z))


def evaluated_bmesh(obj, depsgraph, global_matrix):
    # modifiers applied, then moved into engine space, so every length below is in engine units
    obj_eval = obj.evaluated_get(depsgraph)
    bm = bmesh.new()
    bm.from_mesh(obj_eval.to_mesh())
    obj_eval.to_mesh_clear()

    matrix = global_matrix @ obj_eval.matrix_world
    bm.transform(matrix)
    if matrix.to_3x3().determinant() < 0:
        # a mirrored object turns its faces inside out, put them back so volumes stay positive
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    bm.normal_update()
    bm.faces.ensure_lookup_table()
    return bm, obj_eval.matrix_world


def make_obb(points, axes):
    # box over the points in the given basis. the centre is the middle of the extents, not the
    # mean of the points, which would pull it towards whichever side has more vertices
    center = Vector((0, 0, 0))
    half_extents = []
    for axis in axes:
        projs = [p.dot(axis) for p in points]
        lo, hi = min(projs), max(projs)
        center += axis * ((lo + hi) / 2.0)
        half_extents.append((hi - lo) / 2.0)

    # clamp thinnest axis to minimum wall thickness
    min_idx = half_extents.index(min(half_extents))
    half_extents[min_idx] = max(half_extents[min_idx], MIN_WALL_THICKNESS)
    return (center, list(axes), half_extents)


def obb_volume(points, axes):
    vol = 1.0
    for axis in axes:
        projs = [p.dot(axis) for p in points]
        vol *= max(projs) - min(projs)
    return vol


def floor_normal(normal):
    # floors keep the convention the exporter has always written: normal flipped to +Y
    n = normal.copy()
    if n.y < 0:
        n = -n
    return n


def add_floor_face(floor_tris, points, normal):
    n = axis_to_fixed(floor_normal(normal))
    for i in range(1, len(points) - 1):
        floor_tris.append((vec_to_int(points[0]), vec_to_int(points[i]), vec_to_int(points[i + 1]), n))


# ---------------------------------------------------------------- 2d helpers


def convex_hull_2d(pts):
    pts = sorted(set(pts))
    if len(pts) <= 2:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower = []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    upper = []
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def min_area_rect(pts):
    # rotating calipers over the hull edges, returns (area, (dx, dy)) for the best direction
    hull = convex_hull_2d(pts)
    best = None
    for i in range(len(hull)):
        ax, ay = hull[i]
        bx, by = hull[(i + 1) % len(hull)]
        dx, dy = bx - ax, by - ay
        length = math.hypot(dx, dy)
        if length < 1e-9:
            continue
        dx, dy = dx / length, dy / length
        us = [x * dx + y * dy for x, y in hull]
        vs = [-x * dy + y * dx for x, y in hull]
        area = (max(us) - min(us)) * (max(vs) - min(vs))
        if best is None or area < best[0]:
            best = (area, (dx, dy))
    if best is None:
        return 0.0, (1.0, 0.0)
    return best


def plane_basis(normal, up):
    u = up.cross(normal)
    if u.length < 1e-6:
        u = Vector((1, 0, 0)).cross(normal)
    u.normalize()
    v = normal.cross(u).normalized()
    return u, v


# ---------------------------------------------------------------- compact props


def best_box_axes(points, obj_axes, up):
    # cheapest boxes worth trying: the object's own axes, and the smallest upright box
    # (rotating calipers on the footprint), which is what most props are
    candidates = [obj_axes]
    gu, gv = plane_basis(up, Vector((1, 0, 0)) if abs(up.x) < 0.9 else Vector((0, 0, 1)))
    footprint = [(p.dot(gu), p.dot(gv)) for p in points]
    area, (dx, dy) = min_area_rect(footprint)
    if area > 0:
        a0 = (gu * dx + gv * dy).normalized()
        candidates.append([a0, up.copy(), a0.cross(up).normalized()])
    return min(candidates, key=lambda axes: obb_volume(points, axes))


def island_volume(faces):
    vol = 0.0
    for face in faces:
        v0 = face.verts[0].co
        for i in range(1, len(face.verts) - 1):
            vol += v0.dot(face.verts[i].co.cross(face.verts[i + 1].co)) / 6.0
    return vol


def face_islands(bm):
    seen = set()
    islands = []
    for face in bm.faces:
        if face.index in seen:
            continue
        seen.add(face.index)
        stack = [face]
        island = []
        while stack:
            f = stack.pop()
            island.append(f)
            for e in f.edges:
                for g in e.link_faces:
                    if g.index not in seen:
                        seen.add(g.index)
                        stack.append(g)
        islands.append(island)
    return islands


def is_closed(island):
    return all(len(e.link_faces) == 2 for f in island for e in f.edges)


# ---------------------------------------------------------------- wall merging


class WallFace:
    __slots__ = ("points", "normal", "area")

    def __init__(self, points, normal, area):
        self.points = points
        self.normal = normal
        self.area = area


def face_basis_obb(wall):
    # thin axis is the face normal, in-plane axis follows the longest edge
    pts = wall.points
    longest = max(((pts[(i + 1) % len(pts)] - pts[i]) for i in range(len(pts))), key=lambda e: e.length)
    u = longest - wall.normal * longest.dot(wall.normal)
    if u.length < 1e-9:
        u, _ = plane_basis(wall.normal, Vector((0, 1, 0)))
    u.normalize()
    v = wall.normal.cross(u).normalized()
    return make_obb(pts, (u, v, wall.normal))


def group_planes(walls):
    planes = []  # [normal, distance, [wall indices]]
    for idx, wall in enumerate(walls):
        d = wall.normal.dot(wall.points[0])
        for plane in planes:
            c = plane[0].dot(wall.normal)
            if c >= PLANE_COS_TOLERANCE and abs(plane[1] - d) <= PLANE_DISTANCE_TOLERANCE:
                plane[2].append(idx)
                break
            if c <= -PLANE_COS_TOLERANCE and abs(plane[1] + d) <= PLANE_DISTANCE_TOLERANCE:
                plane[2].append(idx)
                break
        else:
            planes.append([wall.normal.copy(), d, [idx]])
    return planes


def plane_adjacency(faces2d):
    # faces are neighbours when they share a stretch of edge, which also catches T-junctions
    # and faces that only touch because they sit on the same line in different objects
    lines = {}
    for fi, pts in enumerate(faces2d):
        for i in range(len(pts)):
            (ax, ay), (bx, by) = pts[i], pts[(i + 1) % len(pts)]
            dx, dy = bx - ax, by - ay
            length = math.hypot(dx, dy)
            if length < EDGE_TOLERANCE:
                continue
            dx, dy = dx / length, dy / length
            if dx < -1e-9 or (abs(dx) <= 1e-9 and dy < 0):
                dx, dy = -dx, -dy
            offset = -ax * dy + ay * dx
            key = (round(math.atan2(dy, dx), 3), round(offset / EDGE_TOLERANCE))
            t0, t1 = sorted((ax * dx + ay * dy, bx * dx + by * dy))
            lines.setdefault(key, []).append((t0, t1, fi))

    neighbours = [set() for _ in faces2d]
    for spans in lines.values():
        spans.sort()
        active = []
        for t0, t1, fi in spans:
            active = [s for s in active if s[1] - t0 > EDGE_TOLERANCE]
            for s in active:
                if s[2] != fi and min(s[1], t1) - t0 > EDGE_TOLERANCE:
                    neighbours[fi].add(s[2])
                    neighbours[s[2]].add(fi)
            active.append((t0, t1, fi))
    return neighbours


def merge_plane(walls, idxs, normal, up, tolerance, stats):
    pu, pv = plane_basis(normal, up)
    faces2d = []
    members = []
    seen = set()
    for idx in idxs:
        pts = [(p.dot(pu), p.dot(pv)) for p in walls[idx].points]
        key = tuple(sorted((round(x), round(y)) for x, y in pts))
        if key in seen:
            # the back of a double sided wall, same box
            stats["duplicate"] += 1
            continue
        seen.add(key)
        faces2d.append(pts)
        members.append(idx)

    neighbours = plane_adjacency(faces2d)

    def fit(chunk):
        pts = [p for i in chunk for p in faces2d[i]]
        rect_area, direction = min_area_rect(pts)
        area = sum(walls[members[i]].area for i in chunk)
        return (area / rect_area if rect_area > 0 else 1.0), direction

    def emit(chunk, direction):
        dx, dy = direction
        u = (pu * dx + pv * dy).normalized()
        v = normal.cross(u).normalized()
        pts = [p for i in chunk for p in walls[members[i]].points]
        return make_obb(pts, (u, v, normal))

    obbs = []
    unvisited = set(range(len(faces2d)))
    while unvisited:
        # one edge connected region
        start = unvisited.pop()
        region = {start}
        stack = [start]
        while stack:
            i = stack.pop()
            for j in neighbours[i]:
                if j in unvisited:
                    unvisited.discard(j)
                    region.add(j)
                    stack.append(j)

        ratio, direction = fit(region)
        if ratio >= tolerance:
            obbs.append(emit(region, direction))
            stats["region_faces"] += len(region)
            stats["region_obbs"] += 1
            continue

        # greedy split: grow a chunk while it still fits a rectangle, then start the next one
        remaining = set(region)
        while remaining:
            seed = min(remaining, key=lambda i: (min(y for _, y in faces2d[i]), min(x for x, _ in faces2d[i])))
            chunk = {seed}
            remaining.discard(seed)
            direction = fit(chunk)[1]
            while True:
                frontier = {j for i in chunk for j in neighbours[i] if j in remaining}
                best = None
                for j in frontier:
                    r, d = fit(chunk | {j})
                    if r >= tolerance and (best is None or r > best[0]):
                        best = (r, d, {j})
                if best is None:
                    # an L shape on the way to a rectangle needs two faces at once
                    for j in frontier:
                        for k in neighbours[j]:
                            if k in remaining and k not in chunk and k != j:
                                r, d = fit(chunk | {j, k})
                                if r >= tolerance and (best is None or r > best[0]):
                                    best = (r, d, {j, k})
                if best is None:
                    break
                direction = best[1]
                chunk |= best[2]
                remaining -= best[2]
            obbs.append(emit(chunk, direction))
            stats["split_faces"] += len(chunk)
            stats["split_obbs"] += 1
    return obbs


def merge_walls(walls, up, tolerance, stats):
    obbs = []
    for normal, _d, idxs in group_planes(walls):
        obbs += merge_plane(walls, idxs, normal, up, tolerance, stats)
    return obbs


# ---------------------------------------------------------------- per object


def export_col_object(obj, bm, rot_only, floor_normal_threshold, floor_tris, wall_obbs):
    # hand built collision: every face is used, walls are boxed in the object's own axes
    obj_rot = rot_only @ obj.matrix_world.to_3x3().normalized()
    obj_axes = [obj_rot.col[0].normalized(),
                obj_rot.col[1].normalized(),
                obj_rot.col[2].normalized()]

    for face in bm.faces:
        points = [v.co.copy() for v in face.verts]
        if floor_normal(face.normal).y >= floor_normal_threshold:
            add_floor_face(floor_tris, points, face.normal)
        else:
            wall_obbs.append(make_obb(points, obj_axes))


def infer_object(obj, bm, rot_only, up, floor_normal_threshold, fill_ratio, floor_tris, wall_faces, wall_obbs, stats):
    obj_rot = rot_only @ obj.matrix_world.to_3x3().normalized()
    obj_axes = [obj_rot.col[0].normalized(), obj_rot.col[1].normalized(), obj_rot.col[2].normalized()]

    mode = str(obj.get(MODE_PROP, "")).lower()
    if mode == "box":
        points = [v.co.copy() for v in bm.verts]
        if points:
            wall_obbs.append(make_obb(points, best_box_axes(points, obj_axes, up)))
            stats["box_obbs"] += 1
            stats["box_faces"] += len(bm.faces)
        return

    for island in face_islands(bm):
        if mode != "faces" and is_closed(island):
            # a compact prop: one box if the mesh fills enough of it. negative volume means the
            # normals point in, i.e. a room you stand inside, which must not become a solid box
            points = list({v.co.copy().freeze() for f in island for v in f.verts})
            axes = best_box_axes(points, obj_axes, up)
            box_vol = obb_volume(points, axes)
            vol = island_volume(island)
            if box_vol > 0 and vol >= fill_ratio * box_vol:
                wall_obbs.append(make_obb(points, axes))
                stats["box_obbs"] += 1
                stats["box_faces"] += len(island)
                continue

        for face in island:
            n = face.normal.normalized()
            up_dot = n.dot(up)
            points = [v.co.copy() for v in face.verts]
            if up_dot >= floor_normal_threshold:
                add_floor_face(floor_tris, points, n)
                stats["floor_faces"] += 1
            elif up_dot <= -floor_normal_threshold:
                # ceilings and the undersides of shelves are neither floor nor wall
                stats["ceiling_faces"] += 1
            elif face.calc_area() < MIN_FACE_AREA:
                stats["sliver_faces"] += 1
            else:
                wall_faces.append(WallFace(points, n, face.calc_area()))


# ---------------------------------------------------------------- grid + file


def obb_aabb_xz(center, axes, half_extents):
    rx = sum(abs(a.x) * h for a, h in zip(axes, half_extents))
    rz = sum(abs(a.z) * h for a, h in zip(axes, half_extents))
    return center.x - rx, center.x + rx, center.z - rz, center.z + rz


def build_grid(wall_obbs, cell_size):
    if not wall_obbs:
        return {}, 0, 0, cell_size, 0, 0

    # level bounds from the walls' world AABBs (XZ only). a rotated box reaches further than
    # its half extents along X and Z, so use its real footprint
    bounds = [obb_aabb_xz(c, a, h) for c, a, h in wall_obbs]
    all_min_x = min(b[0] for b in bounds)
    all_max_x = max(b[1] for b in bounds)
    all_min_z = min(b[2] for b in bounds)
    all_max_z = max(b[3] for b in bounds)

    origin_x = int(all_min_x) - cell_size
    origin_z = int(all_min_z) - cell_size

    grid_width  = math.ceil((all_max_x - all_min_x + cell_size * 2) / cell_size)
    grid_height = math.ceil((all_max_z - all_min_z + cell_size * 2) / cell_size)

    # initialise empty grid
    grid = {}
    for gx in range(grid_width):
        for gz in range(grid_height):
            grid[(gx, gz)] = []

    # insert each wall into all cells its AABB overlaps
    for wall_idx, (min_x, max_x, min_z, max_z) in enumerate(bounds):
        cell_min_x = max(0, int((min_x - origin_x) // cell_size))
        cell_max_x = min(grid_width  - 1, int((max_x - origin_x) // cell_size))
        cell_min_z = max(0, int((min_z - origin_z) // cell_size))
        cell_max_z = min(grid_height - 1, int((max_z - origin_z) // cell_size))

        for gx in range(cell_min_x, cell_max_x + 1):
            for gz in range(cell_min_z, cell_max_z + 1):
                grid[(gx, gz)].append(wall_idx)

    return grid, origin_x, origin_z, cell_size, grid_width, grid_height


def write_colbin(filepath, floor_tris, wall_obbs, grid, origin_x, origin_z, cell_size, grid_width, grid_height):
    with open(filepath, 'wb') as f:
        # header
        f.write(b'COLBIN')
        f.write(struct.pack('<B', 2))  # version
        f.write(struct.pack('<I', len(floor_tris)))
        f.write(struct.pack('<I', len(wall_obbs)))

        # grid header
        f.write(struct.pack('<i', origin_x))
        f.write(struct.pack('<i', origin_z))
        f.write(struct.pack('<I', cell_size))
        f.write(struct.pack('<H', grid_width))
        f.write(struct.pack('<H', grid_height))

        # grid cells, X major order
        for gx in range(grid_width):
            for gz in range(grid_height):
                indices = grid.get((gx, gz), [])
                f.write(struct.pack('<H', len(indices)))
                for idx in indices:
                    f.write(struct.pack('<H', idx))

        # floor tris
        for (v0, v1, v2, n) in floor_tris:
            write_vec3_int32(f, *v0)
            write_vec3_int32(f, *v1)
            write_vec3_int32(f, *v2)
            write_vec3_int16(f, *n)
            f.write(b'\x00\x00')  # padding

        # wall obbs
        for (center, axes, half_extents) in wall_obbs:
            write_vec3_int32(f, *vec_to_int(center))
            for axis in axes:
                write_vec3_int32(f, *axis_to_fixed(axis))
            write_vec3_int32(f, *(int(round(h)) for h in half_extents))
            f.write(struct.pack('<I', 0))  # flags
        return f.tell()


def gather_objects(context, source):
    # returns (objects exported as hand built collision, objects to infer collision from)
    col = find_collection(COLLECTION_NAME)
    if source == 'COL':
        if col is None:
            return None, f"ERROR: Collection '{COLLECTION_NAME}' not found"
        return [o for o in col.objects if o.type == 'MESH'], []

    def ignored(obj):
        return bool(obj.get(IGNORE_PROP, False))

    if source == 'SELECTED':
        return [], [o for o in context.selected_objects if o.type == 'MESH' and not ignored(o)]

    col_objs = set(col.all_objects) if col else set()
    nocol = find_collection(IGNORE_COLLECTION_NAME)
    skip = set(nocol.all_objects) if nocol else set()
    infer = [o for o in context.scene.objects
             if o.type == 'MESH' and o.visible_get() and o not in col_objs and o not in skip and not ignored(o)]
    hand = [o for o in col.objects if o.type == 'MESH'] if col else []
    return hand, infer


def do_export(context, filepath, floor_normal_threshold, forward_axis, up_axis, scale, cell_size_blender,
              source='COL', merge=True, fill_ratio=0.6, merge_tolerance=0.9):
    started = time.perf_counter()
    hand, infer = gather_objects(context, source)
    if hand is None:
        return infer

    global_matrix = axis_conversion(
        from_forward='Y', from_up='Z',
        to_forward=forward_axis, to_up=up_axis,
    ).to_4x4()
    global_matrix @= mathutils.Matrix.Scale(scale, 4)

    rot_only = global_matrix.to_3x3().normalized()
    up = (rot_only @ Vector((0, 0, 1))).normalized()

    cell_size = int(cell_size_blender * scale)  # convert to engine units

    depsgraph = context.evaluated_depsgraph_get()
    floor_tris = []
    wall_obbs  = []
    wall_faces = []
    stats = dict.fromkeys(("box_obbs", "box_faces", "floor_faces", "ceiling_faces", "sliver_faces",
                           "duplicate", "region_faces", "region_obbs", "split_faces", "split_obbs"), 0)

    for obj in hand:
        bm, _ = evaluated_bmesh(obj, depsgraph, global_matrix)
        export_col_object(obj, bm, rot_only, floor_normal_threshold, floor_tris, wall_obbs)
        bm.free()

    for obj in infer:
        bm, _ = evaluated_bmesh(obj, depsgraph, global_matrix)
        infer_object(obj, bm, rot_only, up, floor_normal_threshold, fill_ratio,
                     floor_tris, wall_faces, wall_obbs, stats)
        bm.free()

    if merge:
        wall_obbs += merge_walls(wall_faces, up, merge_tolerance, stats)
    else:
        wall_obbs += [face_basis_obb(w) for w in wall_faces]

    if len(wall_obbs) > 0xFFFF:
        return f"ERROR: {len(wall_obbs)} wall OBBs, the grid can only index 65535"

    # build spatial grid
    grid, origin_x, origin_z, cell_size_out, grid_width, grid_height = build_grid(wall_obbs, cell_size)
    size = write_colbin(filepath, floor_tris, wall_obbs, grid, origin_x, origin_z, cell_size_out,
                        grid_width, grid_height)

    total_cells = grid_width * grid_height
    total_refs  = sum(len(v) for v in grid.values())
    max_refs    = max((len(v) for v in grid.values()), default=0)
    msg = (f"Exported {len(floor_tris)} floor tris, {len(wall_obbs)} wall OBBs, "
           f"{grid_width}x{grid_height} grid ({total_cells} cells, {total_refs} wall refs, max {max_refs} per cell), "
           f"{size} bytes in {time.perf_counter() - started:.2f}s -> {filepath}")
    if infer:
        msg += ("\n  source %s: %d hand built, %d inferred objects; %d floor faces, %d ceiling faces dropped, "
                "%d slivers dropped; %d faces in %d prop boxes; %d wall faces" % (
                    source, len(hand), len(infer), stats["floor_faces"], stats["ceiling_faces"],
                    stats["sliver_faces"], stats["box_faces"], stats["box_obbs"], len(wall_faces)))
        if merge:
            msg += ("; merge: %d duplicates, %d faces in %d whole regions, %d faces in %d split chunks" % (
                stats["duplicate"], stats["region_faces"], stats["region_obbs"],
                stats["split_faces"], stats["split_obbs"]))
    return msg


class ExportCollBin(bpy.types.Operator, ExportHelper):
    """Export collision to PS1 collision binary"""
    bl_idname    = "export.colbin"
    bl_label     = "Export Collision Binary"

    filename_ext = ".colbin"
    filter_glob: StringProperty(default="*.colbin", options={'HIDDEN'})

    source: EnumProperty(
        name="Source",
        description="Where the collision comes from. Defaults to COL if that collection exists, else Scene",
        items=SOURCE_ITEMS,
        default='COL',
    )

    forward_axis: EnumProperty(
        name="Forward",
        items=AXIS_ITEMS,
        default='Z',
    )

    up_axis: EnumProperty(
        name="Up",
        items=AXIS_ITEMS,
        default='-Y',
    )

    global_scale: FloatProperty(
        name="Scale",
        default=128.0,
        min=0.01,
    )

    floor_threshold: FloatProperty(
        name="Floor Threshold",
        description="Face normal Y above this value is treated as floor (rest = wall)",
        default=0.7,
        min=0.0,
        max=1.0,
    )

    cell_size: FloatProperty(
        name="Grid Cell Size",
        description="Broad phase grid cell size in Blender units",
        default=2.0,
        min=0.1,
    )

    merge_walls: BoolProperty(
        name="Merge Walls",
        description="Join coplanar neighbouring wall faces into as few boxes as fit them (Selected and Scene)",
        default=True,
    )

    merge_tolerance: FloatProperty(
        name="Merge Tolerance",
        description="How much of a merged box the faces must cover, as area of faces / area of box",
        default=0.9,
        min=0.5,
        max=1.0,
    )

    fill_ratio: FloatProperty(
        name="Prop Fill Ratio",
        description="A closed mesh filling at least this much of its bounding box becomes one box",
        default=0.6,
        min=0.0,
        max=1.0,
    )

    def resolve_source(self):
        if not self.properties.is_property_set("source"):
            self.source = 'COL' if find_collection(COLLECTION_NAME) else 'SCENE'

    def invoke(self, context, event):
        self.resolve_source()
        return ExportHelper.invoke(self, context, event)

    def execute(self, context):
        self.resolve_source()
        msg = do_export(
            context,
            self.filepath,
            self.floor_threshold,
            self.forward_axis,
            self.up_axis,
            self.global_scale,
            self.cell_size,
            self.source,
            self.merge_walls,
            self.fill_ratio,
            self.merge_tolerance,
        )
        print(msg)
        if msg.startswith("ERROR"):
            self.report({'ERROR'}, msg)
            return {'CANCELLED'}
        self.report({'INFO'}, msg.split("\n")[0])
        return {'FINISHED'}


def menu_func_export(self, context):
    self.layout.operator(ExportCollBin.bl_idname, text="Collision Binary (.colbin)")


def register():
    bpy.utils.register_class(ExportCollBin)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(ExportCollBin)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()
