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

# a face that's this far off flat gets split into two triangles. the gpu draws a quad as two
# triangles anyway, but the renderer only runs nclip on the first one, so a strongly folded
# quad can have its second half facing the other way to what nclip saw
MAX_QUAD_FOLD_DEGREES = 30.0

# if the obj normals are this close to perpendicular to the face we can't trust them to say
# which side is the front, so fall back to the obj rule (counter-clockwise is the front)
MIN_NORMAL_AGREEMENT = 0.2


def newell_normal(points):
    # newell's method: the area weighted normal of a polygon, works for any vertex count and
    # doesn't care which three verts you pick, unlike a single cross product
    normal = np.zeros(3)
    for i in range(len(points)):
        current = points[i]
        following = points[(i + 1) % len(points)]
        normal[0] += (current[1] - following[1]) * (current[2] + following[2])
        normal[1] += (current[2] - following[2]) * (current[0] + following[0])
        normal[2] += (current[0] - following[0]) * (current[1] + following[1])

    return normal


def triangle_cross(p0, p1, p2):
    return np.cross(p1 - p0, p2 - p0)


def unit(vector):
    length = np.linalg.norm(vector)
    if length == 0:
        return None

    return vector / length


def convert_blender_normal(normal):
    # what this tool used to do to every normal: blender z-up to ps1 -y-up. the vertices were
    # never converted though, so this only helps if the exporter left the normals behind
    return (normal[0], -normal[2], normal[1])


def pick_normal_frame(faces, verts, raw_norms):
    # the normals have to be in the same space as the vertices to say anything about winding.
    # rather than assume, try both and keep whichever one lines up with the actual geometry
    score_as_is = 0.0
    score_converted = 0.0
    for face in faces:
        corners = face["corners"]
        geometric = unit(newell_normal([np.array(verts[c[0]][:3], dtype=float) for c in corners]))
        normal_ids = [c[2] for c in corners if c[2] != -1]
        if geometric is None or not normal_ids:
            continue

        as_is = unit(np.sum([np.array(raw_norms[n]) for n in normal_ids], axis=0))
        converted = unit(np.sum([np.array(convert_blender_normal(raw_norms[n])) for n in normal_ids], axis=0))
        if as_is is not None:
            score_as_is += abs(np.dot(geometric, as_is))
        if converted is not None:
            score_converted += abs(np.dot(geometric, converted))

    return score_converted > score_as_is


def remove_repeated_corners(corners):
    # a corner that repeats a vertex adds no area but does make nclip see a zero area triangle
    # and skip the whole face
    seen = set()
    unique_corners = []
    for corner in corners:
        if corner[0] in seen:
            continue

        seen.add(corner[0])
        unique_corners.append(corner)

    return unique_corners


def triangle_entry(c0, c1, c2):
    # triangles are padded out to the quad format with -1 in the second slot, and the renderer
    # draws them as slots 1, 3, 4
    return ([c0[0], -1, c1[0], c2[0]], [c0[1], -1, c1[1], c2[1]], [c0[2], -1, c1[2], c2[2]])


def quad_entry(a, b, c, d):
    return ([a[0], b[0], c[0], d[0]], [a[1], b[1], c[1], d[1]], [a[2], b[2], c[2], d[2]])


def reverse_winding(items):
    # go round the other way but keep the same first corner, so a face that was already the
    # right way round comes out exactly as it used to
    return items[:1] + items[:0:-1]


def is_front_facing(p0, p1, p2, outward):
    # the renderer's nclip treats a triangle as facing the camera when it is clockwise on
    # screen (mac0 > 0). the gte has y going down the screen and z going into it, which works
    # out as: the cross product of the first three verts points away from the outward normal
    return np.dot(triangle_cross(p0, p1, p2), outward) < 0


