#ifndef LEVIATHAN_INCLUDED_UTILITY_WATCH_HPP
#define LEVIATHAN_INCLUDED_UTILITY_WATCH_HPP

#include <chrono>

#include "../types.hpp"

struct watch_t {
public:
	watch_t();
	watch_t(const watch_t&) = default;
	watch_t(watch_t&&) = default;
	watch_t& operator=(const watch_t&) = default;
	watch_t& operator=(watch_t&&) = default;
	~watch_t() = default;
public:
	real64_t elapsed() const;
	real64_t restart();
	static sint64_t timestamp();
private:
	std::chrono::time_point<std::chrono::high_resolution_clock> point;
};

#endif // LEVIATHAN_INCLUDED_UTILITY_WATCH_HPP
