#version 440

uniform vec4 diffuse_material;
uniform vec4 ambient_material;
uniform vec4 specular_material;
uniform vec4 emission_material;
uniform float shinness_material;
out vec4 fragColor;
in float diffuse;
in float specular;

void main()
{
	vec4 diffuse_color = diffuse_material * diffuse;
	vec4 ambient_color = ambient_material;
	vec4 specular_color = specular_material * specular;
	fragColor = diffuse_color + ambient_color + specular_color + emission_material;
}