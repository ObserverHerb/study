#include <boost/program_options.hpp>
#include <wayland-client.h>
#include <cstdint>
#include <string>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <iostream>
#include <limits>
#include <meta>
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
int width=800;
int height=600;
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
static void MouseMoved(void *userData,wl_pointer *mouse,std::uint32_t time,wl_fixed_t surfaceX,wl_fixed_t surfaceY) { }
static const wl_pointer_listener MouseDispatch={
	.enter=MouseFocused,
	.leave=MouseUnfocused,
	.motion=MouseMoved
};

template <typename Type>
static constexpr std::size_t HeaderSize()
{
	std::size_t size=0;
	template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^Type,std::meta::access_context::unprivileged())))
	{
		size+=std::meta::size_of(member);
	}
	return size;
}

class Bitmap
{
public:
	struct FileHeader
	{
		std::uint16_t type; // The file type; must be 0x4D42 ('BM') for BMP files.
		std::uint32_t size; // The size of the BMP file in bytes.
		std::uint16_t reservedFirst; // Reserved; must be zero.
		std::uint16_t reservedSecond; // Reserved; must be zero.
		std::uint32_t dataOffset; // The offset, in bytes, from the beginning of the BITMAPFILEHEADER structure to the bitmap bits (pixel data).
	};

	struct InformationHeader
	{
		std::uint32_t headerSize; // Size of this header (40 bytes)
		std::int32_t width; // Width of the bitmap in pixels
		std::int32_t  height; // Height of the bitmap in pixels (positive for bottom-up, negative for top-down)
		std::uint16_t colorPlanes; // Number of color planes (must be 1)
		std::uint16_t bitCount; // Number of bits per pixel
		std::uint32_t compressionMethod; // Compression method (0 for uncompressed RGB)
		std::uint32_t size; // Size of the raw bitmap data (can be 0 for uncompressed RGB)
		std::int32_t  pixelsPerMeterX; // Horizontal resolution in pixels per meter
		std::int32_t  pixelsPerMeterY; // Vertical resolution in pixels per meter
		std::uint32_t paletteColorCount; // Number of colors in the color palette
		std::uint32_t importantColorCount; // Number of important colors
	};

	FileHeader fileHeader;
	InformationHeader informationHeader;

