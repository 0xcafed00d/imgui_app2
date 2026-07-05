#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <SDL3/SDL_video.h>

struct SDL_Window;
struct SDL_Renderer;

namespace ImGuiApp {
	struct Texture;

	enum TextureFlags : unsigned int {
		TextureFlagNone = 0,
		TextureFlagPreserveContents = 1u << 0,
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

	Texture* CreateTexture(int width, int height, unsigned int flags = TextureFlagNone);
	bool LockTexture(Texture* texture, void** pixels, int* pitch);
	void UnlockTexture(Texture* texture);
	void FreeTexture(Texture* texture);
	ImTextureID GetTextureID(const Texture* texture);

	void GetSDLWindowAndRenderer(SDL_Window** window, SDL_Renderer** renderer);
	void GetSDLWindowAndGLContext(SDL_Window** window, SDL_GLContext* gl_context);

}  // namespace ImGuiApp
