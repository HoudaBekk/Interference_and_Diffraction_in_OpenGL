#version 330 core
layout(location = 0)  in vec3 a_Position;
layout(location = 1)  in vec3 a_Normal;
layout(location = 2)  in vec2 a_UV;

uniform mat4 u_ViewMat;
uniform mat4 u_ProjMat;
uniform mat4 u_WorldMat;
uniform vec2 u_uvScale = vec2(1,1);
uniform vec2 u_uvOffset = vec2(0,0);
out vec2 f_uv;
out vec4 f_pos;
out vec3 f_normal;

uniform bool u_flatUV;
void main() { 

    f_pos = u_WorldMat * vec4(a_Position, 1.0);
    gl_Position = u_ProjMat * u_ViewMat * f_pos;
    f_uv = a_UV * u_uvScale + u_uvOffset;
    if(u_flatUV) { 
      f_uv = f_pos.xz;
    }
    f_normal = a_Normal;
}
