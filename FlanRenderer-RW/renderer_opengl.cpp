#include <GL/gl3w.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_opengl3_loader.h>

#include "common_defines.h"
#include "input.h"
#include "logger.h"
#include "renderer.h"
#include "resource_manager.h"

static void debug_callback_func(
	GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const GLvoid* userParam);

ShaderGPU Renderer::load_shader(std::string path)
{
	const ShaderGPU shader_gpu{ glCreateProgram() };

	bool vert_loaded = load_shader_part(path + ".vert", ShaderType::vertex, shader_gpu);
	bool frag_loaded = load_shader_part(path + ".frag", ShaderType::pixel, shader_gpu);
	bool comp_loaded = load_shader_part(path + ".comp", ShaderType::compute, shader_gpu);
	load_shader_part(path + ".geom", ShaderType::geometry, shader_gpu);

	if (
		(vert_loaded && frag_loaded) == false &&
		comp_loaded == false)
	{
		Logger::logf("[ERROR] Failed to load shader '%s'! Either the shader files do not exist, or a compilation error occurred.\n", path.c_str());
	}
	if (
		(vert_loaded || frag_loaded) == true &&
		comp_loaded == true)
	{
		Logger::logf("[ERROR] Failed to load shader '%s'! Unsure whether to use vertex/fragment shaders, or compute shaders, since both exist.\n", path.c_str());
	}

	glLinkProgram(shader_gpu.handle);

	loaded_shaders[ResourceManager::generate_hash_from_string(path)] = shader_gpu;

	return shader_gpu;
}

bool Renderer::is_running()
{
	return !glfwWindowShouldClose(render_ctx.window);
}

void Renderer::update_camera_view(glm::mat4 view_matrix)
{
	if (camera_data == nullptr)
		return;
	camera_data->view_matrix = view_matrix;
	camera_data->view_pos = glm::vec3(glm::inverse(view_matrix)[3]);
}

void Renderer::update_camera_proj(glm::mat4 proj_matrix)
{
	if (camera_data == nullptr)
		return;
	camera_data->proj_matrix = proj_matrix;
}

void* Renderer::get_window()
{
	return render_ctx.window;
}

TextureGPU Renderer::get_framebuffer_texture()
{
	return fb_data.fb_col;
}

void Renderer::bind_texture(int slot, TextureGPU texture)
{
	glBindTextureUnit(slot, texture.handle);
}

void Renderer::bind_mesh(MeshGPU mesh)
{
	glBindVertexArray(mesh.vao);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
}

void Renderer::bind_shader(ShaderGPU shader)
{
	glUseProgram(shader.handle);
}

void Renderer::bind_material_pbr(MaterialGPU material)
{
	//Bind PBR shader
	bind_shader(pbr_shader);

	//Set roughness power uniform
	glUniform1f(6, rgh_pow);
	glUniform1i(8, flip_normal_y);

	//Bind textures - if a material doesn't have a texture for a certain slot, that texture handle is 0, which means "no texture" in OpenGL
	if (material.tex_col.handle != 0) { bind_texture(0, material.tex_col); } else { bind_texture(0, tex_default_col); }
	if (material.tex_nrm.handle != 0) { bind_texture(1, material.tex_nrm); } else { bind_texture(1, tex_default_nrm); }	
	if (material.tex_mtl.handle != 0) { bind_texture(2, material.tex_mtl); } else { bind_texture(2, tex_default_mtl); }
	if (material.tex_rgh.handle != 0) { bind_texture(3, material.tex_rgh); } else { bind_texture(3, tex_default_rgh); }
	bind_texture(4, ibl_brdf_lut);
	bind_texture(5, curr_cubemap);

	//Create and bind constant buffer
	ConstantBufferGPU material_const_buffer_gpu{};
	auto* material_buffer_data =  static_cast<MaterialDataConstantBuffer*>(allocate_temporary(sizeof(MaterialDataConstantBuffer)));
	memcpy(&material_buffer_data->mul_col.r, &material.mul_col.r, sizeof(MaterialDataConstantBuffer));
	init_or_update_constant_buffer<MaterialDataConstantBuffer>(static_cast<int>(ConstantBufferType::material_data), material_const_buffer_gpu, material_buffer_data);
	glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<int>(ConstantBufferType::material_data), material_const_buffer_gpu.handle);
	temporary_const_buffers.push_back(material_const_buffer_gpu);
}

