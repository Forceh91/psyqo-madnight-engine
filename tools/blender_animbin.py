bl_info = {
    "name": "Export ANIMBIN (.animbin)",
    "author": "Madnight & ChatGPT",
    "version": (2, 2),
    "blender": (5, 0, 0),
    "location": "File > Export > ANIMBIN",
    "description": "Exports armature animations with absolute rotations (multi-action support)",
    "category": "Import-Export",
}

import bpy
import struct
import mathutils
from bpy_extras.io_utils import ExportHelper, axis_conversion
from bpy.props import StringProperty, IntProperty, EnumProperty, BoolProperty

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
    q_prev = quatRotation(pbone)

    for frame in range(start_frame + 1, end_frame + 1):
        scene.frame_set(frame)
        q = quatRotation(pbone)

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
# Write a single animation block into the file
# ------------------------------------------------------------
def write_animation(f, arm, action, scene, start_frame, end_frame,
                    all_deform_bones, bone_index_map):
    frame_count = end_frame - start_frame + 1

    bone_names = [b.name for b in all_deform_bones if arm.pose.bones.get(b.name)]
    num_tracks = len(bone_names)

    # Animation header
    name_bytes = action.name.encode("ascii", errors="ignore")[:32]
    name_bytes = name_bytes.ljust(32, b"\0")
    f.write(name_bytes)

    flags = 1  # looped
    f.write(struct.pack("<I", flags))
    f.write(struct.pack("<H", frame_count))
    f.write(struct.pack("<H", num_tracks))
    f.write(struct.pack("<H", 0))       # numMarkers

    depsgraph = bpy.context.evaluated_depsgraph_get()

    for bone_name in bone_names:
        joint_id = bone_index_map[bone_name]

        f.write(struct.pack("<B", 0))            # type = ROTATION
        f.write(struct.pack("<B", joint_id))
        f.write(struct.pack("<H", frame_count))

        for i, frame in enumerate(range(start_frame, end_frame + 1)):
            scene.frame_set(frame)
            depsgraph.update()

            arm_eval = arm.evaluated_get(depsgraph)
            pbone = arm_eval.pose.bones[bone_name]

            if pbone.parent is None:
                transform = axis_basis_change @ pbone.matrix
            else:
                transform = pbone.parent.matrix.inverted_safe() @ pbone.matrix

            _, rotation, _ = transform.decompose()
            q = rotation

            f.write(struct.pack("<H", i))
            f.write(struct.pack("<B", 0))
            f.write(struct.pack(
                "<hhhh",
                fp12_16(q.w),
                fp12_16(q.x),
                fp12_16(q.y),
                fp12_16(q.z),
            ))

            print(f"[{action.name}] {bone_name} frame {frame}: "
                  f"w={q.w:.4f} x={q.x:.4f} y={q.y:.4f} z={q.z:.4f}")

    print(f"  -> '{action.name}': {frame_count} frames, {num_tracks} tracks")


# ------------------------------------------------------------
# Main export entry point
# ------------------------------------------------------------
def export_animbin(context, filepath, start_frame, end_frame,
                   export_mode, action_frame_ranges):
    arm = context.object
    if not arm or arm.type != "ARMATURE":
        raise RuntimeError("Select an armature object")

    if not arm.animation_data:
        raise RuntimeError("Armature has no animation data")

    scene = context.scene
    original_frame = scene.frame_current
    original_action = arm.animation_data.action
    original_use_nla = arm.animation_data.use_nla

    # Disable NLA so animation_data.action drives the pose directly
    arm.animation_data.use_nla = False

    if export_mode == 'ACTIVE':
        if not arm.animation_data.action:
            raise RuntimeError("Armature has no active action")
        actions_to_export = [
            (arm.animation_data.action, start_frame, end_frame)
        ]

    elif export_mode == 'ALL':
        actions_to_export = []
        for action in bpy.data.actions:
            if action.frame_range[1] > action.frame_range[0]:
                s = int(action.frame_range[0])
                e = min(int(action.frame_range[1]), s + 29)
            else:
                s, e = start_frame, end_frame
            actions_to_export.append((action, s, e))

    elif export_mode == 'SELECTED':
        actions_to_export = []
        for (aname, s, e) in action_frame_ranges:
            action = bpy.data.actions.get(aname)
            if action is None:
                print(f"Warning: action '{aname}' not found, skipping.")
                continue
            actions_to_export.append((action, s, e))

    else:
        raise RuntimeError(f"Unknown export_mode: {export_mode}")

    if not actions_to_export:
        raise RuntimeError("No actions to export.")

    all_deform_bones = [b for b in arm.pose.bones if b.bone.use_deform]
    bone_index_map = {b.name: i for i, b in enumerate(all_deform_bones)}
    num_animations = len(actions_to_export)

    with open(filepath, "wb") as f:
        f.write(b"ANIMBIN")
        f.write(struct.pack("<B", 1))
        f.write(struct.pack("<B", num_animations))

        for (action, s, e) in actions_to_export:
            # Assign action and force evaluation before writing
            arm.animation_data.action = action
            scene.frame_set(s)
            bpy.context.view_layer.update()

            write_animation(f, arm, action, scene, s, e,
                            all_deform_bones, bone_index_map)

    arm.animation_data.action = original_action
    arm.animation_data.use_nla = original_use_nla
    scene.frame_set(original_frame)

    print(f"\nANIMBIN exported: {filepath}")
    print(f"Total animations: {num_animations}")
    for (action, s, e) in actions_to_export:
        print(f"  '{action.name}' [{s}-{e}]")


# ------------------------------------------------------------
# Blender Operator
# ------------------------------------------------------------

class ExportAnimBin(bpy.types.Operator, ExportHelper):
    bl_idname = "export_anim.animbin"
    bl_label = "Export AnimationBin"

    filename_ext = ".animbin"
    filter_glob: StringProperty(default="*.animbin", options={'HIDDEN'})

    export_mode: EnumProperty(
        name="Actions",
        description="Which actions to export",
        items=[
            ('ACTIVE', "Active Only",    "Export only the currently active action"),
            ('ALL',    "All Actions",    "Export every action in the blend file"),
        ],
        default='ACTIVE',
    )

    start_frame: IntProperty(name="Start Frame", default=1)
    end_frame:   IntProperty(name="End Frame",   default=30)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "export_mode")
        if self.export_mode == 'ACTIVE':
            row = layout.row(align=True)
            row.prop(self, "start_frame")
            row.prop(self, "end_frame")
        else:
            layout.label(text="Frame range auto-detected per action.")

    def execute(self, context):
        export_animbin(
            context,
            self.filepath,
            self.start_frame,
            self.end_frame,
            self.export_mode,
            [],
        )
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