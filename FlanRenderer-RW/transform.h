#pragma once
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

struct TransformComponent
{
public:
	TransformComponent() = default;

	glm::vec3 get_position() const { return position; }
	glm::vec3 get_euler_angles() const { return glm::eulerAngles(rotation); }
	glm::quat get_rotation() const { return rotation; }
	glm::vec3 get_scale() const { return scale; }
	glm::mat4 get_view_matrix();
	
	void set_position(glm::vec3 new_position);
	void add_position(glm::vec3 new_position);

	void set_rotation(glm::vec3 new_euler_angles);
	void set_rotation(glm::quat new_quaternion);

	void add_rotation(glm::vec3 new_euler_angles);
	void add_rotation(const glm::quat new_quaternion, bool world_space = true);

	void set_scale(glm::vec3 new_scale);
	void set_scale(float new_scalar);

	glm::vec3 get_right_vector() const;
	glm::vec3 get_up_vector() const;
	glm::vec3 get_forward_vector() const;

private:
	glm::vec3 position {0, 0, 0};
	glm::quat rotation {1, 0, 0, 0};
	glm::vec3 scale {1, 1, 1};
};

struct CameraComponent
{
	float fov;
	float aspect_ratio;
	float near_plane;
	float far_plane;
	glm::mat4 get_proj_matrix() { return glm::perspective(fov, aspect_ratio, near_plane, far_plane); }
};