#include "mesh_manager.hh"
#include "../helpers/cdrom.hh"
#include "EASTL/string.h"
#include "psyqo/xprintf.h"
#include "skeleton/skeleton.hh"

LoadedMeshBin MeshManager::mLoadedMeshes[MAX_LOADED_MESHES];

psyqo::Coroutine<> MeshManager::LoadMeshFromCDROM(const char *meshName, MeshBin **meshOut) {
  // make sure we get a valid response at least
  *meshOut = nullptr;

  // is it already loaded?
  auto *pMesh = IsMeshLoaded(meshName);
  if (pMesh != nullptr) {
    *meshOut = pMesh;
    co_return;
  }

  // is there space for this mesh?
  int8_t meshIx = FindSpaceForMesh();
  if (meshIx == -1)
    co_return;

  auto buffer = co_await CDRomHelper::LoadFile(meshName);

  void *data = buffer.data();
  size_t size = buffer.size();
  if (data == nullptr || size == 0) {
    buffer.clear();
    printf("MESH: Failed to load mesh or it has no file size.\n");
    co_return;
  }

  // basic struct setup and blanking out of the meshbin struct
  LoadedMeshBin loaded_mesh = {"", 0};
  loaded_mesh.meshName = meshName;
  __builtin_memset(&loaded_mesh.mesh, 0, sizeof(MeshBin));

  // get ready with our buffer
  unsigned char *ptr = (unsigned char *)data;

  // meshbin header
  // magic value
  loaded_mesh.mesh.magic.assign(reinterpret_cast<char *>(ptr), 7);
  ptr += 7;

  // verify the magic value
  if (loaded_mesh.mesh.magic.compare("MESHBIN") != 0) {
    printf("MESH: Header is invalid. aborting.\n");
    __builtin_memset(&loaded_mesh, 0, sizeof(LoadedMeshBin));
    buffer.clear();
    co_return;
  }

  // version + type
  __builtin_memcpy(&loaded_mesh.mesh.version, ptr++, 1); // 1 bytes
  __builtin_memcpy(&loaded_mesh.mesh.type, ptr++, 1);    // 1 bytes

  // subheader (counts basically)
  __builtin_memcpy(&loaded_mesh.mesh.vertexCount, ptr, sizeof(uint32_t)); // 4 bytes
  ptr += sizeof(uint32_t);

  __builtin_memcpy(&loaded_mesh.mesh.indicesCount, ptr, sizeof(uint32_t)); // 4 bytes
  ptr += sizeof(uint32_t);

  __builtin_memcpy(&loaded_mesh.mesh.facesCount, ptr, sizeof(uint32_t)); // 4 bytes
  ptr += sizeof(uint32_t);

  __builtin_memcpy(&loaded_mesh.mesh.normalsCount, ptr, sizeof(uint32_t)); // 4 bytes
  ptr += sizeof(uint32_t);

  __builtin_memcpy(&loaded_mesh.mesh.uvCount, ptr, sizeof(uint32_t)); // 4 bytes
  ptr += sizeof(uint32_t);

  // skeleton exists and how many bones?
  // v2 onwards has skeleton data
  if (loaded_mesh.mesh.version > 1) {
    __builtin_memcpy(&loaded_mesh.mesh.hasSkeleton, ptr++, sizeof(uint8_t)); // 1 byte
    __builtin_memcpy(&loaded_mesh.mesh.numBones, ptr++, sizeof(uint8_t));    // 1 byte
  }

  // do we have too many faces?
  if (loaded_mesh.mesh.facesCount >= MAX_FACES_PER_MESH) {
    printf("MESH: Mesh has too many faces, aborting load.\n");
    __builtin_memset(&loaded_mesh, 0, sizeof(LoadedMeshBin));
    buffer.clear();
    co_return;
  }

  // read the verts
  size_t verticesSize = sizeof(psyqo::Vec3) * loaded_mesh.mesh.vertexCount;
  loaded_mesh.mesh.vertices = (psyqo::Vec3 *)psyqo_malloc(verticesSize);

  for (int i = 0; i < loaded_mesh.mesh.vertexCount; i++) {
    __builtin_memcpy(&loaded_mesh.mesh.vertices[i].x.value, ptr, sizeof(int32_t));
    ptr += sizeof(int32_t);

    __builtin_memcpy(&loaded_mesh.mesh.vertices[i].y.value, ptr, sizeof(int32_t));
    ptr += sizeof(int32_t);

    __builtin_memcpy(&loaded_mesh.mesh.vertices[i].z.value, ptr, sizeof(int32_t));
    ptr += sizeof(int32_t);
  }

  // read the vert colours data
  size_t verticesPaintSize = sizeof(MeshBinVertexColours) * loaded_mesh.mesh.vertexCount;
  loaded_mesh.mesh.vertexColours = (MeshBinVertexColours *)psyqo_malloc(verticesPaintSize);
  __builtin_memcpy(loaded_mesh.mesh.vertexColours, ptr, verticesPaintSize);
  ptr += verticesPaintSize;

  // read the verts indices
  size_t vertexIndicesSize = sizeof(MeshBinIndex) * loaded_mesh.mesh.indicesCount;
  loaded_mesh.mesh.vertexIndices = (MeshBinIndex *)psyqo_malloc(vertexIndicesSize);
  __builtin_memcpy(loaded_mesh.mesh.vertexIndices, ptr, vertexIndicesSize);
  ptr += vertexIndicesSize;

  // read the normals data
  size_t normalsSize = sizeof(psyqo::Vec3) * loaded_mesh.mesh.normalsCount;
  loaded_mesh.mesh.normals = (psyqo::Vec3 *)psyqo_malloc(normalsSize);

  for (int i = 0; i < loaded_mesh.mesh.normalsCount; i++) {
    __builtin_memcpy(&loaded_mesh.mesh.normals[i].x.value, ptr, sizeof(int16_t));
    ptr += sizeof(int16_t);

    __builtin_memcpy(&loaded_mesh.mesh.normals[i].y.value, ptr, sizeof(int16_t));
    ptr += sizeof(int16_t);

    __builtin_memcpy(&loaded_mesh.mesh.normals[i].z.value, ptr, sizeof(int16_t));
    ptr += sizeof(int16_t);
  }

  size_t normalsIndicesSize = sizeof(MeshBinIndex) * loaded_mesh.mesh.indicesCount;
  loaded_mesh.mesh.normalIndices = (MeshBinIndex *)psyqo_malloc(normalsIndicesSize);
  __builtin_memcpy(loaded_mesh.mesh.normalIndices, ptr, normalsIndicesSize);
  ptr += normalsIndicesSize;

  // read the uv data
  size_t uvSize = sizeof(psyqo::PrimPieces::UVCoords) * loaded_mesh.mesh.uvCount;
  loaded_mesh.mesh.uvs = (psyqo::PrimPieces::UVCoords *)psyqo_malloc(uvSize);
  __builtin_memcpy(loaded_mesh.mesh.uvs, ptr, uvSize);
  ptr += uvSize;

  // read the uv indices
  size_t uvIndicesSize = sizeof(MeshBinIndex) * loaded_mesh.mesh.uvCount;
  loaded_mesh.mesh.uvIndices = (MeshBinIndex *)psyqo_malloc(uvIndicesSize);
  __builtin_memcpy(loaded_mesh.mesh.uvIndices, ptr, uvIndicesSize);
  ptr += uvIndicesSize;

  // load aabb min data
  __builtin_memcpy(&loaded_mesh.mesh.collisionBox.min.x.value, ptr, sizeof(int16_t));
  ptr += sizeof(int16_t);

  __builtin_memcpy(&loaded_mesh.mesh.collisionBox.min.y.value, ptr, sizeof(int16_t));
  ptr += sizeof(int16_t);

  __builtin_memcpy(&loaded_mesh.mesh.collisionBox.min.z.value, ptr, sizeof(int16_t));
  ptr += sizeof(int16_t);

  // load aabb max data
  __builtin_memcpy(&loaded_mesh.mesh.collisionBox.max.x.value, ptr, sizeof(int16_t));
  ptr += sizeof(int16_t);

  __builtin_memcpy(&loaded_mesh.mesh.collisionBox.max.y.value, ptr, sizeof(int16_t));
  ptr += sizeof(int16_t);

  __builtin_memcpy(&loaded_mesh.mesh.collisionBox.max.z.value, ptr, sizeof(int16_t));
  ptr += sizeof(int16_t);

  // load skeleton bones
  if (loaded_mesh.mesh.version > 1 && loaded_mesh.mesh.hasSkeleton) {
    __builtin_memset(&loaded_mesh.mesh.skeleton, 0, sizeof(Skeleton));
    __builtin_memset(&loaded_mesh.mesh.skeleton.bones, 0, sizeof(SkeletonBone) * MAX_BONES);

    // number of bones
    loaded_mesh.mesh.skeleton.numBones = loaded_mesh.mesh.numBones;

    // individual bone data
    for (int32_t i = 0; i < loaded_mesh.mesh.numBones; i++) {
      // parent bone
      __builtin_memcpy(&loaded_mesh.mesh.skeleton.bones[i].parent, ptr++, sizeof(int8_t)); // 1 byte

      // local pos
      __builtin_memcpy(&loaded_mesh.mesh.skeleton.bones[i].localPos.x.value, ptr, sizeof(int32_t)); // 4 bytes
      ptr += sizeof(int32_t);

      __builtin_memcpy(&loaded_mesh.mesh.skeleton.bones[i].localPos.y.value, ptr, sizeof(int32_t)); // 4 bytes
      ptr += sizeof(int32_t);

      __builtin_memcpy(&loaded_mesh.mesh.skeleton.bones[i].localPos.z.value, ptr, sizeof(int32_t)); // 4 bytes
      ptr += sizeof(int32_t);

      // local rotation
      __builtin_memcpy(&loaded_mesh.mesh.skeleton.bones[i].localRotation.w.value, ptr, sizeof(int32_t)); // 4 bytes
      ptr += sizeof(int32_t);

      __builtin_memcpy(&loaded_mesh.mesh.skeleton.bones[i].localRotation.x.value, ptr, sizeof(int32_t)); // 4 bytes
      ptr += sizeof(int32_t);

      __builtin_memcpy(&loaded_mesh.mesh.skeleton.bones[i].localRotation.y.value, ptr, sizeof(int32_t)); // 4 bytes
      ptr += sizeof(int32_t);

      __builtin_memcpy(&loaded_mesh.mesh.skeleton.bones[i].localRotation.z.value, ptr, sizeof(int32_t)); // 4 bytes
      ptr += sizeof(int32_t);

      // mark as dirty initially
      loaded_mesh.mesh.skeleton.bones[i].isDirty = true;
    }
  }

  // mark mesh as loaded
  loaded_mesh.isLoaded = true;

  // store in loaded meshes
  mLoadedMeshes[meshIx] = loaded_mesh;

  // free the data
  buffer.clear();

  // give back the pointer to this mesh
  *meshOut = &mLoadedMeshes[meshIx].mesh;

  printf("MESH: Successfully loaded mesh of %d bytes into memory.\n", size);
}

