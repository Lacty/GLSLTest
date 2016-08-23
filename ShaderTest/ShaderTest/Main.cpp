#include <Windows.h>
#include <fstream>
#include <vector>
#include <memory>

#include <gl/glew.h>
#include <GLFW/glfw3.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

__declspec(align(16))struct Node
{
	void* operator new(size_t size)
	{
		void* p = _aligned_malloc(size, 16);
		if (p == 0)  throw std::bad_alloc();
		return p;
	}

	void operator delete(void *p)
	{
		Node* pc = static_cast<Node*>(p);
		_aligned_free(p);
	}

	~Node()
	{
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(3, buffers);
	}

	Eigen::Matrix4f translation;
	GLuint vao;
	GLuint buffers[3];
	unsigned int face_count;
	aiColor4D diffuse_color;
	aiColor4D ambient_color;
	aiColor4D specular_color;
	aiColor4D emission_color;
	float shinness;
};


enum class ShaderType
{
	VERTEX_SHADER = GL_VERTEX_SHADER,
	FRAGMENT_SHADER = GL_FRAGMENT_SHADER
};

void outPutDebugLog(GLint shader)
{
	GLint info_length = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_length);

	if (info_length > 1)
	{
		GLchar* message = new GLchar[info_length];
		glGetShaderInfoLog(shader, info_length, nullptr, message);
		OutputDebugStringA(message);
		delete[] message;
	}
	else
	{
		OutputDebugStringA("compile error");
	}
}

GLint compileShader
(
	const std::string& file_name,
	ShaderType type
)
{
	GLint shader = (type == ShaderType::VERTEX_SHADER) ?
		glCreateShader(GL_VERTEX_SHADER) : glCreateShader(GL_FRAGMENT_SHADER);

	std::ifstream input_file(file_name);
	std::istreambuf_iterator<char> begin(input_file);
	std::istreambuf_iterator<char> end;
	std::string data(begin, end);
	const char* str = data.c_str();

	glShaderSource(shader, 1, &str, nullptr);
	glCompileShader(shader);

	GLint compile_success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_success);

	if (compile_success == GL_TRUE)return shader;

	outPutDebugLog(shader);

	assert(compile_success == GL_TRUE);

	return -1;
}

GLint linkShader
(
	const std::string& vertex_shader_file_name,
	const std::string& fragment_shader_file_name
)
{
	GLint vertex_sahder =
		compileShader(vertex_shader_file_name, ShaderType::VERTEX_SHADER);

	GLint fragment_sahder =
		compileShader(fragment_shader_file_name, ShaderType::FRAGMENT_SHADER);

	GLint shader_program = glCreateProgram();

	glAttachShader(shader_program, vertex_sahder);
	glAttachShader(shader_program, fragment_sahder);

	glDeleteShader(vertex_sahder);
	glDeleteShader(fragment_sahder);

	glLinkProgram(shader_program);

	GLint compile_success = 0;
	glGetProgramiv(shader_program, GL_LINK_STATUS, &compile_success);

	if (compile_success == GL_TRUE)return shader_program;

	outPutDebugLog(shader_program);

	assert(compile_success == GL_TRUE);

	return -1;
}

float toRadian(float degree)
{
	return degree * M_PI / 180.0f;
}

float toDegree(float radian)
{
	return radian * 180.0f / M_PI;
}

float calcFovy
(
	float fov,
	float aspect,
	float near_z
)
{
	if (aspect >= 1.0f)return fov;

	float half_w = std::tan(toRadian(fov / 2)) * near_z;

	float hald_h = half_w / aspect;

	return toDegree(std::atan(half_w / near_z) * 2);
}

// 計算式がwebによって異なるのが謎
//　もしかすると座標系が関係しているかも
Eigen::Matrix4f perspectiveMatrix
(
	float fov_y,
	float aspect,
	float near_z,
	float far_z
)
{
	Eigen::Matrix4f matrix;

	float fov_y_rad = toRadian(fov_y);
	float f = 1.0f / std::tan(fov_y_rad / 2.0f);
	float d = far_z - near_z;

	matrix <<
		f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, f, 0.0f, 0.0f,
		0.0f, 0.0f, -(far_z + near_z) / d, -(2.0f * far_z * near_z) / d,
		0.0f, 0.0f, -1.0f, 0.0f;

	return matrix;
}

