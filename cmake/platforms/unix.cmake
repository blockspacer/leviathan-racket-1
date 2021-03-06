cmake_minimum_required (VERSION 3.15)

find_package (Threads REQUIRED)
target_link_libraries (leviathan PRIVATE
	Threads::Threads
	${CMAKE_DL_LIBS}
)

if (NOT ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang")
	target_link_libraries (leviathan PRIVATE stdc++fs)
endif ()
