#include "./vertex.hpp"
#include "./glcheck.hpp"

#include "../utility/logger.hpp"

#include <utility>
#include <cstddef>

vertex_spec_t::vertex_spec_t() :
	detail(nullptr),
	length(0)
{

}

vertex_spec_t::vertex_spec_t(const vertex_spec_t& that) : vertex_spec_t() {
	if (this != &that) {
		detail = that.detail;
		length = that.length;
	}
}

vertex_spec_t::vertex_spec_t(vertex_spec_t&& that) noexcept : vertex_spec_t() {
	if (this != &that) {
		std::swap(detail, that.detail);
		std::swap(length, that.length);
	}
}

vertex_spec_t& vertex_spec_t::operator=(const vertex_spec_t& that) {
	if (this != &that) {
		detail = that.detail;
		length = that.length;
	}
	return *this;
}

vertex_spec_t& vertex_spec_t::operator=(vertex_spec_t&& that) noexcept {
	if (this != &that) {
		std::swap(detail, that.detail);
		std::swap(length, that.length);
	}
	return *this;
}

bool vertex_spec_t::compare(const uint_t* lhv, const uint_t* rhv) {
	if (lhv == nullptr or rhv == nullptr) {
		synao_log("Warning! vertex_spec_t::compare has null inputs!\n");
		return false;
	}
	while (*lhv) {
		if (*lhv != *rhv) {
			break;
		}
		lhv++; rhv++;
	}
	return (*(const uint_t*)lhv - *(const uint_t*)rhv) == 0;
}

vertex_spec_t vertex_spec_t::from(const uint_t* list) {
	static const uint_t kMinor[] = { GL_FLOAT_VEC2, 0 };
	static const uint_t kBlank[] = { GL_FLOAT_VEC2, GL_FLOAT_VEC4, 0 };
	static const uint_t kMajor[] = { GL_FLOAT_VEC2, GL_FLOAT_VEC3, GL_FLOAT, 0 };
	vertex_spec_t result;
	if (vertex_spec_t::compare(list, kMinor)) {
		result = vertex_spec_t::from(typeid(vtx_minor_t));
	} else if (vertex_spec_t::compare(list, kBlank)) {
		result = vertex_spec_t::from(typeid(vtx_blank_t));
	} else if (vertex_spec_t::compare(list, kMajor)) {
		result = vertex_spec_t::from(typeid(vtx_major_t));
	}
	return result;
}

vertex_spec_t vertex_spec_t::from(const std::type_info& info) {
	vertex_spec_t result;
	if (info == typeid(vtx_minor_t)) {
		result.length = sizeof(vtx_minor_t);
		result.detail = [] {
			glCheck(glEnableVertexAttribArray(0));
			glCheck(glVertexAttribPointer(
				0, glm::vec2::length(),
				GL_FLOAT, GL_FALSE,
				sizeof(vtx_minor_t),
				(const optr_t)offsetof(vtx_minor_t, position)
			));
		};
	} else if (info == typeid(vtx_blank_t)) {
		result.length = sizeof(vtx_blank_t);
		result.detail = [] {
			glCheck(glEnableVertexAttribArray(0));
			glCheck(glVertexAttribPointer(
				0, glm::vec2::length(),
				GL_FLOAT, GL_FALSE,
				sizeof(vtx_blank_t),
				(const optr_t)offsetof(vtx_blank_t, position)
			));
			glCheck(glEnableVertexAttribArray(1));
			glCheck(glVertexAttribPointer(
				1, glm::vec4::length(),
				GL_FLOAT, GL_FALSE, sizeof(vtx_blank_t),
				(const optr_t)offsetof(vtx_blank_t, color)
			));
		};
	} else if (info == typeid(vtx_major_t)) {
		result.length = sizeof(vtx_major_t);
		result.detail = [] {
			glCheck(glEnableVertexAttribArray(0));
			glCheck(glVertexAttribPointer(
				0, glm::vec2::length(),
				GL_FLOAT, GL_FALSE, sizeof(vtx_major_t),
				(const optr_t)offsetof(vtx_major_t, position)
			));
			glCheck(glEnableVertexAttribArray(1));
			glCheck(glVertexAttribPointer(
				1, glm::vec3::length(),
				GL_FLOAT, GL_FALSE,
				sizeof(vtx_major_t),
				(const optr_t)offsetof(vtx_major_t, uvcoords)
			));
			glCheck(glEnableVertexAttribArray(2));
			glCheck(glVertexAttribPointer(
				2, glm::vec1::length(),
				GL_FLOAT, GL_FALSE,
				sizeof(vtx_major_t),
				(const optr_t)offsetof(vtx_major_t, alpha)
			));
		};
	}
	if (result.length == 0) {
		synao_log("Warning! vertex_spec_t result has a length of zero!\n");
	}
	return result;
}

bool vertex_spec_t::operator==(const vertex_spec_t& that) {
	return (
		this->detail == that.detail and
		this->length == that.length
	);
}

bool vertex_spec_t::operator!=(const vertex_spec_t& that) {
	return !(*this == that);
}
