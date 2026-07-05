#include <SDL3/SDL.h>

#include "imgui_app.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <cstdint>
#include <limits>
#include <stdio.h>
#include <vector>

namespace ImGuiApp {
	namespace {
		constexpr unsigned int GL_COLOR_BUFFER_BIT_VALUE = 0x00004000;
		constexpr unsigned int GL_FRAMEBUFFER_VALUE = 0x8D40;
		constexpr unsigned int GL_TEXTURE_2D_VALUE = 0x0DE1;
		constexpr unsigned int GL_TEXTURE_BINDING_2D_VALUE = 0x8069;
		constexpr unsigned int GL_TEXTURE_MIN_FILTER_VALUE = 0x2801;
		constexpr unsigned int GL_TEXTURE_MAG_FILTER_VALUE = 0x2800;
		constexpr unsigned int GL_TEXTURE_WRAP_S_VALUE = 0x2802;
		constexpr unsigned int GL_TEXTURE_WRAP_T_VALUE = 0x2803;
		constexpr unsigned int GL_LINEAR_VALUE = 0x2601;
		constexpr unsigned int GL_CLAMP_TO_EDGE_VALUE = 0x812F;
		constexpr unsigned int GL_RGBA_VALUE = 0x1908;
		constexpr unsigned int GL_UNSIGNED_BYTE_VALUE = 0x1401;
		constexpr unsigned int GL_UNPACK_ALIGNMENT_VALUE = 0x0CF5;

		using GlBindFramebufferProc = void (*)(unsigned int, unsigned int);
		using GlClearColorProc = void (*)(float, float, float, float);
		using GlClearProc = void (*)(unsigned int);
		using GlBindTextureProc = void (*)(unsigned int, unsigned int);
		using GlDeleteTexturesProc = void (*)(int, const unsigned int*);
		using GlGenTexturesProc = void (*)(int, unsigned int*);
		using GlGetIntegervProc = void (*)(unsigned int, int*);
		using GlPixelStoreiProc = void (*)(unsigned int, int);
		using GlTexImage2DProc = void (*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
		using GlTexParameteriProc = void (*)(unsigned int, unsigned int, int);
		using GlTexSubImage2DProc = void (*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);

		struct OpenGLTextureProcs {
			GlBindTextureProc BindTexture{nullptr};
			GlDeleteTexturesProc DeleteTextures{nullptr};
			GlGenTexturesProc GenTextures{nullptr};
			GlGetIntegervProc GetIntegerv{nullptr};
			GlPixelStoreiProc PixelStorei{nullptr};
			GlTexImage2DProc TexImage2D{nullptr};
			GlTexParameteriProc TexParameteri{nullptr};
			GlTexSubImage2DProc TexSubImage2D{nullptr};
			bool loaded{false};
		};

		static OpenGLTextureProcs gl_texture_procs{};
	}

	struct Texture {
		Backend backend{Backend::None};
		unsigned int flags{TextureFlagNone};
		int width{0};
		int height{0};
		int pitch{0};
		SDL_Texture* sdl_texture{nullptr};
		unsigned int gl_texture{0};
		std::vector<unsigned char> pixels{};
		bool locked{false};
	};

	static bool running{false};
	static Backend backend{Backend::None};
	static SDL_Window* window{nullptr};
	static SDL_Renderer* renderer{nullptr};
	static SDL_GLContext gl_context{nullptr};
	static SDL_Event event{};
	static bool begin_frame_called{false};
	static bool owns_sdl{false};
	static bool owns_window{false};
	static bool owns_renderer{false};
	static bool owns_gl_context{false};
	static bool imgui_context_ready{false};
	static bool platform_backend_ready{false};
	static bool renderer_backend_ready{false};

	static bool ensure_not_initialized() {
		if (backend != Backend::None || window != nullptr || renderer != nullptr || gl_context != nullptr) {
			SDL_Log("ImGuiApp is already initialized.");
			return false;
		}

		return true;
	}

