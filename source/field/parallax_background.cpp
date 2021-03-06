#include "./parallax_background.hpp"

#include "../utility/tmx_convert.hpp"
#include "../system/renderer.hpp"

static const byte_t kBoundsXProp[] = "rect.x";
static const byte_t kBoundsYProp[] = "rect.y";
static const byte_t kBoundsWProp[] = "rect.w";
static const byte_t kBoundsHProp[] = "rect.h";
static const byte_t kScrollXProp[] = "scroll.x";
static const byte_t kScrollYProp[] = "scroll.y";

parallax_background_t::parallax_background_t() :
	indices(0),
	position(0.0f),
	scrolling(0.0f),
	dimensions(1.0f),
	bounding()
{

}

parallax_background_t::parallax_background_t(parallax_background_t&& that) noexcept : parallax_background_t() {
	if (this != &that) {
		std::swap(indices, that.indices);
		std::swap(position, that.position);
		std::swap(scrolling, that.scrolling);
		std::swap(dimensions, that.dimensions);
		std::swap(bounding, that.bounding);
	}
}

parallax_background_t& parallax_background_t::operator=(parallax_background_t&& that) noexcept {
	if (this != &that) {
		std::swap(indices, that.indices);
		std::swap(position, that.position);
		std::swap(scrolling, that.scrolling);
		std::swap(dimensions, that.dimensions);
		std::swap(bounding, that.bounding);
	}
	return *this;
}

void parallax_background_t::init(const std::unique_ptr<tmx::Layer>& layer, glm::vec2 dimensions) {
	if (dimensions.x == 0.0f or dimensions.y == 0.0f) {
		dimensions = glm::one<glm::vec2>();
	}
	this->dimensions = dimensions;
	this->bounding = rect_t(0.0f, 0.0f, 1.0f, 1.0f);
	glm::vec2 inv = 1.0f / this->dimensions;
	for (auto&& property : layer->getProperties()) {
		auto& name = property.getName();
		if (name == kBoundsXProp) {
			this->bounding.x = tmx_convert::prop_to_real(property) * inv.x;
		} else if (name == kBoundsYProp) {
			this->bounding.y = tmx_convert::prop_to_real(property) * inv.y;
		} else if (name == kBoundsWProp) {
			this->dimensions.x = tmx_convert::prop_to_real(property);
			this->bounding.w = this->dimensions.x * inv.x;
		} else if (name == kBoundsHProp) {
			this->dimensions.y = tmx_convert::prop_to_real(property);
			this->bounding.h = this->dimensions.y * inv.y;
		} else if (name == kScrollXProp) {
			this->scrolling.x = tmx_convert::prop_to_real(property);
		} else if (name == kScrollYProp) {
			this->scrolling.y = tmx_convert::prop_to_real(property);
		}
	}
}

void parallax_background_t::handle(rect_t viewport) {
	glm::vec2 next = glm::mod(
		viewport.left_top() * -scrolling,
		dimensions
	);
	if (position != next) {
		indices = 0;
		position = next;
	}
}

void parallax_background_t::render(renderer_t& renderer, rect_t viewport, const texture_t* texture) const {
	auto& list = renderer.get_normal_quads(
		layer_value::Parallax,
		blend_mode_t::Alpha,
		buffer_usage_t::Dynamic,
		pipeline_t::VtxMajorSprites,
		texture,
		nullptr
	);
	if (indices == 0) {
		for (real_t y = viewport.y + position.y - dimensions.y; y < viewport.bottom(); y += dimensions.y) {
			for (real_t x = viewport.x + position.x - dimensions.x; x < viewport.right(); x += dimensions.x) {
				list.begin(display_list_t::SingleQuad)
					.vtx_major_write(bounding, dimensions, 0.0f, 1.0f, mirroring_t::None)
					.vtx_transform_write(x, y)
				.end();
				indices += display_list_t::SingleQuad;
			}
		}
	} else {
		list.skip(indices);
	}
}
