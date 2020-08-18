cmake_minimum_required (VERSION 3.15)

find_package (Threads REQUIRED)
target_link_libraries (leviathan PRIVATE
	Threads::Threads
	${CMAKE_DL_LIBS}
)

if (APPLE)
	target_compile_definitions (leviathan PRIVATE "-DTARGET_MISSING_STL_FILESYSTEM")
else ()
	target_link_libraries (leviathan PRIVATE stdc++fs)
endif ()
