#include <wayland-client.h>
#include <cstdint>
#include <string>
#include <random>
#include <atomic>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
#include "wayland-xdg-shell-client-protocol.h" // FIXME: don't put this in build
#include "wayland-xdg-decoration-v1-client-protocol.h"

// https://wayland.freedesktop.org/docs/html/apa.html
wl_display *display;
wl_compositor *compositor;
wl_surface *surface;
wl_shm *sharedMemoryInterface;
wl_buffer *sharedMemoryBuffer;
wl_seat *inputSeat;
xdg_wm_base *windowManager;
zxdg_decoration_manager_v1 *windowDecorationManager;
std::mt19937 generator;
std::uniform_int_distribution<int> distribution(0,std::numeric_limits<int>::max());
constexpr int width=800;
constexpr int height=600;
std::atomic<bool> live=true;

void AddRegistryGlobal(void *userData,wl_registry *registry,std::uint32_t nameID,const char *interface,std::uint32_t version)
{
	std::string haystack(interface);
	if (haystack.contains("wl_compositor"))
	{
		compositor=static_cast<wl_compositor*>(wl_registry_bind(registry,nameID,&wl_compositor_interface,1));
	}
	if (haystack.contains("wl_shm"))
	{
		sharedMemoryInterface=static_cast<wl_shm*>(wl_registry_bind(registry,nameID,&wl_shm_interface,1));
	}
	if (haystack.contains("xdg_wm_base"))
	{
		windowManager=static_cast<xdg_wm_base*>(wl_registry_bind(registry,nameID,&xdg_wm_base_interface,1));
	}
	if (haystack.contains("zxdg_decoration_manager_v1"))
	{
		windowDecorationManager=static_cast<zxdg_decoration_manager_v1*>(wl_registry_bind(registry,nameID,&zxdg_decoration_manager_v1_interface,1));
	}
	if (haystack.contains("wl_seat"))
	{
		inputSeat=static_cast<wl_seat*>(wl_registry_bind(registry,nameID,&wl_seat_interface,1));
	}
}

void RemoveRegistryGlobal(void *userData,wl_registry *registry,std::uint32_t nameID)
{

}

struct wl_registry_listener RegistryDispatch={
	.global=AddRegistryGlobal,
	.global_remove=RemoveRegistryGlobal
};

static void xdg_wm_base_ping(void *userData,xdg_wm_base *windowManager,std::uint32_t serial)
{
	xdg_wm_base_pong(windowManager,serial);
}
static const xdg_wm_base_listener WindowManagerDispatch={
	xdg_wm_base_ping
};

static void SurfaceConfigure(void *userData,xdg_surface *xdgSurface,std::uint32_t serial)
{
	xdg_surface_ack_configure(xdgSurface,serial);
}
static const xdg_surface_listener SurfaceDispatch={
	SurfaceConfigure
};

static void TopLevelConfigure(void *userData,xdg_toplevel *windowTopLevel,std::int32_t width,std::int32_t height,wl_array *states) { }
static void TopLevelClose(void *userData,xdg_toplevel *windowTopLevel)
{
	live=false;
}
static const xdg_toplevel_listener TopLevelDispatch={
	.configure=TopLevelConfigure,
	.close=TopLevelClose
};

void Draw(void *userData,wl_callback *callback,std::uint32_t time);
static const wl_callback_listener FrameDispatch={
	.done=Draw
};

void Draw(void *userData,wl_callback *callback,std::uint32_t time)
{
	wl_callback_destroy(callback);

	int *pixelData=reinterpret_cast<int*>(userData);
	for (int index=0; index < width*height; index++)
	{
		int random_number=distribution(generator);
		pixelData[index]=random_number;
	}

	wl_callback *frame=wl_surface_frame(surface);
	wl_callback_add_listener(frame,&FrameDispatch,userData);

	wl_surface_attach(surface,sharedMemoryBuffer,0,0);
	wl_surface_damage(surface,0,0,width,height);
	wl_surface_commit(surface);
};

static void KeyboardKeymap(void *userData,wl_keyboard *keyboard,std::uint32_t format,int fd,std::uint32_t size) { }
static void KeyboardFocused(void *userData,wl_keyboard *keyboard,std::uint32_t serial,wl_surface *surface,wl_array *keys) { }
static void KeyboardUnfocused(void *userData,wl_keyboard *keyboard,std::uint32_t serial,wl_surface *surface) { }
static void KeyboardRepeat(void *userData,wl_keyboard *keyboard,int rate,int delay) { }
static void KeyboardModifiers(void *userData,wl_keyboard *keyboard,std::uint32_t serial,std::uint32_t mods_depressed,std::uint32_t mods_latched,std::uint32_t mods_locked,std::uint32_t group) { }
static void KeyboardEvent(void *userData,wl_keyboard *keyboard,std::uint32_t serial,std::uint32_t time,std::uint32_t key,std::uint32_t state)
{
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
	{
		if (key == 1)
		{
			live=false;
		}
	}
}
static const wl_keyboard_listener KeyboardDispatch={
	.keymap=KeyboardKeymap,
	.enter=KeyboardFocused,
	.leave=KeyboardUnfocused,
	.key=KeyboardEvent,
	.modifiers=KeyboardModifiers,
	.repeat_info=KeyboardRepeat
};

