#pragma once
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define ENGINE_API
namespace lin {
inline ENGINE_API float* id() {
  static float id[] = {
    1, 0, 0, 0, /*x */
    0, 1, 0, 0, /*y */
    0, 0, 1, 0, /*z*/
    0, 0, 0, 1, /*w*/
  };
  return id;
}

inline ENGINE_API glm::mat4 rotate(glm::vec3 axis, float amount) {
  return glm::rotate(glm::mat4(1.0f), amount, axis);
}
inline ENGINE_API glm::mat4 scale(glm::vec3 scale) {
  return glm::scale(glm::mat4(1.0f), scale);
}

inline ENGINE_API glm::mat4 scale(float scale) {
  return glm::scale(glm::mat4(1.0f), glm::vec3(scale));
}

inline ENGINE_API glm::mat4 translate(float x, float y, float z) {
  return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
}

inline ENGINE_API float* meshTransformPlaneScreen() {
  static glm::mat4 plane = rotate(glm::vec3(1, 0, 0), -M_PI / 2.0f) * scale(2.0) * translate(-0.5, 0, -0.5);
  return &plane[0][0];
}

inline ENGINE_API float* viewMatrix(const glm::vec3& camPos, const glm::vec3& camDir) {
  static glm::mat4 view;
  view = glm::lookAt(camPos, camPos + camDir, glm::vec3(0, 1, 0));
  return &view[0][0];
}
inline ENGINE_API float* projMatrix(float ra) {
  static glm::mat4 proj;
  proj = glm::perspective(float(M_PI) / 2.0f, ra, 0.5f, 2000.0f);
  return &proj[0][0];
}
} // namespace lin
