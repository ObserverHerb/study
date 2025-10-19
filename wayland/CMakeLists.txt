cmake_minimum_required(VERSION 3.31)
project(Playland
	VERSION 0.1
	LANGUAGES C CXX
	# SPDX_LICENSE MIT
	DESCRIPTION "Linux desktop GUI programming from scratch using Wayland"
)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clangd, so it knows we're using C++23

set(CMAKE_INCLUDE_CURRENT_DIR ON) # includes build directory for generated xdg headers
find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_FIND_MODULE_DIR})
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
ecm_add_wayland_client_protocol( # https://gitlab.freedesktop.org/wayland/wayland-protocols/-/merge_requests/337, https://wayland.app/protocols/pointer-warp-v1
	XDG_SOURCES
	BASENAME xdg-pointer-warp-v1
	PROTOCOL
	${WaylandProtocols_DATADIR}/staging/pointer-warp/pointer-warp-v1.xml
)
add_executable(playland main.cpp ${XDG_SOURCES})

target_link_libraries(playland PUBLIC Wayland::Client)

