#include "transform.h"

glm::mat4 TransformComponent::get_view_matrix()
{
	glm::mat4 result(1.0f);
	return (glm::lookAt(
		get_position(),
		get_position() + get_forward_vector(),
		get_up_vector()));
}

void TransformComponent::set_position(const glm::vec3 new_position)
{
	position = new_position;
}

void TransformComponent::add_position(const glm::vec3 new_position)
{
	position += new_position;
}

void TransformComponent::set_rotation(const glm::vec3 new_euler_angles)
{
	rotation = glm::quat(new_euler_angles);
}

void TransformComponent::set_rotation(const glm::quat new_quaternion)
{
	rotation = new_quaternion;
}

void TransformComponent::add_rotation(const glm::vec3 new_euler_angles)
{
	rotation = glm::normalize(glm::normalize(glm::quat(new_euler_angles)) * glm::normalize(rotation));
}

void TransformComponent::add_rotation(const glm::quat new_quaternion, bool world_space)
{
	if (world_space)
		rotation = glm::normalize(glm::normalize(new_quaternion) * glm::normalize(rotation));
	else
		rotation = glm::normalize(rotation) * glm::normalize(glm::normalize(new_quaternion));
}

void TransformComponent::set_scale(const glm::vec3 new_scale)
{
	scale = new_scale;
}

void TransformComponent::set_scale(const float new_scalar)
{
	scale = glm::vec3(new_scalar);
}

glm::vec3 TransformComponent::get_right_vector() const
{
	return rotation * glm::vec3{ 1,0,0 };
}

glm::vec3 TransformComponent::get_up_vector() const
{
	return rotation * glm::vec3{ 0,1,0 };
}

glm::vec3 TransformComponent::get_forward_vector() const
{
	return rotation * glm::vec3{ 0,0,-1 };
}