	void Deserialize(const std::array<char,HeaderSize<FileHeader>()> &fileHeaderData,const std::array<char,HeaderSize<InformationHeader>()> &informationHeaderData)
	{
		template for (std::size_t offset=0; constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^FileHeader,std::meta::access_context::unprivileged())))
		{
			std::size_t size=std::meta::size_of(member);
			std::memcpy(&this->fileHeader.[:member:],fileHeaderData.data()+offset,size);
			offset+=size;
		}

		template for (std::size_t offset=0; constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^InformationHeader,std::meta::access_context::unprivileged())))
		{
			std::size_t size=std::meta::size_of(member);
			std::memcpy(&this->informationHeader.[:member:],informationHeaderData.data()+offset,size);
			offset+=size;
		}
	}
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

	// parse command line options
	boost::program_options::options_description argumentsDescription("Options");
	argumentsDescription.add_options()("filename,f",boost::program_options::value<std::string>(),"path to file");
	boost::program_options::variables_map options;
	boost::program_options::store(boost::program_options::parse_command_line(argumentCount,arguments,argumentsDescription),options);
	boost::program_options::notify(options);

	// load bmp
	std::string filename;
	if (auto filenameOption=options.find("filename"); filenameOption != options.end())
	{
		filename=filenameOption->second.as<std::string>();
		if (!std::filesystem::exists(std::filesystem::path(filename)))
			return static_cast<int>(ExitCode::INVALID_FILE);
	}
	else
	{
		return static_cast<int>(ExitCode::INVALID_ARGUMENT);
	}
	if (std::filesystem::file_size(filename) < HeaderSize<Bitmap::FileHeader>()+HeaderSize<Bitmap::InformationHeader>())
		return static_cast<int>(ExitCode::INVALID_FILE);
	std::ifstream file(filename,std::ios::binary);
	Bitmap bitmap;
	{
		std::array<char,HeaderSize<Bitmap::FileHeader>()> fileHeaderData;
		file.read(fileHeaderData.data(),HeaderSize<Bitmap::FileHeader>());
		std::array<char,HeaderSize<Bitmap::InformationHeader>()> informationHeaderData;
		file.read(informationHeaderData.data(),HeaderSize<Bitmap::InformationHeader>());
		bitmap.Deserialize(fileHeaderData,informationHeaderData);
		if (bitmap.fileHeader.type != 0x4D42)
			return static_cast<int>(ExitCode::INVALID_FILE);
		std::cout << "Width: " << bitmap.informationHeader.width << std::endl;
		std::cout << "Height: " << bitmap.informationHeader.height << std::endl;
		std::cout << "Data Offset: " << bitmap.fileHeader.dataOffset << std::endl;
		std::cout << "Bit Count: " << bitmap.informationHeader.bitCount << std::endl;
		std::cout << "Compression Method: " << bitmap.informationHeader.compressionMethod << std::endl;
		width=bitmap.informationHeader.width;
		height=bitmap.informationHeader.height;
	}

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
	xdg_toplevel_set_title(windowTopLevel,"Playland :D");
	xdg_toplevel_add_listener(windowTopLevel,&TopLevelDispatch,nullptr);
	auto windowDecoration=zxdg_decoration_manager_v1_get_toplevel_decoration(windowDecorationManager,windowTopLevel);
	wl_surface_commit(surface);

	wl_display_roundtrip(display); // wait for things to finish server-side

	int stride=width*sizeof(uint32_t);
	int surfaceMemorySize=stride*height;
	int sharedMemoryFileDescriptor=memfd_create("playland-shm",MFD_CLOEXEC);
	if (sharedMemoryFileDescriptor < 0) goto DISCONNECT;
	if (ftruncate(sharedMemoryFileDescriptor,surfaceMemorySize) < 0) goto CLOSE_SHARED_MEMORY;

	{
		std::vector<char> bitmapData;
		bitmapData.resize(bitmap.informationHeader.size);
		file.read(bitmapData.data(),bitmap.informationHeader.size);

		void *pixelData=mmap(nullptr,surfaceMemorySize,PROT_READ|PROT_WRITE,MAP_SHARED,sharedMemoryFileDescriptor,0);
		if (pixelData == MAP_FAILED) goto CLOSE_SHARED_MEMORY;

		std::size_t bitmapBytesPerPixel=bitmap.informationHeader.bitCount/std::numeric_limits<unsigned char>::digits; // unsigned char is guarateed to be exactly one byte
		std::cout << "Bytes Per Pixel: " << bitmapBytesPerPixel << "\n";
		std::size_t bitmapBytesInRow=width*bitmapBytesPerPixel;
		std::cout << "Bytes In Row: " << bitmapBytesInRow << "\n";
		int paddingBytes=((bitmapBytesInRow+3)&~3)-bitmapBytesInRow;
		std::cout << "Padding Bytes: " << paddingBytes << "\n";
		for (int y=0; y < height; y++)
		{
			for (int x=0; x < width; x++)
			{
				int index=y*width+x;
				//int offset=bitmap.informationHeader.size-(y*width+(width-x))*3-paddingBytes*(y+1);
				int offset=(bitmap.informationHeader.size-y*width*3)-(width*3-x*3)-paddingBytes*y;
				unsigned char r=0;
				std::memcpy(&r,bitmapData.data()+offset,1);
				unsigned char g=0;
				std::memcpy(&g,bitmapData.data()+offset+1,1);
				unsigned char b=0;
				std::memcpy(&b,bitmapData.data()+offset+2,1);

				unsigned int &rgba=static_cast<unsigned int*>(pixelData)[index];
				rgba=0;
				rgba|=static_cast<uint32_t>(255) << 24;
				rgba|=static_cast<uint32_t>(b) << 16;
				rgba|=static_cast<uint32_t>(g) << 8;
				rgba|=static_cast<uint32_t>(r);
			}
		}

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
	return static_cast<int>(ExitCode::SUCCESS);
}
