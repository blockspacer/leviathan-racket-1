#include "./animation.hpp"

#include "../utility/thread_pool.hpp"
#include "../utility/setup_file.hpp"
#include "../utility/logger.hpp"
#include "../utility/vfs.hpp"
#include "../system/renderer.hpp"

animation_t::animation_t() :
	ready(false),
	future(),
	sequences(),
	inverts(1.0f),
	texture(nullptr),
	palette(nullptr)
{

}

animation_t::animation_t(animation_t&& that) noexcept : animation_t() {
	if (this != &that) {
		std::atomic<bool> temp = ready.load();
		ready.store(that.ready.load());
		that.ready.store(temp.load());

		std::swap(future, that.future);
		std::swap(sequences, that.sequences);
		std::swap(inverts, that.inverts);
		std::swap(texture, that.texture);
		std::swap(palette, that.palette);
	}
}

animation_t& animation_t::operator=(animation_t&& that) noexcept {
	if (this != &that) {
		std::atomic<bool> temp = ready.load();
		ready.store(that.ready.load());
		that.ready.store(temp.load());

		std::swap(future, that.future);
		std::swap(sequences, that.sequences);
		std::swap(inverts, that.inverts);
		std::swap(texture, that.texture);
		std::swap(palette, that.palette);
	}
	return *this;
}

void animation_t::update(real64_t delta, bool_t& amend, arch_t state, real64_t& timer, arch_t& frame) const {
	this->assure();
	if (state < sequences.size()) {
		sequences[state].update(delta, amend, timer, frame);
	}
}

void animation_t::render(renderer_t& renderer, const rect_t& viewport, bool_t panic, bool_t& amend, arch_t state, arch_t frame, arch_t variation, mirroring_t mirroring, layer_t layer, real_t alpha, real_t index, glm::vec2 position, glm::vec2 scale, real_t angle, glm::vec2 pivot) const {
	this->assure();
	if (state < sequences.size()) {
		glm::vec2 sequsize = sequences[state].get_dimensions();
		glm::vec2 sequorig = sequences[state].get_origin(frame, variation, mirroring);
		if (viewport.overlaps(position - sequorig, sequsize * scale)) {
			rect_t seququad = sequences[state].get_quad(inverts, frame, variation);
			pipeline_t pipeline = pipeline_t::VtxMajorSprites;
			if (palette != nullptr) {
				pipeline = pipeline_t::VtxMajorIndexed;
				index = palette->convert(index);
			}
			auto& list = renderer.get_normal_quads(
				layer,
				blend_mode_t::Alpha,
				buffer_usage_t::Dynamic,
				pipeline,
				texture,
				palette
			);
			if (amend or panic) {
				amend = false;
				list.begin(display_list_t::SingleQuad)
					.vtx_major_write(seququad, sequsize, index, alpha, mirroring)
					.vtx_transform_write(position - sequorig, scale, pivot, angle)
				.end();
			} else {
				list.skip(display_list_t::SingleQuad);
			}
		}
	}
}

void animation_t::render(renderer_t& renderer, const rect_t& viewport, bool_t panic, bool_t& amend, arch_t state, arch_t frame, arch_t variation, mirroring_t mirroring, layer_t layer, real_t alpha, real_t index, glm::vec2 position, glm::vec2 scale) const {
	this->assure();
	if (state < sequences.size()) {
		glm::vec2 sequsize = sequences[state].get_dimensions();
		glm::vec2 sequorig = sequences[state].get_origin(frame, variation, mirroring);
		if (viewport.overlaps(position - sequorig, sequsize * scale)) {
			rect_t seququad = sequences[state].get_quad(inverts, frame, variation);
			pipeline_t pipeline = pipeline_t::VtxMajorSprites;
			if (palette != nullptr) {
				pipeline = pipeline_t::VtxMajorIndexed;
				index = palette->convert(index);
			}
			auto& list = renderer.get_normal_quads(
				layer,
				blend_mode_t::Alpha,
				buffer_usage_t::Dynamic,
				pipeline,
				texture,
				palette
			);
			if (amend or panic) {
				amend = false;
				list.begin(display_list_t::SingleQuad)
					.vtx_major_write(seququad, sequsize, index, alpha, mirroring)
					.vtx_transform_write(position - sequorig, scale)
				.end();
			} else {
				list.skip(display_list_t::SingleQuad);
			}
		}
	}
}

