bl_info = {
    "name": "Export OBJ+SKEL (.objskel)",
    "author": "Madnight & ChatGPT",
    "version": (1, 0),
    "blender": (3, 0, 0),
    "location": "File > Export > OBJ+SKEL",
    "description": "Exports mesh with skeleton & vertex-bone mapping",
    "category": "Import-Export",
}

import bpy
import mathutils
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty


def export_obj_skel(context, filepath):
    obj = context.object
    if not obj or obj.type != "MESH":
        raise Exception("Select a mesh object with an armature parent.")

    arm = obj.find_armature()
    mesh = obj.to_mesh()

    with open(filepath, "w", encoding="utf-8") as f:
        f.write(f"# Exported from Blender OBJ+SKEL exporter\n")

        # Vertices
        for v in mesh.vertices:
            f.write(f"v {v.co.x:.6f} {v.co.y:.6f} {v.co.z:.6f}\n")

        # UVs (if any)
        if mesh.uv_layers.active:
            for uv in mesh.uv_layers.active.data:
                f.write(f"vt {uv.uv.x:.6f} {uv.uv.y:.6f}\n")

        # Faces (assuming one UV per vertex)
        for poly in mesh.polygons:
            indices = [f"{i+1}/{i+1}" for i in poly.vertices]
            f.write(f"f {' '.join(indices)}\n")

        # Skeleton
        if arm:
            bones = arm.data.bones
            f.write(f"skel {len(bones)}\n")
            for i, bone in enumerate(bones):
                if not bone.use_deform:
                    continue
                
                parent = bones.find(bone.parent.name) if bone.parent else -1
                head = bone.head_local
                rot = bone.matrix_local.to_quaternion()
                f.write(
                    f"bone {i} {bone.name} {parent} "
                    f"{head.x:.6f} {head.y:.6f} {head.z:.6f} "
                    f"{rot.x:.6f} {rot.y:.6f} {rot.z:.6f} {rot.w:.6f}\n"
                )

        # Vertex → Bone mapping
        if arm:
            for v in mesh.vertices:
                if v.groups:
                    # Use strongest weight (first in list)
                    g = max(v.groups, key=lambda gr: gr.weight)
                    vg_name = obj.vertex_groups[g.group].name
                    bone_index = list(bones).index(arm.pose.bones[vg_name].bone)
                    f.write(f"vw {v.index+1} {bone_index}\n")
                else:
                    f.write(f"vw {v.index+1} -1\n")

    print(f"Exported {filepath}")


class ExportObjSkel(bpy.types.Operator, ExportHelper):
    """Export OBJ+SKEL format"""
    bl_idname = "export_scene.obj_skel"
    bl_label = "Export OBJ+SKEL"

    filename_ext = ".obj"
    filter_glob: StringProperty(default="*.obj", options={'HIDDEN'})

    def execute(self, context):
        export_obj_skel(context, self.filepath)
        return {'FINISHED'}


def menu_func_export(self, context):
    self.layout.operator(ExportObjSkel.bl_idname, text="OBJ+SKEL (.obj)")


def register():
    bpy.utils.register_class(ExportObjSkel)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(ExportObjSkel)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()
