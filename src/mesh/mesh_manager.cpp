#include "mesh_manager.hh"
#include "../helpers/cdrom.hh"

LOADED_MESH MeshManager::m_loaded_meshes[MAX_LOADED_MESHES];

psyqo::Coroutine<> MeshManager::LoadMeshFromCDROM(const char *meshName, MESH **meshOut)
{
    // make sure we get a valid response at least
    *meshOut = nullptr;

    // is it already loaded?
    MESH *p_mesh = is_mesh_loaded(meshName);
    if (p_mesh != nullptr)
    {
        *meshOut = p_mesh;
        co_return;
    }

    // is there space for this mesh?
    int8_t mesh_ix = find_space_for_mesh();
    if (mesh_ix == -1)
        co_return;

    auto buffer = co_await CDRomHelper::LoadFile(meshName);

    void *data = buffer.data();
    size_t size = buffer.size();
    if (data == nullptr || size == 0)
    {
        buffer.clear();
        printf("MESH: Failed to load mesh or it has no file size.\n");
        co_return;
    }

    // basic struct setup
    LOADED_MESH loaded_mesh = {0};
    loaded_mesh.mesh_name = meshName;
    __builtin_memset(&loaded_mesh.mesh, 0, sizeof(MESH));

    // get ready with our buffer
    unsigned char *ptr = (unsigned char *)data;

    // read the model file header, which is the vertex/indices/face count
    __builtin_memcpy(&loaded_mesh.mesh.vertex_count, ptr, 4);
    ptr += 4;

    __builtin_memcpy(&loaded_mesh.mesh.indices_count, ptr, 4);
    ptr += 4;

    __builtin_memcpy(&loaded_mesh.mesh.faces_num, ptr, 4);
    ptr += 4;

    // do we have too many faces?
    if (loaded_mesh.mesh.faces_num >= MAX_FACES_PER_MESH)
    {
        printf("MESH: Mesh has too many faces, aborting load.\n");
        __builtin_memset(&loaded_mesh, 0, sizeof(LOADED_MESH));
        buffer.clear();
        co_return;
    }

    // read the verts
    size_t vertices_size = sizeof(psyqo::Vec3) * loaded_mesh.mesh.vertex_count;
    loaded_mesh.mesh.vertices = (psyqo::Vec3 *)psyqo_malloc(vertices_size);

    int32_t x, y, z;
    for (int i = 0; i < loaded_mesh.mesh.vertex_count; i++)
    {
        __builtin_memcpy(&x, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        __builtin_memcpy(&y, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        __builtin_memcpy(&z, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        loaded_mesh.mesh.vertices[i].x.value = x;
        loaded_mesh.mesh.vertices[i].y.value = y;
        loaded_mesh.mesh.vertices[i].z.value = z;
    }

    // read the verts paint data
    size_t vertices_paint_size = sizeof(VERTEX_PAINT) * loaded_mesh.mesh.vertex_count;
    loaded_mesh.mesh.vertex_paint = (VERTEX_PAINT *)psyqo_malloc(vertices_paint_size);
    __builtin_memcpy(loaded_mesh.mesh.vertex_paint, ptr, vertices_paint_size);
    ptr += vertices_paint_size;

    // read the verts indices
    size_t indices_size = sizeof(INDEX) * loaded_mesh.mesh.indices_count;
    loaded_mesh.mesh.indices = (INDEX *)psyqo_malloc(indices_size);
    __builtin_memcpy(loaded_mesh.mesh.indices, ptr, indices_size);
    ptr += indices_size;

    // read the normals count
    int32_t normal_count;
    __builtin_memcpy(&normal_count, ptr, sizeof(int32_t));
    ptr += sizeof(int32_t);

    // read the normals data
    size_t normals_size = sizeof(psyqo::Vec3) * normal_count;
    loaded_mesh.mesh.normals = (psyqo::Vec3 *)psyqo_malloc(normals_size);

    for (int i = 0; i < normal_count; i++)
    {
        __builtin_memcpy(&x, ptr, sizeof(int16_t));
        ptr += sizeof(int16_t);

        __builtin_memcpy(&y, ptr, sizeof(int16_t));
        ptr += sizeof(int16_t);

        __builtin_memcpy(&z, ptr, sizeof(int16_t));
        ptr += sizeof(int16_t);

        loaded_mesh.mesh.normals[i].x.value = x;
        loaded_mesh.mesh.normals[i].y.value = y;
        loaded_mesh.mesh.normals[i].z.value = z;
    }

    size_t normal_indices_size = sizeof(INDEX) * loaded_mesh.mesh.indices_count;
    loaded_mesh.mesh.normal_indices = (INDEX *)psyqo_malloc(normal_indices_size);
    __builtin_memcpy(loaded_mesh.mesh.normal_indices, ptr, normal_indices_size);
    ptr += normal_indices_size;

    // read the UV count
    int32_t uv_count;
    __builtin_memcpy(&uv_count, ptr, sizeof(int32_t));
    ptr += sizeof(int32_t);

    // read the uv data
    size_t uvs_size = sizeof(psyqo::PrimPieces::UVCoords) * uv_count;
    loaded_mesh.mesh.uvs = (psyqo::PrimPieces::UVCoords *)psyqo_malloc(uvs_size);
    __builtin_memcpy(loaded_mesh.mesh.uvs, ptr, uvs_size);
    ptr += uvs_size;

    // read the uv indices
    size_t uv_indices_size = sizeof(INDEX) * loaded_mesh.mesh.indices_count;
    loaded_mesh.mesh.uv_indices = (INDEX *)psyqo_malloc(uv_indices_size);
    __builtin_memcpy(loaded_mesh.mesh.uv_indices, ptr, uv_indices_size);
    ptr += uv_indices_size;

    // mark mesh as loaded
    loaded_mesh.is_loaded = true;

    // store in loaded meshes
    m_loaded_meshes[mesh_ix] = loaded_mesh;

    // free the data
    buffer.clear();

    // give back the pointer to this mesh
    *meshOut = &m_loaded_meshes[mesh_ix].mesh;

    printf("MESH: Successfully loaded mesh of %d bytes into memory.\n", size);
}

MESH *MeshManager::is_mesh_loaded(const char *mesh_name)
{
    using FixedString = eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN>;
    FixedString eastl_mesh_name(mesh_name);

    LOADED_MESH *loaded_mesh = nullptr;
    for (int i = 0; i < MAX_LOADED_MESHES; i++)
    {
        // find the first loaded mesh that matches this mesh_name
        loaded_mesh = &m_loaded_meshes[i];
        if (loaded_mesh && eastl_mesh_name == FixedString(loaded_mesh->mesh_name))
        {
            return &loaded_mesh->mesh;
        }
    }

    // can't find a mesh with this file name
    return nullptr;
}

int8_t MeshManager::find_space_for_mesh(void)
{
    for (int8_t i = 0; i < MAX_LOADED_MESHES; i++)
    {
        // return the first mesh that isn't loaded
        if (m_loaded_meshes->is_loaded == false)
            return i;
    }

    // no space in the mesh manager for it
    return -1;
}

void MeshManager::unload_mesh(const char *mesh_name)
{
    using FixedString = eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN>;
    FixedString eastl_mesh_name(mesh_name);

    LOADED_MESH *loaded_mesh = nullptr;
    for (int i = 0; i < MAX_LOADED_MESHES; i++)
    {
        // find the first loaded mesh that matches this mesh_name
        loaded_mesh = &m_loaded_meshes[i];
        if (loaded_mesh && eastl_mesh_name == FixedString(loaded_mesh->mesh_name))
        {
            __builtin_memset(loaded_mesh, 0, sizeof(LOADED_MESH));
            break;
        }
    }
}

// uint8_t MeshManager::GetMeshesOfType(const MeshType &meshType, LOADED_MESH *meshes[])
// {
//     LOADED_MESH *meshesOfType[MAX_LOADED_MESHES];
//     uint8_t count = 0;
//     for (uint8_t i = 0; i < MAX_LOADED_MESHES; i++)
//     {
//         if (!m_loaded_meshes[i].is_loaded || m_loaded_meshes[i].meshType != meshType)
//             continue;

//         meshes[count++] = &m_loaded_meshes[i];
//     }

//     return count;
// }
