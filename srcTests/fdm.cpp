#include "imgui.h"
#include <video.hpp>
#include <implot/implot.h>
using namespace NextVideo;


// CONSTANTS (es un poc caòtic)
#define C                  299792458.0
#define LIGHT_DECAY_FACTOR 1.0e-5
#define GAMMA_CORRECTED    1.0
#define A_WAVE             5000e-10
#define A_SEPARATION       0.01e-3
#define A_L                = 200.0e-3
#define ZOOM               1e-4
#define TIME_ZOOM          (1e-6 / C)
#define N                  NCOUNT

int   NCOUNT               = 5;       /* Nombre de focus virtuals en xarxa de difracció */
int   INTEGRATION_STEPS    = 15;      /* Nombre de pasos de integració per calcular la mitjana */
bool  LIGHT_DECAY_ENABLED  = false;   /* Activar divisió per distancia */
float LIGHT_DECAY_EXPONENT = 0.00002; /* Correcció per exponent, per ajustar els valors a valors representables */
float plotting_distance    = 200e-3;  /* Distancia de la pantalla */
float plotting_resolution  = 4;       /* Resolució del plot */
int   plotting_count       = 4000;    /* Cantitat de mostreig del plot */
int   plot_highpassWindow  = 10;      /* Tamany de la finestra de cerca de màxims */

#define B_SEPARATION 0.01e-3
#define C_SEPARATION 0.001e-3


/* CPU BACKEND */
using namespace glm;

float uLambda     = 5000e-10;
float uAmpladaMul = C_SEPARATION;

#define LAMBDA uLambda

bool uAmpladaFixa;
bool uNormalitzarXarxa;


// FUNCIONS DE LA SIMULACIÖ
// Aquest codi es el mateix que el de fdm.glsl, pero compilat directament en C++ per poder evaluar la gràfica en
// punts concrets i treure el plot

//Retorna el coeficient de distància amb la pantalla
float lightValue(vec2 st) {
  //Correcció per mostrar de forma dinàmica a la pantalla
  return pow(0.1, LIGHT_DECAY_EXPONENT) / sqrt(st.x * st.x + st.y * st.y);
}


// Retorna el valor de la funció del camp elèctric en un temps t en una posició st del espai
float light(vec2 st, float t) {
  float l = length(vec3(st.x, st.y, 0));
  float k = 2.0 * M_PI / LAMBDA;
  float f = C / LAMBDA;
  float w = f * 2.0 * M_PI;

  float value = (sin(l * k - t * w) * 0.5 + 0.5);
  if (LIGHT_DECAY_ENABLED) return value * lightValue(st);
  return value;
}


float net(vec2 st, float off, float t, float separation) {
  float result = 0.0;
  if (uAmpladaFixa)
    separation = separation / float(N);
  float offset = -float(N) * separation * 0.5 + off;
  for (int i = 0; i < N; i++) {
    result += light(st + vec2(0, offset), t);
    offset += separation;
  }
  if (uNormalitzarXarxa)
    return result / float(N);
  return result;
}

float experimentA(vec2 st, float t) {
  return light(st + vec2(0.0, -A_SEPARATION * 0.5), t) * 0.5 + light(st + vec2(0.0, A_SEPARATION * 0.5), t) * 0.5;
}
float experimentB(vec2 st, float t) {
  return net(st, 0.0, t, B_SEPARATION);
}

typedef float (*experiment_t)(vec2 st, float t);

#define C_SEPARATION 0.001e-3

float experimentC(vec2 st, float t) {
  return net(st, 0.0, t, uAmpladaMul);
}

float experimentD(vec2 st, float t) {
  float o = 0.1e-3;
  return net(st, -o / 2.0, t, C_SEPARATION) * 0.5 + net(st, o / 2.0, t, C_SEPARATION) * 0.5;
}

float integrate(vec2 st, float tP, experiment_t func) {

  float result = 0.0;
  float f      = C / A_WAVE;
  float w      = f * 2.0 * M_PI;
  float L      = float(INTEGRATION_STEPS);
  float dt     = 2.0 * M_PI / (L * w);
  float t      = 0.0;
  for (int i = 0; i < INTEGRATION_STEPS; i++) {
    float partial = func(st, t + tP);
    result += partial * partial;
    t += dt;
  }
  return result / L;
}


// Funcions per trobar els valors del plot
struct PlotResult {
  std::vector<float> y;
  std::vector<float> x;
};

PlotResult plot(experiment_t func) {
  float x     = plotting_distance;
  float dy    = pow(10.0, -plotting_resolution);
  int   count = plotting_count;

  float      current = -dy * count / 2;
  PlotResult res;
  for (int i = 0; i < count; i++) {
    res.y.push_back(integrate(glm::vec2(x, current), 0.0, func));
    res.x.push_back(current);
    current += dy;
  }
  return res;
}


