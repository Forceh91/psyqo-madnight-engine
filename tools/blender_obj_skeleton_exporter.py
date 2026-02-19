bl_info = {
    "name": "Export OBJ+SKEL (.objskel)",
    "author": "Madnight & ChatGPT",
    "version": (1, 1),
    "blender": (3, 0, 0),
    "location": "File > Export > OBJ+SKEL",
    "description": "Exports mesh with skeleton & vertex-bone mapping, with standard OBJ export options",
    "category": "Import-Export",
}

import bpy
import mathutils
from bpy_extras.io_utils import ExportHelper, axis_conversion
from bpy.props import StringProperty, BoolProperty, FloatProperty, EnumProperty

# Go from Blender's coordinate system into PS1's coordinate system:
axis_basis_change = mathutils.Matrix(
    ((1.0,  0.0,  0.0, 0.0),
     (0.0,  -1.0,  0.0, 0.0),
     (0.0, 0.0,  -1.0, 0.0),
     (0.0,  0.0,  0.0, 1.0))
)

def scale_bone_translation(bone, translation, global_matrix, arm_world):
    # Get actual bone length in world space
    bone_head_ws = arm_world @ bone.head
    if bone.parent:
        parent_head_ws = arm_world @ bone.parent.head
        actual_offset = bone_head_ws - parent_head_ws
    else:
        actual_offset = bone_head_ws
    
    # Scale the decomposed translation to match actual bone length
    decomposed_length = translation.length
    if decomposed_length > 0.0001:
        actual_length = actual_offset.length
        scale_factor = actual_length / decomposed_length
        return translation.normalized() * actual_length
    else:
        return actual_offset

def export_obj_skel(context, filepath, apply_modifiers=True, export_selected=True,
                    global_scale=1.0, forward_axis='Z', up_axis='-Y'):

    # Axis conversion + scale
    global_matrix = axis_conversion(from_forward='Y', from_up='Z',
                                    to_forward=forward_axis, to_up=up_axis).to_4x4()
    global_matrix @= mathutils.Matrix.Scale(global_scale, 4)

    objs = [context.object]
    if export_selected:
        objs = context.selected_objects

    with open(filepath, "w", encoding="utf-8") as f:
        f.write("# Exported from Blender OBJ+SKEL exporter\n")

        for obj in objs:
            if obj.type != "MESH":
                continue

            arm = obj.find_armature()
            depsgraph = context.evaluated_depsgraph_get()
            mesh_eval = obj.evaluated_get(depsgraph).to_mesh() if apply_modifiers else obj.to_mesh()

            # Apply transforms
            mesh_eval.transform(global_matrix @ obj.matrix_world)

            f.write(f"o {obj.name}\n")

            # Vertices - export in model space (transformed by global_matrix)
            for v in mesh_eval.vertices:
                f.write(f"v {v.co.x:.6f} {v.co.y:.6f} {v.co.z:.6f}\n")            

            # UVs
            uv_layer = mesh_eval.uv_layers.active
            if uv_layer:
                uvs = [None] * len(mesh_eval.loops)
                for li, loop in enumerate(mesh_eval.loops):
                    uvs[li] = uv_layer.data[li].uv
                for uv in uvs:
                    f.write(f"vt {uv.x:.6f} {uv.y:.6f}\n")

            # Faces
            for poly in mesh_eval.polygons:
                f.write("f")
                for loop_index in poly.loop_indices:
                    vertex_index = mesh_eval.loops[loop_index].vertex_index
                    f.write(f" {vertex_index + 1}/{loop_index + 1}")
                f.write("\n")

            # Skeleton - bone positions transformed same as mesh
            if arm:
                deform_bones = [b for b in arm.pose.bones if b.bone.use_deform]
                bone_index_map = {b.name: i for i, b in enumerate(deform_bones)}
                f.write(f"skel {len(deform_bones)}\n")

                arm_world = global_matrix @ arm.matrix_world
                for i, bone in enumerate(deform_bones):
                    parent_idx = bone_index_map[bone.parent.name] if bone.parent else -1
                    
                    # Get transform in PS1 coordinate system
                    if bone.parent is None:
                        transform = axis_basis_change @ bone.matrix
                    else:
                        transform = bone.parent.matrix.inverted_safe() @ bone.matrix
                    
                    translation, rotation, scale = transform.decompose()
                    translation = scale_bone_translation(bone, translation, global_matrix, arm_world)
                    
                    # Apply global_scale if needed
                    if global_scale > 0.0:
                        translation.x *= global_scale
                        translation.y *= global_scale
                        translation.z *= global_scale
                    
                    rest_quat = rotation
                    f.write(
                        f"bone {i} {bone.name} {parent_idx} "
                        f"{translation.x:.6f} {translation.y:.6f} {translation.z:.6f} "
                        f"{rest_quat.w:.6f} {rest_quat.x:.6f} {rest_quat.y:.6f} {rest_quat.z:.6f}\n"
                    )
                
                # Vertex → Bone mapping
                for v in mesh_eval.vertices:
                    if v.groups:
                        g = max(v.groups, key=lambda gr: gr.weight)
                        vg_name = obj.vertex_groups[g.group].name
                        bone_idx = next((i for i, pb in enumerate(deform_bones) if pb.name == vg_name), -1)
                        f.write(f"vw {v.index + 1} {bone_idx}\n")
                    else:
                        f.write(f"vw {v.index + 1} -1\n")

            obj.to_mesh_clear()

    print(f"Exported {filepath}")


class ExportObjSkel(bpy.types.Operator, ExportHelper):
    """Export OBJ+SKEL format"""
    bl_idname = "export_scene.obj_skel"
    bl_label = ".obj with skeleton"
    filename_ext = ".obj"
    filter_glob: StringProperty(default="*.obj", options={'HIDDEN'})

    apply_modifiers: BoolProperty(
        name="Apply Modifiers",
        description="Apply object modifiers before exporting",
        default=True,
    )
    use_selection: BoolProperty(
        name="Selected Objects Only",
        description="Export only selected objects",
        default=True,
    )
    global_scale: FloatProperty(
        name="Scale",
        description="Scale all data",
        default=1.0,
    )
    forward_axis: EnumProperty(
        name="Forward",
        items=(('X', "X Forward", ""), ('Y', "Y Forward", ""), ('Z', "Z Forward", ""),
               ('-X', "-X Forward", ""), ('-Y', "-Y Forward", ""), ('-Z', "-Z Forward", "")),
        default='Z',
    )
    up_axis: EnumProperty(
        name="Up",
        items=(('X', "X Up", ""), ('Y', "Y Up", ""), ('Z', "Z Up", ""),
               ('-X', "-X Up", ""), ('-Y', "-Y Up", ""), ('-Z', "-Z Up", "")),
        default='-Y',
    )

    def execute(self, context):
        export_obj_skel(
            context, self.filepath,
            self.apply_modifiers, self.use_selection,
            self.global_scale, self.forward_axis, self.up_axis
        )
        return {'FINISHED'}


def menu_func_export(self, context):
    self.layout.operator(ExportObjSkel.bl_idname, text="Wavefront with skeleton (.obj)")


def register():
    bpy.utils.register_class(ExportObjSkel)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(ExportObjSkel)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()