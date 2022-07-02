#version 450

layout (location=0) out vec4 v_color;

void main()
{
	//const array of positions for the triangle
	const vec3 positions[3] = vec3[3](
		vec3(-1.f,1.f, 0.0f),
		vec3(0.f,-1.f, 0.0f),
		vec3(1.f,1.f, 0.0f)
	);
	const vec4 colors[3] = vec4[3](
		vec4(1.f,0.f, 0.0f, 1.0f),
		vec4(0.f,1.f, 0.0f, .0f),
		vec4(0.f,0.f, 1.0f, 1.f)
	);

	//output the position of each vertex
	gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
	v_color = colors[gl_VertexIndex];
}
