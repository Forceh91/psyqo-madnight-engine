import struct
import math

filepath = "cdrom/assets/test.animbin"

# Animation parameters
numAnimations = 1
anim_name = b"Debug_WaveArm".ljust(32, b"\0")
flags = 1
length = 60
numTracks = 3
numMarkers = 0

ROTATION = 0

def float_to_fp12_short(val):
    return int(max(min(val * 4096, 32767), -32768))

def quat_from_axis_angle(axis, angle_rad):
    s = math.sin(angle_rad * 0.5)
    c = math.cos(angle_rad * 0.5)
    # Normalize axis
    length = math.sqrt(axis[0]**2 + axis[1]**2 + axis[2]**2)
    if length > 0:
        axis = (axis[0]/length, axis[1]/length, axis[2]/length)
    return (
        c,
        axis[0] * s,
        axis[1] * s,
        axis[2] * s
    )

# First, let's collect all the data
tracks_data = []

# Track 0: Shoulder
track0_keys = []
for frame in range(length):
    angle = math.radians(45)
    w, x, y, z = quat_from_axis_angle((0, 0, 1), angle)
    track0_keys.append((frame, w, x, y, z))
tracks_data.append((7, track0_keys))

# Track 1: Arm
track1_keys = []
for frame in range(length):
    angle = math.radians(70)
    w, x, y, z = quat_from_axis_angle((0, 1, 0), angle)
    track1_keys.append((frame, w, x, y, z))
tracks_data.append((8, track1_keys))

# Track 2: Forearm
track2_keys = []
for frame in range(length):
    t = frame / length * 2 * math.pi
    angle = math.radians(20) * math.sin(t)
    w, x, y, z = quat_from_axis_angle((0, 1, 0), angle)
    track2_keys.append((frame, w, x, y, z))
tracks_data.append((9, track2_keys))

# Now write the file
with open(filepath, "wb") as f:
    # ===== FILE HEADER =====
    f.write(b"ANIMBIN")          # 7 bytes
    f.write(struct.pack("<B", 1))  # 1 byte - version
    f.write(struct.pack("<B", numAnimations))  # 1 byte
    
    # ===== ANIMATION HEADER =====
    f.write(anim_name)  # 32 bytes
    f.write(struct.pack("<I", flags))  # 4 bytes
    f.write(struct.pack("<H", length))  # 2 bytes
    f.write(struct.pack("<H", numTracks))  # 2 bytes
    f.write(struct.pack("<H", numMarkers))  # 2 bytes
    
    print(f"Header written. File position: {f.tell()}")
    
    # ===== WRITE ALL TRACKS =====
    for track_idx, (joint_id, keys) in enumerate(tracks_data):
        print(f"\nTrack {track_idx}: jointId={joint_id}, numKeys={len(keys)}")
        print(f"  Start position: {f.tell()}")
        
        # Track header
        f.write(struct.pack("<B", ROTATION))  # 1 byte - track type
        f.write(struct.pack("<B", joint_id))  # 1 byte - joint ID
        f.write(struct.pack("<H", len(keys)))  # 2 bytes - num keys
        
        print(f"  After header: {f.tell()}")
        
        # Write all keys for this track
        for frame, w, x, y, z in keys:
            f.write(struct.pack("<H", frame))  # 2 bytes - frame
            f.write(struct.pack("<B", ROTATION))  # 1 byte - key type
            f.write(struct.pack("<hhhh",  # 8 bytes - quaternion
                float_to_fp12_short(w),
                float_to_fp12_short(x),
                float_to_fp12_short(y),
                float_to_fp12_short(z),
            ))
        
        print(f"  After keys: {f.tell()}")

    total_size = f.tell()
    print(f"\nTotal file size: {total_size} bytes")

# Now verify what was written
print("\n=== VERIFICATION ===")
with open(filepath, "rb") as f:
    data = f.read()
    print(f"File size on disk: {len(data)} bytes")
    
    # Skip to after animation header
    offset = 51
    print(f"\nStarting track reading at offset {offset}")
    
    for track_num in range(3):
        print(f"\nTrack {track_num} at offset {offset}:")
        track_type = data[offset]
        joint_id = data[offset + 1]
        num_keys = struct.unpack("<H", data[offset + 2:offset + 4])[0]
        print(f"  type={track_type}, jointId={joint_id}, numKeys={num_keys}")
        
        # Skip this track's header (4 bytes) + all its keys (11 bytes each)
        key_bytes = num_keys * 11
        offset += 4 + key_bytes
        print(f"  Next track should start at offset {offset}")

print(f"\nDebug animation written: {filepath}")