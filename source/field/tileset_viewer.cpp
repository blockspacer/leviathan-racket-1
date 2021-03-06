#include "./tileset_viewer.hpp"

#include "../video/texture.hpp"
#include "../system/input.hpp"
#include "../system/renderer.hpp"
#include "../utility/logger.hpp"
#include "../utility/vfs.hpp"

#include <fstream>

static constexpr arch_t kMaskLength = 256;

static const glm::vec2 kDefaultPosition = glm::vec2(120.0f, 16.0f);

tileset_viewer_t::tileset_viewer_t() :
	amend(false),
	select(false),
	flash(false),
	cursor(0.0f, 0.0f, 16.0f, 16.0f),
	index(0),
	timer(0.0),
	texture(nullptr),
	bitmasks(),
	file()
{

}

void tileset_viewer_t::reset() {
	amend = true;
	select = false;
	index = 0;
	for (auto&& mask : bitmasks) {
		mask = 0;
	}
	synao_log("Cleared!\n");
}

void tileset_viewer_t::handle(const input_t& input) {
	if (!file.empty() and texture != nullptr) {
		if (select) {
			bool pressed = false;
			if (input.pressed[btn_t::Up]) {
				index = glm::max(index - 16, (arch_t)0);
				pressed = true;
			} else if (input.pressed[btn_t::Down]) {
				index = glm::min(index + 16, kMaskLength - 1);
				pressed = true;
			} else if (input.pressed[btn_t::Right]) {
				index = glm::min(index + 1, kMaskLength - 1);
				pressed = true;
			} else if (input.pressed[btn_t::Left]) {
				index = glm::max(index - 1, (arch_t)0);
				pressed = true;
			}
			if (pressed) {
				amend = true;
				index = glm::clamp(index, (arch_t)0, (arch_t)255);
				glm::vec2 position = glm::vec2(
					static_cast<real_t>(index % 16) * cursor.w,
					static_cast<real_t>(index / 16) * cursor.h
				);
				cursor.x = position.x + kDefaultPosition.x;
				cursor.y = position.y + kDefaultPosition.y;
			}
		}
		if (input.pressed[btn_t::ClickL]) {
			const rect_t bounds = rect_t(
				kDefaultPosition,
				texture->get_dimensions()
			);
			const glm::vec2 input_position = input.get_position();
			if (bounds.contains(input_position)) {
				glm::vec2 mouse_position = input_position - kDefaultPosition;
				glm::ivec2 integral_coordinates = glm::ivec2(
					static_cast<sint_t>(mouse_position.x) / 16,
					static_cast<sint_t>(mouse_position.y) / 16
				);
				glm::vec2 real_coordinates = glm::vec2(
					static_cast<real_t>(integral_coordinates.x * 16),
					static_cast<real_t>(integral_coordinates.y * 16)
				);
				synao_log(
					"(%d, %d)\n",
					integral_coordinates.x,
					integral_coordinates.y
				);
				amend = true;
				select = true;
				index = static_cast<arch_t>(integral_coordinates.x) + static_cast<arch_t>(integral_coordinates.y) * 16;
				cursor.x = real_coordinates.x + kDefaultPosition.x;
				cursor.y = real_coordinates.y + kDefaultPosition.y;
			}
		} else if (input.pressed[btn_t::ClickR]) {
			amend = true;
			select = false;
			index = 0;
		}
	} else {
		amend = true;
		select = false;
		index = 0;
	}
}

void tileset_viewer_t::update(real64_t delta) {
	if (select) {
		timer -= delta;
		if (timer <= 0.0) {
			amend = true;
			timer = 0.048;
			flash = !flash;
		}
	}
}

void tileset_viewer_t::render(renderer_t& renderer) const {
	if (amend) {
		amend = false;
		if (select and flash) {
			auto& list = renderer.get_overlay_quads(
				layer_value::HeadsUp,
				blend_mode_t::Alpha,
				buffer_usage_t::Dynamic,
				pipeline_t::VtxBlankColors
			);
			list.begin(display_list_t::SingleQuad)
				.vtx_blank_write(
					rect_t(glm::zero<glm::vec2>(), cursor.dimensions()),
					glm::one<glm::vec4>()
				)
				.vtx_transform_write(cursor.left_top())
			.end();
		}
		if (texture != nullptr) {
			auto& list = renderer.get_overlay_quads(
				layer_value::Automatic,
				blend_mode_t::Alpha,
				buffer_usage_t::Dynamic,
				pipeline_t::VtxMajorSprites,
				texture,
				nullptr
			);
			list.begin(display_list_t::SingleQuad)
				.vtx_major_write(
					rect_t(glm::zero<glm::vec2>(), glm::one<glm::vec2>()),
					texture->get_dimensions(),
					0.0f, 1.0f,
					mirroring_t::None
				)
				.vtx_transform_write(kDefaultPosition)
			.end();
		}
	} else {
		if (select and flash) {
			auto& list = renderer.get_overlay_quads(
				layer_value::HeadsUp,
				blend_mode_t::Alpha,
				buffer_usage_t::Dynamic,
				pipeline_t::VtxBlankColors
			);
			list.skip(display_list_t::SingleQuad);
		}
		if (texture != nullptr) {
			auto& list = renderer.get_overlay_quads(
				layer_value::Automatic,
				blend_mode_t::Alpha,
				buffer_usage_t::Dynamic,
				pipeline_t::VtxMajorSprites,
				texture,
				nullptr
			);
			list.skip(display_list_t::SingleQuad);
		}
	}
}

bool tileset_viewer_t::load(const std::string& path, renderer_t& renderer) {
	amend = true;
	select = false;
	if (file != path) {
		texture = vfs::texture(path);
		if (texture == nullptr) {
			return false;
		}
		const std::string tilekey_path = vfs::resource_path(vfs_resource_path_t::TileKey);
		bitmasks = vfs::sint_buffer(tilekey_path + path + ".atr");
		if (bitmasks.empty()) {
			return false;
		}
		file = path;
		renderer.clear();
		synao_log("Load successful!\n");
		return true;
	}
	return false;
}

bool tileset_viewer_t::save() {
	select = false;
	if (!file.empty()) {
		const std::string tilekey_path = vfs::resource_path(vfs_resource_path_t::TileKey);
		std::ofstream ofs(tilekey_path + file + ".atr", std::ofstream::binary);
		if (ofs.is_open()) {
			ofs.write(
				reinterpret_cast<byte_t*>(&bitmasks[0]),
				sizeof(sint_t) * kMaskLength
			);
			synao_log("Save successful!\n");
			return true;
		} else {
			synao_log("Error! Couldn't save attribute file: %s!\n", file.c_str());
		}
	} else {
		synao_log("Error! Can't save file!\n");
	}
	return false;
}

bool tileset_viewer_t::selected() const {
	return select;
}

void tileset_viewer_t::set_bitmask(sint_t mask) {
	bitmasks[index] = mask;
}

sint_t tileset_viewer_t::get_bitmask() const {
	return bitmasks[index];
}
