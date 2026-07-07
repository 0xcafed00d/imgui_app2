#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_video.h>

#include <string>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;

namespace ImGuiApp {
	struct Texture;
	using FileDialogCallback = SDL_DialogFileCallback;
	using FileDialogFilter = SDL_DialogFileFilter;

	struct FileDialogResult {
		bool completed{false};
		bool accepted{false};
		bool canceled{false};
		bool error{false};
		int filter{-1};
		std::vector<std::string> paths{};
		std::string error_message{};
	};

	enum TextureFlags : unsigned int {
		TextureFlagNone = 0,
		TextureFlagPreserveContents = 1u << 0,
	};

	enum class TextureFilter {
		Nearest,
		Linear,
	};

	enum class Backend {
		None,
		SDLRenderer,
		OpenGL,
	};

	bool Init(const char* title, int width, int height);
	bool InitSDLRenderer(const char* title, int width, int height);
	bool InitOpenGL(const char* title, int width, int height, const char* glsl_version = nullptr);
	bool AttachSDLRenderer(SDL_Window* window, SDL_Renderer* renderer);
	bool AttachOpenGL(SDL_Window* window, SDL_GLContext gl_context, const char* glsl_version = nullptr);
	void RequestQuit();
	bool Loop();
	void Shutdown();
	Backend GetBackend();

	Texture* CreateTexture(int width, int height, unsigned int flags = TextureFlagNone, TextureFilter filter = TextureFilter::Linear);
	Texture* CreateTexture(int width, int height, TextureFilter filter);
	bool LockTexture(Texture* texture, void** pixels, int* pitch);
	void UnlockTexture(Texture* texture);
	void FreeTexture(Texture* texture);
	ImTextureID GetTextureID(const Texture* texture);

	bool ShowOpenFileDialog(
		FileDialogCallback callback,
		void* userdata = nullptr,
		const FileDialogFilter* filters = nullptr,
		int nfilters = 0,
		const char* default_location = nullptr,
		bool allow_many = false);
	bool ShowSaveFileDialog(
		FileDialogCallback callback,
		void* userdata = nullptr,
		const FileDialogFilter* filters = nullptr,
		int nfilters = 0,
		const char* default_location = nullptr);
	FileDialogResult OpenFileDialog(
		const FileDialogFilter* filters = nullptr,
		int nfilters = 0,
		const char* default_location = nullptr,
		bool allow_many = false);
	FileDialogResult SaveFileDialog(
		const FileDialogFilter* filters = nullptr,
		int nfilters = 0,
		const char* default_location = nullptr);

	void GetSDLWindowAndRenderer(SDL_Window** window, SDL_Renderer** renderer);
	void GetSDLWindowAndGLContext(SDL_Window** window, SDL_GLContext* gl_context);

}  // namespace ImGuiApp
