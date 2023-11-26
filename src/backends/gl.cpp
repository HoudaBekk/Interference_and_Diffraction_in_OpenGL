#include <cstring>
#include <video.hpp>
#include <linear.hpp>

#define VERIFY_FRAMEBUFFER                                                               \
  SAFETY(do {                                                                            \
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);                            \
    VERIFY(status == GL_FRAMEBUFFER_COMPLETE, "Framebuffer not complete! %d\n", status); \
  } while (0))

#define VERIFY_OBJECT(x) SAFETY(VERIFY(x != -1 && x >= 0, "Invalid object " #x "\n"))


#include <glm/ext.hpp>
#include <stdio.h>
#define MAX_OBJECTS 512

namespace NextVideo {
ENGINE_API const char* readFile(const char* path);
/* GL_UTIL_FUNCTIONS */

ENGINE_API void glUtilRenderScreenQuad() {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

ENGINE_API void glUtilsSetVertexAttribs(int index) {

  int stride = MESH_FORMAT_SIZE[index] * sizeof(float);

  if (index == 0) {
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
  }
}

ENGINE_API void glUtilRenderQuad(GLuint vbo, GLuint ebo, GLuint worldMat, GLuint viewMat, GLuint projMat) {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glUtilsSetVertexAttribs(0);
  glUniformMatrix4fv(worldMat, 1, 0, lin::meshTransformPlaneScreen());
  glUniformMatrix4fv(viewMat, 1, 0, lin::id());
  glUniformMatrix4fv(projMat, 1, 0, lin::id());
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

ENGINE_API GLuint glUtilLoadProgram(const char* vs, const char* fs) {

  char errorBuffer[2048];

  GLuint VertexShaderID   = glCreateShader(GL_VERTEX_SHADER);
  GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

  GLint Result = GL_FALSE;
  int   InfoLogLength;

  const char* VertexSourcePointer   = readFile(vs);
  const char* FragmentSourcePointer = readFile(fs);

  if (VertexSourcePointer == 0) {
    ERROR("Error reading vertex shader: %s \n", vs);
    return -1;
  }

  if (FragmentSourcePointer == 0) {
    ERROR("Error reading fragment shader: %s \n", fs);
    return -1;
  }

  glShaderSource(VertexShaderID, 1, &VertexSourcePointer, NULL);
  glCompileShader(VertexShaderID);

  // Check Vertex Shader
  glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);

  if (InfoLogLength > 0) {
    glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, errorBuffer);
    ERROR("Error loading vertex shader Source : %s : \nError : %s\n", vs, errorBuffer);
    free((void*)VertexSourcePointer);
    free((void*)FragmentSourcePointer);
    return 0;
  }

  glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer, NULL);
  glCompileShader(FragmentShaderID);

  // Check Fragment Shader
  glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);

  if (InfoLogLength > 0) {
    glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, errorBuffer);
    ERROR("Error loading fragment shader %s : \n%s\n", fs, errorBuffer);
    free((void*)VertexSourcePointer);
    free((void*)FragmentSourcePointer);
    return 0;
  }

  GLuint ProgramID = glCreateProgram();
  glAttachShader(ProgramID, VertexShaderID);
  glAttachShader(ProgramID, FragmentShaderID);
  glLinkProgram(ProgramID);

  // Check the program
  glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
  glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);

  if (InfoLogLength > 0) {
    glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, errorBuffer);
    ERROR("Error compiling program %s : \n%s\n", fs, errorBuffer);
    free((void*)VertexSourcePointer);
    free((void*)FragmentSourcePointer);
    return 0;
  }

  glDetachShader(ProgramID, VertexShaderID);
  glDetachShader(ProgramID, FragmentShaderID);

  glDeleteShader(VertexShaderID);
  glDeleteShader(FragmentShaderID);

  free((void*)VertexSourcePointer);
  free((void*)FragmentSourcePointer);

  LOG("[RENDERER] Program loaded successfully %s\n", fs);
  return ProgramID;
}

