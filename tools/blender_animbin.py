bl_info = {
    "name": "Export ANIMBIN (.animbin)",
    "author": "Madnight & ChatGPT",
    "version": (1, 1),
    "blender": (3, 0, 0),
    "location": "File > Export > ANIMBIN",
    "description": "Exports armature animations into ANIMBIN format",
    "category": "Import-Export",
}

import bpy
import struct
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, IntProperty

FP12_SCALE = 4096

# ------------------------------------------------------------
# Fixed-point helper
# ------------------------------------------------------------

def fp12_16(v):
    i = int(v * FP12_SCALE)
    return max(-32768, min(32767, i))
    
def quat_ensure_shortest(prev, curr):
    # If dot < 0, flip quaternion
    if prev.dot(curr) < 0.0:
        return -curr
    return curr


# ------------------------------------------------------------
# Blender 3.x / 4.x safe F-curve iterator
# ------------------------------------------------------------

def iter_fcurves(action):
    # Blender 3.x
    if hasattr(action, "fcurves"):
        for fc in action.fcurves:
            yield fc

    # Blender 4.x
    elif hasattr(action, "layers"):
        for layer in action.layers:
            for strip in layer.strips:
                if not hasattr(strip, "channels"):
                    continue
                for channel in strip.channels:
                    for fc in channel.fcurves:
                        yield fc

def bone_has_rotation_change(pbone, scene, start_frame, end_frame, eps=1e-5):
    scene.frame_set(start_frame)
    q_prev = pbone.matrix.to_quaternion()

    for frame in range(start_frame + 1, end_frame + 1):
        scene.frame_set(frame)
        q = pbone.matrix.to_quaternion()
        if q_prev.rotation_difference(q).angle > eps:
            return True
        q_prev = q

    return False                        

# ------------------------------------------------------------
# Export
# ------------------------------------------------------------

def export_animbin(context, filepath, start_frame, end_frame):
    arm = context.object
    if not arm or arm.type != "ARMATURE":
        raise RuntimeError("Select an armature object")

    if not arm.animation_data or not arm.animation_data.action:
        raise RuntimeError("Armature has no active action")

    action = arm.animation_data.action
    scene = context.scene
    frame_count = end_frame - start_frame + 1

    # ============================================================
    # Collect bones that actually have rotation keyframes
    # ============================================================
    bone_names = []

    for b in arm.data.bones:
        if not b.use_deform:
            continue

        pbone = arm.pose.bones.get(b.name)
        if not pbone:
            continue

        if bone_has_rotation_change(
            pbone, scene, start_frame, end_frame
        ):
            bone_names.append(b.name)


    if not bone_names:
        raise RuntimeError("No deform bones with rotation keys found")

    num_tracks = len(bone_names)

    with open(filepath, "wb") as f:
        # =========================
        # FILE HEADER
        # =========================
        f.write(b"ANIMBIN")                 # char[7]
        f.write(struct.pack("<B", 1))       # version
        f.write(struct.pack("<B", 1))       # numAnimations

        # =========================
        # ANIMATION HEADER
        # =========================
        name_bytes = action.name.encode("ascii", errors="ignore")[:32]
        name_bytes = name_bytes.ljust(32, b"\0")
        f.write(name_bytes)

        flags = 1  # looped
        f.write(struct.pack("<I", flags))
        f.write(struct.pack("<H", frame_count))
        f.write(struct.pack("<H", num_tracks))
        f.write(struct.pack("<H", 0))       # numMarkers

        # =========================
        # TRACKS (ROTATION ONLY)
        # =========================
        for joint_id, bone_name in enumerate(bone_names):
            pbone = arm.pose.bones.get(bone_name)
            if not pbone:
                raise RuntimeError(f"Missing pose bone: {bone_name}")

            # Track header
            f.write(struct.pack("<B", 0))            # type = ROTATION
            f.write(struct.pack("<B", joint_id))     # jointId
            f.write(struct.pack("<H", frame_count))  # numKeys

            # Keys
            prev_q = None
            for i, frame in enumerate(range(start_frame, end_frame + 1)):
                scene.frame_set(frame)

                q = pbone.matrix.to_quaternion()
                q.normalize()
                
                if i > 0:
                    q = quat_ensure_shortest(prev_q, q)

                prev_q = q.copy()                

                f.write(struct.pack("<H", i))  # frame
                f.write(struct.pack("<B", 0))  # keyType = ROTATION
                f.write(struct.pack(
                    "<hhhh",
                    fp12_16(q.w),
                    fp12_16(q.x),
                    fp12_16(q.y),
                    fp12_16(q.z),
                ))

    print(f"ANIMBIN exported: {filepath}")
    print(f"Frames: {frame_count}, Tracks: {num_tracks}")

# ------------------------------------------------------------
# Blender Operator
# ------------------------------------------------------------

class ExportAnimBin(bpy.types.Operator, ExportHelper):
    bl_idname = "export_anim.animbin"
    bl_label = "Export AnimationBin"

    filename_ext = ".animbin"
    filter_glob: StringProperty(default="*.animbin", options={'HIDDEN'})
    start_frame: IntProperty(name="Start Frame", default=1)
    end_frame: IntProperty(name="End Frame", default=30)

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