static void MouseFocused(void *userData,wl_pointer *mouse,std::uint32_t serial,wl_surface *surface,wl_fixed_t surfaceX,wl_fixed_t surfaceY) { }
static void MouseUnfocused(void *userData,wl_pointer *mouse,std::uint32_t serial,wl_surface *surface) { }
static void MouseMoved(void *userData,wl_pointer *mouse,std::uint32_t time,wl_fixed_t surfaceX,wl_fixed_t surfaceY)
{
	std::cout << "X: " << wl_fixed_to_double(surfaceX) << ", Y: " << wl_fixed_to_double(surfaceY) << std::endl;

}

static const wl_pointer_listener MouseDispatch={
	.enter=MouseFocused,
	.leave=MouseUnfocused,
	.motion=MouseMoved
};

int main()
{
	enum class ExitCode
	{
		SUCCESS,
		WAYLAND_CONNECTION_FAILED
	};

	display=wl_display_connect(nullptr);
	if (!display) return static_cast<int>(ExitCode::WAYLAND_CONNECTION_FAILED);

	auto registry=wl_display_get_registry(display);
	wl_registry_add_listener(registry,&RegistryDispatch,nullptr);
	wl_display_roundtrip(display); // wait for things to finish server-side

	surface=wl_compositor_create_surface(compositor);
	auto windowManagerSurface=xdg_wm_base_get_xdg_surface(windowManager,surface);
	auto windowTopLevel=xdg_surface_get_toplevel(windowManagerSurface);
	xdg_wm_base_add_listener(windowManager,&WindowManagerDispatch,nullptr);
	xdg_surface_add_listener(windowManagerSurface,&SurfaceDispatch,nullptr);
	xdg_toplevel_set_title(windowTopLevel,"Playland :D");
	xdg_toplevel_add_listener(windowTopLevel,&TopLevelDispatch,nullptr);
	auto windowDecoration=zxdg_decoration_manager_v1_get_toplevel_decoration(windowDecorationManager,windowTopLevel);
	wl_surface_commit(surface);

	wl_display_roundtrip(display); // wait for things to finish server-side

	constexpr int stride=width*sizeof(width);
	constexpr int surfaceMemorySize=stride*height;
	int sharedMemoryFileDescriptor=memfd_create("playland-shm",MFD_CLOEXEC);
	if (sharedMemoryFileDescriptor < 0) goto DISCONNECT;
	if (ftruncate(sharedMemoryFileDescriptor,surfaceMemorySize) < 0) goto CLOSE_SHARED_MEMORY;

	{
		void *pixelData=mmap(nullptr,surfaceMemorySize,PROT_READ|PROT_WRITE,MAP_SHARED,sharedMemoryFileDescriptor,0);
		if (pixelData == MAP_FAILED) goto CLOSE_SHARED_MEMORY;
		for (int index=0; index < width*height; index++) static_cast<int*>(pixelData)[index]=0xff0000ff;

		auto sharedMemoryPool=wl_shm_create_pool(sharedMemoryInterface,sharedMemoryFileDescriptor,surfaceMemorySize);
		sharedMemoryBuffer=wl_shm_pool_create_buffer(sharedMemoryPool,0,width,height,stride,WL_SHM_FORMAT_ARGB8888);

		wl_callback *frame=wl_surface_frame(surface);
		wl_callback_add_listener(frame,&FrameDispatch,pixelData); // this needs to happen BEFORE wl_surface_commit

		wl_surface_attach(surface,sharedMemoryBuffer,0,0);
		wl_surface_damage(surface,0,0,width,height);
		wl_surface_commit(surface);

		auto keyboard=wl_seat_get_keyboard(inputSeat);
		wl_keyboard_add_listener(keyboard,&KeyboardDispatch,nullptr);

		auto mouse=wl_seat_get_pointer(inputSeat);
		wl_pointer_add_listener(mouse,&MouseDispatch,nullptr);

		while (live && wl_display_dispatch(display));

		munmap(pixelData,surfaceMemorySize);
		wl_shm_pool_destroy(sharedMemoryPool);
	}

	CLOSE_SHARED_MEMORY:
	close(sharedMemoryFileDescriptor);
	DISCONNECT:
	wl_display_disconnect(display);
	return 0;
}