bool checkScene(Scene* scene) {
  for (Material& mat : scene->materials) {
    VERIFY(mat.albedoTexture < scene->textures.size(), "Invalid texture index\n");
    VERIFY(mat.normalTexture < scene->textures.size(), "Invalid texture index\n");
    VERIFY(mat.emissionTexture < scene->textures.size(), "Invalid texture index\n");
    VERIFY(mat.roughnessTexture < scene->textures.size(), "Invalid texture index\n");
  }
  return true;
}

const inline static unsigned int attachments[] = {
  GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
  GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5,
  GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7};

/* Renderer begin */
/* Engine Default Variables */
static int PARAM_BLOOM_CHAIN_LENGTH = 8;

static int BUFF_START_USER       = 0;
static int BUFF_PLAIN            = 0;
static int TEXT_STD              = 0;
static int TEXT_IBL              = 1;
static int TEXT_ATTACHMENT_COLOR = 2;
static int TEXT_ATTACHMENT_BLOOM = 3;
static int TEXT_GAUSS_RESULT0    = 4;
static int TEXT_GAUSS_RESULT02   = 5;
static int TEXT_GAUSS_RESULT1    = 6;
static int TEXT_GAUSS_RESULT12   = 7;
static int TEXT_GAUSS_RESULT2    = 8;
static int TEXT_GAUSS_RESULT22   = 9;
static int TEXT_GAUSS_RESULT3    = 10;
static int TEXT_GAUSS_RESULT32   = 11;
static int TEXT_BLOOM_START      = 12;
static int TEXT_BLOOM_END        = TEXT_BLOOM_START + PARAM_BLOOM_CHAIN_LENGTH;
static int TEXT_SHADOW_MAP       = TEXT_BLOOM_END + 1;
static int TEXT_END              = TEXT_SHADOW_MAP;
static int TEXT_START_USER       = TEXT_END + 1;
static int FBO_HDR_PASS          = 0;
static int FBO_GAUSS_PASS_PING   = 1;
static int FBO_GAUSS_PASS_PONG   = 2;
static int FBO_BLOOM             = 3;
static int FBO_SHADOW_MAP        = 4;
static int FBO_START_USER        = 5;
static int RBO_HDR_PASS_DEPTH    = 0;

#define UNIFORMLIST_HDR(o, u)        o(u_color, u) o(u_bloom, u)
#define UNIFORMLIST_GAUSS(o, u)      o(u_input, u) o(u_horizontal, u)
#define UNIFORMLIST_UPSAMPLE(o, u)   o(srcTexture, u) o(filterRadius, u)
#define UNIFORMLIST_DOWNSAMPLE(o, u) o(srcTexture, u) o(srcResolution, u)

#define UNIFORMLIST_PBR(o, u)                                                               \
  o(u_envMap, u) o(u_diffuseTexture, u) o(u_specularTexture, u) o(u_bumpTexture, u)         \
    o(u_kd, u) o(u_ka, u) o(u_ks, u) o(u_shinnness, u) o(u_ro, u) o(u_rd, u) o(u_isBack, u) \
      o(u_shadingMode, u) o(u_useTextures, u) o(u_ViewMat, u) o(u_ProjMat, u)               \
        o(u_WorldMat, u) o(u_flatUV, u) o(u_uvScale, u) o(u_uvOffset, u)

#define UNIFORMLIST(o)                         \
  UNIFORMLIST_HDR(o, hdr)                      \
  UNIFORMLIST_GAUSS(o, filter_gauss)           \
  UNIFORMLIST_UPSAMPLE(o, filter_upsample)     \
  UNIFORMLIST_DOWNSAMPLE(o, filter_downsample) \
  UNIFORMLIST_PBR(o, pbr)

