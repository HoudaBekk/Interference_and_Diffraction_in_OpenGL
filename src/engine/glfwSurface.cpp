
#include <video.hpp>
#include <GLFW/glfw3.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

namespace NextVideo {
struct Window {
  void* windowPtr;
  int   width;
  int   height;
  bool  needsUpdate;
  float x;
  float y;
  float ra;
  int   keyboard[512];
  float scroll;
};

/* GLFW CALLBACKS */
ENGINE_API void window_size_callback(GLFWwindow* window, int width, int height) {
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {

  auto* ptr = (Window*)glfwGetWindowUserPointer(window);

  glViewport(0, 0, width, height);
  ptr->width  = width;
  ptr->height = height;

  ptr->ra          = ptr->width / (float)(ptr->height);
  ptr->needsUpdate = 1;
}
ENGINE_API void cursor_position_callback(GLFWwindow* window, double x, double y) {
  auto* ptr = (Window*)glfwGetWindowUserPointer(window);
  ptr->x    = x / (float)ptr->width;
  ptr->y    = y / (float)ptr->height;

  ptr->x -= 0.5;
  ptr->y -= 0.5;

  ptr->x *= 2;
  ptr->y *= 2;
}
ENGINE_API void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
  auto* ptr = (Window*)glfwGetWindowUserPointer(window);
  ptr->scroll += yoffset;
}
ENGINE_API void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  auto* ptr          = (Window*)glfwGetWindowUserPointer(window);
  ptr->keyboard[key] = (action == GLFW_PRESS) || (action == GLFW_REPEAT);
}
struct GLFWSurface : public ISurface {

  Window* window;

  bool initializeDearImGui(GLFWwindow* wind) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    // setup platform/renderer bindings
    if (!ImGui_ImplGlfw_InitForOpenGL(wind, true)) { return false; }
    if (!ImGui_ImplOpenGL3_Init()) { return false; }

    return true;
  }

  ENGINE_API Window* windowCreate(int width, int height) {

    if (glfwInit() != GLFW_TRUE) {
      ERROR("Failed to start GLFW .\n");
      return 0;
    }

    auto defaults = NextVideo::rendererDefaults();
    if (defaults.glfw_noApi) {
      glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
      glfwCreateWindow(width, height, "PathTracing", NULL, NULL);

    if (window == NULL) {
      ERROR("Failed to open GLFW window. width: %d height: %d\n", width, height);
      return 0;
    }

    glfwMakeContextCurrent(window);

#ifdef __EMSCRIPTEN__
#else

    if (!defaults.glfw_noApi) {
      if (glewInit() != GLEW_OK) {
        ERROR("Failed to initialize glew");
        return 0;
      }
    }
#endif

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    LOG("Window created with %d %d\n", width, height);

    initializeDearImGui(window);
    Window* windowInstance      = new Window;
    windowInstance->windowPtr   = window;
    windowInstance->needsUpdate = true;
    windowInstance->width = width;
    windowInstance->height = height;
    glfwSetWindowUserPointer(window, windowInstance);
    return windowInstance;
  }

  ENGINE_API void windowDestroy(Window* window) {}

  GLFWSurface(SurfaceDesc desc) {
    this->window = windowCreate(desc.width, desc.height);
    this->desc   = desc;
  }

  ENGINE_API int update() override {
    window->needsUpdate = false;
    glfwSwapBuffers((GLFWwindow*)window->windowPtr);
    glfwPollEvents();
    return !glfwWindowShouldClose((GLFWwindow*)window->windowPtr);
  }

  SurfaceInput getInput() override {
    SurfaceInput in;
    in.keyboard = window->keyboard;
    in.x        = window->x;
    in.y        = window->y;
    return in;
  }
  SurfaceExtensions getExtensions() override {
    SurfaceExtensions extensions;
    extensions.names = glfwGetRequiredInstanceExtensions(&extensions.count);
    return extensions;
  }

  bool resized() const override { return window->needsUpdate; }

  int getWidth() const override { return window->width; }
  int getHeight() const override { return window->height; }

  ~GLFWSurface() { this->windowDestroy(this->window); }

  void* native() override { return window->windowPtr; }

  void beginUI() override {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();
  }

  void endUI() override {
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }
};


ENGINE_API ISurface* surfaceCreate(SurfaceDesc desc) {
  return new GLFWSurface(desc);
}
} // namespace NextVideo
