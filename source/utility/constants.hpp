#ifndef LEVIATHAN_INCLUDED_UTILITY_CONSTANTS_HPP
#define LEVIATHAN_INCLUDED_UTILITY_CONSTANTS_HPP

#include <type_traits>

#include "../types.hpp"

namespace constants {
	constexpr byte_t WindowName[] = "Leviathan Racket";
	template<typename T>
	constexpr T NormalWidth() { return static_cast<T>(320); }
	template<typename T>
	constexpr T NormalHeight() { return static_cast<T>(180); }
	template<typename T>
	constexpr T ImguiWidth() { return static_cast<T>(800); }
	template<typename T>
	constexpr T ImguiHeight() { return static_cast<T>(800); }
	template<typename T>
	glm::vec<2, T, glm::packed_highp> NormalDimensions() {
		return glm::vec<2, T, glm::packed_highp>(
			constants::NormalWidth<T>(),
			constants::NormalHeight<T>()
		);
	}
	template<typename T>
	glm::vec<2, T, glm::packed_highp> ImguiDimensions() {
		return glm::vec<2, T, glm::packed_highp>(
			constants::ImguiWidth<T>(),
			constants::ImguiHeight<T>()
		);
	}
	template<typename T>
	constexpr T TileSize() { return static_cast<T>(16); }
	template<typename T>
	constexpr T HalfTile() { return constants::TileSize<T>() / 2; }
	template<typename T>
	glm::vec<2, T, glm::packed_highp> TileDimensions() {
		return glm::vec<2, T, glm::packed_highp>(
			constants::TileSize<T>(),
			constants::TileSize<T>()
		);
	}
	template<typename F = real64_t>
	constexpr F MinInterval() {
		static_assert(std::is_floating_point<F>::value);
		return static_cast<F>(1.0) / static_cast<F>(60.0);
	}
	template<typename F = real64_t>
	constexpr F MaxInterval() {
		static_assert(std::is_floating_point<F>::value);
		return static_cast<F>(1.0) / static_cast<F>(62.0);
	}
}

#endif // LEVIATHAN_INCLUDED_UTILITY_CONSTANTS_HPP
