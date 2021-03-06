#include "./palette.hpp"
#include "./glcheck.hpp"

#include "../utility/logger.hpp"
#include "../utility/thread_pool.hpp"

palette_t::palette_t() :
	ready(false),
	future(),
	handle(0),
	dimensions(0),
	format(pixel_format_t::Invalid)
{

}

palette_t::palette_t(palette_t&& that) noexcept : palette_t() {
	if (this != &that) {
		std::atomic<bool> temp = ready.load();
		ready.store(that.ready.load());
		that.ready.store(temp.load());

		std::swap(future, that.future);
		std::swap(handle, that.handle);
		std::swap(dimensions, that.dimensions);
		std::swap(format, that.format);
	}
}

palette_t& palette_t::operator=(palette_t&& that) noexcept {
	if (this != &that) {
		std::atomic<bool> temp = ready.load();
		ready.store(that.ready.load());
		that.ready.store(temp.load());

		std::swap(future, that.future);
		std::swap(handle, that.handle);
		std::swap(dimensions, that.dimensions);
		std::swap(format, that.format);
	}
	return *this;
}

palette_t::~palette_t() {
	this->destroy();
}

void palette_t::load(const std::string& full_path, pixel_format_t format, thread_pool_t& thread_pool) {
	assert(!ready);
	this->format = format;
	this->future = thread_pool.push([](const std::string& full_path) -> image_t {
		return image_t::generate(full_path);
	}, full_path);
}

bool palette_t::create(glm::ivec2 dimensions, pixel_format_t format) {
	if (!handle) {
		this->dimensions = dimensions;
		this->format = format;

		uint_t gl_enum = gfx_t::get_pixel_format_gl_enum(format);

		glCheck(glGenTextures(1, &handle));
		glCheck(glBindTexture(GL_TEXTURE_2D, handle));

		if (sampler_t::has_immutable_option()) {
			glCheck(glTexStorage2D(GL_TEXTURE_2D, 1, gl_enum, dimensions.x, dimensions.y));
		} else {
			glCheck(glTexImage2D(GL_TEXTURE_2D, 0, gl_enum, dimensions.x, dimensions.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
		}

		glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
		glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
		glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		return true;
	}
	synao_log("Warning! Tried to overwrite existing palette!\n");
	return false;
}

void palette_t::destroy() {
	if (future.valid()) {
		auto result = future.get();
	}
	ready = false;
	if (handle != 0) {
		glCheck(glDeleteTextures(1, &handle));
		handle = 0;
	}
	dimensions	= glm::zero<glm::ivec2>();
	format		= pixel_format_t::Invalid;
}

void palette_t::assure() {
	if (!ready and future.valid()) {
		// Do palette loading now
		const image_t image = future.get();
		if (!image.empty()) {
			if (this->create(image.get_dimensions(), format)) {
				glCheck(glTexSubImage2D(
					GL_TEXTURE_2D, 0, 0, 0,
					dimensions.x, dimensions.y,
					GL_RGBA, GL_UNSIGNED_BYTE, &image[0]
				));
				glCheck(glGenerateMipmap(GL_TEXTURE_2D));
			}
			glCheck(glBindTexture(GL_TEXTURE_2D, 0));
		}
		ready = true;
	}
}

void palette_t::assure() const {
	if (!ready) {
		const_cast<palette_t*>(this)->assure();
	}
}

glm::vec2 palette_t::get_dimensions() const {
	this->assure();
	return glm::vec2(dimensions);
}

glm::vec2 palette_t::get_inverse_dimensions() const {
	this->assure();
	if (dimensions.x != 0.0f and dimensions.y != 0.0f) {
		return 1.0f / glm::vec2(dimensions);
	}
	return glm::one<glm::vec2>();
}

glm::ivec2 palette_t::get_integral_dimensions() const {
	this->assure();
	return dimensions;
}

real_t palette_t::convert(real_t index) const {
	this->assure();
	if (dimensions.y > 0) {
		return index / static_cast<real_t>(dimensions.y);
	}
	return 0.0f;
}