#define PROGRAMLIST(O)                                             \
  O(hdr, "assets/filter.vs", "assets/hdr.fs")                      \
  O(filter_gauss, "assets/filter.vs", "assets/gauss.fs")           \
  O(filter_upsample, "assets/filter.vs", "assets/upsample.fs")     \
  O(filter_downsample, "assets/filter.vs", "assets/downsample.fs") \
  O(pbr, "assets/pbr.vs", "assets/pbr.fs")

struct Renderer : public IRenderer {

  RendererDesc desc;

  /* GL Objects */
  GLuint              vao;
  std::vector<GLuint> textures;
  std::vector<GLuint> vbos;
  std::vector<GLuint> ebos;
  std::vector<GLuint> fbos;
  std::vector<GLuint> rbos;

  /* Default programs */

#define PROGRAM_DECL(o, vs, fs) GLuint program_##o;
  PROGRAMLIST(PROGRAM_DECL)
#undef PROGRAM_DECL

#define UNIFORM_DECL(o, u) GLuint u##_##o;
  UNIFORMLIST(UNIFORM_DECL)
#undef UNIFORM_DECL

  /* Debug checks */

  int bindTexture(int textureSlot) {
    glActiveTexture(GL_TEXTURE0 + textureSlot);
    glBindTexture(GL_TEXTURE_2D, textures[textureSlot]);
    return textureSlot;
  }