def build_face_entries(corners, verts, norms, line_number):
    # corners are (vertex, uv, normal) in the order the .obj listed them, which is the order
    # going round the polygon. returns the meshbin index entries for this face
    corners = remove_repeated_corners(corners)
    if len(corners) < 3:
        print(f"Warning: line {line_number}: face has fewer than 3 distinct verts, dropping it")
        return []

    points = [np.array(verts[c[0]][:3], dtype=float) for c in corners]
    geometric = unit(newell_normal(points))
    if geometric is None:
        print(f"Warning: line {line_number}: face has no area, dropping it")
        return []

    # which side is the outside? the .obj rule is counter-clockwise is the front, but exporters
    # don't all stick to it, so if the face has normals let them decide
    normal_ids = [c[2] for c in corners if c[2] != -1]
    if normal_ids:
        average_normal = unit(np.sum([np.array(norms[n], dtype=float) for n in normal_ids], axis=0))
        agreement = np.dot(geometric, average_normal) if average_normal is not None else 0.0
        if agreement < -MIN_NORMAL_AGREEMENT:
            corners = reverse_winding(corners)
            points = reverse_winding(points)
            geometric = -geometric
        elif agreement <= MIN_NORMAL_AGREEMENT:
            print(f"Warning: line {line_number}: face normals don't say which side is the front, assuming counter-clockwise")

    # corners now go counter-clockwise looking at the outside. the engine wants clockwise
    outward = geometric
    corners = reverse_winding(corners)
    points = reverse_winding(points)

    if len(corners) == 3:
        return [triangle_entry(corners[0], corners[1], corners[2])]

    if len(corners) > 4:
        print(f"Warning: line {line_number}: {len(corners)} sided face, splitting into triangles")
        return [triangle_entry(corners[0], corners[i], corners[i + 1]) for i in range(1, len(corners) - 1)]

    # going round the polygon as a, b, c, d the gpu wants a z shape: a, b, d, c. it draws that
    # as the triangles a, b, d and b, d, c, so the split is always along b-d. try each starting
    # corner and keep one where both halves face the same way as the whole face, which also
    # sorts out concave quads (they have to be split through the inward pointing corner)
    best = None
    for start in range(4):
        a, b, c, d = [(start + i) % 4 for i in range(4)]
        if not is_front_facing(points[a], points[b], points[d], outward) or not is_front_facing(points[b], points[c], points[d], outward):
            continue

        first_half = triangle_cross(points[a], points[b], points[d])
        second_half = triangle_cross(points[b], points[c], points[d])
        fold = np.degrees(np.arccos(np.clip(np.dot(unit(first_half), unit(second_half)), -1.0, 1.0)))
        if best is None or round(fold) < round(best[0]):
            best = (fold, (a, b, c, d))

    if best is not None and best[0] <= MAX_QUAD_FOLD_DEGREES:
        a, b, c, d = best[1]
        return [quad_entry(corners[a], corners[b], corners[d], corners[c])]

    # too bent (or too broken) to send as one quad, so send it as two triangles. split along
    # whichever diagonal leaves the halves facing outwards
    for start in range(2):
        a, b, c, d = [(start + i) % 4 for i in range(4)]
        halves = [(a, b, d), (b, c, d)]
        if all(is_front_facing(points[x], points[y], points[z], outward) for x, y, z in halves):
            break

    print(f"Warning: line {line_number}: quad is not flat or is self intersecting, splitting into triangles")
    return [triangle_entry(corners[x], corners[y], corners[z]) for x, y, z in halves
            if np.linalg.norm(triangle_cross(points[x], points[y], points[z])) > 0]


def resolve_obj_index(token, total_seen):
    # obj indices are 1 based, and negative ones count back from the last one seen
    index = int(token)
    if index < 0:
        return total_seen + index

    return index - 1


