#version 440

uniform mat4 lwp_matrix;
layout (location = 0) in vec4 attr_pos;
layout (location = 1) in vec3 attr_normal;
out float diffuse;
out float specular;

void main()
{
	vec3 light = vec3(0.0,-1.0,-1.0);
	light = -normalize(light);
	diffuse = max(0.0,dot(light,attr_normal));

	vec4 position = lwp_matrix * attr_pos;
	vec3 view = -normalize(position.xyz);
	vec3 halfway = normalize(view + light);

	const int shinness = 10;

	specular = pow
		(
			max
			(
				dot(attr_normal,halfway),0.0
			),
			shinness
		);

	gl_Position = position;
}