void Renderer::create_render_context()
{
	//Create window
	render_ctx.window = glfwCreateWindow(
		render_ctx.resolution.x,
		render_ctx.resolution.y, 
		"FlanRenderer-RW (OpenGL)", 
		nullptr, 
		nullptr);

	//Focus on window, init gl3w, and set debug callback
	glfwMakeContextCurrent(render_ctx.window);
	gl3wInit();
	glDebugMessageCallback(debug_callback_func, nullptr);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	//Set clear colour and disable vsync
	glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
	glfwSwapInterval(0);
}

void Renderer::init_framebuffer()
{
	//Generate framebuffer
	glGenFramebuffers(1, &fb_data.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fb_data.fbo);

	//Generate colour buffer
	glGenTextures(1, &fb_data.fb_col.handle);
	glBindTexture(GL_TEXTURE_2D, fb_data.fb_col.handle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, render_ctx.resolution.x, render_ctx.resolution.y, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb_data.fb_col.handle, 0);

	//Generate depth stencil buffer
	glGenRenderbuffers(1, &fb_data.fb_depth.handle);
	glBindRenderbuffer(GL_RENDERBUFFER, fb_data.fb_depth.handle);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, render_ctx.resolution.x, render_ctx.resolution.y);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb_data.fb_depth.handle);

	//Error checking
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		printf("Framebuffer is not complete!");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	//Framebuffer render data
	float quad[] =
	{
		//Vertices
		1,  1,
		-1, -1,
		-1,  1,
		1,  1,
		1, -1,
		-1, -1,

		//Texcoords
		1, 1,
		0, 0,
		0, 1,
		1, 1,
		1, 0,
		0, 0,
	};
	fb_data.fb_shader = load_shader("Assets/Shaders/framebuffer");
	glGenVertexArrays(1, &fb_data.fb_quad.vao);
	glGenBuffers(1, &fb_data.fb_quad.vbo);
	glBindVertexArray(fb_data.fb_quad.vao);
	glBindBuffer(GL_ARRAY_BUFFER, fb_data.fb_quad.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), &quad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)(12 * sizeof(float)));
}

TextureGPU Renderer::upload_texture_to_gpu(const ResourceHandle texture_handle, bool is_srgb, bool unload_resource_afterwards)
{
	if (texture_handle.type == ResourceType::invalid)
		return { 0 };
	//Get texture resource
	auto* texture_resource = resource_manager->get_resource<TextureResource>(texture_handle);
	Logger::logf("Loading texture '%s', size = %ix%i\n", texture_resource->name, texture_resource->width, texture_resource->height);

	//Create texture on GPU
	TextureGPU texture_gpu{};
	glGenTextures(1, &texture_gpu.handle);
	glBindTexture(GL_TEXTURE_2D, texture_gpu.handle);
	glTexImage2D(GL_TEXTURE_2D, 0, is_srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, texture_resource->width, texture_resource->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_resource->data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

	if (unload_resource_afterwards)
	{
		texture_resource->schedule_unload();
	}

	loaded_textures[texture_handle.hash] = texture_gpu;
	return texture_gpu;
}

TextureGPU Renderer::upload_cubemap_to_gpu(std::vector<ResourceHandle> texture_handle, bool unload_resource_afterwards)
{
	//Make sure there are enough sides for the cubemap
	if (texture_handle.size() != 6)
	{
		Logger::logf("[ERROR] Texture resources list does not have 6 entries! Skipping...");
		return { 0 };
	}

	//Create texture object
	TextureGPU texture;

	glGenTextures(1, &texture.handle);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texture.handle);

	//Loop over each cubemap entry; It should be in order -X, +X, -Y, +Y, -Z, +Z
	for (int i = 0; i < 6; i++)
	{
		auto* tex_resource = resource_manager->get_resource<TextureResource>(texture_handle[i]);
		tex_resource->scheduled_for_unload = true;

		if (tex_resource == nullptr)
		{
			Logger::logf("Cubemap face %i: texture \"%s\" could not be loaded!", i, tex_resource->name);
			continue;
		}
		if (tex_resource->data == nullptr)
		{
			Logger::logf("Cubemap face %i: texture \"%s\" could not be loaded!", i, tex_resource->name);
			continue;
		}
		
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, tex_resource->width, tex_resource->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_resource->data);
			
	}
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	return texture;
}

