#version 330 core

layout(location = 0)  in vec3 a_Position;
layout(location = 1)  in vec4 a_Color;
layout(location = 2)  in vec2 a_UV;

out vec4 color;

uniform mat4 u_ViewMat;
uniform mat4 u_ProjMat;
uniform mat4 u_WorldMat;

void main() { 
  gl_Position = u_ProjMat * u_ViewMat * u_WorldMat * vec4(a_Position, 1.0);
  color = a_Color;
}
