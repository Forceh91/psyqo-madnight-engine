bl_info = {
    "name": "Export ANIMBIN (.animbin)",
    "author": "Madnight & ChatGPT",
    "version": (1, 0),
    "blender": (3, 0, 0),
    "location": "File > Export > ANIMBIN",
    "description": "Exports armature animations into ANIMBIN format",
    "category": "Import-Export",
}

import bpy
import mathutils
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty, IntProperty
import struct

def bake_action(armature, action, start_frame, end_frame):
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode='POSE')
    armature.animation_data.action = action

    bpy.ops.nla.bake(
        frame_start=start_frame,
        frame_end=end_frame,
        step=1,
        only_selected=False,
        visual_keying=True,
        clear_constraints=False,
        clear_parents=False,
        use_current_action=True,
        clean_curves=True,
        bake_types={'POSE'},
        channel_types={'LOCATION', 'ROTATION'},
    )

def export_animbin(context, filepath, start_frame, end_frame, fps=30):
    armature = context.object
    if armature.type != 'ARMATURE':
        raise Exception("Select an armature object")

    # Bake the current action first
    action = armature.animation_data.action
    bake_action(armature, action, start_frame, end_frame)

    bones = [pbone for pbone in armature.pose.bones]

    with open(filepath, 'wb') as f:
        # Write header
        f.write(b"ANIMBIN")               # 7 bytes magic
        f.write(struct.pack("<B", 1))     # version
        f.write(struct.pack("<I", len(bones)))  # number of bones
        f.write(struct.pack("<I", end_frame - start_frame + 1))  # frames per bone

        # Write per-bone tracks
        for b, bone in enumerate(bones):
            for frame in range(start_frame, end_frame + 1):
                bpy.context.scene.frame_set(frame)
                mat = bone.matrix
                loc = mat.to_translation()
                rot = mat.to_quaternion()

                # convert to FP12
                def to_fp12(x):
                    val = int(x * 4096)
                    return max(-32768, min(32767, val))

                f.write(struct.pack("<hhh", to_fp12(loc.x), to_fp12(loc.y), to_fp12(loc.z)))
                f.write(struct.pack("<hhhh", to_fp12(rot.w), to_fp12(rot.x), to_fp12(rot.y), to_fp12(rot.z)))

class ExportAnimBin(bpy.types.Operator, ExportHelper):
    bl_idname = "export_anim.animbin"
    bl_label = "Export AnimationBin"

    filename_ext = ".animbin"
    filter_glob: StringProperty(default="*.animbin", options={'HIDDEN'})
    start_frame: IntProperty(name="Start Frame", default=1)
    end_frame: IntProperty(name="End Frame", default=250)

    def execute(self, context):
        export_animbin(context, self.filepath, self.start_frame, self.end_frame)
        return {'FINISHED'}

def menu_func_export(self, context):
    self.layout.operator(ExportAnimBin.bl_idname, text="ANIMBIN (.animbin)")

def register():
    bpy.utils.register_class(ExportAnimBin)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)

def unregister():
    bpy.utils.unregister_class(ExportAnimBin)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)

if __name__ == "__main__":
    register()

# bl_info = {
#     "name": "Export ANIMBIN (.animbin)",
#     "author": "Madnight & ChatGPT",
#     "version": (1, 1),
#     "blender": (3, 0, 0),
#     "location": "File > Export > ANIMBIN",
#     "description": "Exports armature animations in ANIMBIN format with rotation, translation, and markers",
#     "category": "Import-Export",
# }

# import bpy
# import struct
# import mathutils
# from bpy_extras.io_utils import ExportHelper
# from bpy.props import StringProperty

# # Helpers
# def float_to_fp16(val):
#     return int(max(min(val * 32767, 32767), -32768))

# def float_to_fp32(val):
#     return int(val * 4096)