// Funció utiltaria per trobar els màxims de una funció utiltzant una finestra de convolució
std::vector<int> findLocalMaximumValues(std::vector<float>& data) {
  int              lookUpSize = plot_highpassWindow;
  std::vector<int> indices;
  if (data.size() < (lookUpSize * 2 + 1)) return {};

  for (int i = lookUpSize; i < data.size() - lookUpSize - 1; i++) {
    bool isMaxima = true;
    for (int j = i - lookUpSize; j < i + lookUpSize; j++) {
      if (data[j] > data[i]) {
        isMaxima = false;
        break;
      }
    }
    if (isMaxima) indices.push_back(i);
  }

  return indices;
}

/* GL CODE */
/* GL CALLBACKS*/
void messageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
  fprintf(stdout, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n", (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}

GLuint iLambda;
GLuint program;
GLuint iTime;
GLuint iZoom;
GLuint iResolution;
GLuint iIntegrationMode;
GLuint iDecayMode;
GLuint iDecayExponent;
GLuint iExperimentSelector;
GLuint iDistance;
GLuint iN;
GLuint iAmpladaFixa;
GLuint iNormalitzarXarxa;
GLuint iAmpladaMul;

#define INITIAL_LAMBDA 5000e-10
#define MIN_LAMBDA     3000
#define MAX_LAMBDA     8000
// Uniforms
float uDistance   = 0.0;
float uZoom       = 1.0;
float uTime       = 0.0;
int   uExperiment = 0;
bool  uIntegration;

bool experimentPractica = true;

experiment_t currentExperiment() {
  if (uExperiment == 0) return experimentA;
  if (uExperiment == 1) return experimentB;
  if (uExperiment == 2) return experimentC;
  if (uExperiment >= 3) return experimentD;
}

void init() {
  program             = glUtilLoadProgram("assets/filter.vs", "assets/fdm.glsl");
  iTime               = glGetUniformLocation(program, "iTime");
  iZoom               = glGetUniformLocation(program, "iZoom");
  iResolution         = glGetUniformLocation(program, "iResolution");
  iIntegrationMode    = glGetUniformLocation(program, "iIntegrationMode");
  iDecayMode          = glGetUniformLocation(program, "iDecayMode");
  iDecayExponent      = glGetUniformLocation(program, "iDecayExponent");
  iExperimentSelector = glGetUniformLocation(program, "iExperimentSelector");
  iN                  = glGetUniformLocation(program, "N");
  iDistance           = glGetUniformLocation(program, "iDistance");
  iAmpladaFixa        = glGetUniformLocation(program, "iAmpladaFixa");
  iNormalitzarXarxa   = glGetUniformLocation(program, "iNormalitzarXarxa");
  iLambda             = glGetUniformLocation(program, "iLambda");
  iAmpladaMul         = glGetUniformLocation(program, "iAmpladaMul");
}

NextVideo::ISurface* surface;


void uiRender() {
  if (ImGui::Begin("Simulation parameters")) {
    ImGui::Text("Simulation types");
    ImGui::Checkbox("Lab Experiments", &experimentPractica);

    ImGui::Separator();
    ImGui::Text("Simulation parameters");
    ImGui::Checkbox("Use light decay", &LIGHT_DECAY_ENABLED);
    ImGui::Checkbox("Integration", &uIntegration);
    ImGui::Checkbox("Amplada fixa", &uAmpladaFixa);
    ImGui::Checkbox("Normalitzar xarxa", &uNormalitzarXarxa);
    ImGui::SliderFloat("Light decay exponent", &LIGHT_DECAY_EXPONENT, 1.0, 10.0);
    ImGui::SliderFloat("Zoom", &uZoom, 0.01, 50.0);
    static float AmpladaSlider = 1.0f;
    ImGui::SliderFloat("Amplada", &AmpladaSlider, 1.0, 10.0);
    ImGui::SliderFloat("Time", &uTime, 0.01, 10.0);
    static int lambdaSlider = 5000;
    ImGui::SliderInt("Light lambda", &lambdaSlider, 2000, 8000);
    ImGui::SliderInt("N", &NCOUNT, 2, 50);
    ImGui::InputFloat("Distance", &uDistance);

    ImGui::Separator();
    ImGui::InputFloat("Plot resolution", &plotting_resolution);
    ImGui::InputInt("Plot count", &plotting_count);
    ImGui::InputInt("Plot high pass winow", &plot_highpassWindow);

    ImGui::Separator();
    ImGui::InputInt("Integration steps ", &INTEGRATION_STEPS);
    ImGui::End();

    uLambda     = float(lambdaSlider) * 1e-10;
    uAmpladaMul = AmpladaSlider * C_SEPARATION;
  }


  if (experimentPractica && ImGui::Begin("FDM LAB Paramaters")) {
    ImGui::Text("FDB Lab experiments tweak values");
    ImGui::SliderInt("Experiment", &uExperiment, 0, 3);
    ImGui::End();

    static bool showPlot;
    ImGui::Checkbox("Show integration plot", &showPlot);

    if (showPlot) {
      ImGui::SliderFloat("Screen distance", &plotting_distance, 0.0, 1.0);
      static bool currentPlot = 0;
      auto        data        = plot(currentExperiment());

      auto maximum = findLocalMaximumValues(data.y);

      std::vector<float> xMaxData;
      std::vector<float> yMaxData;

      if (maximum.size() > 0) {
        for (int i = 0; i < maximum.size(); i++) {
          xMaxData.push_back(data.x[maximum[i]]);
          yMaxData.push_back(data.y[maximum[i]]);
        }
      }
      std::vector<int> maximum2;
      if (maximum.size() > 0) maximum2 = findLocalMaximumValues(yMaxData);


      static bool normalizeData = false;

      ImGui::Checkbox("Normalize data", &normalizeData);

      if (normalizeData) {
        float minVal = 10e50;
        float maxVal = -10e50;

        for (int i = 0; i < data.y.size(); i++) {
          maxVal = std::max(data.y[i], maxVal);
          minVal = std::min(data.y[i], minVal);
        }

        ImGui::Text("Min value %f\n", minVal);
        ImGui::Text("Max value %f\n", maxVal);


        for (int i = 0; i < data.y.size(); i++) {
          data.y[i] = (data.y[i] - minVal) / (maxVal - minVal);
        }
      }

      if (ImPlot::BeginPlot("FDM", "Distancia en X", "Intensitat llum", ImVec2(800, 400))) {
        ImPlot::PlotLine("Integration", data.x.data(), data.y.data(), data.x.size());

        if (maximum.size() > 0) {
          ImPlot::PlotScatter("Local maxima", xMaxData.data(), yMaxData.data(), xMaxData.size());

          if (maximum2.size() > 0) {
            ImPlot::PlotLine("Local maxima function", xMaxData.data(), yMaxData.data(), xMaxData.size());

            std::vector<float> xMax;
            std::vector<float> yMax;
            for (int i = 0; i < maximum2.size(); i++) {
              xMax.push_back(xMaxData[maximum2[i]]);
              yMax.push_back(yMaxData[maximum2[i]]);
            }

            ImPlot::PlotScatter("Local maxima max", xMax.data(), yMax.data(), xMax.size());
          }
        }

        ImPlot::EndPlot();
      }

      ImGui::Separator();
      if (maximum2.size() > 0) {
        ImGui::Text("Find max maximum %lu\n", maximum2.size());
        for (int i = 0; i < maximum2.size(); i++) {
          ImGui::Text("Local at: %d (%f - %f)\n", maximum2[i], xMaxData[maximum2[i]], yMaxData[maximum2[i]]);
        }

        for (int i = 1; i < maximum2.size(); i++) {
          ImGui::Text("Difference between maximums: %f\n", xMaxData[maximum2[i]] - xMaxData[maximum2[i]]);
        }
      } else {
        ImGui::Text("No max maximum found!\n");
      }
    }
  }
}
void render() {
  glViewport(0, 0, surface->getWidth(), surface->getHeight());
  glUseProgram(program);
  glUniform1f(iTime, uTime);
  glUniform1f(iZoom, uZoom);
  glUniform2f(iResolution, surface->getWidth(), surface->getHeight());
  glUniform1i(iIntegrationMode, uIntegration);
  glUniform1i(iDecayMode, LIGHT_DECAY_ENABLED);
  glUniform1f(iDecayExponent, LIGHT_DECAY_EXPONENT);
  glUniform1i(iExperimentSelector, uExperiment);
  glUniform1f(iDistance, uDistance);
  glUniform1i(iN, NCOUNT);
  glUniform1i(iAmpladaFixa, uAmpladaFixa);
  glUniform1i(iNormalitzarXarxa, uNormalitzarXarxa);
  glUniform1f(iAmpladaMul, uAmpladaMul);
  glUniform1f(iLambda, uLambda);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

/* MAIN CODE */
int main() {
  SurfaceDesc desc;
  desc.width  = 1920;
  desc.height = 1080;
  desc.online = true;
  surface     = NextVideo::surfaceCreate(desc);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  init();
  ImPlot::CreateContext();
  do {
    if (surface->getWidth() > 0 && surface->getHeight() > 0) {
      glBindVertexArray(vao);
      render();
      glBindVertexArray(0);
      surface->beginUI();
      uiRender();
      surface->endUI();
    }
  } while (surface->update());
}
