#version 330 core
  
in vec2 uv;
out vec3 color;

uniform sampler2D u_input;
uniform bool u_horizontal;

void main()
{             
    float w = 1;
    float f = 0.8;
    int kernelSize = 8;
    vec2 tex_offset = 1.0 / textureSize(u_input, 0); // gets size of single texel
    vec3 result = texture(u_input, uv).rgb * w; // current fragment's contribution
    if(u_horizontal)
    {
        w *= f;
        for(int i = 1; i < kernelSize; ++i) {
            result += texture(u_input, uv + vec2(tex_offset.x * i, 0.0)).rgb * w;
            result += texture(u_input, uv - vec2(tex_offset.x * i, 0.0)).rgb * w;
            w *= f;
        }
    }
    else
    {
        w *= f;
        for(int i = 1; i < kernelSize; ++i) {
            result += texture(u_input, uv + vec2(0.0, tex_offset.y * i)).rgb * w;
            result += texture(u_input, uv - vec2(0.0, tex_offset.y * i)).rgb * w;
            w *= f;
        }
    }
    color = result * 0.14;
}
