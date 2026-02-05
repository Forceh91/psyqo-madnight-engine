import struct
import math

filepath = "cdrom/assets/test.animbin"

# Animation parameters
numAnimations = 1
anim_name = b"Idle_20TrackArmsDownDelta".ljust(32, b"\0")
flags = 1        # looped
length = 60
ROTATION = 0

# Bones: (bone_id, name, axis, amp_deg, initialLocalRotation quaternion)
# Example: initialLocalRotation = T-pose quaternion exported from Blender
bones = [
    (0,  "Hips",        (0,0,0), 0, (1,0,0,0)),
    (1,  "Spine",       (1,0,0), 2, (1,0,0,0)),
    (2,  "Spine1",      (1,0,0), 2, (1,0,0,0)),
    (3,  "Spine2",      (1,0,0), 2, (1,0,0,0)),
    (4,  "Neck",        (1,0,0), 2, (1,0,0,0)),
    (5,  "Head",        (1,0,0), 5, (1,0,0,0)),
    # Left arm
    (7,  "LeftShoulder",(1,0.5,0), 20, (1,0.3,0,0)),  # T-pose flared
    (8,  "LeftArm",     (1,0.5,0), 10, (1,0,0.3,0)),
    (9,  "LeftForeArm", (1,0,1), 5, (1,0,0,0)),
    (10, "LeftHand",    (1,1,0), 3, (1,0,0,0)),
    # Right arm
    (19, "RightShoulder",(1,0,0), 0, (1,0,0,0)),
    (20, "RightArm",    (0,0,1), 2, (1,0,0,0)),
    (21, "RightForeArm",(0,0,1), 5, (1,0,0,0)),
    (22, "RightHand",   (0,1,0), 3, (1,0,0,0)),
    # Legs
    (31, "LeftUpLeg",   (1,0,0), 1, (1,0,0,0)),
    (32, "LeftLeg",     (1,0,0), 1, (1,0,0,0)),
    (33, "LeftFoot",    (1,0,0), 1, (1,0,0,0)),
    (36, "RightUpLeg",  (1,0,0), 1, (1,0,0,0)),
    (37, "RightLeg",    (1,0,0), 1, (1,0,0,0)),
    (38, "RightFoot",   (1,0,0), 1, (1,0,0,0)),
]

numTracks = len(bones)

def float_to_fp12_short(val):
    return int(max(min(val * 4096, 32767), -32768))

def quat_from_axis_angle(axis, angle_rad):
    s = math.sin(angle_rad * 0.5)
    return (
        math.cos(angle_rad * 0.5),
        axis[0] * s,
        axis[1] * s,
        axis[2] * s
    )

def quat_conjugate(q):
    w, x, y, z = q
    return (w, -x, -y, -z)

def quat_mult(a, b):
    # quaternion multiplication: a * b
    w1, x1, y1, z1 = a
    w2, x2, y2, z2 = b
    w = w1*w2 - x1*x2 - y1*y2 - z1*z2
    x = w1*x2 + x1*w2 + y1*z2 - z1*y2
    y = w1*y2 - x1*z2 + y1*w2 + z1*x2
    z = w1*z2 + x1*y2 - y1*x2 + z1*w2
    return (w, x, y, z)

with open(filepath, "wb") as f:
    # Header
    f.write(b"ANIMBIN")
    f.write(struct.pack("<B", 1))  # version
    f.write(struct.pack("<B", numAnimations))

    # Animation block
    f.write(anim_name)
    f.write(struct.pack("<I", flags))
    f.write(struct.pack("<H", length))
    f.write(struct.pack("<H", numTracks))
    f.write(struct.pack("<H", 0))  # no markers

    for bone_id, name, axis, amp_deg, initialQuat in bones:
        f.write(struct.pack("<B", ROTATION))
        f.write(struct.pack("<B", bone_id))
        f.write(struct.pack("<H", length))

        for frame in range(length):
            t = frame / length * 2 * math.pi
            phase_offset = bone_id * 0.2  # subtle idle variation
            angle = math.radians(amp_deg) * math.sin(t + phase_offset)
            
            animQuat = quat_from_axis_angle(axis, angle)
            # Delta should be: inverse(bindPose) * anim
            deltaQuat = quat_mult(quat_conjugate(initialQuat), animQuat)  # Swapped order!

            # write keyframe
            f.write(struct.pack("<H", frame))
            f.write(struct.pack("<B", ROTATION))
            f.write(struct.pack(
                "<hhhh",
                float_to_fp12_short(deltaQuat[0]),
                float_to_fp12_short(deltaQuat[1]),
                float_to_fp12_short(deltaQuat[2]),
                float_to_fp12_short(deltaQuat[3]),
            ))

print("Idle 20-track arms-down delta animation written to test.animbin")