  ENGINE_API void glUtilAttachScreenTexture(int textureSlot, int attachment, GLenum type, GLenum format) {

    if (desc.surface->resized()) {

      int width  = desc.surface->getWidth();
      int height = desc.surface->getHeight();
      VERIFY(width > 0, "Width not valid!\n");
      VERIFY(width > 0, "Height not valid!\n");
      glActiveTexture(GL_TEXTURE0 + textureSlot);
      glBindTexture(GL_TEXTURE_2D, textures[textureSlot]);
      glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, type, GL_UNSIGNED_BYTE, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, textures[textureSlot], 0);
      LOG("[RENDERER] Generated screen texture for %d to attachment %d -> %d for size %dx%d\n", textureSlot, attachment, textures[textureSlot], width, height);
      glActiveTexture(GL_TEXTURE0);
    }
  }

  ENGINE_API void glUtilAttachRenderBuffer(int rbo, int attachment, GLenum type) {
    if (desc.surface->resized()) {

      int width  = desc.surface->getWidth();
      int height = desc.surface->getHeight();
      VERIFY(width > 0, "Width not valid!\n");
      VERIFY(width > 0, "Height not valid!\n");
      glBindRenderbuffer(GL_RENDERBUFFER, rbos[rbo]);
      glRenderbufferStorage(GL_RENDERBUFFER, type, width, height);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, rbos[rbo]);
      LOG("[RENDERER] Generated render buffer for %d attachment %d\n for size %dx%d\n", rbo, attachment, width, height);
    }
  }

  ENGINE_API void upload(Scene* scene) override {
    //Texture loading
    {
      const Texture* textureTable = scene->textures.data();
      for (int i = 0; i < scene->textures.size(); i++) {
        glActiveTexture(GL_TEXTURE0 + i + TEXT_START_USER);
        glBindTexture(GL_TEXTURE_2D, textures[i + TEXT_START_USER]);
        if (textureTable[i].useNearest) {
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        } else {
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        if (!textureTable[i].mipmapDisable && _desc.texture_mipmap_enable) {
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        } else {
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }


        LOG("[RENDERER] Uploading texture [%d] width %d height %d channels %d\n", i, textureTable[i].width, textureTable[i].height, textureTable[i].channels);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureTable[i].width, textureTable[i].height, 0, GL_RGB, GL_UNSIGNED_BYTE, textureTable[i].data);
        if (!textureTable[i].mipmapDisable && _desc.texture_mipmap_enable)
          glGenerateMipmap(GL_TEXTURE_2D);
      }
      glActiveTexture(GL_TEXTURE0);
    }

    //Buffer loading
    {
      for (int i = 0; i < scene->meshes.size(); i++) {
        Mesh* mesh = &scene->meshes[i];
        if (mesh->type == CUSTOM) {
          float*        vbo = mesh->tCustom.vertexBuffer;
          unsigned int* ebo = mesh->tCustom.indexBuffer;

          LOG("[RENDERER] Uploading mesh [%d] %p %p with %d %d to %d %d\n", BUFF_START_USER + i, mesh->tCustom.vertexBuffer, mesh->tCustom.indexBuffer, mesh->tCustom.numVertices, mesh->tCustom.numIndices, vbos[BUFF_START_USER + i], ebos[BUFF_START_USER + i]);

          glBindBuffer(GL_ARRAY_BUFFER, vbos[BUFF_START_USER + i]);
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebos[BUFF_START_USER + i]);

          VERIFY(mesh->tCustom.meshFormat >= MESH_FORMAT_DEFAULT && mesh->tCustom.meshFormat < MESH_FORMAT_LAST, "Invalid format %d", mesh->tCustom.meshFormat);
          glBufferData(GL_ARRAY_BUFFER, mesh->tCustom.numVertices * MESH_FORMAT_SIZE[mesh->tCustom.meshFormat] * sizeof(float), vbo, GL_STATIC_DRAW);
          glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->tCustom.numIndices * sizeof(unsigned int), ebo, GL_STATIC_DRAW);
        }
      }
    }
    LOG("[Renderer] Render upload completed.\n");
  }

  ~Renderer() {

    Renderer* renderer = this;
    glDeleteTextures(renderer->textures.size(), renderer->textures.data());
    glDeleteBuffers(renderer->vbos.size(), renderer->vbos.data());
    glDeleteBuffers(renderer->ebos.size(), renderer->ebos.data());
    glDeleteFramebuffers(renderer->fbos.size(), renderer->fbos.data());
    glDeleteRenderbuffers(renderer->rbos.size(), renderer->rbos.data());

    LOG("[Renderer] Render destroy completed.\n");
  }

  ENGINE_API void bindMaterial(Renderer* renderer, Material* mat) {

    glUniform1i(renderer->pbr_u_useTextures, mat->albedoTexture >= 0);
    if (mat->albedoTexture >= 0) {
      glUniform1i(renderer->pbr_u_diffuseTexture, mat->albedoTexture + TEXT_START_USER);
    } else {
      glUniform3f(renderer->pbr_u_kd, mat->albedo.x, mat->albedo.y, mat->albedo.z);
      glUniform3f(renderer->pbr_u_ks, mat->metallic, mat->roughness, mat->roughness);
    }

    VERIFY_OBJECT(renderer->pbr_u_uvScale);
    VERIFY_OBJECT(renderer->pbr_u_uvOffset);
    glUniform2f(renderer->pbr_u_uvScale, mat->uvScale.x, mat->uvScale.y);
    glUniform2f(renderer->pbr_u_uvOffset, mat->uvOffset.x, mat->uvOffset.y);
  }

  ENGINE_API int bindMesh(Renderer* renderer, Mesh* mesh, int meshIdx) {
    int vertexCount = mesh->tCustom.numVertices;
    glUniform1i(renderer->pbr_u_flatUV, 0);
    switch (mesh->type) {
      case CUSTOM:
        glBindBuffer(GL_ARRAY_BUFFER, renderer->vbos[meshIdx + BUFF_START_USER]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebos[meshIdx + BUFF_START_USER]);
        vertexCount = mesh->tCustom.numIndices;
        break;
    }
    return vertexCount;
  }

  ENGINE_API void rendererBeginHDR(Renderer* renderer) {
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbos[FBO_HDR_PASS]);
    glUtilAttachRenderBuffer(RBO_HDR_PASS_DEPTH, GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH24_STENCIL8);
    glUtilAttachScreenTexture(TEXT_ATTACHMENT_COLOR, GL_COLOR_ATTACHMENT0, GL_RGB, GL_RGB16F);
    glUtilAttachScreenTexture(TEXT_ATTACHMENT_BLOOM, GL_COLOR_ATTACHMENT1, GL_RGB, GL_RGB16F);
    glDrawBuffers(2, attachments);
    VERIFY_FRAMEBUFFER;
  }

  ENGINE_API int rendererFilterGauss(Renderer* renderer, int src, int pingTexture, int pongTexture, int passes) {
    int fbo = FBO_GAUSS_PASS_PING;
    int out = pingTexture;
    int in  = src;

    glUseProgram(renderer->program_filter_gauss);
    for (int i = 0; i < 2 * passes; i++) {
      bool horizontal = i % 2;
      glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbos[FBO_GAUSS_PASS_PING + horizontal]);
      glUtilAttachScreenTexture(out, GL_COLOR_ATTACHMENT0, GL_RGB, GL_RGB16F);
      glDrawBuffers(1, attachments);
      VERIFY_FRAMEBUFFER;
      glUniform1i(renderer->filter_gauss_u_input, in);
      glUniform1i(renderer->filter_gauss_u_horizontal, 1);
      glUtilRenderScreenQuad();
      in  = out;
      out = horizontal ? pingTexture : pongTexture;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return out;
  }

  ENGINE_API int rendererFilterBloom(int src, float width, float height, float filterRadius, int count) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbos[FBO_BLOOM]);

    glm::vec2 srcSize(width, height);

    const int bloom_length = std::min(PARAM_BLOOM_CHAIN_LENGTH, count);

    VERIFY(bloom_length > 0, "Invalid bloom length");
    VERIFY(filterRadius >= 1.0f, "Invalid filterRadius");

    if (desc.surface->resized()) {
      LOG("[RENDERER] Bloom update texture and framebuffer");
      glm::vec2 size(width, height);
      for (int i = 0; i < bloom_length; i++) {
        size /= 2;
        bindTexture(i + TEXT_BLOOM_START);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, (int)size.x, (int)size.y, 0, GL_RGB, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        SAFETY(glActiveTexture(GL_TEXTURE0));
      }

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[TEXT_BLOOM_START], 0);
      glDrawBuffers(1, attachments);
      VERIFY_FRAMEBUFFER;
    }


    glm::vec2 size(width, height);
    //Downsample
    {
      glUseProgram(program_filter_downsample);
      bindTexture(src);
      glUniform2f(filter_downsample_srcResolution, size.x, size.y);
      glUniform1i(filter_downsample_srcTexture, src);
      for (int i = 0; i < bloom_length; i++) {
        glViewport(0, 0, size.x / 2, size.y / 2);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[i + TEXT_BLOOM_START], 0);
        VERIFY_FRAMEBUFFER;
        glUtilRenderScreenQuad();
        size = size / 2.0f;
        glUniform2f(filter_downsample_srcResolution, size.x, size.y);
        glUniform1i(filter_downsample_srcTexture, i + TEXT_BLOOM_START);
      }
    }
    //Upsample
    {
      glUseProgram(program_filter_upsample);
      glUniform1i(filter_upsample_filterRadius, filterRadius);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);
      glBlendEquation(GL_FUNC_ADD);

      for (int i = bloom_length - 1; i > 0; i--) {
        int texture        = TEXT_BLOOM_START + i;
        int nextMipTexture = texture - 1;

        glUniform1i(filter_upsample_srcTexture, texture);
        glViewport(0, 0, size.x * 2, size.y * 2);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[nextMipTexture], 0);
        VERIFY_FRAMEBUFFER;
        glUtilRenderScreenQuad();

        size *= 2.0f;
      }
      glDisable(GL_BLEND);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, desc.surface->getWidth(), desc.surface->getHeight());
    return TEXT_BLOOM_START;
  }

  ENGINE_API void passShadowMapBegin() {
    glBindFramebuffer(GL_FRAMEBUFFER, FBO_SHADOW_MAP);
    if (desc.surface->resized()) {
      LOG("[RENDERER] Shadow map update init\n");
      bindTexture(TEXT_SHADOW_MAP);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, _desc.shadowmapping_width, _desc.shadowmapping_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[TEXT_SHADOW_MAP], 0);
      glDrawBuffer(GL_NONE);
      glReadBuffer(GL_NONE);
    }


    glViewport(0, 0, _desc.shadowmapping_width, _desc.shadowmapping_height);
    glClear(GL_DEPTH_BUFFER_BIT);
  }

  ENGINE_API void passShadowMapEnd() {
    glBindTexture(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, desc.surface->getWidth(), desc.surface->getHeight());
  }

  ENGINE_API void renderScene(Renderer* renderer, Scene* scene, Stage* stage, float* viewMat, float* projMat) {

    glUniformMatrix4fv(renderer->pbr_u_ViewMat, 1, 0, viewMat);
    glUniformMatrix4fv(renderer->pbr_u_ProjMat, 1, 0, projMat);

    for (int d = 0; d < stage->instances.size(); d++) {
      ObjectInstanceGroup* g    = &stage->instances[d];
      Object*              obj  = &stage->objects[g->object];
      Mesh*                mesh = &scene->meshes[obj->mesh];
      Material*            mat  = &scene->materials[obj->material];


      VERIFY(valid(stage->objects, g->object), "Invalid object index %d\n", g->object);
      VERIFY(valid(scene->materials, obj->material), "Invalid material index %d\n", obj->material);
      VERIFY(valid(scene->meshes, obj->mesh), "Invalid mesh index %d\n", obj->mesh);

      //Bind mesh and materials
      int vertexCount = bindMesh(renderer, mesh, obj->mesh);
      if (mesh->type == CUSTOM) glUtilsSetVertexAttribs(mesh->tCustom.meshFormat);
      bindMaterial(renderer, mat);

      for (int i = 0; i < g->transforms.size(); i++) {
        glUniformMatrix4fv(renderer->pbr_u_WorldMat, 1, 0, (float*)&g->transforms[i][0][0]);
        glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, 0);
      }
    }
  }

  ENGINE_API void rendererPass(Renderer* renderer, Scene* scene) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(renderer->program_pbr);

    Stage* stage = scene->currentStage();

    float* viewMat = lin::viewMatrix(stage->camPos, stage->camDir);
    float* projMat = lin::projMatrix(desc.surface->ra());

    //TODO HINT
    if(stage->skyTexture >= 0) { 
      VERIFY(stage->skyTexture >= 0 && stage->skyTexture < scene->textures.size(), "Invalid sky texture\n");
      glUniform1i(renderer->pbr_u_envMap, stage->skyTexture + TEXT_START_USER);
    }
    glUniform3f(renderer->pbr_u_ro, stage->camPos.x, stage->camPos.y, stage->camPos.z);
    glUniform3f(renderer->pbr_u_rd, stage->camDir.x, stage->camDir.y, stage->camDir.z);

    glUniform1i(renderer->pbr_u_isBack, 1);
    glUtilRenderQuad(renderer->vbos[BUFF_PLAIN], renderer->ebos[BUFF_PLAIN], renderer->pbr_u_WorldMat, renderer->pbr_u_ViewMat, renderer->pbr_u_ProjMat);
    glUniform1i(renderer->pbr_u_isBack, 0);
    renderScene(renderer, scene, stage, viewMat, projMat);
  }

  ENGINE_API void rendererEnd() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
  ENGINE_API void rendererHDR(Renderer* renderer, Scene* scene) {
    rendererBeginHDR(renderer);
    rendererPass(renderer, scene);
    rendererEnd();

    //int gaussBloomResult = rendererFilterGauss(renderer, TEXT_ATTACHMENT_BLOOM, TEXT_GAUSS_RESULT0, TEXT_GAUSS_RESULT02, desc.gauss_passes);
    int bloomResult = rendererFilterBloom(TEXT_ATTACHMENT_BLOOM, desc.surface->getWidth(), desc.surface->getHeight(), _desc.bloom_radius, _desc.bloom_sampling);
    glUseProgram(renderer->program_hdr);
    glUniform1i(renderer->hdr_u_bloom, bloomResult);
    glUniform1i(renderer->hdr_u_color, TEXT_ATTACHMENT_COLOR);

    VERIFY(renderer->hdr_u_bloom != renderer->hdr_u_color, "Invalid uniform values\n");
    glUtilRenderScreenQuad();
  }

  ENGINE_API void rendererRegular(Renderer* renderer, Scene* scene) {
    rendererPass(renderer, scene);
  }
  ENGINE_API void render(Scene* scene) override {

    if (desc.surface->getWidth() <= 0 || desc.surface->getHeight() <= 0) return;

    VERIFY(checkScene(scene), "Invalid scene graph\n");
    glBindVertexArray(vao);
    rendererHDR(this, scene);
    glBindVertexArray(0);
  }

  Renderer(RendererDesc desc) {
    this->_desc = desc;
  }
};


