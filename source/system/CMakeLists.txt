cmake_minimum_required (VERSION 3.15)

target_sources (leviathan PRIVATE
	"audio.cpp" "audio.hpp"
	"editor.cpp" "editor.hpp"
	"input.cpp" "input.hpp"
	"kernel.cpp" "kernel.hpp"
	"main.cpp"
	"music.cpp" "music.hpp"
	"renderer.cpp" "renderer.hpp"
	"runtime.cpp" "runtime.hpp"
	"video.cpp" "video.hpp"
)
