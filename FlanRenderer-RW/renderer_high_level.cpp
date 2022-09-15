#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GL/gl3w.h>

#include "common_defines.h"
#include "renderer.h"
#include "resource_manager.h"

Renderer::Renderer(ResourceManager* resource_manager_) : resource_manager(resource_manager_)
{
}

void Renderer::init()
{
	//General init
	platform_specific_init();
	create_render_context();
	init_framebuffer();
	init_or_update_constant_buffer<CameraDataConstantBuffer>((int)ConstantBufferType::camera_data, camera_cb_gpu, camera_data);
	camera_data->proj_matrix = glm::mat4(1.0f);
	camera_data->view_matrix = glm::mat4(1.0f);

	//Create temporary quad
	//debug_quad_handle = resource_manager->load_resource<MeshResource>("Assets/Models/monkey.glb");
	//debug_quad_gpu = upload_mesh_to_gpu(debug_quad_handle);

	//Load shader
	pbr_shader = load_shader("Assets/Shaders/lit");

	//TODO: make this more elegant?
	//Create PBR material
	const ResourceHandle test_texture_handle = resource_manager->load_resource_from_disk<TextureResource>("Assets/Textures/test.png");
	pbr_material.tex_col = upload_texture_to_gpu(test_texture_handle);

	//Create default textures in code
	TextureResource tex_col	(1, 1, (Pixel32*)dynamic_allocate(sizeof(Pixel32)), (char*)"internal/default colour texture" );
	TextureResource tex_nrm	(1, 1, (Pixel32*)dynamic_allocate(sizeof(Pixel32)), (char*)"internal/default normal texture" );
	TextureResource tex_mtl	(1, 1, (Pixel32*)dynamic_allocate(sizeof(Pixel32)), (char*)"internal/default metallic texture" );
	TextureResource tex_rgh	(1, 1, (Pixel32*)dynamic_allocate(sizeof(Pixel32)), (char*)"internal/default roughness texture" );
	tex_col.data[0] = { 255, 255, 255, 255 };
	tex_nrm.data[0] = { 128, 128, 255, 255 };
	tex_mtl.data[0] = {   0,   0,   0, 255 };
	tex_rgh.data[0] = { 255, 255, 255, 255 };
	auto resource_col = resource_manager->load_resource_from_buffer<TextureResource>(std::string(tex_col.name), &tex_col);
	auto resource_nrm = resource_manager->load_resource_from_buffer<TextureResource>(std::string(tex_nrm.name), &tex_nrm);
	auto resource_mtl = resource_manager->load_resource_from_buffer<TextureResource>(std::string(tex_mtl.name), &tex_mtl);
	auto resource_rgh = resource_manager->load_resource_from_buffer<TextureResource>(std::string(tex_rgh.name), &tex_rgh);
	tex_default_col = upload_texture_to_gpu(resource_col);
	tex_default_nrm = upload_texture_to_gpu(resource_nrm);
	tex_default_mtl = upload_texture_to_gpu(resource_mtl);
	tex_default_rgh = upload_texture_to_gpu(resource_rgh);

	//Load IBL BRDF LUT
	ibl_brdf_lut = upload_texture_to_gpu(resource_manager->load_resource_from_disk<TextureResource>("Assets/Textures/ibl_brdf_lut.png"));
}

void Renderer::begin_frame()
{
	platform_specific_begin_frame();
	clear_framebuffer();
	//mesh_render_queue.clear();
}

void Renderer::end_frame()
{
	platform_specific_end_frame();

	//Draw mesh queue
	{
		for (auto& mesh : mesh_queue)
		{
			camera_data->model_matrix = mesh.model_matrix;
			init_or_update_constant_buffer((int)ConstantBufferType::camera_data, camera_cb_gpu, camera_data);
			bind_constant_buffer((int)ConstantBufferType::camera_data, camera_cb_gpu);
			bind_material_pbr(mesh.material);
			
			bind_mesh(mesh.mesh);

			issue_draw_call(mesh.mesh.vert_count);
		}
		mesh_queue.clear();
	}

	flip_buffers();

	//Free temporary memory allocations
	for (void* pointers : temporary_memory_allocations)
	{
		dynamic_free(pointers);
	}
	temporary_memory_allocations.clear();
}

void Renderer::draw_model(ResourceHandle model_handle, glm::mat4 model_matrix)
{
	const ModelGPU model_gpu = loaded_models[model_handle.hash];
	for (int i = 0; i < model_gpu.n_meshes; i++)
	{
		MeshRenderData render_data
		{
			model_gpu.meshes[i],
			model_gpu.materials[i],
			model_matrix
		};
		mesh_queue.push_back(render_data);
	}
}

void Renderer::draw_text(const std::string& text, glm::vec2 pos_pixels)
{
	const glm::vec2 correction_factor
	{
		1.0f/static_cast<float>(render_ctx.resolution.x),
		1.0f/static_cast<float>(render_ctx.resolution.y)
	};

	glm::vec2 curr_pos_pixels = pos_pixels;

	//Loop over each character in text
	for (const char character : text)
	{
		//Calculate normalized device coordinates
		glm::vec2 pos_ndc = curr_pos_pixels * correction_factor * glm::vec2(2.0f) - glm::vec2(1.0f);

		//Calculate letter UV coordinates
		glm::vec2 char_coords_base =
		{
			static_cast<float>((character - 0x20) % 8) / 8.0f,
			static_cast<float>((character - 0x20) / 8) / 8.0f,
		};

		//Generate quad
		glm::vec4 quad_verts[4] //xy = pos, zw = uv
		{
			{curr_pos_pixels.x							, curr_pos_pixels.y,							char_coords_base.x			, char_coords_base.y        }, //top left
			{curr_pos_pixels.x + correction_factor.x * 8	, curr_pos_pixels.y,							char_coords_base.x + 0.125f	, char_coords_base.y        }, //top right
			{curr_pos_pixels.x							, curr_pos_pixels.y + correction_factor.y * 8,	char_coords_base.x			, char_coords_base.y + 0.125}, //bottom left
			{curr_pos_pixels.x + correction_factor.x * 8	, curr_pos_pixels.y + correction_factor.y * 8,	char_coords_base.x + 0.125f	, char_coords_base.y + 0.125}, //bottom right
		};

		//Generate indices
		uint16_t indices[6]
		{
			0, 2, 1,
			3, 1, 2,
		};

		//TODO: find an elegant solution for this
		GLuint vbo, vao, ebo;
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);
		glGenVertexArrays(1, &vao);



		curr_pos_pixels.x += 8; //TODO: make this variable width font
	}
}

void* Renderer::allocate_temporary(const uint32_t size, const uint32_t align)
{
	ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "temporary allocation";
	void* memory_chunk = dynamic_allocate(size, align);
	ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "untitled";
	temporary_memory_allocations.push_back(memory_chunk);
	return memory_chunk;
}

TextureGPU Renderer::upload_font_to_gpu(const ResourceHandle font_texture_handle)
{
	curr_font = upload_texture_to_gpu(font_texture_handle);
	return curr_font;
}