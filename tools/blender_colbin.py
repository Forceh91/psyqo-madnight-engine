bl_info = {
    "name": "Export Collision Binary (.colbin)",
    "author": "",
    "version": (1, 0),
    "blender": (3, 0, 0),
    "location": "File > Export > Collision Binary (.colbin)",
    "description": "Export COL collection to PS1 collision binary format",
    "category": "Import-Export",
}

import bpy
import bmesh
import struct
import mathutils
from mathutils import Vector
from bpy_extras.io_utils import ExportHelper, axis_conversion
from bpy.props import StringProperty, FloatProperty, EnumProperty

COLLECTION_NAME = "COL"

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

def do_export(filepath, floor_normal_threshold, forward_axis, up_axis, scale):
    col = next((c for c in bpy.data.collections if c.name.upper() == COLLECTION_NAME.upper()), None)
    if col is None:
        return f"ERROR: Collection '{COLLECTION_NAME}' not found"

    global_matrix = axis_conversion(
        from_forward='Y', from_up='Z',
        to_forward=forward_axis, to_up=up_axis,
    ).to_4x4()
    global_matrix @= mathutils.Matrix.Scale(scale, 4)

    rot_only = global_matrix.to_3x3().normalized()

    floor_tris = []
    wall_obbs  = []

    for obj in col.objects:
        if obj.type != 'MESH':
            continue

        # get object axes in engine space from world transform
        obj_rot = rot_only @ obj.matrix_world.to_3x3().normalized()
        obj_axes = [obj_rot.col[0].normalized(),   # local X
                    obj_rot.col[1].normalized(),   # local Y
                    obj_rot.col[2].normalized()]   # local Z

        bm = world_verts(obj)
        bm.faces.ensure_lookup_table()

        for face in bm.faces:
            verts = face.verts

            # transform face normal to engine space
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
                # no reordering, use object axes directly
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
                MIN_WALL_THICKNESS = 32
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

    with open(filepath, 'wb') as f:
        f.write(b'COLBIN')
        f.write(struct.pack('<B', 1))  # version
        f.write(struct.pack('<I', len(floor_tris)))
        f.write(struct.pack('<I', len(wall_obbs)))

        for (v0, v1, v2, n) in floor_tris:
            write_vec3_int32(f, *v0)
            write_vec3_int32(f, *v1)
            write_vec3_int32(f, *v2)
            write_vec3_int16(f, *n)
            f.write(b'\x00\x00')  # padding

        for (center, axes, half_extents, flags) in wall_obbs:
            write_vec3_int32(f, *center)
            for axis in axes:
                write_vec3_int16(f, *axis)
                f.write(b'\x00\x00')  # padding
            write_vec3_int32(f, *half_extents)
            f.write(struct.pack('<I', flags))

    return f"Exported {len(floor_tris)} floor tris, {len(wall_obbs)} wall OBBs -> {filepath}"


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

    def execute(self, context):
        msg = do_export(
            self.filepath,
            self.floor_threshold,
            self.forward_axis,
            self.up_axis,
            self.global_scale,
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