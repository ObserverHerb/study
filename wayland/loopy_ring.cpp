#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <wayland-client.h>
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <string>
#include <atomic>
#include <queue>
#include <mutex>
#include <thread>
#include <functional>
#include <sys/mman.h>
#include <unistd.h>
#include "wayland-xdg-shell-client-protocol.h"
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
constexpr int WIDTH=240;
constexpr int HEIGHT=240;
std::atomic<bool> live=true;
auto lastTick=std::chrono::steady_clock::now();

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

void RemoveRegistryGlobal(void *userData,wl_registry *registry,std::uint32_t nameID) { }

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

	unsigned int *pixelData=reinterpret_cast<unsigned int*>(userData);

	auto now=std::chrono::steady_clock::now();
	static std::chrono::duration<float> elapsed(0);
	elapsed+=now-lastTick;
	lastTick=now;

	glm::vec2 r(static_cast<float>(WIDTH),static_cast<float>(HEIGHT)); // twigl's r, same as iResolution in ShaderToy
	// lambda that will be run by a task in the thread pool queue for each pixel
	// this is essentially our shader program
	auto Donut=[&r,pixelData](int x,int y) {
		glm::vec4 fragmentCoordinate(static_cast<float>(x),static_cast<float>(y),1.0f,1.0f);
		glm::vec4 o(0.0f);

		auto h=[](glm::vec3 c)->glm::vec3 {
			glm::vec3 r=glm::clamp(glm::abs(glm::mod(c.x*6.0f+glm::vec3(0.0f,4.0f,2.0f),6.0f)-3.0f)-1.0f,0.0f,1.0f);
			return c.z+c.y*(r-0.5f)*(1.0f-glm::abs(2.0f*c.z-1.0f));
		};
		glm::vec2 u=(fragmentCoordinate.xy()-r.xy()*0.5f)/r.y;
		o=glm::vec4(0.0f);
		for(float i=0.0f; i < 0.99; i+=0.01f)
		{
			glm::vec2 v=u+glm::vec2(glm::sin(i*6.28318f)*0.25f*glm::sin((elapsed.count())*0.6f),glm::cos(i*6.28318f)*0.25f*glm::cos((elapsed.count())/2*0.6f));
			float c=glm::smoothstep(0.03f,0.0f,glm::abs(glm::length(v)-0.1f));
			o+=c*glm::vec4(h(glm::vec3(i,0.8f,0.6f)),1.0f)*0.08f;
		}

		int index=(HEIGHT-y-1)*WIDTH+x; // draw bottom up since gl_FragCoord using cartesian coordinates, not screen coordinates
		unsigned int &argb=static_cast<unsigned int*>(pixelData)[index];
		argb=0;
		argb|=static_cast<unsigned int>(255) << 24;
		argb|=static_cast<unsigned int>(std::clamp(o.r*255,0.0f,255.0f)) << 16;
		argb|=static_cast<unsigned int>(std::clamp(o.g*255,0.0f,255.0f)) << 8;
		argb|=static_cast<unsigned int>(std::clamp(o.b*255,0.0f,255.0f));
	};

	// populate the thread pool task queue
	std::queue<std::function<void()>> shaders;
	auto threadCount=std::thread::hardware_concurrency(); // use maximum number of threads
	for (int y=0; y < HEIGHT; y++)
	{
		for (int x=0; x < WIDTH; x++)
		{
			shaders.push(std::bind(Donut,x,y)); // store call to lambda with the pixel position it will be working on
		}
	}

	// create the "shaders" that will run the tasks from the task queue
	std::mutex shaderQueueMutex;
	auto Shader=[&shaders,&shaderQueueMutex]() {
		bool empty=false;
		shaderQueueMutex.lock(); // lock A
		// critical section - checking size of queue and removing an item cannot be done concurrently
		while (!shaders.empty())
		{
			auto Program=shaders.front();
			shaders.pop();
			// end critical section
			shaderQueueMutex.unlock(); // unlocks A once, then B each subsequent iteration

			Program(); // run the shader program (lambda) on this shader (thread)

			shaderQueueMutex.lock(); // lock B
		}
		shaderQueueMutex.unlock(); // unlocks B since iteration ends before unlocking
	};

	// create and start the threads that will act as shaders
	std::vector<std::jthread> threads;
	for (int threadNumber=0; threadNumber < threadCount; threadNumber++)
	{
		threads.emplace_back(Shader); // creating a std::jthread starts it
	}
	for (auto &thread : threads)
	{
		thread.join(); // wait for all threads to finish emptying the queue
	}

	wl_callback *frame=wl_surface_frame(surface);
	wl_callback_add_listener(frame,&FrameDispatch,userData); // set callback for next frame request

	wl_surface_attach(surface,sharedMemoryBuffer,0,0);
	wl_surface_damage(surface,0,0,WIDTH,HEIGHT);
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
		if (key == 1) // escape
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
static void MouseMoved(void *userData,wl_pointer *mouse,std::uint32_t time,wl_fixed_t surfaceX,wl_fixed_t surfaceY) { }
static void MouseClicked(void *userData,wl_pointer *mouse,std::uint32_t serial,std::uint32_t time,std::uint32_t button,std::uint32_t state) { }
static void MouseScroll(void *userData,wl_pointer *mouse,std::uint32_t time,std::uint32_t axis,wl_fixed_t value) { }
static const wl_pointer_listener MouseDispatch={
	.enter=MouseFocused,
	.leave=MouseUnfocused,
	.motion=MouseMoved,
	.button=MouseClicked,
	.axis=MouseScroll
};

