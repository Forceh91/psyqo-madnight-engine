bl_info = {
    "name": "Export Collision Binary (.colbin)",
    "author": "",
    "version": (2, 0),
    "blender": (3, 0, 0),
    "location": "File > Export > Collision Binary (.colbin)",
    "description": "Export COL collection to PS1 collision binary format",
    "category": "Import-Export",
}

import bpy
import bmesh
import struct
import math
import mathutils
from mathutils import Vector
from bpy_extras.io_utils import ExportHelper, axis_conversion
from bpy.props import StringProperty, FloatProperty, EnumProperty

COLLECTION_NAME = "COL"
MIN_WALL_THICKNESS = 32

AXIS_ITEMS = (
    ('X',  'X',  ''),
    ('Y',  'Y',  ''),
    ('Z',  'Z',  ''),
    ('-X', '-X', ''),
    ('-Y', '-Y', ''),
    ('-Z', '-Z', ''),
)

def world_verts(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.transform(bm, matrix=obj.matrix_world, verts=bm.verts)
    return bm

def apply_matrix_vert(mat, vert):
    co = mat @ vert.co
    return (int(co.x), int(co.y), int(co.z))

def axis_to_fixed(v):
    return (int(v.x * 4096), int(v.y * 4096), int(v.z * 4096))

def write_vec3_int32(f, x, y, z):
    f.write(struct.pack('<iii', x, y, z))

def write_vec3_int16(f, x, y, z):
    f.write(struct.pack('<hhh', x, y, z))

def build_grid(wall_obbs, cell_size):
    if not wall_obbs:
        return {}, 0, 0, 0, 0, 0, 0

    # compute level bounds from wall AABB extents (XZ only)
    all_min_x = min(cx - hx for (cx, cy, cz), axes, (hx, hy, hz), flags in wall_obbs)
    all_max_x = max(cx + hx for (cx, cy, cz), axes, (hx, hy, hz), flags in wall_obbs)
    all_min_z = min(cz - hz for (cx, cy, cz), axes, (hx, hy, hz), flags in wall_obbs)
    all_max_z = max(cz + hz for (cx, cy, cz), axes, (hx, hy, hz), flags in wall_obbs)

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
    for wall_idx, (center, axes, half_extents, flags) in enumerate(wall_obbs):
        cx, cy, cz = center
        hx, hy, hz = half_extents

        min_x = cx - hx
        max_x = cx + hx
        min_z = cz - hz
        max_z = cz + hz

        cell_min_x = max(0, int((min_x - origin_x) // cell_size))
        cell_max_x = min(grid_width  - 1, int((max_x - origin_x) // cell_size))
        cell_min_z = max(0, int((min_z - origin_z) // cell_size))
        cell_max_z = min(grid_height - 1, int((max_z - origin_z) // cell_size))

        for gx in range(cell_min_x, cell_max_x + 1):
            for gz in range(cell_min_z, cell_max_z + 1):
                grid[(gx, gz)].append(wall_idx)

    return grid, origin_x, origin_z, cell_size, grid_width, grid_height

def do_export(filepath, floor_normal_threshold, forward_axis, up_axis, scale, cell_size_blender):
    col = next((c for c in bpy.data.collections if c.name.upper() == COLLECTION_NAME.upper()), None)
    if col is None:
        return f"ERROR: Collection '{COLLECTION_NAME}' not found"

    global_matrix = axis_conversion(
        from_forward='Y', from_up='Z',
        to_forward=forward_axis, to_up=up_axis,
    ).to_4x4()
    global_matrix @= mathutils.Matrix.Scale(scale, 4)

    rot_only = global_matrix.to_3x3().normalized()

    cell_size = int(cell_size_blender * scale)  # convert to engine units

    floor_tris = []
    wall_obbs  = []

    for obj in col.objects:
        if obj.type != 'MESH':
            continue

        obj_rot = rot_only @ obj.matrix_world.to_3x3().normalized()
        obj_axes = [obj_rot.col[0].normalized(),
                    obj_rot.col[1].normalized(),
                    obj_rot.col[2].normalized()]

        bm = world_verts(obj)
        bm.faces.ensure_lookup_table()

        for face in bm.faces:
            verts = face.verts

            face_normal = (rot_only @ face.normal).normalized()
            if face_normal.y < 0:
                face_normal = -face_normal

            if face_normal.y >= floor_normal_threshold:
                # floor — fan triangulate
                n = axis_to_fixed(face_normal)
                for i in range(1, len(verts) - 1):
                    v0 = apply_matrix_vert(global_matrix, verts[0])
                    v1 = apply_matrix_vert(global_matrix, verts[i])
                    v2 = apply_matrix_vert(global_matrix, verts[i + 1])
                    floor_tris.append((v0, v1, v2, n))
            else:
                # wall — OBB from object axes
                axes = list(obj_axes)
                positions = [global_matrix @ v.co for v in verts]
                center = sum(positions, Vector((0, 0, 0))) / len(positions)

                half_extents = []
                for axis in axes:
                    projs = [v.dot(axis) for v in positions]
                    extent = (max(projs) - min(projs)) / 2.0
                    half_extents.append(extent)

                # clamp thinnest axis to minimum wall thickness
                min_idx = half_extents.index(min(half_extents))
                half_extents = [int(h) for h in half_extents]
                half_extents[min_idx] = max(half_extents[min_idx], MIN_WALL_THICKNESS)

                flags = 0
                wall_obbs.append((
                    (int(center.x), int(center.y), int(center.z)),
                    [axis_to_fixed(a) for a in axes],
                    half_extents,
                    flags
                ))

        bm.free()

    # build spatial grid
    grid, origin_x, origin_z, cell_size_out, grid_width, grid_height = build_grid(wall_obbs, cell_size)

    with open(filepath, 'wb') as f:
        # header
        f.write(b'COLBIN')
        f.write(struct.pack('<B', 2))  # version
        f.write(struct.pack('<I', len(floor_tris)))
        f.write(struct.pack('<I', len(wall_obbs)))

        # grid header
        f.write(struct.pack('<i', origin_x))
        f.write(struct.pack('<i', origin_z))
        f.write(struct.pack('<I', cell_size_out))
        f.write(struct.pack('<H', grid_width))
        f.write(struct.pack('<H', grid_height))

        # grid cells — X major order
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
        for (center, axes, half_extents, flags) in wall_obbs:
            write_vec3_int32(f, *center)
            for axis in axes:
                write_vec3_int32(f, *axis)
            write_vec3_int32(f, *half_extents)
            f.write(struct.pack('<I', flags))

    total_cells = grid_width * grid_height
    total_refs  = sum(len(v) for v in grid.values())
    return (f"Exported {len(floor_tris)} floor tris, {len(wall_obbs)} wall OBBs, "
            f"{grid_width}x{grid_height} grid ({total_cells} cells, {total_refs} wall refs) -> {filepath}")


class ExportCollBin(bpy.types.Operator, ExportHelper):
    """Export COL collection to PS1 collision binary"""
    bl_idname    = "export.colbin"
    bl_label     = "Export Collision Binary"

    filename_ext = ".colbin"
    filter_glob: StringProperty(default="*.colbin", options={'HIDDEN'})

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

    def execute(self, context):
        msg = do_export(
            self.filepath,
            self.floor_threshold,
            self.forward_axis,
            self.up_axis,
            self.global_scale,
            self.cell_size,
        )
        self.report({'INFO'}, msg)
        print(msg)
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