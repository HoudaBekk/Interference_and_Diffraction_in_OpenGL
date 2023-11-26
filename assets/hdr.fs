#version 330 core
in vec2 uv;
uniform sampler2D u_color;
uniform sampler2D u_bloom;

out vec3 color;
void main() { 
  color = texture2D(u_color, uv).xyz * 1.0 + texture2D(u_bloom, uv).xyz * 0.05;
}
