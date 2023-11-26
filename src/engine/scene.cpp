#include <video.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <stb/stb_image.h>

#include "linear.hpp"
namespace NextVideo {
int Scene::addTexture(const char* path) {
  Texture text;
  text.data = stbi_load(path, &text.width, &text.height, &text.channels, 3);
  VERIFY(text.data != nullptr, "[IO] Error trying to load texture %s\n", path);
  textures.push_back(text);
  return textures.size() - 1;
}
/* SCENE LOADING */

ENGINE_API Scene* sceneCreate() {

  Scene* scene = new Scene;
  auto   plain = scene->addMesh();

  static float plainMesh[] = {
    0, 0, 0, /* POSITION*/ 0, 1, 0, /* NORMAL */ 0, 0, /* UV */
    0, 0, 1, /* POSITION*/ 0, 1, 0, /* NORMAL */ 0, 1, /* UV */
    1, 0, 1, /* POSITION*/ 0, 1, 0, /* NORMAL */ 1, 1, /* UV */
    1, 0, 0, /* POSITION*/ 0, 1, 0, /* NORMAL */ 1, 0, /* UV */
  };

  static unsigned int plainMeshEBO[] = {0, 1, 2, 2, 3, 0};

  plain->type                 = CUSTOM;
  plain->tCustom.meshFormat   = 0;
  plain->tCustom.numVertices  = 4;
  plain->tCustom.numIndices   = 6;
  plain->tCustom.vertexBuffer = plainMesh;
  plain->tCustom.indexBuffer  = plainMeshEBO;
  return scene;
}
} // namespace NextVideo
