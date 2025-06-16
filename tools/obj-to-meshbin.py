import struct
import sys
import os

import numpy as np

def generate_aabb_for_verts(verts):
    min_coords = [sys.maxsize, sys.maxsize, sys.maxsize]
    max_coords = [-sys.maxsize, -sys.maxsize, -sys.maxsize]

    for vert in verts:
        x, y, z = vert[:3]
        if x < min_coords[0]:
            min_coords[0] = x
        if y < min_coords[1]:
            min_coords[1] = y
        if z < min_coords[2]:
            min_coords[2] = z

        if x > max_coords[0]:
            max_coords[0] = x
        if y > max_coords[1]:
            max_coords[1] = y
        if z > max_coords[2]:
            max_coords[2] = z

    return min_coords, max_coords

def reorder_face_clockwise_z(v_idx, uv_idx, n_idx, verts):
    def get_3d(i):
        v = verts[i]
        if len(v) < 3:
            raise ValueError(f"Vertex {i} is malformed: {v}")
        return np.array(v[:3])  # Only take the first 3 components (x, y, z)

    # Get the 3D coordinates for the first three vertices (v0, v1, v2)
    p0 = get_3d(v_idx[0])
    p1 = get_3d(v_idx[1])
    p2 = get_3d(v_idx[2])
    
    # Calculate the normal using the cross product of two edge vectors
    normal = np.cross(p1 - p0, p2 - p0)
      
    # this is incredibly specific to the blender .obj format
    # basically .obj is counterclockwise verts and we want clockwise Z verts
    v_idx = [v_idx[0], v_idx[3], v_idx[1], v_idx[2]]
    uv_idx = [uv_idx[0], uv_idx[3], uv_idx[1], uv_idx[2]]
    n_idx = [n_idx[0], n_idx[3], n_idx[1], n_idx[2]]

    return v_idx, uv_idx, n_idx


def parse_obj_file_with_collision_data(path,texture_size):
    verts = []
    norms = []
    uvs = []
    face_indices = []
    uv_indices = []
    normal_indices = []
    collision_verts = []
    num_faces = 0
    texture_size = int(texture_size)
    is_collision = False
    total_collision_verts = 0
    total_collision_faces = 0
    total_collision_uvs = 0
    ONE = 128

    with open(path, "r") as f:
        for line in f:
            if line.startswith("o col_"):
                is_collision = True
                collision_verts.append([])
            elif line.startswith("o "):
                is_collision = False

            if line.startswith("v "):
                parts = line.strip().split()
                x, y, z = map(float, parts[1:4])
                if len(parts) >= 7:
                    r, g, b = map(lambda v: int(float(v) * 128), parts[4:7])
                else:
                    r, g, b = -1, -1, -1  # or 0s if that's your neutral color
                if is_collision:
                    collision_verts[-1].append((int(float(x)*ONE), int(float(y)*ONE), int(float(z)*ONE)))
                    total_collision_verts += 1
                else:
                    verts.append((int(float(x)*ONE), int(float(y)*ONE), int(float(z)*ONE), r, g, b))

            elif line.startswith("vn "):
                if is_collision: continue
                
                _, x, y, z = line.strip().split()

                # Convert Blender Z-up to PS1 -Y-up (Z-forward)
                # when you change forward/up axis in blender export it doesn't adjust the normals lol
                converted_norm = (float(x), -float(z), float(y))
                norms.append((int(converted_norm[0]), int(converted_norm[1]), int((converted_norm[2]))))
            elif line.startswith("vt "):
                if is_collision:
                    total_collision_uvs += 1
                    continue

                _, u, v = line.strip().split()
                # blender starts bottom left for uvs which is opposite to psx which is top left
                u = int(float(u)*texture_size)-1
                v = int(float(v)*texture_size)-1
                if u < 0: u = 0
                if v < 0: v = 0
                uvs.append((u, v))
            elif line.startswith("f "):
                if is_collision:
                    total_collision_faces += 1
                    continue

                face = line.strip().split()[1:]
                v_idx = []
                uv_idx = []
                n_idx = []

                for vertex in face:
                    parts = vertex.split("/")
                    v = int(parts[0]) - total_collision_verts -1
                    vt = int(parts[1]) - total_collision_uvs - 1 if len(parts) > 1 and parts[1] else -1
                    vn = int(parts[2]) - total_collision_faces - 1 if len(parts) > 2 and parts[2] else -1

                    v_idx.append(v)
                    uv_idx.append(vt)
                    n_idx.append(vn)

                if len(v_idx) == 4:
                    # Reorder for PS1 Z-shaped clockwise
                    v_idx, uv_idx, n_idx = reorder_face_clockwise_z(v_idx, uv_idx, n_idx, verts)

                    face_indices.append(v_idx)
                    uv_indices.append(uv_idx)
                    normal_indices.append(n_idx)

                    num_faces += 1
                elif len(v_idx) == 3:
                    print(f"Skipping triangle: {v_idx}")                

    return verts, norms, uvs, face_indices, uv_indices, normal_indices, num_faces, collision_verts


def write_meshbin(filename, verts, norms, uvs, indices, uv_indices, normal_indices, num_faces, collision_verts):
    with open(filename, "wb") as f:
        f.write(struct.pack("<I", len(verts)))
        f.write(struct.pack("<I", len(indices)))
        f.write(struct.pack("<I", num_faces)) 

        for vert in verts:
            x, y, z = vert[:3]
            f.write(struct.pack("<iii", x, y, z))

        for vert in verts:
            if len(vert) >= 6:
                r, g, b = vert[3:6]
            else:
                r, g, b = -1, -1, -1
            f.write(struct.pack("<hhh", r, g, b))

        for face in indices:
            f.write(struct.pack("<hhhh", *face))

        f.write(struct.pack("<I", len(norms)))
        for x, y, z in norms:
            f.write(struct.pack("<hhh", x, y, z))

        for face in normal_indices:
            f.write(struct.pack("<hhhh", *face))

        f.write(struct.pack("<I", len(uvs)))
        for u, v in uvs:
            f.write(struct.pack("<BB", u, v))

        for face in uv_indices:
            f.write(struct.pack("<hhhh", *face))

        min_coords, max_coords = generate_aabb_for_verts(verts)
        f.write(struct.pack("<hhh", *min_coords))
        f.write(struct.pack("<hhh", *max_coords))


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {os.path.basename(sys.argv[0])} input.obj output.meshbin texture_size")
        sys.exit(1)
 
    input_obj = sys.argv[1]
    output_bin = sys.argv[2]
    texture_size = sys.argv[3]

    verts, norms, uvs, indices, uv_idx, norm_idx, num_faces, collision_verts = parse_obj_file_with_collision_data(input_obj, texture_size)
    write_meshbin(output_bin, verts, norms, uvs, indices, uv_idx, norm_idx, num_faces, collision_verts)
    print(f"Successfully wrote mesh binary to {output_bin}\n")
    print(f"verts: {len(verts)}. indices count: {len(indices)}. faces count: {num_faces}. uv count: {len(uvs)}")