def parse_obj_file_with_collision_data(path,texture_size):
    verts = []
    norms = []
    uvs = []
    faces = []
    face_indices = []
    uv_indices = []
    normal_indices = []
    collision_verts = []
    texture_size = int(texture_size)
    is_collision = False
    ONE_ENGINE_METRE = 128
    ONE_FP12 = 4096
    has_skeleton = False
    skeleton_bone_count = 0
    skeleton_bones = []
    bone_id_for_vert_ix = []
    min_coords = []
    max_coords = []
    bsphere_centre = None
    bsphere_radius = None

    # obj indices count every v/vt/vn in the file, collision objects included. these map them
    # to where they ended up in our own lists (None for anything that went to collision)
    vert_lookup = []
    uv_lookup = []
    norm_lookup = []
    raw_norms = []

    with open(path, "r") as f:
        for line_number, line in enumerate(f, 1):
            if line.startswith("o col_"):
                is_collision = True
                collision_verts.append([])
            elif line.startswith("o "):
                is_collision = False

            if line.startswith("v "):
                parts = line.strip().split()
                x, y, z = map(float, parts[1:4])
                if len(parts) >= 7:
                    r, g, b = map(lambda v: int(v), parts[4:7])
                else:
                    r, g, b = 128, 128, 128

                if is_collision:
                    collision_verts[-1].append((int(float(x)*ONE_ENGINE_METRE), int(float(y)*ONE_ENGINE_METRE), int(float(z)*ONE_ENGINE_METRE)))
                    vert_lookup.append(None)
                else:
                    vert_lookup.append(len(verts))
                    verts.append((int(float(x)*ONE_ENGINE_METRE), int(float(y)*ONE_ENGINE_METRE), int(float(z)*ONE_ENGINE_METRE), r, g, b))

            elif line.startswith("vn "):
                if is_collision:
                    norm_lookup.append(None)
                    continue

                # kept as floats for now, see pick_normal_frame for which space they're in
                x, y, z = map(float, line.strip().split()[1:4])
                norm_lookup.append(len(raw_norms))
                raw_norms.append((x, y, z))
            elif line.startswith("vt "):
                if is_collision:
                    uv_lookup.append(None)
                    continue

                u, v = map(float, line.strip().split()[1:3])
                # blender starts bottom left for uvs which is opposite to psx which is top left
                u = int(u*texture_size)-1
                v = int(v*texture_size)-1
                if u < 0: u = 0
                if v < 0: v = 0
                uv_lookup.append(len(uvs))
                uvs.append((u, v))
            elif line.startswith("f "):
                if is_collision:
                    continue

                corners = []
                for vertex in line.strip().split()[1:]:
                    parts = vertex.split("/")
                    v = vert_lookup[resolve_obj_index(parts[0], len(vert_lookup))]
                    vt = uv_lookup[resolve_obj_index(parts[1], len(uv_lookup))] if len(parts) > 1 and parts[1] else -1
                    vn = norm_lookup[resolve_obj_index(parts[2], len(norm_lookup))] if len(parts) > 2 and parts[2] else -1
                    corners.append((v, vt, vn))

                # winding is sorted out once the whole file is read, the normals might not
                # have turned up yet
                faces.append({"corners": corners, "line": line_number})

            elif line.startswith('aabb '):
                parts = line.split()
                min_coords = [int(float(parts[1]) * ONE_ENGINE_METRE),
                            int(float(parts[2]) * ONE_ENGINE_METRE),
                            int(float(parts[3]) * ONE_ENGINE_METRE)]
                max_coords = [int(float(parts[4]) * ONE_ENGINE_METRE),
                            int(float(parts[5]) * ONE_ENGINE_METRE),
                            int(float(parts[6]) * ONE_ENGINE_METRE)]

            elif line.startswith('bsphere '):
                parts = line.split()
                bsphere_centre = [int(float(parts[1]) * ONE_ENGINE_METRE),
                                int(float(parts[2]) * ONE_ENGINE_METRE),
                                int(float(parts[3]) * ONE_ENGINE_METRE)]
                bsphere_radius = int(float(parts[4]) * ONE_ENGINE_METRE)

            elif line.startswith("skel "):
                has_skeleton = True
                skeleton_bone_count = int(line.strip().split()[1])

            # elif line.startswith("bone ") and has_skeleton:
            #     bone_id, name, parent, x, y, z, rotW, rotX, rotY, rotZ = line.strip().split()[1:]
            #     skeleton_bones.append((int(parent), int(float(x)), int(float(y)), int(float(z)), int(float(rotW)), int(float(rotX)), int(float(rotY)), int(float(rotZ))))

            elif line.startswith("bone ") and has_skeleton:
                bone_id, name, parent, x, y, z, rotW, rotX, rotY, rotZ = line.strip().split()[1:]
                skeleton_bones.append((
                    int(parent), 
                    int(float(x) * ONE_ENGINE_METRE),
                    int(float(y) * ONE_ENGINE_METRE),
                    int(float(z) * ONE_ENGINE_METRE),
                    int(float(rotW) * ONE_FP12), 
                    int(float(rotX) * ONE_FP12), 
                    int(float(rotY) * ONE_FP12), 
                    int(float(rotZ) * ONE_FP12)
                ))
                print(f"bone: {bone_id}. x={x} ({int(float(x) * ONE_ENGINE_METRE)}), y={y} ({int(float(y) * ONE_ENGINE_METRE)}) z={z} ({int(float(z) * ONE_ENGINE_METRE)})")

            elif line.startswith("vw ") and has_skeleton:
                bone_id = line.strip().split()[2]
                bone_id_for_vert_ix.append(int(bone_id))

    # normals: get them into the same space as the verts, make them unit length and store them
    # as fp12 (what psyqo::Vec3 expects). this used to int() the raw float, which left every
    # component as -1, 0 or 1
    if pick_normal_frame(faces, verts, raw_norms):
        print("Normals don't match the vertices as they are, converting them from blender z-up")
        raw_norms = [convert_blender_normal(n) for n in raw_norms]

    for i, normal in enumerate(raw_norms):
        normal = unit(np.array(normal, dtype=float))
        if normal is None:
            normal = np.zeros(3)
        raw_norms[i] = tuple(normal)
        norms.append(tuple(int(max(-32768, min(32767, round(c * ONE_FP12)))) for c in normal))

    for face in faces:
        for v_idx, uv_idx, n_idx in build_face_entries(face["corners"], verts, raw_norms, face["line"]):
            face_indices.append(v_idx)
            uv_indices.append(uv_idx)
            normal_indices.append(n_idx)

    num_faces = len(face_indices)

    # a plain .obj (not from our blender exporter) won't have these
    if not min_coords:
        min_coords, max_coords = generate_aabb_for_verts(verts)
    if bsphere_centre is None:
        bsphere_centre = [(min_coords[i] + max_coords[i]) // 2 for i in range(3)]
        bsphere_radius = int(max(np.linalg.norm(np.array(v[:3]) - np.array(bsphere_centre)) for v in verts))

    return verts, norms, uvs, face_indices, uv_indices, normal_indices, num_faces, collision_verts, min_coords, max_coords, has_skeleton, skeleton_bone_count, skeleton_bones, bone_id_for_vert_ix, bsphere_centre, bsphere_radius


def write_meshbin(filename, verts, norms, uvs, indices, uv_indices, normal_indices, num_faces, collision_verts, min_coords, max_coords, has_skeleton, skeleton_bone_count, skeleton_bones, bone_id_for_vert_ix, bsphere_centre, bsphere_radius):
    with open(filename, "wb") as f:
        f.write(b"MESHBIN") # magic
        f.write(struct.pack("<B", 3)) # version
        f.write(struct.pack("<B", 1)) # type

        # subheader
        f.write(struct.pack("<I", len(verts)))
        f.write(struct.pack("<I", len(indices)))
        f.write(struct.pack("<I", num_faces)) 
        f.write(struct.pack("<I", len(norms)))
        f.write(struct.pack("<I", len(uvs)))
        f.write(struct.pack("<B", has_skeleton)) 
        f.write(struct.pack("<B", skeleton_bone_count)) 

        for vert in verts:
            x, y, z = vert[:3]
            f.write(struct.pack("<iii", x, y, z))

        for vert in verts:
            if len(vert) >= 6:
                r, g, b = vert[3:6]
            else:
                r, g, b = 128, 128, 128

            f.write(struct.pack("<BBB", r, g, b))

        for face in indices:
            f.write(struct.pack("<hhhh", *face))

        for x, y, z in norms:
            f.write(struct.pack("<hhh", x, y, z))

        for face in normal_indices:
            f.write(struct.pack("<hhhh", *face))

        for u, v in uvs:
            f.write(struct.pack("<BB", u, v))

        for face in uv_indices:
            f.write(struct.pack("<hhhh", *face))

        # aabb collision box
        f.write(struct.pack("<hhh", *min_coords))
        f.write(struct.pack("<hhh", *max_coords))

        # bounding sphere
        f.write(struct.pack("<hhh", *bsphere_centre))
        f.write(struct.pack("<i", bsphere_radius))

        # skeleton bones
        for bone in skeleton_bones:
            f.write(struct.pack("<b3i4h", *bone))

        # skeleton bone id for vert ix
        for bone_mapping in bone_id_for_vert_ix:
            f.write(struct.pack("<B", bone_mapping))


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {os.path.basename(sys.argv[0])} input.obj output.meshbin texture_size")
        sys.exit(1)
 
    input_obj = sys.argv[1]
    output_bin = sys.argv[2]
    texture_size = sys.argv[3]

    verts, norms, uvs, indices, uv_idx, norm_idx, num_faces, collision_verts, min_coords, max_coords, has_skeleton, skeleton_bone_count, skeleton_bones, bone_id_for_vert_ix, bsphere_centre, bsphere_radius = parse_obj_file_with_collision_data(input_obj, texture_size)
    write_meshbin(output_bin, verts, norms, uvs, indices, uv_idx, norm_idx, num_faces, collision_verts, min_coords, max_coords, has_skeleton, skeleton_bone_count, skeleton_bones, bone_id_for_vert_ix, bsphere_centre, bsphere_radius)
    print(f"Successfully wrote mesh binary to {output_bin}\n")
    print(f"verts: {len(verts)}. indices count: {len(indices)}. faces count: {num_faces}. uv count: {len(uvs)}. bone count: {skeleton_bone_count}")