MeshBin *MeshManager::IsMeshLoaded(const char *meshName) {
  using FixedString = eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN>;
  FixedString eastl_mesh_name(meshName);

  LoadedMeshBin *loadedMesh = nullptr;
  for (int i = 0; i < MAX_LOADED_MESHES; i++) {
    // find the first loaded mesh that matches this mesh_name
    loadedMesh = &mLoadedMeshes[i];
    if (loadedMesh && eastl_mesh_name == FixedString(loadedMesh->meshName)) {
      return &loadedMesh->mesh;
    }
  }

  // can't find a mesh with this file name
  return nullptr;
}

int8_t MeshManager::FindSpaceForMesh(void) {
  for (int8_t i = 0; i < MAX_LOADED_MESHES; i++) {
    // return the first mesh that isn't loaded
    if (mLoadedMeshes[i].isLoaded == false)
      return i;
  }

  // no space in the mesh manager for it
  return -1;
}

void MeshManager::UnloadMesh(const char *mesh_name) {
  using FixedString = eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN>;
  FixedString eastl_mesh_name(mesh_name);

  LoadedMeshBin *loaded_mesh = nullptr;
  for (int i = 0; i < MAX_LOADED_MESHES; i++) {
    // find the first loaded mesh that matches this mesh_name
    loaded_mesh = &mLoadedMeshes[i];
    if (loaded_mesh && eastl_mesh_name == FixedString(loaded_mesh->meshName)) {
      __builtin_memset(loaded_mesh, 0, sizeof(LoadedMeshBin));
      break;
    }
  }
}

void MeshManager::GetMeshFromName(const char *meshName, MeshBin **meshOut) { *meshOut = IsMeshLoaded(meshName); }

void MeshManager::Dump(void) {
  // clear out every instance of loaded_mesh, putting it back to zero
  for (int8_t i = 0; i < MAX_LOADED_MESHES; i++) {
    mLoadedMeshes[i] = {"", false};
    __builtin_memset(&mLoadedMeshes[i].mesh, 0, sizeof(MeshBin));
  }
}