/* GL CALLBACKS*/
ENGINE_API void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
  fprintf(stdout, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n", (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}

ENGINE_API IRenderer* rendererCreate(RendererDesc desc) {
  dprintf(2, "[RENDERER] Creating renderer " __DATE__ "  " __TIME__ "\n");
  VERIFY(desc.surface != nullptr, "Invalid surface");
  Renderer* renderer = new Renderer(desc);
  renderer->desc     = desc;
  renderer->textures.resize(MAX_OBJECTS);
  renderer->vbos.resize(MAX_OBJECTS);
  renderer->ebos.resize(MAX_OBJECTS);
  renderer->fbos.resize(MAX_OBJECTS);
  renderer->rbos.resize(MAX_OBJECTS);

  // GL configuration
  {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_MULTISAMPLE);

#ifdef DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);
#endif
  }
  // GL gen
  {
    glGenVertexArrays(1, &renderer->vao);
    glGenBuffers(renderer->ebos.size(), renderer->ebos.data());
    glGenBuffers(renderer->vbos.size(), renderer->vbos.data());
    glGenTextures(renderer->textures.size(), renderer->textures.data());
    glGenFramebuffers(renderer->fbos.size(), renderer->fbos.data());
    glGenRenderbuffers(renderer->rbos.size(), renderer->rbos.data());
  }

  {
    glBindVertexArray(renderer->vao);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
  }

#define PROGRAM_ASSIGN(name, vs, fs)                    \
  renderer->program_##name = glUtilLoadProgram(vs, fs); \
  VERIFY_OBJECT(renderer->program_##name);

  PROGRAMLIST(PROGRAM_ASSIGN);
#undef PROGRAM_ASSIGN
#define UNIFORM_ASSIGN(o, u) \
  renderer->u##_##o = glGetUniformLocation(renderer->program_##u, #o);

  UNIFORMLIST(UNIFORM_ASSIGN)
#undef UNIFORM_ASSIGN

  LOG("[Renderer] Render create completed.\n");
  return renderer;
}


ENGINE_API RendererBackendDefaults rendererDefaults() {
  static RendererBackendDefaults def = {
    .glfw_noApi = false};
  return def;
} // namespace NextVideo
} // namespace NextVideo


