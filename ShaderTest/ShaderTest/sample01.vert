uniform mat4 lwp_matrix;
in vec4 attr_pos;
in vec3 attr_normal;
out vec4 position;
out vec3 normal;

void main()
{
	position = lwp_matrix * attr_pos;
	normal = attr_normal;


	gl_Position = position;
}