ModelGPU Renderer::upload_mesh_to_gpu(ResourceHandle model_handle, bool unload_resources)
{
	//Get model resource
	ModelResource* model_resource = resource_manager->get_resource<ModelResource>(model_handle);

	//Create output variable
	ModelGPU model_gpu;
	model_gpu.meshes = (MeshGPU*)dynamic_allocate(sizeof(MeshGPU) * model_resource->n_meshes);
	model_gpu.materials = (MaterialGPU*)dynamic_allocate(sizeof(MaterialGPU) * model_resource->n_materials);
	model_gpu.n_meshes = model_resource->n_meshes;
	model_gpu.n_materials = model_resource->n_materials;

	//Set default handles to 0 by memsetting
	memset(model_gpu.materials, 0, sizeof(MaterialGPU) * model_resource->n_materials);

	//Parse all meshes
	for (int i = 0; i < model_resource->n_meshes; i++)
	{
		model_gpu.meshes[i] = init_vertex_buffer(model_resource->meshes[i].verts, model_resource->meshes[i].n_verts);
		dynamic_free(model_resource->meshes[i].verts);
	}

	//Parse all materials
	for (int i = 0; i < model_resource->n_materials; i++)
	{
		model_gpu.materials[i].tex_col = upload_texture_to_gpu(model_resource->materials[i].tex_col, false, true);
		model_gpu.materials[i].tex_nrm = upload_texture_to_gpu(model_resource->materials[i].tex_nrm, false, true);
		model_gpu.materials[i].tex_mtl = upload_texture_to_gpu(model_resource->materials[i].tex_mtl, false, true);
		model_gpu.materials[i].tex_rgh = upload_texture_to_gpu(model_resource->materials[i].tex_rgh, false, true);
		/*
		if (model_resource->materials[i].tex_col != nullptr) {
			const std::string name = model_resource->materials[i].tex_col->name;
			const auto resource = model_resource->materials[i].tex_col;
			const auto handle = resource_manager->load_resource_from_buffer<TextureResource>(name, resource);
			model_gpu.materials[i].tex_col = upload_texture_to_gpu(handle, true);
		}
		*/
	}


	loaded_models[model_handle.hash] = model_gpu;

	return model_gpu;
}

void Renderer::clear_framebuffer()
{
	glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::platform_specific_begin_frame()
{
	glfwMakeContextCurrent(render_ctx.window);
	glfwPollEvents();
	glBindFramebuffer(GL_FRAMEBUFFER, fb_data.fbo);
	glFrontFace(GL_CCW);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}

void Renderer::platform_specific_end_frame()
{
	for (auto buffer : temporary_const_buffers)
	{
		glDeleteBuffers(1, &buffer.handle);
	}
	temporary_const_buffers.clear();

	glBindFramebuffer(GL_FRAMEBUFFER, fb_data.fbo);
	glFrontFace(GL_CCW);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	/*
	//TODO: remove this
	if (Input::keys_held[GLFW_KEY_I])
		rgh_pow += 0.0001f;
	if (Input::keys_held[GLFW_KEY_K])
		rgh_pow -= 0.0001f;
	if (Input::keys_held[GLFW_KEY_U])
		rgh_mul += 0.0001f;
	if (Input::keys_held[GLFW_KEY_J])
		rgh_mul -= 0.0001f;
	printf("\nrgh_pow = %f,\trgh_mul = %f\n", rgh_pow, rgh_mul);

	glProgramUniform1f(pbr_shader.handle, 6, rgh_pow);
	glProgramUniform1f(pbr_shader.handle, 7, rgh_mul);
	*/
}

template <typename T>
void Renderer::init_or_update_constant_buffer(int slot, ConstantBufferGPU& const_buffer, T*& buffer_data)
{
	//If the buffer_data pointer == nullptr, create the buffer_data object
	if (buffer_data == nullptr)
	{
		ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "constant buffer";
		buffer_data = static_cast<T*>(dynamic_allocate(sizeof(T)));
		ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "untitled";
		memset(buffer_data, 0, sizeof(T));
	}
	//If the buffer doesn't already exist on GPU, create it 
	if (const_buffer.handle == 0)
	{
		glGenBuffers(1, &const_buffer.handle);
		glBindBuffer(GL_UNIFORM_BUFFER, const_buffer.handle);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(T), &buffer_data[0], GL_STATIC_DRAW);
	}

	glBindBuffer(GL_UNIFORM_BUFFER, const_buffer.handle);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(T), &buffer_data[0]);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