// Canvas


namespace NextVideo { 

  template <typename T>
  struct SmartBuffer { 
    std::vector<T> cpu;

    GLuint gpu;
    int gpuAllocatedSize;
    GLenum target;
    int lastCount;

    SmartBuffer(GLenum target) { 
      glGenBuffers(1, &gpu);
      this->target = target;
      this->gpuAllocatedSize = 0;
    }

    void push(const T& data) { 
      cpu.push_back(data);
    }

    void flush() { 
      if(cpu.size() == 0) return;
      glBindBuffer(target, gpu);
      lastCount = cpu.size();
      if(cpu.size() > gpuAllocatedSize) { 
        glBufferData(target, cpu.size() * sizeof(T), cpu.data(), GL_DYNAMIC_DRAW);
        gpuAllocatedSize = cpu.size();
      } else { 
        glBufferSubData(target, 0, cpu.size() * sizeof(T), cpu.data());
      }
      cpu.clear();
    }

    int count() { return lastCount; }
  };

  struct GLCanvasContext : public ICanvasContext{

    struct BatchInformation { 
      SmartBuffer<ICanvasContext::CanvasContextVertex> vertices;
      SmartBuffer<unsigned int> indices;

      GLuint vao;

      BatchInformation() : vertices(GL_ARRAY_BUFFER), indices(GL_ELEMENT_ARRAY_BUFFER) { 
        const int stride = sizeof(ICanvasContext::CanvasContextVertex);

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, vertices.gpu);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * (3 + 4)));
      }

      void pushVertex(CanvasContextVertex vtx) { 
        vertices.push(vtx);
      }
      void pushIndex(int idx) { 
        indices.push(idx); 
      }

      void flush() { 
        vertices.flush();
        indices.flush();
      }

      int count() { return indices.count(); }
    };

    void bindBatch(int index) { 
      if((index + 1) > batches.size())
        batches.resize(index + 1);
    }

    GLCanvasContext() { 
      renderingProgram = glUtilLoadProgram("assets/2d.vs", "assets/2d.fs");
      VERIFY_OBJECT(renderingProgram);

      u_ViewMat = glGetUniformLocation(renderingProgram, "u_ViewMat");
      u_ProjMat = glGetUniformLocation(renderingProgram, "u_ProjMat");
      u_WorldMat = glGetUniformLocation(renderingProgram, "u_WorldMat");

      VERIFY_OBJECT(u_ViewMat);
      VERIFY_OBJECT(u_ProjMat);
      VERIFY_OBJECT(u_WorldMat);
    }

    int beginPath(int index = -1) override { 
      if(index == -1) index = batches.size();
      currentBatch = index;
      bindBatch(index);
      return index;
    };

    void flush() override { 
      batches[currentBatch].flush();
    };

    void draw(int index) override { 
      VERIFY(index < batches.size(), "Invalid index");
      glBindVertexArray(batches[index].vao);
      glUseProgram(renderingProgram);
      glDrawElements(GL_TRIANGLE_FAN, batches[index].count(), GL_UNSIGNED_INT, 0);
    }

    void pushVertex(CanvasContextVertex vtx) override { batches[currentBatch].pushVertex(vtx); };
    void pushIndex(int idx) override { batches[currentBatch].pushIndex(idx); };

    virtual void setViewTransform(const glm::mat4& tr) override { 
      glUseProgram(renderingProgram);
      glUniformMatrix4fv(u_ViewMat, 1, 0, &tr[0][0]);
    }
    virtual void setProjectionTransform(const glm::mat4& tr) override { 
      glUseProgram(renderingProgram);
      glUniformMatrix4fv(u_ProjMat, 1, 0, &tr[0][0]);
    }
    virtual void setModelTransform(const glm::mat4& tr) override { 
      glUseProgram(renderingProgram);
      glUniformMatrix4fv(u_WorldMat, 1, 0, &tr[0][0]);
    }

    private:
    std::vector<BatchInformation> batches;
    int currentBatch;

    GLuint renderingProgram;
    GLuint u_ViewMat;
    GLuint u_ProjMat;
    GLuint u_WorldMat;
  };

  ICanvasContext* createCanvasContext() { return new GLCanvasContext;}
}
