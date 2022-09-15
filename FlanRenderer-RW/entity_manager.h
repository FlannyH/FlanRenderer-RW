#pragma once
#include "entt/entt.hpp"

class Entity
{
public:
	Entity();
	template <typename T>
	T& add_component() {}
private:
	entt::entity* entity;
};