template void Renderer::init_or_update_constant_buffer(int slot, ConstantBufferGPU& const_buffer, CameraDataConstantBuffer*& buffer_data);
template void Renderer::init_or_update_constant_buffer(int slot, ConstantBufferGPU& const_buffer, MaterialDataConstantBuffer*& buffer_data);

void Renderer::platform_specific_init()
{
	//Init GLFW and set the OpenGL version to 4.6
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//Enable debugging
#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#else
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE);
#endif
}

MeshGPU Renderer::init_vertex_buffer(Vertex* vertices, int n_vertices)
{
	MeshGPU mesh_gpu{};

	//Set number of vertices
	mesh_gpu.vert_count = n_vertices;

	//Generate buffers on GPU
	glGenVertexArrays(1, &mesh_gpu.vao);
	glGenBuffers(1, &mesh_gpu.vbo);

	//Bind buffers on GPU
	glBindVertexArray(mesh_gpu.vao);
	glBindBuffer(GL_ARRAY_BUFFER, mesh_gpu.vbo);

	//Setup vertex arrays
	glVertexAttribPointer(0, sizeof(Vertex::position) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position.x)));
	glVertexAttribPointer(1, sizeof(Vertex::normal  ) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal  .x)));
	glVertexAttribPointer(2, sizeof(Vertex::tangent ) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, tangent .x)));
	glVertexAttribPointer(3, sizeof(Vertex::colour  ) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, colour  .x)));
	glVertexAttribPointer(4, sizeof(Vertex::texcoord) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord.x)));

	//Enable vertex arrays
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glEnableVertexAttribArray(4);

	//Send vertices to vertex buffer
	glBufferData(GL_ARRAY_BUFFER, static_cast<int>(sizeof(Vertex) * n_vertices), &vertices[0], GL_STATIC_DRAW);

	//Unbind buffers
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return mesh_gpu;
}

//Tries to load a shader stage. If the shader part can not be found, it will be skipped silently
bool Renderer::load_shader_part(const std::string& path, ShaderType type, const ShaderGPU& program)
{
	constexpr int shader_types[]
	{
		GL_VERTEX_SHADER,
		GL_FRAGMENT_SHADER,
		GL_GEOMETRY_SHADER,
		GL_COMPUTE_SHADER,
	};

	//Read header file
	int header_size;
	char* header_data;
	ResourceManager::read_file("Assets/Shaders/const_buffer_header.h", header_size, header_data, false);
	if (header_data == nullptr)
	{
		Logger::logf("[ERROR] Header file 'Assets/Shaders/const_buffer_header.h' is missing. Please make sure this file exists.\n");
		//Clean up
		dynamic_free(header_data);
		return false;
	}

	//Read shader source file
	int shader_size;
	char* shader_data;
	ResourceManager::read_file(path, shader_size, shader_data, true);

	if (shader_data == nullptr)
	{
		//Clean up
		dynamic_free(shader_data);
		dynamic_free(header_data);
		return false;
	}

	//Combine them together
	const int combined_size = header_size + 1 + shader_size;
	ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "shader loading";
	char* combined_data = static_cast<char*>(dynamic_allocate(header_size + 1 + shader_size));
	ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "untitled";
	memcpy(combined_data, header_data, header_size);
	*(combined_data + header_size) = '\n';
	memcpy(combined_data + header_size + 1, shader_data, shader_size);

	//Create shader on GPU
	const GLuint type_to_create = shader_types[static_cast<int>(type)];
	const GLuint shader = glCreateShader(type_to_create);

	//Compile shader source
	glShaderSource(shader, 1, &combined_data, &combined_size);
	glCompileShader(shader);

	//Error checking
	GLint result = GL_FALSE;
	int log_length;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
	std::vector<char> frag_shader_error(log_length > 1 ? log_length : 1);
	glGetShaderInfoLog(shader, log_length, nullptr, &frag_shader_error[0]);
	if (log_length > 0)
	{
		Logger::logf("[ERROR] File '%s':\n\n%s\n", path.c_str(), &frag_shader_error[0]);
		//Clean up
		dynamic_free(shader_data);
		dynamic_free(header_data);
		dynamic_free(combined_data);
		return false;
	}

	//Attach to program
	glAttachShader(program.handle, shader);

	//Clean up
	dynamic_free(shader_data);
	dynamic_free(header_data);
	dynamic_free(combined_data);

	return true;
}

