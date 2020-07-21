#ifndef SYNAO_PRECOMPILED_HEADER_HPP
#define SYNAO_PRECOMPILED_HEADER_HPP

// C standard libraries
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cinttypes>
#include <cassert>
#include <cstring>
#include <cstdio>

// C++ standard libraries
#include <typeinfo>
#include <bitset>
#include <functional>
#include <utility>
#include <chrono>
#include <tuple>
#include <optional>
#include <new>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <array>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <random>
#include <numeric>
#include <fstream>
#include <sstream>
#include <iomanip> 
#include <streambuf>
#include <locale>
#include <atomic>
#include <thread>
#include <mutex>
#include <future>

// C++ filesystem library is weird on Apple Clang
#ifndef __APPLE__
#include <filesystem>
#else
#include <dirent.h>
#endif // __APPLE__

// Other libraries
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <entt/entt.hpp>
#include <tmxlite/Map.hpp>
#include <tmxlite/TileLayer.hpp>
#include <angelscript.h>
#include <nlohmann/json.hpp>
#include <SDL2/SDL.h>

#include "./types.hpp"

#endif // SYNAO_PRECOMPILED_HEADER_HPP