	static bool setup_imgui_context() {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		imgui_context_ready = true;
		ImGuiIO& io = ImGui::GetIO();
		(void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

		ImGui::StyleColorsDark();
		return true;
	}

	static bool make_opengl_current() {
		if (window == nullptr || gl_context == nullptr) {
			return false;
		}

		return SDL_GL_MakeCurrent(window, gl_context);
	}

	static bool load_opengl_texture_procs() {
		if (gl_texture_procs.loaded) {
			return true;
		}

		gl_texture_procs.BindTexture = reinterpret_cast<GlBindTextureProc>(SDL_GL_GetProcAddress("glBindTexture"));
		gl_texture_procs.DeleteTextures = reinterpret_cast<GlDeleteTexturesProc>(SDL_GL_GetProcAddress("glDeleteTextures"));
		gl_texture_procs.GenTextures = reinterpret_cast<GlGenTexturesProc>(SDL_GL_GetProcAddress("glGenTextures"));
		gl_texture_procs.GetIntegerv = reinterpret_cast<GlGetIntegervProc>(SDL_GL_GetProcAddress("glGetIntegerv"));
		gl_texture_procs.PixelStorei = reinterpret_cast<GlPixelStoreiProc>(SDL_GL_GetProcAddress("glPixelStorei"));
		gl_texture_procs.TexImage2D = reinterpret_cast<GlTexImage2DProc>(SDL_GL_GetProcAddress("glTexImage2D"));
		gl_texture_procs.TexParameteri = reinterpret_cast<GlTexParameteriProc>(SDL_GL_GetProcAddress("glTexParameteri"));
		gl_texture_procs.TexSubImage2D = reinterpret_cast<GlTexSubImage2DProc>(SDL_GL_GetProcAddress("glTexSubImage2D"));

		gl_texture_procs.loaded =
			gl_texture_procs.BindTexture != nullptr &&
			gl_texture_procs.DeleteTextures != nullptr &&
			gl_texture_procs.GenTextures != nullptr &&
			gl_texture_procs.GetIntegerv != nullptr &&
			gl_texture_procs.PixelStorei != nullptr &&
			gl_texture_procs.TexImage2D != nullptr &&
			gl_texture_procs.TexParameteri != nullptr &&
			gl_texture_procs.TexSubImage2D != nullptr;

		if (!gl_texture_procs.loaded) {
			SDL_Log("ImGuiApp failed to load the required OpenGL texture functions.");
		}

		return gl_texture_procs.loaded;
	}

	static void clear_opengl_framebuffer(const ImVec4& clear_color) {
		static GlBindFramebufferProc gl_bind_framebuffer = reinterpret_cast<GlBindFramebufferProc>(SDL_GL_GetProcAddress("glBindFramebuffer"));
		static GlClearColorProc gl_clear_color = reinterpret_cast<GlClearColorProc>(SDL_GL_GetProcAddress("glClearColor"));
		static GlClearProc gl_clear = reinterpret_cast<GlClearProc>(SDL_GL_GetProcAddress("glClear"));

		if (gl_bind_framebuffer != nullptr) {
			gl_bind_framebuffer(GL_FRAMEBUFFER_VALUE, 0);
		}

		if (gl_clear_color == nullptr || gl_clear == nullptr) {
			return;
		}

		gl_clear_color(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		gl_clear(GL_COLOR_BUFFER_BIT_VALUE);
	}

	static bool setup_sdl_renderer_backend(SDL_Window* new_window, SDL_Renderer* new_renderer) {
		if (new_window == nullptr || new_renderer == nullptr) {
			SDL_Log("ImGuiApp SDLRenderer backend requires a valid SDL_Window and SDL_Renderer.");
			return false;
		}

		window = new_window;
		renderer = new_renderer;
		gl_context = nullptr;
		backend = Backend::SDLRenderer;

		setup_imgui_context();

		if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)) {
			Shutdown();
			return false;
		}
		platform_backend_ready = true;

		if (!ImGui_ImplSDLRenderer3_Init(renderer)) {
			Shutdown();
			return false;
		}
		renderer_backend_ready = true;

		running = true;
		begin_frame_called = false;
		return true;
	}

