

#include <chrono>
#include <fstream>
#include <map>

#include "common_defines.h"
#include "editor_layer.h"
#include "input.h"
#include "logger.h"
#include "renderer.h"
#include "resource_manager.h"
#include "entt/entt.hpp"

struct TextureResource;

void update_camera(entt::registry& entity_registry, Renderer renderer, Input input, float move_speed, float delta_time, float mouse_sensitivity)
{
	auto view = entity_registry.view<TransformComponent, CameraComponent>();
	for (auto entity : view)
	{
		TransformComponent& transform = entity_registry.get<TransformComponent>(entity);
			
		if (Input::keys_held[GLFW_KEY_D]) { transform.add_position(move_speed * delta_time * +transform.get_right_vector()); }
		if (Input::keys_held[GLFW_KEY_A]) { transform.add_position(move_speed * delta_time * -transform.get_right_vector()); }
		if (Input::keys_held[GLFW_KEY_W]) { transform.add_position(move_speed * delta_time * +transform.get_forward_vector()); }
		if (Input::keys_held[GLFW_KEY_S]) { transform.add_position(move_speed * delta_time * -transform.get_forward_vector()); }
		if (Input::keys_held[GLFW_KEY_SPACE]) { transform.add_position(move_speed * delta_time * +glm::vec3{ 0, 1, 0 }); }
		if (Input::keys_held[GLFW_KEY_LEFT_SHIFT]) { transform.add_position(move_speed * delta_time * -glm::vec3{ 0, 1, 0 }); }


		//TODO: make this platform independent
		if (input.mouse_held[GLFW_MOUSE_BUTTON_RIGHT])
		{
			glfwSetInputMode(static_cast<GLFWwindow*>(renderer.get_window()), GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
			transform.add_rotation(glm::radians(
				glm::vec3 {
					0,
					-mouse_sensitivity * input.mouse_position_relative.x,
					0,
				}));
			transform.add_rotation(glm::angleAxis(glm::radians( - mouse_sensitivity * input.mouse_position_relative.y), glm::vec3{1.0f, 0.0f, 0.0f}), false);
		}
		else if (input.mouse_released[GLFW_MOUSE_BUTTON_RIGHT])
		{
			glfwSetInputMode(static_cast<GLFWwindow*>(renderer.get_window()), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			int x, y;
			glfwGetWindowSize(static_cast<GLFWwindow*>(renderer.get_window()), &x, &y);
			glfwSetCursorPos(static_cast<GLFWwindow*>(renderer.get_window()), x / 2, y / 2);
		}
			
		CameraComponent& camera = entity_registry.get<CameraComponent>(entity);
		renderer.update_camera_view(transform.get_view_matrix());
		renderer.update_camera_proj(camera.get_proj_matrix());
	}
}

static float calculate_delta_time()
{
	static std::chrono::time_point<std::chrono::steady_clock> start;
	static std::chrono::time_point<std::chrono::steady_clock> end;

	end = std::chrono::steady_clock::now();
	const std::chrono::duration<float> delta = end - start;
	start = std::chrono::steady_clock::now();
	return delta.count();
}

int main()
{
	//Init systems
	entt::registry entity_registry;
	ResourceManager resource_manager;
	Renderer renderer(&resource_manager);
	renderer.init();
	Input input(&renderer);
	EditorLayer editor_layer;
	editor_layer.init(&renderer, &resource_manager);

	//Create camera
	auto camera = entity_registry.create();
	entity_registry.emplace<TransformComponent>(camera);
	entity_registry.emplace<CameraComponent>(camera);
	CameraComponent& camera_component = entity_registry.get<CameraComponent>(camera);
	camera_component.fov = glm::radians(70.0f);
	camera_component.aspect_ratio = 16.0f/9.0f;
	camera_component.near_plane = 0.001f;
	camera_component.far_plane = 1000.0f;

	//Load cubemap
	std::vector<ResourceHandle> cubemap_handles;
	cubemap_handles.push_back(resource_manager.load_resource_from_disk<TextureResource>("Assets/Textures/KelpDome/kelp_-x.png"));
	cubemap_handles.push_back(resource_manager.load_resource_from_disk<TextureResource>("Assets/Textures/KelpDome/kelp_+x.png"));
	cubemap_handles.push_back(resource_manager.load_resource_from_disk<TextureResource>("Assets/Textures/KelpDome/kelp_-y.png"));
	cubemap_handles.push_back(resource_manager.load_resource_from_disk<TextureResource>("Assets/Textures/KelpDome/kelp_+y.png"));
	cubemap_handles.push_back(resource_manager.load_resource_from_disk<TextureResource>("Assets/Textures/KelpDome/kelp_-z.png"));
	cubemap_handles.push_back(resource_manager.load_resource_from_disk<TextureResource>("Assets/Textures/KelpDome/kelp_+z.png"));
	renderer.curr_cubemap = renderer.upload_cubemap_to_gpu(cubemap_handles, true);

	//Load model for testing purposes
	ResourceHandle handle_goomboss = resource_manager.load_resource_from_disk<ModelResource>("Assets/Models/kelp.gltf");
	ModelGPU model_gpu_goomboss = renderer.upload_mesh_to_gpu(handle_goomboss);

	float move_speed = 2.0f;
	float mouse_sensitivity = 0.3f;
	printf("\n");
	while (renderer.is_running())
	{
		//Calculate deltatime
		float delta_time = calculate_delta_time();
		if (delta_time > 0.1f)
			delta_time = 0.1f;

		renderer.begin_frame();
		input.update(static_cast<GLFWwindow*>(renderer.get_window()));

		//Update camera position
		update_camera(entity_registry, renderer, input, move_speed, delta_time, mouse_sensitivity);

		//temporary
		renderer.draw_model(handle_goomboss, glm::mat4(1.0f));
		//renderer.draw_text(std::string("frametime: ").append(std::to_string(delta_time)), { 16, 16 });
		renderer.end_frame();
		editor_layer.update();
		resource_manager.tick(delta_time);
	}

	return 0;
}