# def bake_action(armature, action, start_frame, end_frame):
#     bpy.context.view_layer.objects.active = armature
#     bpy.ops.object.mode_set(mode='POSE')
#     armature.animation_data.action = action
#     bpy.ops.nla.bake(
#         frame_start=start_frame,
#         frame_end=end_frame,
#         step=1,
#         only_selected=False,
#         visual_keying=True,
#         clear_constraints=False,
#         clear_parents=False,
#         use_current_action=True,
#         clean_curves=True,
#         bake_types={'POSE'},
#         channel_types={'LOCATION', 'ROTATION'},
#     )

# def export_animbin(context, filepath):
#     armature = context.object
#     if not armature or armature.type != 'ARMATURE':
#         raise Exception("Select an armature to export animations.")

#     actions = bpy.data.actions
#     with open(filepath, 'wb') as f:
#         # Magic
#         f.write(b'ANIMBIN')
#         # Number of animations
#         f.write(struct.pack("<I", len(actions)))

#         for action in actions:
#             start_frame = int(action.frame_range[0])
#             end_frame = int(action.frame_range[1])
#             length = end_frame - start_frame + 1

#             bake_action(armature, action, start_frame, end_frame)

#             # Animation header
#             name_bytes = action.name.encode('utf-8')[:32].ljust(32, b'\0')
#             f.write(name_bytes)

#             flags = 1  # example: looped
#             f.write(struct.pack("<I", flags))
#             f.write(struct.pack("<H", length))

#             bones = [b for b in armature.pose.bones]
#             numTracks = len(bones) * 2  # rotation + translation
#             f.write(struct.pack("<H", numTracks))

#             for bone_index, bone in enumerate(bones):
#                 # Rotation track
#                 f.write(struct.pack("<B", 0))  # track type 0 = rotation
#                 f.write(struct.pack("<B", bone_index))
#                 f.write(struct.pack("<H", length))  # numKeys

#                 for frame in range(start_frame, end_frame+1):
#                     bpy.context.scene.frame_set(frame)
#                     rot = bone.rotation_quaternion
#                     f.write(struct.pack("<I", frame))
#                     f.write(struct.pack("<B", 0))  # keyType 0 = rotation
#                     f.write(struct.pack("<hhhh",
#                         float_to_fp16(rot.w),
#                         float_to_fp16(rot.x),
#                         float_to_fp16(rot.y),
#                         float_to_fp16(rot.z),
#                     ))

#                 # Translation track
#                 f.write(struct.pack("<B", 1))  # track type 1 = translation
#                 f.write(struct.pack("<B", bone_index))
#                 f.write(struct.pack("<H", length))  # numKeys

#                 for frame in range(start_frame, end_frame+1):
#                     bpy.context.scene.frame_set(frame)
#                     loc = bone.location
#                     f.write(struct.pack("<I", frame))
#                     f.write(struct.pack("<B", 1))  # keyType 1 = translation
#                     f.write(struct.pack("<iii",
#                         float_to_fp32(loc.x),
#                         float_to_fp32(loc.y),
#                         float_to_fp32(loc.z),
#                     ))

#             # Export markers from Blender scene
#             markers = [m for m in bpy.context.scene.timeline_markers]
#             f.write(struct.pack("<H", len(markers)))
#             for marker in markers:
#                 name_bytes = marker.name.encode('utf-8')[:32].ljust(32, b'\0')
#                 f.write(name_bytes)
#                 f.write(struct.pack("<I", int(marker.frame)))

# class ExportAnimBin(bpy.types.Operator, ExportHelper):
#     """Export ANIMBIN format"""
#     bl_idname = "export_anim.animbin"
#     bl_label = "Export ANIMBIN"

#     filename_ext = ".animbin"
#     filter_glob: StringProperty(default="*.animbin", options={'HIDDEN'})

#     def execute(self, context):
#         export_animbin(context, self.filepath)
#         return {'FINISHED'}

# def menu_func_export(self, context):
#     self.layout.operator(ExportAnimBin.bl_idname, text="ANIMBIN (.animbin)")

# def register():
#     bpy.utils.register_class(ExportAnimBin)
#     bpy.types.TOPBAR_MT_file_export.append(menu_func_export)

# def unregister():
#     bpy.utils.unregister_class(ExportAnimBin)
#     bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)

# if __name__ == "__main__":
#     register()
