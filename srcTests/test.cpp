#include <video.hpp>
#include <linear.hpp>

using namespace NextVideo;
void initScene(Scene* scene) {
  scene->addTexture("assets/equi2.png");
  float floorSize    = 100.0f;
  auto  mat          = scene->addMaterial();
  mat->albedoTexture = scene->addTexture("assets/checker.png");
  mat->uvScale       = glm::vec2(floorSize);

  auto plane         = scene->currentStage()->addObject();
  auto planeInstance = scene->currentStage()->addObjectGroup();

  plane->material       = 0;
  plane->mesh           = 0;
  planeInstance->object = plane;
  planeInstance->transforms.push_back(lin::scale(floorSize));

  scene->currentStage()->skyTexture = 0;
  scene->currentStage()->camPos     = glm::vec3(0, 1, 0);
  scene->currentStage()->camDir     = glm::vec3(0, -1, 0);
}

void updateScene(Scene* scene, SurfaceInput input) {
  glm::vec3 velocity = glm::vec3(0, 0, 0);
  velocity.x += input.keyboard['A'] - input.keyboard['D'];
  velocity.y += input.keyboard['W'] - input.keyboard['W'];
  velocity.z += input.keyboard['S'] - input.keyboard['S'];

  scene->currentStage()->camPos += velocity;
  scene->currentStage()->camDir.x = cos(input.x * M_PI / 2.0f);
  scene->currentStage()->camDir.z = sin(input.x * M_PI / 2.0f);
  scene->currentStage()->camDir.y = sin(input.y * M_PI / 2.0f);
}

int main() {
  SurfaceDesc surf_desc;
  surf_desc.width   = 800;
  surf_desc.height  = 600;
  ISurface* surface = surfaceCreate(surf_desc);

  RendererDesc desc;
  desc.bloom_enable   = 1;
  desc.hdr_enable     = 1;
  desc.bloom_sampling = 4;
  desc.surface        = surface;

  IRenderer* renderer = rendererCreate(desc);

  Scene* scene = sceneCreate();
  initScene(scene);
  renderer->upload(scene);

  do {
    renderer->render(scene);
    updateScene(scene, surface->getInput());
  } while (surface->update());

  delete renderer;
  delete surface;
}
