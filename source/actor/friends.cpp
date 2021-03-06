#include "./friends.hpp"

#include "../component/kontext.hpp"
#include "../component/location.hpp"
#include "../component/kinematics.hpp"
#include "../component/sprite.hpp"
#include "../component/blinker.hpp"

#include "../resource/id.hpp"

LEVIATHAN_CTOR_TABLE_CREATE(routine_generator_t) {
	LEVIATHAN_CTOR_TABLE_PUSH(ai::kyoko::type, 		ai::kyoko::ctor);
	LEVIATHAN_CTOR_TABLE_PUSH(ai::nauzika::type, 	ai::nauzika::ctor);
	LEVIATHAN_CTOR_TABLE_PUSH(ai::senma::type, 		ai::senma::ctor);
	LEVIATHAN_CTOR_TABLE_PUSH(ai::fox::type, 		ai::fox::ctor);
	LEVIATHAN_CTOR_TABLE_PUSH(ai::gyo::type, 		ai::gyo::ctor);
}

void ai::friends::tick(entt::entity s, routine_tuple_t& rtp) {
	auto& kinematics = rtp.ktx.get<kinematics_t>(s);
	kinematics.accel_y(0.1f, 6.0f);
}

void ai::kyoko::ctor(entt::entity s, kontext_t& ktx) {
	auto& location = ktx.get<location_t>(s);
	location.bounding = rect_t(4.0f, -2.0f, 8.0f, 18.0f);

	auto& sprite = ktx.assign_if<sprite_t>(s, res::anim::Kyoko);
	sprite.table = 0.0f;
	sprite.layer = 0.2f;
	sprite.position = location.position;

	ktx.assign_if<kinematics_t>(s);
	ktx.assign_if<blinker_t>(s, 0, 2);
	ktx.assign_if<routine_t>(s, friends::tick);
	ktx.sort<sprite_t>(sprite_t::compare);
}

void ai::nauzika::ctor(entt::entity s, kontext_t& ktx) {
	auto& location = ktx.get<location_t>(s);
	location.bounding = rect_t(5.0f, 0.0f, 8.0f, 16.0f);

	auto& sprite = ktx.assign_if<sprite_t>(s, res::anim::Nauzika);
	sprite.table = 0.0f;
	sprite.layer = 0.2f;
	sprite.position = location.position;

	ktx.assign_if<kinematics_t>(s);
	ktx.assign_if<blinker_t>(s, 0, 2);
	ktx.assign_if<routine_t>(s, friends::tick);
	ktx.sort<sprite_t>(sprite_t::compare);
}

void ai::senma::ctor(entt::entity s, kontext_t& ktx) {
	auto& location = ktx.get<location_t>(s);
	location.bounding = rect_t(4.0f, -8.0f, 8.0f, 24.0f);

	auto& sprite = ktx.assign_if<sprite_t>(s, res::anim::Senma);
	sprite.table = 0.0f;
	sprite.layer = 0.2f;
	sprite.position = location.position;

	ktx.assign_if<kinematics_t>(s);
	ktx.assign_if<blinker_t>(s, 0, 2);
	ktx.assign_if<routine_t>(s, friends::tick);
	ktx.sort<sprite_t>(sprite_t::compare);
}

void ai::fox::ctor(entt::entity s, kontext_t& ktx) {
	auto& location = ktx.get<location_t>(s);
	location.bounding = rect_t(4.0f, 0.0f, 8.0f, 16.0f);

	auto& sprite = ktx.assign_if<sprite_t>(s, res::anim::Foxie);
	sprite.table = 0.0f;
	sprite.layer = 0.2f;
	sprite.position = location.position;

	ktx.assign_if<kinematics_t>(s);
	ktx.assign_if<blinker_t>(s, 0, 2);
	ktx.assign_if<routine_t>(s, friends::tick);
	ktx.sort<sprite_t>(sprite_t::compare);
}

void ai::gyo::ctor(entt::entity s, kontext_t& ktx) {
	auto& location = ktx.get<location_t>(s);
	location.bounding = rect_t(4.0f, 0.0f, 8.0f, 16.0f);

	auto& sprite = ktx.assign_if<sprite_t>(s, res::anim::GyoShin);
	sprite.table = 0.0f;
	sprite.layer = 0.2f;
	sprite.position = location.position;

	ktx.assign_if<kinematics_t>(s);
	ktx.assign_if<blinker_t>(s, 0, 2);
	ktx.assign_if<routine_t>(s, friends::tick);
	ktx.sort<sprite_t>(sprite_t::compare);
}