int main(int argumentCount,char *arguments[])
{
	enum class ExitCode
	{
		SUCCESS,
		INVALID_ARGUMENT,
		INVALID_FILE,
		WAYLAND_CONNECTION_FAILED
	};

	// initialize wayland
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
	xdg_toplevel_set_title(windowTopLevel,"Loopy Ring (by @supahfunk)");
	xdg_toplevel_add_listener(windowTopLevel,&TopLevelDispatch,nullptr);
	auto windowDecoration=zxdg_decoration_manager_v1_get_toplevel_decoration(windowDecorationManager,windowTopLevel);
	wl_surface_commit(surface);

	wl_display_roundtrip(display); // wait for things to finish server-side

	int stride=WIDTH*sizeof(uint32_t);
	int surfaceMemorySize=stride*HEIGHT;
	int sharedMemoryFileDescriptor=memfd_create("loopy-ring-shm",MFD_CLOEXEC);
	if (sharedMemoryFileDescriptor < 0) goto DISCONNECT;
	if (ftruncate(sharedMemoryFileDescriptor,surfaceMemorySize) < 0) goto CLOSE_SHARED_MEMORY;

	{
		void *pixelData=mmap(nullptr,surfaceMemorySize,PROT_READ|PROT_WRITE,MAP_SHARED,sharedMemoryFileDescriptor,0);
		if (pixelData == MAP_FAILED) goto CLOSE_SHARED_MEMORY;

		auto sharedMemoryPool=wl_shm_create_pool(sharedMemoryInterface,sharedMemoryFileDescriptor,surfaceMemorySize);
		sharedMemoryBuffer=wl_shm_pool_create_buffer(sharedMemoryPool,0,WIDTH,HEIGHT,stride,WL_SHM_FORMAT_ARGB8888);

		wl_callback *frame=wl_surface_frame(surface);
		wl_callback_add_listener(frame,&FrameDispatch,pixelData); // this needs to happen BEFORE wl_surface_commit

		wl_surface_attach(surface,sharedMemoryBuffer,0,0);
		wl_surface_damage(surface,0,0,WIDTH,HEIGHT);
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
	return static_cast<int>(ExitCode::SUCCESS);
}
