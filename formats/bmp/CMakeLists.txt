cmake_minimum_required(VERSION 4.00)
project(BMP
	VERSION 0.1
	LANGUAGES C CXX
	DESCRIPTION "Open and display a bitmap file"
)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2c -stdlib=libc++ -freflection-latest")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clangd, so it knows we're using C++26
set(CMAKE_INCLUDE_CURRENT_DIR ON) # includes build directory for generated xdg headers

# boost
include(FetchContent)
FetchContent_Declare(boost GIT_REPOSITORY https://github.com/boostorg/boost.git)
FetchContent_MakeAvailable(boost)
# extra cmake modules
find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_FIND_MODULE_DIR})
# wayland
find_package(Wayland REQUIRED)
find_package(WaylandProtocols REQUIRED)
find_package(WaylandScanner REQUIRED) # https://api.kde.org/ecm/find-module/FindWaylandScanner.html

ecm_add_wayland_client_protocol(
	XDG_SOURCES
	BASENAME xdg-shell
	PROTOCOL ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
)
ecm_add_wayland_client_protocol( # https://redstrate.com/blog/2023/07/how-to-use-xdg-decoration-in-a-wayland-client/
	XDG_SOURCES
	BASENAME xdg-decoration-v1
	PROTOCOL
	${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
)

add_executable(bmp main.cpp ${XDG_SOURCES})

target_link_libraries(bmp PUBLIC Boost::program_options Wayland::Client)
