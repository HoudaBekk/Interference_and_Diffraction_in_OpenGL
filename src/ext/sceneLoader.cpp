#include "../engine/engine.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../engine/linear.hpp"

namespace NextVideo {
namespace SceneLoader {

struct LoaderCache {
  std::unordered_map<int, int>         meshCache;
  std::unordered_map<std::string, int> textureCache;
  std::unordered_map<int, int>         materialCache;

  void clear() {
    meshCache.clear();
    textureCache.clear();
    materialCache.clear();
  }
};

LoaderCache    cache;
ENGINE_API int processTexture(const std::string& path, Scene* tracerScene) {
  auto it = cache.textureCache.find(path);
  if (it != cache.textureCache.end()) return it->second;
  int texture = tracerScene->addTexture(path.c_str());

  LOG("[LOADER] Texture created %d\n", texture);
  return cache.textureCache[path] = texture;
}
ENGINE_API int processMaterial(int materialIdx, const aiScene* scene, Scene* tracerScene) {
  auto it = cache.materialCache.find(materialIdx);
  if (it != cache.materialCache.end()) return it->second;

  aiMaterial* mat = scene->mMaterials[materialIdx];


  auto traceMaterialPtr = tracerScene->addMaterial();

  mat->Get(AI_MATKEY_COLOR_DIFFUSE, traceMaterialPtr->kd);
  mat->Get(AI_MATKEY_COLOR_AMBIENT, traceMaterialPtr->ka);
  mat->Get(AI_MATKEY_COLOR_SPECULAR, traceMaterialPtr->ks);

  static aiTextureType types[] = {aiTextureType_DIFFUSE, aiTextureType_SPECULAR};
  //TODO: Magic number
  for (int type = 0; type < 2; type++) {
    for (int i = 0; i < mat->GetTextureCount(types[type]); i++) {
      aiString path;
      mat->GetTexture(types[type], i, &path);
      switch (type) {
        case 0: traceMaterialPtr->albedoTexture = processTexture(path.C_Str(), tracerScene); break;
        case 1: traceMaterialPtr->specularTexture = processTexture(path.C_Str(), tracerScene); break;
      }
    }
  }

  LOG("[LOADER] Material created %d\n", (int)traceMaterialPtr);
  return cache.materialCache[materialIdx] = (int)traceMaterialPtr;
}
ENGINE_API int processMesh(int meshIdx, const aiScene* scene, Scene* tracerScene) {

  auto it = cache.meshCache.find(meshIdx);
  if (it != cache.meshCache.end()) return it->second;

  aiMesh* mesh = scene->mMeshes[meshIdx];

  int vbo_count = mesh->mNumVertices;

  float* vbo_data = (float*)malloc(MESH_FORMAT_SIZE[MESH_FORMAT_DEFAULT] * sizeof(float) * vbo_count);

  int t = 0;
  for (int i = 0; i < vbo_count; i++) {
    vbo_data[t++] = mesh->mVertices[i].x;
    vbo_data[t++] = mesh->mVertices[i].y;
    vbo_data[t++] = mesh->mVertices[i].z;

    vbo_data[t++] = mesh->mNormals[i].x;
    vbo_data[t++] = mesh->mNormals[i].y;
    vbo_data[t++] = mesh->mNormals[i].z;

    if (mesh->mTextureCoords[0] != 0) {
      vbo_data[t++] = mesh->mTextureCoords[0][i].x;
      vbo_data[t++] = mesh->mTextureCoords[0][i].y;
    } else {
      vbo_data[t++] = 0;
      vbo_data[t++] = 0;
    }
  }
  if (t != (vbo_count * MESH_FORMAT_SIZE[MESH_FORMAT_DEFAULT])) {
    LOG("VBO Buffer overflow\n");
    exit(1);
  }

  int ebo_count = 0;
  for (int i = 0; i < mesh->mNumFaces; i++) { ebo_count += mesh->mFaces[i].mNumIndices; }

  unsigned int* ebo_data = (unsigned int*)malloc(sizeof(unsigned int) * ebo_count);

  t = 0;
  for (int i = 0; i < mesh->mNumFaces; i++) {
    for (int j = 0; j < mesh->mFaces[i].mNumIndices; j++) {
      ebo_data[t++] = mesh->mFaces[i].mIndices[j];
    }
  }

  if (t != (ebo_count)) {
    LOG("EBO Buffer overflow\n");
    exit(1);
  }

  auto ptrMeshTracer = tracerScene->addMesh();

  ptrMeshTracer->type                 = CUSTOM;
  ptrMeshTracer->tCustom.numIndices   = ebo_count;
  ptrMeshTracer->tCustom.numVertices  = vbo_count;
  ptrMeshTracer->tCustom.vertexBuffer = vbo_data;
  ptrMeshTracer->tCustom.indexBuffer  = ebo_data;

  LOG("[LOADER] Mesh created %d\n", (int)ptrMeshTracer);
  return cache.meshCache[meshIdx] = (int)ptrMeshTracer;
}


ENGINE_API glm::mat4 toMat(aiMatrix4x4& n) {
  glm::mat4 mat;
  mat[0] = glm::vec4(n.a1, n.a2, n.a3, n.a4);
  mat[1] = glm::vec4(n.b1, n.b2, n.b3, n.b4);
  mat[2] = glm::vec4(n.c1, n.c2, n.c3, n.c4);
  mat[3] = glm::vec4(n.d1, n.d2, n.d3, n.d4);
  return mat;
}

ENGINE_API void processNode(aiNode* node, const aiScene* scene, Scene* tracerScene, aiMatrix4x4 parentTransform) {

  aiMatrix4x4 nodeTransform = parentTransform * node->mTransformation;

  Stage* stage = tracerScene->currentStage();
  for (unsigned int i = 0; i < node->mNumMeshes; i++) {
    int meshTracer    = processMesh(node->mMeshes[i], scene, tracerScene);
    int materialTrace = processMaterial(scene->mMeshes[node->mMeshes[i]]->mMaterialIndex, scene, tracerScene);

    aiVector3t<float> pos, scale, rot;
    node->mTransformation.Decompose(scale, rot, pos);

    auto obj = stage->addObject();

    obj->mesh     = meshTracer;
    obj->material = materialTrace;

    auto instanceTracer = stage->addObjectGroup();

    instanceTracer->object = obj;
    instanceTracer->transforms.push_back(lin::translate(pos.x, pos.y, pos.z));

    LOG("[LOADER] Object created %d\n", (int)obj);
  }
  // then do the same for each of its children
  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    processNode(node->mChildren[i], scene, tracerScene, nodeTransform);
  }
}

ENGINE_API int _sceneLoadOBJ(const char* path, Scene* scene) {
  cache.clear();
  Assimp::Importer importer;
  const aiScene*   scn = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
  if (!scn || scn->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scn->mRootNode) {
    LOG("[LOADER] Error processing mesh\n");
    return 1;
  }
  aiMatrix4x4 id;
  id.a1 = 1;
  id.b2 = 1;
  id.c3 = 1;
  id.d4 = 1;
  processNode(scn->mRootNode, scn, scene, id);
  LOG("[LOADER] Mesh processing successful\n");
  return 0;
}
} // namespace SceneLoader
}
