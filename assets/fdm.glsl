#version 300 es
precision highp float;

out vec3 color;
uniform vec2 iMouse;
uniform float iTime;
uniform float iZoom;
uniform float iDistance;
uniform vec2 iResolution;
uniform mat4 iCameraTransform;

#define M_PI 3.1415926535897932384626433832795
#define C 299792458.0
#define LIGHT_DECAY_ENABLED
#define LIGHT_DECAY_FACTOR 1.0e-5
#define INTEGRATION_ENABLED
#define INTEGRATION_STEPS 4
#define GAMMA_CORRECTED 1.0
#define A_WAVE 5000e-10
#define A_SEPARATION 0.01e-3
#define A_L = 200.0e-3
uniform int N;
#define ZOOM 1e-4
#define TIME_ZOOM (1e-6 / C)

uniform float iLambda;
uniform bool iIntegrationMode;
uniform bool iDecayMode;
uniform bool iAmpladaFixa;
uniform float iAmpladaMul;
uniform bool iNormalitzarXarxa;
uniform float iDecayExponent;
uniform int iExperimentSelector;

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 colorRamp(float t) { 
    vec3 up = vec3(0.5,0.5,1);
    vec3 down = vec3(1,-0.2,-0.2);

    return up * t + down * (1.0 - t);
}

//Return world coordinates
vec2 getSt() { 
    vec2 st = gl_FragCoord.xy / iResolution;
    st = st - 0.5;
    st = st * iZoom * 10.0;
    return st;
}

vec2 realSt() { 
     vec2 st = (( gl_FragCoord.xy / iResolution.xy - 0.5))
      * ZOOM * iZoom;
     return st;
}

#define LAMBDA iLambda

//Inverse square intensity falloff
float lightValue(vec2 st) { 
    return pow(10.0, -iDecayExponent) / sqrt(st.x * st.x + st.y * st.y);
}
//Ligth wave intensity value for a wave with frequency w
float light(vec2 st, float t) { 
    float l = length(vec3(st.x, st.y, iDistance));
    float k = 2.0 * M_PI / LAMBDA;
	  float f = C / LAMBDA;
	  float w = f * 2.0 * M_PI;
    float result = sin(l * k - t * w);
    if(!iIntegrationMode) result = result *0.5 + 0.5;
    if(iDecayMode) result = result * lightValue(st);
    return result;
}




//Show light wave for centered omni light
float test1(vec2 st, float t) { 
    return light(st * 10.0, t);
}

//Show light intensity for same case than 1
float test2(vec2 st, float t) { 
    return lightValue(st);
}

//Combine both graphs, with interference pattern combined with p1 and p2
float test3(vec2 st, float t) { 
    st = st * 20.0;
    vec2 p1 = vec2(30.0,0.0) + st;
    vec2 p2 = vec2(-30.0,0.0) + st;

    return (light(p1, t) + light(p2, t)) * 0.5;
}

//Single slip experiment
float test4(vec2 st, float t)
{
    float result = 0.0;
    float resultValue = 0.0;
    float altResult = 0.0;

    int count = 0;
    for(float k = 0.0; k <= 2.0; k+= 2.0) { 
    for(float i = -1.0; i < 2.0; i+= 0.1) { 
        count++;
        vec2 off = vec2(0.0,-10.3 + i + k * 6.0 * sin(iTime * 0.002)) * 10.0;
        vec2 p = off * ZOOM + st;
        float lig = light(p, t);
        altResult = max(lig, altResult);
        result += lig;
        resultValue += lightValue(p);
    }
    }

    result /= count;
    return result;
}

float net(vec2 st, float off, float t, float separation) { 
	float result = 0.0;
  if(iAmpladaFixa)
    separation = separation / float(N);
	float offset = -float(N) * separation * 0.5 + off;
	for(int i = 0; i < N; i++) { 
		result += light(st + vec2(0,offset), t);
		offset += separation;
	}
  if(iNormalitzarXarxa)
    return result / float(N);
	return result / float(N);
}

#define B_SEPARATION 0.01e-3
float testHouda(vec2 st, float t) { 
	return net(st, 0.0,t, B_SEPARATION * 0.3) * 0.5 + net(st, B_SEPARATION * float(N) * 0.4,t, B_SEPARATION * 0.3) * 0.5;
}


float experimentA(vec2 st, float t) { 
	return light(st + vec2(0.0, -A_SEPARATION * 0.5), t) * 0.5  + light(st + vec2(0.0, A_SEPARATION * 0.5), t) * 0.5;
}

float experimentB(vec2 st, float t) { 
	return net(st, 0.0,t, iAmpladaMul * 10.0);
}

#define C_SEPARATION 0.001e-3

float experimentC(vec2 st, float t) {
	return net(st, 0.0,t, iAmpladaMul);
}

float experimentD(vec2 st, float t) { 
	float o = 0.1e-3;
	return net(st, -o/2.0,t, C_SEPARATION) * 0.5 + net(st, o/2.0,t, C_SEPARATION) * 0.5;
}

float experiment(vec2 st, float t) { 
    if( iExperimentSelector == 0) return experimentA(st, t);
    if( iExperimentSelector == 1) return experimentB(st, t);
    if( iExperimentSelector == 2) return experimentC(st, t);
    if( iExperimentSelector >= 3) return experimentD(st, t);
}

float fft(vec2 st, float t) { 
    //st *= 100.0;
    float maxF = -100.0;
    float minF = 100.0;

    for(float x = 0.0; x < 1000.0; x+= 100.0) { 
        float l = experiment(st, t + x);
        maxF = max(maxF, l);
        minF = min(minF, l);
    }
    return maxF - minF;
}

float executar(vec2 st, float tP) { 
	float result = 0.0;
	float f = C / A_WAVE;
	float w = f * 2.0 * M_PI;
	float L = float(INTEGRATION_STEPS);
	float dt = 2.0 * M_PI / (L * w);
	float t = 0.0;
	for(int i = 0; i < INTEGRATION_STEPS; i++) { 
		float partial = experiment(st, t + tP);
		result += partial * partial;
		t += dt;
	}
	return result / L;
}

void main() { 
  vec2 st = realSt();
  float result;
  if(iIntegrationMode) result = executar(st, iTime * TIME_ZOOM);
  else result = experiment(st, iTime * TIME_ZOOM);

  color = vec3(result);
}