	static bool setup_opengl_backend(SDL_Window* new_window, SDL_GLContext new_gl_context, const char* glsl_version) {
		if (new_window == nullptr || new_gl_context == nullptr) {
			SDL_Log("ImGuiApp OpenGL backend requires a valid SDL_Window and SDL_GLContext.");
			return false;
		}

		window = new_window;
		renderer = nullptr;
		gl_context = new_gl_context;
		backend = Backend::OpenGL;

		if (!make_opengl_current()) {
			SDL_Log("ImGuiApp failed to make the supplied OpenGL context current: %s", SDL_GetError());
			Shutdown();
			return false;
		}

		setup_imgui_context();

		if (!ImGui_ImplSDL3_InitForOpenGL(window, gl_context)) {
			Shutdown();
			return false;
		}
		platform_backend_ready = true;

		if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
			Shutdown();
			return false;
		}
		renderer_backend_ready = true;

		running = true;
		begin_frame_called = false;
		return true;
	}

	bool Init(const char* title, int width, int height) {
		return InitSDLRenderer(title, width, height);
	}

	bool InitSDLRenderer(const char* title, int width, int height) {
		if (!ensure_not_initialized()) {
			return false;
		}

		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
			printf("Error: %s\n", SDL_GetError());
			return false;
		}
		owns_sdl = true;

		// Create window with SDL_Renderer graphics context
		float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
		SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

		window = SDL_CreateWindow(title, (int)(width * main_scale), (int)(height * main_scale), window_flags);
		if (window == nullptr) {
			printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
			Shutdown();
			return false;
		}
		owns_window = true;

		renderer = SDL_CreateRenderer(window, nullptr);
		if (renderer == nullptr) {
			SDL_Log("Error creating SDL_Renderer!");
			Shutdown();
			return false;
		}
		owns_renderer = true;
		SDL_SetRenderVSync(renderer, 1);
		SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		SDL_ShowWindow(window);