void Renderer::issue_draw_call(int draw_count)
{
	glDrawArrays(GL_TRIANGLES, 0, draw_count);
}

void Renderer::flip_buffers()
{
#ifndef _DEBUG
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	clear_framebuffer();

	//Draw framebuffer to window
	glViewport(0, 0, render_ctx.resolution.x, render_ctx.resolution.y);
	glUseProgram(fb_data.fb_shader.handle);
	glBindVertexArray(fb_data.fb_quad.vao);
	glBindTextureUnit(0, fb_data.fb_col.handle);
	glBindTextureUnit(1, 0);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glEnable(GL_CULL_FACE);
	glfwSwapBuffers(render_ctx.window);
#endif
}

void Renderer::bind_constant_buffer(int slot, const ConstantBufferGPU& constant_buffer_gpu)
{
	glBindBufferBase(GL_UNIFORM_BUFFER, slot, constant_buffer_gpu.handle);
}

static void debug_callback_func(
	GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const GLvoid* userParam)
{
	// Skip some less useful info
	if (id == 131218)	// http://stackoverflow.com/questions/12004396/opengl-debug-context-performance-warning
		return;

	(void)(length);
	(void)(userParam);
	std::string source_string;
	std::string type_string;
	std::string severity_string;

	// The AMD variant of this extension provides a less detailed classification of the error,
	// which is why some arguments might be "Unknown".
	switch (source) {
	case GL_DEBUG_SOURCE_API: {
		source_string = "API";
		break;
	}
	case GL_DEBUG_SOURCE_APPLICATION: {
		source_string = "Application";
		break;
	}
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: {
		source_string = "Window System";
		break;
	}
	case GL_DEBUG_SOURCE_SHADER_COMPILER: {
		source_string = "Shader Compiler";
		break;
	}
	case GL_DEBUG_SOURCE_THIRD_PARTY: {
		source_string = "Third Party";
		break;
	}
	case GL_DEBUG_SOURCE_OTHER: {
		source_string = "Other";
		break;
	}
	default: {
		source_string = "Unknown";
		break;
	}
	}

	switch (type) {
	case GL_DEBUG_TYPE_ERROR: {
		type_string = "Error";
		break;
	}
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: {
		type_string = "Deprecated Behavior";
		break;
	}
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: {
		type_string = "Undefined Behavior";
		break;
	}
	case GL_DEBUG_TYPE_PORTABILITY_ARB: {
		type_string = "Portability";
		break;
	}
	case GL_DEBUG_TYPE_PERFORMANCE: {
		type_string = "Performance";
		break;
	}
	case GL_DEBUG_TYPE_OTHER: {
		type_string = "Other";
		break;
	}
	default: {
		type_string = "Unknown";
		break;
	}
	}

	switch (severity) {
	case GL_DEBUG_SEVERITY_HIGH: {
		severity_string = "High";
		break;
	}
	case GL_DEBUG_SEVERITY_MEDIUM: {
		severity_string = "Medium";
		break;
	}
	case GL_DEBUG_SEVERITY_LOW: {
		severity_string = "Low";
		break;
	}
	default: {
		severity_string = "Unknown";
		return;
	}
	}

	Logger::logf("[ERROR]\
GL Debug Callback:\n\
source:		%i:%s \n\
type:		%i:%s \n\
id:			%i \n\
severity:	%i:%s \n\
message:	%s\n", 
		source,	source_string.c_str(), 
		type, type_string.c_str(), 
		id, 
		severity, severity_string.c_str(), 
		message);
}