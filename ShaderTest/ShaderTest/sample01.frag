out vec4 fragColor;
in vec4 position;
in vec3 normal;

void main()
{
	vec3 light = vec3(0.0,1.0,1.0);
	light = -normalize(light);
	float diffuse = max(0.0,dot(light,normal));

	vec3 view = -normalize(position.xyz);
	vec3 halfway = normalize(view + light);

	const int shinness = 10;

	float specular = pow
		(
			max
			(
				dot(normal,halfway),0.0
			),
			shinness
		);

	vec4 diffuse_color = vec4(1.0,0.0,0.0,1.0) * diffuse;
	vec4 ambient_color = vec4(0.2,0.2,0.2,0.2);
	vec4 specular_color = vec4(1.0,1.0,1.0,1.0) * specular;
	fragColor = diffuse_color + ambient_color + specular_color;
}