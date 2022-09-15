#pragma once
#include <GL/glcorearb.h>
#include <glfw/glfw3.h>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "resource_handler_structs.h"

struct Vertex
{
	glm::vec3 position{ 0,0,0 };
	glm::vec3 normal { 0,1,0 };
	glm::vec3 tangent { 0,0,1 };
	glm::vec3 colour { 1,1,1 };
	glm::vec2 texcoord { 0,0 };
};


struct MeshGPU
{
	GLuint vao = 0;
	GLuint vbo = 0;
	int vert_count = 0;
};


struct TextureGPU
{
	GLuint handle = 0;
};

struct ShaderGPU
{
	GLuint handle = 0;
};

struct MaterialGPU
{
	TextureGPU tex_col;
	TextureGPU tex_nrm;
	TextureGPU tex_rgh;
	TextureGPU tex_mtl;
	TextureGPU tex_emm;
	glm::vec4 mul_col;
	glm::vec3 mul_nrm;
	float mul_rgh;
	float mul_mtl;
	glm::vec3 mul_emm;
	glm::vec2 mul_tex;
};

struct ModelGPU
{
	MeshGPU* meshes;
	MaterialGPU* materials;
	int n_meshes;
	int n_materials;
};

struct MeshBufferData
{
	Vertex* verts;
	int n_verts;
};

struct FrameBufferData
{
	GLuint fbo;
	TextureGPU fb_col;
	TextureGPU fb_depth;
	ShaderGPU fb_shader;
	MeshGPU fb_quad;
};

struct RenderContextData
{
	GLFWwindow* window;
	glm::ivec2 resolution{ 1280,720 };
	bool fullscreen = false;
};

enum class ShaderType
{
	vertex,
	pixel,
	geometry,
	compute,
};

struct CameraDataConstantBuffer
{
	glm::mat4 model_matrix = glm::mat4(1.0f);
	glm::mat4 view_matrix = glm::mat4(1.0f);
	glm::mat4 proj_matrix = glm::mat4(1.0f);
	glm::vec3 view_pos;
};

struct MaterialDataConstantBuffer
{
	glm::vec3 mul_col;
	glm::vec3 mul_nrm;
	glm::vec3 mul_rgh;
	glm::vec3 mul_mtl;
	glm::vec3 mul_emm;
	glm::vec2 mul_tex;
};

enum class ConstantBufferType
{
	camera_data,
	material_data
};

struct ConstantBufferGPU
{
	GLuint handle = 0;
};

struct ModelRenderComponent
{
	ResourceHandle model;
};

struct MeshRenderData
{
	MeshGPU mesh;
	MaterialGPU material;
	glm::mat4 model_matrix;
};