		return setup_sdl_renderer_backend(window, renderer);
	}

	bool InitOpenGL(const char* title, int width, int height, const char* glsl_version) {
		if (!ensure_not_initialized()) {
			return false;
		}

		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
			printf("Error: %s\n", SDL_GetError());
			return false;
		}
		owns_sdl = true;

		float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
		SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		window = SDL_CreateWindow(title, (int)(width * main_scale), (int)(height * main_scale), window_flags);
		if (window == nullptr) {
			printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
			Shutdown();
			return false;
		}
		owns_window = true;

		gl_context = SDL_GL_CreateContext(window);
		if (gl_context == nullptr) {
			printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
			Shutdown();
			return false;
		}
		owns_gl_context = true;

		SDL_GL_MakeCurrent(window, gl_context);
		SDL_GL_SetSwapInterval(1);
		SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		SDL_ShowWindow(window);

		return setup_opengl_backend(window, gl_context, glsl_version);
	}

	bool AttachSDLRenderer(SDL_Window* new_window, SDL_Renderer* new_renderer) {
		if (!ensure_not_initialized()) {
			return false;
		}

		return setup_sdl_renderer_backend(new_window, new_renderer);
	}

	bool AttachOpenGL(SDL_Window* new_window, SDL_GLContext new_gl_context, const char* glsl_version) {
		if (!ensure_not_initialized()) {
			return false;
		}

		return setup_opengl_backend(new_window, new_gl_context, glsl_version);
	}

	bool begin_frame() {
		begin_frame_called = true;

		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui
		// wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main
		// application, or clear/overwrite your copy of the mouse data.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main
		// application, or clear/overwrite your copy of the keyboard data. Generally you may always
		// pass all inputs to dear imgui, and hide them from your application based on those two
		// flags.

		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT)
				running = false;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
				running = false;
		}

		// [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
		if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
			SDL_Delay(10);
		}

		// Start the Dear ImGui frame
		switch (backend) {
			case Backend::SDLRenderer:
				ImGui_ImplSDLRenderer3_NewFrame();
				break;
			case Backend::OpenGL:
				if (!make_opengl_current()) {
					running = false;
					return false;
				}
				ImGui_ImplOpenGL3_NewFrame();
				break;
			case Backend::None:
				running = false;
				return false;
		}

		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		return running;
	}

	void end_frame() {
		static ImVec4 clear_color{0.45f, 0.55f, 0.60f, 1.00f};

		ImGui::Render();
		ImGuiIO& io = ImGui::GetIO();

		switch (backend) {
			case Backend::SDLRenderer:
				SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
				SDL_SetRenderDrawColorFloat(renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w);
				SDL_RenderClear(renderer);
				ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
				SDL_RenderPresent(renderer);
				break;
			case Backend::OpenGL:
				if (make_opengl_current()) {
					clear_opengl_framebuffer(clear_color);
					ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
					SDL_GL_SwapWindow(window);
				}
				break;
			case Backend::None:
				break;
		}
	}

	bool Loop() {
		if (begin_frame_called) {
			end_frame();
		}
		return begin_frame();
	}

	bool is_running() {
		return running;
	}

	void RequestQuit() {
		running = false;
	}

	void Shutdown() {
		running = false;

		if (renderer_backend_ready && backend == Backend::OpenGL) {
			make_opengl_current();
			ImGui_ImplOpenGL3_Shutdown();
		} else if (renderer_backend_ready && backend == Backend::SDLRenderer) {
			ImGui_ImplSDLRenderer3_Shutdown();
		}

		if (platform_backend_ready) {
			ImGui_ImplSDL3_Shutdown();
		}

		if (imgui_context_ready && ImGui::GetCurrentContext() != nullptr) {
			ImGui::DestroyContext();
		}

		if (owns_renderer && renderer != nullptr) {
			SDL_DestroyRenderer(renderer);
		}

		if (owns_gl_context && gl_context != nullptr) {
			SDL_GL_DestroyContext(gl_context);
		}

		if (owns_window && window != nullptr) {
			SDL_DestroyWindow(window);
		}

		if (owns_sdl) {
			SDL_Quit();
		}

		backend = Backend::None;
		window = nullptr;
		renderer = nullptr;
		gl_context = nullptr;
		begin_frame_called = false;
		owns_sdl = false;
		owns_window = false;
		owns_renderer = false;
		owns_gl_context = false;
		imgui_context_ready = false;
		platform_backend_ready = false;
		renderer_backend_ready = false;
	}

	Backend GetBackend() {
		return backend;
	}

	Texture* CreateTexture(int width, int height, unsigned int flags) {
		if (width <= 0 || height <= 0 || width > std::numeric_limits<int>::max() / 4) {
			SDL_Log("ImGuiApp::CreateTexture requires a positive RGBA texture size.");
			return nullptr;
		}

		if (backend == Backend::None) {
			SDL_Log("ImGuiApp::CreateTexture requires ImGuiApp to be initialized.");
			return nullptr;
		}

		Texture* texture = new Texture{};
		texture->backend = backend;
		texture->flags = flags;
		texture->width = width;
		texture->height = height;
		texture->pitch = width * 4;

		switch (backend) {
			case Backend::SDLRenderer:
				if (renderer == nullptr) {
					delete texture;
					return nullptr;
				}

				texture->sdl_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height);
				if (texture->sdl_texture == nullptr) {
					SDL_Log("ImGuiApp failed to create SDL texture: %s", SDL_GetError());
					delete texture;
					return nullptr;
				}

				SDL_SetTextureBlendMode(texture->sdl_texture, SDL_BLENDMODE_BLEND);
				SDL_SetTextureScaleMode(texture->sdl_texture, SDL_SCALEMODE_LINEAR);
				if ((texture->flags & TextureFlagPreserveContents) != 0) {
					texture->pixels.resize(static_cast<std::size_t>(texture->pitch) * static_cast<std::size_t>(height));
					if (!SDL_UpdateTexture(texture->sdl_texture, nullptr, texture->pixels.data(), texture->pitch)) {
						SDL_Log("ImGuiApp failed to initialize SDL texture data: %s", SDL_GetError());
					}
				}
				return texture;

			case Backend::OpenGL: {
				if (!make_opengl_current() || !load_opengl_texture_procs()) {
					delete texture;
					return nullptr;
				}

				texture->pixels.resize(static_cast<std::size_t>(texture->pitch) * static_cast<std::size_t>(height));

				int previous_texture = 0;
				gl_texture_procs.GetIntegerv(GL_TEXTURE_BINDING_2D_VALUE, &previous_texture);
				gl_texture_procs.GenTextures(1, &texture->gl_texture);
				if (texture->gl_texture == 0) {
					SDL_Log("ImGuiApp failed to create OpenGL texture.");
					delete texture;
					return nullptr;
				}

				gl_texture_procs.BindTexture(GL_TEXTURE_2D_VALUE, texture->gl_texture);
				gl_texture_procs.TexParameteri(GL_TEXTURE_2D_VALUE, GL_TEXTURE_MIN_FILTER_VALUE, GL_LINEAR_VALUE);
				gl_texture_procs.TexParameteri(GL_TEXTURE_2D_VALUE, GL_TEXTURE_MAG_FILTER_VALUE, GL_LINEAR_VALUE);
				gl_texture_procs.TexParameteri(GL_TEXTURE_2D_VALUE, GL_TEXTURE_WRAP_S_VALUE, GL_CLAMP_TO_EDGE_VALUE);
				gl_texture_procs.TexParameteri(GL_TEXTURE_2D_VALUE, GL_TEXTURE_WRAP_T_VALUE, GL_CLAMP_TO_EDGE_VALUE);
				gl_texture_procs.PixelStorei(GL_UNPACK_ALIGNMENT_VALUE, 1);
				gl_texture_procs.TexImage2D(
					GL_TEXTURE_2D_VALUE,
					0,
					static_cast<int>(GL_RGBA_VALUE),
					width,
					height,
					0,
					GL_RGBA_VALUE,
					GL_UNSIGNED_BYTE_VALUE,
					texture->pixels.data());
				gl_texture_procs.BindTexture(GL_TEXTURE_2D_VALUE, static_cast<unsigned int>(previous_texture));
				return texture;
			}

			case Backend::None:
				break;
		}

		delete texture;
		return nullptr;
	}

	bool LockTexture(Texture* texture, void** pixels, int* pitch) {
		if (texture == nullptr || pixels == nullptr || pitch == nullptr) {
			SDL_Log("ImGuiApp::LockTexture requires a texture, pixels pointer, and pitch pointer.");
			return false;
		}

		if (texture->locked) {
			SDL_Log("ImGuiApp::LockTexture called on a texture that is already locked.");
			return false;
		}

		if (texture->backend != backend) {
			SDL_Log("ImGuiApp::LockTexture requires the texture to match the active backend.");
			return false;
		}

		switch (texture->backend) {
			case Backend::SDLRenderer:
				if ((texture->flags & TextureFlagPreserveContents) != 0) {
					if (texture->pixels.empty()) {
						SDL_Log("ImGuiApp SDL texture staging buffer is unavailable.");
						return false;
					}
					*pixels = texture->pixels.data();
					*pitch = texture->pitch;
					texture->locked = true;
					return true;
				}

				if (texture->sdl_texture == nullptr || !SDL_LockTexture(texture->sdl_texture, nullptr, pixels, pitch)) {
					SDL_Log("ImGuiApp failed to lock SDL texture: %s", SDL_GetError());
					return false;
				}
				texture->locked = true;
				return true;

			case Backend::OpenGL:
				if (texture->pixels.empty()) {
					SDL_Log("ImGuiApp OpenGL texture staging buffer is unavailable.");
					return false;
				}
				*pixels = texture->pixels.data();
				*pitch = texture->pitch;
				texture->locked = true;
				return true;

			case Backend::None:
				break;
		}

		return false;
	}

	void UnlockTexture(Texture* texture) {
		if (texture == nullptr || !texture->locked) {
			return;
		}

		switch (texture->backend) {
			case Backend::SDLRenderer:
				if (texture->sdl_texture != nullptr) {
					if ((texture->flags & TextureFlagPreserveContents) != 0) {
						if (!SDL_UpdateTexture(texture->sdl_texture, nullptr, texture->pixels.data(), texture->pitch)) {
							SDL_Log("ImGuiApp failed to upload SDL texture data: %s", SDL_GetError());
						}
					} else {
						SDL_UnlockTexture(texture->sdl_texture);
					}
				}
				break;

			case Backend::OpenGL:
				if (texture->gl_texture != 0 && make_opengl_current() && load_opengl_texture_procs()) {
					int previous_texture = 0;
					gl_texture_procs.GetIntegerv(GL_TEXTURE_BINDING_2D_VALUE, &previous_texture);
					gl_texture_procs.PixelStorei(GL_UNPACK_ALIGNMENT_VALUE, 1);
					gl_texture_procs.BindTexture(GL_TEXTURE_2D_VALUE, texture->gl_texture);
					gl_texture_procs.TexSubImage2D(
						GL_TEXTURE_2D_VALUE,
						0,
						0,
						0,
						texture->width,
						texture->height,
						GL_RGBA_VALUE,
						GL_UNSIGNED_BYTE_VALUE,
						texture->pixels.data());
					gl_texture_procs.BindTexture(GL_TEXTURE_2D_VALUE, static_cast<unsigned int>(previous_texture));
				} else {
					SDL_Log("ImGuiApp failed to upload OpenGL texture data.");
				}
				break;

			case Backend::None:
				break;
		}

		texture->locked = false;
	}

	void FreeTexture(Texture* texture) {
		if (texture == nullptr) {
			return;
		}

		if (texture->locked &&
			texture->backend == Backend::SDLRenderer &&
			(texture->flags & TextureFlagPreserveContents) == 0 &&
			texture->sdl_texture != nullptr) {
			SDL_UnlockTexture(texture->sdl_texture);
		}

		switch (texture->backend) {
			case Backend::SDLRenderer:
				if (texture->sdl_texture != nullptr) {
					SDL_DestroyTexture(texture->sdl_texture);
				}
				break;

			case Backend::OpenGL:
				if (texture->gl_texture != 0 && make_opengl_current() && load_opengl_texture_procs()) {
					gl_texture_procs.DeleteTextures(1, &texture->gl_texture);
				} else if (texture->gl_texture != 0) {
					SDL_Log("ImGuiApp could not delete OpenGL texture because no OpenGL context is current.");
				}
				break;

			case Backend::None:
				break;
		}

		delete texture;
	}

	ImTextureID GetTextureID(const Texture* texture) {
		if (texture == nullptr || texture->backend != backend) {
			return ImTextureID_Invalid;
		}

		switch (texture->backend) {
			case Backend::SDLRenderer:
				return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture->sdl_texture));
			case Backend::OpenGL:
				return static_cast<ImTextureID>(static_cast<std::uintptr_t>(texture->gl_texture));
			case Backend::None:
				break;
		}

		return ImTextureID_Invalid;
	}

	void GetSDLWindowAndRenderer(SDL_Window** window, SDL_Renderer** renderer) {
		*window = ImGuiApp::window;
		*renderer = ImGuiApp::renderer;
	}

	void GetSDLWindowAndGLContext(SDL_Window** window, SDL_GLContext* gl_context) {
		*window = ImGuiApp::window;
		*gl_context = ImGuiApp::gl_context;
	}

}  // namespace ImGuiApp
