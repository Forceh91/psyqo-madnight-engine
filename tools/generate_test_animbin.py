import struct
import math

filepath = "cdrom/assets/test.animbin"

# Animation parameters
numAnimations = 1
anim_name = b"Debug_WaveArm".ljust(32, b"\0")
flags = 1        # looped
length = 60
numTracks = 3
numMarkers = 0

# boneId, name, parentId, axis, amplitude_deg
bones = [
    (5,  "Head",          4, (1, 0, 0),  10),   # gentle nod
    (7,  "LeftShoulder",  3, (0, 0, 1), 15),
    # (8,  "LeftArm",       7, (0, 0, 1), 20),
    # (9,  "LeftForeArm",   8, (0, 0, 1), 25),
    (10, "LeftHand",      9, (0, 1, 0), 10),
]

ROTATION = 0

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

with open(filepath, "wb") as f:
    # Header
    f.write(b"ANIMBIN")
    f.write(struct.pack("<B", 1))
    f.write(struct.pack("<B", numAnimations))

    # Animation block
    f.write(anim_name)
    f.write(struct.pack("<I", flags))
    f.write(struct.pack("<H", length))
    f.write(struct.pack("<H", numTracks))
    f.write(struct.pack("<H", numMarkers))

    for bone_id, name, parent, axis, amp_deg in bones:
        f.write(struct.pack("<B", ROTATION))
        f.write(struct.pack("<B", bone_id))
        f.write(struct.pack("<H", length))

        for frame in range(length):
            t = frame / length * 2 * math.pi
            angle = math.radians(amp_deg) * math.sin(t)

            w, x, y, z = quat_from_axis_angle(axis, angle)

            f.write(struct.pack("<H", frame))
            f.write(struct.pack("<B", ROTATION))
            f.write(struct.pack(
                "<hhhh",
                float_to_fp12_short(w),
                float_to_fp12_short(x),
                float_to_fp12_short(y),
                float_to_fp12_short(z),
            ))

print("Debug animation written:", filepath)
