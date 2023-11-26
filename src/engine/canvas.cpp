#include <video.hpp>
#include <vector>

using namespace NextVideo;

struct Canvas : public ICanvas{

  struct Batch {
    int currentIndex = 0;
  };

  Batch batch;

  Canvas(ICanvasContext* ctx) : ICanvas(ctx) {
    this->ctx = ctx;
    this->ctx->setModelTransform(glm::mat4(1.0));
    this->ctx->setProjectionTransform(glm::mat4(1.0));
    this->ctx->setViewTransform(glm::mat4(1.0));
  }

  void setColor(glm::vec4 color) override { 
    this->currentColor = color; 
  };

  void setLineWidth(float lineWidth) override { 
    this->currentLineWidth = lineWidth;
  };


  void resetIndex() { batch.currentIndex = 0; }
  void move(glm::vec3 position) override { 
    resetIndex();
    push(position);
  };

  void push(glm::vec3 position) override { 
    ctx->pushVertex(getVertex(position));
    ctx->pushIndex(batch.currentIndex);
    batch.currentIndex++;
  };


  NextVideo::ICanvasContext::CanvasContextVertex getVertex(glm::vec3 pos) { 
    NextVideo::ICanvasContext::CanvasContextVertex vtx;
    vtx.position = pos;
    vtx.color = currentColor;
    vtx.uv = currentUV;
    return vtx;
  }

  void flush() override { 
    resetIndex();
  }

  private:
  glm::vec4 currentColor = glm::vec4(1,1,1,1);
  glm::vec2 currentUV = glm::vec2(0,0);
  float     currentLineWidth = 5.0;
  ICanvasContext* ctx;
};


void ICanvas::createCircle(glm::vec2 position, float size) { 

  flush();
  int count = 4;
  for(int i = 0; i < count; i++) {
    float a = 2 * M_PI * float(i / float(count));

    float x = cos(a);
    float y = sin(a);

    push({x * size + position.x, y * size + position.y, 0});
  }
  flush();
}

ICanvas* NextVideo::createCanvas(ICanvasContext* ctx) { return new Canvas(ctx); }

