#ifndef LEVIATHAN_INCLUDED_SYSTEM_VIDEO_HPP
#define LEVIATHAN_INCLUDED_SYSTEM_VIDEO_HPP

#include <tuple>
#include <SDL2/SDL_video.h>

#include "../utility/watch.hpp"

struct setup_file_t;
struct frame_buffer_t;

struct screen_params_t {
public:
	bool_t vsync, full;
	sint_t scaling;
	real64_t framerate;
	static constexpr sint_t 	kDefaultScaling 	= 1;
	static constexpr sint_t 	kHighestScaling		= 12; // 4K
	static constexpr real64_t 	kDefaultFramerate 	= 60.0;
public:
	screen_params_t(bool_t vsync, bool_t full, sint_t scaling, real_t framerate) :
		vsync(vsync),
		full(full),
		scaling(scaling),
		framerate(framerate) {}
	screen_params_t() :
		vsync(false),
		full(false),
		scaling(kDefaultScaling),
		framerate(kDefaultFramerate) {}
	screen_params_t(const screen_params_t&) = default;
	screen_params_t(screen_params_t&&) = default;
	screen_params_t& operator=(const screen_params_t&) = default;
	screen_params_t& operator=(screen_params_t&&) = default;
	~screen_params_t() = default;
};

struct video_t : public not_copyable_t {
public:
	video_t();
	video_t(video_t&&) = default;
	video_t& operator=(video_t&&) = default;
	~video_t();
public:
	bool init(const setup_file_t& config, bool start_imgui = false);
	void submit(const frame_buffer_t* frame_buffer, arch_t index) const;
	void flush() const;
	void set_parameters(screen_params_t params);
	screen_params_t get_parameters() const;
	glm::vec2 get_dimensions() const;
	glm::ivec2 get_integral_dimensions() const;
	glm::ivec2 get_imgui_dimensions() const;
	glm::ivec2 get_opengl_version() const;
	auto get_device() const {
		return std::make_tuple(window, context);
	}
private:
	SDL_Window* window;
	SDL_GLContext context;
	screen_params_t params;
	sint_t major, minor;
};

#endif // LEVIATHAN_INCLUDED_SYSTEM_VIDEO_HPP
