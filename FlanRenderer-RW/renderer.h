#pragma once

#include <unordered_map>

#include "renderer_structs.h"
#include "resources.h"
#include "resource_handler_structs.h"
#include "transform.h"
#include "glfw/glfw3.h"

class Camera;
class ResourceManager;

class Renderer
{
public:
	Renderer(ResourceManager* resource_manager_);
	void init();
	void begin_frame();
	void flip_buffers();
	void bind_constant_buffer(int camera_data, const ConstantBufferGPU& constant_buffer_gpu);
	void end_frame();
	void draw_model(ResourceHandle model_handle, glm::mat4 model_matrix);
	void draw_text(const std::string& text, glm::vec2 pos_pixels/*, AnchorPoint anchor*/); //TODO: anchorpoint
	void issue_draw_call(int draw_count);
	TextureGPU upload_texture_to_gpu(ResourceHandle texture_handle, bool is_srgb = true, bool unload_resource_afterwards = false);
	TextureGPU upload_cubemap_to_gpu(std::vector<ResourceHandle> texture_handle, bool unload_resource_afterwards = false);
	TextureGPU upload_font_to_gpu(ResourceHandle font_texture_handle);
	ModelGPU upload_mesh_to_gpu(ResourceHandle model_handle, bool unload_resources = true);
	void set_resolution(glm::ivec2 resolution);
	void toggle_fullscreen();
	ShaderGPU load_shader(std::string path);
	bool is_running();
	void update_camera_view(glm::mat4 view_matrix);
	void update_camera_proj(glm::mat4 proj_matrix);
	void* get_window();
	TextureGPU get_framebuffer_texture();
	TextureGPU curr_cubemap;
	TextureGPU curr_font;

	bool flip_normal_y = true;
	float rgh_pow = 0.55f;
	float rgh_mul = 1.0f;
private:
	void bind_texture(int slot, TextureGPU texture);
	void bind_mesh(MeshGPU mesh);
	void bind_shader(ShaderGPU shader);
	void bind_material_pbr(MaterialGPU material);
	void create_render_context();
	void init_framebuffer();
	void clear_framebuffer();
	void platform_specific_begin_frame();
	void platform_specific_end_frame();
	void platform_specific_init();
	bool load_shader_part(const std::string& path, ShaderType type, const ShaderGPU& program);
	void* allocate_temporary(uint32_t size, uint32_t align = 16);
	MeshGPU init_vertex_buffer(Vertex* vertices, int n_vertices);

	template<typename T>
	void init_or_update_constant_buffer(int slot, ConstantBufferGPU& const_buffer, T*& buffer_data);

	RenderContextData render_ctx{};
	FrameBufferData fb_data{};
	CameraDataConstantBuffer* camera_data{};
	ConstantBufferGPU camera_cb_gpu{};
	std::unordered_map<uint32_t, TextureGPU>	loaded_textures;
	std::unordered_map<uint32_t, MeshGPU>		loaded_meshes;
	std::unordered_map<uint32_t, ModelGPU>		loaded_models;
	std::unordered_map<uint32_t, ShaderGPU>		loaded_shaders;
	std::vector<void*> temporary_memory_allocations;
	std::vector<ConstantBufferGPU> temporary_const_buffers;
	std::vector<MeshRenderData> mesh_queue;

	ResourceHandle debug_quad_handle;
	MeshGPU debug_quad_gpu;
	ShaderGPU pbr_shader;
	MaterialGPU pbr_material;
	TextureGPU tex_default_col;
	TextureGPU tex_default_nrm;
	TextureGPU tex_default_mtl;
	TextureGPU tex_default_rgh;
	TextureGPU ibl_brdf_lut;

	ResourceManager* resource_manager;
};