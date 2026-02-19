bl_info = {
    "name": "Export ANIMBIN (.animbin)",
    "author": "Madnight & ChatGPT",
    "version": (2, 1),
    "blender": (5, 0, 0),
    "location": "File > Export > ANIMBIN",
    "description": "Exports armature animations with absolute rotations",
    "category": "Import-Export",
}

import bpy
import struct
import mathutils
from bpy_extras.io_utils import ExportHelper, axis_conversion
from bpy.props import StringProperty, IntProperty

FP12_SCALE = 4096

# Go from Blender's coordinate system into PS1's coordinate system:
axis_basis_change = mathutils.Matrix(
    ((1.0,  0.0,  0.0, 0.0),
     (0.0,  -1.0,  0.0, 0.0),
     (0.0, 0.0,  -1.0, 0.0),
     (0.0,  0.0,  0.0, 1.0))
)

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
# Get bone rotation as quaternion
# ------------------------------------------------------------
def quatRotation(poseBone):
    if poseBone.parent:
        matrix_basis = poseBone.bone.convert_local_to_pose(
            poseBone.matrix,
            poseBone.bone.matrix_local,
            parent_matrix=poseBone.parent.matrix,
            parent_matrix_local=poseBone.parent.bone.matrix_local,
            invert=True
        )
        
    else:
        matrix_basis = poseBone.bone.convert_local_to_pose(
            poseBone.matrix,
            poseBone.bone.matrix_local,
            invert=True
        )

    return matrix_basis.to_quaternion()

# ------------------------------------------------------------
# Determine if bone has rotation keyframes
# ------------------------------------------------------------
def bone_has_rotation_change(pbone, scene, start_frame, end_frame, eps=1e-5):
    scene.frame_set(start_frame)
    q_prev = get_bone_rotation_quat(pbone)

    for frame in range(start_frame + 1, end_frame + 1):
        scene.frame_set(frame)
        q = get_bone_rotation_quat(pbone)
            
        if q_prev.rotation_difference(q).angle > eps:
            return True
        q_prev = q

    return False

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
        channel_types={'ROTATION'},
    )    

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
    bake_action(arm, action, start_frame, end_frame)

    scene = context.scene
    frame_count = end_frame - start_frame + 1

    # Store current frame
    original_frame = scene.frame_current

    # ============================================================
    # Build bone index map (matching skeleton export order)
    # ============================================================
    all_deform_bones = [b for b in arm.pose.bones if b.bone.use_deform]
    bone_index_map = {b.name: i for i, b in enumerate(all_deform_bones)}

    # ============================================================
    # Collect bones that have rotation changes
    # ============================================================
    bone_names = []

    for b in all_deform_bones:
        pbone = arm.pose.bones.get(b.name)
        if not pbone:
            continue

        # if bone_has_rotation_change(pbone, scene, start_frame, end_frame):
        bone_names.append(b.name)

    if not bone_names:
        raise RuntimeError("No deform bones with rotation changes found")

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
        # TRACKS (ABSOLUTE ROTATIONS)
        # =========================
        for bone_name in bone_names:
            pbone = arm.pose.bones.get(bone_name)
            if not pbone:
                raise RuntimeError(f"Missing pose bone: {bone_name}")

            # CRITICAL: Use actual skeleton bone index, not enumeration!
            joint_id = bone_index_map[bone_name]

            # Track header
            f.write(struct.pack("<B", 0))            # type = ROTATION
            f.write(struct.pack("<B", joint_id))     # jointId - MATCHES SKELETON!
            f.write(struct.pack("<H", frame_count))  # numKeys
            
            if joint_id == 8:
                scene.frame_set(0)
                bpy.context.view_layer.update()
                q_rest = quatRotation(pbone)
                print(f"{pbone.name} REST: w={q_rest.w:.4f} x={q_rest.x:.4f} y={q_rest.y:.4f} z={q_rest.z:.4f}")            

            # Keys - absolute rotations
            for i, frame in enumerate(range(start_frame, end_frame + 1)):
                scene.frame_set(frame)
                bpy.context.view_layer.update()

                if pbone.parent is None:
                    transform = axis_basis_change @ pbone.matrix
                else:
                    transform = pbone.parent.matrix.inverted_safe() @ pbone.matrix
                
                translation, rotation, scale = transform.decompose()
                q = rotation

                f.write(struct.pack("<H", i))  # frame
                f.write(struct.pack("<B", 0))  # keyType = ROTATION
                f.write(struct.pack(
                    "<hhhh",
                    fp12_16(q.w),
                    fp12_16(q.x),
                    fp12_16(q.y),
                    fp12_16(q.z),
                ))

                print(f"{bone_name} frame {frame}: w={q.w:.4f} x={q.x:.4f} y={q.y:.4f} z={q.z:.4f}")
                print(f"  encoded: w={fp12_16(q.w)} x={fp12_16(q.x)} y={fp12_16(q.y)} z={fp12_16(q.z)}")

    # Restore original frame
    scene.frame_set(original_frame)

    print(f"ANIMBIN exported: {filepath}")
    print(f"Frames: {frame_count}, Tracks: {num_tracks}")
    print(f"Bone indices used:")
    for bone_name in bone_names:
        print(f"  {bone_name} → joint {bone_index_map[bone_name]}")

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