Eigen::Matrix4f orthoMatrix
(
	float width,
	float height,
	float near_z,
	float far_z
)
{
	Eigen::Matrix4f matrix;
	matrix <<
		2 / width, 0, 0, 0,
		0, 2 / height, 0, 0,
		0, 0, 1 / (far_z - near_z), 1,
		0, 0, near_z / (near_z - far_z), 0;

	return matrix;
}


Eigen::Matrix4f lookAt
(
	const Eigen::Vector3f& eye,
	const Eigen::Vector3f& target
)
{
	//	MEMO:カメラ座標いじくる

	auto forward = (target - eye).normalized();

	Eigen::Translation3f translation(eye);
	auto rotation = Eigen::Quaternionf::FromTwoVectors(Eigen::Vector3f::UnitZ(), forward);

	Eigen::Affine3f matrix = translation * rotation;

	return matrix.matrix();
}

static std::vector<std::shared_ptr<Node>> nodes;

void createNode(const aiNode* node, const aiScene* scene, const aiMatrix4x4& parent)
{
	aiMatrix4x4 local_matrix = parent * node->mTransformation;

	for (int i = 0; i < node->mNumMeshes; ++i)
	{
		std::shared_ptr<Node> temp_node = std::shared_ptr<Node>(new Node);

		auto transpose = local_matrix;
		transpose.Transpose();

		temp_node->translation = Eigen::Matrix4f(transpose[0]);

		glGenVertexArrays(1, &temp_node->vao);
		glBindVertexArray(temp_node->vao);

		glGenBuffers(3, temp_node->buffers);

		std::vector<unsigned int> indices;

		temp_node->face_count = scene->mMeshes[i]->mNumFaces;

		for (int j = 0; j < scene->mMeshes[i]->mNumFaces; ++j)
		{
			for (int k = 0; k < scene->mMeshes[i]->mFaces[j].mNumIndices; ++k)
			{
				indices.emplace_back(scene->mMeshes[i]->mFaces[j].mIndices[k]);
			}
		}


		glBindBuffer(GL_ARRAY_BUFFER, temp_node->buffers[0]);
		glBufferData(GL_ARRAY_BUFFER, 3 * scene->mMeshes[i]->mNumVertices * sizeof(GLfloat), scene->mMeshes[i]->mVertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, temp_node->buffers[1]);
		glBufferData(GL_ARRAY_BUFFER, 3 * scene->mMeshes[i]->mNumVertices * sizeof(GLfloat), scene->mMeshes[i]->mNormals, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, temp_node->buffers[2]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// Material
		auto material_index = scene->mMeshes[i]->mMaterialIndex;

		aiColor4D diffuse;
		scene->mMaterials[material_index]->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		temp_node->diffuse_color = diffuse;

		aiColor4D ambient;
		scene->mMaterials[material_index]->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
		temp_node->ambient_color = ambient;

		aiColor4D specular;
		scene->mMaterials[material_index]->Get(AI_MATKEY_COLOR_SPECULAR, specular);
		temp_node->specular_color = specular;

		float shinness = 10.0f;
		scene->mMaterials[material_index]->Get(AI_MATKEY_SHININESS, shinness);
		temp_node->shinness = shinness;

		aiColor4D emssion;
		scene->mMaterials[material_index]->Get(AI_MATKEY_COLOR_EMISSIVE, emssion);
		temp_node->emission_color = emssion;

		nodes.emplace_back(temp_node);

	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		createNode(node->mChildren[i], scene, local_matrix);
	}
}

int main()
{
	GLFWwindow* window;

	glfwInit();

	window = glfwCreateWindow(640, 480, "ShaderTest", nullptr, nullptr);

	glfwMakeContextCurrent(window);

	glewInit();

	GLint shader_program = linkShader("sample.vert", "sample.frag");

	GLint attr_pos = glGetAttribLocation(shader_program, "attr_pos");
	GLint attr_normal = glGetAttribLocation(shader_program, "attr_normal");
	GLint lwp_matrix_id = glGetUniformLocation(shader_program, "lwp_matrix");
	GLint diffuse_material = glGetUniformLocation(shader_program, "diffuse_material");
	GLint ambient_material = glGetUniformLocation(shader_program, "ambient_material");
	GLint specular_material = glGetUniformLocation(shader_program, "specular_material");
	GLint emission_material = glGetUniformLocation(shader_program, "emission_material");
	GLint shinness_material = glGetUniformLocation(shader_program, "shinness_material");

	glUseProgram(shader_program);

	glEnable(GL_DEPTH_TEST);

	int width, height;
	glfwGetWindowSize(window, &width, &height);

	float aspect = static_cast<float>(width) / height;

	auto fov_y = calcFovy(60.0f, aspect, 0.1f);
	auto perspective_matrix = perspectiveMatrix(fov_y, aspect, 0.1f, 1000.0f);

	// OpenGLは右手座標系（奥にいけばいくほどマイナス）
	// Eigenは右から順に掛けていく
	Eigen::Affine3f camera_matrix =
		Eigen::Translation3f(-Eigen::Vector3f::UnitZ() * 10)
		* Eigen::Quaternionf::Identity();

	auto camera_matrix_test =
		lookAt(-Eigen::Vector3f::UnitZ() * 10, Eigen::Vector3f::Zero());

	Eigen::Matrix4f lwp_matrix =
		perspective_matrix
		* camera_matrix_test.matrix();

	// Buffer
	GLuint vao;

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	GLuint buffers[3];
	glGenBuffers(3, buffers);

	struct Test
	{
		aiVector3D position;
		aiVector3D normal;
	};

	Test test[3] =
	{
		{ aiVector3D(0.0f, 1.0f, 0.0f) ,aiVector3D(0.0f, 0.0f, -1.0f) },
		{ aiVector3D(1.0f,-1.0f,0.0f) ,aiVector3D(0.0f, 0.0f, -1.0f) },
		{ aiVector3D(-1.0f,-1.0f,0.0f) ,aiVector3D(0.0f, 0.0f, -1.0f) },
	};

	Eigen::Vector3f positions[] =
	{
		Eigen::Vector3f(0.0f,1.0f,0.0f),
		Eigen::Vector3f(1.0f,-1.0f,0.0f),
		Eigen::Vector3f(-1.0f,-1.0f,0.0f),
	};

	const GLfloat normals[] =
	{
		0.0f,0.0f,-1.0f,
		0.0f,0.0f,-1.0f,
		0.0f,0.0f,-1.0f,
	};

	const GLuint indices[] =
	{
		0,1,2
	};

	unsigned int offset = 0;

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(test), test, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Test), (const void*)offset);
	glEnableVertexAttribArray(0);



	offset += sizeof(aiVector3D);

	//glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(normals), normals, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Test), (const void*)offset);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile
	(
		"penguin.x",
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType
	);



	createNode(scene->mRootNode, scene, aiMatrix4x4());


	while (!glfwWindowShouldClose(window))
	{
		glClearColor(0.0f, 0.0f, 1.0f, 1.0f);

		glClear
		(
			GL_COLOR_BUFFER_BIT |
			GL_DEPTH_BUFFER_BIT |
			GL_STENCIL_BUFFER_BIT
		);

		static float angle = 0.0f;
		angle += 0.05f;

		Eigen::Affine3f rotation = Eigen::Translation3f(Eigen::Vector3f(0,0,-10)) * Eigen::Quaternionf(Eigen::AngleAxisf(angle,Eigen::Vector3f::UnitX()));

		for (auto& node : nodes)
		{
			glBindVertexArray(node->vao);

			Eigen::Matrix4f matrix = lwp_matrix * rotation.matrix();

			glUniformMatrix4fv(lwp_matrix_id, 1, GL_FALSE, matrix.data());
			glUniform4f(diffuse_material, node->diffuse_color.r, node->diffuse_color.g, node->diffuse_color.b, node->diffuse_color.a);
			glUniform4f(ambient_material, node->ambient_color.r, node->ambient_color.g, node->ambient_color.b, node->ambient_color.a);
			glUniform4f(specular_material, node->specular_color.r, node->specular_color.g, node->specular_color.b, node->specular_color.a);
			glUniform4f(emission_material, node->emission_color.r, node->emission_color.g, node->emission_color.b, node->emission_color.a);
			glUniform1f(shinness_material, node->shinness);


			//glDrawArrays(GL_TRIANGLES, 0, node->face_count * 3);
			glDrawElements(GL_TRIANGLES, node->face_count * 3, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);
		}

		//glBindVertexArray(vao);
		//glUniformMatrix4fv(lwp_matrix_id, 1, GL_FALSE, lwp_matrix.data());

		////glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
		//glDrawArrays(GL_TRIANGLES, 0, 3);
		//glBindVertexArray(0);


		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteBuffers(3, buffers);
	glDeleteVertexArrays(1, &vao);
	glfwTerminate();

	return 0;
}