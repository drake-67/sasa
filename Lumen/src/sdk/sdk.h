#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <memory/memory.h>

#include "offsets/offsets.h"
#include "math/math.h"

namespace rbx
{
	/* forward declarations */
	struct c_addressable;
	struct c_nameable;
	struct c_node;
	struct c_instance;
	struct c_player;
	struct c_model_instance;
	struct c_humanoid;
	struct c_humanoid_root_part;
	struct c_part;
	struct c_primitive;
	struct c_datamodel;
	struct c_workspace;
	struct c_visualengine;
	struct c_camera;

	/* SDK classes */
	struct c_addressable
	{
		std::uint64_t address = 0;

		c_addressable() = default;
		c_addressable(std::uint64_t address) : address(address) {}
	};

	struct c_nameable : public c_addressable
	{
		using c_addressable::c_addressable;

		std::string get_name() const;
		std::string get_class_name() const;
	};

	struct c_node
	{
		std::uint64_t find_first_child(std::string_view name) const;
		std::uint64_t find_first_child_by_class(std::string_view name) const;

		template <typename type>
		std::vector<type> get_children() const;

		std::vector<std::uint64_t> get_children() const;

		std::uint64_t get_parent() const;
		void set_parent(const std::uint64_t& parent);
	};

	struct c_instance : public c_nameable, public c_node
	{
		using c_nameable::c_nameable;

		c_primitive get_primitive() const;
	};

	struct c_player final : public c_instance
	{
		using c_instance::c_instance;

		c_model_instance get_model_instance() const;
		std::uint64_t get_team() const;
		std::string get_display_name() const;
	};

	struct c_model_instance final : public c_addressable, public c_node
	{
		using c_addressable::c_addressable;
	};

	struct c_humanoid final : public c_nameable
	{
		using c_nameable::c_nameable;

		float get_health() const;
		float get_max_health() const;

		float get_jump_power() const;
		void set_jump_power(const float& jump) const;

		float get_walk_speed() const;
		void set_walk_speed(const float& speed) const;

		float get_hip_height() const;
		void set_hip_height(const float& height) const;

		std::uint8_t get_rig_type() const;
		std::uint16_t get_state() const;
	};

	struct c_humanoid_root_part final : public c_nameable
	{
		using c_nameable::c_nameable;

		// TODO: add getters and setters such as velocity
	};

	struct c_part final : public c_nameable
	{
		using c_nameable::c_nameable;

		c_primitive get_primitive() const;
	};

	struct c_primitive final : public c_addressable
	{
		using c_addressable::c_addressable;

		math::vector3 get_position() const;
		void set_position(const math::vector3& position) const;

		math::matrix3 get_rotation() const;
		void set_rotation(const math::matrix3& rotation) const;

		math::vector3 get_size() const;
		void set_size(const math::vector3& size) const;

		math::cframe get_cframe() const;
	};

	struct c_datamodel final : public c_instance
	{
		using c_instance::c_instance;

		static std::unique_ptr<c_datamodel> get()
		{
			auto fake_datamodel{ memory->read<std::uint64_t>(memory->m_base_address + Offsets::FakeDataModel::Pointer) };
			auto real_datamodel{ memory->read<std::uint64_t>(fake_datamodel + Offsets::FakeDataModel::RealDataModel) };
			return std::make_unique<c_datamodel>(real_datamodel);
		}

		c_workspace get_workspace() const;

		std::uint64_t get_game_id() const;
		std::uint64_t get_place_id() const;
		std::uint64_t get_creator_id() const;

		std::string get_server_ip() const;
	};

	struct c_workspace final : public c_instance
	{
		using c_instance::c_instance;
	};

	struct c_visualengine final : public c_addressable
	{
		using c_addressable::c_addressable;

		static std::unique_ptr<c_visualengine> get()
		{
			auto visualengine{ memory->read<std::uint64_t>(memory->m_base_address + Offsets::VisualEngine::Pointer) };
			return std::make_unique<c_visualengine>(visualengine);
		}

		math::vector2 get_dimensions() const;
		math::matrix4 get_viewmatrix() const;

		/* this function signature is "goyslop" */
		bool world_to_screen(
			const math::matrix4& view,
			const math::vector2& dims,
			const math::vector3& world,
			math::vector2& out
		) const;
	};

	struct c_camera final : public c_addressable
	{
		using c_addressable::c_addressable;

		math::matrix3 get_rotation() const;
		math::vector3 get_position() const;
		void set_rotation(const math::matrix3& value) const;
		float get_field_of_view() const;
	};
}

template <typename type>
std::vector<type> rbx::c_node::get_children() const
{
	auto* self{ static_cast<const rbx::c_instance*>(this) };

	static thread_local std::vector<type> container{};
	container.clear();

	auto start{ memory->read<std::uint64_t>(self->address + Offsets::Instance::ChildrenStart) };
	auto end{ memory->read<std::uint64_t>(start + Offsets::Instance::ChildrenEnd) };

	for (
		auto instance{ memory->read<std::uint64_t>(start) };
		instance != end;
		instance += sizeof(std::shared_ptr<void*>)
		)
	{
		container.emplace_back(memory->read<std::uint64_t>(instance));
	}

	return container;
}