void animation_t::render(renderer_t& renderer, bool_t& amend, arch_t state, arch_t frame, arch_t variation, real_t index, glm::vec2 position) const {
	this->assure();
	if (state < sequences.size()) {
		pipeline_t pipeline = pipeline_t::VtxMajorSprites;
		if (palette != nullptr) {
			pipeline = pipeline_t::VtxMajorIndexed;
			index = palette->convert(index);
		}
		auto& list = renderer.get_overlay_quads(
			layer_value::HeadsUp,
			blend_mode_t::Alpha,
			buffer_usage_t::Dynamic,
			pipeline,
			texture,
			palette
		);
		if (amend) {
			amend = false;
			glm::vec2 sequsize = sequences[state].get_dimensions();
			glm::vec2 sequorig = sequences[state].get_origin(frame, variation, mirroring_t::None);
			rect_t seququads   = sequences[state].get_quad(inverts, frame, variation);
			list.begin(display_list_t::SingleQuad)
				.vtx_major_write(seququads, sequsize, index, 1.0f, mirroring_t::None)
				.vtx_transform_write(position - sequorig)
			.end();
		} else {
			list.skip(display_list_t::SingleQuad);
		}
	}
}

void animation_t::load(const std::string& full_path) {
	if (sequences.size() > 0) {
		synao_log("Warning! Tried to overwrite animation!\n");
		return;
	}
	setup_file_t setup;
	if (setup.load(full_path)) {
		std::vector<std::string> matfile;
		std::string palfile;
		setup.get("Main", "Material", matfile);
		setup.get("Main", "Palettes", palfile);
		setup.get("Main", "Inverter", inverts);

		if (inverts.x == 0.0f or inverts.y == 0.0f) {
			inverts = glm::one<glm::vec2>();
		} else {
			inverts = 1.0f / inverts;
		}

		if (!matfile.empty()) {
			texture = vfs::texture(matfile);
		}
		if (!palfile.empty()) {
			palette = vfs::palette(palfile);
		}

		glm::vec4 points = glm::zero<glm::vec4>();
		glm::vec2 axnpnt = glm::zero<glm::vec2>();

		for (arch_t chunk = 1; chunk < setup.size(); ++chunk) {
			glm::vec2 starts = glm::zero<glm::vec2>();
			glm::vec2 vksize = glm::zero<glm::vec2>();
			real64_t tdelay  = 0.0;
			arch_t hvtype 	 = 0;
			arch_t frames 	 = 0;
			bool_t repeat 	 = true;
			bool_t reflect 	 = false;
			setup.get(chunk, "starts", starts);
			setup.get(chunk, "vksize", vksize);
			setup.get(chunk, "tdelay", tdelay);
			setup.get(chunk, "hvtype", hvtype);
			setup.get(chunk, "frames", frames);
			setup.get(chunk, "repeat", repeat);
			setup.get(chunk, "reflect", reflect);

			auto& sequence = sequences.emplace_back(vksize, tdelay, frames, repeat, reflect);
			for (arch_t d = 0; d < hvtype; ++d) {
				axnpnt = glm::zero<glm::vec2>();
				setup.get(chunk, std::to_string(d) + "-X", axnpnt);
				sequence.append(axnpnt);
				for (arch_t f = 0; f < frames; ++f) {
					points = glm::zero<glm::vec4>();
					setup.get(
						chunk,
						std::to_string(d) + '-' + std::to_string(f),
						points
					);
					sequence.append(inverts, starts, points);
				}
			}
		}
		ready = true;
	} else {
		synao_log("Failed to load animation from %s!\n", full_path.c_str());
	}
}

void animation_t::load(const std::string& full_path, thread_pool_t& thread_pool) {
	assert(!ready);
	this->future = thread_pool.push([this](const std::string& full_path) -> void {
		this->load(full_path);
	}, full_path);
}

void animation_t::assure() const {
	if (!ready and future.valid()) {
		future.wait();
	}
}

bool animation_t::visible(const rect_t& viewport, arch_t state, arch_t frame, arch_t variation, layer_t layer, glm::vec2 position, glm::vec2 scale) const {
	if (layer == layer_value::Invisible) {
		return false;
	}
	this->assure();
	if (state < sequences.size()) {
		glm::vec2 sequsize = sequences[state].get_dimensions();
		glm::vec2 sequorig = sequences[state].get_origin(frame, variation, mirroring_t::None);
		return viewport.overlaps(position - sequorig, sequsize * scale);
	}
	return false;
}

bool animation_t::is_finished(arch_t state, arch_t frame, real64_t timer) const {
	this->assure();
	if (state < sequences.size()) {
		return sequences[state].is_finished(frame, timer);
	}
	return false;
}

glm::vec2 animation_t::get_origin(arch_t state, arch_t frame, arch_t variation, mirroring_t mirroring) const {
	this->assure();
	if (state < sequences.size()) {
		return sequences[state].get_origin(frame, variation, mirroring);
	}
	return glm::zero<glm::vec2>();
}

glm::vec2 animation_t::get_action_point(arch_t state, arch_t variation, mirroring_t mirroring) const {
	this->assure();
	if (state < sequences.size()) {
		return sequences[state].get_action_point(variation, mirroring);
	}
	return glm::zero<glm::vec2>();
}
