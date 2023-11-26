#version 300 es
out vec2 uv;
void main() { 
	const vec2 verticesData[6] = vec2[6](
		vec2(0.0,0.0),
		vec2(0.0,1.0),
		vec2(1.0,1.0),
		vec2(1.0,1.0),
		vec2(1.0,0.0),
		vec2(0.0,0.0)
	);

	uv = verticesData[gl_VertexID];
	gl_Position.xy = uv * 2.0 - 1.0;
	gl_Position.zw = vec2(0.0,1.0);
}
