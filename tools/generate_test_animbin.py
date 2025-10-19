import struct
import math

filepath = "cdrom/assets/test.animbin"

# Animation parameters
numAnimations = 1
anim_name = b"WaveArm".ljust(32, b"\0")
flags = 1        # looped
length = 60      # frames
numTracks = 4    # LeftShoulder, LeftArm, LeftForeArm, LeftHand
numMarkers = 0

# Bones: id, name, parent
bones = [
    (5, "Head", 4),
    (7, "LeftShoulder", 3),
    (8, "LeftArm", 7),
    (9, "LeftForeArm", 8),
    (10, "LeftHand", 9),
]

# KeyType constants
ROTATION = 0
TRANSLATION = 1

def float_to_fp12_short(val):
    """Convert [-1,1] float to int16 fixed point"""
    return int(max(min(val * 4096, 32767), -32768))

def quat_from_axis_angle(axis, angle_rad):
    """Simple rotation around axis (x,y,z), returns (w,x,y,z)"""
    s = math.sin(angle_rad / 2)
    return (math.cos(angle_rad / 2), axis[0]*s, axis[1]*s, axis[2]*s)

with open(filepath, "wb") as f:
    # Header
    f.write(b"ANIMBIN")          # 7 bytes magic
    f.write(struct.pack("<B", 1)) # version
    f.write(struct.pack("<B", numAnimations))

    # Animation
    f.write(anim_name)
    f.write(struct.pack("<I", flags))
    f.write(struct.pack("<H", length))
    f.write(struct.pack("<H", numTracks))
    f.write(struct.pack("<H", numMarkers))

    # Tracks (one per bone)
    for bone_id, name, parent in bones:
        f.write(struct.pack("<B", ROTATION))  # type
        f.write(struct.pack("<B", bone_id))   # jointId
        f.write(struct.pack("<H", length))    # numKeys

        # Each bone has slightly offset oscillation
        offset = (bone_id - 5) * math.pi / 8

        for frame in range(length):
            angle_rad = math.radians(30) * math.sin(frame / length * 2 * math.pi + offset)
            w, x, y, z = quat_from_axis_angle((0,0,1), angle_rad)
            
            f.write(struct.pack("<H", frame))
            f.write(struct.pack("<B", ROTATION))
            f.write(struct.pack("<hhhh",
                                float_to_fp12_short(w),
                                float_to_fp12_short(x),
                                float_to_fp12_short(y),
                                float_to_fp12_short(z)))

    # No markers

print("Dynamic arm waving ANIMBIN created:", filepath)
