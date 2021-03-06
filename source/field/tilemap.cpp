#include "./tilemap.hpp"
#include "./tileflag.hpp"
#include "./camera.hpp"

#include "../system/renderer.hpp"
#include "../utility/vfs.hpp"
#include "../utility/constants.hpp"
#include "../utility/tmx_convert.hpp"

#include <tmxlite/Map.hpp>
#include <tmxlite/ImageLayer.hpp>

static constexpr sint_t kScreenWidth  = 21;
static constexpr sint_t kScreenHeight = 12;

tilemap_t::tilemap_t() :
	amend(false),
	dimensions(0),
	attributes(),
	attribute_key(),
	previous_viewport(glm::zero<glm::vec2>(), constants::NormalDimensions<real_t>()),
	tilemap_layer_texture(nullptr),
	tilemap_layer_palette(nullptr),
	parallax_texture(nullptr),
	backgrounds(),
	tilemap_layers()
{

}

void tilemap_t::reset() {
	amend = true;
	dimensions = glm::zero<glm::ivec2>();
	attributes.clear();
	previous_viewport = rect_t(
		-constants::TileDimensions<real_t>(),
		constants::NormalDimensions<real_t>()
	);
	tilemap_layer_texture = nullptr;
	tilemap_layer_palette = nullptr;
	parallax_texture = nullptr;
	backgrounds.clear();
	tilemap_layers.clear();
}

void tilemap_t::handle(const camera_t& camera) {
	rect_t viewport = camera.get_viewport();
	for (auto&& background : backgrounds) {
		background.handle(viewport);
	}
	if (!previous_viewport.cmp_round(viewport)) {
		previous_viewport = viewport;
		amend = true;
		glm::ivec2 first = glm::ivec2(
			glm::max(tilemap_t::floor(viewport.x), 0),
			glm::max(tilemap_t::floor(viewport.y), 0)
		);
		glm::ivec2 last = glm::ivec2(
			glm::min(tilemap_t::ceiling(viewport.right() + constants::TileSize<real_t>()), dimensions.x),
			glm::min(tilemap_t::ceiling(viewport.bottom() + constants::TileSize<real_t>()), dimensions.y)
		);
		arch_t range = camera.get_tile_range(first, last);
		for (auto&& tilemap_layer : tilemap_layers) {
			tilemap_layer.handle(range, first, last, dimensions);
		}
	}
}

void tilemap_t::render(renderer_t& renderer, rect_t viewport) const {
	for (auto&& background : backgrounds) {
		background.render(
			renderer,
			viewport,
			parallax_texture
		);
	}
	for (auto&& tilemap_layer : tilemap_layers) {
		tilemap_layer.render(
			renderer,
			amend,
			tilemap_layer_texture,
			tilemap_layer_palette
		);
	}
	amend = false;
}

static const byte_t kPaletteProperty[] = "indexed";

void tilemap_t::push_properties(const tmx::Map& tmxmap) {
	const tmx::FloatRect bounds = tmxmap.getBounds();
	dimensions = glm::ivec2(
		glm::max(static_cast<sint_t>(bounds.width) / constants::TileSize<sint_t>(), kScreenWidth),
		glm::max(static_cast<sint_t>(bounds.height) / constants::TileSize<sint_t>(), kScreenHeight)
	);
	attributes.resize(
		static_cast<arch_t>(dimensions.x) *
		static_cast<arch_t>(dimensions.y)
	);
	auto& tilesets = tmxmap.getTilesets();
	if (!tilesets.empty()) {
		auto& tileset = tilesets[0];
		for (auto&& property : tileset.getProperties()) {
			auto& name = property.getName();
			if (name == kPaletteProperty) {
				tilemap_layer_palette = vfs::palette(tileset.getName());
			}
		}
		const std::string& name = tmx_convert::path_to_name(tileset.getImagePath());
		tilemap_layer_texture = vfs::texture(name);
		const std::string tilekey_path = vfs::resource_path(vfs_resource_path_t::TileKey);
		attribute_key = vfs::sint_buffer(tilekey_path + name + ".atr");
	}
}

void tilemap_t::push_tile_layer(const std::unique_ptr<tmx::Layer>& layer) {
	amend = true;
	if (!attribute_key.empty()) {
		glm::vec2 inverse = tilemap_layer_texture != nullptr ?
			tilemap_layer_texture->get_inverse_dimensions() :
			glm::zero<glm::vec2>();
		auto& recent = tilemap_layers.emplace_back(dimensions);
		recent.init(
			layer,
			inverse,
			attributes,
			attribute_key
		);
	}
}

void tilemap_t::push_parallax_background(const std::unique_ptr<tmx::Layer>& layer) {
	amend = true;
	const std::string& path = dynamic_cast<tmx::ImageLayer*>(layer.get())->getImagePath();
	parallax_texture = vfs::texture(tmx_convert::path_to_name(path));
	glm::vec2 parallax_dimensions = parallax_texture != nullptr ?
		parallax_texture->get_dimensions() :
		glm::zero<glm::vec2>();
	auto& recent = backgrounds.emplace_back();
	recent.init(layer, parallax_dimensions);
}

sint_t tilemap_t::get_attribute(sint_t x, sint_t y) const {
	if (x >= 0 and y >= 0 and x < dimensions.x and y < dimensions.y) {
		return attributes[
			static_cast<arch_t>(x) +
			static_cast<arch_t>(y) *
			static_cast<arch_t>(dimensions.x)
		];
	} else if (y > (dimensions.y + 1)) {
		return tileflag_t::OutBounds;
	}
	return tileflag_t::Empty;
}

sint_t tilemap_t::get_attribute(glm::ivec2 index) const {
	return this->get_attribute(index.x, index.y);
}

sint_t tilemap_t::round(real_t value) {
	return static_cast<sint_t>(value) / constants::TileSize<sint_t>();
}

sint_t tilemap_t::ceiling(real_t value) {
	return static_cast<sint_t>(glm::ceil(value / constants::TileSize<real_t>()));
}

sint_t tilemap_t::floor(real_t value) {
	return static_cast<sint_t>(glm::floor(value / constants::TileSize<real_t>()));
}

real_t tilemap_t::extend(sint_t value) {
	return static_cast<real_t>(value * constants::TileSize<sint_t>());
}
