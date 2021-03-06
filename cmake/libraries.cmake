cmake_minimum_required (VERSION 3.15)

find_package (Angelscript CONFIG REQUIRED)
find_package (glm CONFIG REQUIRED)
find_package (EnTT CONFIG REQUIRED)

include ("${PROJECT_SOURCE_DIR}/cmake/modules/FindTmxlite.cmake")
if (NOT TMXLITE_FOUND)
	message (FATAL_ERROR "Could not find Tmxlite!")
endif ()

include ("${PROJECT_SOURCE_DIR}/cmake/modules/FindNlohmannJson.cmake")
if (NOT NLOHMANN_JSON_FOUND)
	message (FATAL_ERROR "Could not find JSON library!")
endif ()

include ("${PROJECT_SOURCE_DIR}/cmake/modules/FindStb.cmake")
if (NOT STB_FOUND)
	message (FATAL_ERROR "Could not find stb!")
endif ()

# Vcpkg makes OpenAL and SDL2 finnicky

if (VCPKG_TOOLCHAIN)
	find_package (OpenAL CONFIG REQUIRED)
	set (OPENAL_LIBRARY OpenAL::OpenAL)

	find_package (SDL2 CONFIG REQUIRED)
	if (APPLE)
		set (SDL2_LIBRARIES SDL2::SDL2)
	elseif (MINGW)
		set (SDL2_LIBRARIES "-lmingw32" SDL2::SDL2main SDL2::SDL2 "-mwindows")
	else ()
		set (SDL2_LIBRARIES SDL2::SDL2main SDL2::SDL2)
	endif ()
else ()
	include ("${PROJECT_SOURCE_DIR}/cmake/modules/FindOpenAL.cmake")
	if (NOT OPENAL_FOUND)
		message (FATAL_ERROR "Could not find OpenAL!")
	endif ()
	include ("${PROJECT_SOURCE_DIR}/cmake/modules/FindSDL2.cmake")
	if (NOT SDL2_FOUND)
		message (FATAL_ERROR "Could not find SDL2!")
	elseif (NOT SDL2MAIN_FOUND AND NOT APPLE)
		message (FATAL_ERROR "Could not find SDL2-Main!")
	endif ()
endif ()
