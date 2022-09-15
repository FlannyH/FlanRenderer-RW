#pragma once
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GL/gl3w.h>

#include "logger.h"
#include "renderer.h"
#include "resource_manager.h"

class EditorLayer
{
public:
	float width_memory_debugger = 320;
	float width_resource_debugger = 320;
	void init(Renderer* _renderer, ResourceManager* _resource_manager)
	{
		//Init ImGui
		renderer = _renderer;
		resource_manager = _resource_manager;
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)renderer->get_window(), true);
		ImGui_ImplOpenGL3_Init();
		ImGui::StyleColorsDark();
	}

	std::string visualize_byte_size(intptr_t size)
	{
		std::string return_value;
		if (size < 1024)
		{
			return_value = (std::to_string(size) + " B");
		}
		else if (size < 1048576)
		{
			return_value = (std::to_string(size / 1024) + " KB");
		}
		else
		{
			return_value = (std::to_string(size / 1048576) + " MB");
		}
		return return_value;
	}

	void update()
	{
		//Init new frame
		glm::ivec2 window_size;
		glfwGetWindowSize((GLFWwindow*)renderer->get_window(), &window_size.x, &window_size.y);
		ImGui_ImplGlfw_NewFrame();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui::NewFrame();

		//Setup flags
		ImGuiWindowClass window_class;
		window_class.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoDecoration;
		ImGui::SetNextWindowClass(&window_class);

		//Render memory debugger
		ImGui::Begin("Memory debugger");
		{
			//Setup
			ImGui::SetWindowPos({ 0, 0 });
			ImVec2 viewport_size;
			viewport_size.x = width_memory_debugger;
			viewport_size.y = window_size.y;
			ImGui::SetWindowSize(viewport_size);

			//Statistics
			ImGui::BeginGroup();
			std::vector<MemoryChunk> memory_chunks = ResourceManager::get_allocator_instance()->get_memory_chunk_list();
			int total_size = 0;
			int chunk_count = 0;
			for (auto& chunk : memory_chunks)
			{
				total_size += chunk.size;
				chunk_count++;
			}
			ImGui::Text("Total used: %s\tAllocated chunks: %i", visualize_byte_size(total_size).c_str(), chunk_count);
			ImGui::EndGroup();

			//Actual chunks
			ImGui::BeginGroup();
			ImGui::BeginChild("Memory Chunks");
			for (auto& chunk : memory_chunks)
			{
				if (chunk.is_free)
					continue;
				ImGui::BeginChild("Chunk");
				ImGui::Text(chunk.name.c_str());
				ImGui::SameLine();
				ImGui::Separator();
				ImGui::Text("0x%p\t size: %s", chunk.pointer, visualize_byte_size((intptr_t)chunk.size).c_str());
				ImGui::EndChild();
			}
			ImGui::EndChild();
			ImGui::EndGroup();
		} ImGui::End();

		//Render viewport
		ImGui::Begin("Scene View");
		{
			//Setup
			ImGui::SetWindowPos({ width_memory_debugger, 0 });
			ImVec2 viewport_size;
			viewport_size.x = window_size.x - width_memory_debugger - width_resource_debugger;
			viewport_size.y = viewport_size.x * (9.0f / 16.0f);
			ImGui::SetWindowSize(viewport_size);
			ImGui::BeginChild("Viewport");

			//Show image
			ImGui::Image((ImTextureID)renderer->get_framebuffer_texture().handle, ImGui::GetWindowSize(), ImVec2(0, 1), ImVec2(1, 0));
			ImGui::EndChild();
		} ImGui::End();

		//Render console log
		ImGui::Begin("Console Log");
		{
			ImGui::SetWindowPos({ width_memory_debugger, (window_size.x - width_memory_debugger - width_resource_debugger) * (9.0f / 16.0f) });
			ImVec2 viewport_size;
			viewport_size.x = window_size.x - width_memory_debugger - width_resource_debugger;
			viewport_size.y = window_size.y - (window_size.x - width_memory_debugger - width_resource_debugger) * (9.0f / 16.0f);
			ImGui::SetWindowSize(viewport_size);
			ImGui::BeginChild("Console Log");
			for (auto& [colour, text] : Logger::messages)
			{
				ImGui::TextColored({
					colour.r / 255.0f,
					colour.g / 255.0f,
					colour.b / 255.0f,
					colour.a / 255.0f, },
					text.c_str());
				ImGui::Separator();
			}
			ImGui::SetScrollHereY(1.0f);
			ImGui::EndChild();
		} ImGui::End();

		//Show loaded resources
		/*
		ImGui::Begin("Show loaded resources");
		{
			//Setup
			ImGui::SetWindowPos({ window_size.x - width_resource_debugger, 0 });
			ImVec2 viewport_size;
			viewport_size.x = width_resource_debugger;
			viewport_size.y = window_size.y;
			ImGui::SetWindowSize(viewport_size);

			//Statistics
			ImGui::BeginGroup();
			std::vector<ResourceDebug> loaded_resources = resource_manager->debug_loaded_resources();
			ImGui::Text("Resources loaded: %i", loaded_resources.size());
			ImGui::EndGroup();

			//Actual chunks
			ImGui::BeginGroup();
			ImGui::BeginChild("Loaded resources");
			for (auto& resource : loaded_resources)
			{
				ImGui::BeginChild("Resource");
				ImGui::Text("Type: %s", resource.type.c_str());
				ImGui::SameLine();
				ImGui::Separator();
				ImGui::Text("Name: %s", resource.name.c_str());
				ImGui::EndChild();
			}
			ImGui::EndChild();
			ImGui::EndGroup();
		} ImGui::End();
		*/

		//Show debug menu

		ImGui::Begin("Show loaded resources");
		{
			//Setup
			ImGui::SetWindowPos({ window_size.x - width_resource_debugger, 0 });
			ImVec2 viewport_size;
			viewport_size.x = width_resource_debugger;
			viewport_size.y = window_size.y;
			ImGui::SetWindowSize(viewport_size);

			//Actual chunks
			ImGui::BeginGroup();
			ImGui::BeginChild("Roughness Power slider");
			ImGui::SliderFloat("Roughness Power", &renderer->rgh_pow, 0.0f, 3.0f);
			ImGui::Checkbox("Flip normal green channel", &renderer->flip_normal_y);
			ImGui::EndChild();
			ImGui::EndGroup();
		} ImGui::End();

		//Render final frame
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers((GLFWwindow*)renderer->get_window());
	}
	Renderer* renderer;
	ResourceManager* resource_manager;
};

