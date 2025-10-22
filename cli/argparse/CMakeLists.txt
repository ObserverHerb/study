cmake_minimum_required(VERSION 3.31)
project(Args
	VERSION 0.1
	LANGUAGES C CXX
	# SPDX_LICENSE MIT
	DESCRIPTION "Parsing command line arguments via Boost"
)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clangd, so it knows we're using C++23

set(CMAKE_INCLUDE_CURRENT_DIR ON) # includes build directory for generated xdg headers
find_package(Boost COMPONENTS program_options REQUIRED)

add_executable(args main.cpp)

target_link_libraries(args PUBLIC Boost::program_options)

